/**
 * @file syn_ed25519.c
 * @brief Pure C99 Ed25519 Key Generation, Signing & Verification (RFC 8032).
 *
 * Implements Ed25519 with field arithmetic modulo 2^255 - 19, SHA-512,
 * point arithmetic on Edwards25519, and scalar reduction modulo L.
 */

#include "syn_ed25519.h"

#include <string.h>

/** @cond INTERNAL */

/* ── SHA-512 (FIPS 180-4 / RFC 6234) ──────────────────────────────────────── */

#define ROR64(x, n) (((x) >> (n)) | ((x) << (64 - (n))))
#define Ch(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define Maj(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define Sigma0(x) (ROR64(x, 28) ^ ROR64(x, 34) ^ ROR64(x, 39))
#define Sigma1(x) (ROR64(x, 14) ^ ROR64(x, 18) ^ ROR64(x, 41))
#define sigma0(x) (ROR64(x, 1) ^ ROR64(x, 8) ^ ((x) >> 7))
#define sigma1(x) (ROR64(x, 19) ^ ROR64(x, 61) ^ ((x) >> 6))

static const uint64_t K512[80] = {
    0x428A2F98D728AE22ULL, 0x7137449123EF65CDULL, 0xB5C0FBCFEC4D3B2FULL, 0xE9B5DBA58189DBBCULL,
    0x3956C25BF348B538ULL, 0x59F111F1B605D019ULL, 0x923F82A4AF194F9BULL, 0xAB1C5ED5DA6D8118ULL,
    0xD807AA98A3030242ULL, 0x12835B0145706FBEULL, 0x243185BE4EE4B28CULL, 0x550C7DC3D5FFB4E2ULL,
    0x72BE5D74F27B896FULL, 0x80DEB1FE3B1696B1ULL, 0x9BDC06A725C71235ULL, 0xC19BF174CF692694ULL,
    0xE49B69C19EF14AD2ULL, 0xEFBE4786384F25E3ULL, 0x0FC19DC68B8CD5B5ULL, 0x240CA1CC77AC9C65ULL,
    0x2DE92C6F592B0275ULL, 0x4A7484AA6EA6E483ULL, 0x5CB0A9DCBD41FBD4ULL, 0x76F988DA831153B5ULL,
    0x983E5152EE66DFABULL, 0xA831C66D2DB43210ULL, 0xB00327C898FB213FULL, 0xBF597FC7BEEF0EE4ULL,
    0xC6E00BF33DA88FC2ULL, 0xD5A79147930AA725ULL, 0x06CA6351E003826FULL, 0x142929670A0E6E70ULL,
    0x27B70A8546D22FFCULL, 0x2E1B21385C26C926ULL, 0x4D2C6DFC5AC42AEDULL, 0x53380D139D95B3DFULL,
    0x650A73548BAF63DEULL, 0x766A0ABB3C77B2A8ULL, 0x81C2C92E47EDAEE6ULL, 0x92722C851482353BULL,
    0xA2BFE8A14CF10364ULL, 0xA81A664BBC423001ULL, 0xC24B8B70D0F89791ULL, 0xC76C51A30654BE30ULL,
    0xD192E819D6EF5218ULL, 0xD69906245565A910ULL, 0xF40E35855771202AULL, 0x106AA07032BBD1B8ULL,
    0x19A4C116B8D2D0C8ULL, 0x1E376C085141AB53ULL, 0x2748774CDF8EEB99ULL, 0x34B0BCB5E19B48A8ULL,
    0x391C0CB3C5C95A63ULL, 0x4ED8AA4AE3418ACBULL, 0x5B9CCA4F7763E373ULL, 0x682E6FF3D6B2B8A3ULL,
    0x748F82EE5DEFB2FCULL, 0x78A5636F43172F60ULL, 0x84C87814A1F0AB72ULL, 0x8CC702081A6439ECULL,
    0x90BEFFFA23631E28ULL, 0xA4506CEBDE82BDE9ULL, 0xBEF9A3F7B2C67915ULL, 0xC67178F2E372532BULL,
    0xCA273ECEEA26619CULL, 0xD186B8C721C0C207ULL, 0xEADA7DD6CDE0EB1EULL, 0xF57D4F7FEE6ED178ULL,
    0x06F067AA72176FBAULL, 0x0A637DC5A2C898A6ULL, 0x113F9804BEF90DAEULL, 0x1B710B35131C471BULL,
    0x28DB77F523047D84ULL, 0x32CAAB7B40C72493ULL, 0x3C9EBE0A15C9BEBCULL, 0x431D67C49C100D4CULL,
    0x4CC5D4BECB3E42B6ULL, 0x597F299CFC657E2AULL, 0x5FCB6FAB3AD6FAECULL, 0x6C44198C4A475817ULL};

