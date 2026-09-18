#include "mobile_dungeon_exploration.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <fstream>
#include <set>
#include <tuple>

namespace {
bool validCell(int x, int y, int level, int width, int height, int levels) {
    return width > 0 && height > 0 && levels > 0 &&
        x >= 0 && y >= 0 && level >= 0 &&
        x < width && y < height && level < levels;
}

const MobileDungeonExploration::Shape *shapeFor(
    const std::vector<MobileDungeonExploration::Shape> &shapes, int mapId) {
    auto found = std::find_if(shapes.begin(), shapes.end(), [&](const auto &shape) {
        return shape.mapId == mapId;
    });
    return found == shapes.end() ? nullptr : &*found;
}

bool validShapes(const std::vector<MobileDungeonExploration::Shape> &shapes) {
    std::set<int> ids;
    std::size_t capacity = 0;
    for (const auto &shape : shapes) {
        if (shape.mapId < 0 || shape.width <= 0 || shape.height <= 0 || shape.levels <= 0 ||
            shape.width > 256 || shape.height > 256 || shape.levels > 64 ||
            !ids.insert(shape.mapId).second) return false;
        capacity += (std::size_t)shape.width * shape.height * shape.levels;
        if (capacity > MobileDungeonExploration::MAX_CELLS) return false;
    }
    return !shapes.empty();
}
}

bool MobileDungeonExploration::reveal(int mapId, int x, int y, int level,
                                      int width, int height, int levels) {
    if (mapId < 0 || !validCell(x, y, level, width, height, levels)) return false;
    if (isRevealed(mapId, x, y, level)) return true;
    if (cells.size() >= MAX_CELLS) return false;
    cells.push_back({mapId, x, y, level});
    return true;
}

bool MobileDungeonExploration::isRevealed(int mapId, int x, int y, int level) const {
    return std::any_of(cells.begin(), cells.end(), [&](const Cell &cell) {
        return cell.mapId == mapId && cell.x == x && cell.y == y && cell.level == level;
    });
}

bool MobileDungeonExploration::save(const std::string &path) const {
    const std::string temporary = path + ".tmp";
    std::ofstream output(temporary, std::ios::trunc);
    if (!output) return false;
    output << "U4-DUNGEON-EXPLORATION 1\n";
    for (const Cell &cell : cells)
        output << cell.mapId << ' ' << cell.x << ' ' << cell.y << ' ' << cell.level << '\n';
    output.flush();
    if (!output) {
        output.close();
        std::remove(temporary.c_str());
        return false;
    }
    output.close();
    if (!output || std::rename(temporary.c_str(), path.c_str()) != 0) {
        std::remove(temporary.c_str());
        return false;
    }
    return true;
}

bool MobileDungeonExploration::load(const std::string &path,
                                    const std::vector<Shape> &shapes) {
    if (!validShapes(shapes)) return false;
    errno = 0;
    std::ifstream input(path);
    if (!input) {
        if (errno == ENOENT) {
            cells.clear();
            return true;
        }
        return false;
    }
    std::string header;
    if (!std::getline(input, header) || header != "U4-DUNGEON-EXPLORATION 1") return false;
    std::vector<Cell> loaded;
    std::set<std::tuple<int, int, int, int>> unique;
    while (true) {
        input >> std::ws;
        if (input.eof()) break;
        Cell cell = {};
        if (!(input >> cell.mapId >> cell.x >> cell.y >> cell.level)) return false;
        const Shape *shape = shapeFor(shapes, cell.mapId);
        if (!shape || !validCell(cell.x, cell.y, cell.level,
                                 shape->width, shape->height, shape->levels) ||
            loaded.size() >= MAX_CELLS ||
            !unique.insert(std::make_tuple(cell.mapId, cell.x, cell.y, cell.level)).second)
            return false;
        loaded.push_back(cell);
    }
    if (input.bad()) return false;
    cells.swap(loaded);
    return true;
}
