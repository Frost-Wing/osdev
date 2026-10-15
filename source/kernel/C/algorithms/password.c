#include <algorithms/password.h>

#include <memory.h>

typedef struct {
    uint32_t state[8];
    uint64_t bit_count;
    uint8_t block[64];
    size_t block_len;
} sha256_ctx_t;

static const uint32_t sha256_k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

static uint32_t rotate_right(uint32_t value, unsigned int bits) {
    return (value >> bits) | (value << (32 - bits));
}

static void sha256_transform(sha256_ctx_t *ctx, const uint8_t block[64]) {
    uint32_t w[64];
    for (size_t i = 0; i < 16; ++i) {
        w[i] = ((uint32_t)block[i * 4] << 24) |
               ((uint32_t)block[i * 4 + 1] << 16) |
               ((uint32_t)block[i * 4 + 2] << 8) |
               (uint32_t)block[i * 4 + 3];
    }
    for (size_t i = 16; i < 64; ++i) {
        uint32_t s0 = rotate_right(w[i - 15], 7) ^
                      rotate_right(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = rotate_right(w[i - 2], 17) ^
                      rotate_right(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    uint32_t a = ctx->state[0], b = ctx->state[1];
    uint32_t c = ctx->state[2], d = ctx->state[3];
    uint32_t e = ctx->state[4], f = ctx->state[5];
    uint32_t g = ctx->state[6], h = ctx->state[7];
    for (size_t i = 0; i < 64; ++i) {
        uint32_t s1 = rotate_right(e, 6) ^ rotate_right(e, 11) ^
                      rotate_right(e, 25);
        uint32_t choice = (e & f) ^ (~e & g);
        uint32_t t1 = h + s1 + choice + sha256_k[i] + w[i];
        uint32_t s0 = rotate_right(a, 2) ^ rotate_right(a, 13) ^
                      rotate_right(a, 22);
        uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = s0 + majority;
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

static void sha256_init(sha256_ctx_t *ctx) {
    static const uint32_t initial[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    memcpy(ctx->state, initial, sizeof(initial));
    ctx->bit_count = 0;
    ctx->block_len = 0;
}

static void sha256_update(sha256_ctx_t *ctx, const uint8_t *data, size_t len) {
    while (len) {
        size_t take = sizeof(ctx->block) - ctx->block_len;
        if (take > len)
            take = len;
        memcpy(ctx->block + ctx->block_len, data, take);
        ctx->block_len += take;
        data += take;
        len -= take;
        if (ctx->block_len == sizeof(ctx->block)) {
            sha256_transform(ctx, ctx->block);
            ctx->bit_count += sizeof(ctx->block) * 8;
            ctx->block_len = 0;
        }
    }
}

static void sha256_final(sha256_ctx_t *ctx, uint8_t digest[32]) {
    uint64_t total_bits = ctx->bit_count + ctx->block_len * 8;
    ctx->block[ctx->block_len++] = 0x80;
    if (ctx->block_len > 56) {
        memset(ctx->block + ctx->block_len, 0, 64 - ctx->block_len);
        sha256_transform(ctx, ctx->block);
        ctx->block_len = 0;
    }
    memset(ctx->block + ctx->block_len, 0, 56 - ctx->block_len);
    for (size_t i = 0; i < 8; ++i)
        ctx->block[63 - i] = (uint8_t)(total_bits >> (i * 8));
    sha256_transform(ctx, ctx->block);

    for (size_t i = 0; i < 8; ++i) {
        digest[i * 4] = (uint8_t)(ctx->state[i] >> 24);
        digest[i * 4 + 1] = (uint8_t)(ctx->state[i] >> 16);
        digest[i * 4 + 2] = (uint8_t)(ctx->state[i] >> 8);
        digest[i * 4 + 3] = (uint8_t)ctx->state[i];
    }
    memset(ctx, 0, sizeof(*ctx));
}

static void hmac_sha256(const uint8_t *key, size_t key_len,
    const uint8_t *data, size_t data_len, uint8_t out[32]) {
    uint8_t key_block[64] = {0};
    if (key_len > sizeof(key_block)) {
        sha256_ctx_t key_ctx;
        sha256_init(&key_ctx);
        sha256_update(&key_ctx, key, key_len);
        sha256_final(&key_ctx, key_block);
        key_len = 32;
    } else {
        memcpy(key_block, key, key_len);
    }

    uint8_t inner_pad[64], outer_pad[64], inner_digest[32];
    for (size_t i = 0; i < 64; ++i) {
        inner_pad[i] = key_block[i] ^ 0x36;
        outer_pad[i] = key_block[i] ^ 0x5c;
    }

    sha256_ctx_t ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, inner_pad, sizeof(inner_pad));
    sha256_update(&ctx, data, data_len);
    sha256_final(&ctx, inner_digest);

    sha256_init(&ctx);
    sha256_update(&ctx, outer_pad, sizeof(outer_pad));
    sha256_update(&ctx, inner_digest, sizeof(inner_digest));
    sha256_final(&ctx, out);

    memset(key_block, 0, sizeof(key_block));
    memset(inner_pad, 0, sizeof(inner_pad));
    memset(outer_pad, 0, sizeof(outer_pad));
    memset(inner_digest, 0, sizeof(inner_digest));
}

void password_derive_key(const uint8_t *password, size_t password_len,
    const uint8_t salt[16], uint32_t iterations, uint8_t out[32]) {
    if (!password || !salt || !out || iterations == 0) {
        if (out)
            memset(out, 0, 32);
        return;
    }

    uint8_t first_input[20], u[32], result[32];
    memcpy(first_input, salt, 16);
    first_input[16] = 0;
    first_input[17] = 0;
    first_input[18] = 0;
    first_input[19] = 1;
    hmac_sha256(password, password_len, first_input, sizeof(first_input), u);
    memcpy(result, u, sizeof(result));

    for (uint32_t i = 1; i < iterations; ++i) {
        hmac_sha256(password, password_len, u, sizeof(u), u);
        for (size_t j = 0; j < sizeof(result); ++j)
            result[j] ^= u[j];
    }

    memcpy(out, result, sizeof(result));
    memset(first_input, 0, sizeof(first_input));
    memset(u, 0, sizeof(u));
    memset(result, 0, sizeof(result));
}
