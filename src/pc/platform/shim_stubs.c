/**
 * shim_stubs.c - Implementations of shim_* functions for the PC port
 *
 * On the N64, effect overlays use "shim" functions as trampolines to call
 * engine functions. The shim table is set up at runtime so overlays can
 * reference engine code without direct linking.
 *
 * On PC, we link everything statically, so each shim_X() simply calls the
 * real X() function directly.
 *
 * NOTE: Do NOT include effect_shims.h here. That header defines macros like
 * #define guRotateF shim_guRotateF which would cause infinite recursion.
 */

#include "common.h"
#include "effects.h"
#include "functions.h"
#include "PR/gu.h"

// Forward declarations for functions not covered by the above headers
void sfx_play_sound_at_position(s32 soundID, s32 flags, f32 posX, f32 posY, f32 posZ);

// ============================================================================
// libultra GU math shims
// ============================================================================

void shim_guRotateF(float mf[4][4], float a, float x, float y, float z) {
    guRotateF(mf, a, x, y, z);
}

void shim_guTranslateF(float mf[4][4], float x, float y, float z) {
    guTranslateF(mf, x, y, z);
}

void shim_guTranslate(Mtx *m, float x, float y, float z) {
    guTranslate(m, x, y, z);
}

void shim_guScaleF(float mf[4][4], float x, float y, float z) {
    guScaleF(mf, x, y, z);
}

void shim_guMtxCatF(float m[4][4], float n[4][4], float r[4][4]) {
    guMtxCatF(m, n, r);
}

void shim_guMtxF2L(float mf[4][4], Mtx *m) {
    guMtxF2L(mf, m);
}

void shim_guPerspectiveF(f32 mf[4][4], u16* perspNorm, f32 fovy, f32 aspect, f32 near, f32 far, f32 scale) {
    guPerspectiveF(mf, perspNorm, fovy, aspect, near, far, scale);
}

void shim_guPositionF(float mf[4][4], float r, float p, float h, float s, float x, float y, float z) {
    guPositionF(mf, r, p, h, s, x, y, z);
}

void shim_guOrthoF(float mf[4][4], float l, float r, float b, float t, float n, float f, float scale) {
    guOrthoF(mf, l, r, b, t, n, f, scale);
}

void shim_guFrustumF(float mf[4][4], float l, float r, float b, float t, float n, float f, float scale) {
    guFrustumF(mf, l, r, b, t, n, f, scale);
}

// ============================================================================
// Effect system shims
// ============================================================================

RenderTask* shim_queue_render_task(RenderTask* task) {
    return queue_render_task(task);
}

EffectInstance* shim_create_effect_instance(EffectBlueprint* effectBp) {
    return create_effect_instance(effectBp);
}

void shim_remove_effect(EffectInstance* effect) {
    remove_effect(effect);
}

s32 shim_load_effect(s32 effectIndex) {
    return load_effect(effectIndex);
}

// ============================================================================
// Memory/utility shims
// ============================================================================

void* shim_general_heap_malloc(s32 size) {
    return general_heap_malloc(size);
}

void shim_mem_clear(void* data, s32 numBytes) {
    mem_clear(data, numBytes);
}

s32 shim_rand_int(s32 max) {
    return rand_int(max);
}

// ============================================================================
// Math shims
// ============================================================================

f32 shim_clamp_angle(f32 theta) {
    return clamp_angle(theta);
}

f32 shim_sin_deg(f32 x) {
    return sin_deg(x);
}

f32 shim_cos_deg(f32 x) {
    return cos_deg(x);
}

f32 shim_atan2(f32 startX, f32 startZ, f32 endX, f32 endZ) {
    return atan2(startX, startZ, endX, endZ);
}

float shim_sqrtf(float value) {
    return sqrtf(value);
}

// ============================================================================
// Collision/geometry shims
// ============================================================================

bool shim_npc_raycast_down_sides(s32 ignoreFlags, f32* posX, f32* posY, f32* posZ, f32* hitDepth) {
    return npc_raycast_down_sides(ignoreFlags, posX, posY, posZ, hitDepth);
}

void shim_transform_point(Matrix4f mtx, f32 inX, f32 inY, f32 inZ, f32 inS, f32* outX, f32* outY, f32* outZ, f32* outW) {
    transform_point(mtx, inX, inY, inZ, inS, outX, outY, outZ, outW);
}

bool shim_is_point_visible(f32 x, f32 y, f32 z, s32 depthQueryID, f32* screenX, f32* screenY) {
    return is_point_visible(x, y, z, depthQueryID, screenX, screenY);
}

// ============================================================================
// Rendering/UI shims
// ============================================================================

void shim_mdl_draw_hidden_panel_surface(Gfx** arg0, u16 treeIndex) {
    mdl_draw_hidden_panel_surface(arg0, treeIndex);
}

void shim_draw_prev_frame_buffer_at_screen_pos(s32 arg0, s32 arg1, s32 arg2, s32 arg3, f32 arg4) {
    draw_prev_frame_buffer_at_screen_pos(arg0, arg1, arg2, arg3, arg4);
}

void shim_draw_box(
    s32 flags, WindowStyle windowStyle, s32 posX, s32 posY, s32 posZ, s32 width, s32 height, u8 opacity,
    u8 darkening, f32 scaleX, f32 scaleY, f32 rotX, f32 rotY, f32 rotZ, void (*fpDrawContents)(void*),
    void* drawContentsArg0, Matrix4f rotScaleMtx, s32 translateX, s32 translateY, f32 (*outMtx)[4]
) {
    draw_box(flags, windowStyle, posX, posY, posZ, width, height, opacity,
             darkening, scaleX, scaleY, rotX, rotY, rotZ,
             (void (*)(s32, s32, s32, s32, s32, s32, s32))fpDrawContents,
             drawContentsArg0, rotScaleMtx, translateX, translateY, outMtx);
}

void shim_draw_msg(s32 msgID, s32 posX, s32 posY, s32 opacity, s32 palette, s32 style) {
    draw_msg(msgID, posX, posY, opacity, palette, (u8)style);
}

s32 shim_get_msg_width(s32 msgID, u16 charset) {
    return get_msg_width(msgID, charset);
}

void shim_mdl_get_shroud_tint_params(u8* r, u8* g, u8* b, u8* a) {
    mdl_get_shroud_tint_params(r, g, b, a);
}

// ============================================================================
// Audio shims
// ============================================================================

void shim_sfx_play_sound_at_position(s32 soundID, s32 value2, f32 posX, f32 posY, f32 posZ) {
    sfx_play_sound_at_position(soundID, value2, posX, posY, posZ);
}
