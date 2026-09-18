/*
 * $Id: game.cpp 3076 2014-07-30 00:20:58Z darren_janeczek $
 */

#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <ctime>
#include <map>
#include <vector>
#include <sys/stat.h>

#include "game.h"
#include "combat.h"
#include "save_validation.h"
#if defined(ZU4_IOS) || defined(ZU4_WEB)
#include "mobile_adjacent_action.h"
#include "mobile_context_action.h"
#include "mobile_pathfinding.h"
#include "mobile_tap_action.h"
#include "mobile_dungeon_exploration.h"
#include "mobile_map_discoveries.h"
#include "mobile_map_pins.h"
#include "zu4_ios_ui.h"
#ifdef ZU4_WEB
#include <SDL.h>
#include <emscripten.h>
#include "web_prompt.h"
#include "web_action_event.h"
#include "web_journal.h"
#endif
struct Person;
static unsigned mobilePanelDepth = 0;
static bool mobileBumpInputEligible = true;
static bool mobileWalkActive = false;
static bool mobileWalkExecuting = false;
static bool mobileWalkAwaitingTurn = false;
static uint32_t mobileWalkGeneration = 0;
static Map *mobileWalkMap = nullptr;
static Coords mobileWalkTarget = {};
static Coords mobileWalkExpected = {};
static MoveResult mobileWalkResult = MOVE_SUCCEEDED;
static std::vector<Direction> mobileWalkRoute;
static std::size_t mobileWalkRouteIndex = 0;
static int mobileWalkStepsTaken = 0;
static MobileTap::Target mobileWalkInteraction = MobileTap::NONE;
static Coords mobileWalkInteractionTarget = {};
static Person *mobileWalkPerson = nullptr;
static std::vector<unsigned char> mobileWorldExplored;
static const char *mobileMapFilename = "explored-map.dat";
static MobileMapPins mobileMapPins;
static const char *mobileMapPinsFilename = "map-pins.dat";
static MobileMapDiscoveries mobileMapDiscoveries;
static const char *mobileMapDiscoveriesFilename = "map-discoveries.dat";
static MobileDungeonExploration mobileDungeonExploration;
static const char *mobileDungeonExplorationFilename = "explored-dungeons.dat";
static std::vector<unsigned char> mobilePreparedMapPixels;
static int mobilePreparedMapWidth = 0;
static int mobilePreparedMapHeight = 0;
static int mobilePreparedMapPlayerX = 0;
static int mobilePreparedMapPlayerY = 0;
bool talkAt(const Coords &coords);
bool openAt(const Coords &coords);
extern "C" void zu4_mobile_move(int direction, int allowBump);
static bool mobilePrepareTapRoute(int offsetX, int offsetY);
static bool mobileRefreshTalkRoute();
static void mobilePerformWalkInteraction();
static void mobileWalkStep(uint32_t generation);
#endif
#ifdef ZU4_IOS
#include "adventure_package.h"
#include "adventure_transfer.h"
#include "cloud_accounts.h"
static void mobileAdventureBackups(bool duringPlay);
static void mobileAccount(bool duringPlay);
static void mobileCloudSyncAfterCheckpoint();
#include "topicjournal.h"
#include "journal_notebook.h"
#include "soundtrack.h"
#include "mobile_rules.h"
#include "save_recovery.h"
#include "save_slots.h"
#include "save_snapshot.h"
#include "save_store_contract.h"
#include "test_tools_panel.h"
#include "topic_panel.h"
static TopicJournal mobileTopics;
static JournalNotebook mobileNotebook;
static bool mobileNotebookWritable = true;
static const char *mobileNotebookFilename = "journal-notebook.dat";
static uint32_t mobileLastCheckpointAttempt = 0;
enum MobileBackgroundCheckpointResult {
    MOBILE_BACKGROUND_NO_GAME,
    MOBILE_BACKGROUND_RETAINED,
    MOBILE_BACKGROUND_SAVED,
    MOBILE_BACKGROUND_FAILED
};
static MobileBackgroundCheckpointResult mobileBackgroundCheckpointResult = MOBILE_BACKGROUND_NO_GAME;
static int mobileBackgroundCheckpointSlot = 1;
static int mobilePartyInitialMember = -1;
static void observeMobileDialogue(const std::string &text, Person *talker, const std::string &topic = "");
#endif

#include "annotation.h"
#include "camp.h"
#include "cheat.h"
#include "city.h"
#include "conversation.h"
#include "death.h"
#include "dungeonview.h"
#include "error.h"
#include "experience_settings.h"
#include "intro.h"
#include "item.h"
#include "imagemgr.h"
#include "mapmgr.h"
#include "moongate.h"
#include "music.h"
#include "names.h"
#include "person.h"
#include "portal.h"
#include "progress_bar.h"
#include "screen.h"
#include "shrine.h"
#include "sound.h"
#include "spell.h"
#include "stats.h"
#include "random.h"
#include "tilemap.h"
#include "u4.h"

GameController *game = NULL;
#if defined(ZU4_IOS) || defined(ZU4_WEB)
static void mobileCancelWalk() {
    mobileWalkActive = false;
    mobileWalkAwaitingTurn = false;
    mobileWalkMap = nullptr;
    mobileWalkRoute.clear();
    mobileWalkRouteIndex = 0;
    mobileWalkStepsTaken = 0;
    mobileWalkInteraction = MobileTap::NONE;
    mobileWalkPerson = nullptr;
    ++mobileWalkGeneration;
}

static bool mobileWalkEligible() {
    return settings.tapToWalk && game && c && c->location && c->party &&
        eventHandler->getController() == game && !mobilePanelDepth &&
        c->location->viewMode == VIEW_NORMAL &&
        (c->location->context & CTX_NORMAL) &&
        !(c->location->context & (CTX_DUNGEON | CTX_COMBAT)) &&
        !collisionOverride && !c->party->isFlying() &&
        (c->transportContext == TRANSPORT_FOOT ||
         (c->transportContext == TRANSPORT_HORSE && !c->horseSpeed));
}

static bool mobileWalkCellHazardous(Map *map, const Coords &coords) {
    const Tile *tile = map ? map->tileTypeAt(coords, WITH_GROUND_OBJECTS) : nullptr;
    return !tile || tile->getEffect() != EFFECT_NONE;
}

static MobilePathfinding::Route mobilePlanWalk(
    const std::vector<MobilePathfinding::Point> &targets, int maxSteps) {
    const int side = 11;
    const int center = side / 2;
    std::vector<int> validMoves(side * side, 0);
    std::vector<unsigned char> hazardous(side * side, 1);
    Map *map = c->location->map;
    MapTile transport = c->party->getTransport();
    bool horse = transport.getTileType()->isHorse();

    for (int y = 0; y < side; ++y) for (int x = 0; x < side; ++x) {
        Coords from = c->location->coords;
        movexy(&from, x - center, y - center, map);
        int index = y * side + x;
        if (MAP_IS_OOB(map, from)) continue;
        hazardous[index] = mobileWalkCellHazardous(map, from) ? 1 : 0;
        MapTile previous = *map->tileAt(from, WITHOUT_OBJECTS);
        for (Direction direction : {DIR_WEST, DIR_NORTH, DIR_EAST, DIR_SOUTH}) {
            Coords to = from;
            movedir(&to, direction, map);
            if (MAP_IS_OOB(map, to)) continue;
            Object *object = map->objectAt(to);
            if (object && object->getType() != Object::UNKNOWN) continue;
            MapTile destination = *map->tileAt(to, WITH_OBJECTS);
            if (destination.getTileType()->canWalkOn(direction) &&
                (!horse || destination.getTileType()->isCreatureWalkable()) &&
                previous.getTileType()->canWalkOff(direction))
                validMoves[index] = DIR_ADD_TO_MASK(direction, validMoves[index]);
        }
    }

    return MobilePathfinding::shortestPathToAny(
        side, side, {center, center}, targets, validMoves, hazardous, maxSteps);
}

#ifdef ZU4_WEB
static void mobileWalkTimeout(void *context) {
    // A route step may start combat, an animation, or a conversation that
    // yields. Browser timers must only enqueue work, never re-enter Asyncify.
    SDL_Event event = {};
    event.type = SDL_USEREVENT;
    event.user.code = ZU4_WEB_ACTION_WALK_STEP;
    event.user.data1 = context;
    SDL_PushEvent(&event);
}
#else
static Uint32 mobileWalkTimer(Uint32, void *context) {
    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_USEREVENT;
    event.user.code = ZU4_IOS_ACTION_EVENT;
    event.user.data1 = (void *)(intptr_t)ZU4_MOBILE_ACTION_WALK_STEP;
    event.user.data2 = context;
    SDL_PushEvent(&event);
    return 0;
}
#endif

static void mobileScheduleWalkStep() {
    uint32_t generation = mobileWalkGeneration;
#ifdef ZU4_WEB
    emscripten_async_call(mobileWalkTimeout, (void *)(uintptr_t)generation, 110);
#else
    if (!SDL_AddTimer(110, mobileWalkTimer, (void *)(uintptr_t)generation)) {
        mobileCancelWalk();
        screenMessage("Tap route stopped.\n");
    }
#endif
}

static void mobileContinueWalkAfterTurn() {
    if (!mobileWalkActive || !mobileWalkAwaitingTurn) return;
    mobileWalkAwaitingTurn = false;
    if (!mobileWalkEligible() || c->location->map != mobileWalkMap ||
        !zu4_coords_equal(c->location->coords, mobileWalkExpected) ||
        !(mobileWalkResult & MOVE_SUCCEEDED) ||
        (mobileWalkResult & (MOVE_BLOCKED | MOVE_SLOWED | MOVE_MAP_CHANGE |
                             MOVE_TURNED | MOVE_DRIFT_ONLY | MOVE_EXIT_TO_PARENT |
                             MOVE_INTERACTED)) ||
        mobileWalkCellHazardous(mobileWalkMap, c->location->coords)) {
        mobileCancelWalk();
        return;
    }
    if (mobileWalkInteraction == MobileTap::PERSON && !mobileRefreshTalkRoute()) {
        mobileCancelWalk();
        screenMessage("That person moved out of reach.\n");
        return;
    }
    if (zu4_coords_equal(c->location->coords, mobileWalkTarget)) {
        if (mobileWalkInteraction != MobileTap::NONE) mobileScheduleWalkStep();
        else mobileCancelWalk();
        return;
    }
    if (mobileWalkRouteIndex >= mobileWalkRoute.size()) {
        mobileCancelWalk();
        return;
    }
    mobileScheduleWalkStep();
}

static void mobileWalkStep(uint32_t generation) {
    if (!mobileWalkActive || generation != mobileWalkGeneration) return;
    if (!mobileWalkEligible() || c->location->map != mobileWalkMap) {
        mobileCancelWalk();
        return;
    }
    if (mobileWalkRouteIndex >= mobileWalkRoute.size()) {
        if (mobileWalkInteraction != MobileTap::NONE &&
            zu4_coords_equal(c->location->coords, mobileWalkTarget))
            mobilePerformWalkInteraction();
        else
            mobileCancelWalk();
        return;
    }
    Direction direction = mobileWalkRoute[mobileWalkRouteIndex++];
    int validMoves = mobileWalkMap->getValidMoves(
        c->location->coords, c->party->getTransport());
    if (!DIR_IN_MASK(direction, validMoves)) {
        mobileCancelWalk();
        return;
    }
    mobileWalkExpected = c->location->coords;
    movedir(&mobileWalkExpected, direction, mobileWalkMap);
    if (MAP_IS_OOB(mobileWalkMap, mobileWalkExpected) ||
        mobileWalkCellHazardous(mobileWalkMap, mobileWalkExpected)) {
        mobileCancelWalk();
        return;
    }
    mobileWalkResult = MOVE_SUCCEEDED;
    mobileWalkAwaitingTurn = true;
    mobileWalkExecuting = true;
    ++mobileWalkStepsTaken;
    zu4_mobile_move(direction, 0);
    mobileWalkExecuting = false;
}

static void mobileStartWalk(uint32_t token) {
    mobileCancelWalk();
    if (!(token & 0x40000000u) || !settings.tapToWalk || !game || !c ||
        !c->location || !c->party || eventHandler->getController() != game) return;
    int offsetX = (int)(token & 15u) - 5;
    int offsetY = (int)((token >> 4) & 15u) - 5;
    int originX = (int)((token >> 8) & 0xffu);
    int originY = (int)((token >> 16) & 0xffu);
    int mapId = (int)((token >> 24) & 0x3fu);
    if (originX != (c->location->coords.x & 0xff) ||
        originY != (c->location->coords.y & 0xff) ||
        mapId != (c->location->map->id & 0x3f)) return;
    if (!mobileWalkEligible()) {
        screenMessage(c->transportContext == TRANSPORT_HORSE && c->horseSpeed
            ? "Slow the horse before tap walking.\n" : "Tap walking is unavailable here.\n");
        return;
    }
    mobileWalkMap = c->location->map;
    if (!mobilePrepareTapRoute(offsetX, offsetY)) {
        mobileWalkMap = nullptr;
        screenMessage("No safe route to that target.\n");
        return;
    }
    mobileWalkRouteIndex = 0;
    mobileWalkActive = true;
    mobileWalkStep(mobileWalkGeneration);
}
#endif

#if defined(ZU4_IOS) || defined(ZU4_WEB)
static const char *mobileCondition(StatusType status) {
    switch (status) {
    case STAT_GOOD: return "Healthy";
    case STAT_POISONED: return "Poisoned";
    case STAT_SLEEPING: return "Asleep";
    case STAT_DEAD: return "Dead";
    default: return "Unknown condition";
    }
}
extern "C" int zu4_mobile_can_repeat_movement(void) {
    return game && eventHandler->getController() == game && !mobilePanelDepth;
}
extern "C" void zu4_mobile_set_bump_input_eligible(int eligible) {
    mobileBumpInputEligible = eligible != 0;
}
extern "C" int zu4_mobile_map_visible(void) {
    return game && c && c->location && c->party &&
        (c->location->viewMode == VIEW_NORMAL || c->location->viewMode == VIEW_DUNGEON ||
         (c->location->context & CTX_COMBAT));
}
static Location *mobileWorldLocation() {
    Location *location = c ? c->location : nullptr;
    while (location && !location->map->isWorldMap()) location = location->prev;
    return location;
}
static void mobileRevealWorld() {
    Location *location = mobileWorldLocation();
    if (!location || location != c->location) return;
    Map *world = location->map;
    std::size_t size = (std::size_t)world->width * world->height;
    if (mobileWorldExplored.size() != size) mobileWorldExplored.assign(size, 0);
    // Reveal exactly the terrain in the ordinary 11x11 overhead viewport.
    for (int dy = -5; dy <= 5; ++dy) for (int dx = -5; dx <= 5; ++dx) {
        Coords at = {location->coords.x + dx, location->coords.y + dy, 0};
        wrap(&at, world);
        mobileWorldExplored[(std::size_t)at.y * world->width + at.x] = 1;
    }
}
static bool mobileLoadExplorationMap(const std::string &directory) {
    Map *world = mapMgr->get(MAP_WORLD);
    std::size_t size = (std::size_t)world->width * world->height;
    mobileWorldExplored.assign(size, 0);
    FILE *file = fopen((directory + mobileMapFilename).c_str(), "rb");
    if (!file) return true; // Existing adventures acquire a map on their next checkpoint.
    char header[64] = {};
    bool valid = fgets(header, sizeof(header), file) &&
        strcmp(header, "ZU4-EXPLORED-MAP-1\n") == 0 &&
        fread(mobileWorldExplored.data(), 1, size, file) == size &&
        fgetc(file) == EOF && !ferror(file);
    fclose(file);
    if (!valid) mobileWorldExplored.assign(size, 0);
    return valid;
}
static bool mobileSaveExplorationMap(const std::string &directory) {
    Map *world = mapMgr->get(MAP_WORLD);
    std::size_t size = (std::size_t)world->width * world->height;
    if (mobileWorldExplored.size() != size) mobileWorldExplored.assign(size, 0);
    FILE *file = fopen((directory + mobileMapFilename).c_str(), "wb");
    if (!file) return false;
    const char header[] = "ZU4-EXPLORED-MAP-1\n";
    bool ok = fwrite(header, 1, sizeof(header) - 1, file) == sizeof(header) - 1 &&
        fwrite(mobileWorldExplored.data(), 1, size, file) == size && fflush(file) == 0;
#ifndef ZU4_WEB
    ok = ok && fsync(fileno(file)) == 0;
#endif
    if (fclose(file) != 0) ok = false;
    return ok;
}
static bool mobileLoadMapPins(const std::string &directory) {
    Map *world = mapMgr->get(MAP_WORLD);
    return mobileMapPins.load(directory + mobileMapPinsFilename, world->width, world->height);
}
static bool mobileSaveMapPins(const std::string &directory) {
    return mobileMapPins.save(directory + mobileMapPinsFilename);
}
static bool mobileLoadMapDiscoveries(const std::string &directory) {
    Map *world = mapMgr->get(MAP_WORLD);
    return mobileMapDiscoveries.load(directory + mobileMapDiscoveriesFilename,
                                     world->width, world->height);
}
static bool mobileSaveMapDiscoveries(const std::string &directory) {
    return mobileMapDiscoveries.save(directory + mobileMapDiscoveriesFilename);
}
static std::vector<MobileDungeonExploration::Shape> mobileDungeonShapes() {
    std::vector<MobileDungeonExploration::Shape> shapes;
    for (int id = MAP_DECEIT; id <= MAP_ABYSS; ++id) {
        Map *map = mapMgr->get((MapId)id);
        if (!map || map->type != Map::DUNGEON) return {};
        shapes.push_back({id, (int)map->width, (int)map->height, (int)map->levels});
    }
    return shapes;
}
static bool mobileLoadDungeonExploration(const std::string &directory) {
    return mobileDungeonExploration.load(directory + mobileDungeonExplorationFilename,
                                         mobileDungeonShapes());
}
static bool mobileSaveDungeonExploration(const std::string &directory) {
    return mobileDungeonExploration.save(directory + mobileDungeonExplorationFilename);
}
static void mobileRevealDungeon() {
    if (!c || !c->location || c->location->context != CTX_DUNGEON ||
        c->location->map->type != Map::DUNGEON) return;
    Map *map = c->location->map;
    mobileDungeonExploration.reveal((int)map->id, c->location->coords.x,
        c->location->coords.y, c->location->coords.z,
        (int)map->width, (int)map->height, (int)map->levels);
}
extern "C" int zu4_mobile_dungeon_cell_revealed(int mapId, int x, int y, int level) {
    return mobileDungeonExploration.isRevealed(mapId, x, y, level) ? 1 : 0;
}
extern "C" int zu4_mobile_dungeon_remember_cell(int mapId, int x, int y, int level) {
    if (!c || !c->location || c->location->context != CTX_DUNGEON ||
        c->location->map->type != Map::DUNGEON ||
        (int)c->location->map->id != mapId || c->location->coords.z != level)
        return 0;
    Map *map = c->location->map;
    return mobileDungeonExploration.reveal(mapId, x, y, level,
        (int)map->width, (int)map->height, (int)map->levels) ? 1 : 0;
}
static void mobileTerrainColor(const Tile *tile, unsigned char *pixel, bool dungeon = false) {
    std::string name = tile ? tile->getName() : "";
    for (char &ch : name) ch = (char)std::tolower((unsigned char)ch);
    unsigned char r = dungeon ? 42 : 55, g = dungeon ? 46 : 132, b = dungeon ? 50 : 55;
    if (name.find("shallows") != std::string::npos || name.find("shore") != std::string::npos)
        r = 38, g = 112, b = 205;
    else if ((tile && tile->isWater()) || name.find("water") != std::string::npos ||
             name.find("sea") != std::string::npos || name.find("deep") != std::string::npos)
        r = 24, g = 70, b = 170;
    else if (name.find("swamp") != std::string::npos || name.find("marsh") != std::string::npos)
        r = 75, g = 92, b = 38;
    else if (name.find("forest") != std::string::npos || name.find("woods") != std::string::npos)
        r = 18, g = 82, b = 32;
    else if (name.find("brush") != std::string::npos)
        r = 73, g = 112, b = 43;
    else if (name.find("hill") != std::string::npos)
        r = 142, g = 112, b = 54;
    else if (name.find("mountain") != std::string::npos)
        r = 120, g = 116, b = 112;
    else if (name.find("ladder") != std::string::npos)
        r = 76, g = 190, b = 210;
    else if (name.find("door") != std::string::npos)
        r = 184, g = 126, b = 54;
    else if (name.find("lava") != std::string::npos || name.find("fire") != std::string::npos)
        r = 205, g = 58, b = 38;
    else if (name.find("poison") != std::string::npos)
        r = 74, g = 172, b = 68;
    else if (name.find("energy") != std::string::npos || name.find("magic") != std::string::npos)
        r = 154, g = 74, b = 205;
    else if (name.find("sleep") != std::string::npos)
        r = 74, g = 112, b = 190;
    else if (name.find("floor") != std::string::npos)
        r = 104, g = 88, b = 68;
    else if (name.find("wall") != std::string::npos || name.find("solid") != std::string::npos ||
             name.find("rock") != std::string::npos || name.find("column") != std::string::npos)
        r = 116, g = 116, b = 120;
    else if (name.find("city") != std::string::npos || name.find("town") != std::string::npos ||
             name.find("castle") != std::string::npos || name.find("village") != std::string::npos ||
             name.find("shrine") != std::string::npos || name.find("dungeon") != std::string::npos)
        r = 236, g = 196, b = 78;
    pixel[0] = r; pixel[1] = g; pixel[2] = b; pixel[3] = 255;
}
static void mobileDungeonColor(Dungeon *dungeon, const Coords &at, unsigned char *pixel) {
    unsigned char r = 96, g = 82, b = 66;
    bool ladderUp = dungeon->ladderUpAt(at);
    bool ladderDown = dungeon->ladderDownAt(at);
    DungeonToken token = dungeon->tokenAt(at);
    if (ladderUp && ladderDown) r = 238, g = 238, b = 245;
    else if (ladderUp) r = 68, g = 205, b = 224;
    else if (ladderDown) r = 72, g = 116, b = 230;
    else if (token == DUNGEON_ROOM) r = 242, g = 188, b = 64;
    else if (token == DUNGEON_FIELD) {
        switch ((FieldType)dungeon->subTokenAt(at)) {
        case FIELD_POISON: r = 70, g = 190, b = 76; break;
        case FIELD_ENERGY: r = 176, g = 82, b = 224; break;
        case FIELD_FIRE: r = 225, g = 62, b = 42; break;
        case FIELD_SLEEP: r = 72, g = 128, b = 218; break;
        default: r = 196, g = 86, b = 196; break;
        }
    } else if (token == DUNGEON_DOOR)
        r = 184, g = 126, b = 54;
    else if (token == DUNGEON_WALL || token == DUNGEON_SECRET_DOOR)
        r = 118, g = 118, b = 124;
    else if (token == DUNGEON_CHEST) r = 210, g = 150, b = 44;
    else if (token == DUNGEON_MAGIC_ORB) r = 210, g = 112, b = 232;
    else if (token == DUNGEON_TRAP) r = 224, g = 106, b = 48;
    else if (token == DUNGEON_FOUNTAIN) r = 54, g = 164, b = 218;
    else if (token == DUNGEON_ALTAR) r = 238, g = 218, b = 142;
    else if (token == DUNGEON_CEILING_HOLE || token == DUNGEON_FLOOR_HOLE)
        r = 42, g = 44, b = 50;
    pixel[0] = r; pixel[1] = g; pixel[2] = b; pixel[3] = 255;
}
extern "C" int zu4_mobile_exploration_map_enabled(void) {
    return zu4_experience_exploration_map() ? 1 : 0;
}
static void mobileMiniPixel(unsigned char *rgba, int side, int x, int y,
                            unsigned char r, unsigned char g, unsigned char b) {
    if (x < 0 || y < 0 || x >= side || y >= side) return;
    unsigned char *pixel = &rgba[((std::size_t)y * side + x) * 4];
    pixel[0] = r; pixel[1] = g; pixel[2] = b; pixel[3] = 255;
}
extern "C" int zu4_mobile_minimap(unsigned char *rgba, int side) {
    if (!rgba || side < 9 || side > 65 || !(side & 1) || !game || !c ||
        !c->location || !zu4_experience_exploration_map() ||
        eventHandler->getController() != game ||
        (c->location->viewMode != VIEW_NORMAL && c->location->viewMode != VIEW_DUNGEON)) return 0;
    if (c->location->context == CTX_DUNGEON && c->location->map->type == Map::DUNGEON) {
        mobileRevealDungeon();
        Dungeon *dungeon = static_cast<Dungeon *>(c->location->map);
        for (int y = 0; y < side; ++y) for (int x = 0; x < side; ++x)
            mobileMiniPixel(rgba, side, x, y, 5, 7, 8);
        int cellScale = std::max(1, std::min((side - 2) / (int)dungeon->width,
                                             (side - 2) / (int)dungeon->height));
        int originX = (side - (int)dungeon->width * cellScale) / 2;
        int originY = (side - (int)dungeon->height * cellScale) / 2;
        for (unsigned y = 0; y < dungeon->height; ++y) for (unsigned x = 0; x < dungeon->width; ++x) {
            if (!mobileDungeonExploration.isRevealed((int)dungeon->id, (int)x, (int)y,
                                                      c->location->coords.z)) continue;
            Coords at = {(int)x, (int)y, c->location->coords.z};
            unsigned char color[4];
            mobileDungeonColor(dungeon, at, color);
            for (int oy = 0; oy < cellScale; ++oy) for (int ox = 0; ox < cellScale; ++ox)
                mobileMiniPixel(rgba, side, originX + (int)x * cellScale + ox,
                    originY + (int)y * cellScale + oy, color[0], color[1], color[2]);
        }
        int partyX = originX + c->location->coords.x * cellScale;
        int partyY = originY + c->location->coords.y * cellScale;
        for (int oy = 0; oy < cellScale; ++oy) for (int ox = 0; ox < cellScale; ++ox)
            mobileMiniPixel(rgba, side, partyX + ox, partyY + oy, 245, 245, 245);
        mobileMiniPixel(rgba, side, partyX + cellScale / 2, partyY + cellScale / 2,
                        235, 45, 55);
        return 2;
    }
    if (c->location != mobileWorldLocation() || c->location->context != CTX_WORLDMAP)
        return 0;
    mobileRevealWorld();
    Map *world = c->location->map;
    int half = side / 2;
    for (int py = 0; py < side; ++py) for (int px = 0; px < side; ++px) {
        Coords at = {c->location->coords.x + px - half,
                     c->location->coords.y + py - half, 0};
        wrap(&at, world);
        std::size_t explored = (std::size_t)at.y * world->width + at.x;
        unsigned char *pixel = &rgba[((std::size_t)py * side + px) * 4];
        if (explored >= mobileWorldExplored.size() || !mobileWorldExplored[explored]) {
            pixel[0] = 5; pixel[1] = 7; pixel[2] = 8; pixel[3] = 255;
        } else {
            mobileTerrainColor(world->tileTypeAt(at, WITHOUT_OBJECTS), pixel);
        }
    }
    for (const MobileMapDiscoveries::Place &place : mobileMapDiscoveries.all()) {
        std::size_t explored = (std::size_t)place.y * world->width + place.x;
        if (explored >= mobileWorldExplored.size() || !mobileWorldExplored[explored]) continue;
        int worldWidth = (int)world->width, worldHeight = (int)world->height;
        int dx = place.x - c->location->coords.x;
        int dy = place.y - c->location->coords.y;
        if (dx > worldWidth / 2) dx -= worldWidth;
        if (dx < -worldWidth / 2) dx += worldWidth;
        if (dy > worldHeight / 2) dy -= worldHeight;
        if (dy < -worldHeight / 2) dy += worldHeight;
        int x = half + dx, y = half + dy;
        if (x < -1 || y < -1 || x > side || y > side) continue;
        unsigned char r = 51, g = 199, b = 255;
        if (place.category == MobileMapDiscoveries::CASTLE) r = 255, g = 232, b = 140;
        else if (place.category == MobileMapDiscoveries::VILLAGE) r = 87, g = 232, b = 184;
        else if (place.category == MobileMapDiscoveries::SHRINE) r = 209, g = 135, b = 255;
        else if (place.category == MobileMapDiscoveries::DUNGEON) r = 255, g = 115, b = 61;
        if (place.category == MobileMapDiscoveries::TOWN) {
            mobileMiniPixel(rgba, side, x, y, r, g, b);
            mobileMiniPixel(rgba, side, x - 1, y, r, g, b);
            mobileMiniPixel(rgba, side, x + 1, y, r, g, b);
            mobileMiniPixel(rgba, side, x, y - 1, r, g, b);
            mobileMiniPixel(rgba, side, x, y + 1, r, g, b);
        } else if (place.category == MobileMapDiscoveries::CASTLE) {
            for (int oy = -1; oy <= 1; ++oy) for (int ox = -1; ox <= 1; ++ox)
                mobileMiniPixel(rgba, side, x + ox, y + oy, r, g, b);
        } else if (place.category == MobileMapDiscoveries::VILLAGE) {
            for (int oy = -1; oy <= 1; ++oy) for (int ox = -1; ox <= 1; ++ox)
                if (ox || oy) mobileMiniPixel(rgba, side, x + ox, y + oy, r, g, b);
        } else if (place.category == MobileMapDiscoveries::SHRINE) {
            for (int offset = -1; offset <= 1; ++offset) {
                mobileMiniPixel(rgba, side, x + offset, y + offset, r, g, b);
                mobileMiniPixel(rgba, side, x + offset, y - offset, r, g, b);
            }
        } else {
            mobileMiniPixel(rgba, side, x, y - 1, r, g, b);
            for (int ox = -1; ox <= 1; ++ox)
                mobileMiniPixel(rgba, side, x + ox, y, r, g, b);
            for (int ox = -1; ox <= 1; ++ox)
                mobileMiniPixel(rgba, side, x + ox, y + 1, r, g, b);
        }
    }
    for (int oy = -1; oy <= 1; ++oy) for (int ox = -1; ox <= 1; ++ox)
        mobileMiniPixel(rgba, side, half + ox, half + oy, 245, 245, 245);
    mobileMiniPixel(rgba, side, half, half, 235, 45, 55);
    return 1;
}
extern "C" int zu4_mobile_map_pins_enabled(void) {
    return zu4_experience_map_pins() ? 1 : 0;
}
extern "C" int zu4_mobile_map_pin_count(void) {
    return zu4_experience_map_pins() ? (int)mobileMapPins.all().size() : 0;
}
extern "C" int zu4_mobile_map_pin_at(int index, Zu4MobileMapPin *pin) {
    if (!pin || index < 0 || index >= zu4_mobile_map_pin_count()) return 0;
    const MobileMapPins::Pin &source = mobileMapPins.all()[(std::size_t)index];
    pin->x = source.x;
    pin->y = source.y;
    snprintf(pin->label, sizeof(pin->label), "%s", source.label.c_str());
    return 1;
}
extern "C" int zu4_mobile_map_discovery_count(void) {
    return (int)mobileMapDiscoveries.all().size();
}
extern "C" int zu4_mobile_map_discovery_at(int index, Zu4MobileMapDiscovery *place) {
    if (!place || index < 0 || index >= zu4_mobile_map_discovery_count()) return 0;
    const MobileMapDiscoveries::Place &source =
        mobileMapDiscoveries.all()[(std::size_t)index];
    Map *world = mapMgr->get(MAP_WORLD);
    std::size_t exploredIndex = (std::size_t)source.y * world->width + source.x;
    if (exploredIndex >= mobileWorldExplored.size() || !mobileWorldExplored[exploredIndex])
        return 0;
    place->x = source.x;
    place->y = source.y;
    place->category = (int)source.category;
    snprintf(place->name, sizeof(place->name), "%s", source.name.c_str());
    return 1;
}
extern "C" int zu4_mobile_can_set_map_pin(int x, int y) {
    if (!zu4_experience_map_pins()) return 0;
    Map *world = mapMgr->get(MAP_WORLD);
    if (x < 0 || y < 0 || x >= world->width || y >= world->height) return 0;
    std::size_t index = (std::size_t)y * world->width + x;
    if (index >= mobileWorldExplored.size() || !mobileWorldExplored[index]) return 0;
    const std::vector<MobileMapPins::Pin> &pins = mobileMapPins.all();
    return pins.size() < MobileMapPins::MAX_PINS ||
        std::any_of(pins.begin(), pins.end(), [&](const MobileMapPins::Pin &pin) {
            return pin.x == x && pin.y == y;
        });
}
extern "C" int zu4_mobile_set_map_pin(int x, int y, const char *label) {
    if (!label || !zu4_mobile_can_set_map_pin(x, y)) return 0;
    Map *world = mapMgr->get(MAP_WORLD);
    return mobileMapPins.set(x, y, label, world->width, world->height) ? 1 : 0;
}
extern "C" int zu4_mobile_remove_map_pin(int x, int y) {
    if (!zu4_experience_map_pins()) return 0;
    return mobileMapPins.remove(x, y) ? 1 : 0;
}
extern "C" void zu4_mobile_exploration_map(void) {
    int kind = zu4_mobile_prepare_exploration_map();
    if (!kind) return;
#ifdef ZU4_IOS
    if (kind == 2) {
        std::string name = c->location->map->getName();
        if (name.empty()) name = "Dungeon";
        zu4_ios_show_dungeon_exploration_map(mobilePreparedMapPixels.data(), mobilePreparedMapWidth,
            mobilePreparedMapHeight, mobilePreparedMapPlayerX, mobilePreparedMapPlayerY,
            name.c_str(), c->location->coords.z + 1);
    } else {
        zu4_ios_show_exploration_map(mobilePreparedMapPixels.data(), mobilePreparedMapWidth,
            mobilePreparedMapHeight, mobilePreparedMapPlayerX, mobilePreparedMapPlayerY,
            zu4_experience_map_pins());
    }
#endif
}

extern "C" int zu4_mobile_prepare_exploration_map(void) {
    if (!game || !c || eventHandler->getController() != game ||
        !zu4_experience_exploration_map()) return 0;
    if (c->location && c->location->context == CTX_DUNGEON &&
        c->location->map->type == Map::DUNGEON) {
        mobileRevealDungeon();
        Dungeon *dungeon = static_cast<Dungeon *>(c->location->map);
        mobilePreparedMapPixels.assign((std::size_t)dungeon->width * dungeon->height * 4, 255);
        for (unsigned y = 0; y < dungeon->height; ++y) for (unsigned x = 0; x < dungeon->width; ++x) {
            unsigned char *pixel = &mobilePreparedMapPixels[((std::size_t)y * dungeon->width + x) * 4];
            if (!mobileDungeonExploration.isRevealed((int)dungeon->id, (int)x, (int)y,
                                                      c->location->coords.z)) {
                pixel[0] = 5; pixel[1] = 7; pixel[2] = 8; pixel[3] = 255;
            } else {
                Coords at = {(int)x, (int)y, c->location->coords.z};
                mobileDungeonColor(dungeon, at, pixel);
            }
        }
        mobilePreparedMapWidth = (int)dungeon->width;
        mobilePreparedMapHeight = (int)dungeon->height;
        mobilePreparedMapPlayerX = c->location->coords.x;
        mobilePreparedMapPlayerY = c->location->coords.y;
        return 2;
    }
    Location *location = mobileWorldLocation();
    if (!location) return 0;
    mobileRevealWorld();
    Map *world = location->map;
    mobilePreparedMapPixels.assign((std::size_t)world->width * world->height * 4, 255);
    for (unsigned y = 0; y < world->height; ++y) for (unsigned x = 0; x < world->width; ++x) {
        std::size_t index = (std::size_t)y * world->width + x;
        unsigned char *pixel = &mobilePreparedMapPixels[index * 4];
        if (index >= mobileWorldExplored.size() || !mobileWorldExplored[index]) {
            pixel[0] = 5; pixel[1] = 7; pixel[2] = 8; pixel[3] = 255;
        } else {
            Coords at = {(int)x, (int)y, 0};
            mobileTerrainColor(world->tileTypeAt(at, WITHOUT_OBJECTS), pixel);
        }
    }
    mobilePreparedMapWidth = (int)world->width;
    mobilePreparedMapHeight = (int)world->height;
    mobilePreparedMapPlayerX = location->coords.x;
    mobilePreparedMapPlayerY = location->coords.y;
    return 1;
}
extern "C" const unsigned char *zu4_mobile_prepared_map_pixels(void) { return mobilePreparedMapPixels.empty() ? nullptr : mobilePreparedMapPixels.data(); }
extern "C" int zu4_mobile_prepared_map_width(void) { return mobilePreparedMapWidth; }
extern "C" int zu4_mobile_prepared_map_height(void) { return mobilePreparedMapHeight; }
extern "C" int zu4_mobile_prepared_map_player_x(void) { return mobilePreparedMapPlayerX; }
extern "C" int zu4_mobile_prepared_map_player_y(void) { return mobilePreparedMapPlayerY; }

