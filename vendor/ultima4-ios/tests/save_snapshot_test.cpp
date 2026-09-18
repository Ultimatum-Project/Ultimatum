#include "save_snapshot.h"
#include <cassert>
#include <fstream>
#include <string>
int main() {
    char directory[] = "/private/tmp/u4-snapshot-test-XXXXXX";
    assert(mkdtemp(directory));
    std::string root = directory;
    auto first = SaveSnapshot::begin(root);
    assert(!first.empty());
    std::ofstream(first + "party.sav") << "party one";
    std::ofstream(first + "monsters.sav") << "monsters one";
    assert(SaveSnapshot::publish(root, first, {"party.sav", "monsters.sav"}));
    assert(SaveSnapshot::current(root) == first);
    assert(SaveSnapshot::previous(root).empty());
    auto second = SaveSnapshot::begin(root);
    std::ofstream(second + "party.sav") << "party two";
    // An incomplete generation must leave the prior complete snapshot selected.
    assert(!SaveSnapshot::publish(root, second, {"party.sav", "monsters.sav"}));
    assert(SaveSnapshot::current(root) == first);
    std::ofstream(second + "monsters.sav") << "monsters two";
    assert(SaveSnapshot::publish(root, second, {"party.sav", "monsters.sav"}));
    assert(SaveSnapshot::current(root) == second);
    assert(SaveSnapshot::previous(root) == first);
    // Runtime recovery replaces only the tiny pointer. A malformed CURRENT
    // can be restored without modifying or deleting either generation.
    { std::ofstream invalid(root + "/CURRENT"); invalid << "../outside\n"; }
    assert(SaveSnapshot::current(root).empty());
    assert(SaveSnapshot::previous(root) == first);
    assert(SaveSnapshot::writePointer(root, "CURRENT", first.substr(root.size() + 1,
        first.size() - root.size() - 2)));
    assert(SaveSnapshot::current(root) == first);
    assert(SaveSnapshot::writePointer(root, "CURRENT", second.substr(root.size() + 1,
        second.size() - root.size() - 2)));
    assert(SaveSnapshot::current(root) == second);
    assert(!SaveSnapshot::writePointer(root + "/missing", "CURRENT", "generation-valid"));
    assert(!SaveSnapshot::publish(root, second, {"../CURRENT"}));
    std::vector<std::string> extra;
    for (int i = 0; i < 4; ++i) {
        auto generation = SaveSnapshot::begin(root);
        std::ofstream(generation + "party.sav") << "older fixture";
        extra.push_back(generation);
    }
    auto unfamiliar = SaveSnapshot::begin(root);
    std::ofstream(unfamiliar + "personal-note.txt") << "keep me";
    auto linked = SaveSnapshot::begin(root);
    assert(symlink((first + "party.sav").c_str(), (linked + "party.sav").c_str()) == 0);
    // Cleanup must honor the durable recovery pointer even without a caller hint.
    SaveSnapshot::retainRecent(root, "");
    assert(SaveSnapshot::current(root) == second);
    assert(access((first + "party.sav").c_str(), F_OK) == 0);
    assert(access((unfamiliar + "personal-note.txt").c_str(), F_OK) == 0);
    struct stat linkInfo;
    assert(lstat((linked + "party.sav").c_str(), &linkInfo) == 0 && S_ISLNK(linkInfo.st_mode));
    int retainedExtras = 0;
    for (const auto &generation : extra) {
        if (access(generation.c_str(), F_OK) == 0) ++retainedExtras;
        unlink((generation + "party.sav").c_str()); rmdir(generation.c_str());
    }
    assert(retainedExtras == 1); // current + predecessor + one other generation
    unlink((unfamiliar + "personal-note.txt").c_str()); rmdir(unfamiliar.c_str());
    unlink((linked + "party.sav").c_str()); rmdir(linked.c_str());
    std::ofstream(root + "/CURRENT") << "../outside\n";
    assert(SaveSnapshot::current(root).empty());
    assert(SaveSnapshot::previous(root) == first);
    for (auto generation : {first, second}) {
        unlink((generation + "party.sav").c_str());
        unlink((generation + "monsters.sav").c_str());
        rmdir(generation.c_str());
    }
    unlink((root + "/CURRENT").c_str());
    unlink((root + "/PREVIOUS").c_str());
    rmdir(root.c_str());
}
