/*
 * orre_sign: makes OrrePort's signing key once, and signs each release's update.txt (2026-11-23;
 * docs/port/updater-plan.md). snatcherLeo runs it; the programs only ever check (pc_update.c holds the public key).
 *
 * Why: an update is code that runs on testers' PCs, so each one must provably come from OrrePort. The release's
 * update.txt (version, the zip's size and SHA-512, what's new) is signed with Ed25519 (Monocypher, approved by snatcherLeo
 * 2026-11-23); both programs refuse an update.txt whose signature doesn't check against the public key built into them,
 * and the zip must match the SHA-512 inside it.
 *
 *   orre_sign keygen <secret key file>          once: makes the key pair, writes the SECRET key to the file (refuses
 *                                               to overwrite one) and prints the PUBLIC key to paste into pc_update.c
 *   orre_sign sign <secret key file> <update.txt> <update.sig>
 *   orre_sign check <public key hex> <update.txt> <update.sig>
 *
 * The secret key file never goes into any repo (it lives outside the checkout) and is backed up twice (snatcherLeo's call):
 * losing it means no tester's copy can ever be updated again; leaking it means anyone could sign an update.
 *
 * Platforms: the random seed comes from the system (Windows: BCryptGenRandom; Linux / macOS: /dev/urandom, likely);
 * the rest is plain C and Monocypher, the same everywhere.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "monocypher.h"
#include "monocypher-ed25519.h"

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
static int random_bytes(uint8_t* out, size_t n) {
    return BCryptGenRandom(NULL, out, (ULONG)n, BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0;
}
#else
static int random_bytes(uint8_t* out, size_t n) {
    FILE* f = fopen("/dev/urandom", "rb");
    const int ok = f != NULL && fread(out, 1, n, f) == n;
    if (f) fclose(f);
    return ok;
}
#endif

static unsigned char* read_file(const char* path, size_t* len) {
    FILE* f = fopen(path, "rb");
    unsigned char* buf;
    long n;
    if (f == NULL) return NULL;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = (unsigned char*)malloc(n > 0 ? (size_t)n + 1 : 1);
    if (buf != NULL && fread(buf, 1, (size_t)n, f) != (size_t)n) { free(buf); buf = NULL; }
    fclose(f);
    if (buf) { buf[n] = 0; *len = (size_t)n; }
    return buf;
}

static int from_hex(const char* hex, uint8_t* out, size_t n) {
    for (size_t i = 0; i < n; i++) {
        unsigned v;
        if (sscanf(hex + 2 * i, "%2x", &v) != 1) return 0;
        out[i] = (uint8_t)v;
    }
    return 1;
}

static void print_hex(FILE* f, const uint8_t* b, size_t n) {
    for (size_t i = 0; i < n; i++) fprintf(f, "%02x", b[i]);
}

int main(int argc, char** argv) {
    if (argc == 3 && strcmp(argv[1], "keygen") == 0) {
        uint8_t seed[32], secret[64], pub[32];
        FILE* f = fopen(argv[2], "rb");
        if (f != NULL) {
            fclose(f);
            fprintf(stderr, "%s already exists: a key is never overwritten (losing it would strand every copy)\n", argv[2]);
            return 1;
        }
        if (!random_bytes(seed, sizeof seed)) { fprintf(stderr, "no random numbers from the system\n"); return 1; }
        crypto_ed25519_key_pair(secret, pub, seed); /* (wipes the seed) */
        f = fopen(argv[2], "wb");
        if (f == NULL) { fprintf(stderr, "can't write %s\n", argv[2]); return 1; }
        print_hex(f, secret, 64);
        fputc('\n', f);
        fclose(f);
        crypto_wipe(secret, sizeof secret);
        printf("Secret key written to %s. Back it up twice, keep it out of every repo.\n", argv[2]);
        printf("Public key (goes into pc_update.c, kPublicKeyHex):\n");
        print_hex(stdout, pub, 32);
        printf("\n");
        return 0;
    }
    /* pubkey: the public key again, from the secret key file (snatcherLeo, 2026-11-23: the keygen line had scrolled away).
     * Monocypher's 64-byte Ed25519 secret key is the seed followed by the public key, so the public key is its last 32
     * bytes; nothing secret is printed */
    if (argc == 3 && strcmp(argv[1], "pubkey") == 0) {
        size_t klen = 0;
        unsigned char* key = read_file(argv[2], &klen);
        uint8_t secret[64];
        if (key == NULL || klen < 128 || !from_hex((const char*)key, secret, 64)) {
            fprintf(stderr, "can't read the secret key file %s\n", argv[2]);
            return 1;
        }
        printf("Public key (goes into pc_update.c, kPublicKeyHex):\n");
        print_hex(stdout, secret + 32, 32);
        printf("\n");
        crypto_wipe(secret, sizeof secret);
        crypto_wipe(key, klen);
        return 0;
    }
    if (argc == 5 && strcmp(argv[1], "sign") == 0) {
        size_t klen = 0, mlen = 0;
        unsigned char* key = read_file(argv[2], &klen);
        unsigned char* msg = read_file(argv[3], &mlen);
        uint8_t secret[64], sig[64];
        FILE* f;
        if (key == NULL || klen < 128 || !from_hex((const char*)key, secret, 64) || msg == NULL) {
            fprintf(stderr, "can't read the secret key or %s\n", argv[3]);
            return 1;
        }
        crypto_ed25519_sign(sig, secret, msg, mlen);
        crypto_wipe(secret, sizeof secret);
        crypto_wipe(key, klen);
        f = fopen(argv[4], "wb");
        if (f == NULL) { fprintf(stderr, "can't write %s\n", argv[4]); return 1; }
        print_hex(f, sig, 64);
        fputc('\n', f);
        fclose(f);
        printf("signed %s -> %s\n", argv[3], argv[4]);
        return 0;
    }
    if (argc == 5 && strcmp(argv[1], "check") == 0) {
        size_t slen = 0, mlen = 0;
        unsigned char* msg = read_file(argv[3], &mlen);
        unsigned char* sh = read_file(argv[4], &slen);
        uint8_t pub[32], sig[64];
        if (msg == NULL || sh == NULL || slen < 128 || strlen(argv[2]) != 64 || !from_hex(argv[2], pub, 32) ||
            !from_hex((const char*)sh, sig, 64)) {
            fprintf(stderr, "can't read the inputs\n");
            return 2;
        }
        if (crypto_ed25519_check(sig, pub, msg, mlen) != 0) { printf("BAD signature\n"); return 1; }
        printf("signature OK\n");
        return 0;
    }
    fprintf(stderr, "usage: orre_sign keygen <secret key file> | sign <secret key file> <update.txt> <update.sig> |"
                    " check <public key hex> <update.txt> <update.sig>\n");
    return 2;
}
