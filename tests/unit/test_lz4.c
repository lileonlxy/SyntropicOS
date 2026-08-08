#include "syntropic/util/syn_lz4.h"
#include "unity/unity.h"

#include <stdint.h>
#include <string.h>

static SYN_Lz4 g_lz4;

void test_lz4_init_and_null_checks(void)
{
    uint8_t src[16] = {0};
    uint8_t dst[64] = {0};

    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_lz4_init(NULL));
    TEST_ASSERT_EQUAL(SYN_OK, syn_lz4_init(&g_lz4));

    TEST_ASSERT_EQUAL(0, syn_lz4_compress_bound(SIZE_MAX - 10));
    TEST_ASSERT_TRUE(syn_lz4_compress_bound(100) > 100);

    TEST_ASSERT_EQUAL(0, syn_lz4_compress(NULL, src, sizeof(src), dst, sizeof(dst)));
    TEST_ASSERT_EQUAL(0, syn_lz4_compress(&g_lz4, NULL, sizeof(src), dst, sizeof(dst)));
    TEST_ASSERT_EQUAL(0, syn_lz4_compress(&g_lz4, src, sizeof(src), NULL, sizeof(dst)));
    TEST_ASSERT_EQUAL(0, syn_lz4_compress(&g_lz4, src, 0, dst, sizeof(dst)));

    TEST_ASSERT_EQUAL(0, syn_lz4_decompress(NULL, sizeof(src), dst, sizeof(dst)));
    TEST_ASSERT_EQUAL(0, syn_lz4_decompress(src, sizeof(src), NULL, sizeof(dst)));
    TEST_ASSERT_EQUAL(0, syn_lz4_decompress(src, 0, dst, sizeof(dst)));
    TEST_ASSERT_EQUAL(0, syn_lz4_decompress(src, sizeof(src), dst, 0));
}

void test_lz4_repetitive_text_roundtrip(void)
{
    const char *input = "SyntropicOS SyntropicOS SyntropicOS SyntropicOS SyntropicOS "
                        "SyntropicOS SyntropicOS SyntropicOS SyntropicOS SyntropicOS";
    size_t in_len = strlen(input);

    uint8_t compressed[256];
    uint8_t decompressed[256];

    size_t comp_size = syn_lz4_compress(&g_lz4, input, in_len, compressed, sizeof(compressed));
    TEST_ASSERT_TRUE(comp_size > 0);
    TEST_ASSERT_TRUE(comp_size < in_len);

    size_t decomp_size =
        syn_lz4_decompress(compressed, comp_size, decompressed, sizeof(decompressed));
    TEST_ASSERT_EQUAL(in_len, decomp_size);
    TEST_ASSERT_EQUAL_MEMORY(input, decompressed, in_len);
}

void test_lz4_small_payload(void)
{
    const char *small = "Abc";
    size_t in_len = strlen(small);

    uint8_t compressed[32];
    uint8_t decompressed[32];

    size_t comp_size = syn_lz4_compress(&g_lz4, small, in_len, compressed, sizeof(compressed));
    TEST_ASSERT_TRUE(comp_size > 0);

    size_t decomp_size =
        syn_lz4_decompress(compressed, comp_size, decompressed, sizeof(decompressed));
    TEST_ASSERT_EQUAL(in_len, decomp_size);
    TEST_ASSERT_EQUAL_MEMORY(small, decompressed, in_len);
}

void test_lz4_binary_telemetry_roundtrip(void)
{
    uint8_t telemetry[128];
    for (size_t i = 0; i < sizeof(telemetry); i++) {
        telemetry[i] = (uint8_t)(i % 16);
    }

    uint8_t compressed[256];
    uint8_t decompressed[256];

    size_t comp_size =
        syn_lz4_compress(&g_lz4, telemetry, sizeof(telemetry), compressed, sizeof(compressed));
    TEST_ASSERT_TRUE(comp_size > 0);

    size_t decomp_size =
        syn_lz4_decompress(compressed, comp_size, decompressed, sizeof(decompressed));
    TEST_ASSERT_EQUAL(sizeof(telemetry), decomp_size);
    TEST_ASSERT_EQUAL_MEMORY(telemetry, decompressed, sizeof(telemetry));
}

