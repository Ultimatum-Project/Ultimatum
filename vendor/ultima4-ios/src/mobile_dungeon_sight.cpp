#include "mobile_dungeon_sight.h"

#include <cstddef>

std::vector<unsigned char> mobileDungeonCardinalSight(
    int width, int height, int partyX, int partyY,
    const std::vector<unsigned char> &blocksSight) {
    if (width <= 0 || height <= 0 || partyX < 0 || partyY < 0 ||
        partyX >= width || partyY >= height)
        return {};
    std::size_t size = (std::size_t)width * height;
    if (blocksSight.size() != size)
        return {};

    std::vector<unsigned char> visible(size, 0);
    visible[(std::size_t)partyY * width + partyX] = 1;
    auto castRay = [&](int stepX, int stepY) {
        int x = partyX;
        int y = partyY;
        int limit = stepX ? width - 1 : height - 1;
        for (int distance = 0; distance < limit; ++distance) {
            x = (x + stepX + width) % width;
            y = (y + stepY + height) % height;
            if (x == partyX && y == partyY)
                break;
            std::size_t index = (std::size_t)y * width + x;
            visible[index] = 1;
            if (blocksSight[index])
                break;
        }
    };
    castRay(-1, 0);
    castRay(1, 0);
    castRay(0, -1);
    castRay(0, 1);
    return visible;
}
