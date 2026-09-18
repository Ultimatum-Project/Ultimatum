#include <SDL.h>
#include <emscripten/emscripten.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <deque>
#include <sys/stat.h>
#include <sstream>
#include <string>
#include <utility>

#include "armor.h"
#include "combat.h"
#include "context.h"
#include "event.h"
#include "game.h"
#include "image.h"
#include "intro.h"
#include "map.h"
#include "miniz.h"
#include "names.h"
#include "player.h"
#include "screen.h"
#include "settings.h"
#include "music.h"
#include "soundtrack.h"
#include "spell.h"
#include "u4file.h"
#include "weapon.h"
#include "web_action_event.h"
#include "web_prompt.h"
#include "web_menu.h"
#include "experience_settings.h"
#include "save_validation.h"
#include "topicjournal.h"
#include "journal_notebook.h"
#include "web_journal.h"
#include "mobile_map_pins.h"
#include "mobile_map_discoveries.h"
#include "mobile_dungeon_exploration.h"
#include "zu4_ios_ui.h"

extern "C" int zu4_mobile_context_action(char *label, int capacity);
extern "C" void zu4_mobile_context(void);
extern "C" int zu4_mobile_capture_walk(int offsetX, int offsetY);
extern "C" void zu4_web_walk_start(int token);
extern "C" void zu4_web_walk_step(int generation);
extern "C" int zu4_mobile_world_taps_enabled(void);
extern "C" int zu4_mobile_capture_adjacent_interaction(int direction);
extern "C" int zu4_mobile_adjacent_action_label(int direction, char *label, int capacity);
extern "C" void zu4_mobile_adjacent_interaction(int encoded);
int gameSave(void);
#ifdef ZU4_WEB_RUNTIME_TESTS
extern "C" void zu4_web_test_dispatch(int scenario);
#endif

