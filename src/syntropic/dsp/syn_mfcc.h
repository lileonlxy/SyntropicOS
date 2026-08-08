/**
 * @file syn_mfcc.h
 * @brief Zero-Heap Fixed-Point Mel-Frequency Cepstral Coefficients (MFCC) Extractor.
 * @ingroup syn_dsp
 *
 * Extracts 13 Mel-Frequency Cepstral Coefficients (MFCC) from raw 16-bit PCM audio
 * using fixed-point Q15 arithmetic for microcontrollers running keyword spotting (KWS)
 * and acoustic anomaly detection with syn_nn.
 */

#ifndef SYN_MFCC_H
#define SYN_MFCC_H

#include "syntropic/common/syn_defs.h"
#include "syntropic/dsp/syn_fft.h"
#include "syntropic/util/syn_dsp.h"
#include "syntropic/util/syn_qmath.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SYN_MFCC_MAX_FFT_SIZE 128U   /**< Maximum supported FFT length (128 samples) */
#define SYN_MFCC_NUM_MEL_FILTERS 26U /**< Standard Mel filterbank count (26 filters) */
#define SYN_MFCC_NUM_COEFFS 13U      /**< Retained MFCC cepstral coefficient count (13) */

/** MFCC Extractor Configuration and Workspace Context */
typedef struct {
    uint16_t sample_rate_hz;                      /**< Audio sampling rate in Hz (e.g. 16000) */
    uint16_t fft_size;                            /**< FFT length (must be <= 128, power of 2) */
    q16_t fft_real[SYN_MFCC_MAX_FFT_SIZE];        /**< Internal FFT real workspace */
    q16_t fft_imag[SYN_MFCC_MAX_FFT_SIZE];        /**< Internal FFT imag workspace */
    q16_t mel_energies[SYN_MFCC_NUM_MEL_FILTERS]; /**< Mel filterbank log energies */
    q7_t mfcc_coeffs[SYN_MFCC_NUM_COEFFS];        /**< Output 13 Q7 MFCC feature vector */
} SYN_MFCC;

/**
 * @brief Initialize MFCC Extractor Context.
 * @param mfcc Pointer to MFCC context.
 * @param sample_rate_hz Audio sample rate in Hz.
 * @param fft_size FFT window size (must be 64 or 128).
 * @return SYN_OK on success, SYN_INVALID_PARAM if invalid.
 */
SYN_Status syn_mfcc_init(SYN_MFCC *mfcc, uint16_t sample_rate_hz, uint16_t fft_size);

/**
 * @brief Process one window of 16-bit PCM audio samples and compute 13 Q7 MFCC coefficients.
 * @param mfcc Pointer to MFCC context.
 * @param pcm_in Array of 16-bit PCM input samples (length = fft_size).
 * @param mfcc_out Output buffer for 13 Q7 MFCC coefficients.
 * @return SYN_OK on success, SYN_INVALID_PARAM if NULL.
 */
SYN_Status syn_mfcc_process_frame(SYN_MFCC *mfcc, const int16_t *pcm_in,
                                  q7_t mfcc_out[SYN_MFCC_NUM_COEFFS]);

#ifdef __cplusplus
}
#endif

#endif /* SYN_MFCC_H */