static uint64_t dl64(const uint8_t *x)
{
    uint64_t u = 0;
    for (int i = 0; i < 8; i++) {
        u = (u << 8) | (uint64_t)x[i];
    }
    return u;
}

static void ts64(uint8_t *x, uint64_t u)
{
    for (int i = 7; i >= 0; --i) {
        x[i] = (uint8_t)(u & 0xFFU);
        u >>= 8;
    }
}

static void crypto_hashblocks(uint8_t *x, const uint8_t *m, uint64_t n)
{
    uint64_t z[8], b[8], a[8], w[16], t;
    int i, j;

    for (i = 0; i < 8; i++) {
        z[i] = a[i] = dl64(x + 8 * i);
    }

    while (n >= 128U) {
        for (i = 0; i < 16; i++) {
            w[i] = dl64(m + 8 * i);
        }

        for (i = 0; i < 80; i++) {
            for (j = 0; j < 8; j++) {
                b[j] = a[j];
            }
            t = a[7] + Sigma1(a[4]) + Ch(a[4], a[5], a[6]) + K512[i] + w[i % 16];
            b[7] = t + Sigma0(a[0]) + Maj(a[0], a[1], a[2]);
            b[3] += t;
            for (j = 0; j < 8; j++) {
                a[(j + 1) % 8] = b[j];
            }
            if ((i % 16) == 15) {
                for (j = 0; j < 16; j++) {
                    w[j] += w[(j + 9) % 16] + sigma0(w[(j + 1) % 16]) + sigma1(w[(j + 14) % 16]);
                }
            }
        }

        for (i = 0; i < 8; i++) {
            a[i] += z[i];
            z[i] = a[i];
        }

        m += 128;
        n -= 128U;
    }

    for (i = 0; i < 8; i++) {
        ts64(x + 8 * i, z[i]);
    }
}

static const uint8_t sha512_iv[64] = {
    0x6a, 0x09, 0xe6, 0x67, 0xf3, 0xbc, 0xc9, 0x08, 0xbb, 0x67, 0xae, 0x85, 0x84, 0xca, 0xa7, 0x3b,
    0x3c, 0x6e, 0xf3, 0x72, 0xfe, 0x94, 0xf8, 0x2b, 0xa5, 0x4f, 0xf5, 0x3a, 0x5f, 0x1d, 0x36, 0xf1,
    0x51, 0x0e, 0x52, 0x7f, 0xad, 0xe6, 0x82, 0xd1, 0x9b, 0x05, 0x68, 0x8c, 0x2b, 0x3e, 0x6c, 0x1f,
    0x1f, 0x83, 0xd9, 0xab, 0xfb, 0x41, 0xbd, 0x6b, 0x5b, 0xe0, 0xcd, 0x19, 0x13, 0x7e, 0x21, 0x79};

typedef struct {
    uint64_t state[8];
    uint64_t count;
    uint8_t buffer[128];
} SYN_SHA512_Ctx;

static void sha512_init(SYN_SHA512_Ctx *ctx)
{
    ctx->count = 0;
    for (int i = 0; i < 8; i++) {
        ctx->state[i] = dl64(sha512_iv + 8 * i);
    }
}

static void sha512_update(SYN_SHA512_Ctx *ctx, const uint8_t *data, size_t len)
{
    size_t left = (size_t)(ctx->count % 128U);
    ctx->count += (uint64_t)len;

    if (left > 0U) {
        size_t to_copy = (len < (128U - left)) ? len : (128U - left);
        (void)memcpy(ctx->buffer + left, data, to_copy);
        data += to_copy;
        len -= to_copy;
        left += to_copy;
        if (left == 128U) {
            uint8_t block[64];
            for (int i = 0; i < 8; i++) {
                ts64(block + 8 * i, ctx->state[i]);
            }
            crypto_hashblocks(block, ctx->buffer, 128U);
            for (int i = 0; i < 8; i++) {
                ctx->state[i] = dl64(block + 8 * i);
            }
            left = 0;
        }
    }

    while (len >= 128U) {
        uint8_t block[64];
        for (int i = 0; i < 8; i++) {
            ts64(block + 8 * i, ctx->state[i]);
        }
        crypto_hashblocks(block, data, 128U);
        for (int i = 0; i < 8; i++) {
            ctx->state[i] = dl64(block + 8 * i);
        }
        data += 128U;
        len -= 128U;
    }

    if (len > 0U) {
        (void)memcpy(ctx->buffer + left, data, len);
    }
}

