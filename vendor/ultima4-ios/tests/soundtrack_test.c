#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "settings.h"
#include "music.h"
#include "soundtrack.h"
#include "cmixer.h"
SettingsData settings;
static bool save_ok = true;
bool zu4_settings_write(void) { return save_ok; }
void zu4_error(int level, const char *fmt, ...) { (void)level; (void)fmt; }
static void audible(void) {
    cm_Int16 pcm[4096]; long energy = 0;
    for (int block = 0; block < 30; ++block) {
        cm_process(pcm, 4096);
        for (int n = 0; n < 4096; ++n) energy += pcm[n] < 0 ? -pcm[n] : pcm[n];
    }
    assert(energy > 0);
}
int main(void) {
    settings.musicVol = 8; settings.soundtrack = ZU4_SOUNDTRACK_HURIN;
    assert(zu4_soundtrack_from_key("bad") == -1 && !zu4_soundtrack(99));
    zu4_music_init();
    if (getenv("ZU4_EMPTY_AUDIO")) {
        assert(zu4_music_active_pack() == -1 && !zu4_music_is_enabled());
        zu4_music_set_enabled(true); assert(!zu4_music_is_enabled());
        zu4_music_play(1); zu4_music_vol(0); zu4_music_toggle();
        assert(!zu4_music_select_pack(0)); zu4_music_deinit();
        puts("empty public music packaging starts and shuts down safely"); return 0;
    }
    assert(zu4_music_active_pack() == 0);
    assert(ZU4_SOUNDTRACK_COUNT == 1 && zu4_music_pack_available(ZU4_SOUNDTRACK_HURIN));
    for (int song = 1; song < TRACK_MAX; ++song) {
        zu4_music_play(song);
        assert(zu4_music_select_pack(ZU4_SOUNDTRACK_HURIN));
        assert(zu4_music_current_track() == song && settings.musicVol == 8);
        audible();
    }
    assert(!zu4_music_select_pack(-1) && !zu4_music_select_pack(99));
    zu4_music_set_enabled(false);
    zu4_music_play(TRACK_DUNGEON);
    assert(zu4_music_select_pack(ZU4_SOUNDTRACK_HURIN) && !zu4_music_is_enabled() && zu4_music_current_track() == TRACK_DUNGEON);
    zu4_music_set_enabled(true); audible();
    save_ok = false;
    assert(zu4_music_select_pack(ZU4_SOUNDTRACK_HURIN));
    save_ok = true;
    for (int n = 0; n < 20; ++n) assert(zu4_music_select_pack(ZU4_SOUNDTRACK_HURIN));
    zu4_music_deinit(); zu4_music_deinit();
    puts("xu4 soundtrack: nine decoded songs, context/mute preservation, invalid selection and repeated loading passed");
}
