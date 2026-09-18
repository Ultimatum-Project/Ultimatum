/*
 * music.c
 * Copyright (C) 2019-2020 R. Danbrook
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 * 
 */

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cmixer.h"

#include "error.h"
#include "music.h"
#include "settings.h"
#include "sound.h"
#include "soundtrack.h"

static int curtrack = TRACK_NONE;
static int prevtrack = TRACK_NONE;
static bool music_enabled = false;
static int active_pack = -1;

static SDL_AudioSpec *spec, *obtained;
static SDL_mutex *audio_mutex;
#ifdef ZU4_WEB
static SDL_AudioDeviceID web_audio_device;
#endif

static cm_Source *track[TRACK_MAX];

static void lock_handler(cm_Event *e) {
	if (e->type == CM_EVENT_LOCK) {
		SDL_LockMutex(audio_mutex);
	}
	if (e->type == CM_EVENT_UNLOCK) {
		SDL_UnlockMutex(audio_mutex);
	}
}
static void unlocked_handler(cm_Event *e) { (void)e; }

void zu4_music_play(int music) {
	if (music < TRACK_NONE || music >= TRACK_MAX) return;
	if (!music_enabled) { prevtrack = music; return; }
	if (music == TRACK_NONE) { zu4_music_stop(); return; }
	if (music_enabled) {
		if (curtrack == music) { return; }
		else {
			zu4_music_stop();
			curtrack = music;
			if (track[curtrack]) cm_play(track[curtrack]);
		}
	}
}

void zu4_music_stop() {
	if (curtrack && track[curtrack] && (cm_get_state(track[curtrack]) != CM_STATE_STOPPED)) {
		cm_stop(track[curtrack]);
	}
	if (curtrack) prevtrack = curtrack;
	curtrack = TRACK_NONE;
}

void zu4_music_fadeout(int msecs) { // Implement later
	zu4_music_stop();
}

void zu4_music_fadein(int msecs, bool loadFromMap) { // Implement later
	zu4_music_play(prevtrack);
}

void zu4_music_vol(double volume) {
	// Every source has to be done independently
	for (int i = 1; i < TRACK_MAX; i++) {
		if (track[i]) cm_set_gain(track[i], volume);
	}
}

int zu4_music_vol_inc() {
	if (++settings.musicVol > MAX_VOLUME) { settings.musicVol = MAX_VOLUME; }
	else { zu4_music_vol((double)settings.musicVol / MAX_VOLUME); }
	return (settings.musicVol * MAX_VOLUME);
}

int zu4_music_vol_dec() {
	if (--settings.musicVol < 0) { settings.musicVol = 0; }
	else { zu4_music_vol((double)settings.musicVol / MAX_VOLUME); }
	return (settings.musicVol * MAX_VOLUME);
}

bool zu4_music_toggle() {
	zu4_music_set_enabled(!music_enabled);
	return music_enabled;
}

bool zu4_music_is_enabled() { return music_enabled; }
int zu4_music_current_track() { return curtrack ? curtrack : prevtrack; }
int zu4_music_active_pack() { return active_pack; }
void zu4_music_set_enabled(bool enabled) {
    enabled = enabled && active_pack >= 0;
    if (enabled == music_enabled) return;
    if (!enabled) zu4_music_stop();
    music_enabled = enabled;
    if (enabled) zu4_music_play(prevtrack);
}

static void zu4_audio_cb(void *userdata, Uint8 *stream, int len) {
	cm_process((cm_Int16*)stream, len / 2);
}

static void destroy_tracks(cm_Source **sources) {
    for (int i = 1; i < TRACK_MAX; ++i) {
        if (sources[i]) cm_destroy_source(sources[i]);
        sources[i] = NULL;
    }
}

bool zu4_music_pack_available(int id) {
    const Zu4Soundtrack *pack = zu4_soundtrack(id);
    if (!pack) return false;
    for (int i = 0; i < 9; ++i) {
        char path[160]; snprintf(path, sizeof(path), "music/%s", pack->files[i]);
        FILE *file = fopen(path, "rb");
        if (!file) return false;
        int byte = fgetc(file); fclose(file);
        if (byte == EOF) return false;
    }
    return true;
}