namespace {
std::deque<std::string> messages;
std::deque<std::string> recentEvents;
std::string snapshot;
std::vector<unsigned char> minimapPixels(33 * 33 * 4);
int minimapKind = 0;

struct AdjacentAction {
    int token = 0;
    std::string label;
};

std::string jsonEscape(const std::string &value) {
    std::ostringstream out;
    for (unsigned char ch : value) {
        switch (ch) {
        case '\"': out << "\\\""; break;
        case '\\': out << "\\\\"; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        case '\033': out << "\\u001b"; break;
        default:
            if (ch >= 0x20) out << ch;
            break;
        }
    }
    return out.str();
}

const char *statusName(StatusType status) {
    switch (status) {
    case STAT_GOOD: return "Good";
    case STAT_POISONED: return "Poisoned";
    case STAT_SLEEPING: return "Sleeping";
    case STAT_DEAD: return "Dead";
    default: return "Unknown";
    }
}

const char *contextName(LocationContext context) {
    if (context & CTX_COMBAT) return "Combat";
    if (context & CTX_DUNGEON) return "Dungeon";
    if (context & CTX_CITY) return "Settlement";
    if (context & CTX_SHRINE) return "Shrine";
    if (context & CTX_ALTAR_ROOM) return "Altar room";
    return "Britannia";
}

const char *inputMode() {
    Controller *controller = eventHandler ? eventHandler->getController() : nullptr;
    if (!controller) return "loading";
    if (auto *prompt = dynamic_cast<WebPromptController *>(controller)) return prompt->kind.c_str();
    if (dynamic_cast<ReadIntController *>(controller)) return "amount";
    if (dynamic_cast<ReadStringController *>(controller)) return "text";
    if (dynamic_cast<ReadDirController *>(controller)) return "direction";
    if (dynamic_cast<ReadPlayerController *>(controller)) return "player";
    if (dynamic_cast<ReadChoiceController *>(controller)) return "choice";
    if (dynamic_cast<AlphaActionController *>(controller)) return "letter";
    if (controller->isCombatController()) return webInteractionDepth() ? "busy" : "combat";
    if (dynamic_cast<IntroController *>(controller)) return "intro";
    if (dynamic_cast<GameController *>(controller)) return webInteractionDepth() ? "busy" : "command";
    return "busy";
}

SDL_Keycode sdlKeyFor(int key) {
    switch (key) {
    case 1001: return SDLK_UP;
    case 1002: return SDLK_DOWN;
    case 1003: return SDLK_LEFT;
    case 1004: return SDLK_RIGHT;
    case 8: return SDLK_BACKSPACE;
    case 13: return SDLK_RETURN;
    case 27: return SDLK_ESCAPE;
    default: return static_cast<SDL_Keycode>(std::tolower(key));
    }
}

void enqueueKey(int key) {
    SDL_Event event = {};
    event.type = SDL_KEYDOWN;
    event.key.state = SDL_PRESSED;
    event.key.repeat = 0;
    event.key.keysym.sym = sdlKeyFor(key);
    if (key >= 'A' && key <= 'Z') event.key.keysym.mod = KMOD_SHIFT;
    SDL_PushEvent(&event);
}

void enqueueAction(int action, int parameter = 0) {
    // A contextual action may enter a nested legacy input loop. Queue it for
    // the active engine loop rather than re-entering Asyncify from a JS ccall.
    SDL_Event event = {};
    event.type = SDL_USEREVENT;
    event.user.code = action;
    event.user.data1 = reinterpret_cast<void *>(static_cast<intptr_t>(parameter));
    SDL_PushEvent(&event);
}

AdjacentAction uniqueAdjacentAction() {
    AdjacentAction result;
    for (Direction direction : {DIR_NORTH, DIR_WEST, DIR_SOUTH, DIR_EAST}) {
        char label[32] = {};
        if (!zu4_mobile_adjacent_action_label(direction, label, sizeof(label))) continue;
        const int token = zu4_mobile_capture_adjacent_interaction(direction);
        if (!token) continue;
        if (result.token) return {};
        result.token = token;
        result.label = label;
    }
    return result;
}

void writePromptOptions(std::ostringstream &out, const char *mode, Controller *controller) {
    if (webInteractionDepth() && std::string(mode) == "busy") {
        out << ",\"title\":\"Please wait\",\"context\":\""
            << jsonEscape(messages.empty() ? "The interaction is finishing." : messages.back())
            << "\",\"acceptsText\":false,\"options\":[]";
        return;
    }
    if (auto *prompt = dynamic_cast<WebPromptController *>(controller)) {
        out << ",\"id\":" << prompt->generation
            << ",\"title\":\"" << jsonEscape(prompt->title)
            << "\",\"context\":\"" << jsonEscape(prompt->context)
            << "\",\"acceptsText\":" << (prompt->acceptsText ? "true" : "false")
            << ",\"maxLength\":" << prompt->maxLength
            << ",\"submitted\":" << (prompt->submitted || prompt->completed ? "true" : "false")
            << ",\"options\":[";
        bool comma = false;
        for (const auto &option : prompt->options) {
            if (comma) out << ',';
            comma = true;
            out << "{\"value\":\"" << jsonEscape(option.value)
                << "\",\"label\":\"" << jsonEscape(option.label)
                << "\",\"disabled\":" << (option.enabled ? "false" : "true") << '}';
        }
        out << ']';
        return;
    }
    out << ",\"options\":[";
    bool needsComma = false;
    auto writeOption = [&](int key, const char *label, const char *symbol = nullptr) {
        if (needsComma) out << ',';
        needsComma = true;
        out << "{\"key\":" << key << ",\"label\":\"" << label << "\"";
        if (symbol) out << ",\"symbol\":\"" << symbol << "\"";
        out << '}';
    };

    if (std::string(mode) == "direction") {
        writeOption(1001, "North", "↑");
        writeOption(1003, "West", "←");
        writeOption(1002, "South", "↓");
        writeOption(1004, "East", "→");
        writeOption(U4_ESC, "Cancel");
    } else if (std::string(mode) == "choice") {
        auto *choice = dynamic_cast<ReadChoiceController *>(controller);
        if (!choice) {
            out << ']';
            return;
        }
        bool seen[128] = {};
        if (choice->getChoices().empty()) writeOption(13, "Continue");
        for (unsigned char raw : choice->getChoices()) {
            if (raw >= 128 || !std::isprint(raw)) continue;
            const unsigned char key = std::isupper(raw) ? std::tolower(raw) : raw;
            if (seen[key]) continue;
            seen[key] = true;
            std::string label(1, static_cast<char>(std::toupper(key)));
            if (key == 'y') label = "Yes";
            if (key == 'n') label = "No";
            writeOption(key, jsonEscape(label).c_str());
        }
        if (choice->getChoices().find(static_cast<char>(U4_ESC)) != std::string::npos)
            writeOption(U4_ESC, "Cancel");
    }
    out << ']';
}

std::string upperBasename(const char *path) {
    std::string name = path ? path : "";
    const size_t slash = name.find_last_of("/\\");
    if (slash != std::string::npos) name.erase(0, slash + 1);
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    return name;
}

bool writeBytes(const std::string &path, const void *data, size_t size) {
    FILE *file = fopen(path.c_str(), "wb");
    if (!file) return false;
    const bool written = fwrite(data, 1, size, file) == size;
    return fclose(file) == 0 && written;
}
}

extern "C" void zu4_web_message(const char *text) {
    if (!text || !text[0]) return;
    std::string clean;
    for (const unsigned char ch : std::string(text)) {
        if (ch == '\r') continue;
        if (ch == '\n' || ch == '\t' || ch >= 0x20) clean.push_back(static_cast<char>(ch));
    }
    while (!clean.empty() && std::isspace(static_cast<unsigned char>(clean.back()))) clean.pop_back();
    if (clean.empty()) return;
    messages.push_back(clean);
    while (messages.size() > 40) messages.pop_front();
    // Prompts/NPC prose have their own live surface and durable journal.
    // Keep the event log useful instead of duplicating whole conversations.
    if (!webInteractionDepth() && eventHandler &&
        (eventHandler->getController() == game || dynamic_cast<CombatController *>(eventHandler->getController()))) {
        recentEvents.push_back(clean);
        while (recentEvents.size() > 40) recentEvents.pop_front();
    }
}

extern "C" EMSCRIPTEN_KEEPALIVE int zu4_web_ready() {
    return c && c->party && c->saveGame && c->location && c->location->map;
}

