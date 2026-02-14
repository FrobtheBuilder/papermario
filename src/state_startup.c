#include "common.h"
#include "hud_element.h"
#include "audio/public.h"
#include "fio.h"
#include "sprite.h"
#include "game_modes.h"

#ifdef PC_BUILD
#include <stdio.h>
static void pc_trace_startup(const char* msg) {
    FILE* f = fopen("pc_boot_trace.log", "a");
    if (f) { fprintf(f, "[state_startup] %s\n", msg); fclose(f); }
}
#define PC_TRACE_S(msg) pc_trace_startup(msg)
#else
#define PC_TRACE_S(msg) ((void)0)
#endif

void state_init_startup(void) {
    PC_TRACE_S("state_init_startup");
    gOverrideFlags |= GLOBAL_OVERRIDES_DISABLE_DRAW_FRAME;
    gGameStatusPtr->startupState = 3;
}

void state_step_startup(void) {
    GameStatus* gameStatus = gGameStatusPtr;
    s32 i;

    if (gameStatus->startupState != 0) {
        PC_TRACE_S("countdown");
        gameStatus->startupState--;
        return;
    }

    PC_TRACE_S("reinit starting");

    gOverrideFlags = 0;
    gGameStatusPtr->areaID = 0;
    gGameStatusPtr->context = CONTEXT_WORLD;
    gameStatus->prevArea = -1;
    gameStatus->mapID = 0;
    gameStatus->entryID = 0;
    gGameStatusPtr->debugUnused1 = false;
    gGameStatusPtr->debugScripts = DEBUG_SCRIPTS_NONE;
    gGameStatusPtr->keepUsingPartnerOnMapChange = false;
    gGameStatusPtr->introPart = INTRO_PART_NONE;
    gGameStatusPtr->demoBattleFlags = 0;
    gGameStatusPtr->unk_A9 = -1;
    gGameStatusPtr->demoState = DEMO_STATE_NONE;

    PC_TRACE_S("general_heap_create");
    general_heap_create();
    PC_TRACE_S("clear_render_tasks");
    clear_render_tasks();
    clear_worker_list();
    clear_script_list();
    create_cameras();
#ifdef PC_BUILD
    PC_TRACE_S("spr_init_sprites SKIPPED (needs ROM sprite data)");
#else
    spr_init_sprites(PLAYER_SPRITES_MARIO_WORLD);
#endif
    clear_entity_models();
    clear_animator_list();
    clear_model_data();
    clear_sprite_shading_data();
    reset_background_settings();
    hud_element_set_aux_cache(0, 0);
    hud_element_clear_cache();
    clear_trigger_data();
#ifdef PC_BUILD
    PC_TRACE_S("clear_printers SKIPPED (needs ROM charset)");
#else
    clear_printers();
#endif
    clear_entity_data(false);
    clear_screen_overlays();
    clear_player_status();
    clear_npcs();
    clear_player_data();
    reset_battle_status();
    init_encounter_status();
    clear_effect_data();
    clear_item_entity_data();
    clear_saved_variables();
    initialize_collision();
#ifdef PC_BUILD
    PC_TRACE_S("bgm_init_music_players SKIPPED (needs audio system)");
#else
    bgm_init_music_players();
#endif
    clear_windows();
    partner_initialize_data();
#ifdef PC_BUILD
    PC_TRACE_S("sfx/bgm SKIPPED (no audio system)");
#else
    sfx_clear_sounds();
    bgm_reset_volume();
#endif
    initialize_curtains();

    for (i = 0; i < ARRAY_COUNT(gGameStatusPtr->holdRepeatInterval); i++) {
        gGameStatusPtr->holdRepeatInterval[i] = 4;
        gGameStatusPtr->holdDelayTime[i] = 15;
    }

    PC_TRACE_S("fio_load_globals");
    fio_load_globals();

#ifdef PC_BUILD
    gGameStatusPtr->soundOutputMode = SOUND_OUT_STEREO;
    PC_TRACE_S("sound mode set to stereo (PC)");
#else
    if (gSaveGlobals.useMonoSound == 0) {
        gGameStatusPtr->soundOutputMode = SOUND_OUT_STEREO;
        snd_set_stereo();
    } else {
        gGameStatusPtr->soundOutputMode = SOUND_OUT_MONO;
        snd_set_mono();
    }
#endif

#if VERSION_PAL
    if (gSaveGlobals.language >= 4) {
        gSaveGlobals.language = LANGUAGE_DEFAULT;
    }
    gCurrentLanguage = gSaveGlobals.language;
#endif

    gOverrideFlags &= ~GLOBAL_OVERRIDES_DISABLE_DRAW_FRAME;
    PC_TRACE_S("DISABLE_DRAW_FRAME cleared, setting GAME_MODE_LOGOS");
    set_game_mode(GAME_MODE_LOGOS);
    PC_TRACE_S("reinit complete");
}

void state_drawUI_startup(void) {
    startup_draw_prim_rect(0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, 0, 0, 0, 255);
}
