/**
 * @file syn_vad.h
 * @brief Zero-Heap Acoustic Voice Activity Detector (VAD) Engine (Q15/Q16 fixed-point).
 * @ingroup syn_audio
 *
 * Implements a low-latency, deterministic Voice Activity Detector for embedded microcontrollers:
 * - Frame-by-frame acoustic analysis (10ms - 30ms windows of 16-bit PCM audio).
 * - Multi-feature extraction: Short-Time Energy (STE), Zero-Crossing Rate (ZCR), and High/Low
 * Spectral Energy Ratio.
 * - Dynamic background noise floor tracking via Exponential Moving Average (EMA).
 * - Configurable attack onset validation and hangover release timing.
 * - Sensitivity presets (Aggressive, Normal, Sensitive).
 */

#ifndef SYN_VAD_H
#define SYN_VAD_H

#include "../common/syn_defs.h"
#include "../util/syn_qmath.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if !defined(SYN_USE_VAD) || SYN_USE_VAD

#ifdef __cplusplus
extern "C" {
#endif

/* ── Constants & Limits ─────────────────────────────────────────────────── */

#define SYN_VAD_DEFAULT_FRAME_LEN 160U     /**< Default 160 samples (10ms @ 16kHz or 20ms @ 8kHz) */
#define SYN_VAD_DEFAULT_ATTACK_FRAMES 2U   /**< Consecutive active frames required to trigger */
#define SYN_VAD_DEFAULT_HANGOVER_FRAMES 6U /**< Hangover frames to sustain active speech */

/* ── Enums ──────────────────────────────────────────────────────────────── */

/**
 * @brief Voice Activity Detection state.
 */
typedef enum {
    SYN_VAD_STATE_SILENCE = 0, /**< Background noise / silence detected */
    SYN_VAD_STATE_SPEECH = 1   /**< Active voice / speech detected */
} SYN_VAD_State;

/**
 * @brief VAD sensitivity preset level.
 */
typedef enum {
    SYN_VAD_SENSITIVITY_SENSITIVE = 0, /**< High sensitivity (detects soft speech/whispers) */
    SYN_VAD_SENSITIVITY_NORMAL = 1,    /**< Balanced default sensitivity */
    SYN_VAD_SENSITIVITY_AGGRESSIVE = 2 /**< Aggressive noise suppression (strict speech trigger) */
} SYN_VAD_Sensitivity;

/* ── Feature Extraction Representation ──────────────────────────────────── */

/**
 * @brief Extracted audio frame feature metrics.
 */
typedef struct {
    uint32_t energy;        /**< Normalized short-time frame energy */
    uint16_t zcr;           /**< Zero crossing count across frame */
    uint32_t hf_energy;     /**< High-frequency spectral differential energy */
    uint32_t noise_floor;   /**< Current estimated background noise floor */
    bool is_speech_instant; /**< Raw un-smoothed speech decision for current frame */
} SYN_VAD_Features;

/* ── Configuration Descriptor ────────────────────────────────────────────── */

/**
 * @brief VAD configuration descriptor.
 */
typedef struct {
    uint16_t sample_rate_hz;         /**< Audio sample rate in Hz (e.g. 8000, 16000) */
    uint16_t frame_length;           /**< Frame size in samples (e.g. 160, 240, 320) */
    uint8_t attack_frames;           /**< Number of consecutive speech frames to trigger onset */
    uint8_t hangover_frames;         /**< Number of hangover frames to sustain speech state */
    SYN_VAD_Sensitivity sensitivity; /**< Sensitivity preset */
    uint32_t initial_noise_floor;    /**< Initial noise floor estimate */
} SYN_VAD_Config;

/* ── VAD Context Structure ──────────────────────────────────────────────── */

/**
 * @brief Voice Activity Detector instance context.
 */
typedef struct {
    SYN_VAD_Config cfg;   /**< Configuration */
    SYN_VAD_State state;  /**< Current smoothed VAD state */
    uint32_t noise_floor; /**< Adaptive background noise floor */

    uint8_t attack_counter;   /**< Speech onset attack accumulator */
    uint8_t hangover_counter; /**< Hangover release countdown */

    uint32_t energy_threshold_multiplier; /**< Noise-to-speech energy scale factor */
    uint16_t min_zcr_threshold;           /**< Minimum ZCR threshold */
    uint16_t max_zcr_threshold;           /**< Maximum ZCR threshold */
} SYN_VAD;

/* ── Public API ─────────────────────────────────────────────────────────── */

/**
 * @brief Initialize Voice Activity Detector context.
 * @param vad VAD instance pointer.
 * @param cfg Configuration descriptor (or NULL for default 16kHz parameters).
 * @return SYN_OK on success, SYN_INVALID_PARAM on invalid argument.
 */
SYN_Status syn_vad_init(SYN_VAD *vad, const SYN_VAD_Config *cfg);

/**
 * @brief Reset VAD state counters and restore default noise floor.
 * @param vad VAD instance pointer.
 * @return SYN_OK on success.
 */
SYN_Status syn_vad_reset(SYN_VAD *vad);

/**
 * @brief Process single frame of 16-bit signed PCM audio samples.
 * @param vad VAD instance pointer.
 * @param samples Array of 16-bit PCM audio samples.
 * @param num_samples Number of samples in frame (should match configured frame_length).
 * @param out_features Optional pointer to receive extracted frame metrics (can be NULL).
 * @return Current smoothed VAD decision (SYN_VAD_STATE_SPEECH or SYN_VAD_STATE_SILENCE).
 */
SYN_VAD_State syn_vad_process_frame(SYN_VAD *vad, const int16_t *samples, size_t num_samples,
                                    SYN_VAD_Features *out_features);

/**
 * @brief Get current smoothed VAD state without processing a new frame.
 * @param vad VAD instance pointer.
 * @return Current VAD state.
 */
SYN_VAD_State syn_vad_get_state(const SYN_VAD *vad);

/**
 * @brief Adjust VAD sensitivity preset.
 * @param vad VAD instance pointer.
 * @param sensitivity Sensitivity preset level.
 * @return SYN_OK on success.
 */
SYN_Status syn_vad_set_sensitivity(SYN_VAD *vad, SYN_VAD_Sensitivity sensitivity);

#ifdef __cplusplus
}
#endif

#endif /* !defined(SYN_USE_VAD) || SYN_USE_VAD */

#endif /* SYN_VAD_H */
