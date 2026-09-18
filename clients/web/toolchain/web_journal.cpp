#include <emscripten/emscripten.h>
#include <algorithm>
#include <cctype>
#include <ctime>
#include <cerrno>
#include <sstream>
#include <memory>
#include <sys/stat.h>
#include "web_journal.h"
#include "web_prompt.h"
#include "journal_notebook.h"
#include "context.h"
#include "game.h"
#include "person.h"
#include "map.h"

namespace {
TopicJournal journal;
JournalNotebook notebook;
bool writable = true;
struct JournalOverlay : Controller { bool keyPressed(int) override { return true; } };
std::unique_ptr<JournalOverlay> overlay;
const char *notebookFile = "journal-notebook.dat";
std::string quote(const std::string &text) {
    std::string out = "\"";
    for (unsigned char c : text) {
        if (c == '"' || c == '\\') { out += '\\'; out += c; }
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else if (c < 32) { const char *hex = "0123456789abcdef"; out += "\\u00"; out += hex[c>>4]; out += hex[c&15]; }
        else out += c;
    }
    return out + '"';
}
std::string key(int index) {
    return index >= 0 && index < (int)journal.history().size() ? JournalNotebook::key(journal.history()[index]) : "";
}
bool commit(const JournalNotebook &old) {
    if (writable && webJournalSave(gameSaveDirectory())) return true;
    notebook = old; return false;
}
}
bool webJournalIsOpen() { return overlay != nullptr; }
void webJournalLoad() {
    journal.clear(); notebook.clear();
    const std::string directory = gameSaveDirectory();
    struct stat info;
    const int found=stat((directory + "topics.txt").c_str(), &info);
    writable = found==0 ? journal.load(directory + "topics.txt") : errno==ENOENT;
    writable = notebook.load(directory + notebookFile) && notebook.validFor(journal) && writable;
}
bool webJournalSave(const std::string &directory) {
    return writable && journal.save(directory + "topics.txt") && notebook.save(directory + notebookFile);
}
void webJournalObserve(const TopicJournal::Passage &passage) {
    if (!writable || passage.text.empty()) return;
    journal.observe(passage.text,passage.source,passage.speaker,passage.place,passage.kind,passage.topic);
    journal.save(gameSaveDirectory() + "topics.txt");
}
void webJournalDialogue(const std::string &text, Person *talker, const std::string &topic) {
    if (!c || !c->location) return;
    const std::string place=c->location->map->getName();
    std::string speaker=talker ? talker->getName() : "";
    if (speaker == "(unnamed person)") speaker.clear();
    webJournalObserve({text,speaker.empty() ? "Conversation in " + place : speaker + " in " + place,
        speaker,place,speaker.empty() ? "conversation" : "person",topic});
}
extern "C" EMSCRIPTEN_KEEPALIVE int zu4_web_journal_open() {
    if (overlay || !game || !c || !c->party || (c->location->context & CTX_COMBAT)) return 0;
    Controller *owner = eventHandler->getController();
    auto *prompt=dynamic_cast<WebPromptController *>(owner);
    if (owner != game && (!prompt || prompt->submitted)) return 0;
    overlay.reset(new JournalOverlay());
    ++webInteractionDepth();eventHandler->pushController(overlay.get());return 1;
}
extern "C" EMSCRIPTEN_KEEPALIVE int zu4_web_journal_close() {
    if (!overlay || eventHandler->getController() != overlay.get()) return 0;
    eventHandler->popController();overlay.reset();--webInteractionDepth();
    c->lastCommandTime=time(NULL);return 1;
}
extern "C" EMSCRIPTEN_KEEPALIVE void zu4_web_journal_reload() { if (overlay) webJournalLoad(); }
extern "C" EMSCRIPTEN_KEEPALIVE const char *zu4_web_journal_json() {
    static std::string result;
    std::ostringstream out;out << "{\"writable\":" << (writable ? "true" : "false") << ",\"passages\":[";
    bool first=true;
    for (int index=0;index<(int)journal.history().size();++index) {
        const auto &p=journal.history()[index];std::string normalized;
        for(unsigned char ch:p.text) if(std::isalnum(ch)) normalized += (char)std::tolower(ch);
        if(normalized.empty() || normalized=="bye" || normalized=="farewell" || normalized=="yourinterest" || normalized=="whatelse") continue;
        if(!first)out << ',';first=false;
        const std::string identity=key(index);
        out << "{\"index\":" << index << ",\"text\":" << quote(p.text) << ",\"source\":" << quote(p.source)
            << ",\"speaker\":" << quote(p.speaker) << ",\"place\":" << quote(p.place)
            << ",\"kind\":" << quote(p.kind) << ",\"topic\":" << quote(p.topic)
            << ",\"favorite\":" << (notebook.favorite(identity) ? "true" : "false")
            << ",\"noteId\":" << quote(std::to_string(notebook.attachedNote(identity))) << '}';
    }
    out << "],\"notes\":[";first=true;
    for(const auto &note:notebook.notes()) {
        if(!first)out << ',';first=false;
        int passage=-1;
        for(int i=0;i<(int)journal.history().size();++i)if(key(i)==note.passage){passage=i;break;}
        out << "{\"id\":" << quote(std::to_string(note.id)) << ",\"passage\":" << passage << ",\"text\":" << quote(note.text) << '}';
    }
    result=out.str()+"]}";return result.c_str();
}
extern "C" EMSCRIPTEN_KEEPALIVE int zu4_web_journal_favorite(int index) {
    if(!overlay || !writable || key(index).empty())return 0;
    JournalNotebook old=notebook;
    return notebook.toggleFavorite(key(index)) && commit(old);
}
extern "C" EMSCRIPTEN_KEEPALIVE int zu4_web_journal_note(int index, const char *id, const char *text) {
    if(!overlay || !writable || !id || !text || index < -1 || (index>=0 && key(index).empty()))return 0;
    uint64_t identifier=0;
    for(char c:std::string(id)) {
        if(c<'0' || c>'9' || identifier>(UINT64_MAX-(c-'0'))/10)return 0;
        identifier=identifier*10+(c-'0');
    }
    JournalNotebook old=notebook;
    return notebook.setNote(identifier,index<0 ? "" : key(index),text) && commit(old);
}
extern "C" EMSCRIPTEN_KEEPALIVE int zu4_web_journal_delete(const char *id) {
    if(!overlay || !writable || !id)return 0;
    for(const auto &note:notebook.notes())if(std::to_string(note.id)==id) {
        JournalNotebook old=notebook;return notebook.deleteNote(note.id) && commit(old);
    }
    return 0;
}
