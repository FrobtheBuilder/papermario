#include "common.h"
#include "nu/nusys.h"
#include "audio/public.h"
#include "ld_addrs.h"
#include "hud_element.h"
#include "sprite.h"
#include "overlay.h"
#include "game_modes.h"

#ifdef PC_BUILD
#include <stdio.h>
void pc_trace_ml(const char* msg) {
    FILE* f = fopen("pc_boot_trace.log", "a");
    if (f) { fprintf(f, "[load_engine] %s\n", msg); fclose(f); }
}
#define PC_TRACE_ML(msg) pc_trace_ml(msg)
#else
#define PC_TRACE_ML(msg) ((void)0)
#endif

s32 gOverrideFlags;
s32 gTimeFreezeMode;
u16** nuGfxCfb;
BSS s16 SoftResetDelay;

DisplayContext D_80164000[2];

s8 gGameStepDelayAmount = 1;
s8 gGameStepDelayCount = 5;

GameStatus gGameStatus = {
    .curButtons = { 0 },
    .pressedButtons = { 0 },
    .heldButtons = { 0 },
    .prevButtons = { 0 },
    .stickX = { 0 },
    .stickY = { 0 },
    .holdDelayTime = { 0 },
    .holdRepeatInterval = { 0 },
};

GameStatus* gGameStatusPtr = &gGameStatus;
s16 SoftResetOverlayAlpha = 0;
s16 SoftResetState = 0;
s32 D_800741A4 = 0;

Mtx MasterIdentityMtx = RDP_MATRIX(
    1.000000, 0.000000, 0.000000, 0.000000,
    0.000000, 1.000000, 0.000000, 0.000000,
    0.000000, 0.000000, 1.000000, 0.000000,
    0.000000, 0.000000, 0.000000, 1.000000
);

s32 D_800741E8[2] = {0, 0}; // padding?
u16 gMatrixListPos = 0;
u16 D_800741F2 = 0;
s32 gCurrentDisplayContextIndex = 0;
s32 gPauseBackgroundFade = 0;
s32 D_800741FC = 0;

void gfx_init_state(void);
void gfx_draw_background(void);

void step_game_loop(void) {
    PlayerData* playerData = &gPlayerData;
    const int MAX_GAME_TIME = 1000*60*60*60 - 1; // 1000 hours minus one frame at 60 fps

#if !VERSION_JP
    update_input();
#endif

    gGameStatusPtr->frameCounter++;

    playerData->frameCounter += 2;
    if (playerData->frameCounter > MAX_GAME_TIME) {
        playerData->frameCounter = MAX_GAME_TIME;
    }

#if VERSION_JP
    update_input();
#endif

    update_max_rumble_duration();

    if (gGameStepDelayCount != 0) {
        gGameStepDelayCount-- ;
        if (gGameStepDelayCount == 0) {
            gGameStepDelayCount = gGameStepDelayAmount;
        } else {
            return;
        }
    }

    mdl_reset_transform_flags();
    npc_iter_no_op();
    update_workers();
    update_triggers();
    update_scripts();
    update_messages();
    update_hud_elements();
    step_game_mode();
    update_entities();
    func_80138198();
#ifndef PC_BUILD
    bgm_update_music_control();
    update_ambient_sounds();
    sfx_update_env_sound_params();
#endif
    update_windows();
    update_curtains();

    if (gOverrideFlags & GLOBAL_OVERRIDES_SOFT_RESET) {
        switch (SoftResetState) {
            case 0:
                gOverrideFlags |= GLOBAL_OVERRIDES_200;
                disable_player_input();

                if (SoftResetOverlayAlpha == 255) {
                    SoftResetState = 1;
                    SoftResetDelay = 3;
                } else {
                    SoftResetOverlayAlpha += 10;
                    if (SoftResetOverlayAlpha > 255) {
                        SoftResetOverlayAlpha = 255;
                    }
                }
                break;
            case 1:
                gOverrideFlags |= GLOBAL_OVERRIDES_DISABLE_DRAW_FRAME;
                SoftResetDelay--;
                if (SoftResetDelay == 0) {
                    sfx_stop_env_sounds();
                    set_game_mode(GAME_MODE_STARTUP);
                    gOverrideFlags &= ~GLOBAL_OVERRIDES_SOFT_RESET;
                }
                break;
        }
    } else {
        SoftResetOverlayAlpha = 0;
        SoftResetState = 0;
    }

    if (gOverrideFlags & GLOBAL_OVERRIDES_DISABLE_BATTLES) {
        gOverrideFlags |= GLOBAL_OVERRIDES_PREV_DISABLE_BATTLES;
    } else {
        gOverrideFlags &= ~GLOBAL_OVERRIDES_PREV_DISABLE_BATTLES;
    }

    if (gOverrideFlags & GLOBAL_OVERRIDES_200) {
        gOverrideFlags |= GLOBAL_OVERRIDES_PREV_200;
    } else {
        gOverrideFlags &= ~GLOBAL_OVERRIDES_PREV_200;
    }

    if (gOverrideFlags & GLOBAL_OVERRIDES_400) {
        gOverrideFlags |= GLOBAL_OVERRIDES_PREV_400;
    } else {
        gOverrideFlags &= ~GLOBAL_OVERRIDES_PREV_400;
    }

    if (gOverrideFlags & GLOBAL_OVERRIDES_800) {
        gOverrideFlags |= GLOBAL_OVERRIDES_PREV_800;
    } else {
        gOverrideFlags &= ~GLOBAL_OVERRIDES_PREV_800;
    }

    // Unused rand_int used to advance the global random seed each visual frame
    rand_int(1);
}