void gameDiscoverWorldPlace(const Coords &coords, Map *destination) {
    if (!destination || destination->type == Map::WORLD ||
        destination->type == Map::COMBAT) return;
    Map *world = mapMgr->get(MAP_WORLD);
    if (coords.x < 0 || coords.y < 0 || coords.x >= (int)world->width ||
        coords.y >= (int)world->height) return;
    MobileMapDiscoveries::Category category;
    if (City *city = dynamic_cast<City *>(destination)) {
        if (city->type == "castle") category = MobileMapDiscoveries::CASTLE;
        else if (city->type == "village") category = MobileMapDiscoveries::VILLAGE;
        else category = MobileMapDiscoveries::TOWN;
    } else if (destination->type == Map::SHRINE) {
        category = MobileMapDiscoveries::SHRINE;
    } else if (destination->type == Map::DUNGEON) {
        category = MobileMapDiscoveries::DUNGEON;
    } else {
        return;
    }
    if (!mobileMapDiscoveries.discover(coords.x, coords.y, category,
                                        destination->getName(), world->width, world->height)) return;
    std::size_t size = (std::size_t)world->width * world->height;
    if (mobileWorldExplored.size() != size) mobileWorldExplored.assign(size, 0);
    mobileWorldExplored[(std::size_t)coords.y * world->width + coords.x] = 1;
}
extern "C" int zu4_mobile_world_status(char *buffer, int capacity) {
    if (!game || !c || !c->location || !c->party) return 0;
    mobileRevealWorld();
    mobileRevealDungeon();
    Controller *controller = eventHandler->getController();
    if (CombatController *combat = dynamic_cast<CombatController *>(controller)) {
        PartyMember *active = combat->getCurrentPlayer();
        if (!active) return 0;
        MobileCombatTarget target;
        if (combat->mobileSelectedTarget(&target))
            snprintf(buffer, capacity, "%s's turn  •  %s\nHP %d/%d  •  Magic %d  •  %s\nTarget: %s  •  %d %s %s",
                active->getName().c_str(), mobileCondition(active->getStatus()),
                active->getHp(), active->getMaxHp(), active->getMp(), active->getWeapon()->name,
                target.creature->getName().c_str(), target.distance,
                target.distance == 1 ? "tile" : "tiles", getDirectionName(target.direction));
        else
            snprintf(buffer, capacity, "%s's turn  •  %s\nHP %d/%d  •  Magic %d  •  %s\nTarget: none",
                active->getName().c_str(), mobileCondition(active->getStatus()),
                active->getHp(), active->getMaxHp(), active->getMp(), active->getWeapon()->name);
        return 1;
    }
    if (c->location->viewMode == VIEW_DUNGEON &&
        c->location->context == CTX_DUNGEON && c->location->map->type == Map::DUNGEON) {
        std::string place = c->location->map->getName();
        if (place.empty()) place = "Dungeon";
        int torches = c->saveGame->torches;
        snprintf(buffer, capacity,
            "%s  •  L%d  •  %s\n%s %d  •  %d torch%s  •  Food %d  •  Gold %d",
            place.c_str(), c->location->coords.z + 1,
            getDirectionName((Direction)c->saveGame->orientation),
            c->party->getTorchDuration() > 0 ? "Lit" : "Dark",
            c->party->getTorchDuration(), torches, torches == 1 ? "" : "es",
            c->saveGame->food / 100, c->saveGame->gold);
        return 1;
    }
    if (c->location->viewMode != VIEW_NORMAL && c->location->viewMode != VIEW_MIXTURES) return 0;
    std::string place = c->location->map->getName();
    if (place.empty()) place = "Britannia";
    snprintf(buffer, capacity, "%s  •  Moons %d/%d  •  Wind %s\nFood %d  •  Gold %d",
        place.c_str(), c->saveGame->trammelphase, c->saveGame->feluccaphase,
        getDirectionName((Direction)c->windDirection), c->saveGame->food / 100,
        c->saveGame->gold);
    return 1;
}
extern "C" int zu4_mobile_party_status(Zu4MobilePartyMemberStatus *members, int capacity) {
    if (!game || !c || !c->party || !members || capacity <= 0) return 0;
    Controller *controller = eventHandler->getController();
    CombatController *combat = dynamic_cast<CombatController *>(controller);
    if (controller != game && !combat) return 0;
    PartyMember *active = combat ? combat->getCurrentPlayer() : nullptr;
    int count = std::min(c->party->size(), capacity);
    for (int i = 0; i < count; ++i) {
        PartyMember *member = c->party->member(i);
        snprintf(members[i].name, sizeof(members[i].name), "%s", member->getName().c_str());
        members[i].hp = member->getHp();
        members[i].maxHp = member->getMaxHp();
        members[i].condition = (char)member->getStatus();
        members[i].active = member == active;
    }
    return count;
}
extern "C" int zu4_mobile_combat_active(void) {
    return dynamic_cast<CombatController *>(eventHandler->getController()) != nullptr;
}
static int mobileCombatScreenCoordinate(int coordinate, int mapSize, int center) {
    if (mapSize <= 11) center = mapSize / 2;
    return coordinate - center + 5;
}
extern "C" int zu4_mobile_combat_target_count(void) {
    CombatController *combat = dynamic_cast<CombatController *>(eventHandler->getController());
    return combat ? (int)combat->mobileAttackTargets().size() : 0;
}
extern "C" int zu4_mobile_combat_target_at(int index, Zu4MobileCombatTarget *target) {
    CombatController *combat = dynamic_cast<CombatController *>(eventHandler->getController());
    if (!combat || !target || index < 0) return 0;
    std::vector<MobileCombatTarget> targets = combat->mobileAttackTargets();
    if (index >= (int)targets.size()) return 0;
    MobileCombatTarget selected;
    bool hasSelected = combat->mobileSelectedTarget(&selected);
    PartyMember *attacker = combat->getCurrentPlayer();
    CombatMap *map = combat->getMap();
    const MobileCombatTarget &candidate = targets[index];
    target->token = 0x20000000 | (candidate.coords.x & 0xff) | ((candidate.coords.y & 0xff) << 8);
    target->screenX = mobileCombatScreenCoordinate(candidate.coords.x, map->width, c->location->coords.x);
    target->screenY = mobileCombatScreenCoordinate(candidate.coords.y, map->height, c->location->coords.y);
    target->attackerScreenX = mobileCombatScreenCoordinate(attacker->getCoords().x, map->width, c->location->coords.x);
    target->attackerScreenY = mobileCombatScreenCoordinate(attacker->getCoords().y, map->height, c->location->coords.y);
    target->distance = candidate.distance;
    target->direction = candidate.direction;
    target->selected = hasSelected && zu4_coords_equal(selected.coords, candidate.coords);
    snprintf(target->name, sizeof(target->name), "%s", candidate.creature->getName().c_str());
    return target->screenX >= 0 && target->screenX < 11 && target->screenY >= 0 && target->screenY < 11;
}
extern "C" int zu4_mobile_combat_target_selected(void) {
    CombatController *combat = dynamic_cast<CombatController *>(eventHandler->getController());
    return combat && combat->mobileSelectedTarget(nullptr);
}
extern "C" int zu4_mobile_combat_repeat_target(char *name, int capacity) {
    CombatController *combat = dynamic_cast<CombatController *>(eventHandler->getController());
    MobileCombatTarget target;
    if (!combat || !combat->mobileLastAttackTarget(&target)) return 0;
    if (name && capacity > 0) snprintf(name, capacity, "%s", target.creature->getName().c_str());
    return 1;
}
extern "C" int zu4_mobile_combat_target_prepared(void) {
    CombatController *combat = dynamic_cast<CombatController *>(eventHandler->getController());
    return combat && combat->mobileHasTargetPreparation();
}
static void mobileCombatRepeatAttack() {
    CombatController *combat = dynamic_cast<CombatController *>(eventHandler->getController());
    if (!combat) return;
    if (!combat->mobilePrepareLastAttack()) screenMessage("Repeat unavailable. Choose a target.\n");
    gameUpdateScreen();
}
static void mobileCombatSelectTarget(int token) {
    CombatController *combat = dynamic_cast<CombatController *>(eventHandler->getController());
    if (!combat || !(token & 0x20000000)) return;
    Coords coords = {token & 0xff, (token >> 8) & 0xff, 0};
    if (combat->mobileSelectTarget(coords)) gameUpdateScreen();
}
static void mobileCombatCycleTarget(int step) {
    CombatController *combat = dynamic_cast<CombatController *>(eventHandler->getController());
    if (combat && combat->mobileCycleTarget(step)) gameUpdateScreen();
}
static void mobileCombatClearTarget() {
    CombatController *combat = dynamic_cast<CombatController *>(eventHandler->getController());
    if (!combat) return;
    combat->mobileClearTarget();
    gameUpdateScreen();
}
extern "C" int zu4_mobile_dungeon_active(void) {
    return game && c && c->location && c->party &&
        c->location->context == CTX_DUNGEON &&
        c->location->viewMode == VIEW_DUNGEON &&
        c->location->map->type == Map::DUNGEON;
}
extern "C" int zu4_mobile_dungeon_top_down(void) {
    return zu4_mobile_dungeon_active() && !DungeonViewer.is3DDungeonViewEnabled();
}
static void mobileDungeonCommand(int key) {
    if (!zu4_mobile_dungeon_active() || eventHandler->getController() != game) return;
    game->notifyKeyPressed(key);
}
static void mobileToggleDungeonView() {
    if (!zu4_mobile_dungeon_active() || eventHandler->getController() != game) return;
    bool firstPerson = DungeonViewer.toggle3DDungeonView();
    screenMessage("%s view\n", firstPerson ? "First-person" : "Overhead");
    gameUpdateScreen();
}
extern "C" void zu4_mobile_perform_gameplay_action(int action, int parameter) {
    mobileCancelWalk();
    switch (action) {
    case ZU4_MOBILE_ACTION_MOVE: zu4_mobile_move(parameter & 7, parameter & 0x100); break;
    case ZU4_MOBILE_ACTION_DUNGEON_SEARCH: mobileDungeonCommand('s'); break;
    case ZU4_MOBILE_ACTION_DUNGEON_TORCH: mobileDungeonCommand('i'); break;
    case ZU4_MOBILE_ACTION_DUNGEON_VIEW: mobileToggleDungeonView(); break;
    case ZU4_MOBILE_ACTION_COMBAT_TARGET: mobileCombatSelectTarget(parameter); break;
    case ZU4_MOBILE_ACTION_COMBAT_CYCLE: mobileCombatCycleTarget(parameter); break;
    case ZU4_MOBILE_ACTION_COMBAT_CLEAR_TARGET: mobileCombatClearTarget(); break;
    case ZU4_MOBILE_ACTION_COMBAT_REPEAT_ATTACK: mobileCombatRepeatAttack(); break;
    default: break;
    }
}
#endif
#if defined(ZU4_IOS) || defined(ZU4_WEB)
static MobileContext::State mobileContextState() {
    MobileContext::State state = {};
    state.transport = c->transportContext == TRANSPORT_HORSE ? MobileContext::HORSE :
        (c->transportContext == TRANSPORT_SHIP ? MobileContext::SHIP :
        (c->transportContext == TRANSPORT_BALLOON ? MobileContext::BALLOON : MobileContext::FOOT));
    state.flying = c->party->isFlying();
    Object *object = c->location->map->objectAt(c->location->coords);
    if (object) {
        const Tile *tile = object->getTile().getTileType();
        state.boardableHere = tile->isShip() || tile->isHorse() || tile->isBalloon();
    }
    const Tile *ground = c->location->map->tileTypeAt(c->location->coords, WITH_GROUND_OBJECTS);
    state.chestHere = ground && ground->isChest();
    state.enterPortal = c->location->map->portalAt(c->location->coords, ACTION_ENTER) != nullptr;
    state.climbPortal = c->location->map->portalAt(c->location->coords, ACTION_KLIMB) != nullptr;
    state.descendPortal = c->location->map->portalAt(c->location->coords, ACTION_DESCEND) != nullptr;
    state.dungeon = c->location->context == CTX_DUNGEON;
    if (state.dungeon) {
        Dungeon *dungeon = static_cast<Dungeon *>(c->location->map);
        state.ladderUp = dungeon->ladderUpAt(c->location->coords);
        state.ladderDown = dungeon->ladderDownAt(c->location->coords);
    }
    return state;
}
extern "C" int zu4_mobile_context_action(char *label, int capacity) {
    if (!label || capacity <= 0) return 0;
    MobileContext::Action action = MobileContext::NONE;
    if (game && c && c->location && c->party && eventHandler->getController() == game)
        action = MobileContext::resolve(mobileContextState());
    snprintf(label, capacity, "%s", MobileContext::label(action));
    return action != MobileContext::NONE;
}
extern "C" void zu4_mobile_context(void) {
    if (!game || !c || !c->location || !c->party || eventHandler->getController() != game) return;
    // Resolve again when the queued event reaches the engine so stale HUD state
    // can never dispatch an action that no longer matches the world.
    int command = MobileContext::command(MobileContext::resolve(mobileContextState()));
    if (command) game->notifyKeyPressed(command);
}
#endif
#if defined(ZU4_IOS) || defined(ZU4_WEB)

static bool mobilePersonEligible(Person *person) {
    return person && person->canConverse() &&
        person->getMovementBehavior() != MOVEMENT_ATTACK_AVATAR;
}

static bool mobilePersonIsLive(Person *person) {
    if (!person || !mobileWalkMap) return false;
    for (Object *object : mobileWalkMap->objects)
        if (object == person) return true;
    return false;
}

static MobileTap::Target mobileTapTargetAt(const Coords &target, Person **personOut) {
    if (personOut) *personOut = nullptr;
    if (City *city = dynamic_cast<City *>(c->location->map)) {
        Person *person = city->personAt(target);
        if (mobilePersonEligible(person)) {
            if (personOut) *personOut = person;
            return MobileTap::PERSON;
        }
    }
    const Tile *tile = c->location->map->tileTypeAt(target, WITH_OBJECTS);
    if (tile && tile->isLockedDoor()) return MobileTap::LOCKED_DOOR;
    if (tile && tile->isDoor()) return MobileTap::UNLOCKED_DOOR;
    tile = c->location->map->tileTypeAt(target, WITH_GROUND_OBJECTS);
    if (tile && tile->isChest()) return MobileTap::CHEST;
    return MobileTap::NONE;
}

static int mobileTalkOverDirections(const Coords &personCoords) {
    int directions = 0;
    for (Direction direction : {DIR_WEST, DIR_NORTH, DIR_EAST, DIR_SOUTH}) {
        Coords between = personCoords;
        movedir(&between, direction, c->location->map);
        if (MAP_IS_OOB(c->location->map, between)) continue;
        const Tile *tile = c->location->map->tileTypeAt(between, WITH_GROUND_OBJECTS);
        if (tile && Tile::canTalkOverTile(tile))
            directions = DIR_ADD_TO_MASK(direction, directions);
    }
    return directions;
}

static bool mobileOffsetTo(const Coords &target, int *offsetX, int *offsetY) {
    bool found = false;
    int bestDistance = 0;
    for (int y = -5; y <= 5; ++y) for (int x = -5; x <= 5; ++x) {
        Coords candidate = c->location->coords;
        movexy(&candidate, x, y, c->location->map);
        int distance = std::abs(x) + std::abs(y);
        if (zu4_coords_equal(candidate, target) && (!found || distance < bestDistance)) {
            found = true;
            bestDistance = distance;
            *offsetX = x;
            *offsetY = y;
        }
    }
    return found;
}

static void mobileApplyWalkPlan(const MobilePathfinding::Route &plan) {
    const int center = 5;
    mobileWalkTarget = c->location->coords;
    movexy(&mobileWalkTarget, plan.target.x - center, plan.target.y - center,
           c->location->map);
    mobileWalkRoute = plan.steps;
    mobileWalkRouteIndex = 0;
}

static bool mobilePrepareTapRoute(int offsetX, int offsetY) {
    Coords tapped = c->location->coords;
    movexy(&tapped, offsetX, offsetY, c->location->map);
    if (MAP_IS_OOB(c->location->map, tapped)) return false;

    Person *person = nullptr;
    MobileTap::Target interaction = settings.directInteractions
        ? mobileTapTargetAt(tapped, &person) : MobileTap::NONE;
    std::vector<MobilePathfinding::Point> goals;
    if (interaction == MobileTap::NONE)
        goals.push_back({5 + offsetX, 5 + offsetY});
    else
        goals = MobileTap::approachPoints(interaction, {5 + offsetX, 5 + offsetY},
            interaction == MobileTap::PERSON ? mobileTalkOverDirections(tapped) : 0);

    MobilePathfinding::Route plan = mobilePlanWalk(goals, 12);
    if (!plan.found) return false;
    mobileWalkInteraction = interaction;
    mobileWalkInteractionTarget = tapped;
    mobileWalkPerson = person;
    mobileApplyWalkPlan(plan);
    return true;
}

static bool mobileRefreshTalkRoute() {
    if (mobileWalkInteraction != MobileTap::PERSON ||
        !mobilePersonIsLive(mobileWalkPerson) || !mobilePersonEligible(mobileWalkPerson))
        return false;
    Coords target = mobileWalkPerson->getCoords();
    int offsetX = 0, offsetY = 0;
    if (!mobileOffsetTo(target, &offsetX, &offsetY)) return false;
    std::vector<MobilePathfinding::Point> goals = MobileTap::approachPoints(
        MobileTap::PERSON, {5 + offsetX, 5 + offsetY}, mobileTalkOverDirections(target));
    MobilePathfinding::Route plan = mobilePlanWalk(goals, 12 - mobileWalkStepsTaken);
    if (!plan.found) return false;
    mobileWalkInteractionTarget = target;
    mobileApplyWalkPlan(plan);
    return true;
}

static bool mobileCardinallyAdjacent(const Coords &target) {
    for (Direction direction : {DIR_WEST, DIR_NORTH, DIR_EAST, DIR_SOUTH}) {
        Coords adjacent = c->location->coords;
        movedir(&adjacent, direction, c->location->map);
        if (zu4_coords_equal(adjacent, target)) return true;
    }
    return false;
}

static bool mobileCanTalkTo(const Coords &target) {
    for (Direction direction : {DIR_WEST, DIR_NORTH, DIR_EAST, DIR_SOUTH}) {
        Coords between = c->location->coords;
        movedir(&between, direction, c->location->map);
        if (zu4_coords_equal(between, target)) return true;
        const Tile *tile = c->location->map->tileTypeAt(between, WITH_GROUND_OBJECTS);
        if (!tile || !Tile::canTalkOverTile(tile)) continue;
        Coords beyond = between;
        movedir(&beyond, direction, c->location->map);
        if (zu4_coords_equal(beyond, target)) return true;
    }
    return false;
}

static void mobilePerformWalkInteraction() {
    MobileTap::Target interaction = mobileWalkInteraction;
    Coords target = mobileWalkInteractionTarget;
    Person *person = mobileWalkPerson;
    bool valid = mobileWalkEligible() && c->location->map == mobileWalkMap;
    if (interaction == MobileTap::PERSON) {
        valid = valid && mobilePersonIsLive(person) && mobilePersonEligible(person);
        if (valid) target = person->getCoords();
        City *city = valid ? dynamic_cast<City *>(c->location->map) : nullptr;
        valid = valid && city && mobileCanTalkTo(target) && city->personAt(target) == person;
    } else if (interaction == MobileTap::UNLOCKED_DOOR ||
               interaction == MobileTap::LOCKED_DOOR) {
        const Tile *tile = valid
            ? c->location->map->tileTypeAt(target, WITH_OBJECTS) : nullptr;
        valid = valid && mobileCardinallyAdjacent(target) && tile &&
            (tile->isDoor() || tile->isLockedDoor());
    } else if (interaction == MobileTap::CHEST) {
        const Tile *tile = valid
            ? c->location->map->tileTypeAt(target, WITH_GROUND_OBJECTS) : nullptr;
        valid = valid && zu4_coords_equal(c->location->coords, target) &&
            tile && tile->isChest();
    } else {
        valid = false;
    }

    mobileCancelWalk();
    if (!valid) {
        screenMessage("That target is no longer available.\n");
        return;
    }

    bool performed = false;
    if (interaction == MobileTap::PERSON) {
        performed = talkAt(target);
    } else if (interaction == MobileTap::UNLOCKED_DOOR ||
               interaction == MobileTap::LOCKED_DOOR) {
        const Tile *tile = c->location->map->tileTypeAt(target, WITH_OBJECTS);
        if (tile && tile->isLockedDoor()) {
            screenMessage("%cLocked door.%c\nUse Explore > Unlock.\n", FG_GREY, FG_WHITE);
            performed = true;
        } else if (tile && tile->isDoor()) {
            performed = openAt(target);
        }
    } else if (interaction == MobileTap::CHEST) {
        getChest();
        performed = true;
    }
    if (performed && game && eventHandler->getController() == game)
        game->finishTurn();
}

static MobileAdjacent::Action mobileAdjacentAction(
    Direction direction, bool direct, Coords *resolvedTarget = nullptr) {
    MobileAdjacent::State state = {};
    state.enabled = direct ? settings.directInteractions : settings.bumpInteractions;
    state.ordinaryExploration = game && c && c->location && c->party &&
        eventHandler->getController() == game && !mobilePanelDepth &&
        c->location->viewMode == VIEW_NORMAL && (c->location->context & CTX_NORMAL) &&
        !(c->location->context & (CTX_DUNGEON | CTX_COMBAT));
    state.deliberateInput = direct || mobileBumpInputEligible;
    state.collisionOverride = collisionOverride;
    state.footOrHorse = c && (c->transportContext & TRANSPORT_FOOT_OR_HORSE) &&
        c->party && !c->party->isFlying();
    state.target = MobileAdjacent::NO_TARGET;
    if (!state.ordinaryExploration) return MobileAdjacent::NONE;

    Coords target = c->location->coords;
    movedir(&target, direction, c->location->map);
    if (City *city = dynamic_cast<City *>(c->location->map)) {
        Person *person = city->personAt(target);
        if (person && person->canConverse())
            state.target = person->getMovementBehavior() == MOVEMENT_ATTACK_AVATAR
                ? MobileAdjacent::HOSTILE_PERSON : MobileAdjacent::CONVERSABLE_PERSON;
    }
    if (state.target == MobileAdjacent::NO_TARGET) {
        const Tile *tile = c->location->map->tileTypeAt(target, WITH_OBJECTS);
        if (tile && tile->isLockedDoor()) state.target = MobileAdjacent::LOCKED_DOOR;
        else if (tile && tile->isDoor()) state.target = MobileAdjacent::UNLOCKED_DOOR;
    }
    if (state.target == MobileAdjacent::NO_TARGET) {
        const Tile *between = c->location->map->tileTypeAt(target, WITH_GROUND_OBJECTS);
        if (between && Tile::canTalkOverTile(between)) {
            Coords beyond = target;
            movedir(&beyond, direction, c->location->map);
            if (City *city = dynamic_cast<City *>(c->location->map)) {
                Person *person = city->personAt(beyond);
                if (person && person->canConverse()) {
                    target = beyond;
                    state.target = person->getMovementBehavior() == MOVEMENT_ATTACK_AVATAR
                        ? MobileAdjacent::HOSTILE_PERSON : MobileAdjacent::CONVERSABLE_PERSON;
                }
            }
        }
    }
    MobileAdjacent::Action action = MobileAdjacent::resolve(state);
    if (resolvedTarget && action != MobileAdjacent::NONE) *resolvedTarget = target;
    return action;
}
extern "C" int zu4_mobile_adjacent_action_label(int direction, char *label, int capacity) {
    if (!label || capacity <= 0) return 0;
    Direction dir = static_cast<Direction>(direction);
    MobileAdjacent::Action action = dir >= DIR_WEST && dir <= DIR_SOUTH
        ? mobileAdjacentAction(dir, true) : MobileAdjacent::NONE;
    const char *text = "Interact";
    if (action == MobileAdjacent::TALK) text = "Talk";
    else if (action == MobileAdjacent::OPEN) text = "Open Door";
    else if (action == MobileAdjacent::LOCKED_NOTICE) text = "Locked Door";
    snprintf(label, capacity, "%s", text);
    return action != MobileAdjacent::NONE;
}
static bool mobilePerformAdjacent(Direction direction, bool direct) {
    Coords target = {};
    MobileAdjacent::Action action = mobileAdjacentAction(direction, direct, &target);
    if (action == MobileAdjacent::NONE) return false;
    if (action == MobileAdjacent::LOCKED_NOTICE) {
        screenMessage("%cLocked door.%c\nUse Explore > Unlock.\n", FG_GREY, FG_WHITE);
        return true;
    }
    return action == MobileAdjacent::TALK ? talkAt(target) : openAt(target);
}
extern "C" void zu4_mobile_move(int direction, int allowBump) {
    if (!game || eventHandler->getController() != game) return;
    Direction dir = (Direction)direction;
    if (dir < DIR_WEST || dir > DIR_SOUTH) return;
    if (zu4_mobile_dungeon_top_down()) {
        // A north-up map should accept north-up movement. Turning is free in
        // the original dungeon rules, so face the intended cardinal direction
        // and dispatch the ordinary Forward command as one semantic touch.
        c->saveGame->orientation = dir;
        game->notifyKeyPressed(U4_UP);
        return;
    }
    bool previous = mobileBumpInputEligible;
    mobileBumpInputEligible = allowBump != 0;
    int key = dir == DIR_NORTH ? U4_UP : dir == DIR_SOUTH ? U4_DOWN :
        dir == DIR_WEST ? U4_LEFT : U4_RIGHT;
    game->notifyKeyPressed(key);
    mobileBumpInputEligible = previous;
}
extern "C" int zu4_mobile_capture_adjacent_interaction(int direction) {
    Direction dir = (Direction)direction;
    if (dir < DIR_WEST || dir > DIR_SOUTH ||
        mobileAdjacentAction(dir, true) == MobileAdjacent::NONE) return 0;
    uint32_t token = 0x40000000u | ((uint32_t)dir & 7u) |
        (((uint32_t)c->location->coords.x & 0xffu) << 3) |
        (((uint32_t)c->location->coords.y & 0xffu) << 11) |
        (((uint32_t)c->location->map->id & 0xffu) << 19);
    return (int)token;
}
extern "C" int zu4_mobile_direct_interactions_enabled(void) {
    return settings.directInteractions ? 1 : 0;
}
extern "C" int zu4_mobile_world_taps_enabled(void) {
    return settings.directInteractions || settings.tapToWalk ? 1 : 0;
}
extern "C" void zu4_mobile_cancel_walk(void) {
    mobileCancelWalk();
}
extern "C" int zu4_mobile_capture_walk(int offsetX, int offsetY) {
    mobileCancelWalk();
    if (!settings.tapToWalk || !game || !c || !c->location || !c->party ||
        eventHandler->getController() != game || mobilePanelDepth ||
        c->location->viewMode != VIEW_NORMAL || !(c->location->context & CTX_NORMAL) ||
        (c->location->context & (CTX_DUNGEON | CTX_COMBAT)) ||
        offsetX < -5 || offsetX > 5 || offsetY < -5 || offsetY > 5 ||
        (!offsetX && !offsetY)) return 0;
    uint32_t token = 0x40000000u |
        ((uint32_t)(offsetX + 5) & 15u) |
        (((uint32_t)(offsetY + 5) & 15u) << 4) |
        (((uint32_t)c->location->coords.x & 0xffu) << 8) |
        (((uint32_t)c->location->coords.y & 0xffu) << 16) |
        (((uint32_t)c->location->map->id & 0x3fu) << 24);
    return (int)token;
}
extern "C" int zu4_mobile_world_tap(int offsetX, int offsetY) {
    int token = zu4_mobile_capture_walk(offsetX, offsetY);
    if (!token) return 0;
    mobileStartWalk((uint32_t)token);
    return 1;
}
#ifdef ZU4_WEB
extern "C" void zu4_web_walk_start(int token) { mobileStartWalk((uint32_t)token); }
extern "C" void zu4_web_walk_step(int generation) { mobileWalkStep((uint32_t)generation); }
#endif
extern "C" void zu4_mobile_adjacent_interaction(int encoded) {
    uint32_t token = (uint32_t)encoded;
    if (!(token & 0x40000000u) || !game || !c || !c->location ||
        eventHandler->getController() != game) return;
    Direction dir = (Direction)(token & 7u);
    if (((token >> 3) & 0xffu) != ((uint32_t)c->location->coords.x & 0xffu) ||
        ((token >> 11) & 0xffu) != ((uint32_t)c->location->coords.y & 0xffu) ||
        ((token >> 19) & 0xffu) != ((uint32_t)c->location->map->id & 0xffu)) return;
    if (mobilePerformAdjacent(dir, true) && game && eventHandler->getController() == game)
        game->finishTurn();
}
#endif
#ifdef ZU4_IOS
extern "C" void zu4_mobile_cast(void) {
    Controller *controller = eventHandler->getController();
    if (game && (controller == game || dynamic_cast<CombatController *>(controller)))
        controller->notifyKeyPressed('c');
}
extern "C" void zu4_mobile_enter(void) {
    if (game && eventHandler->getController() == game) game->notifyKeyPressed('e');
}
extern "C" void zu4_mobile_talk(void) {
    if (CombatController *combat = dynamic_cast<CombatController *>(eventHandler->getController())) {
        combat->notifyKeyPressed('a');
        return;
    }
    if (game && eventHandler->getController() == game) {
        // Use the same command dispatcher as desktop Talk, including its
        // direction prompt, flying restriction, range, and turn accounting.
        // No software keyboard or synthesized key events are involved.
        game->notifyKeyPressed('t');
    }
}
extern "C" void zu4_mobile_perform_action(int action, int parameter) {
    if (zu4_journal_panel_is_visible()) return; // notebook owns input, no queued gameplay action
    if (action != ZU4_MOBILE_ACTION_WALK_START && action != ZU4_MOBILE_ACTION_WALK_STEP)
        mobileCancelWalk();
    switch (action) {
    case ZU4_MOBILE_ACTION_MENU: zu4_mobile_menu(); break;
    case ZU4_MOBILE_ACTION_CAST: zu4_mobile_cast(); break;
    case ZU4_MOBILE_ACTION_PARTY: zu4_mobile_party(); break;
    case ZU4_MOBILE_ACTION_JOURNAL: zu4_mobile_journal(); break;
    case ZU4_MOBILE_ACTION_ENTER: zu4_mobile_enter(); break;
    case ZU4_MOBILE_ACTION_TALK: zu4_mobile_talk(); break;
    case ZU4_MOBILE_ACTION_PARTY_MEMBER: zu4_mobile_party_member(parameter); break;
    case ZU4_MOBILE_ACTION_MAP: zu4_mobile_exploration_map(); break;
    case ZU4_MOBILE_ACTION_CONTEXT: zu4_mobile_context(); break;
    case ZU4_MOBILE_ACTION_MOVE: zu4_mobile_move(parameter & 7, parameter & 0x100); break;
    case ZU4_MOBILE_ACTION_ADJACENT: zu4_mobile_adjacent_interaction(parameter); break;
    case ZU4_MOBILE_ACTION_DUNGEON_SEARCH: mobileDungeonCommand('s'); break;
    case ZU4_MOBILE_ACTION_DUNGEON_TORCH: mobileDungeonCommand('i'); break;
    case ZU4_MOBILE_ACTION_DUNGEON_VIEW: mobileToggleDungeonView(); break;
    case ZU4_MOBILE_ACTION_COMBAT_TARGET: mobileCombatSelectTarget(parameter); break;
    case ZU4_MOBILE_ACTION_COMBAT_CYCLE: mobileCombatCycleTarget(parameter); break;
    case ZU4_MOBILE_ACTION_COMBAT_CLEAR_TARGET: mobileCombatClearTarget(); break;
    case ZU4_MOBILE_ACTION_COMBAT_REPEAT_ATTACK: mobileCombatRepeatAttack(); break;
    case ZU4_MOBILE_ACTION_WALK_START: mobileStartWalk((uint32_t)parameter); break;
    case ZU4_MOBILE_ACTION_WALK_STEP: mobileWalkStep((uint32_t)parameter); break;
    default: break;
    }
}
#endif
#ifdef ZU4_IOS
class MobileTopicInput : public WaitableController<std::string> {
public:
    bool ready = false;
    bool submittedText = false;
    bool keyPressed(int) override { return true; }
    static void submit(const char *keyword, int textEntry, void *context) {
        MobileTopicInput *input = static_cast<MobileTopicInput *>(context);
        input->value = keyword;
        input->submittedText = textEntry != 0;
        input->ready = true;
        input->doneWaiting();
    }
    std::string read(const std::string &text, const std::vector<TopicJournal::Choice> &choices,
                     int maxLength, bool nameEntry = false, bool compactDetails = false,
                     Zu4TopicPanelStyle panelStyle = ZU4_TOPIC_PANEL_STANDARD) {
        struct PauseScope {
            PauseScope() { ++mobilePanelDepth; }
            ~PauseScope() {
                --mobilePanelDepth;
                // Reading time must not count toward the idle auto-pass timer.
                if (!mobilePanelDepth && c) c->lastCommandTime = time(NULL);
            }
        } pauseScope;
        std::vector<const char *> keys, labels;
        std::vector<int> roles;
        for (const auto &choice : choices) {
            keys.push_back(choice.keyword.c_str());
            labels.push_back(choice.title.c_str());
            roles.push_back((int)choice.role);
        }
        zu4_topic_panel_show(text.c_str(), keys.data(), labels.data(), roles.data(), (int)keys.size(),
                             maxLength, nameEntry, compactDetails, panelStyle, submit, this);
        if (ready) return value;
        eventHandler->pushController(this);
        return waitFor();
    }
};

static TopicJournal::Choice mobileBack(const std::string &keyword, const std::string &title) {
    return TopicJournal::Choice(keyword, title, TopicJournal::CHOICE_BACK);
}

static TopicJournal::Choice mobileDismiss(const std::string &keyword, const std::string &title) {
    return TopicJournal::Choice(keyword, title, TopicJournal::CHOICE_DISMISS);
}

static void mobileAppendPageControls(std::vector<TopicJournal::Choice> &choices,
                                     size_t page, size_t first, size_t last,
                                     size_t total, const std::string &noun) {
    choices.push_back({page ? "__page_previous" : "__page_previous_disabled", "‹ Previous"});
    choices.push_back({last < total ? "__page_next" : "__page_next_disabled", "Next ›"});
    std::string range = total ? std::to_string(first + 1) + "–" + std::to_string(last) : "0";
    choices.push_back({"__page_label", range + " of " + std::to_string(total) + " " + noun});
}

class MobileTestToolsInput : public WaitableController<std::string> {
public:
    bool ready = false;
    bool enabled = false;
    bool keyPressed(int) override { return true; }
    static void submit(const char *action, int value, void *context) {
        MobileTestToolsInput *input = static_cast<MobileTestToolsInput *>(context);
        input->value = action ? action : "done";
        input->enabled = value != 0;
        input->ready = true;
        input->doneWaiting();
    }
    std::string read(const Zu4TestToolsState &state) {
        struct PauseScope {
            PauseScope() { ++mobilePanelDepth; }
            ~PauseScope() {
                --mobilePanelDepth;
                if (!mobilePanelDepth && c) c->lastCommandTime = time(NULL);
            }
        } pauseScope;
        zu4_test_tools_panel_show(&state, submit, this);
        if (ready) return value;
        eventHandler->pushController(this);
        return waitFor();
    }
};

class MobileGemInput : public WaitableController<bool> {
public:
    bool ready = false;
    bool keyPressed(int key) override {
        if (key == U4_ENTER || key == U4_ESC || key == U4_SPACE)
            zu4_ios_dismiss_map();
        return true;
    }
    static void submit(void *context) {
        MobileGemInput *input = static_cast<MobileGemInput *>(context);
        input->value = true;
        input->ready = true;
        input->doneWaiting();
    }
    bool read(const std::vector<unsigned char> &rgba, int width, int height,
              int playerX, int playerY, const std::string &locationName) {
        struct PauseScope {
            PauseScope() { ++mobilePanelDepth; }
            ~PauseScope() {
                --mobilePanelDepth;
                if (!mobilePanelDepth && c) c->lastCommandTime = time(NULL);
            }
        } pauseScope;
        zu4_ios_show_gem_map(rgba.data(), width, height, playerX, playerY,
                             locationName.c_str(), submit, this);
        if (ready) return value;
        eventHandler->pushController(this);
        return waitFor();
    }
};

static bool mobileShowGemMap() {
    if (!c || !c->location || !c->location->map) return false;
    Map *map = c->location->map;
    if (!map->width || !map->height) return false;
    std::vector<unsigned char> rgba((std::size_t)map->width * map->height * 4, 255);
    bool dungeon = map->type == Map::DUNGEON;
    for (unsigned y = 0; y < map->height; ++y) for (unsigned x = 0; x < map->width; ++x) {
        Coords at = {(int)x, (int)y, c->location->coords.z};
        mobileTerrainColor(map->tileTypeAt(at, WITHOUT_OBJECTS),
                           &rgba[((std::size_t)y * map->width + x) * 4], dungeon);
    }
    std::string name = map->getName();
    if (name.empty()) name = map->isWorldMap() ? "Britannia" : "Current Area";
    if (dungeon) name += " — Level " + std::to_string(c->location->coords.z + 1);
    MobileGemInput input;
    return input.read(rgba, (int)map->width, (int)map->height,
                      c->location->coords.x, c->location->coords.y, name);
}

static std::string readMobileConversationTopic(const std::string &text,
                                               const std::vector<TopicJournal::Choice> &choices,
                                               int maxLength) {
    const size_t pageSize = 6;
    std::vector<TopicJournal::Choice> pageable;
    TopicJournal::Choice goodbye;
    for (const auto &choice : choices) {
        if (choice.keyword == "bye") goodbye = choice;
        else pageable.push_back(choice);
    }
    size_t page = 0;
    const size_t pageCount = (pageable.size() + pageSize - 1) / pageSize;
    for (;;) {
        const size_t first = page * pageSize;
        const size_t last = std::min(first + pageSize, pageable.size());
        std::vector<TopicJournal::Choice> visible(pageable.begin() + first, pageable.begin() + last);
        if (!goodbye.keyword.empty()) visible.push_back(goodbye);
        visible.push_back({page ? "__previous_topics" : "__previous_topics_disabled", "‹ Previous topics"});
        visible.push_back({page + 1 < pageCount ? "__next_topics" : "__next_topics_disabled", "More topics ›"});
        std::string pageText = text + "\n\nTopics " + std::to_string(first + 1) + "–" +
            std::to_string(last) + " of " + std::to_string(pageable.size());
        MobileTopicInput input;
        std::string result = input.read(pageText, visible, maxLength);
        if (result == "__previous_topics") { --page; continue; }
        if (result == "__next_topics") { ++page; continue; }
        return result;
    }
}
int gameSave(void);

