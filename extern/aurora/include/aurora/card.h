#ifndef AURORA_CARD_H
#define AURORA_CARD_H

#include <stddef.h>
#include <stdbool.h>

#include <dolphin/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum AuroraCardType {
  AURORA_CARD_UNAVAILABLE = 0,
  AURORA_CARD_GCI_DIRECTORY,
  AURORA_CARD_RAW_IMAGE,
} AuroraCardType;

/**
 * Returns the type of CARD for a channel after CARDInit().
 */
AuroraCardType aurora_card_get_type(s32 channel);

/**
 * Completion callbacks of the CARD...Async functions. On the GameCube they run later, from the EXI interrupt,
 * after the call has returned; Aurora finishes the work at once. A dispatcher set here receives each callback
 * (callback, channel, result) instead and decides when it runs, e.g. posted to the game thread (OrrePort: the game
 * marks a request pending after the call returns, so an immediate callback was overwritten and the boot waited
 * forever on the memory card). NULL (the default) calls it directly.
 */
typedef void (*AuroraCardCallback)(s32 chan, s32 result);
typedef void (*AuroraCardCallbackDispatcher)(AuroraCardCallback callback, s32 chan, s32 result);
void aurora_card_set_callback_dispatcher(AuroraCardCallbackDispatcher dispatcher);

/**
 * Console-like card timing (OrrePort, 2026-10-19). Aurora finishes each card operation at once; on the GameCube an
 * operation keeps the channel busy (CARDGetResultCode = CARD_RESULT_BUSY) while the card transfers, and games that
 * poll the result code wait for it frame by frame. The op hook is told about each operation as it starts (op names:
 * "mount", "check", "format", "create", "delete", "read", "write", "setstatus", "setattributes", "rename"; length in
 * bytes for read / write, else 0); while the busy query says a channel is busy, CARDGetResultCode returns
 * CARD_RESULT_BUSY. NULL (the default) keeps Aurora's instant behaviour.
 */
typedef void (*AuroraCardOpHook)(s32 chan, const char* op, s32 length);
typedef bool (*AuroraCardBusyQuery)(s32 chan);
void aurora_card_set_timing_hooks(AuroraCardOpHook opHook, AuroraCardBusyQuery busyQuery);

/**
 * Gets the UTF-8 path to a channel's GCI directory or raw card image for a game.
 *
 * When the requested type is mounted, this returns its active path. Otherwise it resolves the path
 * from Aurora's configured user directory. gameName is the four-character game ID and determines
 * the card region.
 *
 * Returns the required buffer size, including the null terminator. Passing a null buffer or a
 * capacity of zero queries the required size. Returns zero for invalid arguments. An undersized
 * non-null buffer is set to an empty string.
 */
size_t aurora_card_get_path(const char* gameName, AuroraCardType type, s32 channel, char* buffer, size_t capacity);

/** Closes and reopens a mounted channel so external filesystem changes are discovered. */
bool aurora_card_remount(s32 channel);

/** OrrePort addition: re-opens a channel on another GCI folder after CARDInit (the in-game save switch). Refused
 *  (false, the old folder kept) while a file is open on the channel or the new folder can't be opened. */
bool aurora_card_switch_folder(const char* path, s32 channel);

/** Calls visit for each filename matching game and maker. Returns false if the image cannot be read. */
bool aurora_card_raw_list(const char* imagePath, const char* game, const char* maker,
                          void (*visit)(const char* fileName, void* userData), void* userData);

/** Extracts one CARD file from a raw image as GCI bytes. */
size_t aurora_card_raw_extract(const char* imagePath, const char* game, const char* maker, const char* fileName,
                               void* gciOut, size_t capacity);

/** Inserts GCI bytes into a raw image, optionally replacing a matching CARD file. */
bool aurora_card_raw_insert(const char* imagePath, const void* gci, size_t size, bool replace);

/** Deletes one CARD file from a raw image. */
bool aurora_card_raw_delete(const char* imagePath, const char* game, const char* maker, const char* fileName);

/** Builds a GCI filename from its game, maker, and internal CARD filename. */
size_t aurora_card_gci_filename(const char* game, const char* maker, const char* fileName, char* buffer,
                                size_t capacity);

#ifdef __cplusplus
}
#endif

#endif
