/**
 * @file syn_ed25519.h
 * @brief Pure C99 Ed25519 Key Generation, Signing & Verification (RFC 8032).
 * @ingroup syn_crypto
 */

#ifndef SYN_ED25519_H
#define SYN_ED25519_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Size of Ed25519 public key in bytes (32). */
#define SYN_ED25519_PUBLIC_KEY_SIZE 32U
/** @brief Size of Ed25519 secret key (seed) in bytes (32). */
#define SYN_ED25519_SECRET_KEY_SIZE 32U
/** @brief Size of Ed25519 seed in bytes (32). */
#define SYN_ED25519_SEED_SIZE 32U
/** @brief Size of Ed25519 signature in bytes (64). */
#define SYN_ED25519_SIGNATURE_SIZE 64U

/** @brief SHA-512 context for incremental hashing (RFC 6234). */
typedef struct {
    uint64_t state[8];   /**< Intermediate 64-bit state words */
    uint64_t count;      /**< Total byte count ingested       */
    uint8_t buffer[128]; /**< 128-byte block buffer           */
} SYN_SHA512_Ctx;

/**
 * @brief Initialize a SHA-512 context.
 * @param ctx Pointer to context.
 */
void syn_sha512_init(SYN_SHA512_Ctx *ctx);

/**
 * @brief Ingest data into running SHA-512 context.
 * @param ctx  Pointer to context.
 * @param data Data buffer to hash.
 * @param len  Length of data in bytes.
 */
void syn_sha512_update(SYN_SHA512_Ctx *ctx, const uint8_t *data, size_t len);

/**
 * @brief Finalize running SHA-512 and output 64-byte digest.
 * @param ctx    Pointer to context.
 * @param digest Output 64-byte digest buffer.
 */
void syn_sha512_final(SYN_SHA512_Ctx *ctx, uint8_t digest[64]);

/**
 * @brief Derive a 32-byte Ed25519 public key from a 32-byte secret key (RFC 8032 Section 5.1.5).
 *
 * @param secret_key 32-byte private key/seed.
 * @param public_key Output 32-byte compressed Edwards public key.
 * @return true on success, false on invalid parameter.
 */
bool syn_ed25519_publickey(const uint8_t secret_key[SYN_ED25519_SECRET_KEY_SIZE],
                           uint8_t public_key[SYN_ED25519_PUBLIC_KEY_SIZE]);

/**
 * @brief Create an Ed25519 keypair from a 32-byte random seed (RFC 8032).
 *
 * @param public_key Output 32-byte public key.
 * @param secret_key Output 32-byte secret key (copies seed).
 * @param seed       Input 32-byte random seed.
 * @return true on success, false on invalid parameter.
 */
bool syn_ed25519_create_keypair(uint8_t public_key[SYN_ED25519_PUBLIC_KEY_SIZE],
                                uint8_t secret_key[SYN_ED25519_SECRET_KEY_SIZE],
                                const uint8_t seed[SYN_ED25519_SEED_SIZE]);

/**
 * @brief Generate an Ed25519 signature (RFC 8032 Section 5.1.6).
 *
 * @param msg        Message to sign.
 * @param msg_len    Length of message in bytes.
 * @param secret_key 32-byte private key.
 * @param public_key 32-byte public key (optional; if NULL, derived internally from secret_key).
 * @param sig        Output 64-byte signature (R || S).
 * @return true on success, false on invalid parameter.
 */
bool syn_ed25519_sign(const uint8_t *msg, size_t msg_len,
                      const uint8_t secret_key[SYN_ED25519_SECRET_KEY_SIZE],
                      const uint8_t public_key[SYN_ED25519_PUBLIC_KEY_SIZE],
                      uint8_t sig[SYN_ED25519_SIGNATURE_SIZE]);

/**
 * @brief Verify an Ed25519 signature (RFC 8032 Section 5.1.7).
 *
 * @param sig        64-byte signature (R || S).
 * @param msg        Message bytes.
 * @param msg_len    Message length in bytes.
 * @param public_key 32-byte Ed25519 public key.
 * @return true if valid signature, false otherwise.
 */
bool syn_ed25519_verify(const uint8_t sig[SYN_ED25519_SIGNATURE_SIZE], const uint8_t *msg,
                        size_t msg_len, const uint8_t public_key[SYN_ED25519_PUBLIC_KEY_SIZE]);

/**
 * @brief Verify an Ed25519 signature from a precomputed 64-byte challenge digest H(R || A || M).
 *
 * @param sig        64-byte signature (R || S).
 * @param h          64-byte SHA-512 digest.
 * @param public_key 32-byte Ed25519 public key.
 * @return true if valid signature, false otherwise.
 */
bool syn_ed25519_verify_hash(const uint8_t sig[SYN_ED25519_SIGNATURE_SIZE], const uint8_t h[64],
                             const uint8_t public_key[SYN_ED25519_PUBLIC_KEY_SIZE]);

#ifdef __cplusplus
}
#endif

#endif /* SYN_ED25519_H */
