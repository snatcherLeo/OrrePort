/* pc_save_cipher.h: the save file's slot cipher, both ways, and the SHA-1 it is built on.
 *
 * Why its own file (2026-11-19, Phase 3 step 3.5): reading a save only ever needed the decrypt (pc_saves.c).
 * "Make a GameCube copy..." for another region has to change bytes inside a slot (the region stamp, PAL's language
 * bytes, species names), so it needs the way back too: re-seal and re-encrypt, exactly as the game does when it saves.
 * Both directions live here, next to each other, so they can be tested as a pair (test/save_cipher_test.c: every slot
 * of real saves decrypted and encrypted again must give the same bytes) and used by pc_saves.c and pc_gcopy.c alike.
 *
 * The slot (0x1E000 bytes, three per save file after the 0x6000-byte header; docs/engine/save-data.md, verified
 * against real saves from all three regions):
 *   +0x00..+0x17  stored as is (+0x04 the save counter, +0x08 the region stamp, +0x0C the "sum word")
 *   +0x18..0x1DFD7 encrypted in 20-byte steps: the first key is the stored digest inverted, each next key the SHA-1 of
 *                 the previous encrypted step
 *   +0x1DFEC      the digest: SHA-1 over +0x00..0x1DFD7 of the plain slot, with the sum word zeroed
 *   the sum word  set last, so the first eight stored big-endian words add up to 0 (mod 2^32)
 *
 * Plain C99 on bytes, no platform calls: the same on Windows, Linux and macOS (only built and tested on Windows yet). */
#ifndef PC_SAVE_CIPHER_H
#define PC_SAVE_CIPHER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    PC_SAVE_SLOT_SIZE = 0x1E000,
    PC_SAVE_ENC_START = 0x18,
    PC_SAVE_ENC_END = 0x1DFD8,
    PC_SAVE_DIGEST_AT = 0x1DFEC
};

/* SHA-1 (FIPS 180-1) of len bytes */
void pc_save_sha1(const void* data, size_t len, uint8_t out[20]);

/* Decrypts one stored slot (PC_SAVE_SLOT_SIZE bytes) into plain. Returns 1 when its digest checks out (a valid slot),
 * 0 otherwise (plain is still written: an empty or damaged slot decrypts to noise). enc and plain may not overlap. */
int pc_save_slot_decrypt(const uint8_t* enc, uint8_t* plain);

/* Encrypts a plain slot into enc, the game's way: the sum word zeroed, the digest over the plain bytes stored, the key
 * stream applied, then the sum word set so the first eight stored words add up to 0. Whatever plain holds in the sum
 * word is ignored. enc and plain may not overlap. */
void pc_save_slot_encrypt(const uint8_t* plain, uint8_t* enc);

#ifdef __cplusplus
}
#endif

#endif /* PC_SAVE_CIPHER_H */
