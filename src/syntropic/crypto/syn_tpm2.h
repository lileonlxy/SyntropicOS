/**
 * @file syn_tpm2.h
 * @brief TCG TPM 2.0 Command Marshaller, Measured Boot, & Hardware Root-of-Trust Engine.
 * @ingroup syn_crypto
 *
 * Implements a zero-heap, deterministic TCG TPM 2.0 command serializer and response parser:
 * - Direct interface over SPI / I2C / LPC TCG FIFO hardware interface (`SYN_Transport`).
 * - Measured Boot PCR Operations (`TPM2_PCR_Read`, `TPM2_PCR_Extend` for SHA-256 / SHA-384).
 * - Hardware Cryptographic Entropy (`TPM2_GetRandom`).
 * - Hardware Identity & Remote Attestation Quotes (`TPM2_Quote`).
 * - Tamper-Proof Non-Volatile Storage (`TPM2_NV_Read`, `TPM2_NV_Write`).
 * - Power & Self-Test Lifecycle (`TPM2_Startup`, `TPM2_SelfTest`).
 */

#ifndef SYN_TPM2_H
#define SYN_TPM2_H

#include "../common/syn_defs.h"
#include "../net/syn_transport.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if !defined(SYN_USE_TPM2) || SYN_USE_TPM2

