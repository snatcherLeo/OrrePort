/** DVD request helpers, 0x80167E64 - 0x80167FA4. */
#include "dolphin/types.h"

#ifdef TARGET_PC
#include <dolphin/dvd.h> /* DVDFileInfo: GSDVDWork embeds the real one on the PC */
#undef DVDRead /* Aurora's macros; the game calls the functions port/src/pc_dvd.c provides */
#undef DVDReadAsync
#define GSDVD_INFO_OFS 8 /* offsetof(GSDVDWork, fileInfo): DVDFileInfo holds pointers, so it is 8-aligned */
#else
#define GSDVD_INFO_OFS 4 /* offsetof(GSDVDWork, fileInfo) */
#endif

typedef struct GSDVDWork {
    u8 active;
    u8 started;
    u8 drawing;
    u8 _pad03;
#ifdef TARGET_PC
    DVDFileInfo fileInfo[1]; /* Aurora's DVDFileInfo is ~0x60 bytes and DVDOpen / DVDRead write all of it (B's boot run 33);
                              * one element, so `work->fileInfo` is still the pointer the DVD calls take */
#else
    u8 fileInfo[0x3C];
#endif
    void (*callback)(s32 result, struct GSDVDWork* work);
} GSDVDWork;

extern void _freeDvdWork(u8* flag);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void DVDClose(void* fileInfo);
#endif
extern BOOL DVDReadAsync(void* fileInfo, void* addr, s32 length, s32 offset,
                         void (*callback)(s32 result, void* fileInfo),
                         s32 priority);
extern s32 DVDRead(void* fileInfo, void* addr, s32 length, s32 offset,
                   s32 priority);
extern s32 DVDConvertPathToEntrynum(const char* path);
extern GSDVDWork* _getNewDvdWork(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern BOOL DVDOpen(const char* path, void* fileInfo);
#endif
extern void _AsyncCallback(s32 result, void* fileInfo);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void __assert(const char* file, u32 line, const char* condition);
#endif
extern const char lbl_8047D584[8];
extern const char lbl_8047D58C[8];

void GSdvdClose(u8* file)
{
    _freeDvdWork(file);
    DVDClose(file + GSDVD_INFO_OFS);
}

u8 GSdvdReadAsync(GSDVDWork* work, void* addr, s32 length, s32 offset,
                void (*callback)(s32 result, GSDVDWork* work))
{
    work->callback = callback;
    return DVDReadAsync(work->fileInfo, addr, length, offset, _AsyncCallback,
                        2);
}

s32 GSdvdRead(GSDVDWork* work, void* addr, s32 length, s32 offset)
{
    return DVDRead(work->fileInfo, addr, length, offset, 2);
}

u32 fn_80167EF8(const char* path)
{
    return DVDConvertPathToEntrynum(path) != -1;
}

GSDVDWork* GSdvdOpen(const char* path)
{
    GSDVDWork* work = _getNewDvdWork();

    if (work == NULL) {
        return NULL;
    }
    if (!DVDOpen(path, work->fileInfo)) {
        __assert(lbl_8047D584, 0x26A, lbl_8047D58C);
        _freeDvdWork((u8*)work);
        return NULL;
    }
    return work;
}
