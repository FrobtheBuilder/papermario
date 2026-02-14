#include "common.h"
#include "hud_element.h"
#include "sprite.h"
#include "game_modes.h"

#ifdef PC_BUILD
#include <stdio.h>
static void pc_trace_intro(const char* msg) {
    FILE* f = fopen("pc_boot_trace.log", "a");
    if (f) { fprintf(f, "[load_engine] [state_intro] %s\n", msg); fclose(f); }
}
#define TRACE_INTRO(msg) pc_trace_intro(msg)
#else
#define TRACE_INTRO(msg) ((void)0)
#endif

enum IntroStates {
    INTRO_INIT                  = 0x00000000,
    INTRO_DISABLE_DRAW_FRAME    = 0x00000001,
    INTRO_LOAD_MAP              = 0x00000002,
    INTRO_AWAIT_MAIN            = 0x00000003,
    INTRO_FADE_IN               = 0x00000004,
    INTRO_ENABLE_DRAW_FRAME      = 0x00000015, // unused
};

BSS s32 IntroEnableDrawFrameDelay;
BSS s16 IntroOverlayAlpha;
BSS s16 IntroFrontFadeAlpha;
BSS s16 IntroOverlayDelta;
BSS s16 IntroFadeColorR;
BSS s16 IntroFadeColorG;
BSS s16 IntroFadeColorB;
BSS s32 IntroOverlayType;
BSS s32 D_800A0964; // related to skipping the intro

void state_init_intro(void) {
    s8 viewportMode;

    TRACE_INTRO("state_init_intro ENTER");
    gGameStatusPtr->startupState = INTRO_INIT;

    set_curtain_scale_goal(1.0f);
    set_curtain_fade_goal(0.3f);

    viewportMode = gGameStatusPtr->introPart;
    switch (viewportMode) {
        case 0:
            startup_set_fade_screen_alpha(0);

            IntroOverlayAlpha = 255;
            IntroFrontFadeAlpha = 16;
            IntroOverlayDelta = 4;
            IntroOverlayType = OVERLAY_SCREEN_COLOR;
            IntroFadeColorR = 208;
            IntroFadeColorG = 208;
            IntroFadeColorB = 208;
            D_800A0964 = 0;

            // hos_05 (Star Sanctuary)
            gGameStatusPtr->areaID = AREA_HOS;
            gGameStatusPtr->mapID = 5; //TODO hard-coded map ID
            gGameStatusPtr->entryID = 3;
            break;
        case 1:
            startup_set_fade_screen_alpha(0);

            IntroOverlayAlpha = 0;
#if VERSION_PAL
            IntroFrontFadeAlpha = 14;
#else
            IntroFrontFadeAlpha = 12;
#endif
            IntroOverlayDelta = 4;
            IntroOverlayType = OVERLAY_VIEWPORT_COLOR;
            IntroFadeColorR = 0;
            IntroFadeColorG = 0;
            IntroFadeColorB = 0;
            D_800A0964 = 0;

            // hos_04 (Outside the Sanctuary)
            gGameStatusPtr->areaID = AREA_HOS;
            gGameStatusPtr->mapID = 4; //TODO hard-coded map ID
            gGameStatusPtr->entryID = 4;
            break;
        default:
            startup_set_fade_screen_alpha(0);
            startup_set_fade_screen_color(208);

            gGameStatusPtr->introPart = INTRO_PART_NONE;

            IntroFrontFadeAlpha = 6;
            IntroOverlayDelta = 6;

            IntroMessageIdx++;
            if (IntroMessageIdx >= 4) {
                // both hos_04 and hos_05 have an IntroMessage array of length 4
                IntroMessageIdx = 0;
            }

            D_800A0964 = 3;
            break;
    }

    set_screen_overlay_params_back(IntroOverlayType, IntroOverlayAlpha);
    set_screen_overlay_color(SCREEN_LAYER_BACK, IntroFadeColorR, IntroFadeColorG, IntroFadeColorB);

    startup_fade_screen_update();
    TRACE_INTRO("state_init_intro DONE");
}

