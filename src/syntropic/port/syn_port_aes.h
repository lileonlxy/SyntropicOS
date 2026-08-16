/**
 * @file syn_port_aes.h
 * @brief AES / GCM hardware acceleration port interface.
 *
 * Implement these functions to offload AES block cipher and GHASH field multiplication
 * to microcontroller hardware security / cryptographic accelerators (e.g. STM32 CRYP,
 * ESP32 hardware AES).
 *
 * Enable via SYN_USE_PORT_AES in syn_config.h. When disabled or returning SYN_NOT_IMPLEMENTED,
 * SyntropicOS automatically falls back to software calculation.
 * @ingroup syn_crypto
 */

#ifndef SYN_PORT_AES_H
#define SYN_PORT_AES_H

#include "../common/syn_defs.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the hardware AES/crypto accelerator.
 * @return SYN_OK on success, SYN_NOT_IMPLEMENTED if hardware crypto is not available.
 */
SYN_Status syn_port_aes_init(void);

/**
 * @brief Offload a single 16-byte block encryption to hardware.
 * @param round_keys Pointer to expanded round keys.
 * @param nr Number of rounds (10, 12, or 14).
 * @param in Input 16-byte plaintext block.
 * @param out Output 16-byte ciphertext block.
 * @return SYN_OK on success, SYN_NOT_IMPLEMENTED to use software AES.
 */
SYN_Status syn_port_aes_encrypt_block(const uint8_t *round_keys, uint8_t nr, const uint8_t in[16],
                                      uint8_t out[16]);

/**
 * @brief Offload a single 16-byte block decryption to hardware.
 * @param round_keys Pointer to expanded round keys.
 * @param nr Number of rounds (10, 12, or 14).
 * @param in Input 16-byte ciphertext block.
 * @param out Output 16-byte plaintext block.
 * @return SYN_OK on success, SYN_NOT_IMPLEMENTED to use software AES.
 */
SYN_Status syn_port_aes_decrypt_block(const uint8_t *round_keys, uint8_t nr, const uint8_t in[16],
                                      uint8_t out[16]);

/**
 * @brief Offload GHASH Galois field GF(2^128) multiplication to hardware.
 * @param x Input 16-byte block.
 * @param h Input 16-byte subkey block.
 * @param out Output 16-byte product block.
 * @return SYN_OK on success, SYN_NOT_IMPLEMENTED to use software GHASH.
 */
SYN_Status syn_port_ghash_mult(const uint8_t x[16], const uint8_t h[16], uint8_t out[16]);

#ifdef __cplusplus
}
#endif

#endif /* SYN_PORT_AES_H */