extern "C" EMSCRIPTEN_KEEPALIVE void zu4_web_configure_input() {
    // The modern shell owns keyboard input. Keep SDL from globally consuming
    // HTML focus/typing or also queuing a compatibility key for the same tap.
    // Standalone SDL pages retain their normal default keyboard target.
    SDL_SetHint(SDL_HINT_EMSCRIPTEN_KEYBOARD_ELEMENT, "#engineCanvas");
}

extern "C" EMSCRIPTEN_KEEPALIVE int zu4_web_has_game_data() {
    U4FILE *avatar = u4fopen("AVATAR.EXE");
    U4FILE *title = u4fopen("TITLE.EXE");
    const bool found = avatar && title;
    if (avatar) u4fclose(avatar);
    if (title) u4fclose(title);
    return found ? 1 : 0;
}

extern "C" EMSCRIPTEN_KEEPALIVE uintptr_t zu4_web_screen_pixels() {
    Image *screen = zu4_img_get_screen();
    return screen && screen->pixels
        ? reinterpret_cast<uintptr_t>(screen->pixels)
        : 0;
}

extern "C" EMSCRIPTEN_KEEPALIVE int zu4_web_install_zip(const unsigned char *data, int size, int kind) {
    if (!data || size < 1) return -1;
    if (kind == 2) return writeBytes("/u4upgrad.zip", data, static_cast<size_t>(size)) ? 1 : -2;
    if (kind != 1 && kind != 3) return -3;
    const bool installSaves = kind == 1;

    mz_zip_archive archive = {};
    if (!mz_zip_reader_init_mem(&archive, data, static_cast<size_t>(size), 0)) return -4;
    mkdir("/ultima4", 0777);
    mkdir("/home/web_user", 0777);
    mkdir("/home/web_user/.xu4", 0777);
    int installed = 0;
    const mz_uint count = mz_zip_reader_get_num_files(&archive);
    for (mz_uint index = 0; index < count; ++index) {
        mz_zip_archive_file_stat stat = {};
        if (!mz_zip_reader_file_stat(&archive, index, &stat) || stat.m_is_directory || !stat.m_is_supported) continue;
        if (stat.m_uncomp_size > 64 * 1024 * 1024) continue;
        const std::string name = upperBasename(stat.m_filename);
        if (name.empty()) continue;
        size_t extractedSize = 0;
        void *contents = mz_zip_reader_extract_to_heap(&archive, index, &extractedSize, 0);
        if (!contents) continue;
        const bool written = writeBytes("/ultima4/" + name, contents, extractedSize);
        if (installSaves && written && (name == "PARTY.SAV" || name == "MONSTERS.SAV" ||
                        name == "OUTMONST.SAV" || name == "DNGMAP.SAV")) {
            std::string lower = name;
            std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char ch) {
                return static_cast<char>(std::tolower(ch));
            });
            writeBytes("/home/web_user/.xu4/" + lower, contents, extractedSize);
        }
        mz_free(contents);
        if (written) ++installed;
    }
    mz_zip_reader_end(&archive);
    return installed;
}

extern "C" EMSCRIPTEN_KEEPALIVE void zu4_web_send_key(int key) {
    if (webJournalIsOpen()) return;
    if (webInteractionDepth() && eventHandler->getController() == game) return;
    if (zu4_mobile_dungeon_top_down()) {
        Direction direction = key == 1001 ? DIR_NORTH : key == 1002 ? DIR_SOUTH :
            key == 1003 ? DIR_WEST : key == 1004 ? DIR_EAST : DIR_NONE;
        if (direction != DIR_NONE) {
            enqueueAction(ZU4_WEB_ACTION_GAMEPLAY_BASE + ZU4_MOBILE_ACTION_MOVE, direction);
            return;
        }
    }
    enqueueKey(key);
}

extern "C" EMSCRIPTEN_KEEPALIVE void zu4_web_send_text(const char *text) {
    if (webJournalIsOpen()) return;
    if (!text) return;
    for (const unsigned char ch : std::string(text)) {
        if (ch == '\n' || ch == '\r') continue;
        enqueueKey(ch);
    }
    enqueueKey(13);
}

extern "C" EMSCRIPTEN_KEEPALIVE int zu4_web_submit_prompt(const char *text) {
    if (webJournalIsOpen()) return 0;
    if (!text || !eventHandler) return 0;
    Controller *controller = eventHandler->getController();
    if (!controller) return 0;
    std::string value(text);

    if (auto *prompt = dynamic_cast<WebPromptController *>(controller)) {
        if (!prompt->stage(value)) return 0;
        enqueueAction(ZU4_WEB_ACTION_PROMPT, prompt->generation);
        return 1;
    }

    if (dynamic_cast<ReadStringController *>(controller)) {
        for (const unsigned char ch : value) {
            if (ch == '\n' || ch == '\r') continue;
            enqueueKey(ch);
        }
        enqueueKey('\r');
        return 1;
    }

    if (dynamic_cast<ReadChoiceController *>(controller) ||
        dynamic_cast<ReadPlayerController *>(controller) ||
        dynamic_cast<AlphaActionController *>(controller)) {
        const size_t first = value.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return 0;
        enqueueKey(static_cast<unsigned char>(value[first]));
        return 1;
    }
    return 0;
}

