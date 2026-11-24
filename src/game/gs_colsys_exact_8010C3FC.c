/**
 * @file gs_colsys_exact_8010C3FC.c
 * @brief Byte-exact GScolsys2 range, 0x8010C3FC - 0x8010C46C.
 */
#include "dolphin/types.h"
#include "pc/disc_types.h" /* DISC_SLOT: the menu-face table is common_rel data */

extern void* lbl_80478E70;
extern void* lbl_80478E74;
extern char lbl_80272000[];
extern char lbl_8035B4E8[];
extern void GSlogWrite(char*, char*, ...);

void* _menuFaceBiosGetPtr__FUs(u16 idx)
{
    u8* entry;
    u16 i = idx;

    if (i >= *(u32*)lbl_80478E70) {
#ifdef TARGET_PC
        { /* PC, observe only: the out-of-range key and who asked (P24's party menu, 2026-10-27) */
            extern void OSReport(const char*, ...);
            OSReport("[face] key 0x%04X out of range (count %u) from %p\n", (unsigned)idx,
                     (unsigned)*(u32*)lbl_80478E70, __builtin_return_address(0));
        }
#endif
        GSlogWrite(lbl_80272000, lbl_8035B4E8);
        entry = NULL;
    } else {
        entry = (u8*)lbl_80478E74 + i * 8;
    }
    if (entry == NULL) {
        return NULL;
    }
#ifdef TARGET_PC /* a relocated pointer inside common_rel: a big-endian 32-bit slot */
    return DISC_SLOT(entry + 4, 0);
#else
    return *(void**)(entry + 4);
#endif
}