static std::string mobileExperienceLabel() {
    std::string label = zu4_experience_profile_name(zu4_experience_profile());
    if (zu4_experience_is_customized()) label += " — Customized";
    return label;
}

static bool mobileCommitExperience(const Zu4ExperiencePreferences &oldPreferences,
                                   const SettingsData &oldSettings) {
    int newVideoType = settings.videoType;
    if (newVideoType == ZU4_GRAPHICS_VGA && !u4isUpgradeAvailable()) {
        experiencePreferences = oldPreferences;
        settings = oldSettings;
        return false;
    }
    if (!zu4_settings_write()) {
        experiencePreferences = oldPreferences;
        settings = oldSettings;
        return false;
    }
    if (newVideoType != oldSettings.videoType && !screenApplyVideoType(newVideoType)) {
        experiencePreferences = oldPreferences;
        settings = oldSettings;
        zu4_settings_write();
        screenApplyVideoType(oldSettings.videoType);
        return false;
    }
    return true;
}

static std::string mobileProfileSummary(Zu4ExperienceProfile profile) {
    Zu4ExperiencePreferences savedPreferences = experiencePreferences;
    SettingsData savedSettings = settings;
    zu4_experience_set_profile(profile, &settings);
    std::string summary = std::string(settings.videoType == ZU4_GRAPHICS_VGA
        ? "VGA color graphics" : "Original EGA graphics") + "\n" +
        (settings.filterMoveMessages ? "Routine movement messages filtered" : "All movement messages shown") + "\n" +
        (zu4_experience_exploration_map() ? "Exploration map available" : "No exploration map") + "\n" +
        (zu4_experience_map_pins() ? "Player map pins available" : "No player map pins");
    experiencePreferences = savedPreferences;
    settings = savedSettings;
    return summary;
}

static void mobileChooseExperienceProfile() {
    Zu4ExperienceProfile active = zu4_experience_profile();
    std::vector<TopicJournal::Choice> profiles = {
        {"classic", std::string(active == ZU4_EXPERIENCE_CLASSIC ? "✓ " : "") + "Classic — Original information boundaries"},
        {"ultimatum", std::string(active == ZU4_EXPERIENCE_ULTIMATUM ? "✓ " : "") + "Ultimatum — Recommended mobile experience"},
        {"assisted", std::string(active == ZU4_EXPERIENCE_ASSISTED ? "✓ " : "") + "Assisted — Additional optional guidance"},
        mobileBack("back", "Experience")
    };
    MobileTopicInput chooser;
    std::string choice = chooser.read(
        "Choose Experience Profile\n\nProfiles change convenience and presentation defaults, not game rules or your adventure.",
        profiles, 0, false, false, ZU4_TOPIC_PANEL_FULLSCREEN);
    Zu4ExperienceProfile proposed;
    if (!zu4_experience_profile_from_key(choice.c_str(), &proposed)) return;
    if (proposed == zu4_experience_profile() && !zu4_experience_is_customized()) return;

    std::string text = std::string("Apply ") + zu4_experience_profile_name(proposed) +
        "?\n\n" + mobileProfileSummary(proposed) +
        "\n\nAny individual Experience overrides will be cleared. Your adventure and accessibility preferences are unchanged.";
    MobileTopicInput confirmation;
    std::string confirmed = confirmation.read(text,
        {{"cancel", "Cancel"}, {"apply", std::string("Apply ") + zu4_experience_profile_name(proposed)}},
        0, false, false, ZU4_TOPIC_PANEL_FULLSCREEN);
    if (confirmed != "apply") return;

    Zu4ExperiencePreferences oldPreferences = experiencePreferences;
    SettingsData oldSettings = settings;
    zu4_experience_set_profile(proposed, &settings);
    MobileTopicInput result;
    if (mobileCommitExperience(oldPreferences, oldSettings))
        result.read(std::string("Experience Updated\n\nProfile changed to ") + mobileExperienceLabel() + ".",
            {mobileBack("back", "Experience")}, 0, false, false, ZU4_TOPIC_PANEL_FULLSCREEN);
    else
        result.read("Experience Unchanged\n\nThe profile could not be applied. Your previous settings are still active.",
            {mobileBack("back", "Experience")}, 0, false, false, ZU4_TOPIC_PANEL_FULLSCREEN);
}

static void mobileCustomizeExperience() {
    for (;;) {
        std::string graphics = settings.videoType == ZU4_GRAPHICS_VGA ? "VGA color" : "EGA original";
        std::string movement = settings.filterMoveMessages ? "Filtered" : "All shown";
        std::string explorationMap = zu4_experience_exploration_map() ? "On" : "Off";
        std::string mapPins = zu4_experience_map_pins() ? "On" : "Off";
        MobileTopicInput customization;
        std::string choice = customization.read(
            std::string("Experience Customization\n\nBased on ") +
                zu4_experience_profile_name(zu4_experience_profile()) +
                "\n\nPresentation",
            {{"graphics", "Graphics — " + graphics},
             {"movement", "Movement messages — " + movement},
             {"map", "Exploration map — " + explorationMap},
             {"pins", "Player map pins — " + mapPins},
             mobileBack("back", "Experience")}, 0, false, false, ZU4_TOPIC_PANEL_FULLSCREEN);
        if (choice == "back" || choice == "bye") return;

        Zu4ExperiencePreferences oldPreferences = experiencePreferences;
        SettingsData oldSettings = settings;
        if (choice == "graphics") {
            bool vgaAvailable = u4isUpgradeAvailable();
            MobileTopicInput picker;
            std::string selected = picker.read(
                "Graphics\n\nChanges take effect immediately. The VGA set is the 256-color Ultima IV Upgrade artwork.",
                {{"ega", std::string(settings.videoType == ZU4_GRAPHICS_EGA ? "✓ " : "") + "EGA — Original graphics"},
                 {vgaAvailable ? "vga" : "unavailable", std::string(settings.videoType == ZU4_GRAPHICS_VGA ? "✓ " : "") +
                    (vgaAvailable ? "VGA — 256-color upgrade" : "VGA — Unavailable in this build")},
                 mobileBack("back", "Experience")}, 0, false, false, ZU4_TOPIC_PANEL_FULLSCREEN);
            if (selected == "back" || selected == "bye") continue;
            if (selected == "unavailable") {
                MobileTopicInput unavailable;
                unavailable.read("Graphics\n\nThe VGA graphics files are not present in this build.",
                    {mobileBack("back", "Graphics")}, 0, false, false, ZU4_TOPIC_PANEL_FULLSCREEN);
                continue;
            }
            Zu4GraphicsTheme theme = selected == "vga" ? ZU4_GRAPHICS_VGA : ZU4_GRAPHICS_EGA;
            if ((int)theme == settings.videoType) continue;
            zu4_experience_set_graphics_theme(theme, &settings);
        } else if (choice == "movement") {
            MobileTopicInput picker;
            std::string selected = picker.read(
                "Movement Messages\n\nFiltering hides routine direction and blocked-movement text. It does not alter turns or movement rules.",
                {{"filtered", std::string(settings.filterMoveMessages ? "✓ " : "") + "Filtered"},
                 {"all", std::string(!settings.filterMoveMessages ? "✓ " : "") + "All shown"},
                 mobileBack("back", "Experience")}, 0, false, false, ZU4_TOPIC_PANEL_FULLSCREEN);
            if (selected == "back" || selected == "bye") continue;
            bool enabled = selected == "filtered";
            if (enabled == settings.filterMoveMessages) continue;
            zu4_experience_set_filter_movement_messages(enabled, &settings);
        } else if (choice == "map") {
            MobileTopicInput picker;
            bool enabled = zu4_experience_exploration_map();
            std::string selected = picker.read(
                "Exploration Map\n\nShows only overworld terrain already seen and dungeon tiles the party has reached. It never reveals unknown locations or dungeon layout.",
                {{"on", std::string(enabled ? "✓ " : "") + "On"},
                 {"off", std::string(!enabled ? "✓ " : "") + "Off"},
                 mobileBack("back", "Experience")}, 0, false, false, ZU4_TOPIC_PANEL_FULLSCREEN);
            if (selected == "back" || selected == "bye") continue;
            bool newValue = selected == "on";
            if (newValue == enabled) continue;
            zu4_experience_set_exploration_map(newValue, &settings);
        } else if (choice == "pins") {
            MobileTopicInput picker;
            bool enabled = zu4_experience_map_pins();
            std::string selected = picker.read(
                "Player Map Pins\n\nLets you attach your own short labels to places you have already reached. Turning pins off keeps existing labels for later.",
                {{"on", std::string(enabled ? "✓ " : "") + "On"},
                 {"off", std::string(!enabled ? "✓ " : "") + "Off"},
                 mobileBack("back", "Experience")}, 0, false, false, ZU4_TOPIC_PANEL_FULLSCREEN);
            if (selected == "back" || selected == "bye") continue;
            bool newValue = selected == "on";
            if (newValue == enabled) continue;
            zu4_experience_set_map_pins(newValue, &settings);
        } else continue;

        if (!mobileCommitExperience(oldPreferences, oldSettings)) {
            MobileTopicInput failed;
            failed.read("Experience Unchanged\n\nThat setting could not be applied. Your previous settings are still active.",
                {mobileBack("back", "Experience")}, 0, false, false, ZU4_TOPIC_PANEL_FULLSCREEN);
        }
    }
}

static void mobileAudioMenu() {
    for (;;) {
        const Zu4Soundtrack *pack = zu4_soundtrack(settings.soundtrack);
        MobileTopicInput menu;
        std::string action = menu.read("Audio\n\nPreferences apply to all adventures.",
            {{"music", "Music volume\n" + std::to_string(settings.musicVol * 10) + "%"},
             {"effects", "Sound effects\n" + std::to_string(settings.soundVol * 10) + "%"},
             {"credits", "Music and graphics\nCredits"}, mobileBack("back", "Experience")},
            0, false, false, ZU4_TOPIC_PANEL_DENSE_FULLSCREEN);
        if (action == "back" || action == "bye") return;
        if (action == "credits") {
            MobileTopicInput credits;
            credits.read(std::string(pack ? pack->credits : "No soundtrack loaded.") +
                "\n\nVGA artwork: Joshua Steele (Wiltshire Dragon). Ultima IV Upgrade: Ryan Wiener (Aradindae Dragon). Graphics-only upgrade; original game files and saves are preserved.\n\nPix's Ultima Patcher collects these independent patches; it is not executed by this app.",
                {mobileBack("back", "Audio")}, 0, false, false, ZU4_TOPIC_PANEL_READING_FULLSCREEN);
            continue;
        }
        if (action != "music" && action != "effects") continue;
        bool music = action == "music";
        int old = music ? settings.musicVol : settings.soundVol;
        std::vector<TopicJournal::Choice> choices;
        for (int i = 0; i <= MAX_VOLUME; i += 2)
            choices.push_back({std::to_string(i), std::string(old == i ? "✓ " : "") +
                (i ? std::to_string(i * 10) + "%\nVolume" : "Off\nMuted")});
        choices.push_back(mobileBack("back", "Audio"));
        MobileTopicInput picker;
        std::string selected = picker.read(music ? "Music volume" : "Sound effects volume", choices,
            0, false, false, ZU4_TOPIC_PANEL_DENSE_FULLSCREEN);
        if (selected == "back" || selected == "bye") continue;
        if (selected != "0" && selected != "2" && selected != "4" && selected != "6" && selected != "8" && selected != "10") continue;
        int volume = atoi(selected.c_str());
        if (volume < 0 || volume > MAX_VOLUME) continue;
        if (music) settings.musicVol = volume; else settings.soundVol = volume;
        if (!zu4_settings_write()) {
            if (music) settings.musicVol = old; else settings.soundVol = old;
            MobileTopicInput error;
            error.read("Audio unchanged\n\nThe preference could not be saved.", {mobileBack("back", "Audio")},
                0, false, false, ZU4_TOPIC_PANEL_FULLSCREEN);
        } else if (music) {
            zu4_music_vol((double)volume / MAX_VOLUME);
            zu4_music_set_enabled(volume > 0);
        } else zu4_snd_vol((double)volume / MAX_VOLUME);
    }
}
#ifdef ZU4_IOS_AUDIO_RUNTIME_TESTS
extern "C" void zu4_audio_runtime_menu() { mobileAudioMenu(); }
#endif

static void mobileExperienceMenu() {
    for (;;) {
        std::vector<TopicJournal::Choice> choices = {
            {"profile", "Choose profile"}, {"customize", "Customize"}, {"audio", "Audio"}
        };
        if (zu4_experience_is_customized()) choices.push_back({"restore", "Restore Profile Defaults"});
        choices.push_back(mobileBack("back", "Menu"));
        MobileTopicInput menu;
        std::string choice = menu.read(
            std::string("Experience\n\n") + mobileExperienceLabel() +
            "\n\nProfiles control convenience and presentation. They never create a different adventure or change the game rules.",
            choices, 0, false, false, ZU4_TOPIC_PANEL_FULLSCREEN);
        if (choice == "back" || choice == "bye") return;
        if (choice == "profile") mobileChooseExperienceProfile();
        else if (choice == "audio") mobileAudioMenu();
        else if (choice == "customize") mobileCustomizeExperience();
        else if (choice == "restore") {
            Zu4ExperiencePreferences oldPreferences = experiencePreferences;
            SettingsData oldSettings = settings;
            zu4_experience_restore_defaults(&settings);
            if (!mobileCommitExperience(oldPreferences, oldSettings)) {
                MobileTopicInput failed;
                failed.read("Experience Unchanged\n\nProfile defaults could not be restored. Your previous settings are still active.",
                    {mobileBack("back", "Experience")}, 0, false, false, ZU4_TOPIC_PANEL_FULLSCREEN);
            }
        }
    }
}

static bool mobileCommitControlSetting(const SettingsData &oldSettings) {
    if (zu4_settings_write()) {
        zu4_ios_refresh_controls();
        return true;
    }
    settings = oldSettings;
    return false;
}

static void mobileInteractionControlsMenu() {
    for (;;) {
        MobileTopicInput menu;
        std::string choice = menu.read(
            "Touch interactions\n\nChoose optional touch interactions.",
            {{"bump", std::string(settings.bumpInteractions ? "✓ " : "") + "Bump to interact"},
             {"direct", std::string(settings.directInteractions ? "✓ " : "") + "Tap to interact"},
             {"walk", std::string(settings.tapToWalk ? "✓ " : "") + "Tap to walk"},
             {"help", "How interaction works"}, mobileBack("back", "Controls")},
            0, false, false, ZU4_TOPIC_PANEL_FULLSCREEN);
        if (choice == "back" || choice == "bye") return;
        if (choice == "help") {
            MobileTopicInput help;
            help.read("Interaction controls\n\nBump to interact: press once toward a friendly person or unlocked door. This also reaches a person across a counter or sign when the game allows talking over it. Holding an arrow never repeats the interaction.\n\nTap to interact: tap a visible friendly person, door, or chest. With Tap to walk enabled, the Avatar takes a short safe route to the correct interaction position, then talks, opens, or collects.\n\nTap to walk: tap any other visible map tile to follow a short safe route. It stops at blocked or hazardous terrain, encounters, prompts, and other interruptions. Tap another destination or use any control to cancel.\n\nA locked door reports itself and points to Unlock under Explore; no key is spent automatically. Hostile creatures are never activated automatically. The D-pad, Talk, and Explore remain explicit alternatives.",
                {mobileBack("back", "Controls")}, 0, false, false, ZU4_TOPIC_PANEL_FULLSCREEN);
            continue;
        }
        SettingsData oldSettings = settings;
        if (choice == "bump") settings.bumpInteractions = !settings.bumpInteractions;
        else if (choice == "direct") settings.directInteractions = !settings.directInteractions;
        else if (choice == "walk") settings.tapToWalk = !settings.tapToWalk;
        else continue;
        if (!mobileCommitControlSetting(oldSettings)) {
            MobileTopicInput failed;
            failed.read("Controls unchanged\n\nThat preference could not be saved. Your previous controls are still active.",
                {mobileBack("back", "Controls")}, 0, false, false, ZU4_TOPIC_PANEL_FULLSCREEN);
        }
    }
}

static void mobileControlsMenu() {
    static const char *sizeNames[] = {"Small", "Standard", "Large"};
    for (;;) {
        MobileTopicInput menu;
        std::string choice = menu.read("Controls\n\nPreferences apply to all adventures.",
            {{"size", std::string("D-pad size: ") + sizeNames[settings.dpadSize]},
             {"flip", std::string("Flip controls: ") + (settings.flipControls ? "On" : "Off")},
             {"combat", std::string("Combat pacing: ") + (settings.fastCombatPresentation ? "Fast" : "Standard")},
             {"interactions", "Touch interactions"}, mobileBack("back", "Menu")},
            0, false, false, ZU4_TOPIC_PANEL_FULLSCREEN);
        if (choice == "back" || choice == "bye") return;
        if (choice == "interactions") { mobileInteractionControlsMenu(); continue; }
        SettingsData oldSettings = settings;
        if (choice == "flip") settings.flipControls = !settings.flipControls;
        else if (choice == "combat") {
            MobileTopicInput picker;
            std::string pacing = picker.read("Combat pacing\n\nFast shortens flashes and removes repeated turn-start narration. All results, costs, and turn rules stay unchanged.",
                {{"standard", std::string(!settings.fastCombatPresentation ? "✓ " : "") + "Standard"},
                 {"fast", std::string(settings.fastCombatPresentation ? "✓ " : "") + "Fast"},
                 mobileBack("back", "Controls")}, 0, false, false, ZU4_TOPIC_PANEL_FULLSCREEN);
            if (pacing != "standard" && pacing != "fast") continue;
            settings.fastCombatPresentation = pacing == "fast";
        }
        else if (choice == "size") {
            std::vector<TopicJournal::Choice> choices;
            for (int i = 0; i < 3; ++i)
                choices.push_back({std::to_string(i), std::string(settings.dpadSize == i ? "✓ " : "") + sizeNames[i]});
            choices.push_back(mobileBack("back", "Controls"));
            MobileTopicInput picker;
            std::string size = picker.read("D-pad size\n\nLarger sizes fit available space without covering the world.",
                choices, 0, false, false, ZU4_TOPIC_PANEL_FULLSCREEN);
            if (size == "back" || size == "bye") continue;
            if (size != "0" && size != "1" && size != "2") continue;
            settings.dpadSize = atoi(size.c_str());
        } else continue;
        if (!mobileCommitControlSetting(oldSettings)) {
            MobileTopicInput failed;
            failed.read("Controls unchanged\n\nThat preference could not be saved.",
                {mobileBack("back", "Controls")}, 0, false, false, ZU4_TOPIC_PANEL_FULLSCREEN);
        }
    }
}

static std::string mobileTestPagedPicker(const std::string &title,
                                         const std::vector<TopicJournal::Choice> &all) {
    const size_t pageSize = 6;
    size_t page = 0;
    for (;;) {
        size_t first = page * pageSize;
        size_t last = std::min(first + pageSize, all.size());
        std::vector<TopicJournal::Choice> visible(all.begin() + first, all.begin() + last);
        if (page) visible.push_back({"__test_previous", "‹ Previous"});
        if (last < all.size()) visible.push_back({"__test_next", "Next ›"});
        visible.push_back(mobileBack("cancel", "Tools"));
        MobileTopicInput picker;
        std::string choice = picker.read(title + "\n\nShowing " + std::to_string(first + 1) + "–" +
            std::to_string(last) + " of " + std::to_string(all.size()), visible, 0,
            false, false, ZU4_TOPIC_PANEL_FULLSCREEN);
        if (choice == "__test_previous") { --page; continue; }
        if (choice == "__test_next") { ++page; continue; }
        return choice;
    }
}

static bool mobileTestConfirmDanger(const std::string &title, const std::string &detail) {
    MobileTopicInput confirmation;
    return confirmation.read(title + "\n\n" + detail,
        {{"cancel", "Cancel"}, {"apply", title}}, 0, false, false,
        ZU4_TOPIC_PANEL_FULLSCREEN) == "apply";
}

static bool mobileTestPrepareChange(bool &prepared, bool combat) {
    if (prepared) return true;
    bool canCheckpoint = !combat && (c->location->context & CTX_CAN_SAVE_GAME);
    MobileTopicInput confirmation;
    std::string detail = canCheckpoint
        ? "A recovery checkpoint for Slot " + std::to_string(gameActiveSaveSlot()) +
          " will be created before this Debug Tools session changes the adventure."
        : "Saving is unavailable in this context, so this test change cannot create a recovery checkpoint.";
    if (confirmation.read("Apply a test change?\n\n" + detail,
        {{"cancel", "Cancel"},
         {"apply", canCheckpoint ? "Create checkpoint and apply" : "Apply without checkpoint"}},
        0, false, false, ZU4_TOPIC_PANEL_FULLSCREEN) != "apply") return false;
    if (canCheckpoint && !gameSave()) {
        MobileTopicInput failed;
        failed.read("Test change cancelled\n\nThe recovery checkpoint could not be created.",
            {mobileBack("back", "Debug Tools")}, 0, false, false, ZU4_TOPIC_PANEL_FULLSCREEN);
        return false;
    }
    prepared = true;
    return true;
}

static void mobileTestCheatKey(int key) {
    CheatMenuController command(game);
    command.keyPressed(key);
}

static void mobileTestGameDebugKey(int key) {
    bool oldDebug = settings.debug;
    settings.debug = true;
    game->keyPressed(key);
    settings.debug = oldDebug;
}

static std::vector<TopicJournal::Choice> mobileTestWorldDestinations() {
    std::vector<TopicJournal::Choice> choices;
    std::vector<std::string> seen;
    Map *world = mapMgr->get(MAP_WORLD);
    for (size_t i = 0; world && i < world->portals.size(); ++i) {
        std::string name = mapMgr->get(world->portals[i]->destid)->getName();
        if (name.empty() || std::find(seen.begin(), seen.end(), name) != seen.end()) continue;
        seen.push_back(name);
        choices.push_back({"portal:" + std::to_string(i), name});
    }
    return choices;
}

static void mobileTestTeleportToWorldPortal(int portalIndex) {
    Map *world = mapMgr->get(MAP_WORLD);
    if (!world || portalIndex < 0 || portalIndex >= (int)world->portals.size()) return;
    while (!c->location->map->isWorldMap())
        if (!game->exitToParentMap()) return;
    c->location->coords = world->portals[portalIndex]->coords;
    c->location->viewMode = VIEW_NORMAL;
    gameUpdateScreen();
}

static Zu4TestToolsState mobileTestToolsState(CombatController *combat) {
    Zu4TestToolsState state = {};
    Coords coords = c->location->coords;
    state.activeSlot = gameActiveSaveSlot();
    state.location = nullptr; // Assigned from stable storage immediately before presentation.
    state.x = coords.x;
    state.y = coords.y;
    state.z = coords.z;
    state.worldMap = c->location->map->isWorldMap();
    state.dungeon = (c->location->context & CTX_DUNGEON) != 0;
    state.combat = combat != nullptr;
    state.canDungeonTeleport = state.worldMap && (c->transportContext & TRANSPORT_FOOT_OR_HORSE);
    state.collisionOverride = collisionOverride;
    state.seeThroughWalls = !c->opacity;
    state.windLocked = c->windLock;
    state.windDirection = getDirectionName((Direction)c->windDirection);
    state.torchDuration = c->party->getTorchDuration();
    return state;
}

static bool mobileTestToolsMenu(CombatController *combat) {
    bool preparedChange = false;
    std::string page = "root";
    for (;;) {
        std::string locationName = c->location->map->getName();
        Zu4TestToolsState state = mobileTestToolsState(combat);
        // Keep the backing storage alive throughout the nested UIKit event loop.
        state.page = page.c_str();
        state.location = locationName.empty() ? "Britannia" : locationName.c_str();
        MobileTestToolsInput input;
        std::string action = input.read(state);
        if (action == "done" || action.empty()) {
            zu4_test_tools_panel_dismiss();
            return false;
        }
        if (action == "back") {
            page = "root";
            continue;
        }
        if (action.compare(0, 5, "page_") == 0) {
            page = action.substr(5);
            continue;
        }

        if (action == "collision") {
            if ((bool)collisionOverride != input.enabled) mobileTestCheatKey('c');
            continue;
        }
        if (action == "opacity") {
            if ((bool)!c->opacity != input.enabled) mobileTestCheatKey('o');
            continue;
        }
        if (action == "peer_preview") {
            mobileShowGemMap();
            continue;
        }
        if (action == "wind_lock") {
            c->windLock = input.enabled;
            screenMessage("Wind direction is %slocked!\n", c->windLock ? "" : "un");
            continue;
        }
        if (action == "virtue_values") {
            std::string values = "Virtue values\n\n";
            for (int i = 0; i < 8; ++i)
                values += std::string(getVirtueName((Virtue)i)) + " — " +
                    (c->saveGame->karma[i] ? std::to_string(c->saveGame->karma[i]) : "Avatar") + "\n";
            MobileTopicInput details;
            details.read(values, {mobileBack("back", "Debug Tools")}, 0, false, false,
                         ZU4_TOPIC_PANEL_FULLSCREEN);
            continue;
        }
        if (action == "wind") {
            MobileTopicInput picker;
            std::string direction = picker.read("Set wind direction",
                {{"north", "North ↑"}, {"east", "East →"}, {"south", "South ↓"},
                 {"west", "West ←"}, mobileBack("cancel", "Tools")}, 0, false, false,
                 ZU4_TOPIC_PANEL_FULLSCREEN);
            if (direction != "cancel" && direction != "bye" &&
                mobileTestPrepareChange(preparedChange, false)) {
                c->windDirection = direction == "north" ? DIR_NORTH : direction == "east" ? DIR_EAST :
                    direction == "south" ? DIR_SOUTH : DIR_WEST;
                screenMessage("Wind %s!\n", getDirectionName((Direction)c->windDirection));
            }
            continue;
        }
        if (action == "moongate") {
            std::vector<TopicJournal::Choice> gates;
            for (int i = 1; i <= 8; ++i) gates.push_back({std::to_string(i), "Moongate " + std::to_string(i)});
            std::string gate = mobileTestPagedPicker("Go to moongate", gates);
            if (gate != "cancel" && mobileTestPrepareChange(preparedChange, false))
                mobileTestCheatKey(gate[0]);
            continue;
        }
        if (action == "dungeon") {
            Map *world = mapMgr->get(MAP_WORLD);
            std::vector<TopicJournal::Choice> dungeons;
            for (int i = 0; i < 8; ++i) {
                int portal = 16 + i;
                std::string label = portal < (int)world->portals.size()
                    ? mapMgr->get(world->portals[portal]->destid)->getName() : "Dungeon " + std::to_string(i + 1);
                dungeons.push_back({std::to_string(i), label});
            }
            std::string selected = mobileTestPagedPicker("Go to dungeon entrance", dungeons);
            if (selected != "cancel" && mobileTestPrepareChange(preparedChange, false))
                mobileTestGameDebugKey(U4_FKEY + atoi(selected.c_str()));
            continue;
        }
        if (action == "altar") {
            MobileTopicInput picker;
            std::string selected = picker.read("Go to altar room",
                {{"0", "Truth"}, {"1", "Love"}, {"2", "Courage"}, mobileBack("cancel", "Tools")},
                0, false, false, ZU4_TOPIC_PANEL_FULLSCREEN);
            if (selected != "cancel" && mobileTestPrepareChange(preparedChange, false))
                mobileTestGameDebugKey(U4_FKEY + 8 + atoi(selected.c_str()));
            continue;
        }
        if (action == "goto") {
            std::vector<TopicJournal::Choice> destinations = mobileTestWorldDestinations();
            std::string selected = mobileTestPagedPicker("Go to location", destinations);
            if (selected.compare(0, 7, "portal:") == 0 && mobileTestPrepareChange(preparedChange, false))
                mobileTestTeleportToWorldPortal(atoi(selected.c_str() + 7));
            continue;
        }
        if (action == "summon") {
            MobileTopicInput picker;
            std::string creature = picker.read("Summon creature\n\nChoose a common creature or enter another name.",
                {{"rat", "Rat"}, {"orc", "Orc"}, {"troll", "Troll"}, {"dragon", "Dragon"},
                 {"daemon", "Daemon"}, {"pirate", "Pirate"}, {"sea serpent", "Sea serpent"},
                 mobileBack("cancel", "Tools")}, 32, false, false, ZU4_TOPIC_PANEL_FULLSCREEN);
            if (creature != "cancel" && creature != "bye" && mobileTestPrepareChange(preparedChange, false))
                cheatSummonCreature(creature);
            continue;
        }
        if (action == "transport") {
            MobileTopicInput picker;
            std::string transport = picker.read("Create transport",
                {{"h", "Horse"}, {"s", "Ship"}, {"b", "Balloon"}, mobileBack("cancel", "Tools")},
                0, false, false, ZU4_TOPIC_PANEL_FULLSCREEN);
            if (transport == "cancel" || transport == "bye") continue;
            // Direction selection is intentionally game-HUD based; reveal it
            // only after the native picker has completed.
            zu4_test_tools_panel_dismiss();
            Direction direction = gameGetDirection("Place the transport in which direction?");
            if (direction != DIR_NONE && mobileTestPrepareChange(preparedChange, false))
                cheatCreateTransport(transport[0], direction);
            continue;
        }

        bool danger = action == "destroy" || action == "clear_creatures" ||
            action == "end_combat" || action == "final_altar";
        if (danger) {
            std::string detail = action == "destroy" ? "The selected adjacent object or creature will be removed." :
                action == "clear_creatures" ? "Every non-protected creature on the current map will be removed." :
                action == "end_combat" ? "The current battle will end immediately without adjusting virtue." :
                "This bypasses the journey and places the party at the final altar.";
            if (!mobileTestConfirmDanger(action == "destroy" ? "Destroy nearby object" :
                action == "clear_creatures" ? "Destroy all creatures" :
                action == "end_combat" ? "End combat immediately" : "Go to the final altar", detail)) continue;
        }

        bool persistentChange = action == "equipment" || action == "stats" || action == "items" ||
            action == "reagents" || action == "mixtures" || action == "companions" ||
            action == "virtues" || action == "moons" || action == "lord_british" ||
            action == "exit_map" || action == "destroy" || action == "clear_creatures" ||
            action == "final_altar";
        if (persistentChange && !mobileTestPrepareChange(preparedChange, combat != nullptr)) continue;

        if (action == "equipment") mobileTestCheatKey('e');
        else if (action == "stats") mobileTestCheatKey('f');
        else if (action == "items") mobileTestCheatKey('i');
        else if (action == "reagents") mobileTestCheatKey('r');
        else if (action == "mixtures") mobileTestCheatKey('m');
        else if (action == "companions") mobileTestCheatKey('j');
        else if (action == "virtues") mobileTestCheatKey('v');
        else if (action == "moons") mobileTestCheatKey('a');
        else if (action == "lord_british") mobileTestGameDebugKey(U4_CTRL + 'h');
        else if (action == "exit_map") mobileTestCheatKey('x');
        else if (action == "destroy") {
            // destroy() asks for a direction through the gameplay HUD.
            zu4_test_tools_panel_dismiss();
            destroy();
        }
        else if (action == "clear_creatures") {
            extern void gameDestroyAllCreatures(void);
            gameDestroyAllCreatures();
        } else if (action == "end_combat" && combat) {
            zu4_test_tools_panel_dismiss();
            combat->end(false);
            return true;
        } else if (action == "final_altar") mobileTestGameDebugKey(U4_ALT + 'c');
    }
}

