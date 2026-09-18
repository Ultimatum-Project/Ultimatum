#ifndef MOBILE_CONTEXT_ACTION_H
#define MOBILE_CONTEXT_ACTION_H

namespace MobileContext {

enum Transport {
    FOOT,
    HORSE,
    SHIP,
    BALLOON
};

enum Action {
    NONE,
    OPEN_CHEST,
    ENTER,
    CLIMB,
    DESCEND,
    BOARD,
    DISMOUNT,
    DISEMBARK,
    ASCEND,
    LAND
};

struct State {
    Transport transport;
    bool flying;
    bool boardableHere;
    bool chestHere;
    bool enterPortal;
    bool climbPortal;
    bool descendPortal;
    bool dungeon;
    bool ladderUp;
    bool ladderDown;
};

inline Action resolve(const State &state) {
    // Match the classic smart-action precedence where it is predictable, but
    // deliberately omit its turn-consuming Search fallback.
    if (state.chestHere && !state.flying) return OPEN_CHEST;
    if (state.enterPortal) return ENTER;
    if (state.dungeon) {
        // Preserve the engine's established choice when a ladder supports both
        // directions. A visible chooser can replace this in a later slice.
        if (state.ladderUp) return CLIMB;
        if (state.ladderDown) return DESCEND;
    }
    if (state.climbPortal) return CLIMB;
    if (state.descendPortal) return DESCEND;
    if (state.transport == FOOT && state.boardableHere) return BOARD;
    if (state.transport == BALLOON) return state.flying ? LAND : ASCEND;
    if (state.transport == HORSE) return DISMOUNT;
    if (state.transport == SHIP) return DISEMBARK;
    return NONE;
}

inline const char *label(Action action) {
    switch (action) {
    case OPEN_CHEST: return "Open Chest";
    case ENTER: return "Enter";
    case CLIMB: return "Climb";
    case DESCEND: return "Descend";
    case BOARD: return "Board";
    case DISMOUNT: return "Dismount";
    case DISEMBARK: return "Disembark";
    case ASCEND: return "Ascend";
    case LAND: return "Land";
    case NONE: default: return "Interact";
    }
}

inline int command(Action action) {
    switch (action) {
    case OPEN_CHEST: return 'g';
    case ENTER: return 'e';
    case CLIMB: return 'k';
    case DESCEND: return 'd';
    case BOARD: return 'b';
    case DISMOUNT:
    case DISEMBARK: return 'x';
    case ASCEND: return 'k';
    case LAND: return 'd';
    case NONE: default: return 0;
    }
}

} // namespace MobileContext

#endif
