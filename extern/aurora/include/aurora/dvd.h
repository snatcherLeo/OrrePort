#ifndef AURORA_DVD_H
#define AURORA_DVD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <dolphin/types.h>

/**
 * Open a GC/Wii disc image for use by the DVD API.
 * Must be called before DVDInit().
 * Returns true on success, false on failure.
 */
bool aurora_dvd_open(const char* disc_path);

/**
 * Close the disc image and free all resources.
 */
void aurora_dvd_close(void);

/**
 * Completion callbacks of asynchronous DVD commands run on Aurora's DVD worker thread. On the console they ran as
 * the DVD interrupt, on the game's CPU. A dispatcher set here receives each such callback instead (callback, result,
 * command block) and decides where it runs, e.g. on the game thread; NULL (the default) calls it directly.
 * (OrrePort addition.)
 */
struct DVDCommandBlock;
typedef void (*AuroraDvdCallback)(s32 result, struct DVDCommandBlock* block);
typedef void (*AuroraDvdCallbackDispatcher)(AuroraDvdCallback callback, s32 result, struct DVDCommandBlock* block);
void aurora_dvd_set_callback_dispatcher(AuroraDvdCallbackDispatcher dispatcher);

/**
 * Status polls (DVDGetCommandBlockStatus, DVDGetDriveStatus, DVDGetFileInfoStatus) call this hook first, if set.
 * On the GameCube a DVD interrupt can arrive while the game spins on a status poll; with dispatched callbacks the
 * host uses the hook to run them there (OrrePort addition).
 */
void aurora_dvd_set_poll_hook(void (*hook)(void));

/**
 * DVDGetCommandBlockStatus passes the block's state through this hook, if set, and returns what it says (OrrePort,
 * 2026-10-19): the host keeps a read "busy" until it hands the game the read's completion (console-like timing,
 * deterministic replays), as on the GameCube, where the state changes in the same interrupt that runs the callback.
 */
void aurora_dvd_set_status_hook(s32 (*hook)(const struct DVDCommandBlock* block, s32 state));

/**
 * OVERLAY FILES!
 *
 * Overlay files allow you to replace and add ("overlay") files that are present in the loaded DVD.
 * The way this works is pretty simple: you provide some callbacks and a list of files.
 * When an overlaid file gets read, your callbacks get called instead of pulling from the underlying DVD.
 *
 * Original disc EntryNums are not touched by the overlay system. New files and directories
 * are assigned stable EntryNums by Aurora.
 */

/**
 * \brief A single file to be overlaid over the DVD files.
 *
 * You do not need to concern yourself with providing entries for directories. They are automatically merged
 * and created where necessary.
 */
typedef struct AuroraOverlayFile {
  /**
   * \brief Absolute file path of this file.
   *
   * Must be in the form "/foo/bar/baz.txt", note the leading slash.
   */
  const char* fileName;

  /**
   * \brief Userdata pointer that will be passed to the callback when this file is opened.
   */
  void* userData;

  /**
   * \brief Size of this file, in bytes.
   *
   * While this is of type size_t, file sizes larger than u32 are not currently supported.
   */
  size_t size;

} AuroraOverlayFile;

/**
 * \brief Callbacks to implement overlay files.
 *
 * Callbacks may be ran from any thread at any time. Make sure they're thread safe!
 */
typedef struct AuroraOverlayCallbacks {
  /**
   * Called when a new file has been opened.
   *
   * Returns an opaque handle that will be passed to the remaining callbacks. Receives the userdata specified in
   * the AuroraOverlayFile.
   */
  void* (*open)(void* userdata);

  /**
   * Close a file handle previously returned from the open callback.
   */
  void (*close)(void* handle);

  /**
   * Read data from a file handle.
   *
   * Returns the amount of data read, or -1 on error.
   */
  int64_t (*read)(void* handle, uint8_t* buf, size_t len);

  /**
   * Seek to a position in a file handle.
   *
   * Returns the resulting position, or -1 on error.
   */
  int64_t (*seek)(void* handle, int64_t offset, int32_t whence);
} AuroraOverlayCallbacks;

/**
 * \brief Specify callbacks for overlaid files.
 */
void aurora_dvd_overlay_callbacks(const AuroraOverlayCallbacks* callbacks);

/**
 * \brief Specify a set of overlay files to be used by the DVD layer.
 *
 * Calling this function immediately applies the new files and rebuilds the FST, replacing any
 * previously specified set. It may be called again at runtime; FST reads and overlay opens are
 * serialized against the rebuild, and previously assigned EntryNums are stable per path. Files
 * removed by a re-registration fail to open from then on (or revert to the underlying DVD file);
 * already-open handles are unaffected. Only call from one thread at a time.
 *
 * This function must be called *after* aurora_dvd_overlay_callbacks.
 *
 * @param files Array of AuroraOverlayFiles, one for every file being overlaid.
 * @param nFiles Amount of files in the array.
 * @param outEntryNums Optional output array receiving one EntryNum per input file. Unaccepted files receive -1.
 */
void aurora_dvd_overlay_files(const AuroraOverlayFile* files, size_t nFiles, s32* outEntryNums);

/**
 * \brief Gets the amount of FST entries present on the loaded game disc.
 *
 * This does not take overlay files into account.
 */
s32 aurora_dvd_base_entry_count();

/**
 * \brief The handle the overlay callbacks' open returned for this open file, or NULL when it is a disc file (OrrePort
 * addition, 2026-11-19). An overlay that stands in for a disc file also gets that file's disc offset in startAddr (as
 * DVDFastOpen fills it for disc files), so a host can time its reads as the original's.
 */
struct DVDFileInfo;
void* aurora_dvd_overlay_handle(const struct DVDFileInfo* fileInfo);

/**
 * \brief The open disc's main.dol (OrrePort addition): its bytes and size, valid until the disc is closed. Returns
 * false when no disc is open or it has none.
 */
bool aurora_dvd_main_dol(const u8** data, size_t* size);

/**
 * \brief Reads a whole base-disc file (by its FST entry number) on the calling thread, outside the DVD worker, and
 * hands it to consume in chunks of at most chunk bytes (OrrePort addition, 2026-11-02: the disc check before the game
 * starts). Returns the file's length, or -1 when the entry is no readable file.
 */
s64 aurora_dvd_read_entry(s32 entrynum, size_t chunk, void (*consume)(void* user, const u8* data, size_t size),
                          void* user);

#ifdef __cplusplus
}
#endif

#endif
