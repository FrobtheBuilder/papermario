#include "pc/pc_input.h"
#include <SDL2/SDL.h>
#include <string.h>
#include <stdio.h>

// N64 button constants
#define BUTTON_A              0x8000
#define BUTTON_B              0x4000
#define BUTTON_Z              0x2000
#define BUTTON_START          0x1000
#define BUTTON_DPAD_UP        0x0800
#define BUTTON_DPAD_DOWN      0x0400
#define BUTTON_DPAD_LEFT      0x0200
#define BUTTON_DPAD_RIGHT     0x0100
#define BUTTON_L              0x0020
#define BUTTON_R              0x0010
#define BUTTON_C_UP           0x0008
#define BUTTON_C_DOWN         0x0004
#define BUTTON_C_LEFT         0x0002
#define BUTTON_C_RIGHT        0x0001

static SDL_GameController* g_controller = NULL;

void pc_input_init(void) {
    // Initialize SDL game controller subsystem
    if (SDL_WasInit(SDL_INIT_GAMECONTROLLER) == 0) {
        SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
    }

    // Open first available controller
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (SDL_IsGameController(i)) {
            g_controller = SDL_GameControllerOpen(i);
            if (g_controller) {
                printf("Game controller connected: %s\n",
                       SDL_GameControllerName(g_controller));
                break;
            }
        }
    }

    if (!g_controller) {
        printf("No game controller found, using keyboard only\n");
    }
}

void pc_input_shutdown(void) {
    if (g_controller) {
        SDL_GameControllerClose(g_controller);
        g_controller = NULL;
    }
}

void pc_input_update(PCContPad* outPad) {
    if (!outPad) return;

    // Clear output
    outPad->button = 0;
    outPad->stick_x = 0;
    outPad->stick_y = 0;

    // Get keyboard state
    const u8* keys = (const u8*)SDL_GetKeyboardState(NULL);

    // Keyboard button mapping
    if (keys[SDL_SCANCODE_X])      outPad->button |= BUTTON_A;
    if (keys[SDL_SCANCODE_Z])      outPad->button |= BUTTON_B;
    if (keys[SDL_SCANCODE_C])      outPad->button |= BUTTON_Z;
    if (keys[SDL_SCANCODE_RETURN]) outPad->button |= BUTTON_START;
    if (keys[SDL_SCANCODE_UP])     outPad->button |= BUTTON_DPAD_UP;
    if (keys[SDL_SCANCODE_DOWN])   outPad->button |= BUTTON_DPAD_DOWN;
    if (keys[SDL_SCANCODE_LEFT])   outPad->button |= BUTTON_DPAD_LEFT;
    if (keys[SDL_SCANCODE_RIGHT])  outPad->button |= BUTTON_DPAD_RIGHT;
    if (keys[SDL_SCANCODE_LSHIFT]) outPad->button |= BUTTON_L;
    if (keys[SDL_SCANCODE_RSHIFT]) outPad->button |= BUTTON_R;

    // WASD for analog stick
    s8 stickX = 0, stickY = 0;
    if (keys[SDL_SCANCODE_A]) stickX -= 80;
    if (keys[SDL_SCANCODE_D]) stickX += 80;
    if (keys[SDL_SCANCODE_W]) stickY += 80;
    if (keys[SDL_SCANCODE_S]) stickY -= 80;

    // Game controller input
    if (g_controller) {
        // Buttons
        if (SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_A))
            outPad->button |= BUTTON_A;
        if (SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_B))
            outPad->button |= BUTTON_B;
        if (SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER))
            outPad->button |= BUTTON_Z;
        if (SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_START))
            outPad->button |= BUTTON_START;
        if (SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_DPAD_UP))
            outPad->button |= BUTTON_DPAD_UP;
        if (SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN))
            outPad->button |= BUTTON_DPAD_DOWN;
        if (SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT))
            outPad->button |= BUTTON_DPAD_LEFT;
        if (SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT))
            outPad->button |= BUTTON_DPAD_RIGHT;
        if (SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_LEFTSHOULDER))
            outPad->button |= BUTTON_L;

        // Analog stick
        s16 axisX = (s16)SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_LEFTX);
        s16 axisY = (s16)SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_LEFTY);

        // Scale from SDL range (-32768..32767) to N64 range (-80..80)
        s8 controllerX = (s8)((axisX * 80) / 32768);
        s8 controllerY = (s8)((-axisY * 80) / 32768); // Invert Y axis

        // Use controller stick if it's being moved, otherwise use keyboard
        if (controllerX != 0 || controllerY != 0) {
            stickX = controllerX;
            stickY = controllerY;
        }
    }

    outPad->stick_x = stickX;
    outPad->stick_y = stickY;
}

void pc_input_load_config(void) {
    // TODO: Load controller mapping from config file
}

void pc_input_save_config(void) {
    // TODO: Save controller mapping to config file
}
