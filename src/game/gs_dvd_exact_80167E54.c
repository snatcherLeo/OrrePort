#include "dolphin/types.h"

#ifdef TARGET_PC
#include <dolphin/dvd.h> /* DVDFileInfo: GSDVDWork embeds the real one on the PC */
#undef DVDRead /* Aurora's macros; the game calls the functions port/src/pc_dvd.c provides */
#undef DVDReadAsync
#define GSDVD_INFO_OFS 8 /* offsetof(GSDVDWork, fileInfo): DVDFileInfo holds pointers, so it is 8-aligned */
#else
#define GSDVD_INFO_OFS 4 /* offsetof(GSDVDWork, fileInfo) */
#endif

s32 fn_80167E54(void)
{
    return -1;
}

u32 GSdvdGetLength(u8* object)
{
#ifdef TARGET_PC
    return ((DVDFileInfo*)(object + GSDVD_INFO_OFS))->length; /* GameCube: fileInfo +0x34 */
#else
    return *(u32*)(object + 0x38);
#endif
}
