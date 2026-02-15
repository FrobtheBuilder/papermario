#ifndef PC_INPUT_H
#define PC_INPUT_H

#include "pc/platform.h"

#ifdef __cplusplus
extern "C" {
#endif

// PC-native controller pad (matches N64 OSContPad layout)
typedef struct PCContPad {
    u16 button;
    s8 stick_x;
    s8 stick_y;
} PCContPad;

// Input initialization
void pc_input_init(void);
void pc_input_shutdown(void);

// Input polling
void pc_input_update(PCContPad* outPad);

// Controller configuration
void pc_input_load_config(void);
void pc_input_save_config(void);

#ifdef __cplusplus
}
#endif

#endif // PC_INPUT_H
