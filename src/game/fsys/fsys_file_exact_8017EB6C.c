/**
 * @file fsys_file_exact_8017EB6C.c
 * @brief _fsysGetFilename: queue an archive load on a slot
 *        (0x8017EB6C - 0x8017F108).
 *
 * Fills the slot for a load of archive `fileHandle` (callbacks, load mode),
 * builds "<name>.fsys" from the gsfsys.toc name table, and either marks the
 * slot pending (another slot is loading) or makes it the active slot and
 * sets its first state from the load mode (1 / 100 for ordinary modes 0, 2,
 * 3 and 7, 200 for mode 1). A slot already loaded (1000) is re-queued with
 * its data kept; any other state only restarts when a reload was asked for.
 *
 * fsysFindArchiveName is the TOC lookup expanded inline four times, each
 * with the same count/table set-up, compare loop and NULL tail; its found
 * path returns through a local, which level-0 code homes on the stack.
 *
 * Built on the fsys unit's flags: level 0 (see the other fsys units) with
 * read-only strings, which is what puts "%s.fsys" in .sdata2 (0x8047D790),
 * where this unit emits it.
 */
#include "dolphin/types.h"
#include "game/fsys/fsys.h"
#include "pc/disc_types.h" /* DISC_STRUCT / DISC_U32: gsfsys.toc is big-endian disc data, read in place */

extern FSYSManager lbl_80453FEC; /* gFSYSManager */
extern u8* lbl_8047B1B0;         /* gsfsys.toc image */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern int sprintf(char* buf, const char* fmt, ...);
#endif
extern char* strcpy(char* dst, const char* src);

/* gsfsys.toc: +0x08 archive count, +0x10 offset of this table. */
typedef struct DISC_STRUCT FSYSTocName { /* big-endian view on the PC */
    u32 fileHandle;
    u32 nameOffset;
} FSYSTocName;

static inline char* fsysFindArchiveName(u32 fileHandle)
{
    u32 count;
    FSYSTocName* entry;
    u32 i;

    count = DISC_U32(lbl_8047B1B0 + 8);
    entry = (FSYSTocName*)(lbl_8047B1B0 + DISC_U32(lbl_8047B1B0 + 0x10));
    for (i = 0; i < count; i++) {
        if (entry->fileHandle == fileHandle) {
            char* name = (char*)PC_PTR((entry->nameOffset + PC_PTR32(lbl_8047B1B0)));

            return name;
        }
        entry++;
    }
#ifdef TARGET_PC
    {
        /* PC diagnostic (2026-10-17, temporary): a file handle the disc's table doesn't have ("(null).fsys",
         * Willie's battle after "ESPEON used CONFUSION!"); report it and who asked */
        extern void OSReport(const char*, ...);
        extern void pc_crash_print_stack(const char* reason);
        OSReport("[fsys] no archive for file handle 0x%08X (%u entries)\n", (unsigned)fileHandle, (unsigned)count);
        pc_crash_print_stack("[fsys] the caller:");
    }
#endif
    return NULL;
}

void _fsysGetFilename(FSYSSlot* slot, u32 fileHandle, u32 callbackA,
                      u32 callbackB, u32 callbackC, s32 loadMode)
{
    switch (slot->status) {
    case 0:
    case 0x1F4: {
        char name[0x80];

        slot->archiveHandle = 0;
        P32_SET(slot->archiveData, NULL);
        slot->entryIndex = 0;
        slot->callbackA = callbackA;
        slot->callbackB = callbackB;
        slot->callbackC = callbackC;
        slot->fileHandle = fileHandle;
        P32_SET(slot->currentSub, NULL);
        slot->loadMode = loadMode;
        slot->reloadFlag = 0;
        slot->padding100 = 0;
        sprintf(name, "%s.fsys", fsysFindArchiveName(fileHandle));
        strcpy(slot->filename, name);
        if (lbl_80453FEC.activeSlot && slot != lbl_80453FEC.activeSlot) {
            slot->status = 0x1F4;
            return;
        }
        lbl_80453FEC.activeSlot = slot;
        lbl_80453FEC.currentSlot = slot;
        switch (loadMode) {
        case 0:
        case 2:
        case 3:
        case 7:
            slot->status = 1;
            break;
        case 1:
            slot->status = 0xC8;
            break;
        }
        break;
    }
    case 0x3E8:
        if (loadMode == 3) {
            char name[0x80];
            char* archive;

            slot->archiveHandle = 0;
            slot->entryIndex = 0;
            slot->callbackA = callbackA;
            slot->callbackB = callbackB;
            slot->callbackC = callbackC;
            slot->fileHandle = fileHandle;
            P32_SET(slot->currentSub, NULL);
            slot->loadMode = loadMode;
            slot->reloadFlag = 1;
            sprintf(name, "%s.fsys", fsysFindArchiveName(fileHandle));
            strcpy(slot->filename, name);
            if (lbl_80453FEC.activeSlot) {
                slot->status = 0x1F4;
                return;
            }
            lbl_80453FEC.activeSlot = slot;
            lbl_80453FEC.currentSlot = slot;
            switch (loadMode) {
            case 0:
            case 2:
            case 3:
            case 7:
                slot->status = 0x64;
                break;
            case 1:
                slot->status = 0xC8;
                break;
            }
        } else {
            char name[0x80];
            char* archive;

            slot->archiveHandle = 0;
            slot->entryIndex = 0;
            slot->callbackA = callbackA;
            slot->callbackB = callbackB;
            slot->callbackC = callbackC;
            slot->fileHandle = fileHandle;
            P32_SET(slot->currentSub, NULL);
            slot->loadMode = loadMode;
            slot->reloadFlag = 1;
            sprintf(name, "%s.fsys", fsysFindArchiveName(fileHandle));
            strcpy(slot->filename, name);
            if (lbl_80453FEC.activeSlot) {
                slot->status = 0x1F4;
                return;
            }
            lbl_80453FEC.activeSlot = slot;
            lbl_80453FEC.currentSlot = slot;
            switch (loadMode) {
            case 0:
            case 2:
            case 3:
            case 7:
                slot->status = 0x64;
                break;
            case 1:
                slot->status = 0xC8;
                break;
            }
        }
        break;
    default:
        if (slot->reloadFlag == 1) {
            char name[0x80];
            char* archive;

            slot->status = 0;
            slot->archiveHandle = 0;
            P32_SET(slot->archiveData, NULL);
            slot->entryIndex = 0;
            slot->callbackA = callbackA;
            slot->callbackB = callbackB;
            slot->callbackC = callbackC;
            slot->fileHandle = fileHandle;
            P32_SET(slot->currentSub, NULL);
            slot->loadMode = loadMode;
            slot->reloadFlag = 0;
            slot->padding100 = 0;
            sprintf(name, "%s.fsys", fsysFindArchiveName(fileHandle));
            strcpy(slot->filename, name);
            if (lbl_80453FEC.activeSlot && slot != lbl_80453FEC.activeSlot) {
                slot->status = 0x1F4;
                return;
            }
            lbl_80453FEC.activeSlot = slot;
            lbl_80453FEC.currentSlot = slot;
            switch (loadMode) {
            case 0:
            case 2:
            case 3:
            case 7:
                slot->status = 1;
                break;
            case 1:
                slot->status = 0xC8;
                break;
            }
        }
        break;
    }
}
