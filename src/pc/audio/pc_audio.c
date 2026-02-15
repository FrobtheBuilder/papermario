#include "pc/pc_audio.h"
#include <SDL2/SDL_mixer.h>
#include <stdio.h>

#define MAX_SFX_CHANNELS 24

static b32 g_audioInitialized = FALSE;

void pc_audio_init(u32 sampleRate) {
    if (Mix_OpenAudio(sampleRate, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        fprintf(stderr, "Mix_OpenAudio failed: %s\n", Mix_GetError());
        return;
    }

    // Allocate 24 channels to match N64's polyphony
    Mix_AllocateChannels(MAX_SFX_CHANNELS);

    g_audioInitialized = TRUE;

    printf("Audio initialized: %d Hz, %d channels\n", sampleRate, MAX_SFX_CHANNELS);
}

void pc_audio_shutdown(void) {
    if (!g_audioInitialized) return;

    Mix_CloseAudio();
    g_audioInitialized = FALSE;
}

void pc_audio_play_bgm(const char* name, b32 loop) {
    if (!g_audioInitialized) return;

    // TODO: Load and play BGM file
    // For now, this is a stub
    (void)name;
    (void)loop;
}

void pc_audio_stop_bgm(void) {
    if (!g_audioInitialized) return;

    Mix_HaltMusic();
}

void pc_audio_pause_bgm(b32 pause) {
    if (!g_audioInitialized) return;

    if (pause) {
        Mix_PauseMusic();
    } else {
        Mix_ResumeMusic();
    }
}

void pc_audio_play_sfx(u32 sfxId, f32 volume, f32 pan) {
    if (!g_audioInitialized) return;

    // TODO: Load and play SFX
    // For now, this is a stub
    (void)sfxId;
    (void)volume;
    (void)pan;
}

void pc_audio_stop_sfx(u32 sfxId) {
    if (!g_audioInitialized) return;

    // TODO: Stop specific SFX
    (void)sfxId;
}

void pc_audio_set_bgm_volume(f32 volume) {
    if (!g_audioInitialized) return;

    Mix_VolumeMusic((int)(volume * MIX_MAX_VOLUME));
}

void pc_audio_set_sfx_volume(f32 volume) {
    if (!g_audioInitialized) return;

    Mix_Volume(-1, (int)(volume * MIX_MAX_VOLUME));
}
