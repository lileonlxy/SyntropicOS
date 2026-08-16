/**
 * @file syn_aes_cmac.h
 * @brief AES-CMAC (Cipher-based Message Authentication Code, RFC 4493 / NIST SP 800-38B).
 *
 * Constant-time, zero-heap message authentication code engine based on AES-128.
 * Standard primitive for BLE Secure Connections, LoRaWAN, AUTOSAR SecOC, and COSE.
 * @ingroup syn_crypto
 */

#ifndef SYN_AES_CMAC_H
#define SYN_AES_CMAC_H

#include "../common/syn_defs.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief AES-CMAC key size in bytes (16 bytes / 128 bits). */
#define SYN_AES_CMAC_KEY_SIZE 16U

/** @brief AES-CMAC cipher block size in bytes (16 bytes / 128 bits). */
#define SYN_AES_CMAC_BLOCK_SIZE 16U

/** @brief AES-CMAC generated authentication tag size in bytes (16 bytes / 128 bits). */
#define SYN_AES_CMAC_TAG_SIZE 16U

/**
 * @brief Generate a 128-bit AES-CMAC authentication tag (RFC 4493).
 *
 * @param[in]  key      16-byte AES-128 secret key.
 * @param[in]  msg      Pointer to message to authenticate (may be NULL if msg_len is 0).
 * @param[in]  msg_len  Length of the message in bytes.
 * @param[out] mac      16-byte buffer to receive the computed MAC tag.
 * @return SYN_OK on success, or SYN_INVALID_PARAM on invalid/NULL parameters.
 */
SYN_Status syn_aes_cmac(const uint8_t key[SYN_AES_CMAC_KEY_SIZE], const uint8_t *msg,
                        size_t msg_len, uint8_t mac[SYN_AES_CMAC_TAG_SIZE]);

/**
 * @brief Verify an AES-CMAC authentication tag in constant time (RFC 4493).
 *
 * @param[in] key      16-byte AES-128 secret key.
 * @param[in] msg      Pointer to message to verify (may be NULL if msg_len is 0).
 * @param[in] msg_len  Length of the message in bytes.
 * @param[in] mac      16-byte expected MAC tag to verify against.
 * @return true if valid and authentic, false on mismatch or invalid parameters.
 */
bool syn_aes_cmac_verify(const uint8_t key[SYN_AES_CMAC_KEY_SIZE], const uint8_t *msg,
                         size_t msg_len, const uint8_t mac[SYN_AES_CMAC_TAG_SIZE]);

#ifdef __cplusplus
}
#endif

#endif /* SYN_AES_CMAC_H */
