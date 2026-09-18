// Compiled only with ULTIMATUM_WEB_RUNTIME_TESTS=ON, never into the normal client.
#include <SDL.h>
#include <emscripten.h>
#include "city.h"
#include "context.h"
#include "conversation.h"
#include "event.h"
#include "game.h"
#include "mapmgr.h"
#include "person.h"
#include "player.h"
#include "screen.h"
#include "settings.h"
#include "shrine.h"
#include "web_action_event.h"
#include "cheat.h"
#include "dungeon.h"
#include "stats.h"
#include "tileset.h"
#include "annotation.h"
#include "combat.h"
#include "creature.h"

bool talkAt(const Coords &coords);
void gameCreatureAttack(Creature *creature);
static std::string questionKeyword;
static Map *targetMap;
static Coords testTarget;
static int fixtureGeneration;
extern "C" EMSCRIPTEN_KEEPALIVE int zu4_web_test_generation() { return fixtureGeneration; }
extern "C" EMSCRIPTEN_KEEPALIVE int zu4_web_test_objects() { return c->location->map->objects.size(); }
extern "C" EMSCRIPTEN_KEEPALIVE int zu4_web_test_stats() { return c->saveGame->players[0].str + c->saveGame->players[0].dex + c->saveGame->players[0].intel; }
extern "C" EMSCRIPTEN_KEEPALIVE int zu4_web_test_flying() { return c->party->isFlying(); }
extern "C" EMSCRIPTEN_KEEPALIVE int zu4_web_test_wind_counter() { return c->windCounter; }
extern "C" EMSCRIPTEN_KEEPALIVE int zu4_web_test_target_state() {
    if (!targetMap) return 0;
    const Tile *tile = targetMap->tileTypeAt(testTarget, WITH_GROUND_OBJECTS);
    return (tile->isDoor() ? 1 : 0) | (tile->isLockedDoor() ? 2 : 0) | (tile->isChest() ? 4 : 0);
}
extern "C" EMSCRIPTEN_KEEPALIVE const char *zu4_web_test_question_keyword() { return questionKeyword.c_str(); }

extern "C" EMSCRIPTEN_KEEPALIVE int zu4_web_test_prepare(int scenario) {
    if (!c || !game || eventHandler->getController() != game) return 0;
    SDL_Event event = {};
    event.type = SDL_USEREVENT;
    event.user.code = ZU4_WEB_ACTION_TEST;
    event.user.data1 = reinterpret_cast<void *>(static_cast<intptr_t>(scenario));
    return SDL_PushEvent(&event) == 1;
}

