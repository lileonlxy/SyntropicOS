/**
 * @file syn_aes128.h
 * @brief AES-128 cipher engine (ECB & CBC mode with PKCS#7 padding).
 *
 * Constant-time, zero-heap implementation designed for embedded systems.
 * Supports 128-bit key expansion and CBC cipher block chaining.
 * @ingroup syn_crypto
 */

#ifndef SYN_AES128_H
#define SYN_AES128_H

#include "../common/syn_defs.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SYN_AES128_KEY_SIZE 16           /**< AES-128 key length in bytes (16) */
#define SYN_AES128_BLOCK_SIZE 16         /**< AES-128 cipher block size in bytes (16) */
#define SYN_AES128_EXPANDED_KEY_SIZE 176 /**< Total expanded round keys size in bytes (176) */

/**
 * @brief AES-128 Context — stores round keys.
 */
typedef struct {
    uint8_t round_keys[SYN_AES128_EXPANDED_KEY_SIZE]; /**< Expanded round keys array */
} SYN_AES128_Context;

/**
 * @brief Initialize AES-128 context and expand 128-bit key.
 * @param ctx AES-128 context instance.
 * @param key 16-byte key buffer.
 * @return SYN_OK on success.
 */
SYN_Status syn_aes128_init(SYN_AES128_Context *ctx, const uint8_t key[SYN_AES128_KEY_SIZE]);

/**
 * @brief Encrypt a single 16-byte block (ECB mode).
 * @param ctx   Initialized AES-128 context.
 * @param in    16-byte plaintext block.
 * @param out   16-byte ciphertext block.
 */
void syn_aes128_encrypt_block(const SYN_AES128_Context *ctx, const uint8_t in[16], uint8_t out[16]);

/**
 * @brief Decrypt a single 16-byte block (ECB mode).
 * @param ctx   Initialized AES-128 context.
 * @param in    16-byte ciphertext block.
 * @param out   16-byte plaintext block.
 */
void syn_aes128_decrypt_block(const SYN_AES128_Context *ctx, const uint8_t in[16], uint8_t out[16]);

/**
 * @brief Encrypt data using AES-128-CBC with PKCS#7 padding.
 * @param ctx         Initialized AES-128 context.
 * @param iv          16-byte initialization vector.
 * @param in          Plaintext buffer.
 * @param in_len      Plaintext length in bytes.
 * @param out         Ciphertext buffer (must be large enough for in_len + PKCS7 padding).
 * @param out_capacity Maximum size of output buffer.
 * @param out_len     Output byte count written.
 * @return SYN_OK on success.
 */
SYN_Status syn_aes128_cbc_encrypt(const SYN_AES128_Context *ctx, const uint8_t iv[16],
                                  const uint8_t *in, size_t in_len, uint8_t *out,
                                  size_t out_capacity, size_t *out_len);

/**
 * @brief Decrypt data using AES-128-CBC with PKCS#7 padding removal.
 * @param ctx         Initialized AES-128 context.
 * @param iv          16-byte initialization vector.
 * @param in          Ciphertext buffer (multiple of 16 bytes).
 * @param in_len      Ciphertext length in bytes.
 * @param out         Plaintext output buffer.
 * @param out_capacity Output buffer size.
 * @param out_len     Decrypted plaintext byte count written.
 * @return SYN_OK on success, or SYN_INVALID_PARAM on invalid padding.
 */
SYN_Status syn_aes128_cbc_decrypt(const SYN_AES128_Context *ctx, const uint8_t iv[16],
                                  const uint8_t *in, size_t in_len, uint8_t *out,
                                  size_t out_capacity, size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* SYN_AES128_H */
