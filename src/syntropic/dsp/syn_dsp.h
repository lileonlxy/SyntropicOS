/**
 * @file syn_dsp.h
 * @brief Zero-heap Fixed-Point DSP & Spectral Processing Primitives.
 *
 * Provides fixed-point Discrete Cosine Transform (DCT-II) and signal
 * processing helpers tailored for resource-constrained microcontrollers.
 * @ingroup syn_dsp
 */

#ifndef SYN_DSP_H
#define SYN_DSP_H

#include "../util/syn_qmath.h"
#include "syntropic/common/syn_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Compute Type-II Discrete Cosine Transform (DCT-II) on Q7 data.
 *
 * Transforms time-domain sensor samples into frequency-domain coefficients.
 * Formula: X_k = Sum_{n=0}^{N-1} x_n * cos(pi/N * (n + 0.5) * k)
 *
 * @param time_series Pointer to input time-domain sample vector (length = num_samples).
 * @param num_samples Number of time-domain input samples.
 * @param dct_coeffs  Destination vector for frequency coefficients (length = num_coeffs).
 * @param num_coeffs  Number of output frequency coefficients to retain (num_coeffs <= num_samples).
 * @return SYN_OK on success, SYN_INVALID_PARAM on failure.
 */
SYN_Status syn_dsp_dct2_q7(const q7_t *time_series, size_t num_samples, q7_t *dct_coeffs,
                           size_t num_coeffs);

#ifdef __cplusplus
}
#endif

#endif /* SYN_DSP_H */
