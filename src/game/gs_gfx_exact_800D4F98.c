/**
 * @file gs_gfx_exact_800D4F98.c
 * @brief GSgfx request buffer: command recorder and allocator,
 *        0x800D4F98 - 0x800D55D0.
 *
 * While the GSgfx command mode (state+0x000) is 1, the GS state setters
 * record their calls here instead of touching GX: fn_800D4F98 appends the
 * command id and its arguments, laid out by a format code, at the request
 * write pointer.  gfxAllocRequestBuffer allocates that buffer (GSgfxInit passes
 * 0x6DDD0 bytes).
 *
 * The unit owns fn_800D4F98's switch jump table (.data 0x803142F8) and the
 * three request-buffer messages (.rodata 0x802703C0).  MWCC addresses the
 * messages through one string-pool base register; GC/1.3.2 keeps the first
 * one as `addi r3,r31,0` where GC/1.3 emits `mr`.
 */
#include "dolphin/types.h"
#include "dolphin/gx/GX.h"
#ifdef TARGET_PC /* the PC build uses the host C library's va_list */
#include <stdarg.h>
#else
#include "crt/stdarg.h"
#endif
#include "game/gs_gfx_layer.h"

extern void GSlogWrite(const char*, ...);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* memcpy(void* dst, const void* src, u32 n);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16 _toolentryAlloc__FUl(u32 size); /* GSmemAllocRaw */
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* GSmemLock(u16 handle);      /* GSmemGetPtr */
#endif
extern u8 lbl_804001F0[];                  /* per-frame timing counters */

/*
 * Record one GS command.  `format` selects how the variadic arguments are
 * stored: 1-10 words, 11-14 floats, 15 word + 2 floats, 16 a 3x4 matrix,
 * 17 word + matrix, 18 word + 0x18 bytes (0x60-byte slot) + word,
 * 19 three words + matrix, 20 word + GXColor.
 */
void gfxRecordCommand(u32 command, u32 format, ...)
{
    va_list ap;
    f32 value;
    GXColor color;

    *gsgfxState->requestBufferPos++ = command;
    va_start(ap, format);

    switch (format) {
    case 10:
        *gsgfxState->requestBufferPos++ = va_arg(ap, u32);
    case 9:
        *gsgfxState->requestBufferPos++ = va_arg(ap, u32);
    case 8:
        *gsgfxState->requestBufferPos++ = va_arg(ap, u32);
    case 7:
        *gsgfxState->requestBufferPos++ = va_arg(ap, u32);
    case 6:
        *gsgfxState->requestBufferPos++ = va_arg(ap, u32);
    case 5:
        *gsgfxState->requestBufferPos++ = va_arg(ap, u32);
    case 4:
        *gsgfxState->requestBufferPos++ = va_arg(ap, u32);
    case 3:
        *gsgfxState->requestBufferPos++ = va_arg(ap, u32);
    case 2:
        *gsgfxState->requestBufferPos++ = va_arg(ap, u32);
    case 1:
        *gsgfxState->requestBufferPos++ = va_arg(ap, u32);
        break;
    case 14:
        value = va_arg(ap, f64);
        *gsgfxState->requestBufferPos++ = *(u32*)&value;
    case 13:
        value = va_arg(ap, f64);
        *gsgfxState->requestBufferPos++ = *(u32*)&value;
    case 12:
        value = va_arg(ap, f64);
        *gsgfxState->requestBufferPos++ = *(u32*)&value;
    case 11:
        value = va_arg(ap, f64);
        *gsgfxState->requestBufferPos++ = *(u32*)&value;
        break;
    case 15:
        *gsgfxState->requestBufferPos++ = va_arg(ap, u32);
        value = va_arg(ap, f64);
        *gsgfxState->requestBufferPos++ = *(u32*)&value;
        value = va_arg(ap, f64);
        *gsgfxState->requestBufferPos++ = *(u32*)&value;
        break;
    case 16:
        memcpy(gsgfxState->requestBufferPos, va_arg(ap, void*), 0x30);
        gsgfxState->requestBufferPos += 12;
        break;
    case 17:
        *gsgfxState->requestBufferPos++ = va_arg(ap, u32);
        memcpy(gsgfxState->requestBufferPos, va_arg(ap, void*), 0x30);
        gsgfxState->requestBufferPos += 12;
        break;
    case 18:
        *gsgfxState->requestBufferPos++ = va_arg(ap, u32);
        memcpy(gsgfxState->requestBufferPos, va_arg(ap, void*), 0x18);
        gsgfxState->requestBufferPos += 24;
        *gsgfxState->requestBufferPos++ = va_arg(ap, u32);
        break;
    case 19:
        *gsgfxState->requestBufferPos++ = va_arg(ap, u32);
        *gsgfxState->requestBufferPos++ = va_arg(ap, u32);
        *gsgfxState->requestBufferPos++ = va_arg(ap, u32);
        memcpy(gsgfxState->requestBufferPos, va_arg(ap, void*), 0x30);
        gsgfxState->requestBufferPos += 12;
        break;
    case 20:
        *gsgfxState->requestBufferPos++ = va_arg(ap, u32);
        color = va_arg(ap, GXColor);
        *gsgfxState->requestBufferPos++ = *(u32*)&color;
        break;
    }
    va_end(ap);
}

/* Allocate the request buffer that fn_800D4F98 records into. */
void gfxAllocRequestBuffer(u32 size)
{
    if (gsgfxState->requestBuffer != NULL) {
        GSlogWrite("GSgfx: request buffer already alloced\n");
        return;
    }

    gsgfxState->requestBufferHandle = _toolentryAlloc__FUl(size);
    if (gsgfxState->requestBufferHandle == 0) {
        GSlogWrite("GSgfx: can't alloc request buffer\n");
        return;
    }

    gsgfxState->requestBuffer = GSmemLock(gsgfxState->requestBufferHandle);
    gsgfxState->requestBufferPos = gsgfxState->requestBuffer;
    gsgfxState->requestBufferSize = size;
    *(u32*)(lbl_804001F0 + 0x28) = 0;
    GSlogWrite("GSgfx: alloced request buffer of %d bytes at %08X\n",
               gsgfxState->requestBufferSize, gsgfxState->requestBuffer);
}
