#ifndef ZU4_SAVE_VALIDATION_H
#define ZU4_SAVE_VALIDATION_H
#include "savegame.h"
#include <string>
#include <cstring>

namespace SaveValidation {
inline bool characters(const SaveGame &save) {
    if (save.members < 1 || save.members > 8) return false;
    for (unsigned i = 0; i < save.members; ++i) {
        const SaveGamePlayerRecord &player = save.players[i];
        if (!std::memchr(player.name, '\0', sizeof(player.name))) return false;
        if (player.klass < CLASS_MAGE || player.klass > CLASS_SHEPHERD ||
            player.weapon < 0 || player.weapon >= WEAP_MAX ||
            player.armor < 0 || player.armor >= ARMR_MAX) return false;
    }
    return true;
}
inline bool party(const std::string &path) {
    FILE *file = fopen(path.c_str(), "rb");
    if (!file) return false;
    SaveGame save = {};
    bool ok = saveGameRead(&save, file) != 0 && characters(save);
    if (ok) ok = fgetc(file) == EOF && !ferror(file);
    if (fclose(file) != 0) ok = false;
    return ok;
}
inline bool creatures(const std::string &path) {
    FILE *file = fopen(path.c_str(), "rb");
    if (!file) return false;
    SaveGameMonsterRecord records[MONSTERTABLE_SIZE] = {};
    bool ok = saveGameMonstersRead(records, file) != 0;
    if (ok) ok = fgetc(file) == EOF && !ferror(file);
    if (fclose(file) != 0) ok = false;
    return ok;
}
inline bool checkpointFiles(const std::string &directory) {
    if (!party(directory + "party.sav") || !creatures(directory + "monsters.sav")) return false;
    SaveGame save = {};
    FILE *file = fopen((directory + "party.sav").c_str(), "rb");
    if (!file) return false;
    bool ok = saveGameRead(&save, file) != 0;
    if (fclose(file) != 0) ok = false;
    if (!ok) return false;
    if (save.location == 0) return true;
    // Original U4 dungeon maps 17–24 are eight 8x8 levels.
    if (save.location < 17 || save.location > 24 || save.x >= 8 || save.y >= 8 || save.dnglevel >= 8) return false;
    if (!creatures(directory + "outmonst.sav")) return false;
    file = fopen((directory + "dngmap.sav").c_str(), "rb");
    if (!file) return false;
    unsigned char terrain[512];
    ok = fread(terrain, 1, sizeof(terrain), file) == sizeof(terrain) && fgetc(file) == EOF && !ferror(file);
    if (fclose(file) != 0) ok = false;
    return ok;
}

}
#endif
