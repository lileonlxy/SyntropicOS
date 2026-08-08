/**
 * @file syn_audio.h
 * @brief Codec-Agnostic PCM Double-Buffered Audio Playback Engine.
 * @ingroup syn_audio
 *
 * Provides a zero-malloc, hardware-decoupled PCM streaming engine designed to
 * feed audio samples to microcontroller DMA, I2S, SAI, or DAC peripherals.
 *
 * Uses double-buffering (ping-pong halves) to allow uninterrupted audio decoding
 * while the hardware transfers the alternate half-buffer to the DAC via DMA.
 */

#ifndef SYN_AUDIO_H
#define SYN_AUDIO_H

#include "../common/syn_defs.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Hardware output callback function signature.
 *
 * Called when a buffer half is ready to be submitted to DMA or DAC hardware.
 *
 * @param samples Pointer to PCM sample array (16-bit linear PCM).
 * @param count   Number of samples in the array.
 * @param ctx     User context pointer.
 */
typedef void (*SYN_Audio_OutputFn)(const int16_t *samples, size_t count, void *ctx);

/**
 * @brief Audio Playback Configuration.
 */
typedef struct {
    int16_t *buf;              /**< Caller-allocated PCM buffer (capacity >= 2 * half_size) */
    size_t buf_capacity;       /**< Total PCM sample capacity of buffer                    */
    size_t half_size;          /**< Sample count per ping-pong buffer half                  */
    uint32_t sample_rate_hz;   /**< Audio sample rate in Hz (e.g. 16000, 44100, 48000)      */
    uint8_t channels;          /**< Channel count (1 for mono, 2 for stereo)               */
    SYN_Audio_OutputFn out_fn; /**< Hardware transfer trigger callback                     */
    void *ctx;                 /**< User context passed to output callback                 */
} SYN_Audio_Config;

/**
 * @brief Audio Playback Engine Instance State.
 */
typedef struct {
    SYN_Audio_Config cfg;         /**< Copy of active configuration                           */
    volatile uint8_t active_half; /**< Active half buffer index (0 or 1)                  */
    volatile size_t fill_pos;     /**< Write position in current half buffer                  */
    volatile bool running;        /**< True when audio streaming is active                    */
    uint32_t underrun_cnt;        /**< Total buffer underrun count                            */
    uint32_t frames_played;       /**< Total buffer halves submitted                          */
} SYN_Audio;

/**
 * @brief Initialize an Audio Streaming Engine instance.
 *
 * @param audio Pointer to Audio instance.
 * @param cfg   Configuration structure pointer.
 * @return SYN_OK on success, or SYN_INVALID_PARAM.
 */
SYN_Status syn_audio_init(SYN_Audio *audio, const SYN_Audio_Config *cfg);

/**
 * @brief Start audio playback streaming.
 * @param audio Pointer to Audio instance.
 * @return SYN_OK on success, or SYN_INVALID_PARAM.
 */
SYN_Status syn_audio_start(SYN_Audio *audio);

/**
 * @brief Stop audio playback streaming.
 * @param audio Pointer to Audio instance.
 * @return SYN_OK on success, or SYN_INVALID_PARAM.
 */
SYN_Status syn_audio_stop(SYN_Audio *audio);

/**
 * @brief Feed decoded PCM audio samples into the stream buffer.
 *
 * @param audio   Pointer to Audio instance.
 * @param samples Input 16-bit PCM sample array.
 * @param count   Number of samples to push.
 * @return Number of samples actually written to the buffer.
 */
size_t syn_audio_feed(SYN_Audio *audio, const int16_t *samples, size_t count);

/**
 * @brief Hardware ISR Handler for DMA Half-Transfer Interrupt.
 *
 * Call this function from your hardware DMA interrupt when half of the circular
 * buffer transfer completes (`SYN_DMA_EVENT_HALF_COMPLETE`).
 *
 * @param audio Pointer to Audio instance.
 */
void syn_audio_isr_half(SYN_Audio *audio);

/**
 * @brief Hardware ISR Handler for DMA Transfer-Complete Interrupt.
 *
 * Call this function from your hardware DMA interrupt when the entire circular
 * buffer transfer completes (`SYN_DMA_EVENT_COMPLETE`).
 *
 * @param audio Pointer to Audio instance.
 */
void syn_audio_isr_complete(SYN_Audio *audio);

#ifdef __cplusplus
}
#endif

#endif /* SYN_AUDIO_H */
