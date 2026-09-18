#ifndef ZU4_JOURNAL_NOTEBOOK_H
#define ZU4_JOURNAL_NOTEBOOK_H
#include "topicjournal.h"
#include <cstdint>

// Player-authored metadata, never input to TopicJournal's discovery rules.
class JournalNotebook {
public:
    struct Note { uint64_t id; std::string passage, text; };
    static constexpr std::size_t maxItems = 256, maxTextBytes = 4000;
    static std::string key(const TopicJournal::Passage &passage);
    const std::vector<std::string> &favorites() const { return bookmarks; }
    const std::vector<Note> &notes() const { return personalNotes; }
    bool favorite(const std::string &passage) const;
    bool toggleFavorite(const std::string &passage);
    uint64_t attachedNote(const std::string &passage) const;
    uint64_t setNote(uint64_t id, const std::string &passage, const std::string &text);
    bool deleteNote(uint64_t id);
    bool validFor(const TopicJournal &journal) const;
    bool save(const std::string &path) const;
    bool load(const std::string &path); // missing file = empty legacy notebook
    void clear() { bookmarks.clear(); personalNotes.clear(); nextId = 1; }
private:
    std::vector<std::string> bookmarks;
    std::vector<Note> personalNotes;
    uint64_t nextId = 1;
};
#endif
