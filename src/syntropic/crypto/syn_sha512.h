/**
 * @file syn_sha512.h
 * @brief SHA-512 & SHA-384 cryptographic hash and HMAC — pure C99, zero dependencies.
 *
 * Implements FIPS 180-4 compliant SHA-512 and SHA-384 secure hash algorithms
 * and RFC 4231 HMAC-SHA512 / HMAC-SHA384 message authentication.
 *
 * Follows the streaming pattern:
 *   init → update (repeated) → final
 *
 * The context struct is caller-owned (~216 bytes on 32-bit/64-bit targets).
 * Zero heap allocation, zero floating point.
 *
 * @par Usage
 * @code
 *   // One-shot SHA-512:
 *   uint8_t hash512[64];
 *   syn_sha512("abc", 3, hash512);
 *
 *   // One-shot SHA-384:
 *   uint8_t hash384[48];
 *   syn_sha384("abc", 3, hash384);
 *
 *   // Streaming SHA-512:
 *   SYN_SHA512 ctx;
 *   syn_sha512_init(&ctx);
 *   syn_sha512_update(&ctx, chunk1, len1);
 *   syn_sha512_update(&ctx, chunk2, len2);
 *   syn_sha512_final(&ctx, hash512);
 * @endcode
 * @ingroup syn_crypto
 */

#ifndef SYN_SHA512_H
#define SYN_SHA512_H

#if __has_include("syn_config.h")
#include "syn_config.h"
#endif

#if !defined(SYN_USE_SHA512) || SYN_USE_SHA512

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief SHA-512 digest size in bytes (64). */
#define SYN_SHA512_DIGEST_SIZE 64U

/** @brief SHA-384 digest size in bytes (48). */
#define SYN_SHA384_DIGEST_SIZE 48U

/** @brief SHA-512 / SHA-384 block size in bytes (128). */
#define SYN_SHA512_BLOCK_SIZE 128U

/* ── Context ────────────────────────────────────────────────────────────── */

/**
 * @brief SHA-512 / SHA-384 hash context — caller-owned.
 *
 * Size: 216 bytes (8×8 state + 128 buffer + 4 buf_len + 2×8 counters).
 */
typedef struct {
    uint64_t state[8];                  /**< Running 64-bit hash state (H0–H7) */
    uint8_t buf[SYN_SHA512_BLOCK_SIZE]; /**< Partial 128-byte block buffer     */
    uint32_t buf_len;                   /**< Bytes in buffer (0–127)           */
    uint64_t total_len_lo;              /**< Total message length in bytes, low  */
    uint64_t total_len_hi;              /**< Total message length in bytes, high */
} SYN_SHA512;

/* ── API ────────────────────────────────────────────────────────────────── */

/**
 * @brief Initialize context for SHA-512 hashing.
 * @param ctx Context to initialize.
 */
void syn_sha512_init(SYN_SHA512 *ctx);

/**
 * @brief Initialize context for SHA-384 hashing.
 * @param ctx Context to initialize.
 */
void syn_sha384_init(SYN_SHA512 *ctx);

/**
 * @brief Feed data into the SHA-512 / SHA-384 hash.
 * @param ctx  SHA-512 context.
 * @param data Data buffer to hash.
 * @param len  Length in bytes.
 */
void syn_sha512_update(SYN_SHA512 *ctx, const void *data, size_t len);

/**
 * @brief Finalize SHA-512 hash and produce 64-byte digest.
 * @param ctx  SHA-512 context.
 * @param hash Output buffer (must be at least 64 bytes).
 */
void syn_sha512_final(SYN_SHA512 *ctx, uint8_t hash[SYN_SHA512_DIGEST_SIZE]);

/**
 * @brief Finalize SHA-384 hash and produce 48-byte digest.
 * @param ctx  SHA-512 context initialized with syn_sha384_init().
 * @param hash Output buffer (must be at least 48 bytes).
 */
void syn_sha384_final(SYN_SHA512 *ctx, uint8_t hash[SYN_SHA384_DIGEST_SIZE]);

/**
 * @brief One-shot SHA-512 calculation.
 * @param data Data to hash.
 * @param len  Length in bytes.
 * @param hash Output 64-byte digest buffer.
 */
