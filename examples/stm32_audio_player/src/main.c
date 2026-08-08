/**
 * @file main.c
 * @brief STM32 Audio Player Example using syn_audio and syn_adpcm.
 * @ingroup syn_examples
 *
 * Demonstrates:
 *  - Decoding IMA-ADPCM compressed audio samples
 *  - Streaming 16-bit linear PCM audio to I2S / SAI / DAC via double-buffered syn_audio engine
 *  - Zero dynamic heap memory allocation
 */

#include "syntropic/audio/syn_adpcm.h"
#include "syntropic/audio/syn_audio.h"
#include "syntropic/syntropic.h"

#include <stdio.h>
#include <string.h>

#define AUDIO_BUF_HALF_SIZE 256U
#define AUDIO_BUF_CAPACITY  (2U * AUDIO_BUF_HALF_SIZE)

static int16_t g_pcm_stream_buf[AUDIO_BUF_CAPACITY];
static SYN_Audio g_audio_engine;
static SYN_ADPCM_State g_adpcm_state;

/**
 * @brief Hardware I2S/SAI DMA transfer trigger callback.
 */
static void audio_hardware_output_cb(const int16_t *samples, size_t count, void *ctx)
{
    (void)samples;
    (void)count;
    (void)ctx;
    /* Stub: Trigger I2S/SAI DMA transfer to external DAC (e.g. CS43L22 / WM8960) */
}

int main(void)
{
    printf("Starting STM32 Embedded Audio Player Example...\n");

    syn_adpcm_init(&g_adpcm_state);

    SYN_Audio_Config cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.buf = g_pcm_stream_buf;
    cfg.buf_capacity = AUDIO_BUF_CAPACITY;
    cfg.half_size = AUDIO_BUF_HALF_SIZE;
    cfg.sample_rate_hz = 16000U;
    cfg.channels = 1U;
    cfg.out_fn = audio_hardware_output_cb;
    cfg.ctx = NULL;

    SYN_Status status = syn_audio_init(&g_audio_engine, &cfg);
    if (status != SYN_OK) {
        printf("ERROR: Failed to initialize audio engine (%d)\n", (int)status);
        return 1;
    }

    syn_audio_start(&g_audio_engine);

    /* Sample 4-bit ADPCM data block */
    const uint8_t sample_adpcm_block[128] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};
    int16_t decoded_pcm[256];

    size_t samples_decoded = syn_adpcm_decode_block(&g_adpcm_state, sample_adpcm_block,
                                                    decoded_pcm, 256U);
    size_t fed = syn_audio_feed(&g_audio_engine, decoded_pcm, samples_decoded);

    printf("Audio stream active: decoded %3zu ADPCM samples, fed %3zu PCM samples to DMA engine.\n",
           samples_decoded, fed);

    syn_audio_stop(&g_audio_engine);
    printf("Audio Player stopped successfully.\n");

    return 0;
}
