#ifndef ZU4_TOPICJOURNAL_H
#define ZU4_TOPICJOURNAL_H

#include <string>
#include <vector>

// Records only text already presented to the player. Never feed this class
// unread dialogue responses, map data, or a catalogue of puzzle solutions.
class TopicJournal {
public:
    struct Passage {
        std::string text, source, speaker, place, kind, topic;
        Passage(const std::string &textValue = "", const std::string &sourceValue = "",
                const std::string &speakerValue = "", const std::string &placeValue = "",
                const std::string &kindValue = "", const std::string &topicValue = "")
            : text(textValue), source(sourceValue), speaker(speakerValue), place(placeValue),
              kind(kindValue), topic(topicValue) {}
    };
    const std::vector<Passage> &history() const { return passages; }
    enum ChoiceRole {
        CHOICE_ACTION = 0,
        CHOICE_BACK = 1,
        CHOICE_DISMISS = 2
    };
    struct Choice {
        std::string keyword, title;
        ChoiceRole role;
        Choice(const std::string &keywordValue = "", const std::string &titleValue = "",
               ChoiceRole roleValue = CHOICE_ACTION)
            : keyword(keywordValue), title(titleValue), role(roleValue) {}
    };
    std::vector<Choice> choices(const std::vector<std::string> &validKeywords) const;
    struct Exposure { std::string word, source; };
    void observe(const std::string &presentedText, const std::string &source,
                 const std::string &speaker = "", const std::string &place = "",
                 const std::string &kind = "", const std::string &topic = "");
    bool knows(const std::string &keyword) const;
    // Puzzle suggestions require whole-word exposure, never parser prefixes.
    bool knowsPhrase(const std::string &phrase) const;
    bool knowsExact(const std::string &word) const;
    std::string label(const std::string &keyword) const;
    const std::vector<Exposure> &entries() const { return exposures; }
    bool save(const std::string &path) const;
    bool load(const std::string &path);
    void clear() { exposures.clear(); passages.clear(); }
private:
    std::vector<Exposure> exposures;
    std::vector<Passage> passages;
};
#endif