static bool select_pack(int id, bool persist) {
    const Zu4Soundtrack *pack = zu4_soundtrack(id);
    if (!pack || !audio_mutex || !zu4_music_pack_available(id)) return false;
    if (active_pack == id && settings.soundtrack == id) return true;
    cm_Source *prepared[TRACK_MAX] = {0};
    for (int i = 1; i < TRACK_MAX; ++i) {
        char path[160]; snprintf(path, sizeof(path), "music/%s", pack->files[i-1]);
        prepared[i] = cm_new_source_from_file(path);
        if (!prepared[i]) { destroy_tracks(prepared); return false; }
        cm_set_loop(prepared[i], 1);
        cm_set_gain(prepared[i], (double)settings.musicVol / MAX_VOLUME);
    }
    int old = settings.soundtrack;
    settings.soundtrack = id;
    if (persist && !zu4_settings_write()) {
        settings.soundtrack = old; destroy_tracks(prepared); return false;
    }
    int context = zu4_music_current_track();
    zu4_music_stop();
    destroy_tracks(track);
    memcpy(track, prepared, sizeof(track));
    active_pack = id;
    prevtrack = context;
    if (music_enabled) zu4_music_play(context);
    return true;
}

bool zu4_music_select_pack(int id) { return select_pack(id, true); }

void zu4_music_init() {
	cm_init(44100);
	SDL_InitSubSystem(SDL_INIT_AUDIO);
	audio_mutex = SDL_CreateMutex();
	spec = (SDL_AudioSpec*)malloc(sizeof(SDL_AudioSpec));
	obtained = (SDL_AudioSpec*)calloc(1, sizeof(SDL_AudioSpec));
	
	spec->channels = 2;
	spec->freq = 44100;
	spec->format = AUDIO_S16SYS;
	spec->silence = 0;
#ifdef ZU4_WEB
	/* Emscripten's SDL2 backend runs its ScriptProcessor callback on the
	 * browser main thread. A 512-frame buffer gives a phone only ~11 ms of
	 * headroom at 44.1 kHz, so normal layout/rendering work causes underruns.
	 * 4096 frames trades a little latency for stable music and effects. */
	spec->samples = 4096;
#else
	spec->samples = 512;
#endif
	spec->userdata = 0;
	spec->callback = zu4_audio_cb;
	
#ifdef ZU4_WEB
	/* Keep the callback's signed-16 stereo contract. Web Audio requires F32;
	 * SDL must convert it rather than returning F32 to our Int16 mixer. Only
	 * frequency/buffer changes are safe, and the mixer uses the obtained rate. */
	web_audio_device = SDL_OpenAudioDevice(NULL, 0, spec, obtained,
		SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_SAMPLES_CHANGE);
	if (!web_audio_device) {
#else
	if (SDL_OpenAudio(spec, obtained) < 0) {
#endif
		zu4_error(ZU4_LOG_WRN, "Couldn't open audio: %s\n", SDL_GetError());
		free(spec); free(obtained); obtained = NULL;
		SDL_DestroyMutex(audio_mutex); audio_mutex = NULL;
		return;
	}
	free(spec);
	
	cm_init(obtained->freq);
#ifdef ZU4_WEB
	/* Leave headroom for music plus effects and avoid full-scale browser output.
	 * This applies even to previously saved 100% volume preferences. */
	cm_set_master_gain(0.25);
#endif
	cm_set_lock(lock_handler);
	
	if (!select_pack(settings.soundtrack, false)) select_pack(ZU4_SOUNDTRACK_HURIN, false);
	music_enabled = settings.musicVol > 0 && active_pack >= 0;
	
	zu4_music_vol((double)settings.musicVol / MAX_VOLUME);
	
#ifdef ZU4_WEB
	SDL_PauseAudioDevice(web_audio_device, 0);
#else
	SDL_PauseAudio(0);
#endif
}

void zu4_music_deinit() {
#ifdef ZU4_WEB
	if (web_audio_device) SDL_CloseAudioDevice(web_audio_device);
	web_audio_device = 0;
#else
	SDL_CloseAudio();
#endif
	destroy_tracks(track);
	free(obtained);
	obtained = NULL;
	cm_set_lock(unlocked_handler);
	if (audio_mutex) SDL_DestroyMutex(audio_mutex);
	audio_mutex = NULL;
	active_pack = -1; curtrack = prevtrack = TRACK_NONE; music_enabled = false;
}
