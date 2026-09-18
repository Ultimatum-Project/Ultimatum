#ifndef ZU4_SAVE_SLOTS_H
#define ZU4_SAVE_SLOTS_H

#include "save_snapshot.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

// Device-wide selection of one of three independent adventure save roots.
// Slot 1 deliberately retains the original snapshots path so existing iOS
// adventures require no migration. Slots 2 and 3 use sibling roots.
namespace SaveSlots {

static const int SLOT_COUNT = 3;

inline bool valid(int slot) { return slot >= 1 && slot <= SLOT_COUNT; }

inline std::string pointerPath(const std::string &base) {
    return base + "ACTIVE_SAVE_SLOT";
}

inline int active(const std::string &base) {
    FILE *file = fopen(pointerPath(base).c_str(), "rb");
    if (!file) return 1;
    char buffer[4] = {};
    std::size_t count = fread(buffer, 1, sizeof(buffer), file);
    bool ok = !ferror(file) && count == 2 && buffer[1] == '\n' &&
        buffer[0] >= '1' && buffer[0] <= ('0' + SLOT_COUNT);
    if (fclose(file) != 0) ok = false;
    return ok ? buffer[0] - '0' : 1;
}

inline bool select(const std::string &base, int slot) {
    if (!valid(slot)) return false;
    std::string pattern = base + "save-slot-pointer-XXXXXX";
    std::vector<char> temporary(pattern.begin(), pattern.end());
    temporary.push_back(0);
    int fd = mkstemp(temporary.data());
    if (fd < 0) return false;
    char contents[2] = {static_cast<char>('0' + slot), '\n'};
    bool ok = write(fd, contents, sizeof(contents)) == (ssize_t)sizeof(contents);
    if (ok) ok = fsync(fd) == 0;
    if (close(fd) != 0) ok = false;
    if (ok) ok = rename(temporary.data(), pointerPath(base).c_str()) == 0;
    if (!ok) {
        unlink(temporary.data());
        return false;
    }
    return SaveSnapshot::syncDirectory(base);
}

inline std::string snapshotRoot(const std::string &base, int slot) {
    if (!valid(slot)) slot = 1;
    if (slot == 1) return base + "snapshots";
    return base + "snapshots-slot-" + std::to_string(slot);
}

inline std::string fallbackDirectory(const std::string &base, int slot) {
    if (!valid(slot) || slot == 1) return base;
    // Empty secondary slots must never fall through to Slot 1's legacy files.
    return base + "empty-slot-" + std::to_string(slot) + "/";
}

inline bool occupied(const std::string &base, int slot) {
    if (!valid(slot)) return false;
    std::string root = snapshotRoot(base, slot);
    if (access((root + "/CURRENT").c_str(), F_OK) == 0 ||
        access((root + "/PREVIOUS").c_str(), F_OK) == 0) return true;
    return slot == 1 && access((base + "party.sav").c_str(), F_OK) == 0;
}

} // namespace SaveSlots

#endif
