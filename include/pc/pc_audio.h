#ifndef PC_AUDIO_H
#define PC_AUDIO_H

#include "pc/platform.h"

#ifdef __cplusplus
extern "C" {
#endif

// Audio initialization
void pc_audio_init(u32 sampleRate);
void pc_audio_shutdown(void);

// Audio playback
void pc_audio_play_bgm(const char* name, b32 loop);
void pc_audio_stop_bgm(void);
void pc_audio_pause_bgm(b32 pause);

void pc_audio_play_sfx(u32 sfxId, f32 volume, f32 pan);
void pc_audio_stop_sfx(u32 sfxId);

// Volume control
void pc_audio_set_bgm_volume(f32 volume);
void pc_audio_set_sfx_volume(f32 volume);

#ifdef __cplusplus
}
#endif

#endif // PC_AUDIO_H
