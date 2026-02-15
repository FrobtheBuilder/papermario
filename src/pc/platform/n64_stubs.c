/**
 * n64_stubs.c - Stub implementations of N64 SDK functions for the PC port
 *
 * These functions are hardware-specific N64 operations that have no direct
 * PC equivalent. They are stubbed out to allow the game code to link.
 * Functions that need real PC implementations will be replaced over time.
 */

#include "common.h"
#include "nu/nusys.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

// ============================================================================
// BSD libc functions used by the decomp
// ============================================================================

void bcopy(const void *src, void *dst, int len) {
    memmove(dst, src, len);
}

void bzero(void *dst, int len) {
    memset(dst, 0, len);
}

// ============================================================================
// NuSystem Controller Stubs
// ============================================================================

u8 nuContInit(void) {
    // Return 1 = controller 1 connected (bit 0 set)
    return 1;
}

void nuContRmbStart(u32 contNo, u16 freq, u16 frame) {
    // Rumble: no-op on PC
    (void)contNo; (void)freq; (void)frame;
}

s32 nuContRmbCheck(u32 contNo) {
    (void)contNo;
    return 0; // no rumble pack
}

void nuContRmbModeSet(u32 contNo, u8 mode) {
    (void)contNo; (void)mode;
}

void nuContRmbForceStop(void) {
}

void nuContRmbForceStopEnd(void) {
}

// ============================================================================
// NuSystem Graphics Stubs
// ============================================================================

u32 nuGfxCfbNum = 2;

// NuSystem graphics buffers (allocated on PC heap instead of N64 RDRAM)
static u16 sPC_ZBuffer[SCREEN_WIDTH * SCREEN_HEIGHT];
static u16 sPC_Cfb0[SCREEN_WIDTH * SCREEN_HEIGHT];
static u16 sPC_Cfb1[SCREEN_WIDTH * SCREEN_HEIGHT];
static u16* sPC_CfbPtrs[3] = { sPC_Cfb0, sPC_Cfb1, NULL };

void nuGfxInitEX2(void) {
    // Allocate N64-equivalent graphics buffers so display list commands
    // (G_SETCIMG, G_SETZIMG) get valid non-NULL addresses.
    nuGfxZBuffer = sPC_ZBuffer;
    nuGfxCfb = sPC_CfbPtrs;
    nuGfxCfb_ptr = sPC_CfbPtrs[0];
    fprintf(stderr, "[PC] nuGfxInitEX2: zbuf=%p cfb0=%p cfb1=%p\n",
            (void*)nuGfxZBuffer, (void*)sPC_Cfb0, (void*)sPC_Cfb1);
}

void nuGfxDisplayOff(void) {
}

void nuGfxDisplayOn(void) {
}

void nuGfxFuncSet(NUGfxFunc func) {
    // TODO: Store callback for PC main loop to call
    (void)func;
}

void nuGfxPreNMIFuncSet(NUGfxPreNMIFunc func) {
    // Pre-NMI doesn't exist on PC
    (void)func;
}

void nuGfxSetCfb(u16** framebuf, u32 framebufnum) {
    nuGfxCfb = framebuf;
    nuGfxCfbNum = framebufnum;
    if (framebuf && framebufnum > 0) {
        nuGfxCfb_ptr = framebuf[0];
    }
}

void nuGfxTaskAllEndWait(void) {
    // No RSP tasks on PC
}

// Defined in src/pc/gfx/rdp_translator.c
extern void rdp_process_display_list(Gfx* dl, u32 sizeBytes);

void nuGfxTaskStart(Gfx *gfxList_ptr, u32 gfxListSize, u32 ucode, u32 flag) {
    // Route the display list to our RDP-to-OpenGL translator
    rdp_process_display_list(gfxList_ptr, gfxListSize);
    (void)ucode; (void)flag;
}

// ============================================================================
// NuSystem PI (ROM Access) Stubs
// ============================================================================

void nuPiReadRom(u32 rom_addr, void* buf_ptr, u32 size) {
    // TODO: Read from extracted ROM data files
    (void)rom_addr;
    memset(buf_ptr, 0, size);
}

void nuPiReadRomOverlay(NUPiOverlaySegment* segment) {
    // TODO: Load overlay from extracted files
    (void)segment;
}

// ============================================================================
// libultra OS Thread/Message Stubs
// ============================================================================

void osCreateThread(OSThread *t, OSId id, void (*entry)(void *), void *arg,
                    void *sp, OSPri pri) {
    (void)t; (void)id; (void)entry; (void)arg; (void)sp; (void)pri;
}

void osStartThread(OSThread *t) {
    (void)t;
}

void osStopThread(OSThread *t) {
    (void)t;
}

void osCreateMesgQueue(OSMesgQueue *mq, OSMesg *msg, s32 count) {
    if (mq) {
        memset(mq, 0, sizeof(*mq));
    }
    (void)msg; (void)count;
}

s32 osRecvMesg(OSMesgQueue *mq, OSMesg *msg, s32 flag) {
    (void)mq; (void)msg; (void)flag;
    return -1; // no message
}

