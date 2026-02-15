/**
 * effect_stubs.c - Stub implementations for all fx_* effect functions
 *
 * On the N64, each effect is a dynamically loaded overlay with its own code.
 * On PC, these overlays are not loaded, so we provide stub implementations
 * that allow the game to link and run without crashing.
 *
 * Functions returning EffectInstance* return NULL.
 * Functions with EffectInstance** output parameters set them to NULL.
 * All other functions are empty stubs.
 *
 * NOTE: Do NOT include effect_shims.h here, as it redefines engine function
 * names to shim_* variants via macros.
 */

#include "common.h"
#include "effects.h"

// ============================================================================
// void-returning effects with no output parameters
// ============================================================================

void fx_big_smoke_puff(f32 arg0, f32 arg1, f32 arg2) {
}

void fx_landing_dust(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4) {
}

void fx_walking_dust(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, f32 arg5) {
}

void fx_flower_splash(f32 arg0, f32 arg1, f32 arg2, f32 arg3) {
}

void fx_flower_trail(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, f32 arg5) {
}

void fx_cloud_puff(f32 arg0, f32 arg1, f32 arg2, f32 arg3) {
}

void fx_cloud_trail(s32 arg0, f32 arg1, f32 arg2, f32 arg3) {
}

void fx_footprint(f32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4) {
}

void fx_floating_flower(s32 arg0, f32 arg1, f32 arg2, f32 arg3, s32 arg4) {
}

void fx_snowflake(f32 arg0, f32 arg1, f32 arg2, s32 arg3) {
}

void fx_sparkles(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4) {
}

void fx_gather_energy_pink(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5) {
}

void fx_drop_leaves(s32 arg0, f32 arg1, f32 arg2, f32 arg3, s32 arg4) {
}

void fx_shattering_stones(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4) {
}

void fx_smoke_ring(s32 arg0, f32 arg1, f32 arg2, f32 arg3) {
}

void fx_damage_stars(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, f32 arg5, f32 arg6, s32 arg7) {
}

void fx_explosion(s32 arg0, f32 arg1, f32 arg2, f32 arg3) {
}

void fx_lens_flare(s32 arg0, f32 arg1, f32 arg2, f32 arg3, s32 arg4) {
}

void fx_spiky_white_aura(s32 arg0, f32 arg1, f32 arg2, f32 arg3, s32 arg4) {
}

void fx_smoke_impact(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5, f32 arg6, s32 arg7) {
}

void fx_stars_burst(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, f32 arg5, s32 arg6) {
}

void fx_stars_shimmer(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, f32 arg5, s32 arg6, s32 arg7) {
}

void fx_rising_bubble(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4) {
}

void fx_ring_blast(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5) {
}

void fx_shockwave(s32 arg0, f32 arg1, f32 arg2, f32 arg3) {
}

void fx_music_note(s32 arg0, f32 arg1, f32 arg2, f32 arg3) {
}

void fx_smoke_burst(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5) {
}

void fx_sweat(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, f32 arg5, s32 arg6) {
}

void fx_windy_leaves(s32 arg0, f32 arg1, f32 arg2, f32 arg3) {
}

void fx_falling_leaves(s32 arg0, f32 arg1, f32 arg2, f32 arg3) {
}

void fx_stars_spread(s32 arg0, f32 arg1, f32 arg2, f32 arg3, s32 arg4, s32 arg5) {
}

void fx_steam_burst(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5) {
}

void fx_big_snowflakes(s32 arg0, f32 arg1, f32 arg2, f32 arg3) {
}

void fx_energy_shockwave(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5) {
}

void fx_blast(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5) {
}

// ============================================================================
// void-returning effects with EffectInstance** output parameter
// ============================================================================

void fx_emote(s32 arg0, Npc* arg1, f32 arg2, f32 arg3, f32 arg4, f32 arg5, f32 arg6, s32 arg7, EffectInstance** outEffect) {
    if (outEffect != NULL) {
        *outEffect = NULL;
    }
}

void fx_got_item_outline(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, EffectInstance** outEffect) {
    if (outEffect != NULL) {
        *outEffect = NULL;
    }
}

void fx_damage_indicator(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, f32 arg5, s32 arg6, EffectInstance** outEffect) {
    if (outEffect != NULL) {
        *outEffect = NULL;
    }
}

void fx_flame(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, EffectInstance** outEffect) {
    if (outEffect != NULL) {
        *outEffect = NULL;
    }
}

void fx_sleep_bubble(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, f32 arg5, EffectInstance** outEffect) {
    if (outEffect != NULL) {
        *outEffect = NULL;
    }
}

