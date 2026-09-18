#include "journal_notebook.h"
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

namespace {
constexpr std::size_t maxFileBytes = 8 * 1024 * 1024;
bool validKey(const std::string &key) { return !key.empty() && key.size() <= 65536 && key.find('\0') == std::string::npos; }
bool validText(const std::string &text) {
    return !text.empty() && text.size() <= JournalNotebook::maxTextBytes &&
        text.find('\0') == std::string::npos && text.find_first_not_of(" \t\r\n") != std::string::npos;
}
bool readId(std::istream &in, uint64_t &id) {
    std::string token;
    if (!(in >> token) || token.empty() || token.find_first_not_of("0123456789") != std::string::npos) return false;
    id = 0;
    for (char digit : token) {
        unsigned value = digit - '0';
        if (id > (UINT64_MAX - value) / 10) return false;
        id = id * 10 + value;
    }
    return true;
}
}

std::string JournalNotebook::key(const TopicJournal::Passage &p) {
    // Full, length-prefixed identity avoids hashes, positional indices, and
    // ambiguous delimiters. These are the same fields observe() deduplicates.
    std::string result;
    for (const std::string *field : {&p.source, &p.speaker, &p.topic, &p.text})
        result += std::to_string(field->size()) + ":" + *field;
    return result;
}
bool JournalNotebook::favorite(const std::string &passage) const {
    return std::find(bookmarks.begin(), bookmarks.end(), passage) != bookmarks.end();
}
bool JournalNotebook::toggleFavorite(const std::string &passage) {
    if (!validKey(passage)) return false;
    auto found = std::find(bookmarks.begin(), bookmarks.end(), passage);
    if (found != bookmarks.end()) bookmarks.erase(found);
    else {
        if (bookmarks.size() >= maxItems) return false;
        bookmarks.push_back(passage);
    }
    return true;
}
uint64_t JournalNotebook::attachedNote(const std::string &passage) const {
    if (passage.empty()) return 0;
    for (const Note &note : personalNotes) if (note.passage == passage) return note.id;
    return 0;
}
uint64_t JournalNotebook::setNote(uint64_t id, const std::string &passage, const std::string &text) {
    if ((!passage.empty() && !validKey(passage)) || !validText(text)) return 0;
    if (!id && !passage.empty()) id = attachedNote(passage);
    if (id) {
        for (Note &note : personalNotes) if (note.id == id && note.passage == passage) {
            note.text = text; return id;
        }
        return 0; // never reattach an existing note to a different source
    }
    if (personalNotes.size() >= maxItems || nextId == UINT64_MAX) return 0;
    personalNotes.push_back({nextId++, passage, text});
    return personalNotes.back().id;
}
bool JournalNotebook::deleteNote(uint64_t id) {
    auto found = std::find_if(personalNotes.begin(), personalNotes.end(),
        [id](const Note &note) { return note.id == id; });
    if (found == personalNotes.end()) return false;
    personalNotes.erase(found); return true;
}
bool JournalNotebook::validFor(const TopicJournal &journal) const {
    auto known = [&](const std::string &value) {
        return std::any_of(journal.history().begin(), journal.history().end(),
            [&](const TopicJournal::Passage &p) { return key(p) == value; });
    };
    for (const std::string &bookmark : bookmarks) if (!known(bookmark)) return false;
    for (const Note &note : personalNotes) if (!note.passage.empty() && !known(note.passage)) return false;
    return true;
}
bool JournalNotebook::save(const std::string &path) const {
    std::ostringstream out;
    out << "U4NOTEBOOK 1 " << nextId << '\n';
    for (const std::string &bookmark : bookmarks) out << "B " << std::quoted(bookmark) << '\n';
    for (const Note &note : personalNotes)
        out << "N " << note.id << ' ' << std::quoted(note.passage) << ' ' << std::quoted(note.text) << '\n';
    const std::string bytes = out.str();
    if (bytes.size() > maxFileBytes) return false;
    std::string pattern = path + ".XXXXXX";
    std::vector<char> temporary(pattern.begin(), pattern.end()); temporary.push_back(0);
    int fd = mkstemp(temporary.data());
    if (fd < 0) return false;
    std::size_t offset = 0;
    bool ok = true;
    while (offset < bytes.size()) {
        ssize_t count = write(fd, bytes.data() + offset, bytes.size() - offset);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) { ok = false; break; }
        offset += (std::size_t)count;
    }
#ifndef ZU4_WEB
    if (ok) ok = fsync(fd) == 0;
#else
    // Emscripten fd_sync unwinds through Asyncify, which cannot be entered
    // from a journal edit while the engine is already awaiting an event.
    // Web publishes these MEMFS bytes atomically via AdventureStore's
    // IndexedDB transaction, then mirrors IDBFS asynchronously in JavaScript.
#endif
    if (close(fd) != 0) ok = false;
    if (ok) ok = rename(temporary.data(), path.c_str()) == 0;
    if (!ok) unlink(temporary.data());
    return ok;
}
bool JournalNotebook::load(const std::string &path) {
    struct stat info;
    if (stat(path.c_str(), &info) != 0) {
        if (errno != ENOENT) return false;
        clear(); return true;
    }
    if (!S_ISREG(info.st_mode) || info.st_size <= 0 || info.st_size > (off_t)maxFileBytes) return false;
    std::ifstream in(path);
    std::string magic; int version = 0;
    JournalNotebook loaded;
    if (!(in >> magic >> version) || !readId(in, loaded.nextId) || magic != "U4NOTEBOOK" || version != 1 ||
        !loaded.nextId || loaded.nextId == UINT64_MAX) return false;
    while (true) {
        in >> std::ws; if (in.eof()) break;
        char type;
        if (!(in >> type)) return false;
        if (type == 'B') {
            std::string passage;
            if (!(in >> std::quoted(passage)) || !validKey(passage) || loaded.favorite(passage) ||
                loaded.bookmarks.size() >= maxItems) return false;
            loaded.bookmarks.push_back(passage);
        } else if (type == 'N') {
            Note note;
            if (!readId(in, note.id) || !(in >> std::quoted(note.passage) >> std::quoted(note.text)) ||
                !note.id || note.id >= loaded.nextId || !validText(note.text) ||
                (!note.passage.empty() && !validKey(note.passage)) || loaded.personalNotes.size() >= maxItems ||
                loaded.attachedNote(note.passage)) return false;
            for (const Note &existing : loaded.personalNotes) if (existing.id == note.id) return false;
            loaded.personalNotes.push_back(note);
        } else return false;
    }
    if (in.bad()) return false;
    *this = loaded; return true;
}