extern "C" void zu4_mobile_menu(void) {
    if (!game) return;
    if (CombatController *combat = dynamic_cast<CombatController *>(eventHandler->getController())) {
        for (;;) {
            PartyMember *active = combat->getCurrentPlayer();
            std::string text = "Battle paused\n\n";
            if (active) text += active->getName() + " is taking this turn.\n\n";
            for (int i = 0; i < c->saveGame->members; ++i) {
                PartyMember *member = c->party->member(i);
                text += member->getName() + " — " + mobileCondition(member->getStatus()) +
                    "\nHealth " + std::to_string(member->getHp()) + "/" + std::to_string(member->getMaxHp()) +
                    "    Magic " + std::to_string(member->getMp()) + "\n\n";
            }
            std::vector<TopicJournal::Choice> actions = {mobileDismiss("resume", "Resume"), {"item", "Use a quest item"},
                {"experience", "Experience — " + mobileExperienceLabel()}, {"controls", "Controls"}};
            if (zu4_test_tools_available()) actions.push_back({"test_tools", "Debug Tools"});
            Coords position;
            c->location->getCurrentPosition(&position);
            const Tile *ground = c->location->map->tileTypeAt(position, WITH_GROUND_OBJECTS);
            Object *object = c->location->map->objectAt(position);
            if (ground->isChest() || (object && object->getTile().getTileType()->isChest()))
                actions.push_back({"chest", "Open chest — uses turn"});
            actions.push_back({"help", "Battle controls"});
            MobileTopicInput menu;
            std::string choice = menu.read(text, actions, 0);
            if (choice == "experience") { mobileExperienceMenu(); continue; }
            if (choice == "controls") { mobileControlsMenu(); continue; }
            if (choice == "test_tools") {
                if (mobileTestToolsMenu(combat)) return;
                continue;
            }
            if (choice == "item" || choice == "chest") {
                combat->notifyKeyPressed(choice == "item" ? 'u' : 'g');
                return;
            }
            if (choice != "help") return;
            MobileTopicInput help;
            help.read("Battle controls\n\nTap a highlighted combatant or use Previous/Next Target, then Attack to confirm. Repeat Attack in the center of the arrows previews this fighter's last attack target when it is still reachable with the same weapon. Attack confirms; Clear cancels without using a turn.\n\nUse the arrows to move the active character one tile. Weapons that need empty-tile placement still offer direction and distance choices.\n\nSpells opens the active character's spellbook. Cancelling attack or spell selection keeps your turn. Wait passes this character's turn. Controls offers Standard/Fast combat pacing without changing the rules.\n\nTo flee, move each character off the battlefield edge. Saving is unavailable during battle. Opening this menu pauses play.",
                {mobileBack("back", "Battle menu")}, 0);
        }
    }
    if (eventHandler->getController() != game) return;
    for (;;) {
        std::vector<TopicJournal::Choice> actions;
        bool canSave = (c->location->context & CTX_CAN_SAVE_GAME) != 0;
        if (canSave) actions.push_back({"save", "Save to Slot " + std::to_string(gameActiveSaveSlot())});
        actions.push_back({"explore", "Explore"});
        actions.push_back({"travel", "Travel"});
        if (c->location->context == CTX_DUNGEON)
            actions.push_back({"journal", "Journal"});
        actions.push_back({"experience", "Experience"});
        actions.push_back({"controls", "Controls"});
        if (zu4_test_tools_available()) actions.push_back({"test_tools", "Debug Tools"});
        actions.push_back({"account", "Account"});
        actions.push_back({"backups", "Adventure backups — Import / Export"});
        actions.push_back(mobileDismiss("resume", "Resume"));
        MobileTopicInput menu;
        std::string chosen = menu.read(
            canSave ? "Adventure paused" : "Adventure paused\n\nSaving is unavailable here.",
            actions, 0, false, false, ZU4_TOPIC_PANEL_COMPACT_MENU);
        if (chosen == "resume" || chosen == "bye") return;
        if (chosen == "test_tools") {
            mobileTestToolsMenu(nullptr);
        } else if (chosen == "experience") {
            mobileExperienceMenu();
        } else if (chosen == "controls") {
            mobileControlsMenu();
        } else if (chosen == "account") {
            mobileAccount(true);
        } else if (chosen == "backups") {
            mobileAdventureBackups(true);
        } else if (chosen == "journal") {
            zu4_mobile_journal();
            return;
        } else if (chosen == "explore" || chosen == "travel") {
            std::vector<TopicJournal::Choice> commands;
            if (chosen == "explore") {
                commands = {{"s", "Search this location"}, {"u", "Use a quest item"}, {"o", "Open a door"},
                    {"j", "Unlock a door"}, {"g", "Open a chest"}, {"h", "Make camp"}};
                if (c->location->context == CTX_DUNGEON)
                    commands.push_back({"i", "Light a torch"});
                if (c->saveGame->gems > 0) commands.push_back({"p", "Peer through a gem"});
            } else {
                commands = {{"k", "Climb / ascend"}, {"d", "Descend / land"}};
                if (c->transportContext == TRANSPORT_FOOT) commands.push_back({"b", "Board transport"});
                else if (!c->party->isFlying()) commands.push_back({"x", "Leave transport"});
                if (c->transportContext == TRANSPORT_SHIP) commands.push_back({"f", "Fire cannon"});
                if (c->transportContext == TRANSPORT_HORSE)
                    commands.push_back({"y", c->horseSpeed ? "Slow horse" : "Urge horse onward"});
                if (c->saveGame->sextants > 0) commands.push_back({"l", "Locate with sextant"});
            }
            commands.push_back(mobileBack("back", "Menu"));
            MobileTopicInput input;
            std::string command = input.read(chosen == "explore" ? "Explore and interact" : "Travel and transport", commands, 0);
            if (command.size() == 1) {
                // Dispatch only a command actually offered in this context.
                for (const auto &entry : commands) {
                    if (entry.keyword == command) {
                        game->notifyKeyPressed(command[0]);
                        return;
                    }
                }
            }
        } else if (chosen == "save" && (c->location->context & CTX_CAN_SAVE_GAME)) {
            bool worldSaved = gameSave() != 0;
            MobileTopicInput result;
            result.read(worldSaved ? "Adventure saved to Slot " + std::to_string(gameActiveSaveSlot()) + ".\n\nYour party, equipment, mixtures, journal, overworld and dungeon exploration, pins, discovered places, and recovery checkpoint are isolated from the other slots." : "Saving did not finish successfully.\n\nKeep the app open and try again before leaving your adventure.", {mobileBack("back", "Menu")}, 0);
        }
    }
}
extern "C" void zu4_mobile_party(void) {
    if (!game) return;
    Controller *owner = eventHandler->getController();
    CombatController *combat = dynamic_cast<CombatController *>(owner);
    if (owner != game && !combat) return;
    int initialMember = mobilePartyInitialMember;
    mobilePartyInitialMember = -1;
    for (;;) {
        std::vector<TopicJournal::Choice> members;
        for (int i = 0; i < c->party->size(); ++i) {
            PartyMember *p = c->party->member(i);
            members.push_back({std::to_string(i),
                p->getName() + " · " + mobileCondition(p->getStatus()) +
                " · HP " + std::to_string(p->getHp()) + "/" + std::to_string(p->getMaxHp()) +
                " · MP " + std::to_string(p->getMp()) + "/" + std::to_string(p->getMaxMp()) + "\n" +
                p->getWeapon()->name + " · " + p->getArmor()->name});
        }
        if (!combat && c->party->size() > 2) members.push_back({"reorder", "Reorder companions"});
        members.push_back(mobileDismiss("close", "Close"));
        std::string selected;
        if (initialMember >= 0) {
            selected = std::to_string(initialMember);
            initialMember = -1;
        } else {
            MobileTopicInput picker;
            selected = picker.read(combat
                ? "Battle party\n\nInspect companions. Only the active fighter can change weapons; changing one spends the turn."
                : "Party", members, 0,
                false, false, ZU4_TOPIC_PANEL_DENSE_FULLSCREEN);
        }
        if (selected == "close" || selected == "bye") return;
        if (selected == "reorder") {
            std::vector<TopicJournal::Choice> companions;
            for (int i = 1; i < c->party->size(); ++i)
                companions.push_back({std::to_string(i), c->party->member(i)->getName()});
            companions.push_back(mobileBack("cancel", "Party"));
            companions.push_back(mobileDismiss("close", "Close"));
            MobileTopicInput first;
            std::string from = first.read("Reorder companions\n\nChoose the first companion to exchange. The Avatar remains leader.",
                companions, 0, false, false, ZU4_TOPIC_PANEL_DENSE_FULLSCREEN);
            if (from == "close") return;
            if (from == "cancel" || from == "bye") continue;
            companions.erase(std::remove_if(companions.begin(), companions.end(), [&](const TopicJournal::Choice &choice) {
                return choice.keyword == from;
            }), companions.end());
            MobileTopicInput second;
            std::string to = second.read("Exchange companions\n\nChoose the second companion. Confirming uses one turn.",
                companions, 0, false, false, ZU4_TOPIC_PANEL_DENSE_FULLSCREEN);
            if (to == "close") return;
            if (to == "cancel" || to == "bye") continue;
            int a = atoi(from.c_str()), b = atoi(to.c_str());
            if (a > 0 && b > 0 && a < c->party->size() && b < c->party->size() && a != b) {
                c->party->swapPlayers(a, b);
                screenMessage("Companions exchanged places.\n");
                if (eventHandler->getController() == owner) c->location->turnCompleter->finishTurn();
                return;
            }
            continue;
        }
        int index = atoi(selected.c_str());
        if (index < 0 || index >= c->party->size()) return;
        PartyMember *p = c->party->member(index);
        for (;;) {
            char info[640];
            snprintf(info, sizeof(info), "%s\n\n%s • %s\nHealth %d/%d   Magic %d/%d\nSTR %d   DEX %d   INT %d   XP %d\nWeapon: %s\nArmour: %s",
                p->getName().c_str(), getClassName(p->getClass()), mobileCondition(p->getStatus()), p->getHp(), p->getMaxHp(), p->getMp(), p->getMaxMp(),
                p->getStr(), p->getDex(), p->getInt(), p->getExp(), p->getWeapon()->name, p->getArmor()->name);
            MobileTopicInput detail;
            std::vector<TopicJournal::Choice> actions;
            if (!combat || p == combat->getCurrentPlayer()) actions.push_back({"weapon", combat ? "Change weapon — uses turn" : "Change weapon"});
            if (!combat) actions.push_back({"armor", "Change armour"});
            actions.push_back(mobileBack("back", "Party"));
            actions.push_back(mobileDismiss("close", "Close"));
            std::string action = detail.read(info, actions, 0, false, true,
                ZU4_TOPIC_PANEL_DENSE_FULLSCREEN);
            if (action == "close" || action == "bye") return;
            if (action == "back") break;
            bool weapon = action == "weapon";
            std::vector<TopicJournal::Choice> equipment;
            int limit = weapon ? WEAP_MAX : ARMR_MAX;
            for (int item = 0; item < limit; ++item) {
                int equipped = weapon ? (int)p->getWeapon()->type : (int)p->getArmor()->type;
                if (item == equipped) continue;
                int count = weapon ? c->saveGame->weapons[item] : c->saveGame->armor[item];
                if (item != 0 && count <= 0) continue;
                std::string name = weapon ? zu4_weapon_name((WeaponType)item) : zu4_armor_name((ArmorType)item);
                bool allowed = weapon ? zu4_weapon_usable((WeaponType)item, p->getClass()) : zu4_armor_wearable((ArmorType)item, p->getClass());
                if (!allowed) continue;
                equipment.push_back({std::to_string(item), name + (item ? " — " + std::to_string(count) + " spare" : "")});
            }
            bool hasEquipment = !equipment.empty();
            // Four full-width choices plus the fixed footer fit in both
            // orientations. Keeping one page size also makes rotation safe
            // while this synchronous picker is open.
            const size_t pageSize = 4;
            size_t page = 0;
            std::string item;
            for (;;) {
                size_t first = page * pageSize;
                size_t last = std::min(first + pageSize, equipment.size());
                std::vector<TopicJournal::Choice> visible(equipment.begin() + first,
                    equipment.begin() + last);
                if (equipment.size() > pageSize)
                    mobileAppendPageControls(visible, page, first, last, equipment.size(),
                        weapon ? "weapons" : "armour choices");
                visible.push_back(mobileBack("back", p->getName()));
                visible.push_back(mobileDismiss("close", "Close"));
                MobileTopicInput equip;
                std::string prompt = weapon ? "Choose a weapon" : "Choose armour";
                prompt += hasEquipment
                    ? "\n\nAvailable equipment this class can use."
                    : "\n\nNo spare equipment this character can use.";
                item = equip.read(prompt, visible, 0, false, false,
                    ZU4_TOPIC_PANEL_DENSE_FULLSCREEN);
                if (item == "__page_previous") { --page; continue; }
                if (item == "__page_next") { ++page; continue; }
                break;
            }
            if (item == "close" || item == "bye") return;
            if (item == "back") continue;
            int type = atoi(item.c_str());
            if (type < 0 || type >= limit) continue;
            EquipError result = weapon ? p->setWeapon((WeaponType)type) : p->setArmor((ArmorType)type);
            if (result == EQUIP_SUCCEEDED) {
                screenMessage("%s equipped %s.\n", p->getName().c_str(), weapon ? zu4_weapon_name((WeaponType)type) : zu4_armor_name((ArmorType)type));
                // Commit exactly one turn to the controller that opened the
                // panel; inspecting and cancelling never advance combat.
                if (eventHandler->getController() == owner) c->location->turnCompleter->finishTurn();
                return;
            }
            MobileTopicInput failed;
            failed.read("That item can no longer be equipped.", {mobileBack("back", p->getName())}, 0);
        }
    }
}
extern "C" void zu4_mobile_party_member(int index) {
    if (!game || !c || !c->party || index < 0 || index >= c->party->size()) return;
    Controller *owner = eventHandler->getController();
    if (owner != game && !dynamic_cast<CombatController *>(owner)) return;

    // Reuse the ordinary party flow, but let its first selection be supplied by
    // the persistent roster rather than asking the player to choose twice.
    mobilePartyInitialMember = index;
    zu4_mobile_party();
    mobilePartyInitialMember = -1;
}
static std::string mobileNotebookKey(int index) {
    const auto &history = mobileTopics.history();
    return index >= 0 && index < (int)history.size() ? JournalNotebook::key(history[index]) : "";
}
static bool mobileCommitNotebook(const JournalNotebook &old) {
    std::string directory = gameSaveDirectory();
    if (mobileNotebookWritable && mobileTopics.save(directory + "topics.txt") &&
        mobileNotebook.save(directory + mobileNotebookFilename)) return true;
    mobileNotebook = old;
    return false;
}
extern "C" int zu4_mobile_journal_favorite(int index) {
    return mobileNotebook.favorite(mobileNotebookKey(index));
}
extern "C" int zu4_mobile_journal_toggle_favorite(int index) {
    if (!zu4_journal_panel_is_visible() || !mobileNotebookWritable) return 0;
    JournalNotebook old = mobileNotebook;
    return mobileNotebook.toggleFavorite(mobileNotebookKey(index)) && mobileCommitNotebook(old);
}
extern "C" uint64_t zu4_mobile_journal_attached_note(int index) {
    return mobileNotebook.attachedNote(mobileNotebookKey(index));
}
extern "C" int zu4_mobile_journal_note_count(void) { return (int)mobileNotebook.notes().size(); }
extern "C" int zu4_mobile_journal_note_at(int index, Zu4JournalNote *note) {
    if (!note || index < 0 || index >= (int)mobileNotebook.notes().size()) return 0;
    const auto &value = mobileNotebook.notes()[index];
    note->identifier = value.id; note->text = value.text.c_str(); note->passage = -1;
    if (!value.passage.empty()) {
        const auto &history = mobileTopics.history();
        for (int i = 0; i < (int)history.size(); ++i)
            if (JournalNotebook::key(history[i]) == value.passage) { note->passage = i; break; }
    }
    return 1;
}
extern "C" uint64_t zu4_mobile_journal_save_note(int index, uint64_t identifier, const char *text) {
    if (!zu4_journal_panel_is_visible() || !mobileNotebookWritable || !text || index < -1) return 0;
    std::string passage = index < 0 ? "" : mobileNotebookKey(index);
    if (index >= 0 && passage.empty()) return 0;
    JournalNotebook old = mobileNotebook;
    uint64_t result = mobileNotebook.setNote(identifier, passage, text);
    return result && mobileCommitNotebook(old) ? result : 0;
}
extern "C" int zu4_mobile_journal_delete_note(uint64_t identifier) {
    if (!zu4_journal_panel_is_visible() || !mobileNotebookWritable) return 0;
    JournalNotebook old = mobileNotebook;
    return mobileNotebook.deleteNote(identifier) && mobileCommitNotebook(old);
}
extern "C" void zu4_mobile_journal(void) {
    if (!game || eventHandler->getController() != game || zu4_journal_panel_is_visible()) return;
    // The asynchronous native notebook owns input and pauses world timers.
    std::vector<const char *> texts, sources, speakers, places, kinds, topics;
    std::vector<int> indices;
    int index = -1;
    for (const auto &passage : mobileTopics.history()) {
        ++index;
        std::string normalized;
        for (unsigned char ch : passage.text)
            if (std::isalnum(ch)) normalized += (char)std::tolower(ch);
        if (normalized.empty() || normalized == "bye" || normalized == "farewell" ||
            normalized == "yourinterest" || normalized == "whatelse") continue;
        texts.push_back(passage.text.c_str());
        sources.push_back(passage.source.c_str());
        speakers.push_back(passage.speaker.c_str());
        places.push_back(passage.place.c_str());
        kinds.push_back(passage.kind.c_str());
        topics.push_back(passage.topic.c_str());
        indices.push_back(index);
    }
    Zu4JournalAccess access = {zu4_mobile_journal_favorite, zu4_mobile_journal_toggle_favorite,
        zu4_mobile_journal_attached_note, zu4_mobile_journal_note_count, zu4_mobile_journal_note_at,
        zu4_mobile_journal_save_note, zu4_mobile_journal_delete_note, zu4_ios_set_native_text_input_active};
    zu4_journal_panel_show(texts.data(), sources.data(), speakers.data(), places.data(),
                           kinds.data(), topics.data(), indices.data(), (int)texts.size(), &access);
}
static void observeMobileDialogue(const std::string &text, Person *talker, const std::string &topic) {
    const std::string place = c->location->map->getName();
    const std::string speaker = talker ? talker->getName() : "";
    const std::string source = speaker.empty() || speaker == "(unnamed person)"
        ? "Conversation in " + place : speaker + " in " + place;
    gameRecordRevealedText(text, source,
        speaker == "(unnamed person)" ? "" : speaker, place,
        speaker.empty() || speaker == "(unnamed person)" ? "conversation" : "person", topic);
}
#endif

/*-----------------*/
/* Functions BEGIN */

/* main game functions */
void gameAdvanceLevel(PartyMember *player);
void gameInnHandler(void);
void gameLostEighth(Virtue virtue);
void gamePartyStarving(void);
time_t gameTimeSinceLastCommand(void);
int gameSave(void);

/* spell functions */
void gameCastSpell(unsigned int spell, int caster, int param);
bool gameSpellMixHowMany(int spell, int num, Ingredients *ingredients);

void mixReagents();
bool mixReagentsForSpellU4(int spell);
bool mixReagentsForSpellU5(int spell);
void mixReagentsSuper();
void newOrder();

/* conversation functions */
bool talkAt(const Coords &coords);
void talkRunConversation(Conversation &conv, Person *talker, bool showPrompt);

/* action functions */
bool attackAt(const Coords &coords);
bool destroyAt(const Coords &coords);
bool getChestTrapHandler(int player);
bool jimmyAt(const Coords &coords);
bool openAt(const Coords &coords);
void wearArmor(int player = -1);
void ztatsFor(int player = -1);

/* checking functions */
void gameLordBritishCheckLevels(void);

/* creature functions */
void gameDestroyAllCreatures(void);
void gameFixupObjects(Map *map);
void gameCreatureAttack(Creature *obj);

/* Functions END */
/*---------------*/

//extern Object *party[8];
Context *c = NULL;

ReadPlayerController::ReadPlayerController() : ReadChoiceController("12345678 \033\n") {}

ReadPlayerController::~ReadPlayerController() {}

bool ReadPlayerController::keyPressed(int key) {
    bool valid = ReadChoiceController::keyPressed(key);
    if (valid) {
        if (value < '1' ||
            value > ('0' + c->saveGame->members))
            value = '0';
    } else {
        value = '0';
    }
    return valid;
}

int ReadPlayerController::getPlayer() {
    return value - '1';
}

int ReadPlayerController::waitFor() {
    ReadChoiceController::waitFor();
    return getPlayer();
}

bool AlphaActionController::keyPressed(int key) {
    if (islower(key))
        key = toupper(key);

    if (key >= 'A' && key <= toupper(lastValidLetter)) {
        value = key - 'A';
        doneWaiting();
    } else if (key == U4_SPACE || key == U4_ESC || key == U4_ENTER) {
        screenMessage("\n");
        value = -1;
        doneWaiting();
    } else {
        screenMessage("\n%s", prompt.c_str());
        return KeyHandler::defaultHandler(key, NULL);
    }
    return true;
}

int AlphaActionController::get(char lastValidLetter, const std::string &prompt, EventHandler *eh) {
    if (!eh)
        eh = eventHandler;

    AlphaActionController ctrl(lastValidLetter, prompt);
    eh->pushController(&ctrl);
    return ctrl.waitFor();
}

GameController::GameController() : mapArea(BORDER_WIDTH, BORDER_HEIGHT, VIEWPORT_W, VIEWPORT_H), paused(false), pausedTimer(0) {
}

void GameController::initScreen()
{
    Image *screen = zu4_img_get_screen();
    zu4_img_fill(screen, 0, 0, screen->w, screen->h, 0, 0, 0, 255);
}

void GameController::initScreenWithoutReloadingState()
{
    zu4_music_play(c->location->map->music);
    zu4_img_draw(imageMgr->get(BKGD_BORDERS)->image, 0, 0);
    c->stats->update(); /* draw the party stats */

#ifdef ZU4_IOS
    screenMessage("Open Menu for help, saving, and exploration actions.\n");
#else
    screenMessage("Press Alt-h for help\n");
#endif
    screenPrompt();

    eventHandler->setScreenUpdate(&gameUpdateScreen);
}

void GameController::deinit() {
#if defined(ZU4_IOS) || defined(ZU4_WEB)
	mobileCancelWalk();
#endif
	delete c->location;
	delete c->party;
	delete c->aura;
	delete c->stats;
	free(c->saveGame);
	zu4_ctx_deinit();
}

void GameController::init() {
#if defined(ZU4_IOS) || defined(ZU4_WEB)
    mobileCancelWalk();
#endif
    FILE *saveGameFile, *monstersFile;
    char saveGameFileName[1024];

    zu4_error(ZU4_LOG_DBG, "gameInit() running.");

    initScreen();

    ProgressBar pb((320/2) - (200/2), (200/2), 200, 10, 0, 4);
    pb.setBorderColor(240, 240, 240);
    pb.setBorderWidth(1);
    pb.setColor(0, 0, 128);

    screenTextAt(13, 11, "%s", "Loading Game...");

    /* initialize the global game context */
    //c = new Context;
    c = zu4_ctx_init();
    c->saveGame = (SaveGame*)calloc(1, sizeof(SaveGame));

    zu4_error(ZU4_LOG_DBG, "Global context initialized.");

    /* initialize conversation and game state variables */
    c->line = TEXT_AREA_H - 1;
    c->col = 0;
    c->stats = new StatsArea();
    c->moonPhase = 0;
    c->windDirection = DIR_NORTH;
    c->windCounter = 0;
    c->windLock = false;
    c->aura = new Aura();
    c->horseSpeed = 0;
    c->opacity = 1;
    c->lastCommandTime = time(NULL);
    c->lastShip = NULL;

#ifdef ZU4_WEB
    webJournalLoad();
#endif

#ifdef ZU4_IOS
    mobileTopics.clear();
    mobileTopics.load(gameSaveDirectory() + "topics.txt");
    mobileNotebook.clear();
    mobileNotebookWritable = mobileNotebook.load(gameSaveDirectory() + mobileNotebookFilename) &&
        mobileNotebook.validFor(mobileTopics);
    if (!mobileNotebookWritable) zu4_error(ZU4_LOG_WRN, "Journal notes could not be read; preserved on disk and editing disabled.");
#endif
    /* load in the save game */
    std::string saveDirectory = gameSaveDirectory();
    snprintf(saveGameFileName, sizeof(saveGameFileName), "%s%s", saveDirectory.c_str(), PARTY_SAV_BASE_FILENAME);
    saveGameFile = fopen(saveGameFileName, "rb");
    if (saveGameFile) {
        SaveGame loaded = {};
        bool readComplete = saveGameRead(&loaded, saveGameFile) != 0;
        fclose(saveGameFile);
        if (!readComplete || !SaveValidation::characters(loaded))
            zu4_error(ZU4_LOG_ERR, "savegame is incomplete or has an invalid party size");
        *c->saveGame = loaded;
#ifdef ZU4_IOS
        mobileLastCheckpointAttempt = loaded.moves;
#endif
    } else
        zu4_error(ZU4_LOG_ERR, "no savegame found!");

    zu4_error(ZU4_LOG_DBG, "Save game loaded."); ++pb;

    /* initialize our party */
    c->party = new Party(c->saveGame);
    c->party->addObserver(this);

    /* set the map to the world map by default */
    setMap(mapMgr->get(MAP_WORLD), 0, NULL);
    c->location->map->clearObjects();

    zu4_error(ZU4_LOG_DBG, "World map set."); ++pb;

    /* initialize our start location */
    Map *map = mapMgr->get(MapId(c->saveGame->location));
    zu4_error(ZU4_LOG_DBG, "Initializing start location.");

    /* if our map is not the world map, then load our map */
    if (map->type != Map::WORLD)
        setMap(map, 1, NULL);
    else
        /* initialize the moons (must be done from the world map) */
        initMoons();

    /**
     * Translate info from the savegame to something we can use
     */
    if (c->location->prev) {
        c->location->coords = (Coords){c->saveGame->x, c->saveGame->y, c->saveGame->dnglevel};
        c->location->prev->coords = (Coords){c->saveGame->dngx, c->saveGame->dngy, 0};
    }
    else c->location->coords = (Coords){c->saveGame->x, c->saveGame->y, (int)c->saveGame->dnglevel};
    c->saveGame->orientation = (Direction)(c->saveGame->orientation + DIR_WEST);

    /**
     * Fix the coordinates if they're out of bounds.  This happens every
     * time on the world map because (z == -1) is no longer valid.
     * To maintain compatibility with u4dos, this value gets translated
     * when the game is saved and loaded
     */
	if (MAP_IS_OOB(c->location->map, c->location->coords)) {
        Coords newcoords = c->location->coords;
        putInBounds(&newcoords, c->location->map);
        c->location->coords.x = newcoords.x;
        c->location->coords.y = newcoords.y;
        c->location->coords.z = newcoords.z;
	}
#if defined(ZU4_IOS) || defined(ZU4_WEB)
    mobileLoadExplorationMap(saveDirectory);
    mobileLoadMapPins(saveDirectory);
    mobileLoadMapDiscoveries(saveDirectory);
    mobileLoadDungeonExploration(saveDirectory);
    mobileRevealWorld();
    mobileRevealDungeon();
    // A legacy checkpoint created while already inside a place still proves
    // that its entrance was visited, even though it predates marker metadata.
    if (c->location->prev && c->location->prev->map->isWorldMap())
        gameDiscoverWorldPlace(c->location->prev->coords, c->location->map);
#endif

    if (c->location->context & CTX_DUNGEON) {
        Dungeon *dungeon = dynamic_cast<Dungeon *>(c->location->map);
        std::string path = saveDirectory + "dngmap.sav";
        FILE *file = fopen(path.c_str(), "rb");
        if (file) {
            std::size_t size = dungeon->width * dungeon->height * dungeon->levels;
            std::vector<unsigned char> raw(size);
            bool complete = fread(raw.data(), 1, size, file) == size;
            complete = complete && fgetc(file) == EOF && !ferror(file);
            fclose(file);
            if (!complete) zu4_error(ZU4_LOG_ERR, "dungeon save is incomplete or has the wrong size");
            for (std::size_t i = 0; i < size; ++i) {
                dungeon->data[i] = dungeon->translateFromRawTileIndex(raw[i]);
                dungeon->dataSubTokens[i] = raw[i] & 15;
            }
        }
    }

    zu4_error(ZU4_LOG_DBG, "Loading monsters."); ++pb;

    /* load in creatures.sav */
    snprintf(saveGameFileName, sizeof(saveGameFileName), "%s%s", saveDirectory.c_str(), MONSTERS_SAV_BASE_FILENAME);
    monstersFile = fopen(saveGameFileName, "rb");

    if (monstersFile) {
        SaveGameMonsterRecord loaded[MONSTERTABLE_SIZE] = {};
        bool complete = saveGameMonstersRead(loaded, monstersFile) != 0;
        complete = complete && fgetc(monstersFile) == EOF && !ferror(monstersFile);
        fclose(monstersFile);
        if (!complete) zu4_error(ZU4_LOG_ERR, "creature save is incomplete or has trailing data");
        std::copy(loaded, loaded + MONSTERTABLE_SIZE, c->location->map->monsterTable);
    }
    gameFixupObjects(c->location->map);

    /* we have previous creature information as well, load it! */
    if (c->location->prev) {
        snprintf(saveGameFileName, sizeof(saveGameFileName), "%s%s", saveDirectory.c_str(), OUTMONST_SAV_BASE_FILENAME);
        monstersFile = fopen(saveGameFileName, "rb");
        if (monstersFile) {
            SaveGameMonsterRecord loaded[MONSTERTABLE_SIZE] = {};
            bool complete = saveGameMonstersRead(loaded, monstersFile) != 0;
            complete = complete && fgetc(monstersFile) == EOF && !ferror(monstersFile);
            fclose(monstersFile);
            if (!complete) zu4_error(ZU4_LOG_ERR, "outside-creature save is incomplete or has trailing data");
            std::copy(loaded, loaded + MONSTERTABLE_SIZE, c->location->prev->map->monsterTable);
        }
        gameFixupObjects(c->location->prev->map);
    }

    spellSetEffectCallback(&gameSpellEffect);
    itemSetDestroyAllCreaturesCallback(&gameDestroyAllCreatures);

    ++pb;

    zu4_error(ZU4_LOG_DBG, "Settings up reagent menu.");
    c->stats->resetReagentsMenu();

    /* add some observers */
    c->aura->addObserver(c->stats);
    c->party->addObserver(c->stats);

    initScreenWithoutReloadingState();
    zu4_error(ZU4_LOG_DBG, "gameInit() completed successfully.");
}

/**
 * Saves the game state into party.sav and creatures.sav.
 */
bool gamePrepareJourney() {
#ifdef ZU4_IOS
    std::string base = zu4_settings_ptr()->path;
    std::string root = SaveSlots::snapshotRoot(base, SaveSlots::active(base));
    std::string selected = SaveSnapshot::current(root);
    Map *world = mapMgr->get(MAP_WORLD);
    std::string previous = SaveSnapshot::previous(root);

    auto validExplorationMap = [&](const std::string &directory) {
        FILE *file = fopen((directory + mobileMapFilename).c_str(), "rb");
        if (!file) return errno == ENOENT; // Older snapshots acquire it on save.
        char header[64] = {};
        std::size_t expected = (std::size_t)world->width * world->height;
        std::vector<unsigned char> cells(expected);
        bool valid = fgets(header, sizeof(header), file) &&
            strcmp(header, "ZU4-EXPLORED-MAP-1\n") == 0 &&
            fread(cells.data(), 1, expected, file) == expected &&
            fgetc(file) == EOF && !ferror(file);
        if (fclose(file) != 0) valid = false;
        return valid;
    };
    auto validCheckpoint = [&](const std::string &directory) {
        if (directory.empty() || !SaveValidation::checkpointFiles(directory) ||
            !validExplorationMap(directory)) return false;
        TopicJournal journal;
        JournalNotebook notebook;
        MobileMapPins pins;
        MobileMapDiscoveries discoveries;
        MobileDungeonExploration dungeons;
        struct stat topicsInfo;
        const bool topicsPresent=stat((directory+"topics.txt").c_str(),&topicsInfo)==0;
        const bool validTopics=topicsPresent ? journal.load(directory+"topics.txt") : errno==ENOENT;
        return validTopics &&
            notebook.load(directory + mobileNotebookFilename) && notebook.validFor(journal) &&
            pins.load(directory + mobileMapPinsFilename, world->width, world->height) &&
            discoveries.load(directory + mobileMapDiscoveriesFilename,
                             world->width, world->height) &&
            dungeons.load(directory + mobileDungeonExplorationFilename,
                          mobileDungeonShapes());
    };

    bool currentPointerExists = access((root + "/CURRENT").c_str(), F_OK) == 0;
    bool previousPointerExists = access((root + "/PREVIOUS").c_str(), F_OK) == 0;
    SaveRecovery::Plan recovery = SaveRecovery::plan(currentPointerExists,
        previousPointerExists, validCheckpoint(selected), validCheckpoint(previous));
    if (recovery == SaveRecovery::USE_CURRENT || recovery == SaveRecovery::USE_LEGACY)
        return true;

    MobileTopicInput input;
    if (recovery == SaveRecovery::UNAVAILABLE) {
        input.read("The current checkpoint could not be read safely. No validated checkpoint is available for recovery. Your save files have been kept.", {mobileBack("back", "Menu")}, 0);
        return false;
    }

    SaveGame previousSave = {};
    FILE *party = fopen((previous + PARTY_SAV_BASE_FILENAME).c_str(), "rb");
    bool readSummary = party && saveGameRead(&previousSave, party) != 0;
    if (party && fclose(party) != 0) readSummary = false;
    std::string location = "Britannia";
    if (readSummary) {
        Map *map = mapMgr->get((MapId)previousSave.location);
        if (map && !map->getName().empty()) location = map->getName();
        if (previousSave.location >= MAP_DECEIT && previousSave.location <= MAP_ABYSS)
            location += ", Level " + std::to_string(previousSave.dnglevel + 1);
        location += " · position " + std::to_string(previousSave.x) + ", " +
            std::to_string(previousSave.y);
    }
    std::string timestamp = "Time unavailable";
    struct stat checkpointInfo = {};
    if (stat((previous + PARTY_SAV_BASE_FILENAME).c_str(), &checkpointInfo) == 0) {
        struct tm local = {};
        if (localtime_r(&checkpointInfo.st_mtime, &local)) {
            char formatted[80] = {};
            if (strftime(formatted, sizeof(formatted), "%b %d, %Y at %I:%M %p", &local))
                timestamp = formatted;
        }
    }
    std::string detail = "The current checkpoint could not be read safely. A previous checkpoint is available.\n\nSlot " +
        std::to_string(SaveSlots::active(base)) + " · " + timestamp + "\n" + location +
        "\n\nRestore it? Later progress will not be included. Existing save folders will be kept.";
    std::string choice = input.read(detail,
        {{"restore", "Restore previous checkpoint"}, mobileBack("back", "Menu")}, 0);
    if (choice != "restore") return false;
    std::string name = previous.substr(root.size() + 1);
    name.pop_back();
    if (!SaveStoreContract::restore(root, name, SaveStoreContract::inspect(root).currentGenerationId)) {
        MobileTopicInput error;
        error.read("The checkpoint reference could not be restored. Your save files have been kept.", {mobileBack("back", "Menu")}, 0);
        return false;
    }
#endif
    return true;
}

int gameActiveSaveSlot() {
#ifdef ZU4_IOS
    return SaveSlots::active(zu4_settings_ptr()->path);
#else
    return 1;
#endif
}

#ifdef ZU4_IOS
static bool mobileReadSlotParty(int slot, SaveGame *save) {
    std::string base = zu4_settings_ptr()->path;
    std::string root = SaveSlots::snapshotRoot(base, slot);
    std::string directory = SaveSnapshot::current(root);
    // An invalid CURRENT pointer must go through recovery rather than silently
    // displaying or loading Slot 1's legacy adventure.
    if (directory.empty() && access((root + "/CURRENT").c_str(), F_OK) == 0) return false;
    if (directory.empty()) directory = SaveSlots::fallbackDirectory(base, slot);
    FILE *file = fopen((directory + PARTY_SAV_BASE_FILENAME).c_str(), "rb");
    if (!file) return false;
    SaveGame loaded = {};
    bool ok = saveGameRead(&loaded, file) != 0 && SaveValidation::characters(loaded) &&
        fgetc(file) == EOF && !ferror(file);
    if (fclose(file) != 0) ok = false;
    if (ok && save) *save = loaded;
    return ok;
}

static std::string mobileSlotLabel(int slot) {
    std::string label = "Slot " + std::to_string(slot) + " — ";
    SaveGame save = {};
    if (mobileReadSlotParty(slot, &save)) {
        label += save.players[0].name;
        label += " — " + std::to_string(save.members) + (save.members == 1 ? " companion" : " companions");
    } else if (SaveSlots::occupied(zu4_settings_ptr()->path, slot)) {
        label += "Recovery needed";
    } else {
        label += "Empty";
    }
    return label;
}

class MobileAdventureTransferInput : public WaitableController<std::string> {
public:
    bool ready=false;
    int outcome=0;
    std::string error;
    bool keyPressed(int) override { return true; }
    static void submit(int outcome,const unsigned char *bytes,size_t count,const char *error,void *context) {
        auto *input=static_cast<MobileAdventureTransferInput *>(context);
        input->outcome=outcome;input->error=error ? error : "";
        if(bytes && count)input->value.assign((const char *)bytes,count);
        input->ready=true;input->doneWaiting();
    }
    std::string read(bool importing,const std::string &json="",int slot=1,bool cloud=false) {
        struct PauseScope {
            PauseScope(){++mobilePanelDepth;mobileCancelWalk();}
            ~PauseScope(){--mobilePanelDepth;if(!mobilePanelDepth && c)c->lastCommandTime=time(NULL);}
        } pause;
        if(cloud)zu4_cloud_accounts_show(json.data(),json.size(),submit,this);
        else if(importing)zu4_adventure_import_show(submit,this);
        else zu4_adventure_export_show(json.data(),json.size(),slot,submit,this);
        if(ready)return value;
        eventHandler->pushController(this);return waitFor();
    }
};

static void mobileAccount(bool duringPlay) {
    const std::string base=zu4_settings_ptr()->path;
    auto message=[](const std::string &text){MobileTopicInput result;result.read(text,{mobileBack("back","Account")},0);};
    std::string slotsJSON,error;
    if(!AdventurePackage::cloudSlots(base,duringPlay ? gameActiveSaveSlot() : 0,slotsJSON,error)){message(error);return;}
    MobileAdventureTransferInput input;std::string response=input.read(false,slotsJSON,1,true);
    if(input.outcome<0){message(input.error);return;}
    if(input.outcome==0)return;
    if(!AdventurePackage::installCloudResponse(base,duringPlay ? gameActiveSaveSlot() : 0,response,error)){message(error);return;}
    message("Cloud checkpoint installed and validated.\n\nYour replaced local checkpoint is retained for recovery. Choose Journey onward at the title to continue.");
}

extern "C" void zu4_mobile_account_from_title(void) {
    mobileAccount(false);
}

static void mobileCloudSyncResult(int outcome,const unsigned char *bytes,size_t count,const char *error,void *) {
    if(outcome<=0 || !bytes || !count)return;
    std::string response((const char *)bytes,count),installError;
    AdventurePackage::installCloudResponse(zu4_settings_ptr()->path,gameActiveSaveSlot(),response,installError);
}

static void mobileCloudSyncAfterCheckpoint() {
    std::string slotsJSON,error;
    if(AdventurePackage::cloudSlots(zu4_settings_ptr()->path,gameActiveSaveSlot(),slotsJSON,error))
        zu4_cloud_accounts_sync(slotsJSON.data(),slotsJSON.size(),mobileCloudSyncResult,nullptr);
}

static void mobileAdventureBackups(bool duringPlay) {
    const std::string base=zu4_settings_ptr()->path;
    auto message=[](const std::string &text){MobileTopicInput result;result.read(text,{mobileBack("back","Backups")},0);};
    for(;;) {
        MobileTopicInput menu;
        std::string action=menu.read("Adventure backups\n\nA .u4save backup contains game progress, journal, bookmarks, personal notes, explored maps, pins, and discoveries. Game data and app preferences are not included.",
            {{"import","Import from Files"},{"export","Export / Save to Files"},mobileBack("back",duringPlay ? "Menu" : "Slots")},0,false,false,ZU4_TOPIC_PANEL_FULLSCREEN);
        if(action!="import" && action!="export")return;
        AdventurePackage::Bundle bundle;std::string error;
        if(action=="import") {
            MobileAdventureTransferInput input;const std::string json=input.read(true);
            if(input.outcome==0)continue;
            if(input.outcome<0){message(input.error);continue;}
            if(!AdventurePackage::decode(json,bundle,error)){message(error);continue;}
        }
        std::vector<TopicJournal::Choice> slots;
        for(int slot=1;slot<=SaveSlots::SLOT_COUNT;++slot) {
            if(action=="import" && duringPlay && slot==gameActiveSaveSlot())continue;
            if(action=="export" && !SaveSlots::occupied(base,slot))continue;
            slots.push_back({"slot-"+std::to_string(slot),mobileSlotLabel(slot)});
        }
        if(slots.empty()){message("No saved checkpoints are available to export. Save an adventure first.");continue;}
        slots.push_back(mobileBack("back","Backups"));
        MobileTopicInput picker;
        const std::string selected=picker.read(action=="import" ?
            "Import adventure\n\nChoose a destination slot. To replace the adventure you are playing, return to the title first." :
            "Export adventure\n\nChoose a saved slot. Exporting a checkpoint never spends a game turn.",slots,0);
        if(selected.size()!=6 || selected.compare(0,5,"slot-")!=0)continue;
        const int slot=selected[5]-'0';if(!SaveSlots::valid(slot))continue;
        const std::string root=SaveSlots::snapshotRoot(base,slot);
        const std::string expected=SaveSnapshot::current(root);
        if(action=="import") {
            if(duringPlay && slot==gameActiveSaveSlot()){message("Return to the title before replacing the adventure you are playing.");continue;}
            if(SaveSlots::occupied(base,slot)) {
                MobileTopicInput confirm;
                if(confirm.read("Replace "+mobileSlotLabel(slot)+"?\n\nThe incoming backup was validated. Your existing checkpoint is retained for recovery until later saves rotate it out.",
                    {{"replace","Replace Slot "+std::to_string(slot)},mobileBack("back","Backups")},0)!="replace")continue;
            }
            if(!AdventurePackage::install(base,slot,bundle,expected,error)){message(error);continue;}
            AdventurePackage::clearCloudLink(slot);
            message("Adventure imported into Slot "+std::to_string(slot)+".\n\nIt is separate from the other slots. Choose Journey onward at the title to continue it.");
        } else {
            if(duringPlay && slot==gameActiveSaveSlot() && (c->location->context & CTX_CAN_SAVE_GAME)) {
                MobileTopicInput checkpoint;
                std::string choice=checkpoint.read("Export Slot "+std::to_string(slot)+"\n\nSave your current progress first, or export the existing saved checkpoint?",{{"latest","Save current progress and export"},{"saved","Export saved checkpoint"},mobileBack("back","Backups")},0);
                if(choice!="latest" && choice!="saved")continue;
                if(choice=="latest" && !gameSave()){message("Saving did not finish. Your previous checkpoint has been kept; try again before exporting current progress.");continue;}
            } else if(duringPlay && slot==gameActiveSaveSlot()) {
                MobileTopicInput checkpoint;
                if(checkpoint.read("Export saved checkpoint\n\nSaving is unavailable here. This backup contains the last saved game state, not an unfinished battle or current interaction.",{{"saved","Export saved checkpoint"},mobileBack("back","Backups")},0)!="saved")continue;
            }
            std::string directory=SaveSnapshot::current(root);
            if(directory.empty() && access((root+"/CURRENT").c_str(),F_OK)==0){message("This slot needs save recovery before it can be exported.");continue;}
            if(directory.empty())directory=SaveSlots::fallbackDirectory(base,slot);
            if(!AdventurePackage::readCheckpoint(directory,bundle,error)){message(error);continue;}
            std::string json;if(!AdventurePackage::encode(bundle,json,error)){message(error);continue;}
            MobileAdventureTransferInput output;output.read(false,json,slot);
            if(output.outcome<0)message(output.error);
            else if(output.outcome>0)message("Adventure backup shared.\n\nKeep the .u4save file somewhere outside the app. It can be imported on iOS or web.");
        }
    }
}

