#ifndef ZU4_MOBILE_COMBAT_H
#define ZU4_MOBILE_COMBAT_H

#include <cstdint>

namespace MobileCombat {
// Copies/clones have distinct identities; assignment preserves the destination
// identity. Never use an allocator address or a tile as a remembered enemy.
struct Identity {
    static uint64_t next() { static uint64_t serial = 0; return ++serial; }
    uint64_t value = next();
    Identity() = default;
    Identity(const Identity &) : value(next()) {}
    Identity &operator=(const Identity &) { return *this; }
};

struct LastAttack {
    uint64_t enemy = 0;
    const void *weapon = nullptr;
    void clear() { enemy = 0; weapon = nullptr; }
    bool matches(uint64_t candidate, const void *currentWeapon) const {
        return enemy != 0 && enemy == candidate && weapon == currentWeapon;
    }
};

// Keep a visible flash even in Fast mode. These are presentation delays only;
// no controller cycles, random calls, or game-state updates are added/removed.
inline unsigned flashMilliseconds(int frames, bool fast) {
    unsigned standard = frames > 0 ? (unsigned)frames * 33 : 0;
    return fast && standard > 33 ? (standard / 2 < 33 ? 33 : standard / 2) : standard;
}
inline unsigned roundPauseMilliseconds(bool fast) { return fast ? 25 : 50; }
}
#endif
