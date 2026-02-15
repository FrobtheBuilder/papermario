/**
 * asset_stubs.c - Stub definitions for asset/image symbols
 *
 * On the N64, these symbols are defined by the build system from binary
 * assets (sprites, textures, palettes, etc.) that are linked into the ROM.
 * On PC, these assets are not yet loaded from extracted files, so we provide
 * zero-initialized stubs to satisfy the linker.
 *
 * Also includes stubs for a few remaining N64 SDK functions, audio microcode
 * symbols, and obfuscated init functions not covered by n64_stubs.c.
 */

#include "common.h"
#include <math.h>

// ============================================================================
// UI image/palette data (ui_*) (745 symbols)
// ============================================================================


// ============================================================================
// ROM segment 09 sprite data (D_09*) (440 symbols)
// ============================================================================


// ============================================================================
// ROM segment 0A effect data (D_0A*) (130 symbols)
// ============================================================================


// ============================================================================
// Global data at RAM addresses (D_80*) (27 symbols)
// ============================================================================


// ============================================================================
// Level-up screen assets (level_up_*) (100 symbols)
// ============================================================================


// ============================================================================
// Battle image assets (battle_*_png / battle_*_pal) (72 symbols)
// ============================================================================


// ============================================================================
// Pause menu images (pause_*_png / pause_*_pal) (29 symbols)
// ============================================================================


// ============================================================================
// Font charset offsets (charset_*) (33 symbols)
// ============================================================================

s32 charset_standard_OFFSET = 0;
s32 charset_standard_pal_OFFSET = 0;
s32 charset_subtitle_OFFSET = 0;
s32 charset_subtitle_pal_OFFSET = 0;
s32 charset_title_OFFSET = 0;

// ============================================================================
// Title screen assets (title_*) (8 symbols)
// ============================================================================

u8 title_languages_de_png[] = {0};
u8 title_languages_es_png[] = {0};
u8 title_languages_fr_png[] = {0};
u8 title_languages_png[] = {0};
u8 title_start_game_de_png[] = {0};
u8 title_start_game_es_png[] = {0};
u8 title_start_game_fr_png[] = {0};
u8 title_start_game_png[] = {0};

// ============================================================================
// Theater backdrop assets (theater_*) (4 symbols)
// ============================================================================


// ============================================================================
// Map area image assets (47 symbols)
// ============================================================================

u8 dro_02_toad_house_blanket_img[] = {0};
u8 hos_03_toad_house_blanket_img[] = {0};
u8 jan_03_toad_house_blanket_img[] = {0};
u8 kkj_20_toad_house_blanket_img[] = {0};
u8 kmr_02_toad_house_blanket_img[] = {0};
u8 kmr_20_toad_house_blanket_img[] = {0};
u8 kpa_91_toad_house_blanket_img[] = {0};
u8 kpa_95_toad_house_blanket_img[] = {0};
u8 mac_01_toad_house_blanket_img[] = {0};
u8 nok_01_toad_house_blanket_img[] = {0};
u8 sam_02_toad_house_blanket_img[] = {0};
u8 sam_06_toad_house_blanket_img[] = {0};

// ============================================================================
// Animation headers (*_header) (20 symbols)
// ============================================================================

s32 cymbal_crush_header[] = {0};
s32 flip_card_1_header[] = {0};
s32 flip_card_2_header[] = {0};
s32 flip_card_3_header[] = {0};
s32 flutter_down_header[] = {0};
s32 get_in_bed_header[] = {0};
s32 horizontal_pipe_curl_header[] = {0};
s32 shiver_header[] = {0};
s32 shock_header[] = {0};
s32 shuffle_cards_header[] = {0};
s32 spirit_capture_header[] = {0};
s32 startle_header[] = {0};
s32 tutankoopa_gather_header[] = {0};
s32 tutankoopa_swirl_1_header[] = {0};
s32 tutankoopa_swirl_2_header[] = {0};
s32 unfurl_header[] = {0};
s32 unused_1_header[] = {0};
s32 unused_2_header[] = {0};
s32 unused_3_header[] = {0};
s32 vertical_pipe_curl_header[] = {0};

// ============================================================================
// Icon image data (inspect/ispy/pulse_stone) (9 symbols)
// ============================================================================


// ============================================================================
// Miscellaneous image/data symbols (2 symbols)
// ============================================================================


// ============================================================================
// Miscellaneous global symbols (6 symbols)
// ============================================================================

BackgroundHeader gBackgroundImage = {0};
HeapNode heap_battleHead = {0};

void decode_yay0(void* src, void* dst) {
    (void)src; (void)dst;
}

void fx_sun_undeclared(void) {
}

// ============================================================================
// N64 SDK function stubs (7 symbols)
// ============================================================================

u32 osAiGetLength(void) {
    return 0;
}

u32 osAiGetStatus(void) {
    return 0;
}

s32 osAiSetFrequency(u32 freq) {
    (void)freq;
    return 0;
}

s32 osAiSetNextBuffer(void* buf, u32 size) {
    (void)buf; (void)size;
    return 0;
}

s32 osSendMesg(OSMesgQueue* mq, OSMesg msg, s32 flag) {
    (void)mq; (void)msg; (void)flag;
    return 0;
}

OSIntMask osSetIntMask(OSIntMask mask) {
    (void)mask;
    return 0;
}

void nuScAddClient(NUScClient* client, OSMesgQueue* mq, NUScMsg msgType) {
    (void)client; (void)mq; (void)msgType;
}

// ============================================================================
// N64 math functions (sins, coss) (2 symbols)
// ============================================================================

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

s16 sins(u16 angle) {
    return (s16)(sinf((f32)angle * (f32)M_PI / 32768.0f) * 32767.0f);
}

s16 coss(u16 angle) {
    return (s16)(cosf((f32)angle * (f32)M_PI / 32768.0f) * 32767.0f);
}

// ============================================================================
// N64 audio microcode symbols (2 symbols)
// ============================================================================

u8 n_aspMainDataStart = 0;
u8 n_aspMainTextStart = 0;

// ============================================================================
// Obfuscated init function stubs (4 symbols)
// ============================================================================

void obfuscated_battle_heap_create(void) {
}

void obfuscated_create_audio_system(void) {
}

void obfuscated_general_heap_create(void) {
}

void obfuscated_load_engine_data(void) {
}

// ============================================================================
// Battle BSS variables (from excluded battle_bss.c and bss/ files)
// ============================================================================

s32 D_800DC060 = 0;
s32 D_800DC4D4 = 0;
s32 D_800DC4E0 = 0;
s32 D_800DC4F0 = 0;
s32 D_800DC4F8 = 0;
s32 gBattleState = 0;
BattleStatus gBattleStatus = {0};
s32 gBattleSubState = 0;
s32 gCurrentBattleID = 0;
struct Battle* gCurrentBattlePtr = NULL;
s32 gCurrentStageID = 0;
struct StageListRow* gCurrentStagePtr = NULL;
s32 gDefeatedBattleState = 0;
s32 gDefeatedBattleSubstate = 0;
s32 gLastDrawBattleState = 0;
struct Battle* gOverrideBattlePtr = NULL;

// NUSys symbols
OSPiHandle* nuPiCartHandle = NULL;
NUSched nusched = {0};