static SaveSlotSelectionResult mobileSelectSaveSlot(bool newGame) {
    std::string base = zu4_settings_ptr()->path;
    for (;;) {
    std::vector<TopicJournal::Choice> choices;
    for (int slot = 1; slot <= SaveSlots::SLOT_COUNT; ++slot) {
        if (newGame || SaveSlots::occupied(base, slot))
            choices.push_back({"slot-" + std::to_string(slot), mobileSlotLabel(slot)});
    }
    if (choices.empty()) {
        MobileTopicInput empty;
        std::string selected = empty.read(
            "Journey onward\n\nNo saved adventures were found. Start a new adventure or return to the title menu.",
            {{"new-game", "New game"}, {"account", "Account"}, {"backups", "Import / Export adventures"}, mobileBack("back", "Title")}, 0,
            false, false, ZU4_TOPIC_PANEL_COMPACT_MENU);
        if(selected=="account"){mobileAccount(false);continue;}
        if(selected=="backups"){mobileAdventureBackups(false);continue;}
        return selected == "new-game" ? SAVE_SLOT_SELECTION_NEW_GAME : SAVE_SLOT_SELECTION_CANCELLED;
    }
    choices.push_back({"account","Account"});
    choices.push_back({"backups","Import / Export adventures"});
    choices.push_back(mobileBack("back", "Title"));
        MobileTopicInput picker;
        std::string selected = picker.read(newGame ?
            "New adventure\n\nChoose one of three independent save slots." :
            "Journey onward\n\nChoose an adventure.", choices, 0);
        if(selected=="account"){mobileAccount(false);continue;}
        if(selected=="backups"){mobileAdventureBackups(false);continue;}
        if (selected.compare(0, 5, "slot-") != 0 || selected.size() != 6)
            return SAVE_SLOT_SELECTION_CANCELLED;
        int slot = selected[5] - '0';
        if (!SaveSlots::valid(slot)) return SAVE_SLOT_SELECTION_CANCELLED;
        if (newGame && SaveSlots::occupied(base, slot)) {
            MobileTopicInput confirm;
            std::string decision = confirm.read(
                "Replace Slot " + std::to_string(slot) + "?\n\nThis starts a new adventure in the slot. The current checkpoint remains available as the previous recovery checkpoint until later saves rotate it out.",
                {{"replace", "Replace Slot " + std::to_string(slot)}, mobileBack("back", "Slots")}, 0);
            if (decision != "replace") continue;
        }
        if (!SaveSlots::select(base, slot)) {
            MobileTopicInput error;
            error.read("The selected save slot could not be activated. No adventure data was changed.", {mobileBack("back", "Title")}, 0);
            return SAVE_SLOT_SELECTION_CANCELLED;
        }
        if(newGame)AdventurePackage::clearCloudLink(slot);
        return SAVE_SLOT_SELECTION_SELECTED;
    }
}
#endif

SaveSlotSelectionResult gameChooseSaveSlotForJourney() {
#ifdef ZU4_IOS
    return mobileSelectSaveSlot(false);
#else
    return SAVE_SLOT_SELECTION_SELECTED;
#endif
}

bool gameChooseSaveSlotForNewGame() {
#ifdef ZU4_IOS
    return mobileSelectSaveSlot(true) == SAVE_SLOT_SELECTION_SELECTED;
#else
    return true;
#endif
}

std::string gameSaveDirectory() {
    std::string legacy = zu4_settings_ptr()->path;
#ifdef ZU4_IOS
    int slot = SaveSlots::active(legacy);
    std::string root = SaveSlots::snapshotRoot(legacy, slot);
    std::string selected = SaveSnapshot::current(root);
    if (!selected.empty()) return selected;
    if (access((root + "/CURRENT").c_str(), F_OK) == 0)
        zu4_error(ZU4_LOG_ERR, "save snapshot pointer is invalid; recovery is required");
    return SaveSlots::fallbackDirectory(legacy, slot);
#endif
    return legacy;
}
std::string gameBeginSaveDirectory() {
#ifdef ZU4_IOS
    std::string base = zu4_settings_ptr()->path;
    return SaveSnapshot::begin(SaveSlots::snapshotRoot(base, SaveSlots::active(base)));
#else
    return zu4_settings_ptr()->path;
#endif
}
bool gamePublishSaveDirectory(const std::string &directory, bool dungeon) {
#ifdef ZU4_IOS
    if (!SaveValidation::party(directory + PARTY_SAV_BASE_FILENAME) ||
        !SaveValidation::creatures(directory + MONSTERS_SAV_BASE_FILENAME) ||
        (dungeon && !SaveValidation::creatures(directory + OUTMONST_SAV_BASE_FILENAME))) return false;
    TopicJournal journal;
    if (!journal.load(directory + "topics.txt")) return false;
    JournalNotebook notebook;
    if (!notebook.load(directory + mobileNotebookFilename) || !notebook.validFor(journal)) return false;
    Map *world = mapMgr->get(MAP_WORLD);
    MobileMapPins pins;
    if (!pins.load(directory + mobileMapPinsFilename, world->width, world->height)) return false;
    MobileMapDiscoveries discoveries;
    if (!discoveries.load(directory + mobileMapDiscoveriesFilename,
                          world->width, world->height)) return false;
    MobileDungeonExploration dungeonExploration;
    if (!dungeonExploration.load(directory + mobileDungeonExplorationFilename,
                                 mobileDungeonShapes())) return false;
    std::vector<std::string> required{PARTY_SAV_BASE_FILENAME, MONSTERS_SAV_BASE_FILENAME,
                                      "topics.txt", mobileNotebookFilename, mobileMapFilename, mobileMapPinsFilename,
                                      mobileMapDiscoveriesFilename,
                                      mobileDungeonExplorationFilename};
    if (dungeon) { required.push_back("dngmap.sav"); required.push_back(OUTMONST_SAV_BASE_FILENAME); }
    if (access((directory + "conversations.json").c_str(), F_OK) == 0)
        required.push_back("conversations.json");
    std::string base = zu4_settings_ptr()->path;
    std::string root = SaveSlots::snapshotRoot(base, SaveSlots::active(base));
    std::string previous = SaveSnapshot::current(root);
    if (!SaveStoreContract::publish(root, directory, required,
                                    SaveStoreContract::inspect(root).currentGenerationId)) return false;
    SaveSnapshot::retainRecent(root, previous);
    return true;
#else
    (void)directory; (void)dungeon;
    return true;
#endif
}
bool gameSaveExplorationMap(const std::string &directory) {
#if defined(ZU4_IOS) || defined(ZU4_WEB)
    return mobileSaveExplorationMap(directory);
#else
    (void)directory;
    return true;
#endif
}
bool gameSaveEmptyExplorationMap(const std::string &directory) {
#if defined(ZU4_IOS) || defined(ZU4_WEB)
    // A new adventure must never inherit the explored cells of the adventure
    // that was active before returning to the title screen.
    mobileWorldExplored.clear();
    mobileMapPins.clear();
    mobileMapDiscoveries.clear();
    mobileDungeonExploration.clear();
#ifdef ZU4_IOS
    if (!JournalNotebook().save(directory + mobileNotebookFilename)) return false;
#endif
    return mobileSaveExplorationMap(directory) && mobileSaveMapPins(directory) &&
        mobileSaveMapDiscoveries(directory) && mobileSaveDungeonExploration(directory);
#else
    (void)directory;
    return true;
#endif
}

static int gameWriteSave(const std::string &directory) {
    FILE *saveGameFile, *monstersFile, *dngMapFile;
    SaveGame save = *c->saveGame;

    /*************************************************/
    /* Make sure the savegame struct is accurate now */

    if (c->location->prev) {
        save.x = c->location->coords.x;
        save.y = c->location->coords.y;
        save.dnglevel = c->location->coords.z;
        save.dngx = c->location->prev->coords.x;
        save.dngy = c->location->prev->coords.y;
    }
    else {
        save.x = c->location->coords.x;
        save.y = c->location->coords.y;
        save.dnglevel = c->location->coords.z;
        save.dngx = c->saveGame->dngx;
        save.dngy = c->saveGame->dngy;
    }
    save.location = c->location->map->id;
    save.orientation = (Direction)(c->saveGame->orientation - DIR_WEST);

    /* Done making sure the savegame struct is accurate */
    /****************************************************/

    char saveGameFileName[1024];
    snprintf(saveGameFileName, sizeof(saveGameFileName), "%s%s", directory.c_str(), PARTY_SAV_BASE_FILENAME);
    saveGameFile = fopen(saveGameFileName, "wb");
    if (!saveGameFile) {
        screenMessage("Error opening " PARTY_SAV_BASE_FILENAME "\n");
        return 0;
    }

    //if (!save.write(saveGameFile)) {
    if (!saveGameWrite(&save, saveGameFile)) {
        screenMessage("Error writing to " PARTY_SAV_BASE_FILENAME "\n");
        fclose(saveGameFile);
        return 0;
    }
    if (fclose(saveGameFile) != 0) {
        screenMessage("Error completing party save.\n");
        return 0;
    }

    snprintf(saveGameFileName, sizeof(saveGameFileName), "%s%s", directory.c_str(), MONSTERS_SAV_BASE_FILENAME);
    monstersFile = fopen(saveGameFileName, "wb");
    if (!monstersFile) {
        screenMessage("Error opening %s\n", MONSTERS_SAV_BASE_FILENAME);
        return 0;
    }

    /* fix creature animations so they are compatible with u4dos */
    c->location->map->resetObjectAnimations();
    c->location->map->fillMonsterTable(); /* fill the monster table so we can save it */

    if (!saveGameMonstersWrite(c->location->map->monsterTable, monstersFile)) {
        screenMessage("Error opening creatures.sav\n");
        fclose(monstersFile);
        return 0;
    }
    if (fclose(monstersFile) != 0) {
        screenMessage("Error completing creature save.\n");
        return 0;
    }

    /**
     * Write dungeon info
     */
    if (c->location->context & CTX_DUNGEON) {
        unsigned int x, y, z;

        typedef std::map<const Creature*, int> DngCreatureIdMap;
        static DngCreatureIdMap id_map;

        /**
         * Map creatures to u4dos dungeon creature Ids
         */
        if (id_map.size() == 0) {
            id_map[creatureMgr->getById(RAT_ID)]          = 1;
            id_map[creatureMgr->getById(BAT_ID)]          = 2;
            id_map[creatureMgr->getById(GIANT_SPIDER_ID)] = 3;
            id_map[creatureMgr->getById(GHOST_ID)]        = 4;
            id_map[creatureMgr->getById(SLIME_ID)]        = 5;
            id_map[creatureMgr->getById(TROLL_ID)]        = 6;
            id_map[creatureMgr->getById(GREMLIN_ID)]      = 7;
            id_map[creatureMgr->getById(MIMIC_ID)]        = 8;
            id_map[creatureMgr->getById(REAPER_ID)]       = 9;
            id_map[creatureMgr->getById(INSECT_SWARM_ID)] = 10;
            id_map[creatureMgr->getById(GAZER_ID)]        = 11;
            id_map[creatureMgr->getById(PHANTOM_ID)]      = 12;
            id_map[creatureMgr->getById(ORC_ID)]          = 13;
            id_map[creatureMgr->getById(SKELETON_ID)]     = 14;
            id_map[creatureMgr->getById(ROGUE_ID)]        = 15;
        }

        snprintf(saveGameFileName, sizeof(saveGameFileName), "%s%s", directory.c_str(), "dngmap.sav");
        dngMapFile = fopen(saveGameFileName, "wb");
        if (!dngMapFile) {
            screenMessage("Error opening dngmap.sav\n");
            return 0;
        }

        for (z = 0; z < c->location->map->levels; z++) {
            for (y = 0; y < c->location->map->height; y++) {
                for (x = 0; x < c->location->map->width; x++) {
                    Coords position = {(int)x, (int)y, (int)z};
                    Dungeon *dungeon = dynamic_cast<Dungeon *>(c->location->map);
                    MapTile terrain = *dungeon->getTileFromData(position);
                    bool changed = false;
                    for (Annotation *annotation : dungeon->annotations->ptrsToAllAt(position)) {
                        if (!annotation->isVisualOnly() && annotation->getTTL() < 0) {
                            terrain = annotation->getTile();
                            changed = true;
                            break;
                        }
                    }
                    unsigned char tile = dungeon->translateToRawTileIndex(terrain);
                    // Preserve fountain, room and other original subtypes when
                    // the base terrain has not been replaced by an annotation.
                    if (!changed) tile = (tile & 0xF0) | dungeon->subTokenAt(position);
                    Object *obj = c->location->map->objectAt((Coords){(int)x, (int)y, (int)z});

                    /**
                     * Add the creature to the tile
                     */
                    if (obj && obj->getType() == Object::CREATURE) {
                        const Creature *m = dynamic_cast<Creature*>(obj);
                        DngCreatureIdMap::iterator m_id = id_map.find(m);
                        if (m_id != id_map.end())
                            tile |= m_id->second;
                    }

                    // Write the tile
                    if (fputc(tile, dngMapFile) == EOF) {
                        fclose(dngMapFile);
                        screenMessage("Error writing dungeon save.\n");
                        return 0;
                    }
                }
            }
        }

        if (fclose(dngMapFile) != 0) {
            screenMessage("Error completing dungeon save.\n");
            return 0;
        }

        /**
         * Write outmonst.sav
         */

        snprintf(saveGameFileName, sizeof(saveGameFileName), "%s%s", directory.c_str(), OUTMONST_SAV_BASE_FILENAME);
        monstersFile = fopen(saveGameFileName, "wb");
        if (!monstersFile) {
            screenMessage("Error opening %s\n", OUTMONST_SAV_BASE_FILENAME);
            return 0;
        }

        /* fix creature animations so they are compatible with u4dos */
        c->location->prev->map->resetObjectAnimations();
        c->location->prev->map->fillMonsterTable(); /* fill the monster table so we can save it */

        if (!saveGameMonstersWrite(c->location->prev->map->monsterTable, monstersFile)) {
            screenMessage("Error opening %s\n", OUTMONST_SAV_BASE_FILENAME);
            fclose(monstersFile);
            return 0;
        }
        if (fclose(monstersFile) != 0) {
            screenMessage("Error completing outside-creature save.\n");
            return 0;
        }
    }

    return 1;
}

int gameSave() {
    std::string directory = gameBeginSaveDirectory();
    if (directory.empty() || !gameWriteSave(directory)) return 0;
#ifdef ZU4_WEB
    if (!webJournalSave(directory)) { screenMessage("Unable to save journal notes.\n"); return 0; }
    if (!mobileSaveExplorationMap(directory) || !mobileSaveMapPins(directory) ||
        !mobileSaveMapDiscoveries(directory) || !mobileSaveDungeonExploration(directory)) {
        screenMessage("Unable to save exploration data.\n");
        return 0;
    }
#endif
#ifdef ZU4_IOS
    if (!AdventurePackage::copyWebTranscript(gameSaveDirectory(),directory)) {
        screenMessage("Unable to preserve imported web transcripts.\n");
        return 0;
    }
    if (!mobileTopics.save(directory + "topics.txt")) {
        screenMessage("Unable to save discovery journal.\n");
        return 0;
    }
    if (!mobileNotebookWritable || !mobileNotebook.save(directory + mobileNotebookFilename)) {
        screenMessage("Unable to save journal notes.\n");
        return 0;
    }
    if (!mobileSaveExplorationMap(directory)) {
        screenMessage("Unable to save exploration map.\n");
        return 0;
    }
    if (!mobileSaveMapPins(directory)) {
        screenMessage("Unable to save map pins.\n");
        return 0;
    }
    if (!mobileSaveMapDiscoveries(directory)) {
        screenMessage("Unable to save discovered places.\n");
        return 0;
    }
    if (!mobileSaveDungeonExploration(directory)) {
        screenMessage("Unable to save dungeon exploration.\n");
        return 0;
    }
#endif
    if (!gamePublishSaveDirectory(directory, (c->location->context & CTX_DUNGEON) != 0)) {
        screenMessage("Unable to publish save snapshot.\n");
        return 0;
    }
#ifdef ZU4_IOS
    mobileLastCheckpointAttempt = c->saveGame->moves;
    mobileCloudSyncAfterCheckpoint();
#endif
    return 1;
}

#ifdef ZU4_IOS
extern "C" void zu4_mobile_lifecycle_background(void) {
    mobileCancelWalk();
    mobileBackgroundCheckpointResult = MOBILE_BACKGROUND_NO_GAME;
    if (!game || !c || !c->location || !c->party || !c->saveGame) return;

    mobileBackgroundCheckpointSlot = gameActiveSaveSlot();
    bool stableView = c->location->viewMode == VIEW_NORMAL ||
        c->location->viewMode == VIEW_DUNGEON;
    bool canCheckpoint = stableView &&
        (c->location->context & CTX_CAN_SAVE_GAME) && !c->party->isDead();
    if (!canCheckpoint) {
        // Combat, settlements, shrines, and scripted sequences retain the last
        // complete immutable checkpoint rather than serializing a partial
        // interaction that the original save format cannot represent.
        mobileBackgroundCheckpointResult = MOBILE_BACKGROUND_RETAINED;
        return;
    }

    mobileBackgroundCheckpointResult = gameSave()
        ? MOBILE_BACKGROUND_SAVED : MOBILE_BACKGROUND_FAILED;
}

extern "C" void zu4_mobile_lifecycle_foreground(void) {
    if (c) c->lastCommandTime = time(NULL);
    if (!game || !c || !c->location) {
        mobileBackgroundCheckpointResult = MOBILE_BACKGROUND_NO_GAME;
        return;
    }
    if (mobileBackgroundCheckpointResult == MOBILE_BACKGROUND_SAVED)
        screenMessage("Slot %d secured during interruption.\n", mobileBackgroundCheckpointSlot);
    else if (mobileBackgroundCheckpointResult == MOBILE_BACKGROUND_FAILED)
        screenMessage("Interruption checkpoint failed. Use Menu to save.\n");
    else if (mobileBackgroundCheckpointResult == MOBILE_BACKGROUND_RETAINED)
        screenMessage("Resumed from interruption. Latest safe checkpoint retained.\n");
    mobileBackgroundCheckpointResult = MOBILE_BACKGROUND_NO_GAME;
    gameUpdateScreen();
}
#endif

/**
 * Sets the view mode.
 */
void gameSetViewMode(ViewMode newMode) {
    c->location->viewMode = newMode;
}

void gameUpdateScreen() {
    switch (c->location->viewMode) {
    case VIEW_NORMAL:
        screenUpdate(&game->mapArea, true, false);
        break;
    case VIEW_GEM:
        screenGemUpdate();
        break;
    case VIEW_RUNE:
        screenUpdate(&game->mapArea, false, false);
        break;
    case VIEW_DUNGEON:
        screenUpdate(&game->mapArea, true, false);
        break;
    case VIEW_DEAD:
        screenUpdate(&game->mapArea, true, true);
        break;
    case VIEW_CODEX: /* the screen updates will be handled elsewhere */
        break;
    case VIEW_MIXTURES: /* still testing */
        break;
    default:
        zu4_assert(0, "invalid view mode: %d", c->location->viewMode);
    }
}

void GameController::setMap(Map *map, bool saveLocation, const Portal *portal, TurnCompleter *turnCompleter) {
    int viewMode;
    LocationContext context;
    int activePlayer = c->party->getActivePlayer();
    Coords coords;

    if (!turnCompleter)
        turnCompleter = this;

    if (portal)
        coords = portal->start;
    else
        coords = (Coords){(int)map->width / 2, (int)map->height / 2, 0};

    /* If we don't want to save the location, then just return to the previous location,
       as there may still be ones in the stack we want to keep */
    if (!saveLocation)
        exitToParentMap();

    switch (map->type) {
    case Map::WORLD:
        context = CTX_WORLDMAP;
        viewMode = VIEW_NORMAL;
        break;
    case Map::DUNGEON:
        context = CTX_DUNGEON;
        viewMode = VIEW_DUNGEON;
        if (portal)
            c->saveGame->orientation = DIR_EAST;
        break;
    case Map::COMBAT:
        coords = (Coords){-1, -1, 0}; /* set these to -1 just to be safe; we don't need them */
        context = CTX_COMBAT;
        viewMode = VIEW_NORMAL;
        activePlayer = -1; /* different active player for combat, defaults to 'None' */
        break;
    case Map::SHRINE:
        context = CTX_SHRINE;
        viewMode = VIEW_NORMAL;
        break;
    case Map::CITY:
    default:
        context = CTX_CITY;
        viewMode = VIEW_NORMAL;
        break;
    }
    c->location = new Location(coords, map, viewMode, context, turnCompleter, c->location);
    c->location->addObserver(this);
    c->party->setActivePlayer(activePlayer);

    /* now, actually set our new tileset */
    mapArea.setTileset(map->tileset);

    if (isCity(map)) {
        City *city = dynamic_cast<City*>(map);
        city->addPeople();
    }
}

/**
 * Exits the current map and location and returns to its parent location
 * This restores all relevant information from the previous location,
 * such as the map, map position, etc. (such as exiting a city)
 **/

int GameController::exitToParentMap() {
    if (!c->location)
        return 0;

    if (c->location->prev != NULL) {
        // Create the balloon for Hythloth
        if (c->location->map->id == MAP_HYTHLOTH)
            createBalloon(c->location->prev->map);

        // free map info only if previous location was on a different map
        if (c->location->prev->map != c->location->map) {
            c->location->map->annotations->clear();
            c->location->map->clearObjects();

            /* quench the torch of we're on the world map */
            if (c->location->prev->map->isWorldMap())
                c->party->quenchTorch();
        }
        locationFree(&c->location);

        // restore the tileset to the one the current map uses
        mapArea.setTileset(c->location->map->tileset);

        return 1;
    }
    return 0;
}

/**
 * Terminates a game turn.  This performs the post-turn housekeeping
 * tasks like adjusting the party's food, incrementing the number of
 * moves, etc.
 */
void GameController::finishTurn() {
    c->lastCommandTime = time(NULL);
    Creature *attacker = NULL;

    while (1) {

        /* adjust food and moves */
        c->party->endTurn();

        /* count down the aura, if there is one */
        c->aura->passTurn();

        gameCheckHullIntegrity();

        /* update party stats */
        c->stats->setView(STATS_PARTY_OVERVIEW);

        screenUpdate(&this->mapArea, true, false);

        /* Creatures cannot spawn, move or attack while the avatar is on the balloon */
        if (!c->party->isFlying()) {

            // apply effects from tile avatar is standing on
            c->party->applyEffect(c->location->map->tileTypeAt(c->location->coords, WITH_GROUND_OBJECTS)->getEffect());

            // Move creatures and see if something is attacking the avatar
            attacker = c->location->map->moveObjects(c->location->coords);

            // Something's attacking!  Start combat!
            if (attacker) {
#if defined(ZU4_IOS) || defined(ZU4_WEB)
                mobileCancelWalk();
#endif
                gameCreatureAttack(attacker);
                return;
            }

            // cleanup old creatures and spawn new ones
            creatureCleanup();
            checkRandomCreatures();
            checkBridgeTrolls();
        }

        /* update map annotations */
        c->location->map->annotations->passTurn();

        if (!c->party->isImmobilized())
            break;

        if (c->party->isDead()) {
            deathStart(0);
            return;
        } else {
            screenMessage("Zzzzzz\n");
        }
    }

    if (c->location->context == CTX_DUNGEON) {
        Dungeon *dungeon = dynamic_cast<Dungeon *>(c->location->map);
        if (c->party->getTorchDuration() <= 0)
            screenMessage("It's Dark!\n");
        else c->party->burnTorch();

        /* handle dungeon traps */
        if (dungeon->currentToken() == DUNGEON_TRAP) {
            dungeonHandleTrap((TrapType)dungeon->currentSubToken());
            // a little kludgey to have a second test for this
            // right here.  But without it you can survive an
            // extra turn after party death and do some things
            // that could cause a crash, like Hole up and Camp.
            if (c->party->isDead()) {
              deathStart(0);
              return;
            }
        }
    }

#ifdef ZU4_IOS
    if (eventHandler->getController() == game && !mobilePanelDepth &&
        c->location->viewMode == VIEW_NORMAL && (c->location->context & CTX_CAN_SAVE_GAME) &&
        !c->party->isDead() && c->saveGame->moves - mobileLastCheckpointAttempt >= 10) {
        // Retry failures only after another interval; never stall every turn.
        mobileLastCheckpointAttempt = c->saveGame->moves;
        if (gameSave()) screenMessage("Slot %d autosaved.\n", gameActiveSaveSlot());
        else screenMessage("Autosave failed. Use Menu to try saving again.\n");
    }
#endif
#if defined(ZU4_IOS) || defined(ZU4_WEB)
    mobileContinueWalkAfterTurn();
#endif
    /* draw a prompt */
    screenPrompt();
}

/**
 * Show an attack flash at x, y on the current map.
 * This is used for 'being hit' or 'being missed'
 * by weapons, cannon fire, spells, etc.
 */
void GameController::flashTile(const Coords &coords, MapTile tile, int frames) {
    c->location->map->annotations->add(coords, tile, true);

    int msecPerFrame = frames * 33;
#ifdef ZU4_IOS
    if (c->location->context & CTX_COMBAT)
        msecPerFrame = MobileCombat::flashMilliseconds(frames, settings.fastCombatPresentation);
#endif

    screenTileUpdate(&game->mapArea, coords);
    EventHandler::wait_msecs(msecPerFrame);

    c->location->map->annotations->remove(coords, tile);

    screenTileUpdate(&game->mapArea, coords, false);
}

void GameController::flashTile(const Coords &coords, const std::string &tilename, int timeFactor) {
    Tile *tile = c->location->map->tileset->getByName(tilename);
    zu4_assert(tile, "no tile named '%s' found in tileset", tilename.c_str());
    flashTile(coords, tile->getId(), timeFactor);
}

/**
 * Provide feedback to user after a party event happens.
 */
void GameController::update(Party *party, PartyEvent &event) {
    int i;

    switch (event.type) {
    case PartyEvent::LOST_EIGHTH:
        // inform a player he has lost zero or more eighths of avatarhood.
        screenMessage("\n %cThou hast lost\n  an eighth!%c\n", FG_YELLOW, FG_WHITE);
        break;
    case PartyEvent::ADVANCED_LEVEL:
        screenMessage("\n%c%s\nThou art now Level %d%c\n", FG_YELLOW, event.player->getName().c_str(), event.player->getRealLevel(), FG_WHITE);
        gameSpellEffect('r', -1, SOUND_MAGIC); // Same as resurrect spell
        break;
    case PartyEvent::STARVING:
        screenMessage("\n%cStarving!!!%c\n", FG_YELLOW, FG_WHITE);
        /* FIXME: add sound effect here */

        // 2 damage to each party member for starving!
        for (i = 0; i < c->saveGame->members; i++)
            c->party->member(i)->applyDamage(2);
        break;
    default:
        break;
    }
}

/**
 * Provide feedback to user after a movement event happens.
 */
void GameController::update(Location *location, MoveEvent &event) {
    switch (location->map->type) {
    case Map::DUNGEON:
#ifdef ZU4_IOS
        // Capture the destination before a room tile transfers control to its
        // combat map, otherwise that visited room entrance would be lost.
        mobileRevealDungeon();
#endif
        avatarMovedInDungeon(event);
        break;
    case Map::COMBAT:
        // FIXME: let the combat controller handle it
        dynamic_cast<CombatController *>(eventHandler->getController())->movePartyMember(event);
        break;
    default:
        avatarMoved(event);
        break;
    }
#if defined(ZU4_IOS) || defined(ZU4_WEB)
    if (mobileWalkExecuting) mobileWalkResult = event.result;
#endif
}

void gameSpellEffect(int spell, int player, int sound) {

	int time;
    Spell::SpecialEffects effect = Spell::SFX_INVERT;

    if (player >= 0)
        c->stats->highlightPlayer(player);

    time = settings.spellEffectSpeed * 800 / settings.gameCyclesPerSecond;
    zu4_snd_play(sound, false, time);

    ///The following effect multipliers are not accurate
    switch(spell)
    {
    case 'g': /* gate */
    case 'r': /* resurrection */
        break;
    case 't': /* tremor */
        effect = Spell::SFX_TREMOR;
        break;
    default:
        /* default spell effect */
        break;
    }

    switch(effect)
    {
    case Spell::SFX_NONE:
        break;
    case Spell::SFX_TREMOR:
    case Spell::SFX_INVERT:
        gameUpdateScreen();
        game->mapArea.highlight(0, 0, VIEWPORT_W * TILE_WIDTH, VIEWPORT_H * TILE_HEIGHT);
        EventHandler::sleep(time);
        game->mapArea.unhighlight();

        if (effect == Spell::SFX_TREMOR) {
            gameUpdateScreen();
            zu4_snd_play(SOUND_RUMBLE, false, -1);
            screenShake(8);

        }

        break;
    }
}

void gameCastSpell(unsigned int spell, int caster, int param) {
    SpellCastError spellError;
    std::string msg;

    if (!spellCast(spell, caster, param, &spellError, true)) {
        msg = spellGetErrorMessage(spell, spellError);
        if (!msg.empty())
            screenMessage("%s", msg.c_str());
    }
}

/**
 * The main key handler for the game.  Interpretes each key as a
 * command - 'a' for attack, 't' for talk, etc.
 */