extern "C" EMSCRIPTEN_KEEPALIVE int zu4_web_end_conversation() {
    return zu4_web_submit_prompt("bye");
}

extern "C" EMSCRIPTEN_KEEPALIVE int zu4_web_submit_answer(const char *text, int generation) {
    auto *prompt = eventHandler ? dynamic_cast<WebPromptController *>(eventHandler->getController()) : nullptr;
    if (!prompt || prompt->generation != generation) return 0;
    return zu4_web_submit_prompt(text);
}

extern "C" EMSCRIPTEN_KEEPALIVE int zu4_web_submit_option(int key) {
    if (!eventHandler) return 0;
    Controller *controller = eventHandler->getController();
    if (!controller) return 0;

    if (dynamic_cast<ReadDirController *>(controller)) {
        if (key != 1001 && key != 1002 && key != 1003 && key != 1004 && key != U4_ESC)
            return 0;
        enqueueKey(key);
        return 1;
    }

    auto *choice = dynamic_cast<ReadChoiceController *>(controller);
    if (!choice || dynamic_cast<ReadPlayerController *>(controller)) return 0;
    if (key >= 'A' && key <= 'Z') key = std::tolower(key);
    if (!choice->getChoices().empty() &&
        choice->getChoices().find(static_cast<char>(key)) == std::string::npos)
        return 0;
    enqueueKey(key);
    return 1;
}

extern "C" EMSCRIPTEN_KEEPALIVE int zu4_web_equip(int memberIndex, int category, int type) {
    if (webInteractionDepth()) return -2;
    if (!zu4_web_ready() || memberIndex < 0 || memberIndex >= c->party->size()) return -1;
    Controller *owner = eventHandler->getController();
    CombatController *combat = dynamic_cast<CombatController *>(owner);
    if (owner != game && !combat) return -2;

    PartyMember *member = c->party->member(memberIndex);
    EquipError result;
    const char *name;
    if (category == 1) {
        if (type < WEAP_HANDS || type >= WEAP_MAX ||
            (combat && member != combat->getCurrentPlayer()) ||
            member->getWeapon()->type == type) return -3;
        result = member->setWeapon(static_cast<WeaponType>(type));
        name = zu4_weapon_name(static_cast<WeaponType>(type));
    } else if (category == 2) {
        if (type < ARMR_NONE || type >= ARMR_MAX || combat ||
            member->getArmor()->type == type) return -3;
        result = member->setArmor(static_cast<ArmorType>(type));
        name = zu4_armor_name(static_cast<ArmorType>(type));
    } else {
        return -1;
    }

    if (result != EQUIP_SUCCEEDED) return result == EQUIP_NONE_LEFT ? -4 : -5;
    screenMessage("%s equipped %s.\n", member->getName().c_str(), name);
    if (eventHandler->getController() == owner) c->location->turnCompleter->finishTurn();
    return 1;
}

extern "C" EMSCRIPTEN_KEEPALIVE int zu4_web_set_video(int videoType) {
    if (!zu4_web_ready() || webInteractionDepth()) return 0;
    return webApplyGraphics(videoType) ? 1 : 0;
}

namespace {
bool menuQueued = false;
bool checkpointQueued = false;
uint32_t walkRequestGeneration = 0;
int pendingWalkToken = 0;
}
extern "C" EMSCRIPTEN_KEEPALIVE int zu4_web_open_menu() {
    if (menuQueued || !webMenuAvailable()) return 0;
    menuQueued = true;
    enqueueAction(ZU4_WEB_ACTION_MENU);
    return 1;
}

extern "C" EMSCRIPTEN_KEEPALIVE int zu4_web_activate_primary_action() {
    if (webInteractionDepth()) return 0;
    char label[32] = {};
    if (zu4_mobile_context_action(label, sizeof(label))) {
        enqueueAction(ZU4_WEB_ACTION_CONTEXT);
        return 1;
    }
    const AdjacentAction adjacent = uniqueAdjacentAction();
    if (!adjacent.token) return 0;
    enqueueAction(ZU4_WEB_ACTION_ADJACENT, adjacent.token);
    return 1;
}

extern "C" EMSCRIPTEN_KEEPALIVE int zu4_web_gameplay_action(int action, int parameter) {
    if (webInteractionDepth() || action < ZU4_MOBILE_ACTION_DUNGEON_SEARCH ||
        action > ZU4_MOBILE_ACTION_COMBAT_REPEAT_ATTACK) return 0;
    enqueueAction(ZU4_WEB_ACTION_GAMEPLAY_BASE + action, parameter);
    return 1;
}

