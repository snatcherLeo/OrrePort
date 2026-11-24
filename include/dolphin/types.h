#ifdef TARGET_PC
/*
 * OrrePort (PC build): the SDK types come from Aurora's dolphin/types.h (u32 = uint32_t, BOOL, TRUE / FALSE, NULL
 * from <stddef.h>). The PC build puts include/ before extern/aurora/include, so #include_next reaches Aurora's file,
 * which defines the shared guard DOLPHIN_TYPES_H. Only the GameCube-side extras are added here.
 */
#include_next <dolphin/types.h>
#ifndef ORRE_PC_TYPES_EXTRA
#define ORRE_PC_TYPES_EXTRA
typedef s64 OSTime;
typedef s32 OSHeapHandle;
#endif
/* MWCC places data with __declspec(section ...); on the PC the section doesn't matter */
#ifndef SECTION
#define SECTION(name)
#endif
#else /* GameCube build: unchanged */
#ifndef DOLPHIN_TYPES_H
#define DOLPHIN_TYPES_H

/*
 * Standard GameCube/Dolphin SDK type definitions.
 * These match the MetroWerks CodeWarrior / Nintendo SDK conventions.
 */

typedef signed char s8;
typedef signed short s16;
typedef signed long s32;
typedef signed long long s64;

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;
typedef unsigned long long u64;

typedef float f32;
typedef double f64;

typedef int BOOL;

#define TRUE 1
#define FALSE 0
#define NULL ((void*)0)

typedef s64 OSTime;
typedef s32 OSHeapHandle;

/* Place an object in a named section (the PC build defines this as nothing) */
#define SECTION(name) __declspec(section name)

/* Size type for memory operations */
#ifdef __MWERKS__
typedef u32 size_t;
#endif

#endif /* DOLPHIN_TYPES_H */
#endif /* TARGET_PC */
#include "pc/ptr32.h" /* PC_PTR32: pointers kept in 32-bit integers (OrrePort) */
#include "pc/argorder.h" /* calls with float / integer arguments in a different order than the callee (OrrePort) */
