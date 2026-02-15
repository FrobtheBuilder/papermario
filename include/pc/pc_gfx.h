#ifndef PC_GFX_H
#define PC_GFX_H

#include "pc/platform.h"

#ifdef __cplusplus
extern "C" {
#endif

// Graphics context (opaque)
typedef struct GfxContext GfxContext;

// Graphics initialization
GfxContext* gfx_create_context(void* windowHandle, s32 width, s32 height, b32 vsync);
void gfx_destroy_context(GfxContext* ctx);

// Frame rendering
void gfx_begin_frame(GfxContext* ctx);
void gfx_end_frame(GfxContext* ctx);

// Window management
void* gfx_get_window_handle(GfxContext* ctx);

#ifdef __cplusplus
}
#endif

#endif // PC_GFX_H
