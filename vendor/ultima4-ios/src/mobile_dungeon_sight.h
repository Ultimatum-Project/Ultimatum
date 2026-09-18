#ifndef ZU4_MOBILE_DUNGEON_SIGHT_H
#define ZU4_MOBILE_DUNGEON_SIGHT_H

#include <vector>

// Return a row-major visibility mask for four cardinal rays on a wrapping
// dungeon floor. A blocking cell is visible, and terminates only that ray.
std::vector<unsigned char> mobileDungeonCardinalSight(
    int width, int height, int partyX, int partyY,
    const std::vector<unsigned char> &blocksSight);

#endif
