// Web presentation adapter. Game legality, item effects and turn accounting
// remain in the existing engine; menu replies use the SDL-owned prompt loop.
#include "web_menu.h"
#include "web_prompt.h"
#include "cheat.h"
#include "combat.h"
#include "context.h"
#include "experience_settings.h"
#include "game.h"
#include "item.h"
#include "mapmgr.h"
#include "names.h"
#include "player.h"
#include "portal.h"
#include "screen.h"
#include "settings.h"
#include "soundtrack.h"
#include "music.h"
#include "sound.h"
#include "u4.h"
#include "u4file.h"
#include "miniz.h"
#include <ctime>
#include <fstream>
#include <sys/stat.h>

extern "C" void zu4_mobile_cancel_walk();
int gameSave();
void gameDestroyAllCreatures();

namespace {
int storageRevision = 0, libraryRequest = 0, adventureRequest = 0, saveRevision = 0;
bool saveAutomatic = false;
int recoveryDownloadRequest = 0;
std::vector<unsigned char> recoveryZip;
std::vector<unsigned char> gemPixels;
bool gemActive = false;
int gemWidth = 0, gemHeight = 0;
using Options = std::vector<WebPromptOption>;

std::string choose(const std::string &title, const std::string &text,
                   Options options, const char *back = "Back") {
    options.push_back({"\033", back});
    return webReadPrompt("menu", title, text, std::move(options));
}

void message(const std::string &title, const std::string &text) {
    choose(title, text, {}, "Back");
}

bool confirm(const std::string &title, const std::string &text,
             const char *apply = "Apply") {
    return choose(title, text, {{"apply", apply}}, "Cancel") == "apply";
}

std::string profileLabel() {
    return std::string(zu4_experience_profile_name(zu4_experience_profile())) +
        (zu4_experience_is_customized() ? " — Customized" : "");
}

bool commitPreferences(const SettingsData &oldSettings,
                       const Zu4ExperiencePreferences &oldPreferences) {
    // Match startup's EGA fallback without erasing the desired profile theme.
    // Importing the optional upgrade can activate that theme on the next load.
    if (settings.videoType == ZU4_GRAPHICS_VGA && !u4isUpgradeAvailable()) settings.videoType = ZU4_GRAPHICS_EGA;
    if (settings.videoType != oldSettings.videoType &&
        !screenApplyVideoType(settings.videoType)) {
        settings = oldSettings;
        experiencePreferences = oldPreferences;
        return false;
    }
    if (!zu4_settings_write()) {
        const int attemptedVideo = settings.videoType;
        settings = oldSettings;
        experiencePreferences = oldPreferences;
        if (attemptedVideo != oldSettings.videoType) screenApplyVideoType(oldSettings.videoType);
        return false;
    }
    ++storageRevision;
    return true;
}

void audioMenu() {
    for (;;) {
        const Zu4Soundtrack *pack = zu4_soundtrack(settings.soundtrack);
        std::string action = choose("Audio", "Preferences apply to all saved games.", {
            {"music", "Music volume — " + std::to_string(settings.musicVol * 10) + "%"},
            {"effects", "Sound effects — " + std::to_string(settings.soundVol * 10) + "%"},
            {"credits", "Music and graphics credits"}});
        if (action.empty()) return;
        if (action == "credits") {
            message("Music and graphics credits", std::string(pack ? pack->credits : "No soundtrack loaded.") +
                "\n\nVGA artwork: Joshua Steele (Wiltshire Dragon). Ultima IV Upgrade: Ryan Wiener (Aradindae Dragon). "
                "Graphics-only overlay; original game files and saves are preserved. "
                "Pix's Ultima Patcher is not executed by this app.");
        } else if (action == "music" || action == "effects") {
            bool music = action == "music";
            int old = music ? settings.musicVol : settings.soundVol;
            Options levels;
            for (int value=0; value<=MAX_VOLUME; value+=2)
                levels.push_back({std::to_string(value), (value ? std::to_string(value*10)+"%" : "Off") +
                    std::string(value == old ? " — Selected" : "")});
            std::string selected = choose(music ? "Music volume" : "Sound effects", "Changes take effect immediately.", levels);
            if (selected.empty()) continue;
            if (selected != "0" && selected != "2" && selected != "4" && selected != "6" && selected != "8" && selected != "10") continue;
            int value = std::stoi(selected);
            if (music) settings.musicVol=value; else settings.soundVol=value;
            if (!zu4_settings_write()) {
                if (music) settings.musicVol=old; else settings.soundVol=old;
                message("Audio unchanged", "The preference could not be saved."); continue;
            }
            if (music) { zu4_music_vol(value/(double)MAX_VOLUME); zu4_music_set_enabled(value>0); }
            else zu4_snd_vol(value/(double)MAX_VOLUME);
            ++storageRevision;
        }
    }
}

void experienceMenu() {
    for (;;) {
        Options options = {{"profile", "Choose profile"}, {"customize", "Customize"}, {"audio", "Audio"}};
        if (zu4_experience_is_customized()) options.push_back({"restore", "Restore Profile Defaults"});
        std::string action = choose("Experience", profileLabel() +
            "\n\nProfiles change presentation and assistance, not game rules. "
            "Exploration maps and optional player pins are shared across web and iOS." +
            std::string(u4isUpgradeAvailable() ? "" : "\nGraphics remain EGA until upgrade files are imported."), options);
        if (action.empty()) return;
        if (action == "audio") { audioMenu(); continue; }
        const SettingsData oldSettings = settings;
        const Zu4ExperiencePreferences oldPreferences = experiencePreferences;
        bool changed = false;
        if (action == "profile") {
            Options profiles;
            for (int i = 0; i < 3; ++i) {
                auto profile = static_cast<Zu4ExperienceProfile>(i);
                profiles.push_back({zu4_experience_profile_key(profile),
                    std::string(zu4_experience_profile_name(profile)) +
                    (profile == zu4_experience_profile() ? " — Selected" : "")});
            }
            std::string key = choose("Choose profile", "Classic preserves original information boundaries.\n"
                "Ultimatum provides mobile conveniences.\nAssisted adds optional guidance.", profiles);
            Zu4ExperienceProfile profile;
            if (!zu4_experience_profile_from_key(key.c_str(), &profile)) continue;
            if (!confirm("Apply profile?", std::string("Apply ") + zu4_experience_profile_name(profile) +
                "?\n\nIndividual Experience overrides will be cleared. "
                "Your adventure and Controls preferences are unchanged.")) continue;
            changed = zu4_experience_set_profile(profile, &settings);
        } else if (action == "restore") {
            if (!confirm("Restore Profile Defaults?", "Clear individual Experience overrides. "
                "Your adventure and Controls preferences are unchanged.", "Restore defaults")) continue;
            zu4_experience_restore_defaults(&settings);
            changed = true;
        } else if (action == "customize") {
            std::string feature = choose("Customize Experience", profileLabel(), {
                {"graphics", std::string("Graphics — ") + (settings.videoType ? "VGA" : "EGA")},
                {"movement", std::string("Movement messages — ") + (settings.filterMoveMessages ? "Filtered" : "All shown")},
                {"map", std::string("Exploration map — ") + (zu4_experience_exploration_map() ? "On" : "Off")},
                {"pins", std::string("Player map pins — ") + (zu4_experience_map_pins() ? "On" : "Off")}});
            if (feature == "graphics") {
                std::string value = choose("Graphics", "Changes take effect immediately.", {
                    {"ega", "EGA — Original graphics"},
                    {"vga", u4isUpgradeAvailable() ? "VGA — 256-color upgrade" : "VGA — Import upgrade files first", u4isUpgradeAvailable() != 0}});
                if (value.empty()) continue;
                changed = zu4_experience_set_graphics_theme(value == "vga" ? ZU4_GRAPHICS_VGA : ZU4_GRAPHICS_EGA, &settings);
            } else if (feature == "movement") {
                std::string value = choose("Movement messages", "Filtering hides routine direction and blocked-movement text. "
                    "It does not alter turns or movement rules.", {{"filtered", "Filtered"}, {"all", "All shown"}});
                if (value.empty()) continue;
                changed = zu4_experience_set_filter_movement_messages(value == "filtered", &settings);
            } else if (feature == "map") {
                const bool enabled = zu4_experience_exploration_map();
                std::string value = choose("Exploration map", "Tracks only terrain reached in this adventure. Turning it off keeps exploration history for later.",
                    {{"on", std::string(enabled ? "✓ " : "") + "On"}, {"off", std::string(!enabled ? "✓ " : "") + "Off"}});
                if (value.empty()) continue;
                changed = zu4_experience_set_exploration_map(value == "on", &settings);
            } else if (feature == "pins") {
                const bool enabled = zu4_experience_map_pins();
                std::string value = choose("Player map pins", "Attach short labels to explored places. Turning pins off keeps existing labels for later.",
                    {{"on", std::string(enabled ? "✓ " : "") + "On"}, {"off", std::string(!enabled ? "✓ " : "") + "Off"}});
                if (value.empty()) continue;
                changed = zu4_experience_set_map_pins(value == "on", &settings);
            }
        }
        if (changed && !commitPreferences(oldSettings, oldPreferences))
            message("Experience unchanged", "That setting could not be applied. Your previous settings are still active.");
    }
}

void controlsMenu() {
    for (;;) {
        auto label = [](const char *name, bool enabled) { return std::string(name) + (enabled ? " — On" : " — Off"); };
        std::string action = choose("Controls", "D-pad, Talk, and Explore remain explicit alternatives.", {
            {"bump", label("Bump to interact", settings.bumpInteractions)},
            {"direct", label("Tap to interact", settings.directInteractions)},
            {"walk", label("Tap to walk", settings.tapToWalk)}, {"help", "How interaction works"}});
        if (action.empty()) return;
        if (action == "help") {
            message("Interaction help", "Bump: press once toward a friendly person or unlocked door. "
                "Holding a direction never repeats an interaction.\n\nTap to interact: tap a visible person, door, or chest. "
                "Tap to walk can approach it along a short safe route.\n\nTap to walk stops at hazards, obstacles, "
                "encounters and prompts. Another destination or control cancels it. "
                "Locked doors never spend a key automatically; choose Unlock under Explore.");
            continue;
        }
        const SettingsData oldSettings = settings;
        if (action == "bump") settings.bumpInteractions = !settings.bumpInteractions;
        else if (action == "direct") settings.directInteractions = !settings.directInteractions;
        else if (action == "walk") settings.tapToWalk = !settings.tapToWalk;
        if (!zu4_settings_write()) {
            settings = oldSettings;
            message("Controls unchanged", "That preference could not be saved. Your previous controls are still active.");
        } else ++storageRevision;
    }
}

#ifdef ZU4_WEB_DEBUG_TOOLS
std::string lastCheckpoint;

bool exportCheckpoint() {
    if (lastCheckpoint.empty()) return false;
    mz_zip_archive archive = {};
    if (!mz_zip_writer_init_heap(&archive, 0, 0)) return false;
    bool success = true, party = false;
    for (const char *name : {"party.sav", "monsters.sav", "outmonst.sav", "dngmap.sav"}) {
        const std::string path = lastCheckpoint + "/" + name;
        std::ifstream file(path, std::ios::binary);
        if (!file) continue;
        if (!mz_zip_writer_add_file(&archive, name, path.c_str(), nullptr, 0, MZ_DEFAULT_COMPRESSION)) success = false;
        if (std::string(name) == "party.sav") party = true;
    }
    void *data = nullptr; size_t size = 0;
    if (success && party && mz_zip_writer_finalize_heap_archive(&archive, &data, &size)) {
        const auto *bytes = static_cast<unsigned char *>(data);
        recoveryZip.assign(bytes, bytes + size);
        mz_free(data);
        ++recoveryDownloadRequest;
    } else success = false;
    mz_zip_writer_end(&archive);
    return success;
}

bool prepareDebugChange(bool &prepared, bool combat) {
    if (prepared) return true;
    const bool canSave = !combat && (c->location->context & CTX_CAN_SAVE_GAME);
    if (!confirm("Apply a test change?", canSave
        ? "A classic-save recovery snapshot will be kept before the first adventure-changing action in this Debug Tools session."
        : "Saving is unavailable here. This test change cannot create a recovery snapshot.",
        canSave ? "Create snapshot and apply" : "Apply without snapshot")) return false;
    if (canSave) {
        if (!gameSave()) { message("Test change cancelled", "The adventure could not be saved for recovery."); return false; }
        const std::string root = gameSaveDirectory() + "/web-debug-checkpoints";
        mkdir(root.c_str(), 0700);
        static int nextCheckpoint = 0;
        lastCheckpoint = root + "/" + std::to_string(std::time(nullptr)) + "-" + std::to_string(++nextCheckpoint);
        if (mkdir(lastCheckpoint.c_str(), 0700) != 0) {
            message("Test change cancelled", "The recovery directory could not be created."); return false;
        }
        bool copiedParty = false;
        for (const char *name : {"party.sav", "monsters.sav", "outmonst.sav", "dngmap.sav"}) {
            std::ifstream input(gameSaveDirectory() + "/" + name, std::ios::binary);
            if (!input) continue;
            std::ofstream output(lastCheckpoint + "/" + name, std::ios::binary);
            output << input.rdbuf();
            output.close();
            if (!output) { message("Test change cancelled", "The recovery snapshot could not be written."); return false; }
            if (std::string(name) == "party.sav") copiedParty = true;
        }
        if (!copiedParty) { message("Test change cancelled", "No recoverable party save was found."); return false; }
        ++storageRevision;
    }
    prepared = true;
    return true;
}

void cheat(int key) { CheatMenuController controller(game); controller.keyPressed(key); }
void debugKey(int key) {
    const bool oldDebug = settings.debug;
    settings.debug = true;
    game->keyPressed(key);
    settings.debug = oldDebug;
}

std::string directions(const std::string &title) {
    return choose(title, "", {{"north", "North ↑"}, {"east", "East →"}, {"south", "South ↓"}, {"west", "West ←"}}, "Cancel");
}
Direction directionOf(const std::string &value) {
    return value == "north" ? DIR_NORTH : value == "east" ? DIR_EAST : value == "south" ? DIR_SOUTH : DIR_WEST;
}

bool debugMenu(CombatController *combat) {
    bool prepared = false;
    std::string page = "root";
    for (;;) {
        bool world = c->location->map->isWorldMap(), ordinary = !combat;
        bool dungeonTeleport = world && (c->transportContext & TRANSPORT_FOOT_OR_HORSE);
        Options options;
        std::string title = "Debug Tools", detail = "Developer tools can change your adventure. "
            "Session switches reset on reload.";
        if (page == "root") options = {{"page_session", "Session"}, {"page_navigation", "Navigation"},
            {"page_party", "Party & Inventory"}, {"page_world", "World"},
            {"page_diagnostics", "Diagnostics"}, {"page_danger", "Danger Zone"}};
        else if (page == "session") {
            title = "Session";
            options = {{"collision", std::string("Walk through terrain — ") + (collisionOverride ? "On" : "Off"), ordinary},
                {"opacity", std::string("See through opaque tiles — ") + (!c->opacity ? "On" : "Off"), ordinary},
                {"peer_preview", "Preview GEM map — No gem cost", ordinary},
                {"wind_lock", std::string("Lock wind direction — ") + (c->windLock ? "On" : "Off"), ordinary}};
        } else if (page == "navigation") {
            title = "Navigation";
            options = {{"goto", "Go to location", ordinary}, {"moongate", "Go to moongate", ordinary && world},
                {"dungeon", "Go to dungeon entrance", ordinary && dungeonTeleport},
                {"altar", "Go to altar room", ordinary && world}, {"lord_british", "Return to Lord British", ordinary},
                {"exit_map", "Exit current map", ordinary && !world}};
        } else if (page == "party") {
            title = "Party & Inventory";
            options = {{"equipment", "Grant equipment"}, {"stats", "Maximize party stats"},
                {"items", "Grant quest items and supplies"}, {"reagents", "Grant 99 reagents"},
                {"mixtures", "Grant 99 spell mixtures"}, {"companions", "Recruit eligible companions"},
                {"virtues", "Complete all virtues"}};
        } else if (page == "world") {
            title = "World";
            options = {{"moons", "Advance moons", ordinary}, {"wind", "Set wind direction", ordinary},
                {"summon", "Summon creature", ordinary}, {"transport", "Create transport", ordinary && world}};
        } else if (page == "diagnostics") {
            title = "Diagnostics";
            detail = c->location->map->getName() + "\nCoordinates: " + std::to_string(c->location->coords.x) + ", " +
                std::to_string(c->location->coords.y) + ", " + std::to_string(c->location->coords.z) +
                "\nTorch duration: " + std::to_string(c->party->getTorchDuration()) +
                (lastCheckpoint.empty() ? "" : "\nRecovery snapshot: " + lastCheckpoint);
            options = {{"virtue_values", "Virtue values"},
                {"export_checkpoint", "Download recovery snapshot", !lastCheckpoint.empty()}};
        } else if (page == "danger") {
            title = "Danger Zone";
            options = {{"destroy", "Destroy nearby object", ordinary}, {"clear_creatures", "Destroy all creatures"},
                {"end_combat", "End combat immediately", combat != nullptr}, {"final_altar", "Go to the final altar", ordinary && world}};
        }
        std::string action = choose(title, detail, options, page == "root" ? "Done" : "Back to Tools");
        if (action.empty()) { if (page == "root") return false; page = "root"; continue; }
        if (action.compare(0, 5, "page_") == 0) { page = action.substr(5); continue; }
        if (action == "collision") { cheat('c'); continue; }
        if (action == "opacity") { cheat('o'); continue; }
        if (action == "wind_lock") { c->windLock = !c->windLock; continue; }
        if (action == "peer_preview") { peer(false); continue; }
        if (action == "export_checkpoint") {
            if (!exportCheckpoint()) message("Download unavailable", "The recovery snapshot could not be packaged.");
            continue;
        }
        if (action == "virtue_values") {
            std::string values;
            for (int i = 0; i < 8; ++i) values += std::string(getVirtueName(static_cast<Virtue>(i))) + " — " +
                (c->saveGame->karma[i] ? std::to_string(c->saveGame->karma[i]) : "Avatar") + "\n";
            message("Virtue values", values); continue;
        }
        if (action == "wind") {
            std::string value = directions("Set wind direction");
            if (!value.empty() && prepareDebugChange(prepared, combat != nullptr)) c->windDirection = directionOf(value);
            continue;
        }
        if (action == "summon") {
            std::string value = webReadPrompt("text", "Summon creature", "Choose or enter a creature name.", {
                {"rat", "Rat"}, {"orc", "Orc"}, {"troll", "Troll"}, {"dragon", "Dragon"},
                {"daemon", "Daemon"}, {"pirate", "Pirate"}, {"sea serpent", "Sea serpent"}, {"\033", "Cancel"}}, 32, true);
            if (!value.empty() && prepareDebugChange(prepared, combat != nullptr)) cheatSummonCreature(value);
            continue;
        }
        if (action == "transport") {
            std::string value = choose("Create transport", "", {{"h", "Horse"}, {"s", "Ship"}, {"b", "Balloon"}}, "Cancel");
            if (value.empty()) continue;
            std::string direction = directions("Place transport");
            if (!direction.empty() && prepareDebugChange(prepared, combat != nullptr)) cheatCreateTransport(value[0], directionOf(direction));
            continue;
        }
        if (action == "goto" || action == "dungeon" || action == "altar" || action == "moongate") {
            Map *map = mapMgr->get(MAP_WORLD);
            Options destinations;
            if (action == "goto") {
                std::vector<std::string> seen;
                for (size_t i = 0; i < map->portals.size(); ++i) {
                    std::string name = mapMgr->get(map->portals[i]->destid)->getName();
                    if (name.empty() || std::find(seen.begin(), seen.end(), name) != seen.end()) continue;
                    seen.push_back(name); destinations.push_back({std::to_string(i), name});
                }
            } else if (action == "dungeon") {
                for (int i = 0; i < 8; ++i) destinations.push_back({std::to_string(i), mapMgr->get(map->portals[16+i]->destid)->getName()});
            } else if (action == "altar") destinations = {{"0", "Truth"}, {"1", "Love"}, {"2", "Courage"}};
            else for (int i = 1; i <= 8; ++i) destinations.push_back({std::to_string(i), "Moongate " + std::to_string(i)});
            std::string value = choose("Choose destination", "", destinations, "Cancel");
            if (value.empty() || !prepareDebugChange(prepared, combat != nullptr)) continue;
            int index = std::stoi(value);
            if (action == "goto") {
                while (!c->location->map->isWorldMap()) if (!game->exitToParentMap()) break;
                if (c->location->map->isWorldMap()) c->location->coords = map->portals[index]->coords;
            } else if (action == "dungeon") debugKey(U4_FKEY + index);
            else if (action == "altar") debugKey(U4_FKEY + 8 + index);
            else cheat('0' + index);
            gameUpdateScreen(); continue;
        }
        bool danger = action == "destroy" || action == "clear_creatures" || action == "end_combat" || action == "final_altar";
        if (danger && !confirm("Confirm destructive test action", action == "destroy" ? "Remove an adjacent object or creature?" :
            action == "clear_creatures" ? "Remove non-protected creatures from this map?" :
            action == "end_combat" ? "End this battle immediately without adjusting virtue?" :
            "Bypass the journey and go to the final altar?", "Confirm test action")) continue;
        if (!prepareDebugChange(prepared, combat != nullptr)) continue;
        if (action == "equipment") cheat('e');
        else if (action == "stats") cheat('f');
        else if (action == "items") cheat('i');
        else if (action == "reagents") cheat('r');
        else if (action == "mixtures") cheat('m');
        else if (action == "companions") cheat('j');
        else if (action == "virtues") cheat('v');
        else if (action == "moons") cheat('a');
        else if (action == "lord_british") debugKey(U4_CTRL + 'h');
        else if (action == "exit_map") cheat('x');
        else if (action == "destroy") destroy();
        else if (action == "clear_creatures") gameDestroyAllCreatures();
        else if (action == "final_altar") debugKey(U4_ALT + 'c');
        else if (action == "end_combat" && combat) { combat->end(false); return true; }
        gameUpdateScreen();
    }
}
#endif
} // namespace

