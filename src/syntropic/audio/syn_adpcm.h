/**
 * @file syn_adpcm.h
 * @brief IMA/DVI ADPCM Speech and Audio Codec (Encoder & Decoder).
 * @ingroup syn_audio
 *
 * Implements a lightweight, zero-heap, integer-only IMA-ADPCM codec.
 * Achieves 4:1 compression ratio (16-bit linear PCM <-> 4-bit ADPCM nibbles).
 * Fully compatible with standard WAV IMA-ADPCM files and tools (ffmpeg, Audacity).
 *
 * @par Usage Example
 * @code
 *   SYN_ADPCM_State state;
 *   syn_adpcm_init(&state);
 *   syn_adpcm_decode_block(&state, adpcm_data, pcm_output, sample_count);
 * @endcode
 */

#ifndef SYN_ADPCM_H
#define SYN_ADPCM_H

#include "../common/syn_defs.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief IMA-ADPCM Encoder and Decoder State.
 */
typedef struct {
    int16_t predicted_sample; /**< Current predicted 16-bit PCM sample (-32768 to 32767) */
    int8_t step_index;        /**< Current quantization step index (0 to 88)         */
} SYN_ADPCM_State;

/**
 * @brief Initialize or reset an IMA-ADPCM state context.
 * @param state State context pointer.
 */
void syn_adpcm_init(SYN_ADPCM_State *state);

/**
 * @brief Decode a single 4-bit ADPCM nibble to a 16-bit PCM sample.
 *
 * @param state  State context pointer.
 * @param nibble 4-bit ADPCM code (lower 4 bits used).
 * @return Decoded 16-bit PCM sample.
 */
int16_t syn_adpcm_decode_sample(SYN_ADPCM_State *state, uint8_t nibble);

/**
 * @brief Encode a single 16-bit PCM sample to a 4-bit ADPCM nibble.
 *
 * @param state  State context pointer.
 * @param sample 16-bit linear PCM sample.
 * @return Encoded 4-bit ADPCM nibble (in bits 0-3).
 */
uint8_t syn_adpcm_encode_sample(SYN_ADPCM_State *state, int16_t sample);

/**
 * @brief Decode a buffer of packed 4-bit ADPCM bytes into 16-bit PCM samples.
 *
 * @param state        State context pointer.
 * @param in           Input packed ADPCM byte buffer (2 nibbles per byte, low nibble first).
 * @param out          Output 16-bit PCM sample buffer.
 * @param sample_count Total number of samples to decode.
 * @return Number of samples successfully decoded.
 */
size_t syn_adpcm_decode_block(SYN_ADPCM_State *state, const uint8_t *in, int16_t *out,
                              size_t sample_count);

/**
 * @brief Encode 16-bit PCM samples into packed 4-bit ADPCM bytes.
 *
 * @param state        State context pointer.
 * @param in           Input 16-bit PCM sample buffer.
 * @param out          Output packed ADPCM byte buffer (requires at least (sample_count + 1)/2
 * bytes).
 * @param sample_count Number of samples to encode.
 * @return Number of packed bytes produced.
 */
size_t syn_adpcm_encode_block(SYN_ADPCM_State *state, const int16_t *in, uint8_t *out,
                              size_t sample_count);

#ifdef __cplusplus
}
#endif

#endif /* SYN_ADPCM_H */
