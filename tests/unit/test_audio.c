#include "syntropic/audio/syn_audio.h"
#include "unity/unity.h"

#include <string.h>

static int16_t g_pcm_buf[256];
static int g_out_cb_calls = 0;
static size_t g_last_cb_count = 0;

static void dummy_audio_output(const int16_t *samples, size_t count, void *ctx)
{
    (void)samples;
    (void)ctx;
    g_out_cb_calls++;
    g_last_cb_count = count;
}

void test_audio_init_and_null_checks(void)
{
    SYN_Audio audio;
    SYN_Audio_Config cfg;

    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_audio_init(NULL, NULL));
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_audio_init(&audio, NULL));

    memset(&cfg, 0, sizeof(cfg));
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_audio_init(&audio, &cfg));

    cfg.buf = g_pcm_buf;
    cfg.buf_capacity = 256;
    cfg.half_size = 128;
    cfg.sample_rate_hz = 44100;
    cfg.channels = 2;
    cfg.out_fn = dummy_audio_output;

    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_audio_init(&audio, &cfg));

    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_audio_start(NULL));
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_audio_stop(NULL));
    TEST_ASSERT_EQUAL_UINT32(0, syn_audio_feed(NULL, NULL, 0));
    syn_audio_isr_half(NULL);
    syn_audio_isr_complete(NULL);
}

void test_audio_start_stop_and_feed(void)
{
    SYN_Audio audio;
    SYN_Audio_Config cfg;
    cfg.buf = g_pcm_buf;
    cfg.buf_capacity = 256;
    cfg.half_size = 128;
    cfg.sample_rate_hz = 16000;
    cfg.channels = 1;
    cfg.out_fn = dummy_audio_output;

    syn_audio_init(&audio, &cfg);

    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_audio_start(&audio));
    TEST_ASSERT_EQUAL_INT(1, g_out_cb_calls);
    TEST_ASSERT_EQUAL_UINT32(128, g_last_cb_count);

    int16_t input_samples[256];
    for (int i = 0; i < 256; i++) {
        input_samples[i] = (int16_t)(i * 10);
    }

    size_t fed = syn_audio_feed(&audio, input_samples, 256);
    TEST_ASSERT_EQUAL_UINT32(256, fed);

    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_audio_stop(&audio));
}

void test_audio_isr_handlers(void)
{
    SYN_Audio audio;
    SYN_Audio_Config cfg;
    cfg.buf = g_pcm_buf;
    cfg.buf_capacity = 256;
    cfg.half_size = 128;
    cfg.sample_rate_hz = 48000;
    cfg.channels = 2;
    cfg.out_fn = dummy_audio_output;

    syn_audio_init(&audio, &cfg);
    syn_audio_start(&audio);

    int prev_calls = g_out_cb_calls;
    syn_audio_isr_half(&audio);
    TEST_ASSERT_EQUAL_INT(prev_calls + 1, g_out_cb_calls);

    syn_audio_isr_complete(&audio);
    TEST_ASSERT_EQUAL_INT(prev_calls + 2, g_out_cb_calls);
}

void run_audio_tests(void)
{
    RUN_TEST(test_audio_init_and_null_checks);
    RUN_TEST(test_audio_start_stop_and_feed);
    RUN_TEST(test_audio_isr_handlers);
}