void fx_stars_orbiting(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5, EffectInstance** outEffect) {
    if (outEffect != NULL) {
        *outEffect = NULL;
    }
}

void fx_ending_decals(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, EffectInstance** outEffect) {
    if (outEffect != NULL) {
        *outEffect = NULL;
    }
}

void fx_light_rays(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, EffectInstance** outEffect) {
    if (outEffect != NULL) {
        *outEffect = NULL;
    }
}

void fx_aura(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, EffectInstance** outEffect) {
    if (outEffect != NULL) {
        *outEffect = NULL;
    }
}

void fx_bulb_glow(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, EffectInstance** outEffect) {
    if (outEffect != NULL) {
        *outEffect = NULL;
    }
}

void fx_effect_3D(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, f32 arg5, f32 arg6, s32 arg7, EffectInstance** outEffect) {
    if (outEffect != NULL) {
        *outEffect = NULL;
    }
}

// ============================================================================
// EffectInstance*-returning effects
// ============================================================================

EffectInstance* fx_star(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, f32 arg5, f32 arg6, f32 arg7) {
    return NULL;
}

EffectInstance* fx_shape_spell(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, f32 arg5, f32 arg6, s32 arg7) {
    return NULL;
}

EffectInstance* fx_dust(s32 arg0, f32 arg1, f32 arg2, f32 arg3, s32 arg4) {
    return NULL;
}

EffectInstance* fx_purple_ring(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, f32 arg5, f32 arg6, f32 arg7) {
    return NULL;
}

EffectInstance* fx_debuff(s32 arg0, f32 arg1, f32 arg2, f32 arg3) {
    return NULL;
}

EffectInstance* fx_green_impact(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4) {
    return NULL;
}

EffectInstance* fx_radial_shimmer(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5) {
    return NULL;
}

EffectInstance* fx_lightning(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, f32 arg5) {
    return NULL;
}

EffectInstance* fx_fire_breath(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, f32 arg5, f32 arg6, s32 arg7, s32 arg8, s32 arg9) {
    return NULL;
}

EffectInstance* fx_shimmer_burst(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5) {
    return NULL;
}

EffectInstance* fx_shimmer_wave(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, f32 arg5, s32 arg6, s32 arg7) {
    return NULL;
}

EffectInstance* fx_fire_flower(s32 arg0, f32 arg1, f32 arg2, f32 arg3, s32 arg4) {
    return NULL;
}

EffectInstance* fx_recover(s32 arg0, f32 arg1, f32 arg2, f32 arg3, s32 arg4) {
    return NULL;
}

EffectInstance* fx_disable_x(s32 arg0, f32 arg1, f32 arg2, f32 arg3, s32 arg4) {
    return NULL;
}

EffectInstance* fx_bombette_breaking(s32 arg0, s32 arg1, s32 arg2, f32 arg3, s32 arg4, s32 arg5) {
    return NULL;
}

EffectInstance* fx_firework(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5) {
    return NULL;
}

EffectInstance* fx_confetti(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5) {
    return NULL;
}

EffectInstance* fx_snowfall(s32 arg0, s32 arg1) {
    return NULL;
}

EffectInstance* fx_effect_46(s32 arg0, PlayerStatus* arg1, f32 arg2, s32 arg3) {
    return NULL;
}

EffectInstance* fx_gather_magic(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5) {
    return NULL;
}

EffectInstance* fx_attack_result_text(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5) {
    return NULL;
}

EffectInstance* fx_small_gold_sparkle(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5) {
    return NULL;
}

EffectInstance* fx_flashing_box_shockwave(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, f32 arg5) {
    return NULL;
}

EffectInstance* fx_balloon(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5) {
    return NULL;
}

EffectInstance* fx_floating_rock(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, f32 arg5, f32 arg6, f32 arg7, s32 arg8) {
    return NULL;
}

EffectInstance* fx_chomp_drop(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5, f32 arg6, s32 arg7, f32 arg8, s32 arg9) {
    return NULL;
}

EffectInstance* fx_quizmo_stage(s32 arg0, f32 arg1, f32 arg2, f32 arg3) {
    return NULL;
}

EffectInstance* fx_radiating_energy_orb(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5) {
    return NULL;
}

EffectInstance* fx_quizmo_answer(s32 arg0, f32 arg1, f32 arg2, f32 arg3) {
    return NULL;
}

EffectInstance* fx_motion_blur_flame(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5) {
    return NULL;
}

EffectInstance* fx_energy_orb_wave(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5) {
    return NULL;
}

EffectInstance* fx_merlin_house_stars(s32 arg0, f32 arg1, f32 arg2, f32 arg3) {
    return NULL;
}