void state_step_intro(void) {
    PlayerData* playerData = &gPlayerData;
    u32 pressedButtons = gGameStatusPtr->pressedButtons[0];
    s32 i;

    if (gGameStatusPtr->introPart != INTRO_PART_NONE) {
        if (D_800A0964 == 0 && pressedButtons & (BUTTON_A | BUTTON_B | BUTTON_START | BUTTON_Z)) {
            D_800A0964 = 1;
        }

        if (D_800A0964 == 1 && (gGameStatusPtr->startupState == INTRO_INIT ||
                                gGameStatusPtr->startupState == INTRO_DISABLE_DRAW_FRAME ||
                                gGameStatusPtr->startupState == INTRO_FADE_IN))
        {
            gGameStatusPtr->introPart = INTRO_PART_100;
            state_init_intro();
            return;
        }

        if (D_800A0964 == 2 && (gGameStatusPtr->startupState == INTRO_INIT ||
                                gGameStatusPtr->startupState == INTRO_DISABLE_DRAW_FRAME ||
                                gGameStatusPtr->startupState == INTRO_FADE_IN))
        {
            gGameStatusPtr->introPart++;
            state_init_intro();
            return;
        }
    }

    {
        char buf[128];
        snprintf(buf, sizeof(buf), "state_step_intro startupState=%d introPart=%d",
                 (int)gGameStatusPtr->startupState, (int)gGameStatusPtr->introPart);
        TRACE_INTRO(buf);
    }

    switch (gGameStatusPtr->startupState) {
        case INTRO_INIT:
            TRACE_INTRO("INTRO_INIT: update_effects");
            update_effects();
            TRACE_INTRO("INTRO_INIT: update_cameras");
            update_cameras();
            if (gGameStatusPtr->introPart == INTRO_PART_NONE) {
                set_curtain_fade_goal(0.0f);
                if (startup_fade_screen_out(IntroFrontFadeAlpha)) {
                    gGameStatusPtr->startupState = INTRO_DISABLE_DRAW_FRAME;
                    set_curtain_draw_callback(nullptr);
                }
            } else {
                IntroOverlayAlpha += IntroFrontFadeAlpha;
                if (IntroOverlayAlpha >= 255) {
                    IntroOverlayAlpha = 255;
                    gGameStatusPtr->startupState = INTRO_DISABLE_DRAW_FRAME;
                    set_curtain_draw_callback(nullptr);
                }
            }
            break;
        case INTRO_DISABLE_DRAW_FRAME:
            TRACE_INTRO("INTRO_DISABLE_DRAW_FRAME");
            IntroEnableDrawFrameDelay = 4;
            gOverrideFlags |= GLOBAL_OVERRIDES_DISABLE_DRAW_FRAME;
            // this condition is always true, likely leftover from an earlier version
            if (IntroOverlayType != OVERLAY_INTRO_1) {
                gGameStatusPtr->startupState = INTRO_LOAD_MAP;
            }
            break;
        case INTRO_ENABLE_DRAW_FRAME: // unused
            IntroEnableDrawFrameDelay--;
            if (IntroEnableDrawFrameDelay <= 0) {
                gOverrideFlags &= ~GLOBAL_OVERRIDES_DISABLE_DRAW_FRAME;
                gGameStatusPtr->startupState = INTRO_LOAD_MAP;
            }
            break;
        case INTRO_LOAD_MAP:
            TRACE_INTRO("INTRO_LOAD_MAP ENTER");
            set_curtain_draw_callback(nullptr);
            gGameStatusPtr->context = CONTEXT_WORLD;
            gGameStatusPtr->debugUnused1 = false;
            gGameStatusPtr->debugScripts = DEBUG_SCRIPTS_NONE;
            gGameStatusPtr->keepUsingPartnerOnMapChange = false;

            if (gGameStatusPtr->introPart == INTRO_PART_NONE) {
                TRACE_INTRO("INTRO_LOAD_MAP: introPart=NONE, going to title screen");
                general_heap_create();
                TRACE_INTRO("  general_heap_create done");
                clear_render_tasks();
                clear_worker_list();
                clear_script_list();
                create_cameras();
                TRACE_INTRO("  create_cameras done");
                spr_init_sprites(PLAYER_SPRITES_MARIO_WORLD);
                TRACE_INTRO("  spr_init_sprites done");
                clear_entity_models();
                clear_animator_list();
                clear_model_data();
                clear_sprite_shading_data();
                reset_background_settings();
                hud_element_clear_cache();
                clear_trigger_data();
                clear_printers();
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
                TRACE_INTRO("  all clears done, setting GAME_MODE_TITLE_SCREEN");
                set_game_mode(GAME_MODE_TITLE_SCREEN);
                return;
            }

            playerData->curHP = 10;
            playerData->curMaxHP = 10;
            playerData->hardMaxHP = 10;
            playerData->curFP = 5;
            playerData->curMaxFP = 5;
            playerData->hardMaxFP = 5;
            playerData->maxBP = 2;
            playerData->bootsLevel = GEAR_RANK_NORMAL;
            playerData->hammerLevel = GEAR_RANK_NONE;
            playerData->fortressKeyCount = 0;
            playerData->level = 0;

            for (i = 0; i < ARRAY_COUNT(playerData->partners); i++) {
                playerData->partners[i].enabled = false;
            }

            playerData->curPartner = PARTNER_NONE;
            TRACE_INTRO("INTRO_LOAD_MAP: calling load_map_by_IDs");
            load_map_by_IDs(gGameStatusPtr->areaID, gGameStatusPtr->mapID, LOAD_FROM_MAP);
            TRACE_INTRO("INTRO_LOAD_MAP: load_map_by_IDs returned");
            gGameStatusPtr->startupState = INTRO_AWAIT_MAIN;
            disable_player_input();
            break;
        case INTRO_AWAIT_MAIN:
            TRACE_INTRO("INTRO_AWAIT_MAIN: setting overlay");
            if (IntroOverlayType == OVERLAY_INTRO_1) {
                IntroOverlayType = OVERLAY_INTRO_2;
            }
            IntroOverlayAlpha = 255 - IntroOverlayDelta;
            gOverrideFlags &= ~GLOBAL_OVERRIDES_DISABLE_DRAW_FRAME;
            TRACE_INTRO("INTRO_AWAIT_MAIN: clearing camera disabled flag");
            gCameras[CAM_DEFAULT].flags &= ~CAMERA_FLAG_DISABLED;
            gOverrideFlags &= ~GLOBAL_OVERRIDES_DISABLE_RENDER_WORLD;
            TRACE_INTRO("INTRO_AWAIT_MAIN: update_player");
            update_player();
            TRACE_INTRO("INTRO_AWAIT_MAIN: update_encounters");
            update_encounters();
            TRACE_INTRO("INTRO_AWAIT_MAIN: update_npcs");
            update_npcs();
            TRACE_INTRO("INTRO_AWAIT_MAIN: update_effects");
            update_effects();
            TRACE_INTRO("INTRO_AWAIT_MAIN: update_cameras");
            update_cameras();
            TRACE_INTRO("INTRO_AWAIT_MAIN: does_script_exist");
            if (!does_script_exist(gGameStatusPtr->mainScriptID)) {
                gGameStatusPtr->prevArea = gGameStatusPtr->areaID;
                gGameStatusPtr->startupState = INTRO_FADE_IN;
                break;
            }
            return;
        case INTRO_FADE_IN:
            update_effects();
            update_cameras();
            update_npcs();
            if (IntroOverlayAlpha == 0) {
                set_screen_overlay_params_front(OVERLAY_NONE, -1.0f);
                set_screen_overlay_params_back(OVERLAY_NONE, -1.0f);
            } else {
                IntroOverlayAlpha -= IntroOverlayDelta;
                if (IntroOverlayAlpha < 0) {
                    IntroOverlayAlpha = 0;
                }
            }
            break;
    }

    set_screen_overlay_params_back(IntroOverlayType, IntroOverlayAlpha);
    set_screen_overlay_color(SCREEN_LAYER_BACK, IntroFadeColorR, IntroFadeColorG, IntroFadeColorB);
    startup_fade_screen_update();
}

void state_drawUI_intro(void) {
}
