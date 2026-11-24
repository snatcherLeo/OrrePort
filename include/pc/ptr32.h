/* PC_PTR32(p): a pointer kept in a 32-bit integer (OrrePort).
 *
 * The game code often stores pointers in u32 variables, fields and return values. On the GameCube that is exact. On
 * the 64-bit PC it is exact only because everything the game touches lives in the lowest 4 GB (snatcherLeo's decision
 * 2026-10-15, option (a): the project notes, plans.md Phase B). Every such conversion is wrapped in this
 * macro (tools/local/port/ptr32wrap.py), so:
 *   - GameCube: PC_PTR32 (which replaced an explicit `(u32)p` cast) is that cast; PC_PTR32_IMPLICIT (a conversion
 *     that was implicit, often through a local declaration only the GameCube build sees) is the bare expression:
 *     either way the same code as before;
 *   - PC debug builds: asserts that the pointer is below 4 GB, so a violation fails at the exact spot;
 *   - PC builds with ORRE_NO_PTR32_CHECK (release): a plain cast. Not tied to NDEBUG, which the game's own build
 *     flags define for every unit.
 * PC_PTR(u) is the other direction, a 32-bit value turned back into a pointer (the original cast, if any, stays in
 * front: `(T*)PC_PTR(u)`): GameCube `(u)`; PC `(void*)(uintptr_t)(u)`.
 * ORRE_PTR32_ENCODED (arm64 macOS, where the low 4 GB is reserved: docs/port/low-memory.md): stores encode with
 * pc_encode_dp() and loads decode with pc_resolve_dp(), Melee's PC-port method (port/ defines both).
 * Fixing a site properly (a real pointer type) removes its macro. */
#ifndef PC_PTR32_H
#define PC_PTR32_H
/* Included at the end of dolphin/types.h (u32 is defined by then), so every file has it without an extra line. */

#ifdef TARGET_PC
#include <stdint.h>
#if defined(ORRE_PTR32_ENCODED)
u32 pc_encode_dp(const void* p);
void* pc_resolve_dp(u32 value);
#define PC_PTR32(p) pc_encode_dp((const void*)(p))
#define PC_PTR32_IMPLICIT(p) pc_encode_dp((const void*)(p))
#define PC_PTR(u) pc_resolve_dp((u32)(u))
#elif defined(ORRE_NO_PTR32_CHECK)
#define PC_PTR32(p) ((u32)(uintptr_t)(p))
#define PC_PTR32_IMPLICIT(p) ((u32)(uintptr_t)(p))
#define PC_PTR(u) ((void*)(uintptr_t)(u32)(u))
#else
void pc_ptr32_fail(uintptr_t value, const char* file, int line); /* port/: reports and aborts */
static inline u32 pc_ptr32_check(uintptr_t value, const char* file, int line) {
    if (value >> 32) {
        pc_ptr32_fail(value, file, line);
    }
    return (u32)value;
}
#define PC_PTR32(p) pc_ptr32_check((uintptr_t)(p), __FILE__, __LINE__)
#define PC_PTR32_IMPLICIT(p) pc_ptr32_check((uintptr_t)(p), __FILE__, __LINE__)
#define PC_PTR(u) ((void*)(uintptr_t)(u32)(u))
#endif
#else
#define PC_PTR32(p) ((u32)(p))
#define PC_PTR32_IMPLICIT(p) (p)
#define PC_PTR(u) (u)
#endif

/* RegWord: a GameCube register-sized argument in the register-transliteration code's function-pointer casts
 * (`((void (*)(RegWord, RegWord))fn)(r3, r4)`, written by tools/local/sweep/deregister.py). u32 on the GameCube (the
 * same code as before); pointer-sized on the PC, so a u32 value is zero-extended and a callee that really takes a
 * pointer gets a clean one: Windows x64 leaves the upper half of a 32-bit argument register undefined (A, 2026-10-15). */
#ifdef TARGET_PC
typedef uintptr_t RegWord;
#else
typedef u32 RegWord;
#endif

/* PTR32_FIELD(T): a pointer field kept in 32 bits on the PC too, so a runtime record keeps its exact GameCube layout.
 * For records that many files view through private structs and raw offsets (windows, sprite nodes: B's boot run 19
 * crashed because each view put the 8-byte pointers at different offsets). Native byte order (runtime data, never
 * read from the disc), so a raw `*(u32*)(rec + off)` read is right as well. Read with P32(), write with P32_SET().
 * GameCube: a plain pointer; P32 / P32_SET are the expression / assignment themselves. */
#ifdef TARGET_PC
#define PTR32_FIELD(T) union __attribute__((packed, aligned(4))) { u32 raw; T* type_[0]; }
#define P32(slot) ((__typeof__((slot).type_[0]))PC_PTR((slot).raw))
#define P32_SET(slot, p) ((slot).raw = PC_PTR32(p))
#else
#define PTR32_FIELD(T) T*
#define P32(slot) (slot)
#define P32_SET(slot, p) ((slot) = (p))
#endif

/* PTR32_AT / PTR32_AT_SET: a pointer stored at a raw offset inside such a GameCube-layout runtime record
 * (`*(void**)((u8*)window + 0x60)` in the decompiled C): a native 32-bit slot on the PC. Not for disc data (that is
 * big-endian: DISC_SLOT in pc/disc_types.h). */
#ifdef TARGET_PC
#define PTR32_AT(base, off) ((void*)PC_PTR(*(const u32*)((const u8*)(base) + (off))))
#define PTR32_AT_SET(base, off, p) (*(u32*)((u8*)(base) + (off)) = PC_PTR32(p))
#else
#define PTR32_AT(base, off) (*(void**)((u8*)(base) + (off)))
#define PTR32_AT_SET(base, off, p) (*(void**)((u8*)(base) + (off)) = (p))
#endif
/* Typed forms: on the GameCube exactly the original `*(T**)(base + off)` load / store (MWCC schedules a load through
 * void** differently, so matched functions keep the original type); `base` must already be a byte pointer. */
#ifdef TARGET_PC
#define PTR32_AT_T(T, base, off) ((T*)PC_PTR(*(const u32*)((const u8*)(base) + (off))))
#define PTR32_AT_SET_T(T, base, off, p) (*(u32*)((u8*)(base) + (off)) = PC_PTR32(p))
#else
#define PTR32_AT_T(T, base, off) (*(T**)((base) + (off)))
#define PTR32_AT_SET_T(T, base, off, p) (*(T**)((base) + (off)) = (p))
#endif

#endif
