#include "syntropic/dsp/syn_goertzel.h"
#include "unity/unity.h"

#include <math.h>
#include <string.h>

static SYN_Goertzel g_goertzel;

void test_goertzel_init_and_null_checks(void)
{
    int16_t sample = 100;
    int16_t block[4] = {10, 20, 30, 40};

    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_goertzel_init(NULL, 8000, 697, 205));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_goertzel_init(&g_goertzel, 0, 697, 205));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_goertzel_init(&g_goertzel, 8000, 0, 205));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_goertzel_init(&g_goertzel, 8000, 697, 0));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_goertzel_init(&g_goertzel, 8000, 4500, 205)); /* Nyquist overflow */

    TEST_ASSERT_EQUAL(SYN_OK, syn_goertzel_init(&g_goertzel, 8000, 697, 205));

    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_goertzel_reset(NULL));

    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_goertzel_process_sample(NULL, sample));
    TEST_ASSERT_EQUAL(0, syn_goertzel_process_block(NULL, block, 4));
    TEST_ASSERT_EQUAL(0, syn_goertzel_process_block(&g_goertzel, NULL, 4));
    TEST_ASSERT_EQUAL(0, syn_goertzel_process_block(&g_goertzel, block, 0));

    TEST_ASSERT_EQUAL_UINT64(0ULL, syn_goertzel_get_magnitude_sq(NULL));
}

void test_goertzel_dtmf_tone_detection(void)
{
    /* Sampling frequency 8000 Hz, DTMF Row 1 tone = 697 Hz, Block size N = 205 */
    uint32_t fs = 8000;
    uint32_t target_freq = 697;
    size_t n = 205;

    SYN_Goertzel g_matched;
    SYN_Goertzel g_unmatched;

    TEST_ASSERT_EQUAL(SYN_OK, syn_goertzel_init(&g_matched, fs, target_freq, n)); /* 697 Hz */
    TEST_ASSERT_EQUAL(SYN_OK, syn_goertzel_init(&g_unmatched, fs, 1477, n));      /* 1477 Hz */

    /* Generate 205 samples of 697 Hz sine wave: x[i] = 10000 * sin(2 * pi * 697 * i / 8000) */
    int16_t pcm_697[205];
    for (size_t i = 0; i < n; i++) {
        double angle = 2.0 * 3.141592653589793 * 697.0 * (double)i / 8000.0;
        pcm_697[i] = (int16_t)(10000.0 * sin(angle));
    }

    TEST_ASSERT_EQUAL(n, syn_goertzel_process_block(&g_matched, pcm_697, n));
    TEST_ASSERT_EQUAL(n, syn_goertzel_process_block(&g_unmatched, pcm_697, n));

    uint64_t mag_matched = syn_goertzel_get_magnitude_sq(&g_matched);
    uint64_t mag_unmatched = syn_goertzel_get_magnitude_sq(&g_unmatched);

    /* Matched target frequency power should be significantly higher than out-of-band tone power */
    TEST_ASSERT_TRUE(mag_matched > mag_unmatched * 10ULL);

    /* Reset test */
    TEST_ASSERT_EQUAL(SYN_OK, syn_goertzel_reset(&g_matched));
    TEST_ASSERT_EQUAL_INT32(0, g_matched.s1);
    TEST_ASSERT_EQUAL_INT32(0, g_matched.s2);
    TEST_ASSERT_EQUAL(0, g_matched.n_count);
}

void run_goertzel_tests(void)
{
    RUN_TEST(test_goertzel_init_and_null_checks);
    RUN_TEST(test_goertzel_dtmf_tone_detection);
}
