#include "journal_notebook.h"
#include "save_snapshot.h"
#include <cassert>
#include <fstream>
#include <cstdio>
#include <unistd.h>

int main() {
    char directory[] = "/private/tmp/u4-notebook-test-XXXXXX";
    assert(mkdtemp(directory));
    const std::string root = directory, file = root + "/journal-notebook.dat";
    TopicJournal journal;
    journal.observe("Seek the rune.", "Iolo in Britain", "Iolo", "Britain", "person", "rune");
    const std::string passage = JournalNotebook::key(journal.history().front());
    JournalNotebook book;
    assert(book.load(file) && book.notes().empty()); // legacy adventure
    assert(book.toggleFavorite(passage) && book.favorite(passage));
    uint64_t attached = book.setNote(0, passage, "My theory: SUMM\n\"café\" 日本語");
    assert(attached && book.attachedNote(passage) == attached);
    assert(!journal.knowsExact("summ")); // player writing never teaches dialogue
    assert(book.setNote(0, passage, "Revised theory") == attached && book.notes().size() == 1);
    uint64_t standalone = book.setNote(0, "", "Visit Britain again.");
    assert(standalone > attached);
    assert(!book.setNote(attached, "", "Cannot move source"));
    assert(!book.setNote(999, "", "Unknown ID"));
    assert(!book.setNote(0, "", " \n\t"));
    assert(!book.setNote(0, "", std::string(4001, 'x')));
    assert(book.setNote(attached, passage, "My theory: SUMM\n\"café\" 日本語"));
    assert(book.save(file));
    JournalNotebook restored;
    assert(restored.load(file) && restored.validFor(journal));
    assert(restored.notes()[0].text == book.notes()[0].text);
    assert(restored.toggleFavorite(passage) && !restored.favorite(passage));
    assert(restored.attachedNote(passage) == attached && journal.history().size() == 1);
    assert(restored.deleteNote(attached) && restored.notes().size() == 1);
    assert(!restored.deleteNote(attached));
    journal.observe("A second passage.", "Iolo in Britain", "Iolo", "Britain", "person", "job");
    assert(book.validFor(journal) && book.favorite(JournalNotebook::key(journal.history()[0])));
    TopicJournal empty;
    assert(!book.validFor(empty));
    // Corrupt loads never replace live notes.
    for (const char *bad : {"U4NOTEBOOK 2 3\n", "U4NOTEBOOK 1 -2\n", "U4NOTEBOOK 1 3\nN -1 \"\" \"bad\"\n",
                           "U4NOTEBOOK 1 3\nN 1 \"\" \"ok\"\nN 1 \"\" \"duplicate\"\n", "U4NOTEBOOK 1 3\nN 1 \"\" \"unfinished"}) {
        std::ofstream(file) << bad;
        assert(!book.load(file) && book.notes().size() == 2 && book.favorite(passage));
    }
    assert(book.save(file));
    assert(!book.save(root + "/missing/book"));
    assert(restored.load(file) && restored.notes().size() == 2);
    // Checkpoint snapshots retain distinct notebooks and recover them together.
    auto first = SaveSnapshot::begin(root);
    assert(book.save(first + "journal-notebook.dat"));
    assert(SaveSnapshot::publish(root, first, {"journal-notebook.dat"}));
    auto second = SaveSnapshot::begin(root);
    assert(JournalNotebook().save(second + "journal-notebook.dat"));
    assert(SaveSnapshot::publish(root, second, {"journal-notebook.dat"}));
    assert(restored.load(SaveSnapshot::current(root) + "journal-notebook.dat") && restored.notes().empty());
    assert(restored.load(first + "journal-notebook.dat") && restored.notes().size() == 2);
    std::string previousName = first.substr(root.size() + 1); previousName.pop_back();
    assert(SaveSnapshot::writePointer(root, "CURRENT", previousName));
    assert(restored.load(SaveSnapshot::current(root) + "journal-notebook.dat") && restored.favorite(passage));
    SaveSnapshot::retainRecent(root, second);
    assert(restored.load(first + "journal-notebook.dat") && restored.notes().size() == 2);
    std::remove(file.c_str());
    // Exercise both limits without truncating existing user data.
    JournalNotebook limits;
    for (size_t i = 0; i < JournalNotebook::maxItems; ++i) {
        assert(limits.toggleFavorite(std::to_string(i)));
        assert(limits.setNote(0, "", "Note " + std::to_string(i)));
    }
    assert(!limits.toggleFavorite("overflow") && !limits.setNote(0, "", "overflow"));
    for (const auto &generation : {first, second}) {
        std::remove((generation + "journal-notebook.dat").c_str());
        rmdir(generation.c_str());
    }
    std::remove((root + "/CURRENT").c_str()); std::remove((root + "/PREVIOUS").c_str());
    rmdir(root.c_str());
}
