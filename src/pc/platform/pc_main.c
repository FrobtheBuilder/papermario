#include "pc/platform.h"
#include "pc/pc_gfx.h"
#include "pc/pc_input.h"
#include "pc/pc_audio.h"
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

// Forward declarations from game code (src/main.c, src/main_loop.c)
// These are the real game functions - on PC, obfuscation is bypassed
extern void load_obfuscation_shims(void);
extern void create_audio_system(void);
extern void load_engine_data(void);
extern void gfxRetrace_Callback(s32 gfxTaskNum);

// PC helpers (src/pc/platform/n64_stubs.c) - these need common.h access
extern void pc_set_controller_connected(void);
extern u32 osGetCount(void);

// Game globals
extern u32 gRandSeed;

// Platform state
static struct {
    SDL_Window* window;
    GfxContext* gfxContext;
    b32 running;
    PlatformConfig config;
} g_platform;

void platform_init(PlatformConfig* config) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        exit(1);
    }

    // Store config
    g_platform.config = *config;

    // Set OpenGL attributes
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    // Create window
    u32 windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN;
    if (config->fullscreen) {
        windowFlags |= SDL_WINDOW_FULLSCREEN;
    }

    g_platform.window = SDL_CreateWindow(
        "Paper Mario",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        config->windowWidth,
        config->windowHeight,
        windowFlags
    );

    if (!g_platform.window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        exit(1);
    }

    // Create graphics context (pass window handle)
    g_platform.gfxContext = gfx_create_context(
        g_platform.window,
        config->windowWidth,
        config->windowHeight,
        config->vsync
    );

    if (!g_platform.gfxContext) {
        fprintf(stderr, "gfx_create_context failed\n");
        SDL_DestroyWindow(g_platform.window);
        SDL_Quit();
        exit(1);
    }

    // Initialize input
    pc_input_init();

    // Initialize audio
    pc_audio_init(44100);

    g_platform.running = TRUE;

    printf("Platform initialized: %dx%d, vsync=%d\n",
           config->windowWidth, config->windowHeight, config->vsync);
}

void platform_shutdown(void) {
    printf("Shutting down platform...\n");

    pc_audio_shutdown();
    pc_input_shutdown();

    if (g_platform.gfxContext) {
        gfx_destroy_context(g_platform.gfxContext);
    }

    if (g_platform.window) {
        SDL_DestroyWindow(g_platform.window);
    }

    SDL_Quit();
}

u64 platform_get_time_usec(void) {
    return SDL_GetPerformanceCounter() * 1000000ULL / SDL_GetPerformanceFrequency();
}

void platform_sleep_ms(u32 ms) {
    SDL_Delay(ms);
}

// Initialize the game engine (replicate boot_main from src/main.c without
// the N64-specific hardware init and the infinite loop at the end)
static void pc_init_game(void) {
    fprintf(stderr, "[PC] Initializing game engine...\n");

    // Set controller 1 as connected (replicate boot_main behavior)
    pc_set_controller_connected();

    // Bypass obfuscation (no-op on PC)
    load_obfuscation_shims();

    // Skip N64 audio system init - it accesses hardware registers and ROM data.
    // Audio playback on PC is handled by SDL_mixer (Phase 6).

    // Initialize all game subsystems (heap, scripts, NPCs, cameras, etc.)
    load_engine_data();

    // Seed RNG (matches boot_main behavior)
    gRandSeed += osGetCount();

    fprintf(stderr, "[PC] Game engine initialized, starting main loop.\n");
    fflush(stderr);
}

