#ifndef CRT_STRING_H
#define CRT_STRING_H

#include "dolphin/types.h"

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
s32 memcmp(const void* lhs, const void* rhs, u32 len);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void* memcpy(void* dst, const void* src, u32 len);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void* memset(void* dst, int value, u32 len);
#endif

#endif /* CRT_STRING_H */
