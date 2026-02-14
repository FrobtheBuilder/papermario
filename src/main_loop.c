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

#ifdef PC_BUILD
    static int step_count = 0;
    step_count++;
    {
        char buf[128];
        snprintf(buf, sizeof(buf), "[step_game_loop] #%d delay=%d", step_count, (int)gGameStepDelayCount);
        pc_trace_ml(buf);
    }
#endif

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

#ifdef PC_BUILD
    pc_trace_ml("[step_game_loop] past delay gate, running game logic");
#endif

#ifdef PC_BUILD
    #define TRACE_STEP(name) pc_trace_ml("[step_game_loop] " name)
#else
    #define TRACE_STEP(name) ((void)0)
#endif
    TRACE_STEP("mdl_reset_transform_flags");
    mdl_reset_transform_flags();
    TRACE_STEP("npc_iter_no_op");
    npc_iter_no_op();
    TRACE_STEP("update_workers");
    update_workers();
    TRACE_STEP("update_triggers");
    update_triggers();
    TRACE_STEP("update_scripts");
    update_scripts();
    TRACE_STEP("update_messages");
    update_messages();
    TRACE_STEP("update_hud_elements");
    update_hud_elements();
    TRACE_STEP("step_game_mode");
    step_game_mode();
    TRACE_STEP("update_entities");
    update_entities();
    TRACE_STEP("func_80138198");
    func_80138198();
#ifndef PC_BUILD
    TRACE_STEP("bgm_update_music_control");
    bgm_update_music_control();
    TRACE_STEP("update_ambient_sounds");
    update_ambient_sounds();
    TRACE_STEP("sfx_update_env_sound_params");
    sfx_update_env_sound_params();
#endif
    TRACE_STEP("update_windows");
    update_windows();
    TRACE_STEP("update_curtains");
    update_curtains();
    TRACE_STEP("step_game_loop game logic done");

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

#ifdef PC_BUILD
    {
        u32 numCmds = (u32)(gMainGfxPos - gDisplayContext->backgroundGfx);
        u32* words = (u32*)&gDisplayContext->backgroundGfx[0];
        char buf[512];
        snprintf(buf, sizeof(buf),
            "[gfx_task_bg] numCmds=%u gMainGfxPos=%p bgGfx=%p G_ENDDL=0x%x",
            numCmds, (void*)gMainGfxPos, (void*)&gDisplayContext->backgroundGfx[0],
            (unsigned)G_ENDDL);
        pc_trace_ml(buf);
        /* Dump first 10 commands from source memory */
        for (u32 i = 0; i < numCmds && i < 10; i++) {
            snprintf(buf, sizeof(buf), "  bg_cmd[%u]: w0=0x%08x w1=0x%08x",
                     i, words[i*2], words[i*2+1]);
            pc_trace_ml(buf);
        }
    }
#endif

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
#ifdef PC_BUILD
        static int skip_count = 0;
        skip_count++;
        if (skip_count <= 5 || (skip_count % 60 == 0)) {
            char buf[128];
            snprintf(buf, sizeof(buf), "[gfx_draw_frame] SKIP #%d (DISABLE_DRAW_FRAME set, flags=0x%x)", skip_count, (int)gOverrideFlags);
            pc_trace_ml(buf);
        }
#endif
        gCurrentDisplayContextIndex = gCurrentDisplayContextIndex ^ 1;
        return;
    }

#ifdef PC_BUILD
    {
        static int draw_count = 0;
        draw_count++;
        if (draw_count <= 5 || (draw_count % 60 == 0)) {
            char buf[256];
            snprintf(buf, sizeof(buf), "[gfx_draw_frame] DRAWING #%d (flags=0x%x) NEWBUILD gMainGfxPos=%p MasterIdentityMtx=%p", draw_count, (int)gOverrideFlags, (void*)gMainGfxPos, (void*)&MasterIdentityMtx);
            pc_trace_ml(buf);
            pc_trace_ml("[gfx_draw_frame] TRACE_V2 about to gSPMatrix");
        }
    }
