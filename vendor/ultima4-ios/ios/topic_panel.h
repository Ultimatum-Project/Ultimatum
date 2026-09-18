#ifndef ZU4_TOPIC_PANEL_H
#define ZU4_TOPIC_PANEL_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int zu4_topic_panel_is_visible(void);
int zu4_topic_panel_direction_active(void);
int zu4_topic_panel_choose_direction(const char *direction);
typedef void (*Zu4TopicSubmit)(const char *keyword, int textEntry, void *context);
typedef enum Zu4TopicPanelStyle {
    ZU4_TOPIC_PANEL_STANDARD = 0,
    ZU4_TOPIC_PANEL_COMPACT_MENU = 1,
    ZU4_TOPIC_PANEL_FULLSCREEN = 2,
    ZU4_TOPIC_PANEL_CONTROLS_ONLY = 3,
    // Fixed-page, information-dense screens. These deliberately disable
    // scrolling and use compact controls so every offered action fits.
    ZU4_TOPIC_PANEL_DENSE_FULLSCREEN = 4,
    // At most eight party members, all visible with a fixed Cancel header.
    ZU4_TOPIC_PANEL_PARTY_SELECTION = 5,
    // Long read-only attribution/reference text; Back remains in the header.
    ZU4_TOPIC_PANEL_READING_FULLSCREEN = 6
} Zu4TopicPanelStyle;
typedef enum Zu4TopicChoiceRole {
    ZU4_TOPIC_CHOICE_ACTION = 0,
    ZU4_TOPIC_CHOICE_BACK = 1,
    ZU4_TOPIC_CHOICE_DISMISS = 2
} Zu4TopicChoiceRole;
void zu4_topic_panel_show(const char *text, const char **keywords, const char **labels, const int *roles,
                         int count, int maxLength, int nameEntry, int compactDetails,
                         Zu4TopicPanelStyle style, Zu4TopicSubmit submit, void *context);
typedef struct Zu4JournalNote { uint64_t identifier; int passage; const char *text; } Zu4JournalNote;
typedef struct Zu4JournalAccess {
    int (*favorite)(int passage);
    int (*toggleFavorite)(int passage);
    uint64_t (*attachedNote)(int passage);
    int (*noteCount)(void);
    int (*noteAt)(int index, Zu4JournalNote *note);
    uint64_t (*saveNote)(int passage, uint64_t identifier, const char *text);
    int (*deleteNote)(uint64_t identifier);
    void (*textInput)(int active);
} Zu4JournalAccess;
int zu4_mobile_journal_favorite(int passage);
int zu4_mobile_journal_toggle_favorite(int passage);
uint64_t zu4_mobile_journal_attached_note(int passage);
int zu4_mobile_journal_note_count(void);
int zu4_mobile_journal_note_at(int index, Zu4JournalNote *note);
uint64_t zu4_mobile_journal_save_note(int passage, uint64_t identifier, const char *text);
int zu4_mobile_journal_delete_note(uint64_t identifier);
int zu4_journal_panel_is_visible(void);
void zu4_journal_panel_show(const char **texts, const char **sources, const char **speakers,
                            const char **places, const char **kinds, const char **topics,
                            const int *indices, int count, const Zu4JournalAccess *access);
#ifdef __cplusplus
}
#endif
#endif
