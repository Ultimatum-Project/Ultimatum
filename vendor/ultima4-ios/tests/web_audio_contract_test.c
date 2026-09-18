// Verify the exact SDL callback contract without opening an output device.
#include <SDL.h>
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include "settings.h"
#include "music.h"
#include "cmixer.h"

SettingsData settings;
bool zu4_settings_write(void) { return true; }
void zu4_error(int level, const char *fmt, ...) { (void)level; (void)fmt; }
static SDL_AudioSpec callback_spec;
static int output_rate, close_count;
SDL_AudioDeviceID SDL_OpenAudioDevice(const char *device, int capture,
    const SDL_AudioSpec *desired, SDL_AudioSpec *obtained, int changes) {
    assert(!device && !capture);
    assert(desired->format == AUDIO_S16SYS && desired->channels == 2);
    assert(!(changes & (SDL_AUDIO_ALLOW_FORMAT_CHANGE | SDL_AUDIO_ALLOW_CHANNELS_CHANGE)));
    assert(changes & SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    *obtained = *desired; obtained->freq = output_rate;
    callback_spec = *obtained; return 2;
}
void SDL_PauseAudioDevice(SDL_AudioDeviceID device, int paused) { assert(device == 2 && !paused); }
void SDL_CloseAudioDevice(SDL_AudioDeviceID device) { assert(device == 2); ++close_count; }
int SDL_OpenAudio(SDL_AudioSpec *desired, SDL_AudioSpec *obtained) {
    (void)desired; (void)obtained; assert(!"Legacy SDL_OpenAudio allows unsafe F32 negotiation"); return -1;
}
static void tone(cm_Event *event) {
    if(event->type == CM_EVENT_SAMPLES)
        for(int n=0;n<event->length;n++) event->buffer[n]=30000;
}
int main(void) {
    for(int rate=44100;rate<=48000;rate+=3900) {
        output_rate=rate; settings.musicVol=10;
        zu4_music_init();
        cm_SourceInfo info={tone,NULL,44100,441000};
        cm_Source *source=cm_new_source(&info); assert(source);cm_play(source);
        cm_Int16 *pcm=calloc((size_t)rate*2,sizeof(*pcm));assert(pcm);
        callback_spec.callback(NULL,(Uint8*)pcm,rate*2*sizeof(*pcm));
        assert(fabs(cm_get_position(source)-1.0)<0.002);
        int peak=0;for(int n=0;n<rate*2;n++)if(abs(pcm[n])>peak)peak=abs(pcm[n]);
        assert(peak>0 && peak<=7500);
        free(pcm);cm_destroy_source(source);zu4_music_deinit();
    }
    assert(close_count==2);
    puts("Web callback: signed-16 stereo, 44.1/48 kHz real-time duration and quarter-scale output passed without speakers");
}