#endif

    pc_trace_ml("[gfx_draw_frame] about to gSPMatrix MasterIdentityMtx");
    gSPMatrix(gMainGfxPos++, &MasterIdentityMtx, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);

    pc_trace_ml("[gfx_draw_frame] about to spr_render_init");
    spr_render_init();

    pc_trace_ml("[gfx_draw_frame] spr_render_init done");

    if (!(gOverrideFlags & GLOBAL_OVERRIDES_DISABLE_RENDER_WORLD)) {
        pc_trace_ml("[gfx_draw_frame] render_frame(false)");
        render_frame(false);
    }

    pc_trace_ml("[gfx_draw_frame] player_render_interact_prompts");
    player_render_interact_prompts();
    pc_trace_ml("[gfx_draw_frame] func_802C3EE4");
    func_802C3EE4();
    pc_trace_ml("[gfx_draw_frame] render_screen_overlay_backUI");
    render_screen_overlay_backUI();
    pc_trace_ml("[gfx_draw_frame] render_workers_backUI");
    render_workers_backUI();
    pc_trace_ml("[gfx_draw_frame] render_hud_elements_backUI");
    render_hud_elements_backUI();
    pc_trace_ml("[gfx_draw_frame] render_effects_UI");
    render_effects_UI();
    pc_trace_ml("[gfx_draw_frame] render_game_mode_backUI");
    render_game_mode_backUI();

    if (!(gOverrideFlags & GLOBAL_OVERRIDES_WINDOWS_OVER_CURTAINS)) {
        pc_trace_ml("[gfx_draw_frame] render_window_root");
        render_window_root();
    }

    if (!(gOverrideFlags & GLOBAL_OVERRIDES_DISABLE_RENDER_WORLD) && gGameStatusPtr->debugScripts == DEBUG_SCRIPTS_NONE) {
        pc_trace_ml("[gfx_draw_frame] render_frame(true)");
        render_frame(true);
    }

    if (!(gOverrideFlags & GLOBAL_OVERRIDES_MESSAGES_OVER_CURTAINS)
        && !(gOverrideFlags & GLOBAL_OVERRIDES_MESSAGES_OVER_FRONTUI)
    ) {
        pc_trace_ml("[gfx_draw_frame] render_messages");
        render_messages();
    }

    pc_trace_ml("[gfx_draw_frame] render_workers_frontUI");
    render_workers_frontUI();
    pc_trace_ml("[gfx_draw_frame] render_hud_elements_frontUI");
    render_hud_elements_frontUI();
    pc_trace_ml("[gfx_draw_frame] render_screen_overlay_frontUI");
    render_screen_overlay_frontUI();

    if (!(gOverrideFlags & GLOBAL_OVERRIDES_MESSAGES_OVER_CURTAINS)
        && (gOverrideFlags & GLOBAL_OVERRIDES_MESSAGES_OVER_FRONTUI)
    ) {
        render_messages();
    }

    pc_trace_ml("[gfx_draw_frame] render_curtains");
    render_curtains();

    if (gOverrideFlags & GLOBAL_OVERRIDES_MESSAGES_OVER_CURTAINS) {
        render_messages();
    }

    if (gOverrideFlags & GLOBAL_OVERRIDES_WINDOWS_OVER_CURTAINS) {
        render_window_root();
    }

    pc_trace_ml("[gfx_draw_frame] render_game_mode_frontUI");
    render_game_mode_frontUI();
    pc_trace_ml("[gfx_draw_frame] render_game_mode_frontUI done");

    if (gOverrideFlags & GLOBAL_OVERRIDES_SOFT_RESET) {
        switch (SoftResetState) {
            case 0:
            case 1:
                _render_transition_stencil(OVERLAY_SCREEN_MARIO, SoftResetOverlayAlpha, nullptr);
                break;
        }
    }

    pc_trace_ml("[gfx_draw_frame] about to ASSERT gfx overflow check");
#ifdef PC_BUILD
    {
        // Use ptrdiff_t to avoid 32-bit truncation on 64-bit
        ptrdiff_t gfxUsed = gMainGfxPos - gDisplayContext->mainGfx;
        char _assertBuf[128];
        snprintf(_assertBuf, sizeof(_assertBuf), "[gfx_draw_frame] gfxUsed=%td / %d", gfxUsed, (int)ARRAY_COUNT(gDisplayContext->mainGfx));
        pc_trace_ml(_assertBuf);
    }
#endif
    ASSERT((s32)(((u32)(gMainGfxPos - gDisplayContext->mainGfx) << 3) >> 3) < ARRAY_COUNT(gDisplayContext->mainGfx));
    pc_trace_ml("[gfx_draw_frame] ASSERT passed");

    gDPFullSync(gMainGfxPos++);
    gSPEndDisplayList(gMainGfxPos++);
    pc_trace_ml("[gfx_draw_frame] gDPFullSync + gSPEndDisplayList done");

    {
        u32 dlSize = (u32)(gMainGfxPos - gDisplayContext->mainGfx) * 8;
        char _dlBuf[128];
        snprintf(_dlBuf, sizeof(_dlBuf), "[gfx_draw_frame] calling nuGfxTaskStart dlSize=%u", (unsigned)dlSize);
        pc_trace_ml(_dlBuf);
    }
    nuGfxTaskStart(gDisplayContext->mainGfx, (u32)(gMainGfxPos - gDisplayContext->mainGfx) * 8, NU_GFX_UCODE_F3DEX2,
                   NU_SC_TASK_LODABLE | NU_SC_SWAPBUFFER);
    pc_trace_ml("[gfx_draw_frame] nuGfxTaskStart returned");
    gCurrentDisplayContextIndex = gCurrentDisplayContextIndex ^ 1;
    crash_screen_set_draw_info(nuGfxCfb_ptr, SCREEN_WIDTH, SCREEN_HEIGHT);
    pc_trace_ml("[gfx_draw_frame] gfx_draw_frame COMPLETE");
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
