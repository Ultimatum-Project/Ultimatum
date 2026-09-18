#include "topicjournal.h"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace {
bool letter(unsigned char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
std::string lower(std::string s) {
    for (char &c : s) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return s;
}
bool matches(const std::string &word, const std::string &key) {
    // Match U4's four-character parser, but do not discover short keywords
    // from unrelated words (e.g. JOB from JOBLESS).
    // Full topic names require full exposure: e.g. "humility" must not be
    // unlocked just because the player read "humid". Only a four-letter
    // stored abbreviation uses the legacy prefix rule.
    return key.size() != 4 ? word == key : word.size() >= 4 && word.compare(0, 4, key, 0, 4) == 0;
}
}

bool TopicJournal::knowsPhrase(const std::string &phrase) const {
    auto normalize = [](const std::string &text) {
        std::string result = " ";
        for (unsigned char c : text) {
            if (letter(c)) result += lower(std::string(1, c));
            else if (result.back() != ' ') result += ' ';
        }
        if (result.back() != ' ') result += ' ';
        return result;
    };
    std::string needle = normalize(phrase);
    if (needle == " ") return false;
    for (const Passage &passage : passages)
        if (normalize(passage.text).find(needle) != std::string::npos) return true;
    return false;
}

bool TopicJournal::knowsExact(const std::string &word) const {
    const std::string normalized = lower(word);
    if (normalized.empty()) return false;
    return std::any_of(exposures.begin(), exposures.end(), [&](const Exposure &entry) {
        return entry.word == normalized;
    });
}

void TopicJournal::observe(const std::string &text, const std::string &source,
                           const std::string &speaker, const std::string &place,
                           const std::string &kind, const std::string &topic) {
    auto existing = std::find_if(passages.begin(), passages.end(), [&](const Passage &p) {
        return p.text == text && p.source == source && p.speaker == speaker && p.topic == topic;
    });
    if (!text.empty() && existing == passages.end())
        passages.emplace_back(text, source, speaker, place, kind, topic);
    std::string word;
    auto record = [&]() {
        if (!word.empty()) {
            auto found = std::find_if(exposures.begin(), exposures.end(), [&](const Exposure &e) {
                return e.word == word && e.source == source;
            });
            if (found == exposures.end()) exposures.push_back({word, source});
        }
        word.clear();
    };
    for (unsigned char c : text) {
        if (letter(c)) word += lower(std::string(1, c));
        else record();
    }
    record();
}

bool TopicJournal::knows(const std::string &keyword) const { return !label(keyword).empty(); }
std::string TopicJournal::label(const std::string &keyword) const {
    const std::string key = lower(keyword);
    if (key.empty()) return "";
    for (const Exposure &e : exposures) if (matches(e.word, key)) return e.word;
    return "";
}

bool TopicJournal::save(const std::string &path) const {
    const std::string temp = path + ".tmp";
    std::ofstream out(temp, std::ios::trunc);
    if (!out) return false;
    out << "U4TOPICS 3\n";
    for (const Exposure &e : exposures) out << "W " << std::quoted(e.word) << ' ' << std::quoted(e.source) << '\n';
    for (const Passage &p : passages)
        out << "P " << std::quoted(p.text) << ' ' << std::quoted(p.source) << ' '
            << std::quoted(p.speaker) << ' ' << std::quoted(p.place) << ' '
            << std::quoted(p.kind) << ' ' << std::quoted(p.topic) << '\n';
    out.flush();
    if (!out) { out.close(); std::remove(temp.c_str()); return false; }
    out.close();
    if (!out || std::rename(temp.c_str(), path.c_str()) != 0) {
        std::remove(temp.c_str()); return false;
    }
    return true;
}

bool TopicJournal::load(const std::string &path) {
    std::ifstream in(path);
    std::string header;
    if (!std::getline(in, header) ||
        (header != "U4TOPICS 1" && header != "U4TOPICS 2" && header != "U4TOPICS 3")) return false;
    std::vector<Exposure> loaded;
    std::vector<Passage> loadedPassages;
    // quoted strings may contain newlines, so parse records as a stream.
    while (true) {
        in >> std::ws;
        if (in.eof()) break;
        char kind = 'W';
        if (header != "U4TOPICS 1" && !(in >> kind)) return false;
        std::string text, source;
        if (in.peek() == EOF || !(in >> std::quoted(text) >> std::quoted(source))) return false;
        if (kind == 'W') {
            if (text.empty() || text != lower(text) || !std::all_of(text.begin(), text.end(), letter)) return false;
            loaded.push_back({text, source});
        } else if (kind == 'P') {
            if (text.empty()) return false;
            std::string speaker, place, passageKind, topic;
            if (header == "U4TOPICS 3" &&
                !(in >> std::quoted(speaker) >> std::quoted(place) >>
                  std::quoted(passageKind) >> std::quoted(topic))) return false;
            loadedPassages.emplace_back(text, source, speaker, place, passageKind, topic);
        } else return false;
    }
    if (in.bad()) return false;
    exposures.swap(loaded);
    passages.swap(loadedPassages);
    return true;
}

std::vector<TopicJournal::Choice> TopicJournal::choices(const std::vector<std::string> &valid) const {
    std::vector<Choice> result;
    for (const std::string &raw : valid) {
        const std::string key = lower(raw);
        // The empty key aliases Goodbye. OJNA is an engine easter egg, not
        // a player-discoverable conversation topic.
        if (key.empty() || key == "ojna") continue;
        std::string title;
        if (key == "name") title = "Name";
        else if (key == "job") title = "Job";
        else if (key == "heal") title = "Health";
        else if (key == "look") title = "Appearance";
        else if (key == "bye") title = "Goodbye";
        else title = label(key);
        if (title.empty()) continue;
        title[0] = static_cast<char>(title[0] >= 'a' && title[0] <= 'z' ? title[0] - 'a' + 'A' : title[0]);
        auto duplicate = std::find_if(result.begin(), result.end(), [&](const Choice &c) { return c.keyword == key; });
        if (duplicate == result.end()) result.push_back(
            {key, title, key == "bye" ? CHOICE_DISMISS : CHOICE_ACTION});
    }
    auto rank = [](const Choice &c) {
        if (c.keyword == "name") return 0;
        if (c.keyword == "job") return 1;
        if (c.keyword == "heal") return 2;
        if (c.keyword == "look") return 3;
        if (c.keyword == "bye") return 5;
        return 4;
    };
    std::stable_sort(result.begin(), result.end(), [&](const Choice &a, const Choice &b) {
        return rank(a) < rank(b);
    });
    return result;
}
