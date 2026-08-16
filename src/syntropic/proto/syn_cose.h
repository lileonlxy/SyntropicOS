/**
 * @file syn_cose.h
 * @brief Zero-Heap CBOR Object Signing and Encryption (COSE - RFC 9052 / RFC 8152).
 *
 * Implements COSE_Sign1 (Tag 18) and COSE_Encrypt0 (Tag 16) for constrained IoT nodes.
 * Supported Cryptographic Algorithms:
 * - EdDSA (-8): Pure C99 Ed25519 signature scheme (RFC 8032).
 * - ES256 (-7): ECDSA over NIST P-256 curve with SHA-256 (RFC 9053).
 * - ChaCha20/Poly1305 (24): Authenticated encryption with associated data (RFC 9053).
 *
 * @ingroup syn_proto
 */

#ifndef SYN_COSE_H
#define SYN_COSE_H

#if __has_include("syn_config.h")
#include "syn_config.h"
#endif

#if !defined(SYN_USE_COSE) || SYN_USE_COSE

#include "syntropic/common/syn_defs.h"
#include "syntropic/crypto/syn_chacha20poly1305.h"
#include "syntropic/crypto/syn_ed25519.h"
#include "syntropic/crypto/syn_p256.h"
#include "syntropic/util/syn_cbor_read.h"
#include "syntropic/util/syn_cbor_write.h"
#include "syntropic/util/syn_sha256.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── COSE Header Parameter Labels (RFC 9052 Section 3.1) ─────────────────── */

/** @brief Header parameter label for algorithm identifier (1). */
#define SYN_COSE_HEADER_ALG 1
/** @brief Header parameter label for criticality (2). */
#define SYN_COSE_HEADER_CRIT 2
/** @brief Header parameter label for content type (3). */
#define SYN_COSE_HEADER_CONTENT_TYPE 3
/** @brief Header parameter label for key identifier (4). */
#define SYN_COSE_HEADER_KID 4
/** @brief Header parameter label for initialization vector / nonce (5). */
#define SYN_COSE_HEADER_IV 5
/** @brief Header parameter label for partial IV (6). */
#define SYN_COSE_HEADER_PARTIAL_IV 6

/* ── COSE Algorithm Identifiers (RFC 9053 / IANA COSE Algorithms) ────────── */

/** @brief COSE Algorithm Identifiers */
typedef enum {
    SYN_COSE_ALGO_UNKNOWN = 0,
    SYN_COSE_ALGO_ES256 = -7,            /**< ECDSA with SHA-256 on P-256 curve */
    SYN_COSE_ALGO_EDDSA = -8,            /**< EdDSA with Ed25519 (RFC 8032) */
    SYN_COSE_ALGO_CHACHA20_POLY1305 = 24 /**< ChaCha20/Poly1305 AEAD (128-bit tag) */
} SYN_COSE_Algorithm;

/* ── Constants ──────────────────────────────────────────────────────────── */

/** @brief Maximum key ID length (32 bytes). */
#define SYN_COSE_MAX_KID_LEN 32U
/** @brief Maximum IV length (16 bytes). */
#define SYN_COSE_MAX_IV_LEN 16U
/** @brief Maximum signature length in bytes (64 bytes for Ed25519 / P-256). */
#define SYN_COSE_MAX_SIG_LEN 64U
/** @brief Maximum protected header byte length (64 bytes). */
#define SYN_COSE_MAX_PROTECTED_LEN 64U

/* ── COSE Parsed Message Structures ─────────────────────────────────────── */

/**
 * @brief Parsed COSE_Sign1 Message descriptor.
 */
typedef struct {
    SYN_COSE_Algorithm alg;                            /**< Signature algorithm */
    uint8_t kid[SYN_COSE_MAX_KID_LEN];                 /**< Key identifier */
    size_t kid_len;                                    /**< Key ID length */
    const uint8_t *payload;                            /**< Payload pointer */
    size_t payload_len;                                /**< Payload length */
    const uint8_t *signature;                          /**< Signature bytes pointer */
    size_t signature_len;                              /**< Signature length */
    uint8_t protected_hdr[SYN_COSE_MAX_PROTECTED_LEN]; /**< Serialized protected header */
    size_t protected_hdr_len;                          /**< Protected header length */
} SYN_COSE_Sign1Message;

/**
 * @brief Parsed COSE_Encrypt0 Message descriptor.
 */
typedef struct {
    SYN_COSE_Algorithm alg;                            /**< AEAD algorithm */
    uint8_t kid[SYN_COSE_MAX_KID_LEN];                 /**< Key identifier */
    size_t kid_len;                                    /**< Key ID length */
    uint8_t iv[SYN_COSE_MAX_IV_LEN];                   /**< IV / Nonce */
    size_t iv_len;                                     /**< IV length */
    const uint8_t *ciphertext;                         /**< Ciphertext + Tag pointer */
    size_t ciphertext_len;                             /**< Ciphertext length */
    uint8_t protected_hdr[SYN_COSE_MAX_PROTECTED_LEN]; /**< Serialized protected header */
    size_t protected_hdr_len;                          /**< Protected header length */
} SYN_COSE_Encrypt0Message;

