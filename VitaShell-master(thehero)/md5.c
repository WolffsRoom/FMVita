/*
 * MD5 hash algorithm implementation
 * Based on RFC 1321
 */

#include "md5.h"
#include <string.h>

// MD5 constants
static const uint32_t K[64] = {
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
    0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
    0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
    0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
    0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
    0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
    0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
    0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
    0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
    0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
    0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
    0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
    0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
    0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
};

static const int S[64] = {
    7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
    5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
    4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
    6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21
};

#define F(x,y,z) ((x & y) | (~x & z))
#define G(x,y,z) ((x & z) | (y & ~z))
#define H(x,y,z) (x ^ y ^ z)
#define I(x,y,z) (y ^ (x | ~z))
#define ROTATE_LEFT(x, n) ((x << n) | (x >> (32-n)))

static void md5_transform(MD5_CTX *ctx, const uint8_t data[64]) {
    uint32_t a, b, c, d, m[16], i, t;

    // Copy chunk into first 16 words of the message schedule array
    for (i = 0; i < 16; ++i)
        m[i] = (data[i * 4]) + (data[i * 4 + 1] << 8) + (data[i * 4 + 2] << 16) + (data[i * 4 + 3] << 24);

    // Initialize hash value for this chunk
    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];

    // Main loop
    for (i = 0; i < 64; ++i) {
        if (i < 16) {
            t = F(b, c, d) + a + K[i] + m[i];
        } else if (i < 32) {
            t = G(b, c, d) + a + K[i] + m[(5 * i + 1) % 16];
        } else if (i < 48) {
            t = H(b, c, d) + a + K[i] + m[(3 * i + 5) % 16];
        } else {
            t = I(b, c, d) + a + K[i] + m[(7 * i) % 16];
        }

        a = d;
        d = c;
        c = b;
        b = b + ROTATE_LEFT(t, S[i]);
    }

    // Add this chunk's hash to result so far
    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
}

void md5_init(MD5_CTX *ctx) {
    ctx->count[0] = 0;
    ctx->count[1] = 0;
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xefcdab89;
    ctx->state[2] = 0x98badcfe;
    ctx->state[3] = 0x10325476;
}

void md5_update(MD5_CTX *ctx, const uint8_t *data, size_t len) {
    uint32_t i;

    // Update number of bits
    if ((ctx->count[0] += len << 3) < (len << 3))
        ctx->count[1]++;
    ctx->count[1] += len >> 29;

    // Number of bytes we have in the buffer
    i = (ctx->count[0] >> 3) & 0x3F;

    // Copy data
    if (i + len >= 64) {
        memcpy(ctx->buffer + i, data, 64 - i);
        md5_transform(ctx, ctx->buffer);
        for (i = 64 - i; i + 63 < len; i += 64)
            md5_transform(ctx, &data[i]);
        i = 0;
    }
    memcpy(ctx->buffer + i, &data[len - (len % 64)], len % 64);
}

void md5_final(MD5_CTX *ctx, uint8_t hash[MD5_BLOCK_SIZE]) {
    uint32_t i;
    uint8_t bits[8];
    uint32_t index, padLen;

    // Save number of bits
    for (i = 0; i < 8; ++i)
        bits[i] = (ctx->count[i >> 2] >> ((i & 3) << 3)) & 0xff;

    // Pad to 56 mod 64
    index = (ctx->count[0] >> 3) & 0x3f;
    padLen = (index < 56) ? (56 - index) : (120 - index);
    
    uint8_t padding[64] = {0x80};  // First bit set, rest zeros
    md5_update(ctx, padding, padLen);

    // Append length
    md5_update(ctx, bits, 8);

    // Store hash in output
    for (i = 0; i < MD5_BLOCK_SIZE; ++i)
        hash[i] = (ctx->state[i >> 2] >> ((i & 3) << 3)) & 0xff;
}