void platform_run_main_loop(void) {
    const u32 TARGET_FPS = 60;
    const u64 FRAME_TIME_US = 1000000 / TARGET_FPS;

    u64 lastFrameTime = platform_get_time_usec();
    u64 frameCount = 0;
    u64 fpsUpdateTime = lastFrameTime;

    fprintf(stderr, "[PC] Starting main loop (60 FPS target)...\n"); fflush(stderr);
    fprintf(stderr, "[PC] Press ESC to exit, F12 to take screenshot\n"); fflush(stderr);

    u64 totalFrames = 0;
    s32 screenshotRequested = FALSE;

    while (g_platform.running) {
        // Handle SDL events
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                g_platform.running = FALSE;
            } else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    g_platform.running = FALSE;
                } else if (event.key.keysym.sym == SDLK_F12) {
                    screenshotRequested = TRUE;
                }
            }
        }

        u64 currentTime = platform_get_time_usec();
        u64 elapsed = currentTime - lastFrameTime;

        if (elapsed >= FRAME_TIME_US) {
            // Clear the OpenGL framebuffer
            gfx_begin_frame(g_platform.gfxContext);

            // Run the game's retrace callback (same as N64 VBlank handler)
            gfxRetrace_Callback(0);

            totalFrames++;

            // Auto-screenshot burst to catch a rendered frame (remove later)
            if (totalFrames >= 900 && totalFrames <= 905) screenshotRequested = TRUE;

            // F12 screenshot: save as PPM (raw image)
            if (screenshotRequested) {
                screenshotRequested = FALSE;
                s32 w = g_platform.config.windowWidth;
                s32 h = g_platform.config.windowHeight;
                u8* pixels = (u8*)malloc(w * h * 3);
                if (pixels) {
                    // Read from front buffer (what's currently displayed)
                    glReadBuffer(GL_FRONT);
                    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, pixels);
                    glReadBuffer(GL_BACK);
                    char fname[64];
                    snprintf(fname, sizeof(fname), "screenshot_%llu.ppm", (unsigned long long)totalFrames);
                    FILE* f = fopen(fname, "wb");
                    if (f) {
                        fprintf(f, "P6\n%d %d\n255\n", w, h);
                        // Write rows bottom-to-top (OpenGL origin is bottom-left)
                        for (s32 row = h - 1; row >= 0; row--) {
                            fwrite(pixels + row * w * 3, 1, w * 3, f);
                        }
                        fclose(f);
                        fprintf(stderr, "[PC] Screenshot saved to %s\n", fname);
                    }
                    free(pixels);
                }
            }

            // Swap OpenGL buffers
            gfx_end_frame(g_platform.gfxContext);

            lastFrameTime = currentTime;
            frameCount++;

            // Print FPS every second
            if (currentTime - fpsUpdateTime >= 1000000) {
                fprintf(stderr, "FPS: %llu\n", (unsigned long long)frameCount);
                fflush(stderr);
                frameCount = 0;
                fpsUpdateTime = currentTime;
            }
        } else {
            // Small sleep to avoid burning CPU
            u64 remainingUs = FRAME_TIME_US - elapsed;
            if (remainingUs > 1000) {
                platform_sleep_ms((u32)((remainingUs - 1000) / 1000));
            }
        }
    }

    printf("Main loop exited.\n");
}

// PC entry point
int main(int argc, char* argv[]) {
    fprintf(stderr, "Paper Mario PC Port\n");
    fprintf(stderr, "===================\n\n");
    fflush(stderr);

    // Configure platform
    PlatformConfig config = {
        .windowWidth = 640,    // 2x original 320x240
        .windowHeight = 480,
        .fullscreen = FALSE,
        .vsync = TRUE
    };

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--fullscreen") == 0) {
            config.fullscreen = TRUE;
        } else if (strcmp(argv[i], "--no-vsync") == 0) {
            config.vsync = FALSE;
        } else if (strcmp(argv[i], "--width") == 0 && i + 1 < argc) {
            config.windowWidth = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--height") == 0 && i + 1 < argc) {
            config.windowHeight = atoi(argv[++i]);
        }
    }

    // Initialize platform (SDL, OpenGL, input, audio)
    platform_init(&config);

    // Initialize the game engine
    pc_init_game();

    // Run main loop (calls gfxRetrace_Callback at 60 FPS)
    platform_run_main_loop();

    // Cleanup
    platform_shutdown();

    return 0;
}
