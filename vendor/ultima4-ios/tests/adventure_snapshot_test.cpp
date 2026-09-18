#include "save_snapshot.h"
#include "topicjournal.h"
#include "mobile_map_discoveries.h"
#include "mobile_map_pins.h"
#include "mobile_dungeon_exploration.h"
#include "savegame.h"
#include "save_validation.h"
#include <cassert>
#include <fstream>
#include <string>

static void writeWorld(const std::string &directory, unsigned moves) {
    SaveGame save = {};
    save.members = 1;
    save.moves = moves;
    save.players[0].hp = 287;
    save.players[0].hpMax = 300;
    save.players[0].weapon = WEAP_AXE;
    save.players[0].armor = ARMR_LEATHER;
    save.reagents[REAG_GINSENG] = 0;
    save.reagents[REAG_GARLIC] = 1;
    save.mixtures[2] = 3;
    FILE *file = fopen((directory + "party.sav").c_str(), "wb");
    assert(file && saveGameWrite(&save, file));
    assert(fclose(file) == 0);
    SaveGameMonsterRecord monsters[MONSTERTABLE_SIZE] = {};
    monsters[0].tile = 16;
    monsters[0].x = 23;
    monsters[0].y = 42;
    monsters[0].unused1 = 17;
    monsters[0].unused2 = 29;
    file = fopen((directory + "monsters.sav").c_str(), "wb");
    assert(file && saveGameMonstersWrite(monsters, file));
    assert(fclose(file) == 0);
}
static void checkWorld(const std::string &directory, unsigned moves) {
    assert(SaveValidation::party(directory + "party.sav"));
    assert(SaveValidation::creatures(directory + "monsters.sav"));
    SaveGame loaded = {};
    FILE *file = fopen((directory + "party.sav").c_str(), "rb");
    assert(file && saveGameRead(&loaded, file));
    assert(fclose(file) == 0);
    assert(loaded.members == 1 && loaded.moves == moves);
    assert(loaded.players[0].hp == 287 && loaded.players[0].hpMax == 300);
    assert(loaded.players[0].weapon == WEAP_AXE && loaded.players[0].armor == ARMR_LEATHER);
    assert(loaded.reagents[REAG_GINSENG] == 0 && loaded.reagents[REAG_GARLIC] == 1);
    assert(loaded.mixtures[2] == 3);
    SaveGameMonsterRecord monsters[MONSTERTABLE_SIZE] = {};
    file = fopen((directory + "monsters.sav").c_str(), "rb");
    assert(file && saveGameMonstersRead(monsters, file));
    assert(fclose(file) == 0);
    assert(monsters[0].tile == 16 && monsters[0].x == 23 && monsters[0].y == 42);
    assert(monsters[0].unused1 == 17 && monsters[0].unused2 == 29);
}