bool GameController::keyPressed(int key) {
#if defined(ZU4_IOS) || defined(ZU4_WEB)
    if (mobileWalkActive && !mobileWalkExecuting) mobileCancelWalk();
#endif
    bool valid = true;
    int endTurn = 1;
    Object *obj;
    MapTile *tile;

    /* Translate context-sensitive action key into a useful command */
    if (key == U4_ENTER && settings.enhancements && settings.enhancementsOptions.smartEnterKey) {
        /* Attempt to guess based on the character's surroundings etc, what
           action they want */

        /* Do they want to board something? */
        if (c->transportContext == TRANSPORT_FOOT) {
            obj = c->location->map->objectAt(c->location->coords);
            if (obj && (obj->getTile().getTileType()->isShip() ||
                        obj->getTile().getTileType()->isHorse() ||
                        obj->getTile().getTileType()->isBalloon()))
                key = 'b';
        }
        /* Klimb/Descend Balloon */
        else if (c->transportContext == TRANSPORT_BALLOON) {
            if (c->party->isFlying())
                key = 'd';
            else {
                key = 'k';
            }
        }
        /* X-it transport */
        else key = 'x';

        /* Klimb? */
        if ((c->location->map->portalAt(c->location->coords, ACTION_KLIMB) != NULL))
            key = 'k';
        /* Descend? */
        else if ((c->location->map->portalAt(c->location->coords, ACTION_DESCEND) != NULL))
            key = 'd';

		if (c->location->context == CTX_DUNGEON) {
			Dungeon *dungeon = static_cast<Dungeon *>(c->location->map);
			bool up = dungeon->ladderUpAt(c->location->coords);
			bool down = dungeon->ladderDownAt(c->location->coords);
			if (up && down) {
                key = 'k'; // This is consistent with the previous code. Ideally, I would have a UI here as well.
			} else if (up) {
				key = 'k';
			} else {
				key = 'd';
			}
		}

		/* Enter? */
		if (c->location->map->portalAt(c->location->coords, ACTION_ENTER) != NULL)
            key = 'e';

        /* Get Chest? */
        if (!c->party->isFlying()) {
            tile = c->location->map->tileAt(c->location->coords, WITH_GROUND_OBJECTS);

            if (tile->getTileType()->isChest()) key = 'g';
        }

        /* None of these? Default to search */
        if (key == U4_ENTER) key = 's';
    }

    if ((c->location->context & CTX_DUNGEON) && strchr("abefjlotxy", key)) {
        screenMessage("%cNot here!%c\n", FG_GREY, FG_WHITE);
	}
    else {
        switch (key) {

        case U4_UP:
        case U4_DOWN:
        case U4_LEFT:
        case U4_RIGHT:
            {
                /* move the avatar */
                std::string previous_map = c->location->map->fname;
                MoveResult retval = c->location->move(keyToDirection(key), true);

                /* horse doubles speed (make sure we're on the same map as the previous move first) */
                if (retval & (MOVE_SUCCEEDED | MOVE_SLOWED) &&
                    (c->transportContext == TRANSPORT_HORSE) && c->horseSpeed) {
                    gameUpdateScreen(); /* to give it a smooth look of movement */
                    if (previous_map == c->location->map->fname)
                        c->location->move(keyToDirection(key), false);
                }

                endTurn = (retval & MOVE_END_TURN); /* let the movement handler decide to end the turn */
            }

            break;

        case U4_FKEY:
        case U4_FKEY+1:
        case U4_FKEY+2:
        case U4_FKEY+3:
        case U4_FKEY+4:
        case U4_FKEY+5:
        case U4_FKEY+6:
        case U4_FKEY+7:
            /* teleport to dungeon entrances! */
            if (settings.debug && (c->location->context & CTX_WORLDMAP) && (c->transportContext & TRANSPORT_FOOT_OR_HORSE))
            {
                int portal = 16 + (key - U4_FKEY); /* find dungeon portal */
                c->location->coords = c->location->map->portals[portal]->coords;
            }
            else valid = false;
            break;

        case U4_FKEY+8:
            if (settings.debug && (c->location->context & CTX_WORLDMAP)) {
                setMap(mapMgr->get(MAP_DECEIT), 1, NULL);
                c->location->coords = (Coords){1, 0, 7};
                c->saveGame->orientation = DIR_SOUTH;
            }
            else valid = false;
            break;

        case U4_FKEY+9:
            if (settings.debug && (c->location->context & CTX_WORLDMAP)) {
                setMap(mapMgr->get(MAP_DESPISE), 1, NULL);
                c->location->coords = (Coords){3, 2, 7};
                c->saveGame->orientation = DIR_SOUTH;
            }
            else valid = false;
            break;

        case U4_FKEY+10:
            if (settings.debug && (c->location->context & CTX_WORLDMAP)) {
                setMap(mapMgr->get(MAP_DESTARD), 1, NULL);
                c->location->coords = (Coords){7, 6, 7};
                c->saveGame->orientation = DIR_SOUTH;
            }
            else valid = false;
            break;

        case U4_FKEY+11:
            if (settings.debug) {
                screenMessage("Torch: %d\n", c->party->getTorchDuration());
                screenPrompt();
            }
            else valid = false;
            break;

        case U4_CTRL + 'c':                     /* ctrl-C */
            if (settings.debug) {
                screenMessage("Cmd (h = help):");
                CheatMenuController cheatMenuController(this);
                eventHandler->pushController(&cheatMenuController);
                cheatMenuController.waitFor();
            }
            else valid = false;
            break;

        case U4_CTRL + 'd':                     /* ctrl-D */
            if (settings.debug) {
                destroy();
            }
            else valid = false;
            break;

        case U4_CTRL + 'h':                     /* ctrl-H */
            if (settings.debug) {
                screenMessage("Help!\n");
                screenPrompt();

                /* Help! send me to Lord British (who conveniently is right around where you are)! */
                setMap(mapMgr->get(100), 1, NULL);
                c->location->coords.x = 19;
                c->location->coords.y = 8;
                c->location->coords.z = 0;
            }
            else valid = false;
            break;

        case U4_CTRL + 'v':                    /* ctrl-V */
            {
                if (settings.debug && c->location->context == CTX_DUNGEON) {
                    screenMessage("3-D view %s\n", DungeonViewer.toggle3DDungeonView() ? "on" : "off");
                    endTurn = 0;
                }
                else valid = false;
            }
            break;

        case ' ':
            screenMessage("Pass\n");
            break;

        case '+':
        case '-':
        case U4_KEYPAD_ENTER:
            {
                int old_cycles = settings.gameCyclesPerSecond;
                if (key == '+' && ++settings.gameCyclesPerSecond > MAX_CYCLES_PER_SECOND)
                    settings.gameCyclesPerSecond = MAX_CYCLES_PER_SECOND;
                else if (key == '-' && --settings.gameCyclesPerSecond == 0)
                    settings.gameCyclesPerSecond = 1;
                else if (key == U4_KEYPAD_ENTER)
                    settings.gameCyclesPerSecond = DEFAULT_CYCLES_PER_SECOND;

                if (old_cycles != settings.gameCyclesPerSecond) {
                    eventTimerGranularity = (1000 / settings.gameCyclesPerSecond);
                    eventHandler->getTimer()->reset(eventTimerGranularity);

                    if (settings.gameCyclesPerSecond == DEFAULT_CYCLES_PER_SECOND)
                        screenMessage("Speed: Normal\n");
                    else if (key == '+')
                        screenMessage("Speed Up (%d)\n", settings.gameCyclesPerSecond);
                    else screenMessage("Speed Down (%d)\n", settings.gameCyclesPerSecond);
                }
                else if (settings.gameCyclesPerSecond == DEFAULT_CYCLES_PER_SECOND)
                    screenMessage("Speed: Normal\n");
            }

            endTurn = false;
            break;

        /* handle music volume adjustments */
        case ',':
            // decrease the volume if possible
            screenMessage("Music: %d%s\n", zu4_music_vol_dec(), "%");
            endTurn = false;
            break;
        case '.':
            // increase the volume if possible
            screenMessage("Music: %d%s\n", zu4_music_vol_inc(), "%");
            endTurn = false;
            break;

        /* handle sound volume adjustments */
        case '<':
            // decrease the volume if possible
            screenMessage("Sound: %d%s\n", zu4_snd_vol_dec(), "%");
            zu4_snd_play(SOUND_FLEE, true, -1);
            endTurn = false;
            break;
        case '>':
            // increase the volume if possible
            screenMessage("Sound: %d%s\n", zu4_snd_vol_inc(), "%");
            zu4_snd_play(SOUND_FLEE, true, -1);
            endTurn = false;
            break;

        case 'a':
            attack();
            break;

        case 'b':
            board();
            break;

        case 'c':
#ifdef ZU4_IOS
            endTurn = castSpell();
#else
            castSpell();
#endif
            break;

        case 'd': {
            // unload the map for the second level of Lord British's Castle. The reason
            // why is that Lord British's farewell is dependent on the number of party members.
            // Instead of just redoing the dialog, it's a bit severe, but easier to unload the
            // whole level.
            bool cleanMap = (c->party->size() == 1 && c->location->map->id == 100);
            if (!usePortalAt(c->location, c->location->coords, ACTION_DESCEND)) {
                if (c->transportContext == TRANSPORT_BALLOON) {
                    screenMessage("Land Balloon\n");
                    if (!c->party->isFlying())
                        screenMessage("%cAlready Landed!%c\n", FG_GREY, FG_WHITE);
                    else if (c->location->map->tileTypeAt(c->location->coords, WITH_OBJECTS)->canLandBalloon()) {
                        c->saveGame->balloonstate = 0;
                        c->opacity = 1;
                    }
                    else screenMessage("%cNot Here!%c\n", FG_GREY, FG_WHITE);
                }
                else screenMessage("%cDescend what?%c\n", FG_GREY, FG_WHITE);
            } else {
                if (cleanMap)
                    mapMgr->unloadMap(100);
            }
            break;
        }

        case 'e':
            if (!usePortalAt(c->location, c->location->coords, ACTION_ENTER)) {
                if (!c->location->map->portalAt(c->location->coords, ACTION_ENTER))
                    screenMessage("%cEnter what?%c\n", FG_GREY, FG_WHITE);
            }
            else endTurn = 0; /* entering a portal doesn't end the turn */
            break;

        case 'f':
            fire();
            break;

        case 'g':
            getChest();
            break;

        case 'h':
            holeUp();
            break;

        case 'i':
            screenMessage("Ignite torch!\n");
            if (c->location->context == CTX_DUNGEON) {
                if (!c->party->lightTorch())
                    screenMessage("%cNone left!%c\n", FG_GREY, FG_WHITE);
            }
            else screenMessage("%cNot here!%c\n", FG_GREY, FG_WHITE);
            break;

        case 'j':
            jimmy();
            break;

        case 'k':
            if (!usePortalAt(c->location, c->location->coords, ACTION_KLIMB)) {
                if (c->transportContext == TRANSPORT_BALLOON) {
                    c->saveGame->balloonstate = 1;
                    c->opacity = 0;
                    screenMessage("Klimb altitude\n");
                } else
                    screenMessage("%cKlimb what?%c\n", FG_GREY, FG_WHITE);
            }
            break;

        case 'l':
            /* can't use sextant in dungeon or in combat */
            if (c->location->context & ~(CTX_DUNGEON | CTX_COMBAT)) {
                if (c->saveGame->sextants >= 1)
                    screenMessage("Locate position\nwith sextant\n Latitude: %c'%c\"\nLongitude: %c'%c\"\n",
                                  c->location->coords.y / 16 + 'A', c->location->coords.y % 16 + 'A',
                                  c->location->coords.x / 16 + 'A', c->location->coords.x % 16 + 'A');
                else
                    screenMessage("%cLocate position with what?%c\n", FG_GREY, FG_WHITE);
            }
            else screenMessage("%cNot here!%c\n", FG_GREY, FG_WHITE);
            break;

        case 'm':
            mixReagents();
            break;

        case 'n':
            newOrder();
            break;

        case 'o':
            opendoor();
            break;

        case 'p':
            peer();
            break;

        case 'q':
            screenMessage("Quit & Save...\n%d moves\n", c->saveGame->moves);
            if (c->location->context & CTX_CAN_SAVE_GAME) {
                gameSave();
                screenMessage("Press Alt-x to quit\n");
            }
            else screenMessage("%cNot here!%c\n", FG_GREY, FG_WHITE);

            break;

        case 'r':
            readyWeapon();
            break;

        case 's':
            if (c->location->context == CTX_DUNGEON)
                dungeonSearch();
            else if (c->party->isFlying())
                screenMessage("Searching...\n%cDrift only!%c\n", FG_GREY, FG_WHITE);
            else {
                screenMessage("Searching...\n");

                const ItemLocation *item = itemAtLocation(c->location->map, c->location->coords);
                if (item) {
                    if (*item->isItemInInventory != NULL && (*item->isItemInInventory)(item->data))
                        screenMessage("%cNothing Here!%c\n", FG_GREY, FG_WHITE);
                    else {
                        if (item->name)
                            screenMessage("You find...\n%s!\n", item->name);
                        (*item->putItemInInventory)(item->data);
                    }
                } else
                    screenMessage("%cNothing Here!%c\n", FG_GREY, FG_WHITE);
            }

            break;

        case 't':
            talk();
            break;

		case 'u': {
            screenMessage("Use which item:\n");
            if (settings.enhancements) {
                /* a little xu4 enhancement: show items in inventory when prompted for an item to use */
                c->stats->setView(STATS_ITEMS);
            }
#if defined(ZU4_IOS) || defined(ZU4_WEB)
            endTurn = gameUseQuestItem();
            c->stats->setView(STATS_PARTY_OVERVIEW);
#else
            itemUse(gameGetInput().c_str());
#endif
            break;
		}

        case 'v':
            if (zu4_music_toggle())
                screenMessage("Volume On!\n");
            else
                screenMessage("Volume Off!\n");
            endTurn = false;
            break;

        case 'w':
            wearArmor();
            break;

        case 'x':
            if ((c->transportContext != TRANSPORT_FOOT) && !c->party->isFlying()) {
                Object *obj = c->location->map->addObject(c->party->getTransport(), c->party->getTransport(), c->location->coords);
                if (c->transportContext == TRANSPORT_SHIP)
                    c->lastShip = obj;

                Tile *avatar = c->location->map->tileset->getByName("avatar");
                zu4_assert(avatar, "no avatar tile found in tileset");
                c->party->setTransport(avatar->getId());
                c->horseSpeed = 0;
                screenMessage("X-it\n");
            } else
                screenMessage("%cX-it What?%c\n", FG_GREY, FG_WHITE);
            break;

        case 'y':
            screenMessage("Yell ");
            if (c->transportContext == TRANSPORT_HORSE) {
                if (c->horseSpeed == 0) {
                    screenMessage("Giddyup!\n");
                    c->horseSpeed = 1;
                } else {
                    screenMessage("Whoa!\n");
                    c->horseSpeed = 0;
                }
            } else
                screenMessage("%cWhat?%c\n", FG_GREY, FG_WHITE);
            break;

        case 'z':
            ztatsFor();
            break;

        case 'c' + U4_ALT:
            if (settings.debug && c->location->map->isWorldMap()) {
                /* first teleport to the abyss */
                c->location->coords.x = 0xe9;
                c->location->coords.y = 0xe9;
                setMap(mapMgr->get(MAP_ABYSS), 1, NULL);
                /* then to the final altar */
                c->location->coords.x = 7;
                c->location->coords.y = 7;
                c->location->coords.z = 7;
            }
            break;

        case 'h' + U4_ALT: {
            ReadChoiceController pauseController("");

            screenMessage("Key Reference:\n"
                          "Arrow Keys: Move\n"
                          "a: Attack\n"
                          "b: Board\n"
                          "c: Cast Spell\n"
                          "d: Descend\n"
                          "e: Enter\n"
                          "f: Fire Cannons\n"
                          "g: Get Chest\n"
                          "h: Hole up\n"
                          "i: Ignite torch\n"
                          "(more)");

            eventHandler->pushController(&pauseController);
            pauseController.waitFor();

            screenMessage("\n"
                          "j: Jimmy lock\n"
                          "k: Klimb\n"
                          "l: Locate\n"
                          "m: Mix reagents\n"
                          "n: New Order\n"
                          "o: Open door\n"
                          "p: Peer at Gem\n"
                          "q: Quit & Save\n"
                          "r: Ready weapon\n"
                          "s: Search\n"
                          "t: Talk\n"
                          "(more)");

            eventHandler->pushController(&pauseController);
            pauseController.waitFor();

            screenMessage("\n"
                          "u: Use Item\n"
                          "v: Volume On/Off\n"
                          "w: Wear armour\n"
                          "x: eXit\n"
                          "y: Yell\n"
                          "z: Ztats\n"
                          "Space: Pass\n"
                          ",: - Music Vol\n"
                          ".: + Music Vol\n"
                          "<: - Sound Vol\n"
                          ">: + Sound Vol\n"
                          "(more)");

            eventHandler->pushController(&pauseController);
            pauseController.waitFor();

            screenMessage("\n"
                          "Alt-Q: Main Menu\n"
                          "Alt-V: Version\n"
                          "Alt-X: Quit\n"
                          "\n"
                          "\n"
                          "\n"
                          "\n"
                          "\n"
                          "\n"
                          "\n"
                          "\n"
                          );
            screenPrompt();
            break;
        }

        case 'q' + U4_ALT:
            {
                // TODO - implement loop in main() and let quit fall back to there
                // Quit to the main menu
                extern bool quit;
                endTurn = false;

                screenMessage("Quit to menu?");
                char choice = ReadChoiceController::get("yn \n\033");
                screenMessage("%c", choice);
                if (choice != 'y') {
                    screenMessage("\n");
                    break;
                }

                eventHandler->setScreenUpdate(NULL);
                eventHandler->popController();

                eventHandler->pushController(intro);

                // Fade out the music and hide the cursor
                //before returning to the menu.
                zu4_music_fadeout(1000);
                screenHideCursor();

                intro->init();
                eventHandler->run();

                if (!quit) {
                    eventHandler->setControllerDone(false);
                    eventHandler->popController();
                    eventHandler->pushController(this);

                	if (intro->hasInitiatedNewGame())
                    {
                    	//Loads current savegame
                    	init();
                    }
                    else
                    {
                    	//Inits screen stuff without renewing game
                    	initScreen();
                    	initScreenWithoutReloadingState();
                    }

                    this->mapArea.reinit();

                    intro->deleteIntro();
                    eventHandler->run();
                }
            }
            break;

        case 'v' + U4_ALT:
            screenMessage("XU4 %s\n", VERSION);
            endTurn = false;
            break;

        // Turn sound effects on/off
        case 's' + U4_ALT:
            // FIXME: there's probably a more intuitive key combination for this
            settings.soundVol = !settings.soundVol;
            screenMessage("Sound FX %s!\n", settings.soundVol ? "on" : "off");
            endTurn = false;
            break;

        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            if (settings.enhancements && settings.enhancementsOptions.activePlayer)
                gameSetActivePlayer(key - '1');
            else screenMessage("%cBad command!%c\n", FG_GREY, FG_WHITE);

            endTurn = 0;
            break;

        default:
            valid = false;
            break;
        }
	}

    if (valid && endTurn) {
        if (eventHandler->getController() == game)
            c->location->turnCompleter->finishTurn();
    }
    else if (!endTurn) {
        /* if our turn did not end, then manually redraw the text prompt */
        screenPrompt();
    }

    return valid || KeyHandler::defaultHandler(key, NULL);
}

std::string gameGetInput(int maxlen) {
    screenEnableCursor();
    screenShowCursor();
    return ReadStringController::get(maxlen, TEXT_AREA_X + c->col, TEXT_AREA_Y + c->line);
}

const char *gameGetInputC() { // FIXME: this whole function is temporary
    screenEnableCursor();
    screenShowCursor();
    return ReadStringController::get(32, TEXT_AREA_X + c->col, TEXT_AREA_Y + c->line).c_str();
}

int gameGetPlayer(bool canBeDisabled, bool canBeActivePlayer, const char *prompt) {
#ifndef ZU4_IOS
    (void)prompt;
#endif
    int player;
    if (c->saveGame->members <= 1)
    {
        player = 0;
    }
    else
    {
        if (canBeActivePlayer && (c->party->getActivePlayer() >= 0))
        {
            player = c->party->getActivePlayer();
        }
        else
        {
#ifdef ZU4_IOS
            std::vector<TopicJournal::Choice> choices;
            for (int i = 0; i < c->party->size(); ++i) {
                PartyMember *member = c->party->member(i);
                if (canBeDisabled || !member->isDisabled())
                    choices.push_back({std::to_string(i), member->getName() + " · " + mobileCondition(member->getStatus()) + "\nHP " + std::to_string(member->getHp()) + "/" + std::to_string(member->getMaxHp())});
            }
            choices.push_back(mobileDismiss("cancel", "Cancel"));
            MobileTopicInput input;
            std::string choice = input.read(prompt, choices, 0, false, false,
                ZU4_TOPIC_PANEL_PARTY_SELECTION);
            player = (choice == "cancel" || choice == "bye") ? -1 : atoi(choice.c_str());
#else
            ReadPlayerController readPlayerController;
            eventHandler->pushController(&readPlayerController);
            player = readPlayerController.waitFor();
#endif
        }

        if (player == -1)
        {
            screenMessage("None\n");
            return -1;
        }
    }

    c->col--;// display the selected character name, in place of the number
    if ((player >= 0) && (player < 8))
    {
        screenMessage("%s\n", c->saveGame->players[player].name); //Write player's name after prompt
    }

    if (!canBeDisabled && c->party->member(player)->isDisabled())
    {
        screenMessage("%cDisabled!%c\n", FG_GREY, FG_WHITE);
        return -1;
    }

    zu4_assert(player < c->party->size(), "player %d, but only %d members\n", player, c->party->size());
    return player;
}

std::string gameGetRememberedAnswer(const char *prompt) {
#ifdef ZU4_IOS
    std::vector<std::string> words;
    for (auto it = mobileTopics.entries().rbegin(); it != mobileTopics.entries().rend(); ++it) {
        if (it->word.size() > 1 && it->word.size() <= 32 &&
            std::find(words.begin(), words.end(), it->word) == words.end()) words.push_back(it->word);
    }
    std::size_t page = 0;
    for (;;) {
        std::vector<TopicJournal::Choice> choices;
        for (std::size_t i = page * 8; i < words.size() && i < page * 8 + 8; ++i)
            choices.push_back({"word:" + std::to_string(i), words[i]});
        if ((page + 1) * 8 < words.size()) choices.push_back({"older:", "Older words"});
        if (page) choices.push_back({"newer:", "Newer words"});
        MobileTopicInput input;
        std::string answer = input.read(std::string(prompt) +
            "\n\nChoose a remembered word or enter your own answer. These words come from conversations you have already read.", choices, 32);
        if (answer == "older:") { ++page; continue; }
        if (answer == "newer:") { if (page) --page; continue; }
        if (answer.compare(0, 5, "word:") == 0) {
            std::size_t index = (std::size_t)atoi(answer.c_str() + 5);
            if (index < words.size()) return words[index];
            continue;
        }
        return answer;
    }
#else
    (void)prompt;
    return gameGetInput();
#endif
}

std::string gameGetCharacterName() {
#ifdef ZU4_IOS
    MobileTopicInput input;
    std::string name = input.read("By what name shalt thou be known?\n\nChoose a name of up to 12 letters, numbers or spaces.",
        {mobileDismiss("cancel", "Cancel creation")}, 12, true);
    if (!input.submittedText || name.find_first_not_of(' ') == std::string::npos) return "";
    return name;
#else
    return gameGetInput(12);
#endif
}

std::string gameGetVendorTopic(const std::string &context, const std::vector<std::string> &topics, int maxLength) {
#ifdef ZU4_IOS
    std::vector<TopicJournal::Choice> choices;
    for (const std::string &topic : topics) {
        if (mobileTopics.knowsPhrase(topic) && std::none_of(choices.begin(), choices.end(), [&](const TopicJournal::Choice &c) { return c.keyword == topic; }))
            choices.push_back({topic, topic});
    }
    choices.push_back(mobileDismiss("cancel", "Never mind"));
    MobileTopicInput input;
    std::string answer = input.read(context + "\n\nTopics appear after you encounter them. You may also ask about another topic.", choices, maxLength);
    return !input.submittedText && (answer == "cancel" || answer == "bye") ? "" : answer;
#elif defined(ZU4_WEB)
    // Do not expose the script's hidden topic catalogue before discovery.
    (void)topics;
    return webReadPrompt("text", "Ask a topic", context, {{"\033", "Never mind"}}, maxLength, true);
#else
    (void)context;
    (void)topics;
    return ReadStringController::get(maxLength, TEXT_AREA_X + c->col, TEXT_AREA_Y + c->line);
#endif
}

void gameRecordRevealedText(const std::string &text, const std::string &source,
                            const std::string &speaker, const std::string &place,
                            const std::string &kind, const std::string &topic) {
#ifdef ZU4_IOS
    if (text.empty()) return;
    mobileTopics.observe(text, source, speaker, place, kind, topic);
    if (!mobileTopics.save(gameSaveDirectory() + "topics.txt"))
        zu4_error(ZU4_LOG_WRN, "Unable to save discovered conversation topics.");
#elif defined(ZU4_WEB)
    webJournalObserve({text,source,speaker,place,kind,topic});
#else
    (void)text;
    (void)source;
    (void)speaker;
    (void)place;
    (void)kind;
    (void)topic;
#endif
}

void gameObserveVendorDialogue(const std::string &text) {
#ifdef ZU4_IOS
    if (!text.empty()) observeMobileDialogue(text, nullptr);
#elif defined(ZU4_WEB)
    webJournalDialogue(text,nullptr);
#else
    (void)text;
#endif
}

char gameGetVendorChoice(const std::string &options, const std::string &context, bool continuation,
                         const std::vector<std::string> &names, bool allowCancel,
                         bool compactPresentation, bool controlsOnly) {
#ifdef ZU4_IOS
    std::vector<TopicJournal::Choice> choices;
    std::string keys;
    for (unsigned char key : options) {
        if (key > 32 && key < 127 && keys.find(key) == std::string::npos) keys += key;
    }
    for (char key : keys) {
        std::string label(1, (char)toupper((unsigned char)key));
        size_t optionIndex = options.find(key);
        if (optionIndex < names.size() && !names[optionIndex].empty()) label = names[optionIndex];
        if (keys == "yn") label = key == 'y' ? "Yes" : "No";
        else if (keys == "bs") label = key == 'b' ? "Buy" : "Sell";
        else if (keys == "fa") label = key == 'f' ? "Food" : "Ale";
        choices.push_back({std::string(1, key), label});
    }
    if (continuation) choices.push_back({"cancel", "Continue"});
    else if (allowCancel) choices.push_back(mobileDismiss("cancel", "Cancel"));
    MobileTopicInput input;
    Zu4TopicPanelStyle style = controlsOnly ? ZU4_TOPIC_PANEL_CONTROLS_ONLY :
        (compactPresentation ? ZU4_TOPIC_PANEL_COMPACT_MENU : ZU4_TOPIC_PANEL_STANDARD);
    std::string choice = input.read(context.empty() ? "Shop" : context, choices, 0, false, false, style);
    return choice.size() == 1 && keys.find(choice[0]) != std::string::npos ? choice[0] : '\033';
#elif defined(ZU4_WEB)
    (void)compactPresentation;
    (void)controlsOnly;
    std::vector<WebPromptOption> choices = webVendorOptions(options, names, continuation, allowCancel);
    std::string answer = webReadPrompt("choice", continuation ? "Continue" : "Shop", context, choices);
    return answer.empty() ? '\033' : answer[0];
#else
    (void)context;
    (void)continuation;
    (void)names;
    (void)allowCancel;
    (void)compactPresentation;
    (void)controlsOnly;
    return ReadChoiceController::get(options);
#endif
}

int gameGetAmountInput(int maxDigits, const std::string &context) {
#ifdef ZU4_IOS
    int maximum = 0;
    for (int i = 0; i < maxDigits && i < 6; ++i) maximum = maximum * 10 + 9;
    int amount = 0;
    for (;;) {
        std::vector<TopicJournal::Choice> choices;
        for (int step : {1, 10, 100, 1000, 10000, 100000}) {
            if (step > maximum) break;
            if (amount <= maximum - step) choices.push_back({"+" + std::to_string(step), "+" + std::to_string(step)});
            if (amount >= step) choices.push_back({"-" + std::to_string(step), "−" + std::to_string(step)});
        }
        choices.push_back({"cancel", "Cancel"});
        choices.push_back({"confirm", "Confirm " + std::to_string(amount)});
        MobileTopicInput input;
        std::string choice = input.read(context + "\n\nChoose quantity or amount\n\n" + std::to_string(amount), choices, 0);
        if (choice == "confirm") return amount;
        if (choice == "cancel" || choice == "bye") return -1;
        if (!choice.empty() && (choice[0] == '+' || choice[0] == '-')) {
            int next = amount + atoi(choice.c_str());
            if (next >= 0 && next <= maximum) amount = next;
        }
    }
#elif defined(ZU4_WEB)
    bool cancelled = false;
    std::string answer = webReadPrompt("amount", "Choose an amount", context +
        "\n\nGold carried: " + std::to_string(c->saveGame->gold), {{"\033", "Cancel"}}, maxDigits, true, &cancelled);
    return cancelled ? -1 : atoi(answer.c_str());
#else
    (void)context;
    return ReadIntController::get(maxDigits, TEXT_AREA_X + c->col, TEXT_AREA_Y + c->line);
#endif
}

bool gameUseQuestItem() {
#ifdef ZU4_IOS
    std::vector<TopicJournal::Choice> items;
    for (const ItemLocation *item : itemOwnedUsable())
        items.push_back({item->shortname, item->name ? item->name :
            (!strcmp(item->shortname, "key") ? "Three-part key" : "Choose stones")});
    bool empty = items.empty();
    items.push_back(mobileDismiss("cancel", "Cancel"));
    MobileTopicInput input;
    std::string choice = input.read(empty ? "No usable quest items are carried." : "Use a quest item", items, 0);
    if (choice == "cancel" || choice == "bye") return false;
    itemUse(choice.c_str());
#elif defined(ZU4_WEB)
    std::vector<WebPromptOption> choices;
    for (const ItemLocation *item : itemOwnedUsable())
        choices.push_back({item->shortname, item->name ? item->name : item->shortname});
    bool empty = choices.empty();
    choices.push_back({"\033", "Cancel"});
    std::string choice = webReadPrompt("choice", "Use a quest item",
        empty ? "No usable quest items are carried." : "Choose an item carried by the party.", choices);
    if (choice.empty()) return false;
    itemUse(choice.c_str());
#else
    itemUse(gameGetInput().c_str());
#endif
    return true;
}

int gameGetAttackRange(int maximum) {
#ifdef ZU4_IOS
    std::vector<TopicJournal::Choice> choices;
    for (int range = 1; range <= maximum && range <= 9; ++range) {
        std::string value = std::to_string(range);
        choices.push_back({value, value + (range == 1 ? " tile" : " tiles")});
    }
    choices.push_back(mobileDismiss("cancel", "Cancel attack"));
    MobileTopicInput input;
    std::string choice = input.read("Choose attack distance", choices, 0);
    return choice.size() == 1 && choice[0] >= '1' && choice[0] <= '9'
        && choice[0] - '0' <= maximum ? choice[0] - '0' : 0;
#else
    int choice = ReadChoiceController::get("123456789");
    return choice >= '1' && choice <= '9' && choice - '0' <= maximum ? choice - '0' : 0;
#endif
}

int gameGetMeditationCycles() {
#ifdef ZU4_IOS
    MobileTopicInput input;
    std::string choice = input.read("For how many cycles wilt thou meditate?",
        {{"1", "1 cycle"}, {"2", "2 cycles"}, {"3", "3 cycles"},
         mobileDismiss("cancel", "Leave shrine")}, 0);
    return choice.size() == 1 && choice[0] >= '1' && choice[0] <= '3' ? choice[0] - '0' : 0;
#elif defined(ZU4_WEB)
    std::string answer = webReadPrompt("choice", "Meditation", "For how many cycles wilt thou meditate?",
        {{"1", "1 cycle"}, {"2", "2 cycles"}, {"3", "3 cycles"}, {"\033", "Leave shrine"}});
    return answer.empty() ? 0 : atoi(answer.c_str());
#else
    int choice = ReadChoiceController::get("0123\015\033");
    return choice >= '0' && choice <= '3' ? choice - '0' : 0;
#endif
}

std::string gameGetMantraInput() {
#ifdef ZU4_IOS
    // This catalogue only filters already-exposed words; it never teaches them.
    const char *mantras[] = {"ahm", "mu", "ra", "beh", "cah", "summ", "om", "lum"};
    std::vector<TopicJournal::Choice> choices;
    for (const char *mantra : mantras) {
        if (mobileTopics.knowsExact(mantra)) choices.push_back({mantra, mantra});
    }
    choices.push_back(mobileDismiss("cancel", "End meditation"));
    MobileTopicInput input;
    std::string choice = input.read("Speak thy mantra\n\nSuggestions contain only words already encountered. You may also enter a mantra yourself.", choices, 4);
    return choice == "cancel" || choice == "bye" ? "" : choice;
#elif defined(ZU4_WEB)
    return webReadPrompt("text", "Speak thy mantra", "Enter thy mantra.", {{"\033", "End meditation"}}, 4, true);
#else
    return ReadStringController::get(4, TEXT_AREA_X + c->col, TEXT_AREA_Y + c->line);
#endif
}

std::string gameGetVirtueInput(const char *prompt, bool includePrinciples) {
#ifdef ZU4_IOS
    std::vector<TopicJournal::Choice> choices;
    for (int i = 0; i < 8; ++i) {
        std::string virtue = getVirtueName((Virtue)i);
        choices.push_back({virtue, virtue});
    }
    if (includePrinciples) {
        for (int i = 0; i < 3; ++i) {
            std::string principle = getBaseVirtueName(static_cast<BaseVirtue>(1 << i));
            choices.push_back({principle, principle});
        }
    } else choices.push_back(mobileDismiss("cancel", "Cancel"));
    MobileTopicInput input;
    std::string choice = input.read(prompt, choices, 0);
    return choice == "cancel" || choice == "bye" ? "" : choice;
#elif defined(ZU4_WEB)
    std::vector<WebPromptOption> choices;
    for (int i = 0; i < 8; ++i) {
        std::string virtue = getVirtueName(static_cast<Virtue>(i));
        choices.push_back({virtue, virtue});
    }
    if (includePrinciples) {
        for (int i = 0; i < 3; ++i) {
            std::string principle = getBaseVirtueName(static_cast<BaseVirtue>(1 << i));
            choices.push_back({principle, principle});
        }
    } else choices.push_back({"\033", "Leave shrine"});
    return webReadPrompt("text", "Choose a virtue", prompt, choices, 32, true);
#else
    (void)prompt;
    (void)includePrinciples;
    return gameGetInput(32);
#endif
}

std::string gameGetStoneInput() {
#ifdef ZU4_IOS
    std::vector<TopicJournal::Choice> choices;
    for (int i = 0; i < 8; ++i) {
        if (c->saveGame->stones & (1 << i)) {
            std::string color = getStoneName((Virtue)i);
            choices.push_back({color, color + " stone"});
        }
    }
    choices.push_back(mobileDismiss("cancel", "Cancel offering"));
    MobileTopicInput input;
    std::string choice = input.read("Choose a stone to offer", choices, 0);
    return choice == "cancel" || choice == "bye" ? "" : choice;
#else
    return gameGetInput(32);
#endif
}

Direction gameGetDirection(const char *prompt) {
#ifdef ZU4_IOS
    MobileTopicInput input;
    std::string choice = input.read(prompt, {{"north", "North ↑"}, {"south", "South ↓"},
        {"west", "West ←"}, {"east", "East →"}, {"cancel", "Cancel"}}, 0);
    Direction dir = DIR_NONE;
    if (choice == "north") dir = DIR_NORTH;
    else if (choice == "south") dir = DIR_SOUTH;
    else if (choice == "west") dir = DIR_WEST;
    else if (choice == "east") dir = DIR_EAST;
    screenMessage("%s\n", dir == DIR_NONE ? "Cancelled" : getDirectionName(dir));
    return dir;
#else
    (void)prompt;
    ReadDirController dirController;

	screenMessage("Dir?");

    eventHandler->pushController(&dirController);
    Direction dir = dirController.waitFor();

    screenMessage("\b\b\b\b");

    if (dir == DIR_NONE) {
        screenMessage("    \n");
        return dir;
    }
    else {
        screenMessage("%s\n", getDirectionName(dir));
        return dir;
    }
#endif
}

bool gameSpellMixHowMany(int spell, int num, Ingredients *ingredients) {
    int i;

    /* entered 0 mixtures, don't mix anything! */
    if (num == 0) {
        screenMessage("\nNone mixed!\n");
        ingredients->revert();
        return false;
    }

    /* if they ask for more than will give them 99, only use what they need */
    if (num > 99 - c->saveGame->mixtures[spell]) {
        num = 99 - c->saveGame->mixtures[spell];
        screenMessage("\n%cOnly need %d!%c\n", FG_GREY, num, FG_WHITE);
    }

    screenMessage("\nMixing %d...\n", num);

    /* see if there's enough reagents to make number of mixtures requested */
    if (!ingredients->checkMultiple(num)) {
        screenMessage("\n%cYou don't have enough reagents to mix %d spells!%c\n", FG_GREY, num, FG_WHITE);
        ingredients->revert();
        return false;
    }

    screenMessage("\nYou mix the Reagents, and...\n");
    if (spellMix(spell, ingredients)) {
        screenMessage("Success!\n\n");
        /* mix the extra spells */
        ingredients->multiply(num);
        for (i = 0; i < num-1; i++)
            spellMix(spell, ingredients);
    }
    else
        screenMessage("It Fizzles!\n\n");

    return true;
}

bool ZtatsController::keyPressed(int key) {
    switch (key) {
    case U4_UP:
    case U4_LEFT:
        c->stats->prevItem();
        return true;
    case U4_DOWN:
    case U4_RIGHT:
        c->stats->nextItem();
        return true;
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
        if (c->saveGame->members >= key - '0')
            c->stats->setView(StatsView(STATS_CHAR1 + key - '1'));
        return true;
    case '0':
        c->stats->setView(StatsView(STATS_WEAPONS));
        return true;
    case U4_ESC:
    case U4_SPACE:
    case U4_ENTER:
        c->stats->setView(StatsView(STATS_PARTY_OVERVIEW));
        doneWaiting();
        return true;
    default:
        return KeyHandler::defaultHandler(key, NULL);
    }
}

void destroy() {
    screenMessage("Destroy Object\nDir: ");

    Direction dir = gameGetDirection("Destroy an object in which direction?");

    if (dir == DIR_NONE)
        return;

    std::vector<Coords> path = gameGetDirectionalActionPath(MASK_DIR(dir), MASK_DIR_ALL, c->location->coords,
                                                       1, 1, NULL, true);
    for (std::vector<Coords>::iterator i = path.begin(); i != path.end(); i++) {
        if (destroyAt(*i))
            return;
    }

    screenMessage("%cNothing there!%c\n", FG_GREY, FG_WHITE);
}

bool destroyAt(const Coords &coords) {
    Object *obj = c->location->map->objectAt(coords);

    if (obj) {
        if (isCreature(obj)) {
            Creature *c = dynamic_cast<Creature*>(obj);
            screenMessage("%s Destroyed!\n", c->getName().c_str());
        }
        else {
            Tile *t = c->location->map->tileset->get(obj->getTile().id);
            screenMessage("%s Destroyed!\n", t->getName().c_str());
        }

        c->location->map->removeObject(obj);
        screenPrompt();

        return true;
    }

    return false;
}

void attack() {
    screenMessage("Attack: ");

    if (c->party->isFlying()) {
        screenMessage("\n%cDrift only!%c\n", FG_GREY, FG_WHITE);
        return;
    }

    Direction dir = gameGetDirection("Attack in which direction?");

    if (dir == DIR_NONE)
        return;

    std::vector<Coords> path = gameGetDirectionalActionPath(MASK_DIR(dir), MASK_DIR_ALL, c->location->coords,
                                                                       1, 1, NULL, true);
    for (std::vector<Coords>::iterator i = path.begin(); i != path.end(); i++) {
        if (attackAt(*i))
            return;
    }

    screenMessage("%cNothing to Attack!%c\n", FG_GREY, FG_WHITE);
}

/**
 * Attempts to attack a creature at map coordinates x,y.  If no
 * creature is present at that point, zero is returned.
 */
bool attackAt(const Coords &coords) {
    Object *under;
    const Tile *ground;
    Creature *m;

    m = dynamic_cast<Creature*>(c->location->map->objectAt(coords));
    /* nothing attackable: move on to next tile */
    if (m == NULL || !m->isAttackable())
        return false;

    /* attack successful */
    /// TODO: CHEST: Make a user option to not make chests change battlefield
    /// map (1 of 2)
    ground = c->location->map->tileTypeAt(c->location->coords, WITH_GROUND_OBJECTS);
    if (!ground->isChest()) {
        ground = c->location->map->tileTypeAt(c->location->coords, WITHOUT_OBJECTS);
        if ((under = c->location->map->objectAt(c->location->coords)) &&
            under->getTile().getTileType()->isShip())
            ground = under->getTile().getTileType();
    }

    /* You're attacking a townsperson!  Alert the guards! */
    if ((m->getType() == Object::PERSON) && (m->getMovementBehavior() != MOVEMENT_ATTACK_AVATAR))
        c->location->map->alertGuards();

    /* not good karma to be killing the innocent.  Bad avatar! */
    if (m->isGood() || /* attacking a good creature */
        /* attacking a docile (although possibly evil) person in town */
        ((m->getType() == Object::PERSON) && (m->getMovementBehavior() != MOVEMENT_ATTACK_AVATAR)))
        c->party->adjustKarma(KA_ATTACKED_GOOD);

    CombatController *cc = new CombatController(CombatMap::mapForTile(ground, c->party->getTransport().getTileType(), m));
    cc->init(m);
    cc->begin();
    return true;
}

void board() {
    if (c->transportContext != TRANSPORT_FOOT) {
        screenMessage("Board: %cCan't!%c\n", FG_GREY, FG_WHITE);
        return;
    }

    Object *obj = c->location->map->objectAt(c->location->coords);
    if (!obj) {
        screenMessage("%cBoard What?%c\n", FG_GREY, FG_WHITE);
        return;
    }

    const Tile *tile = obj->getTile().getTileType();
    if (tile->isShip()) {
        screenMessage("Board Frigate!\n");
        if (c->lastShip != obj)
            c->party->setShipHull(50);
    }
    else if (tile->isHorse())
        screenMessage("Mount Horse!\n");
    else if (tile->isBalloon())
        screenMessage("Board Balloon!\n");
    else {
        screenMessage("%cBoard What?%c\n", FG_GREY, FG_WHITE);
        return;
    }

    c->party->setTransport(obj->getTile());
    c->location->map->removeObject(obj);
}

