#include "topicjournal.h"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <unistd.h>
int main() {
    TopicJournal phrases;
    phrases.observe("Blacksmiths use stone.", "first");
    assert(!phrases.knowsPhrase("black stone"));
    phrases.observe("A black bird.", "second");
    assert(!phrases.knowsPhrase("black stone"));
    phrases.observe("Seek the BLACK\nSTONE!", "third");
    assert(phrases.knowsPhrase("black stone"));
    assert(!phrases.knowsPhrase("white stone"));
    TopicJournal j;
    assert(!j.knows("rune"));
    auto initial = j.choices({"name", "job", "heal", "rune", "ojna", "", "bye"});
    assert(initial.size() == 4);
    assert(initial[2].title == "Health");
    assert(initial[0].role == TopicJournal::CHOICE_ACTION);
    assert(initial.back().keyword == "bye");
    assert(initial.back().role == TopicJournal::CHOICE_DISMISS);
    j.observe("Seek the RUNE of Compassion. Jobless.", "Iolo, Britain");
    assert(j.knows("rune"));
    auto available = j.choices({"rune", "comp", "humi", "ojna", "rune"});
    assert(available.size() == 2);
    assert(available[1].title == "Compassion");
    assert(j.choices({"humi"}).empty()); // known topic on wrong NPC stays absent
    assert(j.knows("comp"));
    assert(j.label("comp") == "compassion");
    assert(!j.knows("job"));
    assert(!j.knows(""));
    assert(!j.knows("humility"));
    j.observe("The weather is humid.", "A sign");
    assert(!j.knows("humility"));
    auto count = j.entries().size();
    j.observe("Seek the RUNE of Compassion. Jobless.", "Iolo, Britain");
    assert(j.entries().size() == count);
    j.observe("Rune", "A sign");
    assert(j.entries().size() == count + 1);
    std::string file = "/private/tmp/u4-topics-test-" + std::to_string(getpid());
    j.observe("A clue with \"quotes\"\nand a second line.", "A book",
              "Iolo", "Britain", "person", "rune");
    assert(j.save(file));
    TopicJournal restored;
    assert(restored.load(file));
    assert(restored.knows("comp"));
    assert(restored.history().size() == j.history().size());
    assert(restored.history().back().text == j.history().back().text);
    assert(restored.history().back().speaker == "Iolo");
    assert(restored.history().back().place == "Britain");
    assert(restored.history().back().kind == "person");
    assert(restored.history().back().topic == "rune");
    assert(restored.entries().size() == j.entries().size());
    { std::ofstream out(file); out << "U4TOPICS 2\nP \"An older clue\" \"Conversation in Britain\"\n"; }
    TopicJournal legacy;
    assert(legacy.load(file));
    assert(legacy.history().size() == 1);
    assert(legacy.history().front().speaker.empty());
    assert(legacy.history().front().source == "Conversation in Britain");
    { std::ofstream out(file); out << "U4TOPICS 1\n\"rune\" \"sign\"\nBROKEN\n"; }
    assert(!restored.load(file));
    assert(restored.knows("comp")); // corrupt loads preserve the current record
    restored.clear();
    assert(!restored.knows("comp"));
    TopicJournal puzzle;
    puzzle.observe("Summer brings rumors of magic.", "A traveler");
    assert(!puzzle.knowsExact("summ"));
    assert(!puzzle.knowsExact("mu"));
    assert(!puzzle.knowsExact("ra"));
    assert(!puzzle.knowsExact(""));
    auto before = puzzle.entries().size();
    assert(!puzzle.knowsExact("ahm"));
    assert(puzzle.entries().size() == before); // asking does not teach an answer
    puzzle.observe("Speak SUMM, then reflect.", "A teacher");
    assert(puzzle.knowsExact("summ"));
    assert(puzzle.knowsExact("SUMM"));
    assert(puzzle.save(file));
    TopicJournal puzzleReloaded;
    assert(puzzleReloaded.load(file));
    assert(puzzleReloaded.knowsExact("summ"));
    assert(!puzzleReloaded.knowsExact("mu"));
    puzzleReloaded.clear();
    assert(!puzzleReloaded.knowsExact("summ"));
    std::remove(file.c_str());
}
