/**
 * @file syn_audio_mixer.h
 * @brief Zero-Heap Multi-Channel Q15 Fixed-Point Audio Mixer.
 * @ingroup syn_audio
 *
 * Provides a zero-malloc multi-channel software PCM audio mixer. Blends multiple
 * active 16-bit PCM audio channels into a single output buffer with per-channel gain,
 * master gain, looping support, and Q15 saturation protection against clipping.
 */

#ifndef SYN_AUDIO_MIXER_H
#define SYN_AUDIO_MIXER_H

#include "../common/syn_defs.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef SYN_AUDIO_MIXER_MAX_CHANNELS
#define SYN_AUDIO_MIXER_MAX_CHANNELS 4U
#endif

/** Q15 identity gain (1.0 = 32768) */
#define SYN_AUDIO_GAIN_UNITY 32768U

/**
 * @brief State for a single mixer input channel.
 */
typedef struct {
    const int16_t *buf;  /**< Pointer to PCM sample array                         */
    size_t sample_count; /**< Total sample count in buffer                        */
    size_t pos;          /**< Current read index                                  */
    uint16_t volume_q15; /**< Channel gain (Q15 fixed-point, 32768 = 1.0)       */
    bool active;         /**< True if channel is actively playing                 */
    bool loop;           /**< True to loop playback continuously                  */
} SYN_Audio_Mixer_Channel;

/**
 * @brief Audio Mixer Instance State.
 */
typedef struct {
    SYN_Audio_Mixer_Channel channels[SYN_AUDIO_MIXER_MAX_CHANNELS]; /**< Input channels   */
    uint16_t master_volume_q15;                                     /**< Master gain (Q15)*/
} SYN_Audio_Mixer;

/**
 * @brief Initialize an Audio Mixer instance.
 *
 * @param mixer Pointer to Audio Mixer instance.
 * @return SYN_OK on success, or SYN_INVALID_PARAM.
 */
SYN_Status syn_audio_mixer_init(SYN_Audio_Mixer *mixer);

/**
 * @brief Assign a PCM sample buffer to a mixer channel for playback.
 *
 * @param mixer        Pointer to Audio Mixer instance.
 * @param channel_idx  Channel index (0 .. SYN_AUDIO_MIXER_MAX_CHANNELS - 1).
 * @param pcm_buf      Pointer to 16-bit PCM sample array.
 * @param sample_count Sample count in pcm_buf.
 * @param volume_q15   Channel gain (Q15, 32768 = 1.0).
 * @param loop         True to restart automatically upon reaching buffer end.
 * @return SYN_OK on success, or SYN_INVALID_PARAM.
 */
SYN_Status syn_audio_mixer_play(SYN_Audio_Mixer *mixer, uint8_t channel_idx, const int16_t *pcm_buf,
                                size_t sample_count, uint16_t volume_q15, bool loop);

/**
 * @brief Stop channel playback immediately.
 *
 * @param mixer       Pointer to Audio Mixer instance.
 * @param channel_idx Channel index (0 .. SYN_AUDIO_MIXER_MAX_CHANNELS - 1).
 * @return SYN_OK on success, or SYN_INVALID_PARAM / SYN_ERR_OUT_OF_BOUNDS.
 */
SYN_Status syn_audio_mixer_stop(SYN_Audio_Mixer *mixer, uint8_t channel_idx);

/**
 * @brief Adjust gain for a specific channel.
 *
 * @param mixer       Pointer to Audio Mixer instance.
 * @param channel_idx Channel index (0 .. SYN_AUDIO_MIXER_MAX_CHANNELS - 1).
 * @param volume_q15  New gain value (Q15).
 * @return SYN_OK on success, or SYN_INVALID_PARAM / SYN_ERR_OUT_OF_BOUNDS.
 */
SYN_Status syn_audio_mixer_set_channel_volume(SYN_Audio_Mixer *mixer, uint8_t channel_idx,
                                              uint16_t volume_q15);

/**
 * @brief Adjust master gain across all channels.
 *
 * @param mixer      Pointer to Audio Mixer instance.
 * @param volume_q15 New master gain value (Q15).
 * @return SYN_OK on success, or SYN_INVALID_PARAM.
 */
SYN_Status syn_audio_mixer_set_master_volume(SYN_Audio_Mixer *mixer, uint16_t volume_q15);

/**
 * @brief Render mixed PCM audio into an output sample buffer.
 *
 * Accumulates active channels, applies Q15 scaling, saturates to [-32768, 32767],
 * and writes to out_pcm.
 *
 * @param mixer        Pointer to Audio Mixer instance.
 * @param out_pcm      Output 16-bit PCM buffer pointer.
 * @param sample_count Output sample count to render.
 * @return Number of samples actually generated (0 if no active channels or NULL).
 */
size_t syn_audio_mixer_render(SYN_Audio_Mixer *mixer, int16_t *out_pcm, size_t sample_count);

#ifdef __cplusplus
}
#endif

#endif /* SYN_AUDIO_MIXER_H */
