#ifndef MOBILE_PATHFINDING_H
#define MOBILE_PATHFINDING_H

#include <algorithm>
#include <cstddef>
#include <queue>
#include <vector>

#include "direction.h"

namespace MobilePathfinding {

struct Point {
    int x;
    int y;
};

struct Route {
    bool found = false;
    Point target = {};
    std::vector<Direction> steps;
};

/*
 * Find a short cardinal route through the currently visible world grid.
 * validMoves contains the engine's direction mask for every cell, while
 * hazardous marks destinations that automatic movement must never enter.
 */
inline std::vector<Direction> shortestPath(int width, int height,
                                           Point start, Point target,
                                           const std::vector<int> &validMoves,
                                           const std::vector<unsigned char> &hazardous,
                                           int maxSteps) {
    std::vector<Direction> empty;
    if (width <= 0 || height <= 0 || maxSteps <= 0 ||
        start.x < 0 || start.x >= width || start.y < 0 || start.y >= height ||
        target.x < 0 || target.x >= width || target.y < 0 || target.y >= height)
        return empty;
    const std::size_t count = (std::size_t)width * height;
    if (validMoves.size() != count || hazardous.size() != count) return empty;

    const int startIndex = start.y * width + start.x;
    const int targetIndex = target.y * width + target.x;
    if (startIndex == targetIndex || hazardous[targetIndex]) return empty;

    std::vector<int> previous(count, -1);
    std::vector<Direction> arrivedBy(count, DIR_NONE);
    std::vector<int> distance(count, -1);
    std::queue<int> pending;
    distance[startIndex] = 0;
    pending.push(startIndex);

    const Direction directions[] = {DIR_WEST, DIR_NORTH, DIR_EAST, DIR_SOUTH};
    while (!pending.empty()) {
        int current = pending.front();
        pending.pop();
        if (distance[current] >= maxSteps) continue;
        int x = current % width;
        int y = current / width;
        for (Direction direction : directions) {
            if (!DIR_IN_MASK(direction, validMoves[current])) continue;
            int nx = x + (direction == DIR_EAST ? 1 : direction == DIR_WEST ? -1 : 0);
            int ny = y + (direction == DIR_SOUTH ? 1 : direction == DIR_NORTH ? -1 : 0);
            if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;
            int next = ny * width + nx;
            if (distance[next] >= 0 || hazardous[next]) continue;
            distance[next] = distance[current] + 1;
            previous[next] = current;
            arrivedBy[next] = direction;
            if (next == targetIndex) {
                std::vector<Direction> path;
                for (int at = next; at != startIndex; at = previous[at])
                    path.push_back(arrivedBy[at]);
                std::reverse(path.begin(), path.end());
                return path;
            }
            pending.push(next);
        }
    }
    return empty;
}

/* Choose the shortest reachable destination from an ordered set of goals. */
inline Route shortestPathToAny(int width, int height, Point start,
                               const std::vector<Point> &targets,
                               const std::vector<int> &validMoves,
                               const std::vector<unsigned char> &hazardous,
                               int maxSteps) {
    Route best;
    if (width <= 0 || height <= 0 || maxSteps < 0 ||
        start.x < 0 || start.x >= width || start.y < 0 || start.y >= height ||
        validMoves.size() != (std::size_t)width * height ||
        hazardous.size() != (std::size_t)width * height)
        return best;

    for (Point target : targets) {
        if (target.x < 0 || target.x >= width || target.y < 0 || target.y >= height ||
            hazardous[target.y * width + target.x])
            continue;
        if (target.x == start.x && target.y == start.y) {
            best.found = true;
            best.target = target;
            best.steps.clear();
            return best;
        }
        std::vector<Direction> path = shortestPath(
            width, height, start, target, validMoves, hazardous, maxSteps);
        if (!path.empty() && (!best.found || path.size() < best.steps.size())) {
            best.found = true;
            best.target = target;
            best.steps = path;
        }
    }
    return best;
}

} // namespace MobilePathfinding

#endif
