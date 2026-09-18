#ifndef MOBILE_ADJACENT_ACTION_H
#define MOBILE_ADJACENT_ACTION_H

namespace MobileAdjacent {

enum Target {
    NO_TARGET,
    CONVERSABLE_PERSON,
    HOSTILE_PERSON,
    UNLOCKED_DOOR,
    LOCKED_DOOR
};

enum Action { NONE, TALK, OPEN, LOCKED_NOTICE };

struct State {
    bool enabled;
    bool ordinaryExploration;
    bool deliberateInput;
    bool collisionOverride;
    bool footOrHorse;
    Target target;
};

inline Action resolve(const State &state) {
    if (!state.enabled || !state.ordinaryExploration || !state.deliberateInput ||
        state.collisionOverride || !state.footOrHorse)
        return NONE;
    if (state.target == CONVERSABLE_PERSON) return TALK;
    if (state.target == UNLOCKED_DOOR) return OPEN;
    if (state.target == LOCKED_DOOR) return LOCKED_NOTICE;
    return NONE;
}

} // namespace MobileAdjacent

#endif