static void sha512_final(SYN_SHA512_Ctx *ctx, uint8_t digest[64])
{
    uint8_t pad[256];
    (void)memset(pad, 0, sizeof(pad));
    size_t left = (size_t)(ctx->count % 128U);
    (void)memcpy(pad, ctx->buffer, left);
    pad[left] = 0x80;

    size_t pad_len = 256U - 128U * (size_t)(left < 112U);
    pad[pad_len - 9] = (uint8_t)(ctx->count >> 61);
    ts64(pad + pad_len - 8, ctx->count << 3);

    uint8_t block[64];
    for (int i = 0; i < 8; i++) {
        ts64(block + 8 * i, ctx->state[i]);
    }
    crypto_hashblocks(block, pad, (uint64_t)pad_len);
    for (int i = 0; i < 8; i++) {
        ts64(digest + 8 * i, dl64(block + 8 * i));
    }
}

/* ── Curve25519 Field Arithmetic (GF(2^255 - 19), 16 × 16-bit limbs) ──────── */

typedef int64_t gf[16];

static const gf gf0 = {0};
static const gf gf1 = {1};

static const gf D = {0x78a3, 0x1359, 0x4dca, 0x75eb, 0xd8ab, 0x4141, 0x0a4d, 0x0070,
                     0xe898, 0x7779, 0x4079, 0x8cc7, 0xfe73, 0x2b6f, 0x6cee, 0x5203};

static const gf D2 = {0xf159, 0x26b2, 0x9b94, 0xebd6, 0xb156, 0x8283, 0x149a, 0x00e0,
                      0xd130, 0xeef3, 0x80f2, 0x198e, 0xfce7, 0x56df, 0xd9dc, 0x2406};

static const gf X = {0xd51a, 0x8f25, 0x2d60, 0xc956, 0xa7b2, 0x9525, 0xc760, 0x692c,
                     0xdc5c, 0xfdd6, 0xe231, 0xc0a4, 0x53fe, 0xcd6e, 0x36d3, 0x2169};

static const gf Y = {0x6658, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666,
                     0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666, 0x6666};

static const gf I = {0xa0b0, 0x4a0e, 0x1b27, 0xc4ee, 0xe478, 0xad2f, 0x1806, 0x2f43,
                     0xd7a7, 0x3dfb, 0x0099, 0x2b4d, 0xdf0b, 0x4fc1, 0x2480, 0x2b83};

static void car25519(gf o)
{
    int i;
    for (i = 0; i < 16; i++) {
        o[i] += (1LL << 16);
        int64_t c = o[i] >> 16;
        o[(i + 1) * (i < 15)] += c - 1 + 37 * (c - 1) * (i == 15);
        o[i] -= c * 65536LL;
    }
}

static void sel25519(gf p, gf q, int b)
{
    int64_t t, c = ~(int64_t)(b - 1);
    for (int i = 0; i < 16; i++) {
        t = c & (p[i] ^ q[i]);
        p[i] ^= t;
        q[i] ^= t;
    }
}

static void pack25519(uint8_t o[32], const gf n)
{
    int i, j;
    int64_t b;
    gf m, t;
    for (i = 0; i < 16; i++) {
        t[i] = n[i];
    }
    car25519(t);
    car25519(t);
    car25519(t);
    for (j = 0; j < 2; j++) {
        m[0] = t[0] - 0xFFED;
        for (i = 1; i < 15; i++) {
            m[i] = t[i] - 0xFFFF - ((m[i - 1] >> 16) & 1);
            m[i - 1] &= 0xFFFF;
        }
        m[15] = t[15] - 0x7FFF - ((m[14] >> 16) & 1);
        b = (m[15] >> 16) & 1;
        m[14] &= 0xFFFF;
        sel25519(t, m, (int)(1 - b));
    }
    for (i = 0; i < 16; i++) {
        uint16_t v = (uint16_t)t[i];
        o[2 * i] = (uint8_t)(v & 0xFFU);
        o[2 * i + 1] = (uint8_t)((v >> 8) & 0xFFU);
    }
}

