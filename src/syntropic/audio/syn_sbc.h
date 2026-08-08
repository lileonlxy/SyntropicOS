/**
 * @file syn_sbc.h
 * @brief Bluetooth A2DP Sub-Band Codec (SBC) Decoder.
 * @ingroup syn_audio
 *
 * Zero-heap, fixed-point integer implementation of the Bluetooth SIG SBC decoder
 * (A2DP spec v1.3 / Bluetooth Core Spec AVTP).
 * Supports 4 and 8 subbands, 4, 8, 12, 16 blocks, Mono/Dual Channel/Stereo/Joint Stereo,
 * Loudness and SNR bit allocation, and sample rates 16k, 32k, 44.1k, 48k.
 */

#ifndef SYN_SBC_H
#define SYN_SBC_H

#include "../common/syn_defs.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief SBC Frame Synchronization Byte (0x9C) */
#define SYN_SBC_SYNCWORD 0x9CU

/** @brief SBC Channel Mode Enums */
typedef enum {
    SYN_SBC_MODE_MONO = 0,
    SYN_SBC_MODE_DUAL_CHANNEL = 1,
    SYN_SBC_MODE_STEREO = 2,
    SYN_SBC_MODE_JOINT_STEREO = 3
} SYN_SBC_ChannelMode;

/** @brief SBC Allocation Method Enums */
typedef enum { SYN_SBC_ALLOC_LOUDNESS = 0, SYN_SBC_ALLOC_SNR = 1 } SYN_SBC_AllocMethod;

/**
 * @brief SBC Parsed Frame Header Information.
 */
typedef struct {
    uint8_t sample_rate_enum;  /**< 0: 16kHz, 1: 32kHz, 2: 44.1kHz, 3: 48kHz */
    uint16_t sample_rate_hz;   /**< Sample rate in Hz                        */
    SYN_SBC_ChannelMode mode;  /**< Channel mode                             */
    uint8_t channels;          /**< Channel count (1 or 2)                   */
    uint8_t blocks;            /**< Block count (4, 8, 12, 16)               */
    uint8_t subbands;          /**< Subband count (4 or 8)                   */
    SYN_SBC_AllocMethod alloc; /**< Allocation method                        */
    uint8_t bitpool;           /**< Bitpool value (1-250)                    */
    uint8_t join;              /**< Joint stereo subband mask                */
    uint16_t frame_len;        /**< Total frame length in bytes              */
} SYN_SBC_FrameInfo;

/**
 * @brief SBC Decoder Instance State (~1.5 KB static memory).
 */
typedef struct {
    SYN_SBC_FrameInfo info;         /**< Active frame parameters                  */
    int32_t V[2][160];              /**< Synthesis filter subband delay line      */
    int32_t scale_factors[2][8];    /**< Decoded scale factors                    */
    int32_t audio_sample[16][2][8]; /**< Reconstructed subband samples       */
} SYN_SBC_Decoder;

/**
 * @brief Initialize an SBC decoder instance.
 * @param dec Decoder context pointer.
 */
void syn_sbc_decoder_init(SYN_SBC_Decoder *dec);

/**
 * @brief Parse an SBC frame header without decoding audio payload.
 *
 * @param data Input byte buffer containing SBC stream.
 * @param len  Length of input buffer in bytes.
 * @param info Output frame header structure.
 * @return SYN_OK on success, or SYN_INVALID_PARAM / SYN_ERROR if syncword missing or invalid.
 */
SYN_Status syn_sbc_parse_header(const uint8_t *data, size_t len, SYN_SBC_FrameInfo *info);

/**
 * @brief Decode a single SBC audio frame to 16-bit interleaved PCM samples.
 *
 * @param dec         Decoder context pointer.
 * @param in          Input SBC frame buffer starting at syncword 0x9C.
 * @param in_len      Input frame buffer length.
 * @param pcm_out     Output 16-bit PCM buffer (interleaved stereo or mono).
 * @param pcm_cap     Capacity of output PCM buffer in samples.
 * @param out_samples Output pointer receiving actual decoded sample count.
 * @return SYN_OK on successful frame decode, or error code (<0).
 */
SYN_Status syn_sbc_decode_frame(SYN_SBC_Decoder *dec, const uint8_t *in, size_t in_len,
                                int16_t *pcm_out, size_t pcm_cap, size_t *out_samples);

#ifdef __cplusplus
}
#endif

#endif /* SYN_SBC_H */