extern "C" void zu4_web_test_dispatch(int scenario) {
    if (eventHandler->getController() != game) return;
    while (c->location->prev) game->exitToParentMap();
    // Make each case independent, including after a debug-grant save/reload.
    static SaveGame initial = *c->saveGame;
    *c->saveGame = initial;
    c->saveGame->members = 1;
    c->saveGame->players[0].klass = CLASS_MAGE;
    delete c->party;
    c->party = new Party(c->saveGame);
    c->party->addObserver(game);
    c->party->addObserver(c->stats);
    targetMap = nullptr;
    c->saveGame->gold = 1000;
    c->saveGame->food = 10000;
    c->saveGame->moves = 1600;
    c->saveGame->lastmeditation = 0;
    c->saveGame->runes = 0xff;
    for (int i = 0; i < 8; ++i) c->saveGame->karma[i] = 80;
    for (int i = 0; i < 16; ++i) c->saveGame->weapons[i] = 2;
    for (int i = 0; i < 8; ++i) c->saveGame->armor[i] = 2;
    c->saveGame->players[0].xp = 4000;
    c->saveGame->players[0].hpMax = 800;
    c->saveGame->players[0].hp = 300;
    c->saveGame->players[0].status = STAT_GOOD;
    c->party->setTransport(Tileset::findTileByName("avatar")->getId());
    c->saveGame->balloonstate = 0;
    c->horseSpeed = 0;
    c->windLock = true;
    c->opacity = true;
    collisionOverride = 0;
    if (scenario >= 20) {
        Map *world = mapMgr->get(MAP_WORLD);
        world->clearObjects();
        c->location->coords = {50, 50, 0};
        // Find a clear interior land/water patch rather than relying on save coordinates.
        bool water = scenario == 27;
        for (unsigned y = 30; y < world->height - 1; ++y) {
            bool found = false;
            for (unsigned x = 30; x < world->width - 1; ++x) {
                const Tile *ground = world->tileTypeAt({(int)x, (int)y, 0}, WITHOUT_OBJECTS);
                const Tile *north = world->tileTypeAt({(int)x, (int)y - 1, 0}, WITHOUT_OBJECTS);
                if (water ? ground->isSailable() && north->isSailable() : ground->isWalkable() && north->isWalkable() && !ground->isWater() && ((scenario != 28 && scenario != 35) || ground->canLandBalloon())) {
                    c->location->coords = {(int)x, (int)y, 0}; found = true; break;
                }
            }
            if (found) break;
        }
        c->saveGame->gems = 2; c->saveGame->torches = 2; c->saveGame->keys = 2; c->saveGame->sextants = 1;
        c->saveGame->items = ITEM_HORN;
        settings.campTime = 1;
        if (scenario == 36 || scenario == 37) {
            world->annotations->clear();
            c->location->coords = {100, 100, 0};
            for (int y = 94; y <= 106; ++y) for (int x = 94; x <= 106; ++x)
                world->annotations->add({x, y, 0}, MapTile(Tileset::findTileByName("grass")->getId()), false, true);
            settings.tapToWalk = true;
            settings.directInteractions = true;
            // Not on the route: the first north step makes this attacker
            // cardinally adjacent, so combat starts before random wandering.
            if (scenario == 37) world->addCreature(creatureMgr->getByName("rat"), {101, 99, 0});
        }
        if (scenario == 38 || scenario == 39) {
            auto *city = dynamic_cast<City *>(mapMgr->get(MAP_BRITAIN));
            game->setMap(city, true, nullptr);
            city->clearObjects();
            city->annotations->clear();
            c->location->coords = {15, 15, 0};
            for (int y = 9; y <= 21; ++y) for (int x = 9; x <= 21; ++x)
                city->annotations->add({x, y, 0}, city->tileset->getByName("grass")->getId(), false, true);
            settings.tapToWalk = true;
            settings.directInteractions = true;
            if (scenario == 38) for (Person *prototype : city->persons) {
                if (prototype->getNpcType() != NPC_TALKER || !prototype->getDialogue()) continue;
                Person *person = city->addPerson(prototype);
                person->setCoords({15, 12, 0});
                person->setMovementBehavior(MOVEMENT_FIXED);
                person->getDialogue()->setTurnAwayProb(0);
                break;
            }
            if (scenario == 39) {
                testTarget = {15, 12, 0}; targetMap = city;
                city->annotations->add(testTarget, city->tileset->getByName("door")->getId(), false, true);
            }
        }
        if (scenario == 21) {
            game->setMap(mapMgr->get(MAP_BRITAIN), true, nullptr);
            c->location->coords = {15, 15, 0};
        }
        if (scenario >= 22 && scenario <= 24) {
            auto *city = dynamic_cast<City *>(mapMgr->get(MAP_BRITAIN));
            game->setMap(city, true, nullptr);
            city->annotations->clear();
            for (unsigned y = 1; y < city->height - 1; ++y) {
                bool found = false;
                for (unsigned x = 1; x < city->width - 1; ++x) {
                    const Tile *tile = city->tileTypeAt({(int)x, (int)y, 0}, WITHOUT_OBJECTS);
                    bool match = scenario == 22 ? tile->isDoor() : scenario == 23 ? tile->isLockedDoor() : tile->isChest();
                    if (match) {
                        testTarget = {(int)x, (int)y, 0}; targetMap = city;
                        c->location->coords = scenario == 24 ? testTarget : Coords{(int)x, (int)y + 1, 0};
                        found = true; break;
                    }
                }
                if (found) break;
            }
            // Britain has no authored locked door. Place a real engine tile
            // in this isolated fixture; unlock still uses normal keys/rules.
            if (!targetMap) {
                testTarget = {15, 14, 0}; targetMap = city;
                city->annotations->add(testTarget, city->tileset->getByName(
                    scenario == 23 ? "locked_door" : scenario == 24 ? "chest" : "door")->getId());
                c->location->coords = scenario == 24 ? testTarget : Coords{15, 15, 0};
            }
        }
        if (scenario == 25 || scenario == 29 || scenario == 30) {
            auto *dungeon = dynamic_cast<Dungeon *>(mapMgr->get(MAP_DECEIT));
            game->setMap(dungeon, true, nullptr);
            c->location->coords = {1, 1, 0};
            if (scenario >= 29) for (unsigned z = 1; z < dungeon->levels; ++z) {
                bool found = false;
                for (unsigned y = 0; y < dungeon->height; ++y) for (unsigned x = 0; x < dungeon->width; ++x) {
                    Coords at = {(int)x, (int)y, (int)z};
                    bool match = scenario == 29 ? dungeon->ladderUpAt(at) : dungeon->ladderDownAt(at);
                    if (match) { c->location->coords = at; found = true; break; }
                }
                if (found) break;
            }
        }
        if ((scenario >= 26 && scenario <= 28) || scenario == 31 || scenario == 33 || scenario == 35) {
            const char *transport = scenario == 27 ? "ship" : (scenario == 28 || scenario == 35) ? "balloon" : "horse";
            MapTile tile = world->tileset->getByName(transport)->getId();
            Coords at = c->location->coords;
            if (scenario == 33) --at.y;
            world->addObject(tile, tile, at);
            if (scenario != 31 && scenario != 33) board();
            if (scenario == 35) c->saveGame->balloonstate = 1;
        }
        if (scenario == 32 || scenario == 34) {
            cheatSummonCreature("rat");
            if (scenario == 32) {
                for (Object *object : world->objects) if (auto *creature = dynamic_cast<Creature *>(object)) {
                    gameCreatureAttack(creature);
                    // Deterministic semantic-target fixture: take control of
                    // the first party member and put one enemy on a legal
                    // cardinal melee tile. Production combat still owns all
                    // target validation and attack behavior.
                    if (auto *combat = dynamic_cast<CombatController *>(eventHandler->getController())) {
                        combat->setActivePlayer(0);
                        PartyMember *attacker = combat->getCurrentPlayer();
                        auto enemies = combat->getMap()->getCreatures();
                        if (attacker && !enemies.empty()) {
                            Coords at = attacker->getCoords();
                            at.y = std::max(0, at.y - 1);
                            enemies.front()->setCoords(at);
                        }
                    }
                    break;
                }
            }
        }
        gameUpdateScreen();
        ++fixtureGeneration;
        return;
    }
    if (scenario == 14) {
        settings.shrineTime = 1; // Same sequence, accelerated for deterministic QA.
        Map *map = mapMgr->get(static_cast<MapId>(26));
        game->setMap(map, true, nullptr);
        c->location->coords = {5, 5, 0};
        dynamic_cast<Shrine *>(map)->enter();
        return;
    }
    int type = NPC_VENDOR_WEAPONS + scenario - 1;
    int mapId = 6;
    if (scenario == 5) mapId = 5;
    if (scenario == 8) mapId = 14;
    if (scenario == 9) mapId = 13;
    if (scenario == 10) type = NPC_TALKER_BEGGAR;
    if (scenario == 11) type = NPC_TALKER_COMPANION;
    if (scenario == 12) { type = NPC_LORD_BRITISH; mapId = 100; }
    if (scenario == 13) { type = NPC_HAWKWIND; mapId = 1; }
    if (scenario == 15) type = NPC_TALKER;
    if (scenario == 16) {
        type = NPC_VENDOR_HEALER;
        if (c->party->size() < 2) c->party->join("Iolo");
        c->saveGame->players[1].status = STAT_GOOD;
        c->saveGame->players[1].hp = 20;
    }
    auto *city = dynamic_cast<City *>(mapMgr->get(static_cast<MapId>(mapId)));
    game->setMap(city, true, nullptr);
    for (Person *person : city->persons) {
        if (person->getNpcType() != type) continue;
        if (scenario == 15) {
            questionKeyword.clear();
            Dialogue *dialogue = person->getDialogue();
            if (!dialogue || !dialogue->getQuestion()) continue;
            for (const std::string &keyword : dialogue->getKeywords()) {
                const auto &parts = (*dialogue)[keyword]->getResponse()->getParts();
                if (std::find(parts.begin(), parts.end(), ResponsePart::ASK) != parts.end()) { questionKeyword = keyword; break; }
            }
            if (questionKeyword.empty()) continue;
            dialogue->setTurnAwayProb(0);
        }
        // City::persons are placement prototypes; live people are cloned into
        // map objects by setMap/addPeople, so use their authored start tile.
        Coords target = person->getStart();
        c->location->coords = {target.x, target.y + 1, target.z};
        gameUpdateScreen();
        screenMessage("Runtime fixture: %s\n", person->getName().c_str());
        talkAt(target);
        return;
    }
    screenMessage("Runtime fixture failed: NPC type %d not in map %d\n", type, mapId);
}
