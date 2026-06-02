/*
 * MD5 hash algorithm implementation
 * Based on RFC 1321
 */

#ifndef MD5_H
#define MD5_H

#include <stddef.h>
#include <stdint.h>

#define MD5_BLOCK_SIZE 16  // MD5 outputs a 16 byte digest

typedef struct {
    uint32_t state[4];
    uint32_t count[2];
    uint8_t buffer[64];
} MD5_CTX;

void md5_init(MD5_CTX *ctx);
void md5_update(MD5_CTX *ctx, const uint8_t *data, size_t len);
void md5_final(MD5_CTX *ctx, uint8_t hash[MD5_BLOCK_SIZE]);

#endif // MD5_H