void osSetEventMesg(OSEvent e, OSMesgQueue *mq, OSMesg msg) {
    (void)e; (void)mq; (void)msg;
}

// ============================================================================
// libultra Timer/Counter Stubs
// ============================================================================

u32 osGetCount(void) {
    // Return a monotonically increasing counter (microseconds)
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (u32)(ts.tv_sec * 1000000 + ts.tv_nsec / 1000);
}

OSTime osGetTime(void) {
    return (OSTime)osGetCount();
}

void osSetTime(OSTime time) {
    (void)time;
}

// ============================================================================
// libultra Video Stubs
// ============================================================================

s32 osTvType = 1; // NTSC

static u8 s_dummyFramebuffer[320 * 240 * 2]; // 16-bit framebuffer

OSViMode osViModeNtscLan1 = {0};
OSViMode osViModeMpalLan1 = {0};

void osViSetMode(OSViMode *mode) {
    (void)mode;
}

void osViSetSpecialFeatures(u32 features) {
    (void)features;
}

void osViSwapBuffer(void *addr) {
    (void)addr;
}

void osViBlack(u8 on) {
    (void)on;
}

void osViRepeatLine(u8 on) {
    (void)on;
}

void* osViGetCurrentFramebuffer(void) {
    return s_dummyFramebuffer;
}

// ============================================================================
// libultra Cache/Memory Stubs
// ============================================================================

u32 osMemSize = 0x00800000; // 8 MB (Expansion Pak)

void osInvalDCache(void *addr, s32 len) {
    (void)addr; (void)len;
}

void osInvalICache(void *addr, s32 len) {
    (void)addr; (void)len;
}

void osWritebackDCache(void *addr, s32 len) {
    (void)addr; (void)len;
}

void osWritebackDCacheAll(void) {
}

uintptr_t osVirtualToPhysical(void *addr) {
    // On PC, just return the pointer as an integer (no TLB translation)
    return (uintptr_t)addr;
}

// ============================================================================
// libultra TLB Stubs
// ============================================================================

void osMapTLB(s32 index, OSPageMask pm, void *vaddr, u32 evenpaddr, u32 oddpaddr, s32 asid) {
    (void)index; (void)pm; (void)vaddr; (void)evenpaddr; (void)oddpaddr; (void)asid;
}

void osUnmapTLB(s32 index) {
    (void)index;
}

void osUnmapTLBAll(void) {
}

// ============================================================================
// libultra PI (Cartridge DMA) Stubs
// ============================================================================

s32 osEPiStartDma(OSPiHandle *pihandle, OSIoMesg *mb, s32 direction) {
    (void)pihandle; (void)mb; (void)direction;
    return 0;
}

s32 osEPiReadIo(OSPiHandle *pihandle, u32 devAddr, u32 *data) {
    (void)pihandle; (void)devAddr;
    if (data) *data = 0;
    return 0;
}

s32 osEPiWriteIo(OSPiHandle *pihandle, u32 devAddr, u32 data) {
    (void)pihandle; (void)devAddr; (void)data;
    return 0;
}

// ============================================================================
// libultra Flash (Save Data) Stubs
// ============================================================================

static OSPiHandle s_flashHandle;

OSPiHandle *osFlashInit(void) {
    // TODO: Implement file-based save data
    return &s_flashHandle;
}

s32 osFlashReadArray(OSIoMesg *mb, s32 priority, u32 page_num,
                     void *dramAddr, u32 n_pages, OSMesgQueue *mq) {
    (void)mb; (void)priority; (void)page_num; (void)n_pages; (void)mq;
    if (dramAddr) {
        memset(dramAddr, 0, n_pages * 128); // 128 bytes per page
    }
    return 0;
}

s32 osFlashSectorErase(u32 page_num) {
    (void)page_num;
    return 0;
}

s32 osFlashWriteBuffer(OSIoMesg *mb, s32 priority,
                       void *dramAddr, OSMesgQueue *mq) {
    (void)mb; (void)priority; (void)dramAddr; (void)mq;
    return 0;
}

s32 osFlashWriteArray(u32 page_num) {
    (void)page_num;
    return 0;
}

// ============================================================================
// Internal OS Stubs
// ============================================================================

OSThread *__osGetActiveQueue(void) {
    return NULL;
}

// ============================================================================
// libultra printf stub
// The game's _Printf has a different signature from MinGW's _Printf.
// On PC, crash_screen and is_debug output goes to stdout via printf instead.
// ============================================================================

// Use the game's signature from libc/xstdio.h
typedef char *(*pm_outfun)(char*, const char*, size_t);

int _Printf(pm_outfun prout, char *arg, const char *fmt, va_list args) {
    // Redirect to standard vprintf for basic output
    (void)prout; (void)arg;
    return vprintf(fmt, args);
}

// ============================================================================
// PC Boot Helper
// Called from pc_main.c to set game state that needs access to common.h types
// ============================================================================

void pc_set_controller_connected(void) {
    // Replicate boot_main: gGameStatusPtr->contBitPattern = nuContInit()
    gGameStatusPtr->contBitPattern = 1;
}