/* ── COSE_Sign1 API ──────────────────────────────────────────────────────── */

/**
 * @brief Create a COSE_Sign1 (Tag 18) message signed with EdDSA (Ed25519) or ES256 (P-256).
 *
 * @param alg             Signature algorithm (SYN_COSE_ALGO_EDDSA or SYN_COSE_ALGO_ES256).
 * @param secret_key      Secret key (32 bytes for Ed25519 / P-256 scalar).
 * @param public_key      Public key (32 bytes for Ed25519, 64/65 bytes for P-256).
 * @param kid             Optional key identifier (NULL if unused).
 * @param kid_len         Key identifier length.
 * @param payload         Message payload to sign.
 * @param payload_len     Payload length.
 * @param external_aad    Optional external associated data (NULL if unused).
 * @param external_aad_len External AAD length.
 * @param out_buf         [out] Output buffer for encoded COSE_Sign1 message.
 * @param out_buf_size    Capacity of output buffer.
 * @param out_len         [out] Number of bytes written to output buffer.
 * @return SYN_Status     SYN_OK on success, error code otherwise.
 */
SYN_Status syn_cose_sign1_create(SYN_COSE_Algorithm alg, const uint8_t *secret_key,
                                 const uint8_t *public_key, const uint8_t *kid, size_t kid_len,
                                 const uint8_t *payload, size_t payload_len,
                                 const uint8_t *external_aad, size_t external_aad_len,
                                 uint8_t *out_buf, size_t out_buf_size, size_t *out_len);

/**
 * @brief Parse and verify a COSE_Sign1 message using the signer's public key.
 *
 * @param msg             Raw COSE_Sign1 CBOR encoded message.
 * @param msg_len         Message length.
 * @param public_key      Public key of the signer.
 * @param public_key_len  Public key length (32 for Ed25519, 64/65 for P-256).
 * @param external_aad    Optional external associated data (NULL if unused).
 * @param external_aad_len External AAD length.
 * @param parsed_out      [out] Optional parsed message details (may be NULL).
 * @return SYN_Status     SYN_OK if verified and valid, error code otherwise.
 */
SYN_Status syn_cose_sign1_verify(const uint8_t *msg, size_t msg_len, const uint8_t *public_key,
                                 size_t public_key_len, const uint8_t *external_aad,
                                 size_t external_aad_len, SYN_COSE_Sign1Message *parsed_out);

/* ── COSE_Encrypt0 API ───────────────────────────────────────────────────── */

/**
 * @brief Encrypt a payload into a COSE_Encrypt0 (Tag 16) message using ChaCha20/Poly1305.
 *
 * @param alg             Encryption algorithm (SYN_COSE_ALGO_CHACHA20_POLY1305).
 * @param key             Symmetric key (32 bytes).
 * @param iv              Nonce / IV (12 bytes for ChaCha20/Poly1305).
 * @param iv_len          IV length.
 * @param kid             Optional key identifier (NULL if unused).
 * @param kid_len         Key identifier length.
 * @param plaintext       Plaintext payload to encrypt.
 * @param plaintext_len   Plaintext length.
 * @param external_aad    Optional external associated data (NULL if unused).
 * @param external_aad_len External AAD length.
 * @param out_buf         [out] Output buffer for encoded COSE_Encrypt0 message.
 * @param out_buf_size    Capacity of output buffer.
 * @param out_len         [out] Number of bytes written to output buffer.
 * @return SYN_Status     SYN_OK on success, error code otherwise.
 */
SYN_Status syn_cose_encrypt0_create(SYN_COSE_Algorithm alg, const uint8_t *key, const uint8_t *iv,
                                    size_t iv_len, const uint8_t *kid, size_t kid_len,
                                    const uint8_t *plaintext, size_t plaintext_len,
                                    const uint8_t *external_aad, size_t external_aad_len,
                                    uint8_t *out_buf, size_t out_buf_size, size_t *out_len);

/**
 * @brief Decrypt and verify a COSE_Encrypt0 message.
 *
 * @param msg             Raw COSE_Encrypt0 CBOR encoded message.
 * @param msg_len         Message length.
 * @param key             Symmetric key (32 bytes).
 * @param external_aad    Optional external associated data (NULL if unused).
 * @param external_aad_len External AAD length.
 * @param out_plaintext   [out] Output buffer for decrypted plaintext.
 * @param out_plaintext_size Output buffer capacity.
 * @param out_plaintext_len [out] Number of plaintext bytes written.
 * @param parsed_out      [out] Optional parsed message details (may be NULL).
 * @return SYN_Status     SYN_OK if decrypted and authenticated, error code otherwise.
 */
SYN_Status syn_cose_encrypt0_decrypt(const uint8_t *msg, size_t msg_len, const uint8_t *key,
                                     const uint8_t *external_aad, size_t external_aad_len,
                                     uint8_t *out_plaintext, size_t out_plaintext_size,
                                     size_t *out_plaintext_len,
                                     SYN_COSE_Encrypt0Message *parsed_out);

#ifdef __cplusplus
}
#endif

#endif /* SYN_USE_COSE */

#endif /* SYN_COSE_H */