int main() {
    const std::vector<MobileDungeonExploration::Shape> dungeonShapes = {
        {17, 8, 8, 8}, {18, 8, 8, 8}, {19, 8, 8, 8}, {20, 8, 8, 8},
        {21, 8, 8, 8}, {22, 8, 8, 8}, {23, 8, 8, 8}, {24, 8, 8, 8}
    };
    SaveGame characterCheck = {};
    characterCheck.members = 1;
    assert(SaveValidation::characters(characterCheck));
    std::memset(characterCheck.players[0].name, 'A', sizeof(characterCheck.players[0].name));
    assert(!SaveValidation::characters(characterCheck));
    characterCheck.players[0].name[15] = '\0';
    assert(SaveValidation::characters(characterCheck));
    characterCheck.players[0].klass = static_cast<ClassType>(255);
    assert(!SaveValidation::characters(characterCheck));
    characterCheck.players[0].klass = CLASS_MAGE;
    characterCheck.players[0].weapon = static_cast<WeaponType>(WEAP_MAX);
    assert(!SaveValidation::characters(characterCheck));
    characterCheck.players[0].weapon = WEAP_HANDS;
    characterCheck.players[0].armor = static_cast<ArmorType>(ARMR_MAX);
    assert(!SaveValidation::characters(characterCheck));
    char directory[] = "/private/tmp/u4-adventure-test-XXXXXX";
    assert(mkdtemp(directory));
    std::string root = directory;
    const std::vector<std::string> required{"party.sav", "monsters.sav", "topics.txt",
                                            "map-pins.dat", "map-discoveries.dat",
                                            "explored-dungeons.dat"};
    auto oldAdventure = SaveSnapshot::begin(root);
    writeWorld(oldAdventure, 47);
    assert(SaveValidation::checkpointFiles(oldAdventure));
    auto dungeon = SaveSnapshot::begin(root);
    writeWorld(dungeon, 47);
    SaveGame dungeonSave = {};
    FILE *dungeonFile = fopen((dungeon + "party.sav").c_str(), "rb");
    assert(dungeonFile && saveGameRead(&dungeonSave, dungeonFile));
    fclose(dungeonFile);
    dungeonSave.location = 17;
    dungeonSave.dnglevel = 0;
    dungeonFile = fopen((dungeon + "party.sav").c_str(), "wb");
    assert(dungeonFile && saveGameWrite(&dungeonSave, dungeonFile));
    fclose(dungeonFile);
    assert(!SaveValidation::checkpointFiles(dungeon));
    std::ofstream(dungeon + "outmonst.sav", std::ios::binary) << std::string(256, '\0');
    std::ofstream(dungeon + "dngmap.sav", std::ios::binary) << std::string(511, '\0');
    assert(!SaveValidation::checkpointFiles(dungeon));
    std::ofstream(dungeon + "dngmap.sav", std::ios::binary) << std::string(512, '\0');
    assert(SaveValidation::checkpointFiles(dungeon));
    for (const char *name : {"party.sav", "monsters.sav", "outmonst.sav", "dngmap.sav"}) unlink((dungeon + name).c_str());
    rmdir(dungeon.c_str());
    TopicJournal oldJournal;
    oldJournal.observe("Speak SUMM.", "A teacher");
    assert(oldJournal.save(oldAdventure + "topics.txt"));
    MobileMapPins oldPins;
    assert(oldPins.set(23, 42, "Britain healer", 256, 256));
    assert(oldPins.save(oldAdventure + "map-pins.dat"));
    MobileMapDiscoveries oldDiscoveries;
    assert(oldDiscoveries.discover(82, 106, MobileMapDiscoveries::TOWN,
                                   "Britain", 256, 256));
    assert(oldDiscoveries.save(oldAdventure + "map-discoveries.dat"));
    MobileDungeonExploration oldDungeons;
    assert(oldDungeons.reveal(17, 1, 1, 0, 8, 8, 8));
    assert(oldDungeons.save(oldAdventure + "explored-dungeons.dat"));
    assert(SaveSnapshot::publish(root, oldAdventure, required));

    auto newAdventure = SaveSnapshot::begin(root);
    writeWorld(newAdventure, 0);
    assert(!SaveSnapshot::publish(root, newAdventure, required));
    assert(SaveSnapshot::current(root) == oldAdventure);
    checkWorld(SaveSnapshot::current(root), 47);
    TopicJournal selected;
    assert(selected.load(SaveSnapshot::current(root) + "topics.txt"));
    assert(selected.knowsExact("summ"));
    MobileMapPins selectedPins;
    assert(selectedPins.load(SaveSnapshot::current(root) + "map-pins.dat", 256, 256));
    assert(selectedPins.all().size() == 1 && selectedPins.all()[0].label == "Britain healer");
    MobileMapDiscoveries selectedDiscoveries;
    assert(selectedDiscoveries.load(SaveSnapshot::current(root) + "map-discoveries.dat",
                                    256, 256));
    assert(selectedDiscoveries.all().size() == 1 &&
           selectedDiscoveries.all()[0].name == "Britain");
    MobileDungeonExploration selectedDungeons;
    assert(selectedDungeons.load(SaveSnapshot::current(root) + "explored-dungeons.dat",
                                 dungeonShapes));
    assert(selectedDungeons.all().size() == 1 &&
           selectedDungeons.isRevealed(17, 1, 1, 0));

    assert(TopicJournal().save(newAdventure + "topics.txt"));
    assert(MobileMapPins().save(newAdventure + "map-pins.dat"));
    assert(MobileMapDiscoveries().save(newAdventure + "map-discoveries.dat"));
    assert(MobileDungeonExploration().save(newAdventure + "explored-dungeons.dat"));
    assert(SaveSnapshot::publish(root, newAdventure, required));
    assert(SaveSnapshot::current(root) == newAdventure);
    checkWorld(SaveSnapshot::current(root), 0);
    assert(selected.load(SaveSnapshot::current(root) + "topics.txt"));
    assert(!selected.knowsExact("summ"));
    assert(selected.history().empty());
    assert(selectedPins.load(SaveSnapshot::current(root) + "map-pins.dat", 256, 256));
    assert(selectedPins.all().empty());
    assert(selectedDiscoveries.load(SaveSnapshot::current(root) + "map-discoveries.dat",
                                    256, 256));
    assert(selectedDiscoveries.all().empty());
    assert(selectedDungeons.load(SaveSnapshot::current(root) + "explored-dungeons.dat",
                                 dungeonShapes));
    assert(selectedDungeons.all().empty());

    // Previous generations retain their own journal for future recovery.
    assert(selected.load(oldAdventure + "topics.txt"));
    assert(selected.knowsExact("summ"));
    assert(selectedPins.load(oldAdventure + "map-pins.dat", 256, 256));
    assert(selectedPins.all().size() == 1 && selectedPins.all()[0].label == "Britain healer");
    assert(selectedDiscoveries.load(oldAdventure + "map-discoveries.dat", 256, 256));
    assert(selectedDiscoveries.all().size() == 1 &&
           selectedDiscoveries.all()[0].name == "Britain");
    assert(selectedDungeons.load(oldAdventure + "explored-dungeons.dat", dungeonShapes));
    assert(selectedDungeons.all().size() == 1 &&
           selectedDungeons.isRevealed(17, 1, 1, 0));
    // Truncated and overlong files must not pass pre-publication validation.
    assert(truncate((newAdventure + "party.sav").c_str(), 100) == 0);
    assert(!SaveValidation::party(newAdventure + "party.sav"));
    assert(truncate((newAdventure + "monsters.sav").c_str(), 10) == 0);
    assert(!SaveValidation::creatures(newAdventure + "monsters.sav"));
    { std::ofstream extra(oldAdventure + "party.sav", std::ios::app); extra << 'x'; }
    assert(!SaveValidation::party(oldAdventure + "party.sav"));
    for (auto generation : {oldAdventure, newAdventure}) {
        for (auto file : required) unlink((generation + file).c_str());
        rmdir(generation.c_str());
    }
    unlink((root + "/CURRENT").c_str());
    rmdir(root.c_str());
}
