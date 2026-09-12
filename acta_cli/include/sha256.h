#ifndef SHA256_H
#define SHA256_H

#include <stddef.h>

/* ── Types used by the sha256.c implementation ─────────────────── */
typedef unsigned char BYTE;
typedef unsigned int   WORD;

typedef struct {
    WORD  state[8];
    WORD  bitlen;
    WORD  datalen;
    BYTE  data[64];
} SHA256_CTX;

/* ── SHA-256 (init / update / final, see sha256.c) ──────────────── */
void sha256_init(SHA256_CTX *ctx);
void sha256_update(SHA256_CTX *ctx, const BYTE data[], size_t len);
void sha256_final(SHA256_CTX *ctx, BYTE hash[32]);
void sha256_transform(SHA256_CTX *ctx, const BYTE data[]);

/* ── Thin convenience wrappers used by the CLI ──────────────────── */

/* SHA-256. Writes the 32 raw digest bytes to out (must be >= 32 bytes). */
void sha256(const void *data, size_t len, unsigned char out[32]);

/* SHA-256 as 64 lowercase hex chars + NUL.
 * Writes into out (must be >= 65 bytes) and returns out.
 * Same encoding the GUI uses (QCryptographicHash::toHex). */
char *sha256_hex(const void *data, size_t len, char out[65]);

#endif
