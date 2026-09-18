#include "mobile_dungeon_sight.h"

#include <cassert>
#include <vector>

static std::size_t cell(int x, int y) {
    return (std::size_t)y * 8 + x;
}

int main() {
    // Deceit L2 literal edge case: from row 8, looking south must reveal the
    // blocking wall in row 1. A north-side blocker isolates the south ray so
    // cells beyond the wrapped wall remain unseen.
    std::vector<unsigned char> blockers(64, 0);
    blockers[cell(1, 0)] = 1;
    blockers[cell(1, 6)] = 1;
    std::vector<unsigned char> visible =
        mobileDungeonCardinalSight(8, 8, 1, 7, blockers);
    assert(visible.size() == 64);
    assert(visible[cell(1, 7)]);
    assert(visible[cell(1, 0)]);
    assert(!visible[cell(1, 1)]);
    assert(visible[cell(1, 6)]);

    // The equivalent horizontal seam follows the same fixed-board topology.
    blockers.assign(64, 0);
    blockers[cell(0, 3)] = 1;
    blockers[cell(6, 3)] = 1;
    visible = mobileDungeonCardinalSight(8, 8, 7, 3, blockers);
    assert(visible[cell(0, 3)]);
    assert(!visible[cell(1, 3)]);
    assert(visible[cell(6, 3)]);

    // With no blockers, a wrapping cardinal line reveals every cell in its
    // row and column exactly once, but no off-axis cells.
    blockers.assign(64, 0);
    visible = mobileDungeonCardinalSight(8, 8, 4, 4, blockers);
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x)
            assert((visible[cell(x, y)] != 0) == (x == 4 || y == 4));

    assert(mobileDungeonCardinalSight(0, 8, 0, 0, blockers).empty());
    assert(mobileDungeonCardinalSight(8, 8, -1, 0, blockers).empty());
    blockers.pop_back();
    assert(mobileDungeonCardinalSight(8, 8, 0, 0, blockers).empty());
    return 0;
}
