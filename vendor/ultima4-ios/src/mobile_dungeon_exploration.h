#ifndef ZU4_MOBILE_DUNGEON_EXPLORATION_H
#define ZU4_MOBILE_DUNGEON_EXPLORATION_H

#include <cstddef>
#include <string>
#include <vector>

class MobileDungeonExploration {
public:
    struct Shape {
        int mapId;
        int width;
        int height;
        int levels;
    };

    struct Cell {
        int mapId;
        int x;
        int y;
        int level;
    };

    static const std::size_t MAX_CELLS = 8192;

    const std::vector<Cell> &all() const { return cells; }
    bool reveal(int mapId, int x, int y, int level,
                int width, int height, int levels);
    bool isRevealed(int mapId, int x, int y, int level) const;
    bool save(const std::string &path) const;
    // Missing metadata is a valid empty state for checkpoints created before
    // dungeon exploration maps. Invalid input leaves live exploration intact.
    bool load(const std::string &path, const std::vector<Shape> &shapes);
    void clear() { cells.clear(); }

private:
    std::vector<Cell> cells;
};

#endif
