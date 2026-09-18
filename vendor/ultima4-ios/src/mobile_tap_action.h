#ifndef MOBILE_TAP_ACTION_H
#define MOBILE_TAP_ACTION_H

#include <vector>

#include "direction.h"
#include "mobile_pathfinding.h"

namespace MobileTap {

enum Target {
    NONE,
    PERSON,
    UNLOCKED_DOOR,
    LOCKED_DOOR,
    CHEST
};

/*
 * Return the tiles from which a tapped target can be used. People and doors
 * are used cardinally adjacent. A person can additionally be addressed from
 * two tiles away when the intervening tile has the engine's talk-over rule.
 * Chests retain the original rule that the Avatar stands on the chest tile.
 */
inline std::vector<MobilePathfinding::Point> approachPoints(
    Target target, MobilePathfinding::Point tapped, int talkOverDirections = 0) {
    std::vector<MobilePathfinding::Point> points;
    if (target == NONE) return points;
    if (target == CHEST) {
        points.push_back(tapped);
        return points;
    }

    const Direction directions[] = {DIR_WEST, DIR_NORTH, DIR_EAST, DIR_SOUTH};
    for (Direction direction : directions) {
        int dx = direction == DIR_EAST ? 1 : direction == DIR_WEST ? -1 : 0;
        int dy = direction == DIR_SOUTH ? 1 : direction == DIR_NORTH ? -1 : 0;
        points.push_back({tapped.x + dx, tapped.y + dy});
        if (target == PERSON && DIR_IN_MASK(direction, talkOverDirections))
            points.push_back({tapped.x + 2 * dx, tapped.y + 2 * dy});
    }
    return points;
}

} // namespace MobileTap

#endif