#ifdef __cplusplus
extern "C" {
#endif

/* ── Constants & Structures ─────────────────────────────────────────────── */

#define SYN_TPM2_ST_NO_SESSIONS 0x8001U /**< Command/Response Tag: No session authorization */
#define SYN_TPM2_ST_SESSIONS 0x8002U    /**< Command/Response Tag: With session authorization */

#define SYN_TPM2_RC_SUCCESS 0x00000000U /**< TPM 2.0 Success Return Code */

/* TPM 2.0 Command Codes (TPM_CC_*) */
#define SYN_TPM2_CC_STARTUP 0x00000144U    /**< TPM2_Startup */
#define SYN_TPM2_CC_SELFTEST 0x00000143U   /**< TPM2_SelfTest */
#define SYN_TPM2_CC_GETRANDOM 0x0000017BU  /**< TPM2_GetRandom */
#define SYN_TPM2_CC_PCR_READ 0x0000017EU   /**< TPM2_PCR_Read */
#define SYN_TPM2_CC_PCR_EXTEND 0x00000182U /**< TPM2_PCR_Extend */
#define SYN_TPM2_CC_QUOTE 0x00000158U      /**< TPM2_Quote */
#define SYN_TPM2_CC_NV_READ 0x0000014EU    /**< TPM2_NV_Read */
#define SYN_TPM2_CC_NV_WRITE 0x00000137U   /**< TPM2_NV_Write */

/* TPM 2.0 Algorithm Identifiers (TPM_ALG_*) */
#define SYN_TPM2_ALG_SHA256 0x000BU /**< SHA-256 (32 bytes) */
#define SYN_TPM2_ALG_SHA384 0x000CU /**< SHA-384 (48 bytes) */
#define SYN_TPM2_ALG_NULL 0x0010U   /**< Null algorithm */

/* TPM 2.0 Standard Handles & Constants */
#define SYN_TPM2_RH_OWNER 0x40000001U    /**< Owner hierarchy */
#define SYN_TPM2_RH_PLATFORM 0x4000000CU /**< Platform hierarchy */
#define SYN_TPM2_RS_PW 0x40000009U       /**< Empty password authorization session */

#define SYN_TPM2_SU_CLEAR 0x0000U /**< Startup clear (cold boot) */
#define SYN_TPM2_SU_STATE 0x0001U /**< Startup state (warm sleep resume) */

#define SYN_TPM2_MAX_DIGEST_LEN 48U /**< Max hash digest length (SHA-384) */
#define SYN_TPM2_MAX_QUOTE_LEN 256U /**< Max attest quote signature length */

/* ── Result Types ───────────────────────────────────────────────────────── */

/**
 * @brief TPM 2.0 Attestation Quote Output Structure.
 */
typedef struct {
    uint8_t attest_data[SYN_TPM2_MAX_QUOTE_LEN]; /**< TPMS_ATTEST serialized data */
    uint16_t attest_len;                         /**< Length of TPMS_ATTEST data */
    uint8_t signature[SYN_TPM2_MAX_QUOTE_LEN];   /**< TPMT_SIGNATURE quote signature */
    uint16_t signature_len;                      /**< Length of signature */
} SYN_TPM2_QuoteResult;

/**
 * @brief TPM 2.0 Context Configuration Descriptor.
 */
typedef struct {
    SYN_Transport *transport; /**< Low-level SPI/I2C TCG FIFO transport interface */
    uint8_t *rx_buf;          /**< Scratch response buffer */
    size_t rx_buf_size;       /**< Scratch response buffer capacity (>= 256 bytes) */
    uint8_t *tx_buf;          /**< Scratch command buffer */
    size_t tx_buf_size;       /**< Scratch command buffer capacity (>= 256 bytes) */
} SYN_TPM2_Config;

/**
 * @brief TPM 2.0 Client Instance Context.
 */
typedef struct {
    SYN_TPM2_Config cfg; /**< Configuration */
    uint32_t last_rc;    /**< Return code from last executed command */
    bool initialized;    /**< Initialized flag */
} SYN_TPM2_Context;

/* ── Public API ─────────────────────────────────────────────────────────── */

/**
 * @brief Initialize TPM 2.0 Context.
 * @param ctx Context instance.
 * @param cfg Configuration descriptor.
 * @return SYN_OK on success, SYN_INVALID_PARAM on invalid parameter.
 */
SYN_Status syn_tpm2_init(SYN_TPM2_Context *ctx, const SYN_TPM2_Config *cfg);

/**
 * @brief Execute TPM2_Startup command.
 * @param ctx Context instance.
 * @param startup_type Startup mode (SYN_TPM2_SU_CLEAR or SYN_TPM2_SU_STATE).
 * @return SYN_OK on success, SYN_ERROR on TPM failure.
 */
SYN_Status syn_tpm2_startup(SYN_TPM2_Context *ctx, uint16_t startup_type);

/**
 * @brief Execute TPM2_SelfTest command.
 * @param ctx Context instance.
 * @param full_test If true, tests all algorithms; if false, tests incrementally.
 * @return SYN_OK on success.
 */
SYN_Status syn_tpm2_self_test(SYN_TPM2_Context *ctx, bool full_test);

/**
 * @brief Generate cryptographic true random bytes from TPM TRNG (TPM2_GetRandom).
 * @param ctx Context instance.
 * @param num_bytes Number of random bytes requested.
 * @param out_random Buffer to receive random bytes.
 * @param out_len Pointer to receive actual number of bytes returned.
 * @return SYN_OK on success.
 */
SYN_Status syn_tpm2_get_random(SYN_TPM2_Context *ctx, uint16_t num_bytes, uint8_t *out_random,
                               uint16_t *out_len);

/**
 * @brief Read single Platform Configuration Register (TPM2_PCR_Read).
 * @param ctx Context instance.
 * @param pcr_index PCR register index (0..23).
 * @param hash_alg Hash algorithm bank (SYN_TPM2_ALG_SHA256 or SYN_TPM2_ALG_SHA384).
 * @param out_digest Output buffer to receive digest.
 * @param out_digest_len Pointer to receive digest length (32 for SHA-256, 48 for SHA-384).
 * @return SYN_OK on success.
 */
SYN_Status syn_tpm2_pcr_read(SYN_TPM2_Context *ctx, uint32_t pcr_index, uint16_t hash_alg,
                             uint8_t *out_digest, size_t *out_digest_len);

/**
 * @brief Extend Platform Configuration Register with measurement digest (TPM2_PCR_Extend).
 * @param ctx Context instance.
 * @param pcr_index PCR register index (0..23).
 * @param hash_alg Hash algorithm (SYN_TPM2_ALG_SHA256 or SYN_TPM2_ALG_SHA384).
 * @param in_digest Digest bytes to extend PCR with.
 * @param digest_len Length of in_digest (must match hash algorithm).
 * @return SYN_OK on success.
 */
SYN_Status syn_tpm2_pcr_extend(SYN_TPM2_Context *ctx, uint32_t pcr_index, uint16_t hash_alg,
                               const uint8_t *in_digest, size_t digest_len);

/**
 * @brief Generate Remote Attestation Quote over PCR values (TPM2_Quote).
 * @param ctx Context instance.
 * @param key_handle Attestation signing key handle (e.g. AK / EK).
 * @param qualifying_data Nonce / qualifying data to prevent replay.
 * @param qual_len Length of qualifying data.
 * @param pcr_mask 24-bit bitmask of PCRs to include in quote.
 * @param out_quote Pointer to receive quote structure.
 * @return SYN_OK on success.
 */
SYN_Status syn_tpm2_quote(SYN_TPM2_Context *ctx, uint32_t key_handle,
                          const uint8_t *qualifying_data, size_t qual_len, uint32_t pcr_mask,
                          SYN_TPM2_QuoteResult *out_quote);

/**
 * @brief Read data from secure Non-Volatile storage index (TPM2_NV_Read).
 * @param ctx Context instance.
 * @param auth_handle Authorization handle (e.g. SYN_TPM2_RH_OWNER).
 * @param nv_index NVRAM index (e.g. 0x01500000).
 * @param offset Byte offset within NV area.
 * @param size Number of bytes to read.
 * @param out_data Output buffer.
 * @param out_len Pointer to receive bytes read.
 * @return SYN_OK on success.
 */
SYN_Status syn_tpm2_nv_read(SYN_TPM2_Context *ctx, uint32_t auth_handle, uint32_t nv_index,
                            uint16_t offset, uint16_t size, uint8_t *out_data, uint16_t *out_len);

/**
 * @brief Write data to secure Non-Volatile storage index (TPM2_NV_Write).
 * @param ctx Context instance.
 * @param auth_handle Authorization handle (e.g. SYN_TPM2_RH_OWNER).
 * @param nv_index NVRAM index.
 * @param offset Byte offset within NV area.
 * @param in_data Data buffer to write.
 * @param size Number of bytes to write.
 * @return SYN_OK on success.
 */
SYN_Status syn_tpm2_nv_write(SYN_TPM2_Context *ctx, uint32_t auth_handle, uint32_t nv_index,
                             uint16_t offset, const uint8_t *in_data, uint16_t size);

/**
 * @brief Get last TPM 2.0 response code returned by TPM hardware.
 * @param ctx Context instance.
 * @return 32-bit TPM response code (e.g. TPM_RC_SUCCESS = 0).
 */
uint32_t syn_tpm2_get_last_rc(const SYN_TPM2_Context *ctx);

#ifdef __cplusplus
}
#endif

#endif /* !defined(SYN_USE_TPM2) || SYN_USE_TPM2 */

#endif /* SYN_TPM2_H */