int webStorageRevision() { return storageRevision; }
int webLibraryRequest() { return libraryRequest; }
int webAdventureRequest() { return adventureRequest; }
int webSaveRevision() { return saveRevision; }
bool webSaveAutomatic() { return saveAutomatic; }
int webSaveCheckpoint(bool automatic) {
    if (!c || !c->party || !c->saveGame || !c->location ||
        c->party->isDead() || eventHandler->getController() != game || !(c->location->context & CTX_CAN_SAVE_GAME)) return 0;
    if (!gameSave()) return -1;
    ++saveRevision;
    saveAutomatic = automatic;
    return 1;
}
int webRecoveryDownloadRequest() { return recoveryDownloadRequest; }
const unsigned char *webRecoveryZip() { return recoveryZip.empty() ? nullptr : recoveryZip.data(); }
int webRecoveryZipSize() { return static_cast<int>(recoveryZip.size()); }
bool webDebugToolsEnabled() {
#ifdef ZU4_WEB_DEBUG_TOOLS
    return true;
#else
    return false;
#endif
}
bool webMenuAvailable() {
    if (!game || !c || !eventHandler || webInteractionDepth()) return false;
    Controller *owner = eventHandler->getController();
    return owner == game || (owner && owner->isCombatController());
}
bool webApplyGraphics(int videoType) {
    if (videoType != 0 && videoType != 1) return false;
    if (videoType && !u4isUpgradeAvailable()) return false;
    const SettingsData oldSettings = settings;
    const Zu4ExperiencePreferences oldPreferences = experiencePreferences;
    zu4_experience_set_graphics_theme(videoType ? ZU4_GRAPHICS_VGA : ZU4_GRAPHICS_EGA, &settings);
    return commitPreferences(oldSettings, oldPreferences);
}
const unsigned char *webGemMapPixels() { return gemActive ? gemPixels.data() : nullptr; }
int webGemMapWidth() { return gemWidth; }
int webGemMapHeight() { return gemHeight; }
void webShowGemMap() {
    Map *map = c->location->map;
    if (!map->width || !map->height) return;
    gemWidth = map->width; gemHeight = map->height;
    gemPixels.assign(static_cast<size_t>(gemWidth) * gemHeight * 4, 255);
    const bool dungeon = map->type == Map::DUNGEON;
    for (int y = 0; y < gemHeight; ++y) for (int x = 0; x < gemWidth; ++x) {
        const Tile *tile = map->tileTypeAt({x, y, c->location->coords.z}, WITHOUT_OBJECTS);
        std::string name = tile ? tile->getName() : "";
        int r = dungeon ? 42 : 55, g = dungeon ? 46 : 132, b = dungeon ? 50 : 55;
        if (tile && tile->isWater()) r = 24, g = 70, b = 170;
        else if (name.find("forest") != std::string::npos) r = 18, g = 82, b = 32;
        else if (name.find("mountain") != std::string::npos || name.find("wall") != std::string::npos) r = 120, g = 116, b = 112;
        else if (name.find("hill") != std::string::npos) r = 142, g = 112, b = 54;
        else if (name.find("ladder") != std::string::npos) r = 76, g = 190, b = 210;
        else if (name.find("door") != std::string::npos) r = 184, g = 126, b = 54;
        else if (name.find("lava") != std::string::npos || name.find("fire") != std::string::npos) r = 205, g = 58, b = 38;
        else if (name.find("poison") != std::string::npos) r = 74, g = 172, b = 68;
        else if (name.find("energy") != std::string::npos || name.find("magic") != std::string::npos) r = 154, g = 74, b = 205;
        else if (name.find("sleep") != std::string::npos) r = 74, g = 112, b = 190;
        else if (name.find("floor") != std::string::npos) r = 104, g = 88, b = 68;
        else if (name.find("solid") != std::string::npos || name.find("rock") != std::string::npos || name.find("column") != std::string::npos) r = 116, g = 116, b = 120;
        else if (name.find("city") != std::string::npos || name.find("town") != std::string::npos ||
                 name.find("castle") != std::string::npos || name.find("shrine") != std::string::npos || name.find("dungeon") != std::string::npos) r = 236, g = 196, b = 78;
        auto *pixel = &gemPixels[(static_cast<size_t>(y) * gemWidth + x) * 4];
        pixel[0] = r; pixel[1] = g; pixel[2] = b;
    }
    gemActive = true;
    webReadPrompt("map", "Peer map", (map->getName().empty() ? "Britannia" : map->getName()) +
        std::string(dungeon ? " — Level " + std::to_string(c->location->coords.z + 1) : "") +
        "\nThe gold marker is your party. This temporary gem view does not discover terrain.", {{"\033", "Done"}});
    gemActive = false;
}
void webOpenMenu() {
    if (!webMenuAvailable()) return;
    zu4_mobile_cancel_walk();
    CombatController *combat = dynamic_cast<CombatController *>(eventHandler->getController());
    WebInteractionScope interaction;
    for (;;) {
        const bool canSave = !combat && (c->location->context & CTX_CAN_SAVE_GAME);
        Options options;
        if (canSave) options.push_back({"save", "Save Game"});
        options.push_back({"explore", combat ? "Battle actions" : "Explore"});
        if (!combat) options.push_back({"travel", "Travel"});
        options.push_back({"experience", "Experience — " + profileLabel()});
        options.push_back({"controls", "Controls"});
        if (webDebugToolsEnabled()) options.push_back({"debug", "Debug Tools"});
        options.push_back({"library", "Game data"});
        options.push_back({"adventures", "Saved Games"});
        std::string action = choose("Menu", combat ? "Battle paused" :
            (canSave ? "Adventure paused" : "Adventure paused\nSaving is unavailable here."), options, "Resume");
        if (action.empty()) break;
        if (action == "experience") { experienceMenu(); continue; }
        if (action == "controls") { controlsMenu(); continue; }
        if (action == "library") { ++libraryRequest; break; }
        if (action == "adventures") {
            ++adventureRequest;
            // Keep ownership of the paused game while the browser dialog is open.
            Options saveOptions;
            if (canSave) saveOptions.push_back({"save", "Save Game"});
            if (choose("Saved Games", "Manage saves stored in this browser.", saveOptions, "Resume") == "save")
                webSaveCheckpoint();
            break;
        }
        if (action == "save") {
            if (webSaveCheckpoint() == 1) { ++storageRevision; break; }
            else message("Save did not finish", "Keep this tab open and try again before leaving your adventure.");
            continue;
        }
#ifdef ZU4_WEB_DEBUG_TOOLS
        if (action == "debug") { if (debugMenu(combat)) break; continue; }
#endif
        Options commands;
        if (action == "explore") {
            commands = {{"u", "Use a quest item"}, {"g", "Open a chest"}};
            if (!combat) {
                commands.insert(commands.begin(), {"s", "Search this location"});
                commands.push_back({"o", "Open a door"}); commands.push_back({"j", "Unlock a door"});
                commands.push_back({"h", "Make camp"});
                if (c->location->context & CTX_DUNGEON) commands.push_back({"i", "Light a torch"});
                if (c->saveGame->gems) commands.push_back({"p", "Peer through a gem"});
            }
        } else if (action == "travel") {
            commands = {{"k", "Climb / ascend"}, {"d", "Descend / land"}};
            if (c->transportContext == TRANSPORT_FOOT) commands.push_back({"b", "Board transport"});
            else if (!c->party->isFlying()) commands.push_back({"x", "Leave transport"});
            if (c->transportContext == TRANSPORT_SHIP) commands.push_back({"f", "Fire cannon"});
            if (c->transportContext == TRANSPORT_HORSE) commands.push_back({"y", c->horseSpeed ? "Slow horse" : "Urge horse onward"});
            if (c->saveGame->sextants) commands.push_back({"l", "Locate with sextant"});
        }
        std::string command = choose(action == "travel" ? "Travel" : "Explore", "Game actions use the original turn and resource rules.", commands);
        if (command.size() == 1) {
            if (combat) combat->notifyKeyPressed(command[0]);
            else game->notifyKeyPressed(command[0]);
            break;
        }
    }
    if (c) c->lastCommandTime = std::time(nullptr);
}