extern "C" EMSCRIPTEN_KEEPALIVE int zu4_web_prepare_map() {
    return zu4_mobile_prepare_exploration_map();
}
extern "C" EMSCRIPTEN_KEEPALIVE uintptr_t zu4_web_map_pixels() {
    return reinterpret_cast<uintptr_t>(zu4_mobile_prepared_map_pixels());
}
extern "C" EMSCRIPTEN_KEEPALIVE int zu4_web_map_width() { return zu4_mobile_prepared_map_width(); }
extern "C" EMSCRIPTEN_KEEPALIVE int zu4_web_map_height() { return zu4_mobile_prepared_map_height(); }
extern "C" EMSCRIPTEN_KEEPALIVE int zu4_web_map_player_x() { return zu4_mobile_prepared_map_player_x(); }
extern "C" EMSCRIPTEN_KEEPALIVE int zu4_web_map_player_y() { return zu4_mobile_prepared_map_player_y(); }
extern "C" EMSCRIPTEN_KEEPALIVE int zu4_web_set_map_pin(int x, int y, const char *label) {
    return zu4_mobile_set_map_pin(x, y, label);
}
extern "C" EMSCRIPTEN_KEEPALIVE int zu4_web_remove_map_pin(int x, int y) {
    return zu4_mobile_remove_map_pin(x, y);
}

extern "C" void zu4_web_dispatch_action(int action, int parameter) {
    if (webJournalIsOpen()) {
        if(action == ZU4_WEB_ACTION_CHECKPOINT) checkpointQueued=false;
        if(action == ZU4_WEB_ACTION_MENU) menuQueued=false;
        return;
    }
    if (action == ZU4_WEB_ACTION_CHECKPOINT) {
        checkpointQueued = false;
        if (!webInteractionDepth()) webSaveCheckpoint(true);
        return;
    }
    if (action == ZU4_WEB_ACTION_MENU) { ++walkRequestGeneration; menuQueued = false; webOpenMenu(); return; }
    if (action == ZU4_WEB_ACTION_WALK_START) {
        if (static_cast<uint32_t>(parameter) == walkRequestGeneration && !webInteractionDepth())
            zu4_web_walk_start(pendingWalkToken);
        return;
    }
    if (action == ZU4_WEB_ACTION_WALK_STEP) {
        zu4_web_walk_step(parameter);
        return;
    }
#ifdef ZU4_WEB_RUNTIME_TESTS
    if (action == ZU4_WEB_ACTION_TEST) { zu4_web_test_dispatch(parameter); return; }
#endif
    if (action > ZU4_WEB_ACTION_GAMEPLAY_BASE && action <= ZU4_WEB_ACTION_GAMEPLAY_END) {
        zu4_mobile_perform_gameplay_action(action - ZU4_WEB_ACTION_GAMEPLAY_BASE, parameter);
        return;
    }
    if (webInteractionDepth() && (action == ZU4_WEB_ACTION_CONTEXT || action == ZU4_WEB_ACTION_ADJACENT)) return;
    if (action == ZU4_WEB_ACTION_CONTEXT) zu4_mobile_context();
    else if (action == ZU4_WEB_ACTION_ADJACENT) zu4_mobile_adjacent_interaction(parameter);
    else if (action == ZU4_WEB_ACTION_PROMPT) {
        auto *prompt = dynamic_cast<WebPromptController *>(eventHandler->getController());
        if (prompt && prompt->generation == parameter) prompt->dispatch();
    }
}

extern "C" EMSCRIPTEN_KEEPALIVE int zu4_web_save_adventure() {
    if (webInteractionDepth()) return 0;
    if (!zu4_web_ready() || eventHandler->getController() != game ||
        !(c->location->context & CTX_CAN_SAVE_GAME)) return 0;
    screenMessage("Save...\n%d moves\n", c->saveGame->moves);
    return webSaveCheckpoint();
}

extern "C" EMSCRIPTEN_KEEPALIVE int zu4_web_request_checkpoint() {
    if (checkpointQueued || menuQueued || webInteractionDepth() || !zu4_web_ready() ||
        eventHandler->getController() != game || !(c->location->context & CTX_CAN_SAVE_GAME)) return 0;
    checkpointQueued = true;
    enqueueAction(ZU4_WEB_ACTION_CHECKPOINT);
    return 1;
}

extern "C" EMSCRIPTEN_KEEPALIVE const char *zu4_web_save_info(const char *directory) {
    static std::string result;
    const std::string dir = directory ? directory : "";
    if (dir != "/home/web_user/.xu4/" && dir != "/adventure-check/") return "null";
    if (!SaveValidation::checkpointFiles(dir)) return "null";
    std::vector<MobileDungeonExploration::Shape> shapes;
    for (int map = 17; map <= 24; ++map) shapes.push_back({map,8,8,8});
    struct stat journalStat;
    TopicJournal checkedJournal;
    JournalNotebook checkedNotebook;
    if ((stat((dir + "topics.txt").c_str(), &journalStat)==0 && !checkedJournal.load(dir + "topics.txt")) ||
        !checkedNotebook.load(dir + "journal-notebook.dat") || !checkedNotebook.validFor(checkedJournal) ||
        !MobileMapPins().load(dir + "map-pins.dat",256,256) ||
        !MobileMapDiscoveries().load(dir + "map-discoveries.dat",256,256) ||
        !MobileDungeonExploration().load(dir + "explored-dungeons.dat",shapes)) return "null";
    FILE *explored = fopen((dir + "explored-map.dat").c_str(), "rb");
    if (explored) {
        char header[64] = {};
        bool valid = fgets(header,sizeof(header),explored) && std::string(header) == "ZU4-EXPLORED-MAP-1\n";
        for (int i = 0; valid && i < 256*256; ++i) {int byte=fgetc(explored);valid=byte==0 || byte==1;}
        valid = valid && fgetc(explored)==EOF && !ferror(explored);
        if (fclose(explored)!=0) valid=false;
        if (!valid) return "null";
    }
    SaveGame save = {};
    FILE *file = fopen((dir + "party.sav").c_str(), "rb");
    if (!file) return "null";
    bool valid = saveGameRead(&save,file)!=0;
    if (fclose(file)!=0 || !valid) return "null";
    std::ostringstream out;
    out << "{\"name\":\"" << jsonEscape(save.players[0].name) << "\",\"moves\":" << save.moves
        << ",\"members\":" << save.members << ",\"location\":" << save.location
        << ",\"level\":" << unsigned(save.dnglevel)
        << ",\"x\":" << unsigned(save.x) << ",\"y\":" << unsigned(save.y) << "}";
    result=out.str();return result.c_str();
}