void test_lz4_ultra_long_literal_and_match_roundtrip(void)
{
    /* Generate 600 unique non-matching literal bytes followed by 600 repeated match bytes */
    uint8_t input[1200];
    for (size_t i = 0; i < 600; i++) {
        input[i] = (uint8_t)(i & 0xFF);
    }
    memset(input + 600, 0xAA, 600);

    uint8_t compressed[2048];
    uint8_t decompressed[2048];

    size_t comp_size =
        syn_lz4_compress(&g_lz4, input, sizeof(input), compressed, sizeof(compressed));
    TEST_ASSERT_TRUE(comp_size > 0);

    size_t decomp_size =
        syn_lz4_decompress(compressed, comp_size, decompressed, sizeof(decompressed));
    TEST_ASSERT_EQUAL(sizeof(input), decomp_size);
    TEST_ASSERT_EQUAL_MEMORY(input, decompressed, sizeof(input));
}

void test_lz4_buffer_overflow_and_corrupt_rejection(void)
{
    const char *input = "RepetitiveDataRepetitiveDataRepetitiveDataRepetitiveData";
    size_t in_len = strlen(input);

    uint8_t compressed[256];
    uint8_t decompressed[256];

    size_t comp_size = syn_lz4_compress(&g_lz4, input, in_len, compressed, sizeof(compressed));
    TEST_ASSERT_TRUE(comp_size > 0);

    /* Destination buffer too small during compression */
    TEST_ASSERT_EQUAL(0, syn_lz4_compress(&g_lz4, input, in_len, compressed, 1));
    TEST_ASSERT_EQUAL(0, syn_lz4_compress(&g_lz4, input, in_len, compressed, 5));

    /* Destination buffer too small during decompression */
    TEST_ASSERT_EQUAL(0, syn_lz4_decompress(compressed, comp_size, decompressed, 10));

    /* Truncated compressed payload tests */
    for (size_t truncated_len = 1; truncated_len < comp_size; truncated_len++) {
        syn_lz4_decompress(compressed, truncated_len, decompressed, sizeof(decompressed));
    }

    /* Corrupt match offset = 0 */
    uint8_t corrupt_offset_zero[16] = {0x00, 0x01, 'A', 0x00, 0x00};
    TEST_ASSERT_EQUAL(0, syn_lz4_decompress(corrupt_offset_zero, sizeof(corrupt_offset_zero),
                                            decompressed, sizeof(decompressed)));

    /* Corrupt match offset larger than decompressed history */
    uint8_t corrupt_offset_out[16] = {0x01, 'A', 0x50, 0x00};
    TEST_ASSERT_EQUAL(0, syn_lz4_decompress(corrupt_offset_out, sizeof(corrupt_offset_out),
                                            decompressed, sizeof(decompressed)));

    /* Match length exceeds destination capacity during decompression */
    uint8_t match_overflow[16] = {0x0F, 'A', 0x01, 0x00, 0x0A};
    TEST_ASSERT_EQUAL(0,
                      syn_lz4_decompress(match_overflow, sizeof(match_overflow), decompressed, 5));
}

void test_lz4_short_input_long_literal(void)
{
    uint8_t input_short[4] = {'A', 'B', 'C', 'D'};
    uint8_t compressed[64];
    uint8_t decompressed[64];

    size_t comp_size = syn_lz4_compress(&g_lz4, input_short, 4, compressed, sizeof(compressed));
    TEST_ASSERT_TRUE(comp_size > 0);

    size_t decomp_size =
        syn_lz4_decompress(compressed, comp_size, decompressed, sizeof(decompressed));
    TEST_ASSERT_EQUAL(4, decomp_size);
    TEST_ASSERT_EQUAL_MEMORY(input_short, decompressed, 4);

    uint8_t input_18[18];
    for (size_t i = 0; i < 18; i++) {
        input_18[i] = (uint8_t)i;
    }

    comp_size = syn_lz4_compress(&g_lz4, input_18, 18, compressed, sizeof(compressed));
    TEST_ASSERT_TRUE(comp_size > 0);

    decomp_size = syn_lz4_decompress(compressed, comp_size, decompressed, sizeof(decompressed));
    TEST_ASSERT_EQUAL(18, decomp_size);
    TEST_ASSERT_EQUAL_MEMORY(input_18, decompressed, 18);
}

void run_lz4_tests(void)
{
    RUN_TEST(test_lz4_init_and_null_checks);
    RUN_TEST(test_lz4_repetitive_text_roundtrip);
    RUN_TEST(test_lz4_small_payload);
    RUN_TEST(test_lz4_binary_telemetry_roundtrip);
    RUN_TEST(test_lz4_ultra_long_literal_and_match_roundtrip);
    RUN_TEST(test_lz4_short_input_long_literal);
    RUN_TEST(test_lz4_buffer_overflow_and_corrupt_rejection);
}