bool castSpell(int player) {
    bool performed = false;
    auto attempt = [&performed](unsigned int spell, int caster, int param) {
        performed = true;
        gameCastSpell(spell, caster, param);
    };
    if (player == -1) {
#ifndef ZU4_IOS
        screenMessage("Cast Spell!\nPlayer: ");
#endif
        player = gameGetPlayer(false, true, "Choose caster");
    }
    if (player == -1)
        return false;

    // get the spell to cast
    c->stats->setView(STATS_MIXTURES);
#ifndef ZU4_IOS
    screenMessage("Spell: ");
#endif

    int spell = -1;
#ifdef ZU4_IOS
    int page = 0;
    const int spellsPerPage = 8;
    auto availability = [](SpellCastError error) -> const char * {
        switch (error) {
        case CASTERR_NOERROR: return "Ready";
        case CASTERR_NOMIX: return "No mixtures";
        case CASTERR_MPTOOLOW: return "Not enough MP";
        case CASTERR_COMBATONLY: return "Combat only";
        case CASTERR_DUNGEONONLY: return "Dungeon only";
        case CASTERR_WORLDMAPONLY: return "Outdoors only";
        case CASTERR_WRONGCONTEXT: return "Unavailable here";
        default: return "Unavailable";
        }
    };
    for (;;) {
        std::vector<TopicJournal::Choice> choices;
        for (int i = page * spellsPerPage;
             i < SPELL_MAX && i < page * spellsPerPage + spellsPerPage; ++i) {
            SpellCastError readiness = spellCheckPrerequisites(i, player);
            std::string label = std::string(1, (char)('A' + i)) + " · " + spellGetName(i) +
                "  ·  " + std::to_string(c->saveGame->mixtures[i]) +
                " mixed  ·  " + std::to_string(spellGetRequiredMP(i)) + " MP";
            // A zero mixture count already explains why the spell cannot be
            // cast; reserve the final status for information not shown yet.
            if (readiness != CASTERR_NOMIX)
                label += "  ·  " + std::string(availability(readiness));
            choices.push_back({std::to_string(i), label});
        }
        if ((page + 1) * spellsPerPage < SPELL_MAX) choices.push_back({"next", "Next spells"});
        if (page) choices.push_back({"previous", "Previous spells"});
        choices.push_back(mobileDismiss("cancel", "Close"));
        MobileTopicInput input;
        std::string choice = input.read("Spellbook\n\n" + c->party->member(player)->getName() +
            " · " + std::to_string(c->party->member(player)->getMp()) + " MP", choices, 0,
            false, false, ZU4_TOPIC_PANEL_DENSE_FULLSCREEN);
        if (choice == "cancel" || choice == "bye") break;
        if (choice == "next") { ++page; continue; }
        if (choice == "previous") { if (page) --page; continue; }
        int candidate = atoi(choice.c_str());
        if (candidate < 0 || candidate >= SPELL_MAX) continue;
        const Spell *definition = getSpell(candidate);
        int capacity = MobileRules::mixCapacity(definition->components, c->saveGame->reagents,
            REAG_MAX, c->saveGame->mixtures[candidate]);
        std::string details = std::string(definition->name) + "\n\n" +
            std::to_string(definition->mp) + " MP · " +
            std::to_string(c->saveGame->mixtures[candidate]) + " mixed";
        if (eventHandler->getController() == game)
            details += " · Can mix " + std::to_string(capacity);
        details += "\n\nReagents per mixture";
        for (int reagent = 0; reagent < REAG_MAX; ++reagent)
            if (definition->components & (1 << reagent)) {
                int available = c->saveGame->reagents[reagent];
                details += "\n" + std::string(getReagentName((Reagent)reagent)) + " — " +
                    std::to_string(available) + " held";
                if (available == 0) details += " · Missing";
            }
        SpellCastError error = spellCheckPrerequisites(candidate, player);
        std::vector<TopicJournal::Choice> actions;
        if (error == CASTERR_NOERROR) actions.push_back({"cast", "Cast spell"});
        else details += "\n\nCannot cast · " + std::string(availability(error));
        if (eventHandler->getController() == game) {
            if (capacity > 0) actions.push_back({"mix", "Mix this spell"});
        }
        actions.push_back(mobileBack("back", "Spellbook"));
        actions.push_back(mobileDismiss("close", "Close"));
        MobileTopicInput confirm;
        std::string decision = confirm.read(details, actions, 0, false, true,
            ZU4_TOPIC_PANEL_DENSE_FULLSCREEN);
        if (decision == "close" || decision == "bye") break;
        if (decision == "cast") { spell = candidate; break; }
        if (decision == "mix") {
            std::vector<TopicJournal::Choice> quantities{{"1", "Mix 1"}};
            if (capacity >= 5) quantities.push_back({"5", "Mix 5"});
            if (capacity > 1 && capacity != 5) quantities.push_back({std::to_string(capacity), "Mix " + std::to_string(capacity) + " (maximum)"});
            quantities.push_back(mobileDismiss("cancel", "Cancel"));
            MobileTopicInput quantity;
            std::string chosen = quantity.read(std::string("Mix ") + definition->name + "\n\nReagents are consumed only when you choose a quantity.", quantities, 0);
            if (chosen == "cancel" || chosen == "bye") continue;
            int amount = atoi(chosen.c_str());
            int available = MobileRules::mixCapacity(definition->components, c->saveGame->reagents, REAG_MAX, c->saveGame->mixtures[candidate]);
            if (amount < 1 || amount > available) continue;
            Ingredients ingredients;
            bool reserved = true;
            for (int reagent = 0; reagent < REAG_MAX; ++reagent)
                if (definition->components & (1 << reagent))
                    if (!ingredients.addReagent((Reagent)reagent)) { reserved = false; break; }
            if (!reserved) { ingredients.revert(); continue; }
            gameSpellMixHowMany(candidate, amount, &ingredients);
            c->stats->setView(STATS_PARTY_OVERVIEW);
            return true;
        }
    }
    c->stats->setView(STATS_PARTY_OVERVIEW);
#else
    spell = AlphaActionController::get('z', "Spell: ");
#endif
    if (spell == -1)
        return false;

    screenMessage("%s!\n", spellGetName(spell)); //Prints spell name at prompt

    c->stats->setView(STATS_PARTY_OVERVIEW);

    // if we can't really cast this spell, skip the extra parameters
    if (spellCheckPrerequisites(spell, player) != CASTERR_NOERROR) {
        attempt(spell, player, 0);
        return true;
    }

    // Get the final parameters for the spell
    switch (spellGetParamType(spell)) {
    case Spell::PARAM_NONE:
        attempt(spell, player, 0);
        break;
    case Spell::PARAM_PHASE: {
        screenMessage("To Phase: ");

#ifdef ZU4_IOS
        std::vector<TopicJournal::Choice> phases;
        for (int i = 0; i < 8; ++i) phases.push_back({std::to_string(i + 1), "Moon phase " + std::to_string(i + 1)});
        phases.push_back(mobileDismiss("cancel", "Cancel"));
        MobileTopicInput phase;
        std::string chosen = phase.read("Choose destination moon phase", phases, 0);
        int choice = chosen.size() == 1 ? chosen[0] : 0;
#else
        int choice = ReadChoiceController::get("12345678 \033\n");
#endif
        if (choice < '1' || choice > '8')
            screenMessage("None\n");
        else {
            screenMessage("\n");
            attempt(spell, player, choice - '1');
        }
        break;
    }
    case Spell::PARAM_PLAYER: {
        screenMessage("Who: ");
        int subject = gameGetPlayer(true, false, (std::string(spellGetName(spell)) + " — choose the recipient").c_str());
        if (subject != -1)
            attempt(spell, player, subject);
        break;
    }
    case Spell::PARAM_DIR:
        if (c->location->context == CTX_DUNGEON)
            attempt(spell, player, c->saveGame->orientation);
        else {
            screenMessage("Dir: ");
            Direction dir = gameGetDirection(("Cast " + std::string(spellGetName(spell)) + " in which direction?").c_str());
            if (dir != DIR_NONE)
                attempt(spell, player, (int) dir);
        }
        break;
    case Spell::PARAM_TYPEDIR: {
        screenMessage("Energy type? ");
        EnergyFieldType fieldType = ENERGYFIELD_NONE;
#ifdef ZU4_IOS
        MobileTopicInput field;
        std::string chosen = field.read("Choose an energy field",
            {{"f", "Fire"}, {"l", "Lightning"}, {"p", "Poison"}, {"s", "Sleep"},
             mobileDismiss("cancel", "Cancel")}, 0);
        if (chosen == "cancel" || chosen == "bye") return false;
        char key = chosen[0];
#else
        char key = ReadChoiceController::get("flps \033\n\r");
#endif
        switch(key) {
        case 'f': fieldType = ENERGYFIELD_FIRE; break;
        case 'l': fieldType = ENERGYFIELD_LIGHTNING; break;
        case 'p': fieldType = ENERGYFIELD_POISON; break;
        case 's': fieldType = ENERGYFIELD_SLEEP; break;
        default: break;
        }

        if (fieldType != ENERGYFIELD_NONE) {
            screenMessage("\n");

            Direction dir;
            if (c->location->context == CTX_DUNGEON)
                dir = (Direction)c->saveGame->orientation;
            else {
                screenMessage("Dir: ");
                dir = gameGetDirection(("Cast " + std::string(spellGetName(spell)) + " in which direction?").c_str());
            }

            if (dir != DIR_NONE) {

                /* Need to pack both dir and fieldType into param */
                int param = fieldType << 4;
                param |= (int) dir;

                attempt(spell, player, param);
            }
        }
        else {
            /* Invalid input here = spell failure */
            screenMessage("Failed!\n");

            /*
             * Confirmed both mixture loss and mp loss in this situation in the
             * original Ultima IV (at least, in the Amiga version.)
             */
            //c->saveGame->mixtures[castSpell]--;
            c->party->member(player)->adjustMp(-spellGetRequiredMP(spell));
            performed = true;
        }
        break;
    }
    case Spell::PARAM_FROMDIR: {
        screenMessage("From Dir: ");
        Direction dir = gameGetDirection((std::string(spellGetName(spell)) + " — from which direction?").c_str());
        if (dir != DIR_NONE)
            attempt(spell, player, (int) dir);
        break;
    }
    }
    return performed;
}

void fire() {
    if (c->transportContext != TRANSPORT_SHIP) {
        screenMessage("%cFire What?%c\n", FG_GREY, FG_WHITE);
        return;
    }

    screenMessage("Fire Cannon!\nDir: ");
    Direction dir = gameGetDirection("Fire the cannon in which direction?");

    if (dir == DIR_NONE)
        return;

    // can only fire broadsides
    int broadsidesDirs = dirGetBroadsidesDirs(c->party->getDirection());
    if (!DIR_IN_MASK(dir, broadsidesDirs)) {
        screenMessage("%cBroadsides Only!%c\n", FG_GREY, FG_WHITE);
        return;
    }

    // nothing (not even mountains!) can block cannonballs
    std::vector<Coords> path = gameGetDirectionalActionPath(MASK_DIR(dir), broadsidesDirs, c->location->coords,
                                                       1, 3, NULL, false);
    for (std::vector<Coords>::iterator i = path.begin(); i != path.end(); i++) {
        if (fireAt(*i, true))
            return;
    }
}

bool fireAt(const Coords &coords, bool originAvatar) {
    bool validObject = false;
    bool hitsAvatar = false;
    bool objectHit = false;

    Object *obj = NULL;

    MapTile tile(c->location->map->tileset->getByName("miss_flash")->getId());
    GameController::flashTile(coords, tile, 1);

    obj = c->location->map->objectAt(coords);
    Creature *m = dynamic_cast<Creature*>(obj);

    if (obj && obj->getType() == Object::CREATURE && m->isAttackable())
        validObject = true;
    /* See if it's an object to be destroyed (the avatar cannot destroy the balloon) */
    else if (obj &&
             (obj->getType() == Object::UNKNOWN) &&
             !(obj->getTile().getTileType()->isBalloon() && originAvatar))
        validObject = true;

    /* Does the cannon hit the avatar? */
    if (zu4_coords_equal(coords, c->location->coords)) {
        validObject = true;
        hitsAvatar = true;
    }

    if (validObject) {
        /* always displays as a 'hit' though the object may not be destroyed */

        /* Is is a pirate ship firing at US? */
        if (hitsAvatar) {
        	GameController::flashTile(coords, "hit_flash", 4);

            if (c->transportContext == TRANSPORT_SHIP)
                gameDamageShip(-1, 10);
            else gameDamageParty(10, 25); /* party gets hurt between 10-25 damage */
        }
        /* inanimate objects get destroyed instantly, while creatures get a chance */
        else if (obj->getType() == Object::UNKNOWN) {
        	GameController::flashTile(coords, "hit_flash", 4);
            c->location->map->removeObject(obj);
        }

        /* only the avatar can hurt other creatures with cannon fire */
        else if (originAvatar) {
        	GameController::flashTile(coords, "hit_flash", 4);
            if (zu4_random(4) == 0) /* reverse-engineered from u4dos */
                c->location->map->removeObject(obj);
        }

        objectHit = true;
    }

    return objectHit;
}

/**
 * Get the chest at the current x,y of the current context for player 'player'
 */
void getChest(int player)
{
    screenMessage("Get Chest!\n");

    if (c->party->isFlying())
    {
        screenMessage("%cDrift only!%c\n", FG_GREY, FG_WHITE);
        return;
    }

    // first check to see if a chest exists at the current location
    // if one exists, prompt the player for the opener, if necessary
    Coords coords;
    c->location->getCurrentPosition(&coords);
    const Tile *tile = c->location->map->tileTypeAt(coords, WITH_GROUND_OBJECTS);

    /* get the object for the chest, if it is indeed an object */
    Object *obj = c->location->map->objectAt(coords);
    if (obj && !obj->getTile().getTileType()->isChest())
        obj = NULL;

    if (tile->isChest() || obj)
    {
        // if a spell was cast to open this chest,
        // player will equal -2, otherwise player
        // will default to -1 or the defult character
        // number if one was earlier specified
        if (player == -1)
        {
            screenMessage("Who opens? ");
            player = gameGetPlayer(false, true);
        }
        if (player == -1)
            return;

        if (obj)
            c->location->map->removeObject(obj);
        else {
            TileId newTile = c->location->getReplacementTile(coords, tile);
            c->location->map->annotations->add(coords, newTile, false , true);
        }

        // see if the chest is trapped and handle it
        getChestTrapHandler(player);

        screenMessage("The Chest Holds: %d Gold\n", c->party->getChest());

        screenPrompt();

        if (isCity(c->location->map) && obj == NULL)
            c->party->adjustKarma(KA_STOLE_CHEST);
    }
    else
    {
        screenMessage("%cNot Here!%c\n", FG_GREY, FG_WHITE);
    }
}

/**
 * Called by getChest() to handle possible traps on chests
 **/
bool getChestTrapHandler(int player) {
    TileEffect trapType;
    int randNum = zu4_random(4);

    /* Do we use u4dos's way of trap-determination, or the original intended way? */
    int passTest = (settings.enhancements && settings.enhancementsOptions.c64chestTraps) ?
        (zu4_random(2) == 0) : /* xu4-enhanced */
        ((randNum & 1) == 0); /* u4dos original way (only allows even numbers through, so only acid and poison show) */

    /* Chest is trapped! 50/50 chance */
    if (passTest)
    {
        /* Figure out which trap the chest has */
        switch(randNum & zu4_random(4)) {
        case 0: trapType = EFFECT_FIRE; break;   /* acid trap (56% chance - 9/16) */
        case 1: trapType = EFFECT_SLEEP; break;  /* sleep trap (19% chance - 3/16) */
        case 2: trapType = EFFECT_POISON; break; /* poison trap (19% chance - 3/16) */
        case 3: trapType = EFFECT_LAVA; break;   /* bomb trap (6% chance - 1/16) */
        default: trapType = EFFECT_FIRE; break;
        }

        /* apply the effects from the trap */
        if (trapType == EFFECT_FIRE)
            screenMessage("%cAcid%c Trap!\n", FG_RED, FG_WHITE);
        else if (trapType == EFFECT_POISON)
            screenMessage("%cPoison%c Trap!\n", FG_GREEN, FG_WHITE);
        else if (trapType == EFFECT_SLEEP)
            screenMessage("%cSleep%c Trap!\n", FG_PURPLE, FG_WHITE);
        else if (trapType == EFFECT_LAVA)
            screenMessage("%cBomb%c Trap!\n", FG_RED, FG_WHITE);

        // player is < 0 during the 'O'pen spell (immune to traps)
        //
        // if the chest was opened by a PC, see if the trap was
        // evaded by testing the PC's dex
        //
        if ((player >= 0) &&
            (c->saveGame->players[player].dex + 25 < zu4_random(100)))
        {
            if (trapType == EFFECT_LAVA) /* bomb trap */
                c->party->applyEffect(trapType);
            else c->party->member(player)->applyEffect(trapType);
        }
        else screenMessage("Evaded!\n");

        return true;
    }

    return false;
}

void holeUp() {
    screenMessage("Hole up & Camp!\n");

	if (!(c->location->context & (CTX_WORLDMAP | CTX_DUNGEON))) {
        screenMessage("%cNot here!%c\n", FG_GREY, FG_WHITE);
        return;
    }

    if (c->transportContext != TRANSPORT_FOOT) {
        screenMessage("%cOnly on foot!%c\n", FG_GREY, FG_WHITE);
        return;
    }

    CombatController *cc = new CampController();
    cc->init(NULL);
    cc->begin();
}

/**
 * Initializes the moon state according to the savegame file. This method of
 * initializing the moons (rather than just setting them directly) is necessary
 * to make sure trammel and felucca stay in sync
 */
void GameController::initMoons()
{
    int trammelphase = c->saveGame->trammelphase,
        feluccaphase = c->saveGame->feluccaphase;

    zu4_assert(c != NULL, "Game context doesn't exist!");
    zu4_assert(c->saveGame != NULL, "Savegame doesn't exist!");
    //zu4_assert(mapIsWorldMap(c->location->map) && c->location->viewMode == VIEW_NORMAL, "Can only call gameInitMoons() from the world map!");

    c->saveGame->trammelphase = c->saveGame->feluccaphase = 0;
    c->moonPhase = 0;

    while ((c->saveGame->trammelphase != trammelphase) ||
           (c->saveGame->feluccaphase != feluccaphase))
        updateMoons(false);
}

/**
 * Updates the phases of the moons and shows
 * the visual moongates on the map, if desired
 */
void GameController::updateMoons(bool showmoongates)
{
    int realMoonPhase,
        oldTrammel,
        trammelSubphase;
    const Coords *gate;

    if (c->location->map->isWorldMap()) {
        oldTrammel = c->saveGame->trammelphase;

        if (++c->moonPhase >= MOON_PHASES * MOON_SECONDS_PER_PHASE * 4)
            c->moonPhase = 0;

        trammelSubphase = c->moonPhase % (MOON_SECONDS_PER_PHASE * 4 * 3);
        realMoonPhase = (c->moonPhase / (4 * MOON_SECONDS_PER_PHASE));

        c->saveGame->trammelphase = realMoonPhase / 3;
        c->saveGame->feluccaphase = realMoonPhase % 8;

        if (c->saveGame->trammelphase > 7)
            c->saveGame->trammelphase = 7;

        if (showmoongates)
        {
            /* update the moongates if trammel changed */
            if (trammelSubphase == 0) {
                gate = moongateGetGateCoordsForPhase(oldTrammel);
                if (gate)
                    c->location->map->annotations->remove(*gate, c->location->map->translateFromRawTileIndex(0x40));
                gate = moongateGetGateCoordsForPhase(c->saveGame->trammelphase);
                if (gate)
                    c->location->map->annotations->add(*gate, c->location->map->translateFromRawTileIndex(0x40));
            }
            else if (trammelSubphase == 1) {
                gate = moongateGetGateCoordsForPhase(c->saveGame->trammelphase);
                if (gate) {
                    c->location->map->annotations->remove(*gate, c->location->map->translateFromRawTileIndex(0x40));
                    c->location->map->annotations->add(*gate, c->location->map->translateFromRawTileIndex(0x41));
                }
            }
            else if (trammelSubphase == 2) {
                gate = moongateGetGateCoordsForPhase(c->saveGame->trammelphase);
                if (gate) {
                    c->location->map->annotations->remove(*gate, c->location->map->translateFromRawTileIndex(0x41));
                    c->location->map->annotations->add(*gate, c->location->map->translateFromRawTileIndex(0x42));
                }
            }
            else if (trammelSubphase == 3) {
                gate = moongateGetGateCoordsForPhase(c->saveGame->trammelphase);
                if (gate) {
                    c->location->map->annotations->remove(*gate, c->location->map->translateFromRawTileIndex(0x42));
                    c->location->map->annotations->add(*gate, c->location->map->translateFromRawTileIndex(0x43));
                }
            }
            else if ((trammelSubphase > 3) && (trammelSubphase < (MOON_SECONDS_PER_PHASE * 4 * 3) - 3)) {
                gate = moongateGetGateCoordsForPhase(c->saveGame->trammelphase);
                if (gate) {
                    c->location->map->annotations->remove(*gate, c->location->map->translateFromRawTileIndex(0x43));
                    c->location->map->annotations->add(*gate, c->location->map->translateFromRawTileIndex(0x43));
                }
            }
            else if (trammelSubphase == (MOON_SECONDS_PER_PHASE * 4 * 3) - 3) {
                gate = moongateGetGateCoordsForPhase(c->saveGame->trammelphase);
                if (gate) {
                    c->location->map->annotations->remove(*gate, c->location->map->translateFromRawTileIndex(0x43));
                    c->location->map->annotations->add(*gate, c->location->map->translateFromRawTileIndex(0x42));
                }
            }
            else if (trammelSubphase == (MOON_SECONDS_PER_PHASE * 4 * 3) - 2) {
                gate = moongateGetGateCoordsForPhase(c->saveGame->trammelphase);
                if (gate) {
                    c->location->map->annotations->remove(*gate, c->location->map->translateFromRawTileIndex(0x42));
                    c->location->map->annotations->add(*gate, c->location->map->translateFromRawTileIndex(0x41));
                }
            }
            else if (trammelSubphase == (MOON_SECONDS_PER_PHASE * 4 * 3) - 1) {
                gate = moongateGetGateCoordsForPhase(c->saveGame->trammelphase);
                if (gate) {
                    c->location->map->annotations->remove(*gate, c->location->map->translateFromRawTileIndex(0x41));
                    c->location->map->annotations->add(*gate, c->location->map->translateFromRawTileIndex(0x40));
                }
            }
        }
    }
}

/**
 * Handles feedback after avatar moved during normal 3rd-person view.
 */
void GameController::avatarMoved(MoveEvent &event) {
    if (event.userEvent) {

        // is filterMoveMessages even used?  it doesn't look like the option is hooked up in the configuration menu
        if (!settings.filterMoveMessages) {
            switch (c->transportContext) {
                case TRANSPORT_FOOT:
                case TRANSPORT_HORSE:
                    screenMessage("%s\n", getDirectionName(event.dir));
                    break;
                case TRANSPORT_SHIP:
                    if (event.result & MOVE_TURNED)
                        screenMessage("Turn %s!\n", getDirectionName(event.dir));
                    else if (event.result & MOVE_SLOWED)
                        screenMessage("%cSlow progress!%c\n", FG_GREY, FG_WHITE);
                    else
                        screenMessage("Sail %s!\n", getDirectionName(event.dir));
                    break;
                case TRANSPORT_BALLOON:
                    screenMessage("%cDrift Only!%c\n", FG_GREY, FG_WHITE);
                    break;
                default:
                    zu4_assert(0, "bad transportContext %d in avatarMoved()", c->transportContext);
            }
        }

        /* movement was blocked */
        if (event.result & MOVE_BLOCKED) {

#if defined(ZU4_IOS) || defined(ZU4_WEB)
            if (mobilePerformAdjacent(event.dir, false))
                event.result = (MoveResult)(event.result | MOVE_INTERACTED);
#endif

            /* if shortcuts are enabled, try them! */
            if (!(event.result & MOVE_INTERACTED) && settings.shortcutCommands) {
                Coords new_coords = c->location->coords;
                MapTile *tile;

                movedir(&new_coords, event.dir, c->location->map);
                tile = c->location->map->tileAt(new_coords, WITH_OBJECTS);

                if (tile->getTileType()->isDoor()) {
                    openAt(new_coords);
                    event.result = (MoveResult)(MOVE_SUCCEEDED | MOVE_END_TURN);
                } else if (tile->getTileType()->isLockedDoor()) {
                    jimmyAt(new_coords);
                    event.result = (MoveResult)(MOVE_SUCCEEDED | MOVE_END_TURN);
                } /*else if (mapPersonAt(c->location->map, new_coords) != NULL) {
                    talkAtCoord(newx, newy, 1, NULL);
                    event.result = MOVE_SUCCEEDED | MOVE_END_TURN;
                    }*/
            }

            /* if we're still blocked */
            if ((event.result & MOVE_BLOCKED) && !(event.result & MOVE_INTERACTED) && !settings.filterMoveMessages) {
                zu4_snd_play(SOUND_BLOCKED, false, -1);
                screenMessage("%cBlocked!%c\n", FG_GREY, FG_WHITE);
            }
        }
        else if (c->transportContext == TRANSPORT_FOOT || c->transportContext == TRANSPORT_HORSE) {
            /* movement was slowed */
            if (event.result & MOVE_SLOWED) {
                zu4_snd_play(SOUND_WALK_SLOWED, true, -1);
                screenMessage("%cSlow progress!%c\n", FG_GREY, FG_WHITE);
            }
            else if (c->horseSpeed == 1) {
                switch(zu4_random(4)) {
                    case 1: zu4_snd_play(SOUND_HORSE_NORMAL_1, true, -1); break;
                    case 2: zu4_snd_play(SOUND_HORSE_NORMAL_2, true, -1); break;
                    case 3: zu4_snd_play(SOUND_HORSE_NORMAL_3, true, -1); break;
                    default: zu4_snd_play(SOUND_HORSE_NORMAL_4, true, -1); break;
				}
            }
            else {
                switch(zu4_random(4)) {
                    case 1: zu4_snd_play(SOUND_WALK_NORMAL_1, true, -1); break;
                    case 2: zu4_snd_play(SOUND_WALK_NORMAL_2, true, -1); break;
                    case 3: zu4_snd_play(SOUND_WALK_NORMAL_3, true, -1); break;
                    default: zu4_snd_play(SOUND_WALK_NORMAL_4, true, -1); break;
				}
            }
        }
    }

    /* exited map */
    if (event.result & MOVE_EXIT_TO_PARENT) {
        screenMessage("%cLeaving...%c\n", FG_GREY, FG_WHITE);
        exitToParentMap();
        zu4_music_play(c->location->map->music);
    }

    /* things that happen while not on board the balloon */
    if (c->transportContext & ~TRANSPORT_BALLOON)
        checkSpecialCreatures(event.dir);
    /* things that happen while on foot or horseback */
    if ((c->transportContext & TRANSPORT_FOOT_OR_HORSE) &&
        !(event.result & (MOVE_SLOWED|MOVE_BLOCKED))) {
        if (checkMoongates())
            event.result = (MoveResult)(MOVE_MAP_CHANGE | MOVE_END_TURN);
    }
}

/**
 * Handles feedback after moving the avatar in the 3-d dungeon view.
 */
void GameController::avatarMovedInDungeon(MoveEvent &event) {
    Dungeon *dungeon = dynamic_cast<Dungeon *>(c->location->map);
    Direction realDir = dirNormalize((Direction)c->saveGame->orientation, event.dir);

    if (!settings.filterMoveMessages) {
        if (event.userEvent) {
            if (event.result & MOVE_TURNED) {
                if (dirRotateCCW((Direction)c->saveGame->orientation) == realDir)
                    screenMessage("Turn Left\n");
                else screenMessage("Turn Right\n");
            }
            /* show 'Advance' or 'Retreat' in dungeons */
            else screenMessage("%s\n", realDir == c->saveGame->orientation ? "Advance" : "Retreat");
        }

        if (event.result & MOVE_BLOCKED)
            screenMessage("%cBlocked!%c\n", FG_GREY, FG_WHITE);
    }

    /* if we're exiting the map, do this */
    if (event.result & MOVE_EXIT_TO_PARENT) {
        screenMessage("%cLeaving...%c\n", FG_GREY, FG_WHITE);
        exitToParentMap();
        zu4_music_play(c->location->map->music);
    }

    /* check to see if we're entering a dungeon room */
    if (event.result & MOVE_SUCCEEDED) {
        if (dungeon->currentToken() == DUNGEON_ROOM) {
            int room = (int)dungeon->currentSubToken(); /* get room number */

            /**
             * recalculate room for the abyss -- there are 16 rooms for every 2 levels,
             * each room marked with 0xD* where (* == room number 0-15).
             * for levels 1 and 2, there are 16 rooms, levels 3 and 4 there are 16 rooms, etc.
             */
            if (c->location->map->id == MAP_ABYSS)
                room = (0x10 * (c->location->coords.z/2)) + room;

            Dungeon *dng = dynamic_cast<Dungeon*>(c->location->map);
            dng->currentRoom = room;

            /* set the map and start combat! */
            CombatController *cc = new CombatController(dng->roomMaps[room]);
            cc->initDungeonRoom(room, dirReverse(realDir));
            cc->begin();
        }
    }
}

void jimmy() {
    screenMessage("Jimmy: ");
    Direction dir = gameGetDirection("Unlock a door in which direction?");

    if (dir == DIR_NONE)
        return;

    std::vector<Coords> path = gameGetDirectionalActionPath(MASK_DIR(dir), MASK_DIR_ALL, c->location->coords,
                                                                       1, 1, NULL, true);
    for (std::vector<Coords>::iterator i = path.begin(); i != path.end(); i++) {
        if (jimmyAt(*i))
            return;
    }

    screenMessage("%cJimmy what?%c\n", FG_GREY, FG_WHITE);
}

/**
 * Attempts to jimmy a locked door at map coordinates x,y.  The locked
 * door is replaced by a permanent annotation of an unlocked door
 * tile.
 */
bool jimmyAt(const Coords &coords) {
    MapTile *tile = c->location->map->tileAt(coords, WITH_OBJECTS);

    if (!tile->getTileType()->isLockedDoor())
        return false;

    if (c->saveGame->keys) {
        Tile *door = c->location->map->tileset->getByName("door");
        zu4_assert(door, "no door tile found in tileset");
        c->saveGame->keys--;
        c->location->map->annotations->add(coords, door->getId());
        screenMessage("\nUnlocked!\n");
    } else
        screenMessage("%cNo keys left!%c\n", FG_GREY, FG_WHITE);

    return true;
}

void opendoor() {
    ///  XXX: Pressing "o" should close any open door.

	screenMessage("Open: ");

	if (c->party->isFlying()) {
        screenMessage("%cNot Here!%c\n", FG_GREY, FG_WHITE);
        return;
    }

    Direction dir = gameGetDirection("Open a door in which direction?");

    if (dir == DIR_NONE)
        return;

    std::vector<Coords> path = gameGetDirectionalActionPath(MASK_DIR(dir), MASK_DIR_ALL, c->location->coords,
                                                       1, 1, NULL, true);
    for (std::vector<Coords>::iterator i = path.begin(); i != path.end(); i++) {
        if (openAt(*i))
            return;
    }

    screenMessage("%cNot Here!%c\n", FG_GREY, FG_WHITE);
}

/**
 * Attempts to open a door at map coordinates x,y.  The door is
 * replaced by a temporary annotation of a floor tile for 4 turns.
 */
bool openAt(const Coords &coords) {
    const Tile *tile = c->location->map->tileTypeAt(coords, WITH_OBJECTS);

    if (!tile->isDoor() &&
        !tile->isLockedDoor())
        return false;

    if (tile->isLockedDoor()) {
        screenMessage("%cCan't!%c\n", FG_GREY, FG_WHITE);
        return true;
    }

    Tile *floor = c->location->map->tileset->getByName("brick_floor");
    zu4_assert(floor, "no floor tile found in tileset");
    c->location->map->annotations->add(coords, floor->getId(), false, true)->setTTL(4);

    screenMessage("\nOpened!\n");

    return true;
}

/**
 * Readies a weapon for a player.  Prompts for the player and/or the
 * weapon if not provided.
 */
void readyWeapon(int player) {

    // get the player if not provided
    if (player == -1) {
        screenMessage("Ready a weapon for: ");
        player = gameGetPlayer(true, false);
        if (player == -1)
            return;
    }

    // get the weapon to use
    c->stats->setView(STATS_WEAPONS);
    screenMessage("Weapon: ");
    WeaponType weapon = (WeaponType) AlphaActionController::get(WEAP_MAX + 'a' - 1, "Weapon: ");
    c->stats->setView(STATS_PARTY_OVERVIEW);

    PartyMember *p = c->party->member(player);
    //const Weapon *w = Weapon::get(weapon);

    /*if (!w) {
        screenMessage("\n");
        return;
    }*/
    switch (p->setWeapon(weapon)) {
    case EQUIP_SUCCEEDED:
        screenMessage("%s\n", zu4_weapon_name(weapon));
        break;
    case EQUIP_NONE_LEFT:
        screenMessage("%cNone left!%c\n", FG_GREY, FG_WHITE);
        break;
    case EQUIP_CLASS_RESTRICTED: {
        std::string indef_article;

        switch(tolower(zu4_weapon_name(weapon)[0])) {
        case 'a': case 'e': case 'i':
        case 'o': case 'u': case 'y':
            indef_article = "an"; break;
        default:
            indef_article = "a"; break;
        }

        screenMessage("\n%cA %s may NOT use %s %s%c\n", FG_GREY, getClassName(p->getClass()),
                      indef_article.c_str(), zu4_weapon_name(weapon), FG_WHITE);
        break;
    }
    }
}

void talk() {
	screenMessage("Talk: ");

    if (c->party->isFlying()) {
        screenMessage("%cDrift only!%c\n", FG_GREY, FG_WHITE);
        return;
    }

    Direction dir = gameGetDirection("Talk to someone in which direction?");

    if (dir == DIR_NONE)
        return;

    std::vector<Coords> path = gameGetDirectionalActionPath(MASK_DIR(dir), MASK_DIR_ALL, c->location->coords,
                                                                       1, 2, &Tile::canTalkOverTile, true);
    for (std::vector<Coords>::iterator i = path.begin(); i != path.end(); i++) {
        if (talkAt(*i))
            return;
    }

    screenMessage("Funny, no response!\n");
}

/**
 * Mixes reagents.  Prompts for a spell, then which reagents to
 * include in the mix.
 */
void mixReagents() {

    /*  uncomment this line to activate new spell mixing code */
    //   return mixReagentsSuper();
    bool done = false;

    while (!done) {
        screenMessage("Mix reagents\n");

        // Verify that there are reagents remaining in the inventory
        bool found = false;
        for (int i=0; i < 8; i++)
        {
            if (c->saveGame->reagents[i] > 0)
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            screenMessage("%cNone Left!%c", FG_GREY, FG_WHITE);
            done = true;
        }
        else
        {
            screenMessage("For Spell: ");
            c->stats->setView(STATS_MIXTURES);

            int choice = ReadChoiceController::get("abcdefghijklmnopqrstuvwxyz \033\n\r");
            if (choice == ' ' || choice == '\033' || choice == '\n' || choice == '\r')
                break;

            int spell = choice - 'a';
            screenMessage("%s\n", spellGetName(spell));

            // ensure the mixtures for the spell isn't already maxed out
            if (c->saveGame->mixtures[spell] == 99) {
                screenMessage("\n%cYou cannot mix any more of that spell!%c\n", FG_GREY, FG_WHITE);
                break;
            }

            // Reset the reagent spell mix menu by removing
            // the menu highlight from the current item, and
            // hiding reagents that you don't have
            c->stats->resetReagentsMenu();

            c->stats->setView(MIX_REAGENTS);
            if (settings.enhancements && settings.enhancementsOptions.u5spellMixing)
                done = mixReagentsForSpellU5(spell);
            else
                done = mixReagentsForSpellU4(spell);
        }
    }

    c->stats->setView(STATS_PARTY_OVERVIEW);
    screenMessage("\n\n");
}

/**
 * Prompts for spell reagents to mix in the traditional Ultima IV
 * style.
 */
bool mixReagentsForSpellU4(int spell) {
    Ingredients ingredients;

    screenMessage("Reagent: ");

    while (1) {
        int choice = ReadChoiceController::get("abcdefgh\n\r \033");

        // done selecting reagents? mix it up and prompt to mix
        // another spell
        if (choice == '\n' || choice == '\r' || choice == ' ') {
            screenMessage("\n\nYou mix the Reagents, and...\n");

            if (spellMix(spell, &ingredients))
                screenMessage("Success!\n\n");
            else
                screenMessage("It Fizzles!\n\n");

            return false;
        }

        // escape: put ingredients back and quit mixing
        if (choice == '\033') {
            ingredients.revert();
            return true;
        }

        screenMessage("%c\n", toupper(choice));
        if (!ingredients.addReagent((Reagent)(choice - 'a')))
            screenMessage("%cNone Left!%c\n", FG_GREY, FG_WHITE);
        screenMessage("Reagent: ");
    }

    return true;
}

/**
 * Prompts for spell reagents to mix with an Ultima V-like menu.
 */
bool mixReagentsForSpellU5(int spell) {
    Ingredients ingredients;

    screenDisableCursor();

    c->stats->getReagentsMenu()->reset(); // reset the menu, highlighting the first item
    ReagentsMenuController getReagentsController(c->stats->getReagentsMenu(), &ingredients, c->stats->getMainArea());
    eventHandler->pushController(&getReagentsController);
    getReagentsController.waitFor();

    c->stats->getMainArea()->disableCursor();
    screenEnableCursor();

    screenMessage("How many? ");

    int howmany = ReadIntController::get(2, TEXT_AREA_X + c->col, TEXT_AREA_Y + c->line);
    gameSpellMixHowMany(spell, howmany, &ingredients);

    return true;
}

/**
 * Exchanges the position of two players in the party.  Prompts the
 * user for the player numbers.
 */
void newOrder() {
    screenMessage("New Order!\nExchange # ");

    int player1 = gameGetPlayer(true, false);

    if (player1 == -1)
        return;

    if (player1 == 0) {
        screenMessage("%s, You must lead!\n", c->party->member(0)->getName().c_str());
        return;
    }

    screenMessage("    with # ");

    int player2 = gameGetPlayer(true, false);

    if (player2 == -1)
        return;

    if (player2 == 0) {
        screenMessage("%s, You must lead!\n", c->party->member(0)->getName().c_str());
        return;
    }

    if (player1 == player2) {
        screenMessage("%cWhat?%c\n", FG_GREY, FG_WHITE);
        return;
    }

    c->party->swapPlayers(player1, player2);
}

/**
 * Peers at a city from A-P (Lycaeum telescope) and functions like a gem
 */
bool gamePeerCity(int city, void *data) {
    Map *peerMap;

    peerMap = mapMgr->get((MapId)(city+1));

    if (peerMap != NULL) {
        game->setMap(peerMap, 1, NULL);
        game->paused = true;
        game->pausedTimer = 0;

        screenDisableCursor();

#ifdef ZU4_IOS
        mobileShowGemMap();
#else
        c->location->viewMode = VIEW_GEM;
        ReadChoiceController::get("\015 \033");
#endif

        game->exitToParentMap();
        screenEnableCursor();
        game->paused = false;

        return true;
    }
    return false;
}

/**
 * Peers at a gem
 */
