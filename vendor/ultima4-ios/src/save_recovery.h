#ifndef ZU4_SAVE_RECOVERY_H
#define ZU4_SAVE_RECOVERY_H

// Recovery routing is deliberately a pure policy. File parsing and UI remain
// at the call site, while every missing/corrupt pointer combination can be
// covered by a small portable test.
namespace SaveRecovery {

enum Plan {
    USE_CURRENT,
    USE_LEGACY,
    OFFER_PREVIOUS,
    UNAVAILABLE
};

inline Plan plan(bool currentPointerExists, bool previousPointerExists,
                 bool currentValid, bool previousValid) {
    if (currentValid) return USE_CURRENT;
    if (!currentPointerExists && !previousPointerExists) return USE_LEGACY;
    if (previousValid) return OFFER_PREVIOUS;
    return UNAVAILABLE;
}

} // namespace SaveRecovery

#endif
