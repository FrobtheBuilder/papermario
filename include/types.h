#ifndef _TYPES_H_
#define _TYPES_H_

#include "ultra64.h"

#if !defined(OLD_GCC) && (defined(__cplusplus) && __cplusplus >= 201103L)
    /* C++11 or later */
    #include <cstddef>
#elif !defined(OLD_GCC) && (defined(__STDC__) && defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 202000L))
    /* C23 or later */
    #include <stddef.h>
#else
    #define nullptr (void*)0
    typedef void* nullptr_t;
#endif

#define UNK_TYPE s32
#define UNK_PTR void*
#define UNK_RET void
#define UNK_FUN_ARG void(*)(void)
#define UNK_FUN_PTR(name) void(*name)(void)
#define UNK_ARGS

typedef char* (*PrintCallback)(void*, const char*, u32);

/// Linker symbol address, as in `ld_addrs.h`.
/// On N64, Addr symbols are placed by the linker at specific addresses.
/// Code uses `(u32) symbolName` to get the symbol's address = the ROM offset.
/// On PC, we can't place symbols at arbitrary addresses, so Addr is a u32
/// whose VALUE is the ROM offset.
#ifdef PC_BUILD
typedef u32 Addr;
#else
typedef u8 Addr[];
#endif

#define BTL_NONE 0
#define BTL_DEFAULT_STAGE -1

/// Terminates an extraAnimationList
#define ANIM_LIST_END -1

#endif
