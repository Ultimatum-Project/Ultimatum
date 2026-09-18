#include "mobile_map_pins.h"

#include <cassert>
#include <fstream>
#include <string>
#include <unistd.h>

int main() {
    MobileMapPins pins;
    assert(pins.set(10, 20, "Reagent seller", 256, 256));
    assert(pins.set(30, 40, "Return with rune", 256, 256));
    assert(pins.set(10, 20, "Cheap reagents", 256, 256));
    assert(pins.all().size() == 2 && pins.all()[0].label == "Cheap reagents");
    assert(!pins.set(-1, 0, "Outside", 256, 256));
    assert(!pins.set(256, 0, "Outside", 256, 256));
    assert(!pins.set(1, 1, "", 256, 256));
    assert(!pins.set(1, 1, "Line\nbreak", 256, 256));

    MobileMapPins capacity;
    for (std::size_t i = 0; i < MobileMapPins::MAX_PINS; ++i)
        assert(capacity.set((int)i, 1, "Pin", 256, 256));
    assert(!capacity.set(100, 1, "One too many", 256, 256));
    assert(capacity.set(0, 1, "Updating is still allowed", 256, 256));

    char directory[] = "/private/tmp/u4-map-pins-XXXXXX";
    assert(mkdtemp(directory));
    std::string path = std::string(directory) + "/map-pins.dat";
    assert(pins.save(path));

    MobileMapPins loaded;
    assert(loaded.load(path, 256, 256));
    assert(loaded.all().size() == 2);
    assert(loaded.all()[0].x == 10 && loaded.all()[0].y == 20);
    assert(loaded.all()[0].label == "Cheap reagents");
    assert(loaded.remove(10, 20));
    assert(!loaded.remove(10, 20));

    MobileMapPins legacy;
    assert(legacy.set(1, 2, "Will clear", 256, 256));
    assert(legacy.load(std::string(directory) + "/missing.dat", 256, 256));
    assert(legacy.all().empty());

    std::ofstream(path, std::ios::trunc) << "U4MAP-PINS 1\n999 2 \"Bad\"\n";
    assert(!loaded.load(path, 256, 256));
    assert(loaded.all().size() == 1 && loaded.all()[0].label == "Return with rune");

    assert(unlink(path.c_str()) == 0);
    assert(rmdir(directory) == 0);
    return 0;
}