void gfx_task_background(void) {
    gDisplayContext = &D_80164000[gCurrentDisplayContextIndex];
    gMainGfxPos = &gDisplayContext->backgroundGfx[0];

    gfx_init_state();
    gfx_draw_background();

    gDPFullSync(gMainGfxPos++);
    gSPEndDisplayList(gMainGfxPos++);

    // TODO these << 3 >> 3 shouldn't be necessary. There's almost definitely something we're missing here...
    ASSERT((s32)((u32)((gMainGfxPos - gDisplayContext->backgroundGfx) << 3) >> 3) < ARRAY_COUNT(
               gDisplayContext->backgroundGfx))

    nuGfxTaskStart(&gDisplayContext->backgroundGfx[0], (u32)(gMainGfxPos - gDisplayContext->backgroundGfx) * 8,
                   NU_GFX_UCODE_F3DEX2, NU_SC_NOSWAPBUFFER);
}

void gfx_draw_frame(void) {
    gMatrixListPos = 0;
    gMainGfxPos = &gDisplayContext->mainGfx[0];

    if (gOverrideFlags & GLOBAL_OVERRIDES_DISABLE_DRAW_FRAME) {
        gCurrentDisplayContextIndex = gCurrentDisplayContextIndex ^ 1;
        return;
    }

    gSPMatrix(gMainGfxPos++, &MasterIdentityMtx, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);

#ifdef PC_BUILD
    // Test triangle to verify rendering pipeline (remove once real rendering works)
    {
        static int once = 0;
        if (once == 0) {
            TRACE_WORLD("gfx_draw_frame: adding test triangle");
            once = 1;
        }

        // Set up a simple orthographic projection: left=-160, right=160, top=120, bottom=-120, near=1, far=1000
        static Mtx orthoProj;
        if (once == 1) {
            guOrtho(&orthoProj, -160.0f, 160.0f, -120.0f, 120.0f, 1.0f, 1000.0f, 1.0f);
            once = 2;
        }

        gSPMatrix(gMainGfxPos++, &orthoProj, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
        gSPMatrix(gMainGfxPos++, &MasterIdentityMtx, G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);

        static Vtx testVtx[3] = {
            {{ {-64, -64, -100}, 0, {0, 0}, {255, 0, 0, 255} }},
            {{ { 64, -64, -100}, 0, {0, 0}, {0, 255, 0, 255} }},
            {{ {  0,  64, -100}, 0, {0, 0}, {0, 0, 255, 255} }},
        };
        gDPPipeSync(gMainGfxPos++);
        gDPSetCycleType(gMainGfxPos++, G_CYC_1CYCLE);
        gDPSetRenderMode(gMainGfxPos++, G_RM_AA_OPA_SURF, G_RM_AA_OPA_SURF2);
        gDPSetCombineMode(gMainGfxPos++, G_CC_SHADE, G_CC_SHADE);
        gSPClearGeometryMode(gMainGfxPos++, G_ZBUFFER | G_LIGHTING | G_CULL_BOTH);
        gSPVertex(gMainGfxPos++, testVtx, 3, 0);
        gSP1Triangle(gMainGfxPos++, 0, 1, 2, 0);
    }
#endif

    spr_render_init();

    if (!(gOverrideFlags & GLOBAL_OVERRIDES_DISABLE_RENDER_WORLD)) {
        render_frame(false);
    }

    player_render_interact_prompts();
    func_802C3EE4();
    render_screen_overlay_backUI();
    render_workers_backUI();
    render_hud_elements_backUI();
    render_effects_UI();
    render_game_mode_backUI();

    if (!(gOverrideFlags & GLOBAL_OVERRIDES_WINDOWS_OVER_CURTAINS)) {
        render_window_root();
    }

    if (!(gOverrideFlags & GLOBAL_OVERRIDES_DISABLE_RENDER_WORLD) && gGameStatusPtr->debugScripts == DEBUG_SCRIPTS_NONE) {
        render_frame(true);
    }

    if (!(gOverrideFlags & GLOBAL_OVERRIDES_MESSAGES_OVER_CURTAINS)
        && !(gOverrideFlags & GLOBAL_OVERRIDES_MESSAGES_OVER_FRONTUI)
    ) {
        render_messages();
    }

    render_workers_frontUI();
    render_hud_elements_frontUI();
    render_screen_overlay_frontUI();

    if (!(gOverrideFlags & GLOBAL_OVERRIDES_MESSAGES_OVER_CURTAINS)
        && (gOverrideFlags & GLOBAL_OVERRIDES_MESSAGES_OVER_FRONTUI)
    ) {
        render_messages();
    }

    render_curtains();

    if (gOverrideFlags & GLOBAL_OVERRIDES_MESSAGES_OVER_CURTAINS) {
        render_messages();
    }

    if (gOverrideFlags & GLOBAL_OVERRIDES_WINDOWS_OVER_CURTAINS) {
        render_window_root();
    }

    render_game_mode_frontUI();

    if (gOverrideFlags & GLOBAL_OVERRIDES_SOFT_RESET) {
        switch (SoftResetState) {
            case 0:
            case 1:
                _render_transition_stencil(OVERLAY_SCREEN_MARIO, SoftResetOverlayAlpha, nullptr);
                break;
        }
    }

    ASSERT((s32)(((u32)(gMainGfxPos - gDisplayContext->mainGfx) << 3) >> 3) < ARRAY_COUNT(gDisplayContext->mainGfx));

    gDPFullSync(gMainGfxPos++);
    gSPEndDisplayList(gMainGfxPos++);

    nuGfxTaskStart(gDisplayContext->mainGfx, (u32)(gMainGfxPos - gDisplayContext->mainGfx) * 8, NU_GFX_UCODE_F3DEX2,
                   NU_SC_TASK_LODABLE | NU_SC_SWAPBUFFER);
    gCurrentDisplayContextIndex = gCurrentDisplayContextIndex ^ 1;
    crash_screen_set_draw_info(nuGfxCfb_ptr, SCREEN_WIDTH, SCREEN_HEIGHT);
}

void load_engine_data(void) {
    s32 i;

#ifdef PC_BUILD
    PC_TRACE_ML("DMA_COPY_SEGMENT skipped (code already in DLL)");
#else
    DMA_COPY_SEGMENT(engine4);
    DMA_COPY_SEGMENT(engine1);
    DMA_COPY_SEGMENT(evt);
    DMA_COPY_SEGMENT(entity);
    DMA_COPY_SEGMENT(engine2);
    DMA_COPY_SEGMENT(font_width);
#endif

    PC_TRACE_ML("setting game status fields");
    gOverrideFlags = 0;
    gGameStatusPtr->unk_79 = 0;
    gGameStatusPtr->backgroundFlags = 0;
    gGameStatusPtr->musicEnabled = true;
    gGameStatusPtr->healthBarsEnabled = true;
    gGameStatusPtr->introPart = INTRO_PART_NONE;
    gGameStatusPtr->demoBattleFlags = 0;
    gGameStatusPtr->multiplayerEnabled = false;
    gGameStatusPtr->altViewportOffset.x = -8;
    gGameStatusPtr->altViewportOffset.y = 4;
    gTimeFreezeMode = TIME_FREEZE_NONE;
    gGameStatusPtr->debugQuizmo = gGameStatusPtr->unk_13C = 0;
    gGameStepDelayCount = 5;
    gGameStatusPtr->saveCount = 0;
    PC_TRACE_ML("fio_init_flash");
    fio_init_flash();
    PC_TRACE_ML("clear_input");
    clear_input();
    PC_TRACE_ML("general_heap_create");
    general_heap_create();
    PC_TRACE_ML("clear_render_tasks");
    clear_render_tasks();
    PC_TRACE_ML("clear_worker_list");
    clear_worker_list();
    PC_TRACE_ML("clear_script_list");
    clear_script_list();
    PC_TRACE_ML("create_cameras");
    create_cameras();
    PC_TRACE_ML("clear_player_status");
    clear_player_status();
#ifdef PC_BUILD
    PC_TRACE_ML("spr_init_sprites SKIPPED (needs endian conversion)");
#else
    PC_TRACE_ML("spr_init_sprites");
    spr_init_sprites(PLAYER_SPRITES_MARIO_WORLD);
#endif
    PC_TRACE_ML("clear_entity_models");
    clear_entity_models();
    PC_TRACE_ML("clear_animator_list");
    clear_animator_list();
    PC_TRACE_ML("clear_model_data");
    clear_model_data();
    PC_TRACE_ML("clear_sprite_shading_data");
    clear_sprite_shading_data();
    PC_TRACE_ML("reset_background_settings");
    reset_background_settings();
    PC_TRACE_ML("clear_character_set");
    clear_character_set();
#ifdef PC_BUILD
    PC_TRACE_ML("clear_printers SKIPPED (load_font needs ROM charset)");
#else
    PC_TRACE_ML("clear_printers");
    clear_printers();
#endif
    PC_TRACE_ML("clear_game_mode");
    clear_game_mode();
    PC_TRACE_ML("clear_npcs");
    clear_npcs();
    PC_TRACE_ML("hud_element_clear_cache");
    hud_element_clear_cache();
    PC_TRACE_ML("clear_trigger_data");
    clear_trigger_data();
    PC_TRACE_ML("clear_entity_data");
    clear_entity_data(false);
    PC_TRACE_ML("clear_player_data");
    clear_player_data();
    PC_TRACE_ML("init_encounter_status");
    init_encounter_status();
    PC_TRACE_ML("clear_screen_overlays");
    clear_screen_overlays();
    PC_TRACE_ML("clear_effect_data");
    clear_effect_data();
    PC_TRACE_ML("clear_saved_variables");
    clear_saved_variables();
    PC_TRACE_ML("clear_item_entity_data");
    clear_item_entity_data();
#ifdef PC_BUILD
    PC_TRACE_ML("bgm/sfx init SKIPPED (no audio system)");
#else
    PC_TRACE_ML("bgm_reset_sequence_players");
    bgm_reset_sequence_players();
    PC_TRACE_ML("reset_ambient_sounds");
    reset_ambient_sounds();
    PC_TRACE_ML("sfx_clear_sounds");
    sfx_clear_sounds();
#endif
    PC_TRACE_ML("clear_windows");
    clear_windows();
    PC_TRACE_ML("initialize_curtains");
    initialize_curtains();
    PC_TRACE_ML("poll_rumble");
    poll_rumble();

    for (i = 0; i < ARRAY_COUNT(gGameStatusPtr->holdRepeatInterval); i++) {
        gGameStatusPtr->holdRepeatInterval[i] = 3;
        gGameStatusPtr->holdDelayTime[i] = 12;
    }

    PC_TRACE_ML("set_game_mode(GAME_MODE_STARTUP)");
    gOverrideFlags |= GLOBAL_OVERRIDES_DISABLE_DRAW_FRAME;
    set_game_mode(GAME_MODE_STARTUP);
    PC_TRACE_ML("load_engine_data complete");
}

/// Time freeze modes:
///  0: none
///  1: NPCs move, can't be interacted with
///  2: NPCs don't move, no partner ability, can't interact, can't use exits
///  3: NPCs don't more or animate
///  4: NPCs can move, animations don't update, can use exits
void set_time_freeze_mode(s32 mode) {
    switch (mode) {
        case TIME_FREEZE_NONE:
            gTimeFreezeMode = mode;
            gOverrideFlags &= ~(GLOBAL_OVERRIDES_800 | GLOBAL_OVERRIDES_400 | GLOBAL_OVERRIDES_200 | GLOBAL_OVERRIDES_DISABLE_BATTLES);
            resume_all_group(EVT_GROUP_FLAG_INTERACT | EVT_GROUP_FLAG_MENUS);
            break;
        case TIME_FREEZE_PARTIAL:
            gTimeFreezeMode = mode;
            gOverrideFlags &= ~(GLOBAL_OVERRIDES_800 | GLOBAL_OVERRIDES_400 | GLOBAL_OVERRIDES_200);
            gOverrideFlags |= GLOBAL_OVERRIDES_DISABLE_BATTLES;
            suspend_all_group(EVT_GROUP_FLAG_INTERACT);
            break;
        case TIME_FREEZE_FULL:
            gTimeFreezeMode = mode;
            gOverrideFlags &= ~(GLOBAL_OVERRIDES_400 | GLOBAL_OVERRIDES_800);
            gOverrideFlags |= GLOBAL_OVERRIDES_200 | GLOBAL_OVERRIDES_DISABLE_BATTLES;
            suspend_all_group(EVT_GROUP_FLAG_MENUS);
            break;
        case TIME_FREEZE_POPUP_MENU:
            gTimeFreezeMode = mode;
            gOverrideFlags &= ~GLOBAL_OVERRIDES_800;
            gOverrideFlags |= GLOBAL_OVERRIDES_400 | GLOBAL_OVERRIDES_200 | GLOBAL_OVERRIDES_DISABLE_BATTLES;
            suspend_all_group(EVT_GROUP_FLAG_MENUS);
            break;
        case TIME_FREEZE_EXIT:
            gTimeFreezeMode = mode;
            gOverrideFlags |= GLOBAL_OVERRIDES_800 | GLOBAL_OVERRIDES_400 | GLOBAL_OVERRIDES_200 | GLOBAL_OVERRIDES_DISABLE_BATTLES;
            break;
    }
}

s32 get_time_freeze_mode(void) {
    return gTimeFreezeMode;
}

#if VERSION_IQUE
static const f32 rodata_padding[] = {0.0f, 0.0f};
#endif
