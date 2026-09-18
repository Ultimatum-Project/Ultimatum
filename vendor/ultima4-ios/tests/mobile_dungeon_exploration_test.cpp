#include "mobile_dungeon_exploration.h"

#include <cassert>
#include <fstream>
#include <string>
#include <unistd.h>

int main() {
    const std::vector<MobileDungeonExploration::Shape> shapes = {
        {17, 8, 8, 8}, {18, 8, 8, 8}
    };
    MobileDungeonExploration explored;
    assert(explored.reveal(17, 1, 1, 0, 8, 8, 8));
    assert(explored.reveal(17, 1, 1, 0, 8, 8, 8));
    assert(explored.reveal(18, 7, 7, 7, 8, 8, 8));
    assert(explored.all().size() == 2);
    assert(explored.isRevealed(17, 1, 1, 0));
    assert(!explored.isRevealed(17, 2, 1, 0));
    assert(!explored.reveal(17, -1, 1, 0, 8, 8, 8));
    assert(!explored.reveal(17, 1, 8, 0, 8, 8, 8));
    assert(!explored.reveal(17, 1, 1, 8, 8, 8, 8));

    char directory[] = "/private/tmp/u4-dungeon-exploration-XXXXXX";
    assert(mkdtemp(directory));
    std::string path = std::string(directory) + "/explored-dungeons.dat";
    assert(explored.save(path));

    MobileDungeonExploration loaded;
    assert(loaded.load(path, shapes));
    assert(loaded.all().size() == 2);
    assert(loaded.isRevealed(17, 1, 1, 0));
    assert(loaded.isRevealed(18, 7, 7, 7));

    MobileDungeonExploration legacy;
    assert(legacy.reveal(17, 2, 2, 0, 8, 8, 8));
    assert(legacy.load(std::string(directory) + "/missing.dat", shapes));
    assert(legacy.all().empty());

    std::ofstream(path, std::ios::trunc)
        << "U4-DUNGEON-EXPLORATION 1\n17 1 1 0\n17 1 1 0\n";
    assert(!loaded.load(path, shapes));
    assert(loaded.all().size() == 2);
    std::ofstream(path, std::ios::trunc)
        << "U4-DUNGEON-EXPLORATION 1\n99 1 1 0\n";
    assert(!loaded.load(path, shapes));
    assert(loaded.all().size() == 2);
    std::ofstream(path, std::ios::trunc)
        << "U4-DUNGEON-EXPLORATION 1\n17 1 1 9\n";
    assert(!loaded.load(path, shapes));
    assert(loaded.all().size() == 2);

    assert(unlink(path.c_str()) == 0);
    assert(rmdir(directory) == 0);
    return 0;
}