void syn_sha512(const void *data, size_t len, uint8_t hash[SYN_SHA512_DIGEST_SIZE]);

/**
 * @brief One-shot SHA-384 calculation.
 * @param data Data to hash.
 * @param len  Length in bytes.
 * @param hash Output 48-byte digest buffer.
 */
void syn_sha384(const void *data, size_t len, uint8_t hash[SYN_SHA384_DIGEST_SIZE]);

/**
 * @brief HMAC-SHA512 context — caller-owned.
 */
typedef struct {
    SYN_SHA512 inner;                         /**< Inner hash context */
    uint8_t o_key_pad[SYN_SHA512_BLOCK_SIZE]; /**< Outer key pad (K ⊕ opad) */
} SYN_HMAC_SHA512;

/**
 * @brief HMAC-SHA384 context — caller-owned.
 */
typedef struct {
    SYN_SHA512 inner;                         /**< Inner hash context */
    uint8_t o_key_pad[SYN_SHA512_BLOCK_SIZE]; /**< Outer key pad (K ⊕ opad) */
} SYN_HMAC_SHA384;

/**
 * @brief Initialize HMAC-SHA512 context with a key.
 * @param ctx     HMAC-SHA512 context.
 * @param key     Secret key buffer.
 * @param key_len Secret key length in bytes.
 */
void syn_hmac_sha512_init(SYN_HMAC_SHA512 *ctx, const void *key, size_t key_len);

/**
 * @brief Feed data chunk into HMAC-SHA512 computation.
 * @param ctx  HMAC-SHA512 context.
 * @param data Data buffer to authenticate.
 * @param len  Length in bytes.
 */
void syn_hmac_sha512_update(SYN_HMAC_SHA512 *ctx, const void *data, size_t len);

/**
 * @brief Finalize HMAC-SHA512 computation and retrieve 64-byte MAC.
 * @param ctx HMAC-SHA512 context.
 * @param mac Output 64-byte MAC buffer.
 */
void syn_hmac_sha512_final(SYN_HMAC_SHA512 *ctx, uint8_t mac[SYN_SHA512_DIGEST_SIZE]);

/**
 * @brief Initialize HMAC-SHA384 context with a key.
 * @param ctx     HMAC-SHA384 context.
 * @param key     Secret key buffer.
 * @param key_len Secret key length in bytes.
 */
void syn_hmac_sha384_init(SYN_HMAC_SHA384 *ctx, const void *key, size_t key_len);

/**
 * @brief Feed data chunk into HMAC-SHA384 computation.
 * @param ctx  HMAC-SHA384 context.
 * @param data Data buffer to authenticate.
 * @param len  Length in bytes.
 */
void syn_hmac_sha384_update(SYN_HMAC_SHA384 *ctx, const void *data, size_t len);

/**
 * @brief Finalize HMAC-SHA384 computation and retrieve 48-byte MAC.
 * @param ctx HMAC-SHA384 context.
 * @param mac Output 48-byte MAC buffer.
 */
void syn_hmac_sha384_final(SYN_HMAC_SHA384 *ctx, uint8_t mac[SYN_SHA384_DIGEST_SIZE]);

/**
 * @brief Compute HMAC-SHA512 over data using secret key (RFC 4231).
 * @param key      Secret key buffer.
 * @param key_len  Secret key length in bytes.
 * @param data     Data buffer to authenticate.
 * @param data_len Data length in bytes.
 * @param mac      Output 64-byte MAC buffer.
 */
void syn_hmac_sha512(const void *key, size_t key_len, const void *data, size_t data_len,
                     uint8_t mac[SYN_SHA512_DIGEST_SIZE]);

/**
 * @brief Compute HMAC-SHA384 over data using secret key (RFC 4231).
 * @param key      Secret key buffer.
 * @param key_len  Secret key length in bytes.
 * @param data     Data buffer to authenticate.
 * @param data_len Data length in bytes.
 * @param mac      Output 48-byte MAC buffer.
 */
void syn_hmac_sha384(const void *key, size_t key_len, const void *data, size_t data_len,
                     uint8_t mac[SYN_SHA384_DIGEST_SIZE]);

#ifdef __cplusplus
}
#endif

#endif /* SYN_USE_SHA512 */

#endif /* SYN_SHA512_H */
