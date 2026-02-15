#include "pc/pc_gfx.h"
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <stdio.h>

struct GfxContext {
    SDL_GLContext glContext;
    SDL_Window* window;
    s32 width;
    s32 height;
    b32 vsync;
};

static GfxContext* g_ctx = NULL;

GfxContext* gfx_create_context(void* windowHandle, s32 width, s32 height, b32 vsync) {
    GfxContext* ctx = (GfxContext*)malloc(sizeof(GfxContext));
    if (!ctx) {
        fprintf(stderr, "Failed to allocate GfxContext\n");
        return NULL;
    }

    ctx->width = width;
    ctx->height = height;
    ctx->vsync = vsync;
    ctx->window = (SDL_Window*)windowHandle;

    if (!ctx->window) {
        fprintf(stderr, "Invalid window handle\n");
        free(ctx);
        return NULL;
    }

    // Create OpenGL context
    ctx->glContext = SDL_GL_CreateContext(ctx->window);
    if (!ctx->glContext) {
        fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        free(ctx);
        return NULL;
    }

    // Make context current
    SDL_GL_MakeCurrent(ctx->window, ctx->glContext);

    // Initialize GLEW
    glewExperimental = GL_TRUE;
    GLenum glewError = glewInit();
    if (glewError != GLEW_OK) {
        fprintf(stderr, "glewInit failed: %s\n", glewGetErrorString(glewError));
        SDL_GL_DeleteContext(ctx->glContext);
        free(ctx);
        return NULL;
    }

    // Set VSync
    if (SDL_GL_SetSwapInterval(vsync ? 1 : 0) < 0) {
        fprintf(stderr, "Warning: SDL_GL_SetSwapInterval failed: %s\n", SDL_GetError());
    }

    // Print OpenGL info
    printf("OpenGL Version: %s\n", glGetString(GL_VERSION));
    printf("GLSL Version: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
    printf("Renderer: %s\n", glGetString(GL_RENDERER));
    printf("Vendor: %s\n", glGetString(GL_VENDOR));

    // Set up initial OpenGL state
    glViewport(0, 0, width, height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    g_ctx = ctx;

    return ctx;
}

void gfx_destroy_context(GfxContext* ctx) {
    if (!ctx) return;

    if (ctx->glContext) {
        SDL_GL_DeleteContext(ctx->glContext);
    }

    free(ctx);

    if (g_ctx == ctx) {
        g_ctx = NULL;
    }
}

void gfx_begin_frame(GfxContext* ctx) {
    if (!ctx) return;

    // Ensure depth mask is enabled so glClear can write to depth buffer
    glDepthMask(GL_TRUE);
    glDisable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void gfx_end_frame(GfxContext* ctx) {
    if (!ctx) return;

    // Swap buffers
    SDL_GL_SwapWindow(ctx->window);
}

void* gfx_get_window_handle(GfxContext* ctx) {
    return ctx ? ctx->window : NULL;
}
