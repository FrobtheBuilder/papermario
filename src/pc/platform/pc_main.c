#include "pc/platform.h"
#include "pc/pc_gfx.h"
#include "pc/pc_input.h"
#include "pc/pc_audio.h"
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <direct.h>   // _mkdir on Windows
#include <windows.h>  // FindFirstFileA, DeleteFileA

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../stb_image_write.h"

// Forward declarations from game code (src/main.c, src/main_loop.c)
// These are the real game functions - on PC, obfuscation is bypassed
extern void load_obfuscation_shims(void);
extern void create_audio_system(void);
extern void load_engine_data(void);
extern void gfxRetrace_Callback(s32 gfxTaskNum);

// PC helpers (src/pc/platform/n64_stubs.c) - these need common.h access
extern void pc_set_controller_connected(void);
extern u32 osGetCount(void);

// ROM reader (src/pc/platform/rom_reader.c)
extern s32 pc_rom_init(void);
extern void pc_rom_shutdown(void);

// Game globals
extern u32 gRandSeed;

// Platform state
static struct {
    SDL_Window* window;
    GfxContext* gfxContext;
    b32 running;
    PlatformConfig config;
} g_platform;

// Automated screenshot system
// Usage: --screenshot-frames 100,500,900 [--screenshot-dir path]
// Saves BMP files to $TEMP/pm_screenshots/ (or custom dir), then auto-quits.
#define MAX_SCREENSHOT_FRAMES 64
static struct {
    u32 frames[MAX_SCREENSHOT_FRAMES];
    u32 count;
    u32 nextIdx;
    char dir[512];
} g_screenshots;

static void screenshot_ensure_dir(void) {
    if (g_screenshots.dir[0] == '\0') {
        const char* tmp = getenv("TEMP");
        if (!tmp) tmp = getenv("TMP");
        if (!tmp) tmp = "/tmp";
        snprintf(g_screenshots.dir, sizeof(g_screenshots.dir),
                 "%s/pm_screenshots", tmp);
    }
    _mkdir(g_screenshots.dir); // ignore error if exists
}

// Delete old frame_*.png from screenshot dir so stale results can't mislead.
static void screenshot_clean_dir(void) {
    char pattern[768];
    snprintf(pattern, sizeof(pattern), "%s/frame_*.png", g_screenshots.dir);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        char path[768];
        snprintf(path, sizeof(path), "%s/%s", g_screenshots.dir, fd.cFileName);
        DeleteFileA(path);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

static void screenshot_save(u32 frameNum) {
    screenshot_ensure_dir();
    s32 w = g_platform.config.windowWidth;
    s32 h = g_platform.config.windowHeight;
    s32 stride = w * 3;
    u8* pixels = (u8*)malloc(stride * h);
    if (!pixels) return;

    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, pixels);

    // Flip vertically (OpenGL origin is bottom-left)
    u8* row = (u8*)malloc(stride);
    if (row) {
        for (s32 y = 0; y < h / 2; y++) {
            u8* top = pixels + y * stride;
            u8* bot = pixels + (h - 1 - y) * stride;
            memcpy(row, top, stride);
            memcpy(top, bot, stride);
            memcpy(bot, row, stride);
        }
        free(row);
    }

    char path[768];
    snprintf(path, sizeof(path), "%s/frame_%04u.png", g_screenshots.dir, frameNum);
    stbi_write_png(path, w, h, 3, pixels, stride);
    free(pixels);
    fprintf(stderr, "[SCREENSHOT] %s\n", path);
}

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

    // Flag set by rdp_process_display_list when a DL is rendered.
    // The game only renders every other retrace callback (30 FPS render on
    // 60 FPS ticks). On non-render ticks, skip the buffer swap so the
    // previous frame stays on screen instead of flashing black.
    extern int g_rdp_frame_rendered;

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
            // Clear back buffer (invisible until swapped)
            gfx_begin_frame(g_platform.gfxContext);

            // Run the game's retrace callback (same as N64 VBlank handler).
            // On render ticks it submits DLs and sets g_rdp_frame_rendered.
            g_rdp_frame_rendered = 0;
            gfxRetrace_Callback(0);

            totalFrames++;

            if (g_rdp_frame_rendered) {
                // Auto-screenshot at specified frames
                if (g_screenshots.nextIdx < g_screenshots.count &&
                    totalFrames >= g_screenshots.frames[g_screenshots.nextIdx]) {
                    screenshot_save((u32)totalFrames);
                    g_screenshots.nextIdx++;
                    if (g_screenshots.nextIdx >= g_screenshots.count) {
                        g_platform.running = FALSE; // quit after last screenshot
                    }
                }

                // F12 manual screenshot
                if (screenshotRequested) {
                    screenshotRequested = FALSE;
                    screenshot_save((u32)totalFrames);
                }

                // Only swap when we rendered something — keeps the previous
                // frame visible on non-render ticks instead of flashing black.
                gfx_end_frame(g_platform.gfxContext);
            }

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
        } else if (strcmp(argv[i], "--screenshot-frames") == 0 && i + 1 < argc) {
            // Parse comma-separated frame numbers: --screenshot-frames 100,500,900
            char buf[1024];
            strncpy(buf, argv[++i], sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';
            char* token = strtok(buf, ",");
            while (token && g_screenshots.count < MAX_SCREENSHOT_FRAMES) {
                g_screenshots.frames[g_screenshots.count++] = (u32)atoi(token);
                token = strtok(NULL, ",");
            }
            // Sort ascending (simple insertion sort)
            for (u32 a = 1; a < g_screenshots.count; a++) {
                u32 key = g_screenshots.frames[a];
                u32 b = a;
                while (b > 0 && g_screenshots.frames[b - 1] > key) {
                    g_screenshots.frames[b] = g_screenshots.frames[b - 1];
                    b--;
                }
                g_screenshots.frames[b] = key;
            }
        } else if (strcmp(argv[i], "--screenshot-dir") == 0 && i + 1 < argc) {
            strncpy(g_screenshots.dir, argv[++i], sizeof(g_screenshots.dir) - 1);
            g_screenshots.dir[sizeof(g_screenshots.dir) - 1] = '\0';
        }
    }

    // Log screenshot plan
    if (g_screenshots.count > 0) {
        screenshot_ensure_dir();
        fprintf(stderr, "[PC] Screenshot dir: %s\n", g_screenshots.dir);
        fprintf(stderr, "[PC] Will screenshot at frames:");
        for (u32 i = 0; i < g_screenshots.count; i++) {
            fprintf(stderr, " %u", g_screenshots.frames[i]);
        }
        fprintf(stderr, " (auto-quit after last)\n");
        screenshot_clean_dir();
        fflush(stderr);
    }

    // Load ROM image (must happen before game init so DMA loads work)
    pc_rom_init();

    // Initialize platform (SDL, OpenGL, input, audio)
    platform_init(&config);

    // Initialize the game engine
    pc_init_game();

    // Run main loop (calls gfxRetrace_Callback at 60 FPS)
    platform_run_main_loop();

    // Cleanup
    platform_shutdown();
    pc_rom_shutdown();

    return 0;
}
