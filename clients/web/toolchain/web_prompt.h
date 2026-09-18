#ifndef ULTIMATUM_WEB_PROMPT_H
#define ULTIMATUM_WEB_PROMPT_H

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>
#include "event.h"

struct WebPromptOption {
    std::string value;
    std::string label;
    bool enabled = true;
};

inline int &webInteractionDepth() { static int depth = 0; return depth; }
struct WebInteractionScope {
    WebInteractionScope() { ++webInteractionDepth(); }
    ~WebInteractionScope() { --webInteractionDepth(); }
    WebInteractionScope(const WebInteractionScope &) = delete;
    WebInteractionScope &operator=(const WebInteractionScope &) = delete;
};

inline std::vector<WebPromptOption> webVendorOptions(const std::string &options,
                                                     const std::vector<std::string> &names,
                                                     bool continuation, bool allowCancel) {
    if (continuation) return {{"\r", "Continue"}};
    std::vector<WebPromptOption> choices;
    std::string keys;
    for (unsigned char key : options)
        if (key > 32 && key < 127 && keys.find(key) == std::string::npos) keys += key;
    for (size_t i = 0; i < options.size(); ++i) {
        unsigned char key = options[i];
        if (key <= 32 || key >= 127 || options.find(key) != i) continue;
        std::string label(1, static_cast<char>(std::toupper(key)));
        if (i < names.size() && !names[i].empty()) label = names[i];
        if (keys == "yn" || keys == "ny") label = key == 'y' ? "Yes" : "No";
        else if (keys == "bs") label = key == 'b' ? "Buy" : "Sell";
        else if (keys == "fa") label = key == 'f' ? "Food" : "Ale";
        choices.push_back({std::string(1, key), label});
    }
    if (allowCancel) choices.push_back({"\033", "Cancel"});
    return choices;
}

// A modal answer is one value, not a stream of keys which can leak into the
// next nested SDL loop. Its generation also rejects stale queued submissions.
struct WebPromptController : public WaitableController<std::string> {
    std::string kind, title, context;
    std::vector<WebPromptOption> options;
    int maxLength;
    bool acceptsText, cancelled = false, submitted = false, completed = false;
    int generation;
    std::string pending;

    WebPromptController(const std::string &kind, const std::string &title,
                        const std::string &context, std::vector<WebPromptOption> options,
                        int maxLength = 0, bool acceptsText = false)
        : kind(kind), title(title), context(context), options(std::move(options)),
          maxLength(maxLength), acceptsText(acceptsText) {
        static int nextGeneration = 0;
        generation = ++nextGeneration;
    }

    bool valid(const std::string &answer) const {
        for (const auto &option : options) if (answer == option.value) return option.enabled;
        if (!acceptsText || answer.size() > static_cast<size_t>(maxLength)) return false;
        if (kind == "amount")
            return !answer.empty() && answer.find_first_not_of("0123456789") == std::string::npos;
        return answer.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890 ") == std::string::npos;
    }

    bool stage(const std::string &answer) {
        if (submitted || completed || !valid(answer)) return false;
        pending = answer;
        submitted = true;
        return true;
    }

    void complete(const std::string &answer) {
        if (completed) return;
        completed = true;
        cancelled = answer == std::string(1, '\033');
        value = cancelled ? "" : answer;
        doneWaiting();
    }

    void dispatch() { if (submitted && !completed) complete(pending); }

    bool keyPressed(int key) override {
        if (submitted || completed) return true;
        if (key == U4_ESC && valid(std::string(1, '\033'))) { complete(std::string(1, '\033')); return true; }
        if (!acceptsText) {
            if (key >= 0 && key < 128) {
                char letter = static_cast<char>(std::tolower(key));
                for (const auto &option : options) {
                    if (!option.enabled) continue;
                    if ((option.value.size() == 1 && option.value[0] == letter) ||
                        (kind == "confirmation" && !option.value.empty() && option.value[0] == letter)) {
                        complete(option.value);
                        return true;
                    }
                }
                if ((key == '\r' || key == ' ') && options.size() == 1) { complete(options[0].value); return true; }
            }
            return false;
        }
        if (key == '\r' || key == '\n') { if (valid(value)) complete(value); return true; }
        if (key == U4_BACKSPACE) { if (!value.empty()) value.pop_back(); return true; }
        if (key >= 32 && key < 127 && value.size() < static_cast<size_t>(maxLength)) {
            std::string next = value + static_cast<char>(key);
            if (valid(next)) { value = next; return true; }
        }
        return false;
    }
};

inline std::string webReadPrompt(const std::string &kind, const std::string &title,
                                 const std::string &context, std::vector<WebPromptOption> options,
                                 int maxLength = 0, bool acceptsText = false, bool *cancelled = nullptr) {
    WebPromptController controller(kind, title, context, std::move(options), maxLength, acceptsText);
    eventHandler->pushController(&controller);
    std::string answer = controller.waitFor();
    if (cancelled) *cancelled = controller.cancelled;
    return answer;
}

#endif