static int crypto_verify_32(const uint8_t *x, const uint8_t *y)
{
    uint32_t d = 0;
    for (int i = 0; i < 32; i++) {
        d |= (uint32_t)(x[i] ^ y[i]);
    }
    return (int)((1U & ((d - 1U) >> 8)) - 1U);
}

static int neq25519(const gf a, const gf b)
{
    uint8_t c[32], d[32];
    pack25519(c, a);
    pack25519(d, b);
    return crypto_verify_32(c, d);
}

static uint8_t par25519(const gf a)
{
    uint8_t d[32];
    pack25519(d, a);
    return (uint8_t)(d[0] & 1U);
}

static void unpack25519(gf o, const uint8_t n[32])
{
    for (int i = 0; i < 16; i++) {
        uint16_t v = (uint16_t)n[2 * i] | ((uint16_t)n[2 * i + 1] << 8);
        o[i] = (int64_t)v;
    }
    o[15] &= 0x7FFF;
}

static void A(gf o, const gf a, const gf b)
{
    for (int i = 0; i < 16; i++) {
        o[i] = a[i] + b[i];
    }
}

static void Z(gf o, const gf a, const gf b)
{
    for (int i = 0; i < 16; i++) {
        o[i] = a[i] - b[i];
    }
}

static void M(gf o, const gf a, const gf b)
{
    int64_t t[31];
    int i, j;
    for (i = 0; i < 31; i++) {
        t[i] = 0;
    }
    for (i = 0; i < 16; i++) {
        for (j = 0; j < 16; j++) {
            t[i + j] += a[i] * b[j];
        }
    }
    for (i = 0; i < 15; i++) {
        t[i] += 38 * t[i + 16];
    }
    for (i = 0; i < 16; i++) {
        o[i] = t[i];
    }
    car25519(o);
    car25519(o);
}

static void S(gf o, const gf a)
{
    M(o, a, a);
}

static void inv25519(gf o, const gf i_val)
{
    gf c;
    int a;
    for (a = 0; a < 16; a++) {
        c[a] = i_val[a];
    }
    for (a = 253; a >= 0; a--) {
        S(c, c);
        if (a != 2 && a != 4) {
            M(c, c, i_val);
        }
    }
    for (a = 0; a < 16; a++) {
        o[a] = c[a];
    }
}

static void pow2523(gf o, const gf i_val)
{
    gf c;
    int a;
    for (a = 0; a < 16; a++) {
        c[a] = i_val[a];
    }
    for (a = 250; a >= 0; a--) {
        S(c, c);
        if (a != 1) {
            M(c, c, i_val);
        }
    }
    for (a = 0; a < 16; a++) {
        o[a] = c[a];
    }
}

static void set25519(gf r, const gf a)
{
    for (int i = 0; i < 16; i++) {
        r[i] = a[i];
    }
}

/* ── Group Operations on Edwards25519 ─────────────────────────────────────── */

static void add(gf p[4], gf q[4])
{
    gf a, b, c, d_pt, t, e, f, g, h;

    Z(a, p[1], p[0]);
    Z(t, q[1], q[0]);
    M(a, a, t);
    A(b, p[0], p[1]);
    A(t, q[0], q[1]);
    M(b, b, t);
    M(c, p[3], q[3]);
    M(c, c, D2);
    M(d_pt, p[2], q[2]);
    A(d_pt, d_pt, d_pt);
    Z(e, b, a);
    Z(f, d_pt, c);
    A(g, d_pt, c);
    A(h, b, a);

    M(p[0], e, f);
    M(p[1], h, g);
    M(p[2], g, f);
    M(p[3], e, h);
}

static void cswap(gf p[4], gf q[4], uint8_t b)
{
    for (int i = 0; i < 4; i++) {
        sel25519(p[i], q[i], (int)b);
    }
}

static void pack(uint8_t *r, gf p[4])
{
    gf tx, ty, zi;
    inv25519(zi, p[2]);
    M(tx, p[0], zi);
    M(ty, p[1], zi);
    pack25519(r, ty);
    r[31] ^= (uint8_t)(par25519(tx) << 7);
}

