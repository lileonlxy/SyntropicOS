/**
 * @file syn_goertzel.h
 * @brief Integer-Only Goertzel Algorithm Single-Frequency Tone Detector.
 * @ingroup syn_dsp
 *
 * Provides a zero-malloc fixed-point implementation of the Goertzel algorithm
 * for efficient single-frequency magnitude evaluation and DTMF tone decoding
 * on resource-constrained microcontrollers without running full FFTs.
 */

#ifndef SYN_GOERTZEL_H
#define SYN_GOERTZEL_H

#include "../common/syn_defs.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Goertzel Filter Instance State.
 */
typedef struct {
    int32_t coeff_q15; /**< Precomputed 2 * cos(2 * pi * k / N) in Q15 format */
    int32_t s1;        /**< Filter delay element s[n - 1]                     */
    int32_t s2;        /**< Filter delay element s[n - 2]                     */
    size_t n_count;    /**< Accumulated sample count in current block        */
    size_t n_total;    /**< Target block size N                               */
} SYN_Goertzel;

/**
 * @brief Initialize a Goertzel Filter instance.
 *
 * @param g               Pointer to Goertzel instance.
 * @param sample_rate_hz  Sampling frequency in Hz (e.g. 8000).
 * @param target_freq_hz Target tone frequency in Hz (e.g. 697 for DTMF row 1).
 * @param block_size      Sample block size N (e.g. 205).
 * @return SYN_OK on success, or SYN_INVALID_PARAM.
 */
SYN_Status syn_goertzel_init(SYN_Goertzel *g, uint32_t sample_rate_hz, uint32_t target_freq_hz,
                             size_t block_size);

/**
 * @brief Reset Goertzel filter delay accumulators for a new sample block.
 *
 * @param g Pointer to Goertzel instance.
 * @return SYN_OK on success, or SYN_INVALID_PARAM.
 */
SYN_Status syn_goertzel_reset(SYN_Goertzel *g);

/**
 * @brief Process a single 16-bit PCM sample through the Goertzel filter.
 *
 * @param g      Pointer to Goertzel instance.
 * @param sample 16-bit signed PCM input sample.
 * @return SYN_OK on success, or SYN_INVALID_PARAM.
 */
SYN_Status syn_goertzel_process_sample(SYN_Goertzel *g, int16_t sample);

/**
 * @brief Process a buffer of 16-bit PCM samples through the Goertzel filter.
 *
 * @param g       Pointer to Goertzel instance.
 * @param samples Array of 16-bit signed PCM input samples.
 * @param count   Sample count in array.
 * @return Number of samples actually processed.
 */
size_t syn_goertzel_process_block(SYN_Goertzel *g, const int16_t *samples, size_t count);

/**
 * @brief Calculate the squared magnitude |X(k)|^2 at the end of a sample block.
 *
 * @param g Pointer to Goertzel instance.
 * @return 64-bit squared magnitude value |X(k)|^2 (or 0 if invalid).
 */
uint64_t syn_goertzel_get_magnitude_sq(const SYN_Goertzel *g);

#ifdef __cplusplus
}
#endif

#endif /* SYN_GOERTZEL_H */
