/* pc_save_cipher.c: the save slot's cipher both ways, and SHA-1 (pc_save_cipher.h says why and the layout).
 *
 * The SHA-1 and the decrypt moved here from pc_saves.c unchanged (2026-11-19); the encrypt is the inverse, written
 * from the game's rules as tools/local/colo/saveedit.py (encrypt_slot) already applies them, and proven by
 * test/save_cipher_test.c on real saves. Plain C, every OS. */
#include "pc_save_cipher.h"

#include <string.h>

typedef struct {
    uint32_t h[5];
    uint8_t buf[64];
} Sha1;

static uint32_t rol(uint32_t v, int s) { return (v << s) | (v >> (32 - s)); }

static void sha1_block(Sha1* c, const uint8_t* p) {
    uint32_t w[80], a, b, d, e, f, k, t, cc;
    for (int i = 0; i < 16; i++) w[i] = (uint32_t)p[i * 4] << 24 | (uint32_t)p[i * 4 + 1] << 16 | (uint32_t)p[i * 4 + 2] << 8 | p[i * 4 + 3];
    for (int i = 16; i < 80; i++) w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    a = c->h[0]; b = c->h[1]; cc = c->h[2]; d = c->h[3]; e = c->h[4];
    for (int i = 0; i < 80; i++) {
        if (i < 20) { f = (b & cc) | (~b & d); k = 0x5A827999; }
        else if (i < 40) { f = b ^ cc ^ d; k = 0x6ED9EBA1; }
        else if (i < 60) { f = (b & cc) | (b & d) | (cc & d); k = 0x8F1BBCDC; }
        else { f = b ^ cc ^ d; k = 0xCA62C1D6; }
        t = rol(a, 5) + f + e + k + w[i];
        e = d; d = cc; cc = rol(b, 30); b = a; a = t;
    }
    c->h[0] += a; c->h[1] += b; c->h[2] += cc; c->h[3] += d; c->h[4] += e;
}

void pc_save_sha1(const void* data_, size_t len, uint8_t out[20]) {
    const uint8_t* data = (const uint8_t*)data_;
    Sha1 c = {{0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0}, {0}};
    uint64_t bits = (uint64_t)len * 8;
    while (len >= 64) { sha1_block(&c, data); data += 64; len -= 64; }
    memcpy(c.buf, data, len);
    c.buf[len] = 0x80;
    if (len >= 56) { memset(c.buf + len + 1, 0, 63 - len); sha1_block(&c, c.buf); memset(c.buf, 0, 56); }
    else memset(c.buf + len + 1, 0, 55 - len);
    for (int i = 0; i < 8; i++) c.buf[56 + i] = (uint8_t)(bits >> (56 - 8 * i));
    sha1_block(&c, c.buf);
    for (int i = 0; i < 5; i++) for (int j = 0; j < 4; j++) out[i * 4 + j] = (uint8_t)(c.h[i] >> (24 - 8 * j));
}

static uint32_t be32(const uint8_t* p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }

static void put_be32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v;
}

int pc_save_slot_decrypt(const uint8_t* enc, uint8_t* plain) {
    uint8_t key[20], next[20], check[20], saved[4];
    memcpy(plain, enc, PC_SAVE_SLOT_SIZE);
    for (int i = 0; i < 20; i++) key[i] = (uint8_t)~enc[PC_SAVE_DIGEST_AT + i];
    for (size_t off = PC_SAVE_ENC_START; off < PC_SAVE_ENC_END; off += 20) {
        const size_t n = PC_SAVE_ENC_END - off < 20 ? PC_SAVE_ENC_END - off : 20;
        pc_save_sha1(enc + off, n, next); /* the key for the next step is the SHA-1 of this encrypted step */
        for (size_t i = 0; i < n; i++) plain[off + i] ^= key[i];
        memcpy(key, next, 20);
    }
    memcpy(saved, plain + 0x0C, 4);
    memset(plain + 0x0C, 0, 4); /* the digest covers the slot with the sum word zeroed (save-data.md) */
    pc_save_sha1(plain, PC_SAVE_ENC_END, check);
    memcpy(plain + 0x0C, saved, 4);
    return memcmp(check, plain + PC_SAVE_DIGEST_AT, 20) == 0;
}

void pc_save_slot_encrypt(const uint8_t* plain, uint8_t* enc) {
    uint8_t key[20], digest[20];
    uint32_t sum = 0;
    memcpy(enc, plain, PC_SAVE_SLOT_SIZE);
    memset(enc + 0x0C, 0, 4);
    pc_save_sha1(enc, PC_SAVE_ENC_END, digest);
    memcpy(enc + PC_SAVE_DIGEST_AT, digest, 20);
    for (int i = 0; i < 20; i++) key[i] = (uint8_t)~digest[i];
    for (size_t off = PC_SAVE_ENC_START; off < PC_SAVE_ENC_END; off += 20) {
        const size_t n = PC_SAVE_ENC_END - off < 20 ? PC_SAVE_ENC_END - off : 20;
        for (size_t i = 0; i < n; i++) enc[off + i] ^= key[i];
        pc_save_sha1(enc + off, n, key); /* the next key: the SHA-1 of the step just encrypted */
    }
    /* the sum word last: the first eight stored words (two of them already encrypted) must add up to 0 */
    for (int i = 0; i < 8; i++) if (i != 3) sum += be32(enc + i * 4);
    put_be32(enc + 0x0C, (uint32_t)(0u - sum));
}