static void scalarmult(gf p[4], gf q[4], const uint8_t *s)
{
    set25519(p[0], gf0);
    set25519(p[1], gf1);
    set25519(p[2], gf1);
    set25519(p[3], gf0);
    for (int i = 255; i >= 0; --i) {
        uint8_t b = (uint8_t)((s[i / 8] >> (i & 7)) & 1);
        cswap(p, q, b);
        add(q, p);
        add(p, p);
        cswap(p, q, b);
    }
}

static void scalarbase(gf p[4], const uint8_t *s)
{
    gf q[4];
    set25519(q[0], X);
    set25519(q[1], Y);
    set25519(q[2], gf1);
    M(q[3], X, Y);
    scalarmult(p, q, s);
}

static int unpackneg(gf r[4], const uint8_t p[32])
{
    gf t, chk, num, den, den2, den4, den6;
    set25519(r[2], gf1);
    unpack25519(r[1], p);
    S(num, r[1]);
    M(den, num, D);
    Z(num, num, r[2]);
    A(den, r[2], den);

    S(den2, den);
    S(den4, den2);
    M(den6, den4, den2);
    M(t, den6, num);
    M(t, t, den);

    pow2523(t, t);
    M(t, t, num);
    M(t, t, den);
    M(t, t, den);
    M(r[0], t, den);

    S(chk, r[0]);
    M(chk, chk, den);
    if (neq25519(chk, num) != 0) {
        M(r[0], r[0], I);
    }

    S(chk, r[0]);
    M(chk, chk, den);
    if (neq25519(chk, num) != 0) {
        return -1;
    }

    if (par25519(r[0]) == (uint8_t)(p[31] >> 7)) {
        Z(r[0], gf0, r[0]);
    }

    M(r[3], r[0], r[1]);
    return 0;
}

/* ── Scalar Arithmetic Modulo L (RFC 8032 Section 5.1) ───────────────────── */

static const int64_t ORDER_L[32] = {
    0xed, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58, 0xd6, 0x9c, 0xf7, 0xa2, 0xde, 0xf9, 0xde, 0x14,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0x10};

static void modL(uint8_t *r, int64_t x[64])
{
    int64_t carry, i, j;
    for (i = 63; i >= 32; --i) {
        carry = 0;
        for (j = i - 32; j < i - 12; ++j) {
            x[j] += carry - 16 * x[i] * ORDER_L[j - (i - 32)];
            carry = (x[j] + 128) >> 8;
            x[j] -= carry * 256;
        }
        x[j] += carry;
        x[i] = 0;
    }
    carry = 0;
    for (j = 0; j < 32; j++) {
        x[j] += carry - (x[31] >> 4) * ORDER_L[j];
        carry = x[j] >> 8;
        x[j] &= 255;
    }
    for (j = 0; j < 32; j++) {
        x[j] -= carry * ORDER_L[j];
    }
    for (i = 0; i < 32; i++) {
        x[i + 1] += x[i] >> 8;
        r[i] = (uint8_t)(x[i] & 255);
    }
}

static void reduce(uint8_t *r)
{
    int64_t x[64];
    for (int i = 0; i < 64; i++) {
        x[i] = (int64_t)r[i];
    }
    for (int i = 0; i < 64; i++) {
        r[i] = 0;
    }
    modL(r, x);
}

/** @endcond */

/* ── Public API ──────────────────────────────────────────────────────────── */

bool syn_ed25519_publickey(const uint8_t secret_key[SYN_ED25519_SECRET_KEY_SIZE],
                           uint8_t public_key[SYN_ED25519_PUBLIC_KEY_SIZE])
{
    if (secret_key == NULL || public_key == NULL) {
        return false;
    }

    uint8_t d[64];
    gf p[4];

    SYN_SHA512_Ctx hash_ctx;
    sha512_init(&hash_ctx);
    sha512_update(&hash_ctx, secret_key, SYN_ED25519_SECRET_KEY_SIZE);
    sha512_final(&hash_ctx, d);

    d[0] &= 248U;
    d[31] &= 127U;
    d[31] |= 64U;

    scalarbase(p, d);
    pack(public_key, p);

    volatile uint8_t *vp = (volatile uint8_t *)d;
    for (size_t i = 0; i < sizeof(d); i++) {
        vp[i] = 0U;
    }
    return true;
}

