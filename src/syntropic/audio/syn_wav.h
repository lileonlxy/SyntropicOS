/**
 * @file syn_wav.h
 * @brief Zero-Heap RIFF/WAVE Header Parser.
 * @ingroup syn_audio
 *
 * Provides a pure C99 zero-malloc streaming header parser for standard `.wav`
 * audio container files. Supports PCM (Linear) and IMA-ADPCM compressed formats.
 */

#ifndef SYN_WAV_H
#define SYN_WAV_H

#include "../common/syn_defs.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** WAVE format tags */
#define SYN_WAV_FORMAT_PCM 0x0001U       /**< Uncompressed PCM audio format */
#define SYN_WAV_FORMAT_IMA_ADPCM 0x0011U /**< IMA ADPCM compressed audio format */

/**
 * @brief Parsed WAV Header Metadata.
 */
typedef struct {
    uint16_t audio_format;    /**< Format tag (1 = PCM, 17 = IMA ADPCM)          */
    uint16_t num_channels;    /**< Number of channels (1 = Mono, 2 = Stereo)     */
    uint32_t sample_rate;     /**< Sampling rate in Hz                           */
    uint32_t byte_rate;       /**< Average bytes per second                      */
    uint16_t block_align;     /**< Block alignment size in bytes                 */
    uint16_t bits_per_sample; /**< Bits per sample (8, 16, 4)                    */
    size_t data_offset;       /**< Byte offset in buffer where PCM/audio starts */
    uint32_t data_size;       /**< Size of data chunk in bytes                   */
    uint32_t total_samples;   /**< Calculated total sample count                 */
} SYN_WAV_Info;

/**
 * @brief Parse a RIFF/WAVE header from a memory buffer.
 *
 * @param buffer      Pointer to input byte array (at least 44 bytes).
 * @param buffer_size Size of available buffer bytes.
 * @param info        Pointer to SYN_WAV_Info output struct.
 * @return SYN_OK on success, SYN_INVALID_PARAM, or SYN_ERROR on parse fail.
 */
SYN_Status syn_wav_parse_header(const uint8_t *buffer, size_t buffer_size, SYN_WAV_Info *info);

#ifdef __cplusplus
}
#endif

#endif /* SYN_WAV_H */