void peer(bool useGem) {
#if defined(ZU4_IOS) || defined(ZU4_WEB)
    int previousViewMode = c->location->viewMode;
#endif

    if (useGem) {
        if (c->saveGame->gems <= 0) {
            screenMessage("%cPeer at What?%c\n", FG_GREY, FG_WHITE);
            return;
        }

        c->saveGame->gems--;
        screenMessage("Peer at a Gem!\n");
    }

    game->paused = true;
    game->pausedTimer = 0;
    screenDisableCursor();

#ifdef ZU4_IOS
    mobileShowGemMap();
#elif defined(ZU4_WEB)
    extern void webShowGemMap();
    webShowGemMap();
#else
    c->location->viewMode = VIEW_GEM;

    ReadChoiceController::get("\015 \033");
#endif

    screenEnableCursor();
#if defined(ZU4_IOS) || defined(ZU4_WEB)
    c->location->viewMode = previousViewMode;
#else
    c->location->viewMode = VIEW_NORMAL;
#endif
    game->paused = false;
}

/**
 * Begins a conversation with the NPC at map coordinates x,y.  If no
 * NPC is present at that point, zero is returned.
 */
bool talkAt(const Coords &coords) {
    extern int personIsVendor(const Person *person);
    City *city;

    /* can't have any conversations outside of town */
    if (!isCity(c->location->map)) {
        screenMessage("Funny, no response!\n");
        return true;
    }

    city = dynamic_cast<City*>(c->location->map);
    Person *talker = city->personAt(coords);

    /* make sure we have someone we can talk with */
    if (!talker || !talker->canConverse())
        return false;

    /* No response from alerted guards... does any monster both
       attack and talk besides Nate the Snake? */
    if  (talker->getMovementBehavior() == MOVEMENT_ATTACK_AVATAR &&
         talker->getId() != PYTHON_ID)
        return false;

    /* if we're talking to Lord British and the avatar is dead, LB resurrects them! */
#ifdef ZU4_WEB
    WebInteractionScope webInteraction;
#endif
    if (talker->getNpcType() == NPC_LORD_BRITISH &&
        c->party->member(0)->getStatus() == STAT_DEAD) {
        screenMessage("%s, Thou shalt live again!\n", c->party->member(0)->getName().c_str());

        c->party->member(0)->setStatus(STAT_GOOD);
        c->party->member(0)->heal(HT_FULLHEAL);
        gameSpellEffect('r', -1, SOUND_LBHEAL);
    }

    Conversation conv;
    zu4_error(ZU4_LOG_DBG, "Setting up script information providers.");
    conv.script->addProvider("party", c->party);

    conv.state = Conversation::INTRO;
    conv.reply = talker->getConversationText(&conv, "");
    conv.playerInput.erase();
    talkRunConversation(conv, talker, false);

    return true;
}

/**
 * Executes the current conversation until it is done.
 */
void talkRunConversation(Conversation &conv, Person *talker, bool showPrompt) {
#ifdef ZU4_IOS
    std::string mobileTranscript;
    std::string mobileTopic = "Introduction";
#endif
#ifdef ZU4_WEB
    std::string webTranscript;
    std::string webTopic = "Introduction";
#endif
    while (conv.state != Conversation::DONE) {
        // TODO: instead of calculating linesused again, cache the
        // result in person.cpp somewhere.
        int linesused = linecount(conv.reply.front(), TEXT_AREA_W);
        screenMessage("%s", conv.reply.front().c_str());
#ifdef ZU4_WEB
        webTranscript += conv.reply.front();
        webJournalDialogue(conv.reply.front(),talker,webTopic);
#endif
#ifdef ZU4_IOS
        observeMobileDialogue(conv.reply.front(), talker, mobileTopic);
        for (unsigned char ch : conv.reply.front())
            if (ch >= 32 || ch == '\n') mobileTranscript += ch;

#endif
        conv.reply.pop_front();

        /* if all chunks haven't been shown, wait for a key and process next chunk*/
        int size = conv.reply.size();
        if (size > 0) {
#ifdef ZU4_IOS
            MobileTopicInput input;
            input.read(mobileTranscript, {{"continue", "Continue"}}, 0);
            mobileTranscript.clear();
#else
#ifdef ZU4_WEB
            webReadPrompt("choice", "Conversation", webTranscript, {{"\r", "Continue"}});
            webTranscript.clear();
#else
            ReadChoiceController::get("");
#endif
#endif
            continue;
        }

        /* otherwise, clear current reply and proceed based on conversation state */
        conv.reply.clear();

        /* they're attacking you! */
        if (conv.state == Conversation::ATTACK) {
            conv.state = Conversation::DONE;
            talker->setMovementBehavior(MOVEMENT_ATTACK_AVATAR);
        }

        if (conv.state == Conversation::DONE)
            break;

        /* When Lord British heals the party */
        else if (conv.state == Conversation::FULLHEAL) {
            int i;

            for (i = 0; i < c->party->size(); i++) {
                c->party->member(i)->heal(HT_CURE);        // cure the party
                c->party->member(i)->heal(HT_FULLHEAL);    // heal the party
            }
            gameSpellEffect('r', -1, SOUND_MAGIC); // same spell effect as 'r'esurrect

            conv.state = Conversation::TALK;
        }
        /* When Lord British checks and advances each party member's level */
        else if (conv.state == Conversation::ADVANCELEVELS) {
            gameLordBritishCheckLevels();
            conv.state = Conversation::TALK;
        }

        if (showPrompt) {
            std::string prompt = talker->getPrompt(&conv);
            if (!prompt.empty()) {
#if !defined(ZU4_IOS) && !defined(ZU4_WEB)
                if (linesused + linecount(prompt, TEXT_AREA_W) > TEXT_AREA_H) {
                    ReadChoiceController::get("");
                }
#elif defined(ZU4_IOS)
                mobileTranscript += "\n" + prompt;
#else
                webTranscript += "\n" + prompt;
#endif
                screenMessage("%s", prompt.c_str());
            }
        }

        int maxlen;
        switch (conv.getInputRequired(&maxlen)) {
        case Conversation::INPUT_STRING: {
#ifdef ZU4_IOS
            if (conv.state == Conversation::TALK && talker->getDialogue()) {
                auto choices = mobileTopics.choices(talker->getDialogue()->getKeywords());
                conv.playerInput = readMobileConversationTopic(mobileTranscript, choices, maxlen);
                mobileTranscript.clear();
            } else if (conv.state == Conversation::GIVEBEGGAR) {
                int amount = gameGetAmountInput(maxlen, mobileTranscript + "\n\nGold carried: " + std::to_string(c->saveGame->gold));
                conv.playerInput = std::to_string(amount < 0 ? 0 : amount);
                mobileTranscript.clear();
            } else if (conv.state == Conversation::ASK || conv.state == Conversation::ASKYESNO) {
                MobileTopicInput input;
                conv.playerInput = input.read(mobileTranscript, {{"yes", "Yes"}, {"no", "No"}}, 0);
                mobileTranscript.clear();
            } else
#endif
#ifdef ZU4_WEB
            if (conv.state == Conversation::GIVEBEGGAR) {
                const int amount = gameGetAmountInput(maxlen, webTranscript);
                // Cancellation returns to topics without making a donation.
                if (amount < 0) {
                    conv.state = Conversation::TALK;
                    conv.reply.push_back("Never mind.\n");
                    webTranscript.clear();
                    continue;
                }
                conv.playerInput = std::to_string(amount);
            } else if (conv.state == Conversation::ASK || conv.state == Conversation::ASKYESNO) {
                conv.playerInput = webReadPrompt("confirmation", "Answer thy companion", webTranscript,
                    {{"yes", "Yes"}, {"no", "No"}});
            } else {
                std::vector<WebPromptOption> choices = {{"name", "Name"}, {"job", "Job"}, {"health", "Health"}};
                if (talker->getNpcType() == NPC_TALKER_BEGGAR) choices.push_back({"give", "Give gold"});
                Virtue companionVirtue;
                if (c->party->canPersonJoin(talker->getName(), &companionVirtue)) choices.push_back({"join", "Join party"});
                if (talker->getNpcType() == NPC_HAWKWIND) {
                    choices.clear();
                    for (int i = 0; i < 8; ++i) {
                        std::string virtue = getVirtueName(static_cast<Virtue>(i));
                        choices.push_back({virtue, virtue});
                    }
                }
                choices.push_back({"bye", "Goodbye"});
                conv.playerInput = webReadPrompt("text", "Conversation", webTranscript, choices, maxlen, true);
            }
            webTranscript.clear();
#else
            conv.playerInput = gameGetInput(maxlen);
#endif
#ifdef ZU4_WEB
            webTopic = conv.playerInput;
#endif
#ifdef ZU4_IOS
            mobileTopic = conv.playerInput;
#endif
            conv.reply = talker->getConversationText(&conv, conv.playerInput.c_str());
            conv.playerInput.erase();
            showPrompt = true;
            break;
        }
        case Conversation::INPUT_CHARACTER: {
            char message[2];
#ifdef ZU4_IOS
            int choice;
            if (conv.state == Conversation::CONFIRMATION) {
                choice = gameGetVendorChoice("yn", mobileTranscript, false, {}, false);
                mobileTranscript.clear();
            } else choice = ReadChoiceController::get("");
#else
#ifdef ZU4_WEB
            std::string answer = webReadPrompt("confirmation", "Answer Lord British", webTranscript,
                {{"y", "Yes"}, {"n", "No"}});
            int choice = answer.empty() ? 'n' : answer[0];
            webTranscript.clear();
#else
            int choice = ReadChoiceController::get("");
#endif
#endif

            message[0] = choice;
            message[1] = '\0';
#ifdef ZU4_WEB
            webTopic = message;
#endif

#ifdef ZU4_IOS
            mobileTopic = message;
#endif
            conv.reply = talker->getConversationText(&conv, message);
            conv.playerInput.erase();

            showPrompt = true;
            break;
        }

        case Conversation::INPUT_NONE:
            conv.state = Conversation::DONE;
            break;
        }
    }
    if (conv.reply.size() > 0) {
        screenMessage("%s", conv.reply.front().c_str());
#ifdef ZU4_WEB
        webJournalDialogue(conv.reply.front(),talker,webTopic);
#endif
#ifdef ZU4_IOS
        observeMobileDialogue(conv.reply.front(), talker, mobileTopic);
        for (unsigned char ch : conv.reply.front())
            if (ch >= 32 || ch == '\n') mobileTranscript += ch;
#endif
    }
#ifdef ZU4_IOS
    if (!mobileTranscript.empty()) {
        MobileTopicInput input;
        input.read(mobileTranscript, {{"continue", "Continue"}}, 0);
    }
#endif
}

/**
 * Changes a player's armor.  Prompts for the player and/or the armor
 * type if not provided.
 */
void wearArmor(int player) {

    // get the player if not provided
    if (player == -1) {
        screenMessage("Wear Armour\nfor: ");
        player = gameGetPlayer(true, false);
        if (player == -1)
            return;
    }

    c->stats->setView(STATS_ARMOR);
    screenMessage("Armour: ");
    ArmorType armor = (ArmorType) AlphaActionController::get(ARMR_MAX + 'a' - 1, "Armour: ");
    c->stats->setView(STATS_PARTY_OVERVIEW);

    //const Armor *a = Armor::get(armor);
    PartyMember *p = c->party->member(player);

    /*if (!a) {
        screenMessage("\n");
        return;
    }*/
    switch (p->setArmor(armor)) {
    case EQUIP_SUCCEEDED:
        screenMessage("%s\n", zu4_armor_name(armor));
        break;
    case EQUIP_NONE_LEFT:
        screenMessage("%cNone left!%c\n", FG_GREY, FG_WHITE);
        break;
    case EQUIP_CLASS_RESTRICTED:
        screenMessage("\n%cA %s may NOT use %s%c\n", FG_GREY, getClassName(p->getClass()), zu4_armor_name(armor), FG_WHITE);
        break;
    }
}

/**
 * Called when the player selects a party member for ztats
 */
void ztatsFor(int player) {
    // get the player if not provided
    if (player == -1) {
        screenMessage("Ztats for: ");
        player = gameGetPlayer(true, false);
        if (player == -1)
            return;
    }

    // Reset the reagent spell mix menu by removing
    // the menu highlight from the current item, and
    // hiding reagents that you don't have
    c->stats->resetReagentsMenu();

    c->stats->setView(StatsView(STATS_CHAR1 + player));

    ZtatsController ctrl;
    eventHandler->pushController(&ctrl);
    ctrl.waitFor();
}

/**
 * This function is called every quarter second.
 */
void GameController::timerFired() {
#ifdef ZU4_WEB
    // Choosing a service or typing a mandatory answer must not trigger the
    // idle turn underneath the modal (especially on airborne transport).
    if (webInteractionDepth() || dynamic_cast<WebPromptController *>(eventHandler->getController())) {
        screenCycle();
        if (eventHandler->timerQueueEmpty()) gameUpdateScreen();
        return;
    }
#endif
#ifdef ZU4_IOS
    // Controllers beneath a modal retain their timer registrations. Freeze
    // simulation here while UIKit owns input, including airborne transport.
    if (mobilePanelDepth || zu4_journal_panel_is_visible()) {
        screenCycle();
        if (eventHandler->timerQueueEmpty()) gameUpdateScreen();
        return;
    }
#endif

    if (pausedTimer > 0) {
        pausedTimer--;
        if (pausedTimer <= 0) {
            pausedTimer = 0;
            paused = false; /* unpause the game */
        }
    }

    if (!paused && !pausedTimer) {
        if (++c->windCounter >= MOON_SECONDS_PER_PHASE * 4) {
            if (zu4_random(4) == 1 && !c->windLock)
                c->windDirection = dirRandomDir(MASK_DIR_ALL);
            c->windCounter = 0;
        }

        /* balloon moves about 4 times per second */
        if ((c->transportContext == TRANSPORT_BALLOON) &&
            c->party->isFlying()) {
            c->location->move(dirReverse((Direction) c->windDirection), false);
        }

        updateMoons(true);

        screenCycle();

        /*
         * refresh the screen only if the timer queue is empty --
         * i.e. drop a frame if another timer event is about to be fired
         */
        if (eventHandler->timerQueueEmpty())
            gameUpdateScreen();

#ifndef ZU4_IOS
        /* Desktop idle pass is intentionally disabled on touch: reading the
         * battlefield must not silently spend a character's turn. Mobile
         * players advance turns explicitly through actions or Wait. */
        /*
         * force pass if no commands within last 20 seconds
         */
        Controller *controller = eventHandler->getController();
        if (controller != NULL && (eventHandler->getController() == game || dynamic_cast<CombatController *>(eventHandler->getController()) != NULL) &&
             gameTimeSinceLastCommand() > 20) {

            /* pass the turn, and redraw the text area so the prompt is shown */
            controller->keyPressed(U4_SPACE);
        }
#endif
    }

}

/**
 * Checks the hull integrity of the ship and handles
 * the ship sinking, if necessary
 */
void gameCheckHullIntegrity() {
    int i;

    bool killAll = false;
    /* see if the ship has sunk */
    if ((c->transportContext == TRANSPORT_SHIP) && c->saveGame->shiphull <= 0)
    {
        screenMessage("\nThy ship sinks!\n\n");
        killAll = true;
    }

    if (!collisionOverride && c->transportContext == TRANSPORT_FOOT &&
    	c->location->map->tileTypeAt(c->location->coords, WITHOUT_OBJECTS)->isSailable() &&
    	!c->location->map->tileTypeAt(c->location->coords, WITH_GROUND_OBJECTS)->isShip() &&
    	!c->location->map->getValidMoves(c->location->coords, c->party->getTransport()))
    {
        screenMessage("\nTrapped at sea without thy ship, thou dost drown!\n\n");
        killAll = true;
    }

    if (killAll)
    {
        for (i = 0; i < c->party->size(); i++)
        {
            c->party->member(i)->setHp(0);
            c->party->member(i)->setStatus(STAT_DEAD);
        }

        deathStart(5);
    }
}

/**
 * Checks for valid conditions and handles
 * special creatures guarding the entrance to the
 * abyss and to the shrine of spirituality
 */
void GameController::checkSpecialCreatures(Direction dir) {
    int i;
    Object *obj;
    static const struct {
        int x, y;
        Direction dir;
    } pirateInfo[] = {
        { 224, 220, DIR_EAST }, /* N'M" O'A" */
        { 224, 228, DIR_EAST }, /* O'E" O'A" */
        { 226, 220, DIR_EAST }, /* O'E" O'C" */
        { 227, 228, DIR_EAST }, /* O'E" O'D" */
        { 228, 227, DIR_SOUTH }, /* O'D" O'E" */
        { 229, 225, DIR_SOUTH }, /* O'B" O'F" */
        { 229, 223, DIR_NORTH }, /* N'P" O'F" */
        { 228, 222, DIR_NORTH } /* N'O" O'E" */
    };

    /*
     * if heading east into pirates cove (O'A" N'N"), generate pirate
     * ships
     */
    if (dir == DIR_EAST &&
        c->location->coords.x == 0xdd &&
        c->location->coords.y == 0xe0) {
        for (i = 0; i < 8; i++) {
            obj = c->location->map->addCreature(creatureMgr->getById(PIRATE_ID), (Coords){pirateInfo[i].x, pirateInfo[i].y, 0});
            obj->setDirection(pirateInfo[i].dir);
        }
    }

    /*
     * if heading south towards the shrine of humility, generate
     * daemons unless horn has been blown
     */
    if (dir == DIR_SOUTH &&
        c->location->coords.x >= 229 &&
        c->location->coords.x < 234 &&
        c->location->coords.y >= 212 &&
        c->location->coords.y < 217 &&
        c->aura->type != AURA_HORN) {
        for (i = 0; i < 8; i++)
            c->location->map->addCreature(creatureMgr->getById(DAEMON_ID), (Coords){231, c->location->coords.y + 1, c->location->coords.z});
    }
}

/**
 * Checks for and handles when the avatar steps on a moongate
 */
bool GameController::checkMoongates() {
    Coords dest;

    if (moongateFindActiveGateAt(c->saveGame->trammelphase, c->saveGame->feluccaphase, c->location->coords, &dest)) {

        gameSpellEffect(-1, -1, SOUND_MOONGATE); // Default spell effect (screen inversion without 'spell' sound effects)

        if (!zu4_coords_equal(c->location->coords, dest)) {
            c->location->coords = dest;
            gameSpellEffect(-1, -1, SOUND_MOONGATE); // Again, after arriving
        }

        if (moongateIsEntryToShrineOfSpirituality(c->saveGame->trammelphase, c->saveGame->feluccaphase)) {
            Shrine *shrine_spirituality;

            shrine_spirituality = dynamic_cast<Shrine*>(mapMgr->get(MAP_SHRINE_SPIRITUALITY));

            if (!c->party->canEnterShrine(VIRT_SPIRITUALITY))
                return true;

            setMap(shrine_spirituality, 1, NULL);
            zu4_music_play(c->location->map->music);

            shrine_spirituality->enter();
        }

        return true;
    }

    return false;
}

/**
 * Fixes objects initially loaded by saveGameMonstersRead,
 * and alters movement behavior accordingly to match the creature
 */
void gameFixupObjects(Map *map) {
    int i;
    Object *obj;

    /* add stuff from the monster table to the map */
    for (i = 0; i < MONSTERTABLE_SIZE; i++) {
        SaveGameMonsterRecord *monster = &map->monsterTable[i];
        if (monster->prevTile != 0) {
            Coords coords = { monster->x, monster->y, 0 };

            // tile values stored in monsters.sav hardcoded to index into base tilemap
            MapTile tile = TileMap::get("base")->translate(monster->tile),
                oldTile = TileMap::get("base")->translate(monster->prevTile);

            if (i < MONSTERTABLE_CREATURES_SIZE) {
                const Creature *creature = creatureMgr->getByTile(tile);
                /* make sure we really have a creature */
                if (creature)
                    obj = map->addCreature(creature, coords);
                else {
                    fprintf(stderr, "Error: A non-creature object was found in the creature section of the monster table. (Tile: %s)\n", tile.getTileType()->getName().c_str());
                    obj = map->addObject(tile, oldTile, coords);
                }
            }
            else
                obj = map->addObject(tile, oldTile, coords);

            /* set the map for our object */
            obj->setMap(map);
        }
    }
}

time_t gameTimeSinceLastCommand() {
    return time(NULL) - c->lastCommandTime;
}

/**
 * Handles what happens when a creature attacks you
 */
void gameCreatureAttack(Creature *m) {
    Object *under;
    const Tile *ground;

    screenMessage("\nAttacked by %s\n", m->getName().c_str());

    /// TODO: CHEST: Make a user option to not make chests change battlefield
    /// map (2 of 2)
    ground = c->location->map->tileTypeAt(c->location->coords, WITH_GROUND_OBJECTS);
    if (!ground->isChest()) {
        ground = c->location->map->tileTypeAt(c->location->coords, WITHOUT_OBJECTS);
        if ((under = c->location->map->objectAt(c->location->coords)) &&
            under->getTile().getTileType()->isShip())
            ground = under->getTile().getTileType();
    }

    CombatController *cc = new CombatController(CombatMap::mapForTile(ground, c->party->getTransport().getTileType(), m));
    cc->init(m);
    cc->begin();
}

/**
 * Performs a ranged attack for the creature at x,y on the world map
 */
bool creatureRangeAttack(const Coords &coords, Creature *m) {
//    int attackdelay = MAX_BATTLE_SPEED - settings.battleSpeed;

    // Figure out what the ranged attack should look like
    MapTile tile(c->location->map->tileset->getByName((m && !m->getWorldrangedtile().empty()) ?
                                                      m->getWorldrangedtile() :
                                                      "hit_flash")->getId());

    GameController::flashTile(coords, tile, 1);

    // See if the attack hits the avatar
    Object *obj = c->location->map->objectAt(coords);
    m = dynamic_cast<Creature*>(obj);

    // Does the attack hit the avatar?
    if (zu4_coords_equal(coords, c->location->coords)) {
        /* always displays as a 'hit' */
    	GameController::flashTile(coords, tile, 3);

        /* FIXME: check actual damage from u4dos -- values here are guessed */
        if (c->transportContext == TRANSPORT_SHIP)
            gameDamageShip(-1, 10);
        else gameDamageParty(10, 25);

        return true;
    }
    // Destroy objects that were hit
    else if (obj) {
        if ((obj->getType() == Object::CREATURE && m->isAttackable()) ||
            obj->getType() == Object::UNKNOWN) {

        	GameController::flashTile(coords, tile, 3);
            c->location->map->removeObject(obj);

            return true;
        }
    }
    return false;
}

/**
 * Gets the path of coordinates for an action.  Each tile in the
 * direction specified by dirmask, between the minimum and maximum
 * distances given, is included in the path, until blockedPredicate
 * fails.  If a tile is blocked, that tile is included in the path
 * only if includeBlocked is true.
 */
std::vector<Coords> gameGetDirectionalActionPath(int dirmask, int validDirections, const Coords &origin, int minDistance, int maxDistance, bool (*blockedPredicate)(const Tile *tile), bool includeBlocked) {
    std::vector<Coords> path;
    Direction dirx = DIR_NONE,
              diry = DIR_NONE;

    /* Figure out which direction the action is going */
    if (DIR_IN_MASK(DIR_WEST, dirmask))
        dirx = DIR_WEST;
    else if (DIR_IN_MASK(DIR_EAST, dirmask))
        dirx = DIR_EAST;
    if (DIR_IN_MASK(DIR_NORTH, dirmask))
        diry = DIR_NORTH;
    else if (DIR_IN_MASK(DIR_SOUTH, dirmask))
        diry = DIR_SOUTH;

    /*
     * try every tile in the given direction, up to the given range.
     * Stop when the the range is exceeded, or the action is blocked.
     */

    Coords t_c = origin;
    if ((dirx <= 0 || DIR_IN_MASK(dirx, validDirections)) &&
        (diry <= 0 || DIR_IN_MASK(diry, validDirections))) {
        for (int distance = 0; distance <= maxDistance;
             distance++, movedir(&t_c, dirx, c->location->map), movedir(&t_c, diry, c->location->map)) {

            if (distance >= minDistance) {
                /* make sure our action isn't taking us off the map */
                if (MAP_IS_OOB(c->location->map, t_c))
                    break;

                const Tile *tile = c->location->map->tileTypeAt(t_c, WITH_GROUND_OBJECTS);

                /* should we see if the action is blocked before trying it? */
                if (!includeBlocked && blockedPredicate &&
                    !(*(blockedPredicate))(tile))
                    break;

                path.push_back(t_c);

                /* see if the action was blocked only if it did not succeed */
                if (includeBlocked && blockedPredicate &&
                    !(*(blockedPredicate))(tile))
                    break;
            }
        }
    }

    return path;
}

/**
 * Deals an amount of damage between 'minDamage' and 'maxDamage'
 * to each party member, with a 50% chance for each member to
 * avoid the damage.  If (minDamage == -1) or (minDamage >= maxDamage),
 * deals 'maxDamage' damage to each member.
 */
void gameDamageParty(int minDamage, int maxDamage) {
    int i;
    int damage;
    int lastdmged = -1;

    for (i = 0; i < c->party->size(); i++) {
        if (zu4_random(2) == 0) {
            damage = ((minDamage >= 0) && (minDamage < maxDamage)) ?
                zu4_random((maxDamage + 1) - minDamage) + minDamage :
                maxDamage;
            c->party->member(i)->applyDamage(damage);
            c->stats->highlightPlayer(i);
            lastdmged = i;
            EventHandler::sleep(50);
        }
    }

    screenShake(1);

    // Un-highlight the last player
    if (lastdmged != -1) c->stats->highlightPlayer(lastdmged);
}

/**
 * Deals an amount of damage between 'minDamage' and 'maxDamage'
 * to the ship.  If (minDamage == -1) or (minDamage >= maxDamage),
 * deals 'maxDamage' damage to the ship.
 */
void gameDamageShip(int minDamage, int maxDamage) {
    int damage;

    if (c->transportContext == TRANSPORT_SHIP) {
        damage = ((minDamage >= 0) && (minDamage < maxDamage)) ?
            zu4_random((maxDamage + 1) - minDamage) + minDamage :
            maxDamage;

        screenShake(1);

        c->party->damageShip(damage);
        gameCheckHullIntegrity();
    }
}

/**
 * Sets (or unsets) the active player
 */
void gameSetActivePlayer(int player) {
    if (player == -1) {
        c->party->setActivePlayer(-1);
        screenMessage("Set Active Player: None!\n");
    }
    else if (player < c->party->size()) {
        screenMessage("Set Active Player: %s!\n", c->party->member(player)->getName().c_str());
        if (c->party->member(player)->isDisabled())
            screenMessage("Disabled!\n");
        else
            c->party->setActivePlayer(player);
    }
}

/**
 * Removes creatures from the current map if they are too far away from the avatar
 */
void GameController::creatureCleanup() {
    ObjectDeque::iterator i;
    Map *map = c->location->map;

    for (i = map->objects.begin(); i != map->objects.end();) {
        Object *obj = *i;
        Coords o_coords = obj->getCoords();

        if ((obj->getType() == Object::CREATURE) && (o_coords.z == c->location->coords.z) &&
             distance(o_coords, c->location->coords, c->location->map) > MAX_CREATURE_DISTANCE) {

            /* delete the object and remove it from the map */
            i = map->removeObject(i);
        }
        else i++;
    }
}

/**
 * Checks creature conditions and spawns new creatures if necessary
 */
void GameController::checkRandomCreatures() {
    int canSpawnHere = c->location->map->isWorldMap() || c->location->context & CTX_DUNGEON;
    int spawnDivisor = c->location->context & CTX_DUNGEON ? (32 - (c->location->coords.z << 2)) : 32;

    /* If there are too many creatures already,
       or we're not on the world map, don't worry about it! */
    if (!canSpawnHere ||
        c->location->map->getNumberOfCreatures() >= MAX_CREATURES_ON_MAP ||
        zu4_random(spawnDivisor) != 0)
        return;

    gameSpawnCreature(NULL);
}

/**
 * Handles trolls under bridges
 */
void GameController::checkBridgeTrolls() {
    const Tile *bridge = c->location->map->tileset->getByName("bridge");
    if (!bridge)
        return;

    // TODO: CHEST: Make a user option to not make chests block bridge trolls
    if (!c->location->map->isWorldMap() ||
        c->location->map->tileAt(c->location->coords, WITH_OBJECTS)->id != bridge->getId() ||
        zu4_random(8) != 0)
        return;

    screenMessage("\nBridge Trolls!\n");

    Creature *m = c->location->map->addCreature(creatureMgr->getById(TROLL_ID), c->location->coords);
    CombatController *cc = new CombatController(MAP_BRIDGE_CON);
    cc->init(m);
    cc->begin();
}

/**
 * Check the levels of each party member while talking to Lord British
 */
void gameLordBritishCheckLevels() {
    bool advanced = false;

    for (int i = 0; i < c->party->size(); i++) {
        PartyMember *player = c->party->member(i);
        if (player->getRealLevel() < player->getMaxLevel()) {

            // add an extra space to separate messages
            if (!advanced) {
                screenMessage("\n");
                advanced = true;
            }
        }
        player->advanceLevel(); // FIXME: Should this be within the if?
    }

    screenMessage("\nWhat would thou\nask of me?\n");
}

/**
 * Spawns a creature (m) just offscreen of the avatar.
 * If (m==NULL) then it finds its own creature to spawn and spawns it.
 */
bool gameSpawnCreature(const Creature *m) {
    int t, i;
    const Creature *creature;
    Coords coords = c->location->coords;

    if (c->location->context & CTX_DUNGEON) {
        /* FIXME: for some reason dungeon monsters aren't spawning correctly */

        bool found = false;
        Coords new_coords;

        for (i = 0; i < 0x20; i++) {
            new_coords = (Coords){zu4_random(c->location->map->width), zu4_random(c->location->map->height), coords.z};
            const Tile *tile = c->location->map->tileTypeAt(new_coords, WITH_OBJECTS);
            if (tile->isCreatureWalkable()) {
                found = true;
                break;
            }
        }

        if (!found)
            return false;

        coords = new_coords;
    }
    else {
        int dx = 0,
            dy = 0;
        bool ok = false;
        int tries = 0;
        static const int MAX_TRIES = 10;

        while (!ok && (tries < MAX_TRIES)) {
            dx = 7;
            dy = zu4_random(7);

            if (zu4_random(2))
                dx = -dx;
            if (zu4_random(2))
                dy = -dy;
            if (zu4_random(2)) {
                t = dx;
                dx = dy;
                dy = t;
            }

            /* make sure we can spawn the creature there */
            if (m) {
                Coords new_coords = coords;
                movexy(&new_coords, dx, dy, c->location->map);

                const Tile *tile = c->location->map->tileTypeAt(new_coords, WITHOUT_OBJECTS);
                if ((m->sails() && tile->isSailable()) ||
                    (m->swims() && tile->isSwimable()) ||
                    (m->walks() && tile->isCreatureWalkable()) ||
                    (m->flies() && tile->isFlyable()))
                    ok = true;
                else tries++;
            }
            else ok = true;
        }

        if (ok)
            movexy(&coords, dx, dy, c->location->map);
    }

    /* can't spawn creatures on top of the player */
    if (zu4_coords_equal(coords, c->location->coords))
        return false;

    /* figure out what creature to spawn */
    if (m)
        creature = m;
    else if (c->location->context & CTX_DUNGEON)
        creature = creatureMgr->randomForDungeon(c->location->coords.z);
    else
        creature = creatureMgr->randomForTile(c->location->map->tileTypeAt(coords, WITHOUT_OBJECTS));

    if (creature)
        c->location->map->addCreature(creature, coords);
    return true;
}

/**
 * Destroys all creatures on the current map.
 */
void gameDestroyAllCreatures(void) {
    int i;

    gameSpellEffect('t', -1, SOUND_MAGIC); /* same effect as tremor */

    if (c->location->context & CTX_COMBAT) {
        /* destroy all creatures in combat */
        for (i = 0; i < AREA_CREATURES; i++) {
            CombatMap *cm = getCombatMap();
            CreatureVector creatures = cm->getCreatures();
            CreatureVector::iterator obj;

            for (obj = creatures.begin(); obj != creatures.end(); obj++) {
                if ((*obj)->getId() != LORDBRITISH_ID)
                    cm->removeObject(*obj);
            }
        }
    }
    else {
        /* destroy all creatures on the map */
        ObjectDeque::iterator current;
        Map *map = c->location->map;

        for (current = map->objects.begin(); current != map->objects.end();) {
            Creature *m = dynamic_cast<Creature*>(*current);

            if (m) {
                /* the skull does not destroy Lord British */
                if (m->getId() != LORDBRITISH_ID)
                    current = map->removeObject(current);
                else current++;
            }
            else current++;
        }
    }

    /* alert the guards! Really, the only one left should be LB himself :) */
    c->location->map->alertGuards();
}

/**
 * Creates the balloon near Hythloth, but only if the balloon doesn't already exists somewhere
 */
bool GameController::createBalloon(Map *map) {
    ObjectDeque::iterator i;

    /* see if the balloon has already been created (and not destroyed) */
    for (i = map->objects.begin(); i != map->objects.end(); i++) {
        Object *obj = *i;
        if (obj->getTile().getTileType()->isBalloon())
            return false;
    }

    const Tile *balloon = map->tileset->getByName("balloon");
    zu4_assert(balloon, "no balloon tile found in tileset");
    map->addObject(balloon->getId(), balloon->getId(), map->getLabel("balloon"));
    return true;
}

// Colors assigned to reagents based on my best reading of them
// from the book of wisdom.  Maybe we could use BOLD to distinguish
// the two grey and the two red reagents.
const int colors[] = {
  FG_YELLOW, FG_GREY, FG_BLUE, FG_WHITE, FG_RED, FG_GREY, FG_GREEN, FG_RED
};

void
showMixturesSuper(int page = 0) {
  screenTextColor(FG_WHITE);
  for (int i = 0; i < 13; i++) {
    char buf[4];

    const Spell *s = getSpell(i + 13 * page);
    int line = i + 8;
    screenTextAt(2, line, "%s", s->name);

    snprintf(buf, 4, "%3d", c->saveGame->mixtures[i + 13 * page]);
    screenTextAt(6, line, "%s", buf);

    screenShowChar(32, 9, line);
    int comp = s->components;
    for (int j = 0; j < 8; j++) {
      screenTextColor(colors[j]);
      screenShowChar(comp & (1 << j) ? CHARSET_BULLET : ' ', 10 + j, line);
    }
    screenTextColor(FG_WHITE);

    snprintf(buf, 3, "%2d", s->mp);
    screenTextAt(19, line, "%s", buf);
  }
}

void
mixReagentsSuper() {

  screenMessage("Mix reagents\n");

  static int page = 0;

  struct ReagentShop {
    const char *name;
    int price[6];
  };
  ReagentShop shops[] = {
    { "BuccDen", {6, 7, 9, 9, 9, 1} },
    { "Moonglo", {2, 5, 6, 3, 6, 9} },
    { "Paws",    {3, 4, 2, 8, 6, 7} },
    { "SkaraBr", {2, 4, 9, 6, 4, 8} },
  };
  const int shopcount = sizeof (shops) / sizeof (shops[0]);

  int oldlocation = c->location->viewMode;
  c->location->viewMode = VIEW_MIXTURES;
  screenUpdate(&game->mapArea, true, true);

  screenTextAt(16, 2, "%s", "<-Shops");

  c->stats->setView(StatsView(STATS_REAGENTS));
  screenTextColor(FG_PURPLE);
  screenTextAt(2, 7, "%s", "SPELL # Reagents MP");

  for (int i = 0; i < shopcount; i++) {
    int line = i + 1;
    ReagentShop *s = &shops[i];
    screenTextColor(FG_WHITE);
    screenTextAt(2, line, "%s", s->name);
    for (int j = 0; j < 6; j++) {
      screenTextColor(colors[j]);
      screenShowChar('0' + s->price[j], 10 + j, line);
    }
  }

  for (int i = 0; i < 8; i++) {
    screenTextColor(colors[i]);
    screenShowChar('A' + i, 10 + i, 6);
  }

  bool done = false;
  while (!done) {
    showMixturesSuper(page);
    screenMessage("For Spell: ");

    int spell = ReadChoiceController::get("abcdefghijklmnopqrstuvwxyz \033\n\r");
    if (spell < 'a' || spell > 'z' ) {
      screenMessage("\nDone.\n");
      done = true;
    } else {
      spell -= 'a';
      const Spell *s = getSpell(spell);
      screenMessage("%s\n", s->name);
      page = (spell >= 13);
      showMixturesSuper(page);

      // how many can we mix?
      int mixQty = 99 - c->saveGame->mixtures[spell];
      int ingQty = 99;
      int comp = s->components;
      for (int i = 0; i < 8; i++) {
        if (comp & 1 << i) {
          int reagentQty = c->saveGame->reagents[i];
          if (reagentQty < ingQty)
            ingQty = reagentQty;
        }
      }
      screenMessage("You can make %d.\n", (mixQty > ingQty) ? ingQty : mixQty);
      screenMessage("How many? ");

      int howmany = ReadIntController::get(2, TEXT_AREA_X + c->col, TEXT_AREA_Y + c->line);

      if (howmany == 0) {
        screenMessage("\nNone mixed!\n");
      } else if (howmany > mixQty) {
        screenMessage("\n%cYou cannot mix that much more of that spell!%c\n", FG_GREY, FG_WHITE);
      } else if (howmany > ingQty) {
        screenMessage("\n%cYou don't have enough reagents to mix %d spells!%c\n", FG_GREY, howmany, FG_WHITE);
      } else {
        c->saveGame->mixtures[spell] += howmany;
        for (int i = 0; i < 8; i++) {
          if (comp & 1 << i) {
            c->saveGame->reagents[i] -= howmany;
          }
        }
        screenMessage("\nSuccess!\n\n");
      }
    }
    c->stats->setView(StatsView(STATS_REAGENTS));
  }

  c->location->viewMode = oldlocation;
  return;
}