bool syn_ed25519_create_keypair(uint8_t public_key[SYN_ED25519_PUBLIC_KEY_SIZE],
                                uint8_t secret_key[SYN_ED25519_SECRET_KEY_SIZE],
                                const uint8_t seed[SYN_ED25519_SEED_SIZE])
{
    if (public_key == NULL || secret_key == NULL || seed == NULL) {
        return false;
    }

    (void)memcpy(secret_key, seed, SYN_ED25519_SECRET_KEY_SIZE);
    return syn_ed25519_publickey(secret_key, public_key);
}

bool syn_ed25519_sign(const uint8_t *msg, size_t msg_len,
                      const uint8_t secret_key[SYN_ED25519_SECRET_KEY_SIZE],
                      const uint8_t public_key[SYN_ED25519_PUBLIC_KEY_SIZE],
                      uint8_t sig[SYN_ED25519_SIGNATURE_SIZE])
{
    if ((msg == NULL && msg_len > 0U) || secret_key == NULL || sig == NULL) {
        return false;
    }

    uint8_t pk_buf[SYN_ED25519_PUBLIC_KEY_SIZE];
    const uint8_t *pk = public_key;
    if (pk == NULL) {
        (void)syn_ed25519_publickey(secret_key, pk_buf);
        pk = pk_buf;
    }

    uint8_t d[64], r[64];
    int64_t x[64];
    gf p[4];

    SYN_SHA512_Ctx hash_ctx;
    sha512_init(&hash_ctx);
    sha512_update(&hash_ctx, secret_key, SYN_ED25519_SECRET_KEY_SIZE);
    sha512_final(&hash_ctx, d);

    d[0] &= 248U;
    d[31] &= 127U;
    d[31] |= 64U;

    /* Nonce r = SHA-512(d[32..63] || msg) */
    sha512_init(&hash_ctx);
    sha512_update(&hash_ctx, d + 32, 32U);
    if (msg != NULL && msg_len > 0U) {
        sha512_update(&hash_ctx, msg, msg_len);
    }
    sha512_final(&hash_ctx, r);
    reduce(r);

    /* R = r * B */
    scalarbase(p, r);
    pack(sig, p);

    /* Challenge h = SHA-512(R || pk || msg) */
    sha512_init(&hash_ctx);
    sha512_update(&hash_ctx, sig, 32U);
    sha512_update(&hash_ctx, pk, 32U);
    if (msg != NULL && msg_len > 0U) {
        sha512_update(&hash_ctx, msg, msg_len);
    }
    uint8_t h[64];
    sha512_final(&hash_ctx, h);
    reduce(h);

    /* S = (r + h * d) mod L */
    for (int i = 0; i < 64; i++) {
        x[i] = 0;
    }
    for (int i = 0; i < 32; i++) {
        x[i] = (int64_t)r[i];
    }
    for (int i = 0; i < 32; i++) {
        for (int j = 0; j < 32; j++) {
            x[i + j] += (int64_t)h[i] * (int64_t)d[j];
        }
    }
    modL(sig + 32, x);

    volatile uint8_t *vp = (volatile uint8_t *)d;
    for (size_t i = 0; i < sizeof(d); i++) {
        vp[i] = 0U;
    }
    vp = (volatile uint8_t *)r;
    for (size_t i = 0; i < sizeof(r); i++) {
        vp[i] = 0U;
    }
    return true;
}

bool syn_ed25519_verify(const uint8_t sig[SYN_ED25519_SIGNATURE_SIZE], const uint8_t *msg,
                        size_t msg_len, const uint8_t public_key[SYN_ED25519_PUBLIC_KEY_SIZE])
{
    if (sig == NULL || (msg == NULL && msg_len > 0U) || public_key == NULL) {
        return false;
    }

    if ((sig[63] & 224U) != 0U) {
        return false;
    }

    gf p[4], q[4];
    if (unpackneg(q, public_key) != 0) {
        return false;
    }

    /* Challenge h = SHA-512(R || pk || msg) */
    SYN_SHA512_Ctx hash_ctx;
    sha512_init(&hash_ctx);
    sha512_update(&hash_ctx, sig, 32U);
    sha512_update(&hash_ctx, public_key, 32U);
    if (msg != NULL && msg_len > 0U) {
        sha512_update(&hash_ctx, msg, msg_len);
    }
    uint8_t h[64];
    sha512_final(&hash_ctx, h);
    reduce(h);

    scalarmult(p, q, h);
    scalarbase(q, sig + 32);
    add(p, q);

    uint8_t t[32];
    pack(t, p);

    return (crypto_verify_32(sig, t) == 0);
}