extern "C" EMSCRIPTEN_KEEPALIVE int zu4_web_tap_world(int offsetX, int offsetY) {
    if (webInteractionDepth() || menuQueued) return 0;
    const int token = zu4_mobile_capture_walk(offsetX, offsetY);
    if (!token) return 0;
    pendingWalkToken = token;
    enqueueAction(ZU4_WEB_ACTION_WALK_START, static_cast<int>(++walkRequestGeneration));
    return 1;
}

extern "C" EMSCRIPTEN_KEEPALIVE const char *zu4_web_snapshot_json() {
    std::ostringstream out;
    const bool ready = zu4_web_ready();
    Controller *controller = eventHandler ? eventHandler->getController() : nullptr;
    const char *mode = inputMode();
    out << "{\"contractVersion\":1"
        << ",\"journalOpen\":" << (webJournalIsOpen() ? "true" : "false")
        << ",\"ready\":" << (ready ? "true" : "false")
        << ",\"inputMode\":\"" << mode << "\""
        << ",\"interactionActive\":" << (webInteractionDepth() ? "true" : "false")
        << ",\"storageRevision\":" << webStorageRevision()
        << ",\"libraryRequest\":" << webLibraryRequest()
        << ",\"adventureRequest\":" << webAdventureRequest()
        << ",\"saveRevision\":" << webSaveRevision()
        << ",\"saveAutomatic\":" << (webSaveAutomatic() ? "true" : "false")
        << ",\"canSave\":" << (ready && !webInteractionDepth() && controller==game &&
            (c->location->context & CTX_CAN_SAVE_GAME) ? "true" : "false")
        << ",\"recoveryDownload\":{\"request\":" << webRecoveryDownloadRequest()
        << ",\"bytes\":" << reinterpret_cast<uintptr_t>(webRecoveryZip())
        << ",\"size\":" << webRecoveryZipSize() << '}'
        << ",\"menuAvailable\":" << (webMenuAvailable() ? "true" : "false")
        << ",\"preferences\":{\"profile\":\"" << zu4_experience_profile_key(zu4_experience_profile())
        << "\",\"customized\":" << (zu4_experience_is_customized() ? "true" : "false")
        << ",\"filterMovementMessages\":" << (settings.filterMoveMessages ? "true" : "false")
        << ",\"bumpInteractions\":" << (settings.bumpInteractions ? "true" : "false")
        << ",\"directInteractions\":" << (settings.directInteractions ? "true" : "false")
        << ",\"tapToWalk\":" << (settings.tapToWalk ? "true" : "false") << '}'
        << ",\"vgaAvailable\":" << (u4isUpgradeAvailable() ? "true" : "false")
        << ",\"video\":\"" << (settings.videoType ? "vga" : "ega") << "\""
        << ",\"audio\":{\"soundtrack\":\"" << (zu4_soundtrack(settings.soundtrack) ? zu4_soundtrack(settings.soundtrack)->key : "unavailable")
        << "\",\"musicVolume\":" << settings.musicVol << ",\"effectsVolume\":" << settings.soundVol
        << ",\"enabled\":" << (zu4_music_is_enabled() ? "true" : "false")
        << ",\"track\":" << zu4_music_current_track() << '}'
        << ",\"capabilities\":{\"primaryAction\":true,\"worldTap\":"
        << (zu4_mobile_world_taps_enabled() ? "true" : "false")
        << ",\"menu\":true,\"debugTools\":" << (webDebugToolsEnabled() ? "true" : "false")
        << ",\"explorationMap\":" << (zu4_mobile_exploration_map_enabled() ? "true" : "false")
        << ",\"mapPins\":" << (zu4_mobile_map_pins_enabled() ? "true" : "false")
        << ",\"combatTargeting\":true,\"dungeonControls\":true,\"promptInput\":true}"
        << ",\"prompt\":{\"kind\":\"" << mode << "\"";
    writePromptOptions(out, mode, controller);
    out << '}';

    if (ready) {
        Controller *owner = eventHandler->getController();
        CombatController *combat = dynamic_cast<CombatController *>(owner);
        out << ",\"viewMode\":" << c->location->viewMode
            << ",\"transport\":" << c->transportContext
            << ",\"supplies\":{\"gems\":" << c->saveGame->gems
            << ",\"torches\":" << c->saveGame->torches
            << ",\"keys\":" << c->saveGame->keys
            << ",\"sextants\":" << c->saveGame->sextants
            << ",\"items\":" << c->saveGame->items
            << ",\"reagents\":[";
        for (int i = 0; i < REAG_MAX; ++i) { if (i) out << ','; out << c->saveGame->reagents[i]; }
        out << "]}";
        if (webGemMapPixels()) out << ",\"peerMap\":{\"pixels\":" << reinterpret_cast<uintptr_t>(webGemMapPixels())
            << ",\"width\":" << webGemMapWidth() << ",\"height\":" << webGemMapHeight()
            << ",\"x\":" << c->location->coords.x << ",\"y\":" << c->location->coords.y << '}';
        if (webDebugToolsEnabled()) {
            out << ",\"debugState\":{\"collisionOverride\":" << (collisionOverride ? "true" : "false")
                << ",\"seeThroughWalls\":" << (!c->opacity ? "true" : "false")
                << ",\"windLocked\":" << (c->windLock ? "true" : "false")
                << ",\"moonPhase\":" << c->saveGame->trammelphase << '}';
        }
        const std::string mapName = c->location->map->getName().empty()
            ? "Britannia" : c->location->map->getName();
        char primaryActionLabel[32] = {};
        bool primaryActionEnabled = !webInteractionDepth() &&
            zu4_mobile_context_action(primaryActionLabel, sizeof(primaryActionLabel));
        if (!primaryActionEnabled && !webInteractionDepth()) {
            const AdjacentAction adjacent = uniqueAdjacentAction();
            primaryActionEnabled = adjacent.token != 0;
            snprintf(primaryActionLabel, sizeof(primaryActionLabel), "%s",
                primaryActionEnabled ? adjacent.label.c_str() : "Interact");
        }
        out << ",\"location\":{\"name\":\"" << jsonEscape(mapName)
            << "\",\"context\":\"" << contextName(c->location->context)
            << "\",\"x\":" << c->location->coords.x
            << ",\"y\":" << c->location->coords.y
            << ",\"z\":" << c->location->coords.z << "}"
            << ",\"primaryAction\":{\"id\":\"context\",\"label\":\""
            << jsonEscape(primaryActionLabel)
            << "\",\"enabled\":" << (primaryActionEnabled ? "true" : "false") << "}"
            << ",\"moves\":" << c->saveGame->moves
            << ",\"food\":" << c->saveGame->food / 100
            << ",\"gold\":" << c->saveGame->gold
            << ",\"wind\":\"" << getDirectionName(static_cast<Direction>(c->windDirection)) << "\""
            << ",\"moons\":{\"trammel\":" << c->saveGame->trammelphase
            << ",\"felucca\":" << c->saveGame->feluccaphase << "}";

        minimapKind = zu4_mobile_minimap(minimapPixels.data(), 33);
        if (minimapKind) out << ",\"minimap\":{\"pixels\":"
            << reinterpret_cast<uintptr_t>(minimapPixels.data())
            << ",\"width\":33,\"height\":33,\"kind\":" << minimapKind << '}';

        out << ",\"dungeon\":{\"active\":" << (zu4_mobile_dungeon_active() ? "true" : "false")
            << ",\"overhead\":" << (zu4_mobile_dungeon_top_down() ? "true" : "false")
            << ",\"facing\":\"" << getDirectionName(static_cast<Direction>(c->saveGame->orientation))
            << "\",\"light\":" << c->party->getTorchDuration()
            << ",\"torches\":" << c->saveGame->torches
            << ",\"level\":" << (c->location->coords.z + 1) << '}';

        char repeatTarget[ZU4_MOBILE_COMBAT_TARGET_NAME_CAPACITY] = {};
        const bool repeatAvailable = zu4_mobile_combat_repeat_target(repeatTarget, sizeof(repeatTarget));
        out << ",\"combat\":{\"active\":" << (combat ? "true" : "false")
            << ",\"selected\":" << (zu4_mobile_combat_target_selected() ? "true" : "false")
            << ",\"prepared\":" << (zu4_mobile_combat_target_prepared() ? "true" : "false")
            << ",\"repeatTarget\":" << (repeatAvailable ? "\"" + jsonEscape(repeatTarget) + "\"" : "null")
            << ",\"targets\":[";
        bool targetComma = false;
        for (int i = 0; combat && i < zu4_mobile_combat_target_count(); ++i) {
            Zu4MobileCombatTarget target = {};
            if (!zu4_mobile_combat_target_at(i, &target)) continue;
            if (targetComma) out << ',';
            targetComma = true;
            out << "{\"token\":" << target.token << ",\"name\":\"" << jsonEscape(target.name)
                << "\",\"x\":" << target.screenX << ",\"y\":" << target.screenY
                << ",\"attackerX\":" << target.attackerScreenX << ",\"attackerY\":" << target.attackerScreenY
                << ",\"distance\":" << target.distance << ",\"direction\":\""
                << getDirectionName(static_cast<Direction>(target.direction))
                << "\",\"selected\":" << (target.selected ? "true" : "false") << '}';
        }
        out << "]}";

        out << ",\"mapPins\":[";
        for (int i = 0; i < zu4_mobile_map_pin_count(); ++i) {
            Zu4MobileMapPin pin = {};
            if (!zu4_mobile_map_pin_at(i, &pin)) continue;
            if (i) out << ',';
            out << "{\"x\":" << pin.x << ",\"y\":" << pin.y
                << ",\"label\":\"" << jsonEscape(pin.label) << "\"}";
        }
        out << "],\"mapDiscoveries\":[";
        bool discoveryComma = false;
        for (int i = 0; i < zu4_mobile_map_discovery_count(); ++i) {
            Zu4MobileMapDiscovery place = {};
            if (!zu4_mobile_map_discovery_at(i, &place)) continue;
            if (discoveryComma) out << ',';
            discoveryComma = true;
            out << "{\"x\":" << place.x << ",\"y\":" << place.y
                << ",\"category\":" << place.category << ",\"name\":\""
                << jsonEscape(place.name) << "\"}";
        }
        out << ']';

        out << ",\"party\":[";
        for (int i = 0; i < c->party->size(); ++i) {
            PartyMember *member = c->party->member(i);
            if (i) out << ',';
            out << "{\"name\":\"" << jsonEscape(member->getName())
                << "\",\"class\":\"" << getClassName(member->getClass())
                << "\",\"level\":" << member->getRealLevel()
                << ",\"status\":\"" << statusName(member->getStatus())
                << "\",\"hp\":" << member->getHp()
                << ",\"maxHp\":" << member->getMaxHp()
                << ",\"mp\":" << member->getMp()
                << ",\"maxMp\":" << member->getMaxMp()
                << ",\"xp\":" << member->getExp()
                << ",\"weapon\":\"" << jsonEscape(member->getWeapon()->name)
                << "\",\"weaponType\":" << member->getWeapon()->type
                << ",\"armor\":\"" << jsonEscape(member->getArmor()->name)
                << "\",\"armorType\":" << member->getArmor()->type
                << ",\"canEquipWeapon\":" << ((!webInteractionDepth() && (owner == game || (combat && member == combat->getCurrentPlayer()))) ? "true" : "false")
                << ",\"canEquipArmor\":" << (!webInteractionDepth() && owner == game ? "true" : "false")
                << ",\"weaponChoices\":[";
            bool choiceComma = false;
            for (int type = WEAP_HANDS; type < WEAP_MAX; ++type) {
                if (type != member->getWeapon()->type && type != WEAP_HANDS && c->saveGame->weapons[type] < 1) continue;
                if (!zu4_weapon_usable(static_cast<WeaponType>(type), member->getClass())) continue;
                if (choiceComma) out << ',';
                choiceComma = true;
                out << type;
            }
            out << "],\"armorChoices\":[";
            choiceComma = false;
            for (int type = ARMR_NONE; type < ARMR_MAX; ++type) {
                if (type != member->getArmor()->type && type != ARMR_NONE && c->saveGame->armor[type] < 1) continue;
                if (!zu4_armor_wearable(static_cast<ArmorType>(type), member->getClass())) continue;
                if (choiceComma) out << ',';
                choiceComma = true;
                out << type;
            }
            out << ']'
                << ",\"active\":" << ((combat ? combat->getCurrentPlayer() == member
                                                : c->party->getActivePlayer() == i) ? "true" : "false") << '}';
        }
        out << ']';

        out << ",\"inventory\":{\"weapons\":[";
        for (int type = WEAP_HANDS; type < WEAP_MAX; ++type) {
            if (type) out << ',';
            out << "{\"type\":" << type << ",\"name\":\""
                << jsonEscape(zu4_weapon_name(static_cast<WeaponType>(type)))
                << "\",\"count\":" << c->saveGame->weapons[type] << '}';
        }
        out << "],\"armor\":[";
        for (int type = ARMR_NONE; type < ARMR_MAX; ++type) {
            if (type) out << ',';
            out << "{\"type\":" << type << ",\"name\":\""
                << jsonEscape(zu4_armor_name(static_cast<ArmorType>(type)))
                << "\",\"count\":" << c->saveGame->armor[type] << '}';
        }
        out << "]}";

        out << ",\"spells\":[";
        const int caster = std::max(0, c->party->getActivePlayer());
        for (int i = 0; i < SPELL_MAX; ++i) {
            if (i) out << ',';
            const SpellCastError error = spellCheckPrerequisites(i, caster);
            out << "{\"letter\":\"" << static_cast<char>('A' + i)
                << "\",\"name\":\"" << jsonEscape(spellGetName(i))
                << "\",\"mixtures\":" << c->saveGame->mixtures[i]
                << ",\"mp\":" << spellGetRequiredMP(i)
                << ",\"available\":" << (error == CASTERR_NOERROR ? "true" : "false") << '}';
        }
        out << ']';
    }

    out << ",\"messages\":[";
    for (size_t i = 0; i < messages.size(); ++i) {
        if (i) out << ',';
        out << '\"' << jsonEscape(messages[i]) << '\"';
    }
    out << "],\"events\":[";
    for (size_t i=0;i<recentEvents.size();++i) {
        if(i)out << ',';out << '"' << jsonEscape(recentEvents[i]) << '"';
    }
    out << "]}";
    snapshot = out.str();
    return snapshot.c_str();
}
