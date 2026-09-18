#include "save_slots.h"

#include <cassert>
#include <fstream>
#include <string>
#include <unistd.h>

int main() {
    char directory[] = "/private/tmp/u4-save-slots-test-XXXXXX";
    assert(mkdtemp(directory));
    std::string base = std::string(directory) + "/";

    assert(SaveSlots::active(base) == 1);
    assert(SaveSlots::snapshotRoot(base, 1) == base + "snapshots");
    assert(SaveSlots::snapshotRoot(base, 2) == base + "snapshots-slot-2");
    assert(SaveSlots::fallbackDirectory(base, 1) == base);
    assert(SaveSlots::fallbackDirectory(base, 2) == base + "empty-slot-2/");
    assert(!SaveSlots::occupied(base, 1));
    assert(!SaveSlots::occupied(base, 2));

    assert(SaveSlots::select(base, 2));
    assert(SaveSlots::active(base) == 2);
    assert(!SaveSlots::select(base, 0));
    assert(!SaveSlots::select(base, 4));
    assert(SaveSlots::active(base) == 2);
    { std::ofstream invalid(SaveSlots::pointerPath(base)); invalid << "9\n"; }
    assert(SaveSlots::active(base) == 1);
    assert(SaveSlots::select(base, 2));

    std::ofstream(base + "party.sav") << "legacy";
    assert(SaveSlots::occupied(base, 1));
    assert(!SaveSlots::occupied(base, 2));

    std::string second = SaveSlots::snapshotRoot(base, 2);
    std::string generation = SaveSnapshot::begin(second);
    assert(!generation.empty());
    std::ofstream(generation + "party.sav") << "slot two";
    assert(SaveSnapshot::publish(second, generation, {"party.sav"}));
    assert(SaveSlots::occupied(base, 2));
    assert(!SaveSlots::occupied(base, 3));

    unlink((generation + "party.sav").c_str());
    rmdir(generation.c_str());
    unlink((second + "/CURRENT").c_str());
    rmdir(second.c_str());
    unlink((base + "party.sav").c_str());
    unlink(SaveSlots::pointerPath(base).c_str());
    rmdir(directory);
}
