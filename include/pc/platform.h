#ifndef PC_PLATFORM_H
#define PC_PLATFORM_H

#ifdef __cplusplus
extern "C" {
#endif

// When N64 SDK headers are already included (via common.h -> ultra64.h -> ultratypes.h),
// use their type definitions. Otherwise provide our own self-contained types.
#ifndef _ULTRATYPES_H_
#include <stdint.h>

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float f32;
typedef double f64;

typedef s32 b32;

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif
#else
// N64 types already available; just define b32 if not already defined
#ifndef _PC_B32_DEFINED
#define _PC_B32_DEFINED
typedef s32 b32;
#endif
#endif /* _ULTRATYPES_H_ */

// Platform configuration
typedef struct PlatformConfig {
    s32 windowWidth;
    s32 windowHeight;
    b32 fullscreen;
    b32 vsync;
} PlatformConfig;

// Platform initialization and shutdown
void platform_init(PlatformConfig* config);
void platform_shutdown(void);
void platform_run_main_loop(void);

// Timing functions
u64 platform_get_time_usec(void);
void platform_sleep_ms(u32 ms);

#ifdef __cplusplus
}
#endif

#endif // PC_PLATFORM_H
