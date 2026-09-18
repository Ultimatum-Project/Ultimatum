#ifndef ZU4_SOUNDTRACK_H
#define ZU4_SOUNDTRACK_H
#include <string.h>

enum { ZU4_SOUNDTRACK_HURIN, ZU4_SOUNDTRACK_COUNT };
typedef struct {
    const char *key, *name, *description, *credits;
    const char *files[9];
} Zu4Soundtrack;
static const Zu4Soundtrack zu4_soundtracks[] = {
    {"hurin", "xu4 soundtrack", "The soundtrack bundled with xu4",
     "xu4 soundtrack\n\nOriginal score: Kenneth W. Arnold / Origin Systems. MIDI arrangement: Telavar. Roland SC-55st recordings: Hurin (2013).",
     {"hurin/wanderer.ogg", "hurin/towne.ogg", "hurin/shrines.ogg", "hurin/shopping.ogg", "hurin/rulebritannia.ogg", "hurin/fanfare_of_lord_british.ogg", "hurin/dungeons.ogg", "hurin/combat.ogg", "hurin/castles.ogg"}},
};
static inline const Zu4Soundtrack *zu4_soundtrack(int id) {
    return id >= 0 && id < ZU4_SOUNDTRACK_COUNT ? &zu4_soundtracks[id] : NULL;
}
static inline int zu4_soundtrack_from_key(const char *key) {
    for (int i = 0; key && i < ZU4_SOUNDTRACK_COUNT; ++i)
        if (!strcmp(key, zu4_soundtracks[i].key)) return i;
    return -1;
}
#endif
