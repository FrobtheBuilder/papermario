#include "functions.h"
#include "npc.h"
#include "game_modes.h"

void state_world_draw_aux_ui(void);

void state_init_world(void) {
#ifdef PC_BUILD
    TRACE_WORLD("state_init_world ENTER");
#endif
    set_game_mode_render_frontUI(0, state_world_draw_aux_ui);
#ifdef PC_BUILD
    TRACE_WORLD("state_init_world EXIT");
#endif
}

void state_step_world(void) {
#ifdef PC_BUILD
    TRACE_WORLD("state_step_world ENTER");
#endif
    update_encounters();
#ifdef PC_BUILD
    TRACE_WORLD("update_encounters done");
#endif
    update_npcs();
#ifdef PC_BUILD
    TRACE_WORLD("update_npcs done");
#endif
    update_player();
#ifdef PC_BUILD
    TRACE_WORLD("update_player done");
#endif
    update_item_entities();
#ifdef PC_BUILD
    TRACE_WORLD("update_item_entities done");
#endif
    update_effects();
#ifdef PC_BUILD
    TRACE_WORLD("update_effects done");
#endif
    iterate_models();
#ifdef PC_BUILD
    TRACE_WORLD("iterate_models done");
#endif
    update_cameras();
#ifdef PC_BUILD
    TRACE_WORLD("state_step_world EXIT");
#endif
}

void state_drawUI_world(void) {
#ifdef PC_BUILD
    TRACE_WORLD("state_drawUI_world ENTER");
    // Skip UI rendering on PC - needs font/sprite systems not yet implemented
    TRACE_WORLD("state_drawUI_world: UI rendering SKIPPED on PC");
    TRACE_WORLD("state_drawUI_world EXIT");
    return;
#endif
    draw_status_ui();
    draw_encounter_ui();
    imgfx_update_cache();
}

void state_world_draw_aux_ui(void) {
    draw_first_strike_ui();
}
