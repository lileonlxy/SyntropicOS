#include "syntropic/crypto/syn_aes128.h"
#include "unity/unity.h"

#include <string.h>

void test_aes128_nist_vector(void)
{
    /* FIPS 197 NIST Test Vector */
    const uint8_t key[16] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                             0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
    const uint8_t plain[16] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
                               0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
    const uint8_t expected_cipher[16] = {0x69, 0xc4, 0xe0, 0xd8, 0x6a, 0x7b, 0x04, 0x30,
                                         0xd8, 0xcd, 0xb7, 0x80, 0x70, 0xb4, 0xc5, 0x5a};

    SYN_AES128_Context ctx;
    TEST_ASSERT_EQUAL(SYN_OK, syn_aes128_init(&ctx, key));

    uint8_t cipher[16] = {0};
    syn_aes128_encrypt_block(&ctx, plain, cipher);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_cipher, cipher, 16);

    uint8_t decrypted[16] = {0};
    syn_aes128_decrypt_block(&ctx, cipher, decrypted);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(plain, decrypted, 16);
}

void test_aes128_cbc_roundtrip(void)
{
    const uint8_t key[16] = {'S', 'y', 'n', 't', 'r', 'o', 'p', 'i',
                             'c', 'O', 'S', 'K', 'e', 'y', '1', '2'};
    const uint8_t iv[16] = {'I', 'n', 'i', 't', 'V', 'e', 'c', 't',
                            'o', 'r', '1', '2', '3', '4', '5', '6'};

    const char *msg = "SyntropicOS AES-128-CBC secure message test vector!";
    size_t msg_len = strlen(msg);

    SYN_AES128_Context ctx;
    TEST_ASSERT_EQUAL(SYN_OK, syn_aes128_init(&ctx, key));

    uint8_t cipher[128] = {0};
    size_t cipher_len = 0;
    TEST_ASSERT_EQUAL(SYN_OK, syn_aes128_cbc_encrypt(&ctx, iv, (const uint8_t *)msg, msg_len,
                                                     cipher, sizeof(cipher), &cipher_len));

    TEST_ASSERT_TRUE(cipher_len > msg_len);
    TEST_ASSERT_EQUAL(0, cipher_len % 16);

    uint8_t plain[128] = {0};
    size_t plain_len = 0;
    TEST_ASSERT_EQUAL(SYN_OK, syn_aes128_cbc_decrypt(&ctx, iv, cipher, cipher_len, plain,
                                                     sizeof(plain), &plain_len));

    TEST_ASSERT_EQUAL_size_t(msg_len, plain_len);
    TEST_ASSERT_EQUAL_MEMORY(msg, plain, msg_len);
}

static void test_aes128_cbc_capacity_and_padding_errors(void)
{
    const uint8_t key[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    const uint8_t iv[16] = {0};
    const uint8_t msg[16] = {'T', 'e', 's', 't', 'M', 'e', 's', 's',
                             'a', 'g', 'e', '1', '2', '3', '4', '5'};
    SYN_AES128_Context ctx;
    syn_aes128_init(&ctx, key);

    uint8_t cipher[32];
    size_t cipher_len;

    /* Encrypt capacity error (line 294) */
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_aes128_cbc_encrypt(&ctx, iv, msg, 16, cipher, 5, &cipher_len));

    /* Decrypt capacity and invalid len error (line 338) */
    uint8_t plain[32];
    size_t plain_len;
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_aes128_cbc_decrypt(&ctx, iv, cipher, 0, plain,
                                                                sizeof(plain), &plain_len));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_aes128_cbc_decrypt(&ctx, iv, cipher, 16, plain, 5, &plain_len));

    /* Valid encrypt with 14-byte message (pad_len = 2) */
    const uint8_t msg14[14] = {'T', 'e', 's', 't', 'M', 'e', 's',
                               's', 'a', 'g', 'e', '1', '2', '3'};
    syn_aes128_cbc_encrypt(&ctx, iv, msg14, 14, cipher, sizeof(cipher), &cipher_len);

    /* Corrupt last byte to trigger pad_val error (line 360) */
    uint8_t bad_cipher[32];
    memcpy(bad_cipher, cipher, cipher_len);
    bad_cipher[cipher_len - 1] ^= 0x01;
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_aes128_cbc_decrypt(&ctx, iv, bad_cipher, cipher_len,
                                                                plain, sizeof(plain), &plain_len));

    /* Corrupt second-to-last byte to trigger pad mismatch in loop (line 365) */
    memcpy(bad_cipher, cipher, cipher_len);
    bad_cipher[cipher_len - 2] ^= 0x01;
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_aes128_cbc_decrypt(&ctx, iv, bad_cipher, cipher_len,
                                                                plain, sizeof(plain), &plain_len));
}

void run_aes128_tests(void)
{
    RUN_TEST(test_aes128_nist_vector);
    RUN_TEST(test_aes128_cbc_roundtrip);
    RUN_TEST(test_aes128_cbc_capacity_and_padding_errors);
}