EffectInstance* fx_quizmo_audience(s32 arg0, f32 arg1, f32 arg2, f32 arg3) {
    return NULL;
}

EffectInstance* fx_butterflies(s32 arg0, f32 arg1, f32 arg2, f32 arg3) {
    return NULL;
}

EffectInstance* fx_stat_change(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5) {
    return NULL;
}

EffectInstance* fx_snaking_static(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5) {
    return NULL;
}

EffectInstance* fx_thunderbolt_ring(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5) {
    return NULL;
}

EffectInstance* fx_squirt(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, f32 arg5, f32 arg6, f32 arg7, s32 arg8) {
    return NULL;
}

EffectInstance* fx_water_block(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5) {
    return NULL;
}

EffectInstance* fx_waterfall(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5) {
    return NULL;
}

EffectInstance* fx_water_fountain(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5) {
    return NULL;
}

EffectInstance* fx_underwater(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5) {
    return NULL;
}

EffectInstance* fx_lightning_bolt(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, f32 arg5, f32 arg6, f32 arg7, s32 arg8) {
    return NULL;
}

EffectInstance* fx_water_splash(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5) {
    return NULL;
}

EffectInstance* fx_snowman_doll(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5) {
    return NULL;
}

EffectInstance* fx_fright_jar(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5) {
    return NULL;
}

EffectInstance* fx_stop_watch(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5) {
    return NULL;
}

EffectInstance* fx_effect_63(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, f32 arg5, f32 arg6, f32 arg7, s32 arg8, s32 arg9) {
    return NULL;
}

EffectInstance* fx_throw_spiny(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, f32 arg5, f32 arg6, f32 arg7, s32 arg8) {
    return NULL;
}

EffectInstance* fx_effect_65(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5) {
    return NULL;
}

EffectInstance* fx_tubba_heart_attack(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5) {
    return NULL;
}

EffectInstance* fx_whirlwind(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5) {
    return NULL;
}

EffectInstance* fx_red_impact(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5) {
    return NULL;
}

EffectInstance* fx_floating_cloud_puff(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5) {
    return NULL;
}

EffectInstance* fx_energy_in_out(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5) {
    return NULL;
}

EffectInstance* fx_tattle_window(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5) {
    return NULL;
}

EffectInstance* fx_shiny_flare(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5) {
    return NULL;
}

EffectInstance* fx_huff_puff_breath(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, f32 arg5, f32 arg6, s32 arg7) {
    return NULL;
}

EffectInstance* fx_cold_breath(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5) {
    return NULL;
}

EffectInstance* fx_embers(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, f32 arg5, f32 arg6, s32 arg7, s32 arg8, f32 arg9, f32 argA) {
    return NULL;
}

EffectInstance* fx_hieroglyphs(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5) {
    return NULL;
}

EffectInstance* fx_misc_particles(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, f32 arg5, f32 arg6, s32 arg7, s32 arg8) {
    return NULL;
}

EffectInstance* fx_static_status(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5, s32 arg6) {
    return NULL;
}

EffectInstance* fx_moving_cloud(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, f32 arg5, f32 arg6, f32 arg7, f32 arg8) {
    return NULL;
}

EffectInstance* fx_effect_75(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5) {
    return NULL;
}

EffectInstance* fx_firework_rocket(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, f32 arg5, f32 arg6, f32 arg7, s32 arg8) {
    return NULL;
}

EffectInstance* fx_peach_star_beam(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5) {
    return NULL;
}

EffectInstance* fx_chapter_change(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5) {
    return NULL;
}

EffectInstance* fx_ice_shard(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5) {
    return NULL;
}

EffectInstance* fx_spirit_card(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5) {
    return NULL;
}

EffectInstance* fx_lil_oink(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5) {
    return NULL;
}

EffectInstance* fx_something_rotating(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5) {
    return NULL;
}

EffectInstance* fx_breaking_junk(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5) {
    return NULL;
}

EffectInstance* fx_partner_buff(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5) {
    return NULL;
}

EffectInstance* fx_quizmo_assistant(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5) {
    return NULL;
}

EffectInstance* fx_ice_pillar(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5) {
    return NULL;
}

EffectInstance* fx_sun(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5) {
    return NULL;
}

EffectInstance* fx_star_spirits_energy(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5) {
    return NULL;
}

EffectInstance* fx_pink_sparkles(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, f32 arg5) {
    return NULL;
}

EffectInstance* fx_star_outline(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5) {
    return NULL;
}

EffectInstance* fx_effect_86(s32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5) {
    return NULL;
}
