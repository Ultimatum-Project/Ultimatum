#include "mobile_map_discoveries.h"

#include <cassert>
#include <fstream>
#include <string>
#include <unistd.h>

int main() {
    MobileMapDiscoveries discoveries;
    assert(discoveries.all().empty());
    assert(discoveries.discover(82, 106, MobileMapDiscoveries::TOWN, "Britain", 256, 256));
    assert(discoveries.discover(86, 107, MobileMapDiscoveries::CASTLE, "Britannia", 256, 256));
    assert(discoveries.discover(82, 106, MobileMapDiscoveries::TOWN, "Britain", 256, 256));
    assert(discoveries.all().size() == 2);
    assert(!discoveries.discover(-1, 0, MobileMapDiscoveries::TOWN, "Outside", 256, 256));
    assert(!discoveries.discover(1, 1, (MobileMapDiscoveries::Category)0, "Unknown", 256, 256));
    assert(!discoveries.discover(1, 1, MobileMapDiscoveries::SHRINE, "", 256, 256));
    assert(!discoveries.discover(1, 1, MobileMapDiscoveries::DUNGEON, "Bad\nname", 256, 256));

    MobileMapDiscoveries capacity;
    for (std::size_t i = 0; i < MobileMapDiscoveries::MAX_PLACES; ++i)
        assert(capacity.discover((int)i, 1, MobileMapDiscoveries::VILLAGE,
                                 "Known place", 256, 256));
    assert(!capacity.discover(100, 1, MobileMapDiscoveries::TOWN,
                              "One too many", 256, 256));

    char directory[] = "/private/tmp/u4-map-discoveries-XXXXXX";
    assert(mkdtemp(directory));
    std::string path = std::string(directory) + "/map-discoveries.dat";
    assert(discoveries.save(path));

    MobileMapDiscoveries loaded;
    assert(loaded.load(path, 256, 256));
    assert(loaded.all().size() == 2);
    assert(loaded.all()[0].name == "Britain");
    assert(loaded.all()[1].category == MobileMapDiscoveries::CASTLE);

    MobileMapDiscoveries legacy;
    assert(legacy.discover(1, 2, MobileMapDiscoveries::SHRINE,
                           "Will clear", 256, 256));
    assert(legacy.load(std::string(directory) + "/missing.dat", 256, 256));
    assert(legacy.all().empty());

    std::ofstream(path, std::ios::trunc)
        << "U4MAP-DISCOVERIES 1\n999 2 1 \"Bad\"\n";
    assert(!loaded.load(path, 256, 256));
    assert(loaded.all().size() == 2 && loaded.all()[0].name == "Britain");

    assert(unlink(path.c_str()) == 0);
    assert(rmdir(directory) == 0);
    return 0;
}
