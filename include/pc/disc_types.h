/**
 * @file disc_types.h
 * @brief Marks for structs whose bytes come from the disc (OrrePort): HSD archive descriptions and other data the
 * game reads in place.
 *
 * On the GameCube every mark is the plain form (no code change). On the PC (GCC): DISC_STRUCT makes GCC read and
 * write the struct's scalars big-endian (scalar_storage_order). GCC does not swap pointer fields, so a disc pointer
 * slot is DISC_PTR(T): a big-endian u32 holding a low-memory address (option (a), ptr32.h), with a zero-length member
 * that carries the pointee type for DP(). packed + aligned(4) keep the GameCube size and alignment.
 * GCC also does not apply the order to an unmarked nested struct (silently), and refuses to take the address of a
 * reverse-order scalar: embedded aggregates use the disc twins below, and code that needs a native object copies.
 * How the archive loader uses them: the relocation pass adds the archive base to each pointer word with
 * DISC_WORD_ADD; after that every description is read through its marked type (src/hsd, hsd_wobj.c Locate).
 * Proposed by A, added on A's branch at the project's decision (2026-10-15); first written as include/hsd/hsd_disc.h.
 */
#ifndef PC_DISC_TYPES_H
#define PC_DISC_TYPES_H

#ifndef DISC_STRUCT
#ifdef TARGET_PC
#include <stdint.h>
#define DISC_STRUCT __attribute__((scalar_storage_order("big-endian")))
#define DISC_PTR(T) union __attribute__((scalar_storage_order("big-endian"), packed, aligned(4))) { u32 raw; T* type_[0]; }
#define DP(slot) ((__typeof__((slot).type_[0]))PC_PTR((slot).raw))
#define DP_SET(slot, p) ((slot).raw = PC_PTR32(p))
#define DISC_WORD_ADD(wp, v) (*(wp) = __builtin_bswap32(__builtin_bswap32(*(wp)) + (u32)(v)))
/* the i-th 32-bit pointer slot of a list in disc data (e.g. an HSD archive's scene_data lists), as a pointer */
#define DISC_SLOT(list, i) ((void*)PC_PTR(__builtin_bswap32(((const u32*)(list))[i])))
/* a 32-bit word of disc data read at a raw address, keeping the original read's signedness */
#define DISC_U32(p) ((u32)__builtin_bswap32(*(const u32*)(p)))
#define DISC_S32(p) ((s32)__builtin_bswap32(*(const u32*)(p)))
#define DISC_U16(p) ((u16)__builtin_bswap16(*(const u16*)(p)))
#define DISC_S16(p) ((s16)__builtin_bswap16(*(const u16*)(p)))
static inline f32 disc_f32_(const void* p) { union { u32 w; f32 f; } u; u.w = __builtin_bswap32(*(const u32*)p); return u.f; }
#define DISC_F32(p) disc_f32_(p)
static inline void disc_store_f32_(void* p, f32 v) { union { u32 w; f32 f; } u; u.f = v; *(u32*)p = __builtin_bswap32(u.w); }
#define DISC_STORE_F32(p, v) disc_store_f32_((p), (v))
/* a 16- / 32-bit store into disc data at a raw address (T: the original cast's type) */
#define DISC_STORE16(T, p, v) (*(T*)(p) = (T)__builtin_bswap16((u16)(v)))
#define DISC_STORE32(T, p, v) (*(T*)(p) = (T)__builtin_bswap32((u32)(v)))
/* element i of a 16-bit array in disc data, read / stored big-endian (GameCube: the plain element, as matched code
 * reads it: MWCC compiles `t[i]` and `*(s16*)&t[i]` differently) */
#define DISC_EL_S16(t, i) DISC_S16(&(t)[i])
/* a pointer stored as a big-endian 32-bit slot at any address in disc data (e.g. a jump target inside a byte-code
 * stream): GameCube `*(T**)(p)`, exactly as the matched code reads it */
#define DISC_PTR_AT(T, p) ((T*)PC_PTR(__builtin_bswap32(*(const u32*)(p))))
#define DISC_EL_SET16(t, i, v) DISC_STORE16(s16, &(t)[i], (v))
#define DISC_EL_F32(t, i) DISC_F32(&(t)[i])
#else
#define DISC_STRUCT
#define DISC_PTR(T) T*
#define DP(slot) (slot)
#define DP_SET(slot, p) ((slot) = (p))
#define DISC_WORD_ADD(wp, v) (*(wp) += (v))
#define DISC_SLOT(list, i) (((void**)(list))[i])
#define DISC_U32(p) (*(u32*)(p))
#define DISC_S32(p) (*(s32*)(p))
#define DISC_U16(p) (*(u16*)(p))
#define DISC_S16(p) (*(s16*)(p))
#define DISC_F32(p) (*(f32*)(p))
#define DISC_STORE_F32(p, v) (*(f32*)(p) = (v))
#define DISC_STORE16(T, p, v) (*(T*)(p) = (v))
#define DISC_STORE32(T, p, v) (*(T*)(p) = (v))
#define DISC_EL_S16(t, i) ((t)[i])
#define DISC_PTR_AT(T, p) (*(T**)(p))
#define DISC_EL_SET16(t, i, v) ((t)[i] = (v))
#define DISC_EL_F32(t, i) ((t)[i])
#endif
#endif

/* Aggregates embedded in disc structs need the big-endian order too (GCC does not apply it to an unmarked nested
 * struct, silently): their disc twins. Code that copies them into native structs copies field by field. */
#ifdef TARGET_PC
typedef struct DISC_STRUCT HSD_DiscVec3 { f32 x, y, z; } HSD_DiscVec3;
/* copy a disc vector into a native Vec3, field by field */
#define HSD_DISC_VEC3_GET(dst, src) ((dst).x = (src).x, (dst).y = (src).y, (dst).z = (src).z)
/* copy n big-endian 32-bit words (floats, matrices) out of disc data */
static inline void hsd_disc_copy_words(void* dst, const void* src, u32 n)
{
    u32* d = (u32*)dst;
    const u32* s = (const u32*)src;
    while (n-- != 0) {
        *d++ = __builtin_bswap32(*s++);
    }
}
#define HSD_DISC_COPY_F32(dst, src, n) hsd_disc_copy_words((dst), (src), (n))
/* a native Vec temporary holding a disc vector, for calls that take a Vec* (lives to the end of the block) */
#define HSD_DISC_VEC3_TMP(p) (&(Vec){ (p)->x, (p)->y, (p)->z })
#else
#define HSD_DiscVec3 Vec3 /* the same Vec3 as before: no code change */
#define HSD_DISC_VEC3_GET(dst, src) ((dst) = (src))
#define HSD_DISC_COPY_F32(dst, src, n) memcpy((dst), (src), (n) * sizeof(f32))
#define HSD_DISC_VEC3_TMP(p) (p)
#endif
/* next_p() for a disc list (hsd_class.h's next_p on the GameCube) */
#define DISC_NEXT_P(p) ((p) != NULL ? DP((p)->next) : NULL)
typedef f32 HSD_MtxRow[4]; /* DISC_PTR(HSD_MtxRow) is MtxPtr on the GameCube */

#endif /* PC_DISC_TYPES_H */
