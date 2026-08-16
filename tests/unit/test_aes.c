#include "syntropic/crypto/syn_aes.h"
#include "unity/unity.h"

#include <string.h>

/* ── AES ECB NIST Test Vectors (FIPS 197) ────────────────────────────────── */

void test_aes_ecb_nist_vectors(void)
{
    /* FIPS 197 Appendix B (AES-128) */
    const uint8_t key128[16] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                                0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
    const uint8_t plain[16] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
                               0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
    const uint8_t exp_cipher128[16] = {0x69, 0xc4, 0xe0, 0xd8, 0x6a, 0x7b, 0x04, 0x30,
                                       0xd8, 0xcd, 0xb7, 0x80, 0x70, 0xb4, 0xc5, 0x5a};

    SYN_AES_Context ctx;
    TEST_ASSERT_EQUAL(SYN_OK, syn_aes_init(&ctx, key128, sizeof(key128)));
    TEST_ASSERT_EQUAL_UINT8(10, ctx.nr);

    uint8_t cipher[16] = {0};
    syn_aes_encrypt_block(&ctx, plain, cipher);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(exp_cipher128, cipher, 16);

    uint8_t decrypted[16] = {0};
    syn_aes_decrypt_block(&ctx, cipher, decrypted);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(plain, decrypted, 16);

    /* FIPS 197 Appendix C.2 (AES-192) */
    const uint8_t key192[24] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                                0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
                                0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17};
    const uint8_t exp_cipher192[16] = {0xdd, 0xa9, 0x7c, 0xa4, 0x86, 0x4c, 0xdf, 0xe0,
                                       0x6e, 0xaf, 0x70, 0xa0, 0xec, 0x0d, 0x71, 0x91};

    TEST_ASSERT_EQUAL(SYN_OK, syn_aes_init(&ctx, key192, sizeof(key192)));
    TEST_ASSERT_EQUAL_UINT8(12, ctx.nr);

    memset(cipher, 0, sizeof(cipher));
    syn_aes_encrypt_block(&ctx, plain, cipher);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(exp_cipher192, cipher, 16);

    memset(decrypted, 0, sizeof(decrypted));
    syn_aes_decrypt_block(&ctx, cipher, decrypted);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(plain, decrypted, 16);

    /* FIPS 197 Appendix C.3 (AES-256) */
    const uint8_t key256[32] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a,
                                0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
                                0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f};
    const uint8_t exp_cipher256[16] = {0x8e, 0xa2, 0xb7, 0xca, 0x51, 0x67, 0x45, 0xbf,
                                       0xea, 0xfc, 0x49, 0x90, 0x4b, 0x49, 0x60, 0x89};

    TEST_ASSERT_EQUAL(SYN_OK, syn_aes_init(&ctx, key256, sizeof(key256)));
    TEST_ASSERT_EQUAL_UINT8(14, ctx.nr);

    memset(cipher, 0, sizeof(cipher));
    syn_aes_encrypt_block(&ctx, plain, cipher);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(exp_cipher256, cipher, 16);

    memset(decrypted, 0, sizeof(decrypted));
    syn_aes_decrypt_block(&ctx, cipher, decrypted);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(plain, decrypted, 16);
}

/* ── AES CBC NIST Test Vectors (NIST SP 800-38A) ─────────────────────────── */

void test_aes_cbc_nist_vectors(void)
{
    const uint8_t key128[16] = {0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
                                0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c};
    const uint8_t iv[16] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                            0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};

    const uint8_t plain[64] = {0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96, 0xe9, 0x3d, 0x7e,
                               0x11, 0x73, 0x93, 0x17, 0x2a, 0xae, 0x2d, 0x8a, 0x57, 0x1e, 0x03,
                               0xac, 0x9c, 0x9e, 0xb7, 0x6f, 0xac, 0x45, 0xaf, 0x8e, 0x51, 0x30,
                               0xc8, 0x1c, 0x46, 0xa3, 0x5c, 0xe4, 0x11, 0xe5, 0xfb, 0xc1, 0x19,
                               0x1a, 0x0a, 0x52, 0xef, 0xf6, 0x9f, 0x24, 0x45, 0xdf, 0x4f, 0x9b,
                               0x17, 0xad, 0x2b, 0x41, 0x7b, 0xe6, 0x6c, 0x37, 0x10};

    const uint8_t exp_cipher128[64] = {
        0x76, 0x49, 0xab, 0xac, 0x81, 0x19, 0xb2, 0x46, 0xce, 0xe9, 0x8e, 0x9b, 0x12,
        0xe9, 0x19, 0x7d, 0x50, 0x86, 0xcb, 0x9b, 0x50, 0x72, 0x19, 0xee, 0x95, 0xdb,
        0x11, 0x3a, 0x91, 0x76, 0x78, 0xb2, 0x73, 0xbe, 0xd6, 0xb8, 0xe3, 0xc1, 0x74,
        0x3b, 0x71, 0x16, 0xe6, 0x9e, 0x22, 0x22, 0x95, 0x16, 0x3f, 0xf1, 0xca, 0xa1,
        0x68, 0x1f, 0xac, 0x09, 0x12, 0x0e, 0xca, 0x30, 0x75, 0x86, 0xe1, 0xa7};

    SYN_AES_Context ctx;
    TEST_ASSERT_EQUAL(SYN_OK, syn_aes_init(&ctx, key128, sizeof(key128)));

    /* Verify CBC encrypt with PKCS#7 padding */
    uint8_t cipher[128] = {0};
    size_t cipher_len = 0;
    TEST_ASSERT_EQUAL(SYN_OK, syn_aes_cbc_encrypt(&ctx, iv, plain, sizeof(plain), cipher,
                                                  sizeof(cipher), &cipher_len));
    TEST_ASSERT_EQUAL_size_t(80, cipher_len);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(exp_cipher128, cipher, 64);

    /* Verify CBC decrypt */
    uint8_t decrypted[128] = {0};
    size_t plain_len = 0;
    TEST_ASSERT_EQUAL(SYN_OK, syn_aes_cbc_decrypt(&ctx, iv, cipher, cipher_len, decrypted,
                                                  sizeof(decrypted), &plain_len));
    TEST_ASSERT_EQUAL_size_t(64, plain_len);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(plain, decrypted, 64);

    /* Test with 256-bit key (NIST SP 800-38A Section F.2.5) */
    const uint8_t key256[32] = {0x60, 0x3d, 0xeb, 0x10, 0x15, 0xca, 0x71, 0xbe, 0x2b, 0x73, 0xae,
                                0xf0, 0x85, 0x7d, 0x77, 0x81, 0x1f, 0x35, 0x2c, 0x07, 0x3b, 0x61,
                                0x08, 0xd7, 0x2d, 0x98, 0x10, 0xa3, 0x09, 0x14, 0xdf, 0xf4};
    TEST_ASSERT_EQUAL(SYN_OK, syn_aes_init(&ctx, key256, sizeof(key256)));
    TEST_ASSERT_EQUAL(SYN_OK, syn_aes_cbc_encrypt(&ctx, iv, plain, sizeof(plain), cipher,
                                                  sizeof(cipher), &cipher_len));
    TEST_ASSERT_EQUAL_size_t(80, cipher_len);

    uint8_t expected_cbc256[64];
    uint8_t cur_iv[16];
    memcpy(cur_iv, iv, 16);
    for (int b = 0; b < 4; b++) {
        uint8_t blk[16];
        for (int j = 0; j < 16; j++) {
            blk[j] = (uint8_t)(plain[b * 16 + j] ^ cur_iv[j]);
        }
        syn_aes_encrypt_block(&ctx, blk, expected_cbc256 + b * 16);
        memcpy(cur_iv, expected_cbc256 + b * 16, 16);
    }
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_cbc256, cipher, 64);

    memset(decrypted, 0, sizeof(decrypted));
    TEST_ASSERT_EQUAL(SYN_OK, syn_aes_cbc_decrypt(&ctx, iv, cipher, cipher_len, decrypted,
                                                  sizeof(decrypted), &plain_len));
    TEST_ASSERT_EQUAL_size_t(64, plain_len);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(plain, decrypted, 64);
}

/* ── AES CTR NIST Test Vectors (NIST SP 800-38A) ─────────────────────────── */

void test_aes_ctr_nist_vectors(void)
{
    const uint8_t key128[16] = {0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
                                0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c};
    const uint8_t nonce[16] = {0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
                               0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff};

    const uint8_t plain[64] = {0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96, 0xe9, 0x3d, 0x7e,
                               0x11, 0x73, 0x93, 0x17, 0x2a, 0xae, 0x2d, 0x8a, 0x57, 0x1e, 0x03,
                               0xac, 0x9c, 0x9e, 0xb7, 0x6f, 0xac, 0x45, 0xaf, 0x8e, 0x51, 0x30,
                               0xc8, 0x1c, 0x46, 0xa3, 0x5c, 0xe4, 0x11, 0xe5, 0xfb, 0xc1, 0x19,
                               0x1a, 0x0a, 0x52, 0xef, 0xf6, 0x9f, 0x24, 0x45, 0xdf, 0x4f, 0x9b,
                               0x17, 0xad, 0x2b, 0x41, 0x7b, 0xe6, 0x6c, 0x37, 0x10};

    const uint8_t exp_cipher128[64] = {
        0x87, 0x4d, 0x61, 0x91, 0xb6, 0x20, 0xe3, 0x26, 0x1b, 0xef, 0x68, 0x64, 0x99,
        0x0d, 0xb6, 0xce, 0x98, 0x06, 0xf6, 0x6b, 0x79, 0x70, 0xfd, 0xff, 0x86, 0x17,
        0x18, 0x7b, 0xb9, 0xff, 0xfd, 0xff, 0x5a, 0xe4, 0xdf, 0x3e, 0xdb, 0xd5, 0xd3,
        0x5e, 0x5b, 0x4f, 0x09, 0x02, 0x0d, 0xb0, 0x3e, 0xab, 0x1e, 0x03, 0x1d, 0xda,
        0x2f, 0xbe, 0x03, 0xd1, 0x79, 0x21, 0x70, 0xa0, 0xf3, 0x00, 0x9c, 0xee};

    SYN_AES_Context ctx;
    TEST_ASSERT_EQUAL(SYN_OK, syn_aes_init(&ctx, key128, sizeof(key128)));

    uint8_t cipher[64] = {0};
    TEST_ASSERT_EQUAL(SYN_OK, syn_aes_ctr(&ctx, nonce, plain, sizeof(plain), cipher));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(exp_cipher128, cipher, 64);

    /* Symmetric CTR decryption */
    uint8_t decrypted[64] = {0};
    TEST_ASSERT_EQUAL(SYN_OK, syn_aes_ctr(&ctx, nonce, cipher, sizeof(cipher), decrypted));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(plain, decrypted, 64);

    /* Partial block test (35 bytes) */
    uint8_t part_cipher[35] = {0};
    uint8_t part_plain[35] = {0};
    TEST_ASSERT_EQUAL(SYN_OK, syn_aes_ctr(&ctx, nonce, plain, 35, part_cipher));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(exp_cipher128, part_cipher, 35);
    TEST_ASSERT_EQUAL(SYN_OK, syn_aes_ctr(&ctx, nonce, part_cipher, 35, part_plain));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(plain, part_plain, 35);

    /* Zero length test */
    TEST_ASSERT_EQUAL(SYN_OK, syn_aes_ctr(&ctx, nonce, NULL, 0, NULL));
}

/* ── AES GCM NIST Test Vectors (NIST SP 800-38D) ─────────────────────────── */

void test_aes_gcm_nist_vectors(void)
{
    /* Test Case 1: Empty plaintext, empty AAD, 12-byte IV */
    const uint8_t key1[16] = {0};
    const uint8_t iv1[12] = {0};
    const uint8_t exp_tag1[16] = {0x58, 0xe2, 0xfc, 0xce, 0xfa, 0x7e, 0x30, 0x61,
                                  0x36, 0x7f, 0x1d, 0x57, 0xa4, 0xe7, 0x45, 0x5a};

    SYN_AES_GCM_Context gcm_ctx;
    TEST_ASSERT_EQUAL(SYN_OK, syn_aes_gcm_init(&gcm_ctx, key1, sizeof(key1)));

    uint8_t tag[16] = {0};
    TEST_ASSERT_EQUAL(SYN_OK,
                      syn_aes_gcm_encrypt(&gcm_ctx, iv1, sizeof(iv1), NULL, 0, NULL, 0, NULL, tag));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(exp_tag1, tag, 16);

    /* Verify decryption / authentication */
    TEST_ASSERT_EQUAL(SYN_OK,
                      syn_aes_gcm_decrypt(&gcm_ctx, iv1, sizeof(iv1), NULL, 0, NULL, 0, NULL, tag));

    /* Test Case 2: 16 bytes plaintext, 12-byte IV (NIST SP 800-38D Case 2) */
    const uint8_t plain2[16] = {0};
    const uint8_t exp_cipher2[16] = {0x03, 0x88, 0xda, 0xce, 0x60, 0xb6, 0xa3, 0x92,
                                     0xf3, 0x28, 0xc2, 0xb9, 0x71, 0xb2, 0xfe, 0x78};
    const uint8_t exp_tag2[16] = {0xab, 0x6e, 0x47, 0xd4, 0x2c, 0xec, 0x13, 0xbd,
                                  0xf5, 0x3a, 0x67, 0xb2, 0x12, 0x57, 0xbd, 0xdf};

    uint8_t cipher2[16] = {0};
    TEST_ASSERT_EQUAL(SYN_OK, syn_aes_gcm_encrypt(&gcm_ctx, iv1, sizeof(iv1), NULL, 0, plain2,
                                                  sizeof(plain2), cipher2, tag));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(exp_cipher2, cipher2, 16);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(exp_tag2, tag, 16);

    uint8_t decrypted2[16] = {0};
    TEST_ASSERT_EQUAL(SYN_OK, syn_aes_gcm_decrypt(&gcm_ctx, iv1, sizeof(iv1), NULL, 0, cipher2,
                                                  sizeof(cipher2), decrypted2, tag));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(plain2, decrypted2, 16);

    /* Test Case 4: 60 bytes plaintext, 20 bytes AAD, 12-byte IV */
    const uint8_t key4[16] = {0xfe, 0xff, 0xe9, 0x92, 0x86, 0x65, 0x73, 0x1c,
                              0x6d, 0x6a, 0x8f, 0x94, 0x67, 0x30, 0x83, 0x08};
    const uint8_t iv4[12] = {0xca, 0xfe, 0xba, 0xbe, 0xfa, 0xce,
                             0xdb, 0xad, 0xde, 0xca, 0xf8, 0x88};
    const uint8_t aad4[20] = {0xfe, 0xed, 0xfa, 0xce, 0xde, 0xad, 0xbe, 0xef, 0xfe, 0xed,
                              0xfa, 0xce, 0xde, 0xad, 0xbe, 0xef, 0xab, 0xad, 0xda, 0xd2};
    const uint8_t plain4[60] = {
        0xd9, 0x31, 0x32, 0x25, 0xf8, 0x84, 0x06, 0xe5, 0xa5, 0x59, 0x09, 0xc5, 0xaf, 0xf5, 0x26,
        0x9a, 0x86, 0xa7, 0xa9, 0x53, 0x15, 0x34, 0xf7, 0xda, 0x2e, 0x4c, 0x30, 0x3d, 0x8a, 0x31,
        0x8a, 0x72, 0x1c, 0x3c, 0x0c, 0x95, 0x95, 0x68, 0x09, 0x53, 0x2f, 0xcf, 0x0e, 0x24, 0x49,
        0xa6, 0xb5, 0x25, 0xb1, 0x6a, 0xed, 0xf5, 0xaa, 0x0d, 0xe6, 0x57, 0xba, 0x63, 0x7b, 0x39};

    const uint8_t exp_tag4[16] = {0x5b, 0xc9, 0x4f, 0xbc, 0x32, 0x21, 0xa5, 0xdb,
                                  0x94, 0xfa, 0xe9, 0x5a, 0xe7, 0x12, 0x1a, 0x47};

    TEST_ASSERT_EQUAL(SYN_OK, syn_aes_gcm_init(&gcm_ctx, key4, sizeof(key4)));

    uint8_t cipher4[60] = {0};
    TEST_ASSERT_EQUAL(SYN_OK, syn_aes_gcm_encrypt(&gcm_ctx, iv4, sizeof(iv4), aad4, sizeof(aad4),
                                                  plain4, sizeof(plain4), cipher4, tag));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(exp_tag4, tag, 16);

    uint8_t decrypted4[60] = {0};
    TEST_ASSERT_EQUAL(SYN_OK, syn_aes_gcm_decrypt(&gcm_ctx, iv4, sizeof(iv4), aad4, sizeof(aad4),
                                                  cipher4, sizeof(cipher4), decrypted4, tag));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(plain4, decrypted4, 60);

    /* Authentication failure: Corrupted tag */
    uint8_t bad_tag[16];
    memcpy(bad_tag, tag, 16);
    bad_tag[0] ^= 0x01;
    TEST_ASSERT_EQUAL(SYN_ERROR,
                      syn_aes_gcm_decrypt(&gcm_ctx, iv4, sizeof(iv4), aad4, sizeof(aad4), cipher4,
                                          sizeof(cipher4), decrypted4, bad_tag));

    /* Authentication failure: Tampered ciphertext */
    uint8_t bad_cipher[60];
    memcpy(bad_cipher, cipher4, 60);
    bad_cipher[10] ^= 0x80;
    TEST_ASSERT_EQUAL(SYN_ERROR,
                      syn_aes_gcm_decrypt(&gcm_ctx, iv4, sizeof(iv4), aad4, sizeof(aad4),
                                          bad_cipher, sizeof(bad_cipher), decrypted4, tag));

    /* Authentication failure: Tampered AAD */
    uint8_t bad_aad[20];
    memcpy(bad_aad, aad4, 20);
    bad_aad[0] ^= 0x01;
    TEST_ASSERT_EQUAL(SYN_ERROR,
                      syn_aes_gcm_decrypt(&gcm_ctx, iv4, sizeof(iv4), bad_aad, sizeof(bad_aad),
                                          cipher4, sizeof(cipher4), decrypted4, tag));

    /* Test AES-256 GCM (Empty PT, empty AAD) */
    const uint8_t key256[32] = {0};
    TEST_ASSERT_EQUAL(SYN_OK, syn_aes_gcm_init(&gcm_ctx, key256, sizeof(key256)));
    TEST_ASSERT_EQUAL(SYN_OK,
                      syn_aes_gcm_encrypt(&gcm_ctx, iv1, sizeof(iv1), NULL, 0, NULL, 0, NULL, tag));
    TEST_ASSERT_EQUAL(SYN_OK,
                      syn_aes_gcm_decrypt(&gcm_ctx, iv1, sizeof(iv1), NULL, 0, NULL, 0, NULL, tag));

    /* Test non-12-byte IV (e.g. 16-byte IV) */
    const uint8_t iv16[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    uint8_t out16[16] = {0};
    TEST_ASSERT_EQUAL(SYN_OK, syn_aes_gcm_encrypt(&gcm_ctx, iv16, sizeof(iv16), NULL, 0, plain2,
                                                  sizeof(plain2), out16, tag));
    TEST_ASSERT_EQUAL(SYN_OK, syn_aes_gcm_decrypt(&gcm_ctx, iv16, sizeof(iv16), NULL, 0, out16,
                                                  sizeof(out16), decrypted2, tag));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(plain2, decrypted2, 16);
}

/* ── Error & Parameter Validation Tests ──────────────────────────────────── */

void test_aes_param_validation(void)
{
    SYN_AES_Context ctx;
    const uint8_t key[32] = {1, 2, 3, 4, 5};
    const uint8_t iv[16] = {0};
    uint8_t buf[64] = {0};
    size_t out_len = 0;

    /* NULL checks */
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_aes_init(NULL, key, 16));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_aes_init(&ctx, NULL, 16));

    /* Block encrypt/decrypt null no-ops */
    syn_aes_encrypt_block(NULL, buf, buf);
    syn_aes_encrypt_block(&ctx, NULL, buf);
    syn_aes_encrypt_block(&ctx, buf, NULL);
    syn_aes_decrypt_block(NULL, buf, buf);
    syn_aes_decrypt_block(&ctx, NULL, buf);
    syn_aes_decrypt_block(&ctx, buf, NULL);

    /* Invalid key sizes */
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_aes_init(&ctx, key, 0));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_aes_init(&ctx, key, 15));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_aes_init(&ctx, key, 17));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_aes_init(&ctx, key, 25));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_aes_init(&ctx, key, 33));

    /* CBC parameter validation */
    TEST_ASSERT_EQUAL(SYN_OK, syn_aes_init(&ctx, key, 16));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_aes_cbc_encrypt(NULL, iv, buf, 16, buf, sizeof(buf), &out_len));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_aes_cbc_encrypt(&ctx, NULL, buf, 16, buf, sizeof(buf), &out_len));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_aes_cbc_encrypt(&ctx, iv, NULL, 16, buf, sizeof(buf), &out_len));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_aes_cbc_encrypt(&ctx, iv, buf, 16, NULL, sizeof(buf), &out_len));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_aes_cbc_encrypt(&ctx, iv, buf, 16, buf, sizeof(buf), NULL));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_aes_cbc_encrypt(&ctx, iv, buf, 16, buf, 10,
                                                             &out_len)); /* small out_capacity */

    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_aes_cbc_decrypt(NULL, iv, buf, 16, buf, sizeof(buf), &out_len));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_aes_cbc_decrypt(&ctx, NULL, buf, 16, buf, sizeof(buf), &out_len));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_aes_cbc_decrypt(&ctx, iv, NULL, 16, buf, sizeof(buf), &out_len));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_aes_cbc_decrypt(&ctx, iv, buf, 16, NULL, sizeof(buf), &out_len));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_aes_cbc_decrypt(&ctx, iv, buf, 16, buf, sizeof(buf), NULL));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_aes_cbc_decrypt(&ctx, iv, buf, 0, buf, sizeof(buf), &out_len));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_aes_cbc_decrypt(&ctx, iv, buf, 15, buf, sizeof(buf), &out_len));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_aes_cbc_decrypt(&ctx, iv, buf, 16, buf, 10, &out_len));

    /* CTR parameter validation */
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_aes_ctr(NULL, iv, buf, 16, buf));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_aes_ctr(&ctx, NULL, buf, 16, buf));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_aes_ctr(&ctx, iv, NULL, 16, buf));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_aes_ctr(&ctx, iv, buf, 16, NULL));

    /* GCM parameter validation */
    SYN_AES_GCM_Context gcm_ctx;
    uint8_t tag[16];
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_aes_gcm_init(NULL, key, 16));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_aes_gcm_init(&gcm_ctx, NULL, 16));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_aes_gcm_init(&gcm_ctx, key, 17));

    TEST_ASSERT_EQUAL(SYN_OK, syn_aes_gcm_init(&gcm_ctx, key, 16));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_aes_gcm_encrypt(NULL, iv, 12, NULL, 0, buf, 16, buf, tag));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_aes_gcm_encrypt(&gcm_ctx, NULL, 12, NULL, 0, buf, 16, buf, tag));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_aes_gcm_encrypt(&gcm_ctx, iv, 0, NULL, 0, buf, 16, buf, tag));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_aes_gcm_encrypt(&gcm_ctx, iv, 12, NULL, 1, buf, 16, buf, tag));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_aes_gcm_encrypt(&gcm_ctx, iv, 12, NULL, 0, NULL, 16, buf, tag));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_aes_gcm_encrypt(&gcm_ctx, iv, 12, NULL, 0, buf, 16, NULL, tag));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_aes_gcm_encrypt(&gcm_ctx, iv, 12, NULL, 0, buf, 16, buf, NULL));

    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_aes_gcm_decrypt(NULL, iv, 12, NULL, 0, buf, 16, buf, tag));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_aes_gcm_decrypt(&gcm_ctx, NULL, 12, NULL, 0, buf, 16, buf, tag));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_aes_gcm_decrypt(&gcm_ctx, iv, 0, NULL, 0, buf, 16, buf, tag));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_aes_gcm_decrypt(&gcm_ctx, iv, 12, NULL, 1, buf, 16, buf, tag));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_aes_gcm_decrypt(&gcm_ctx, iv, 12, NULL, 0, NULL, 16, buf, tag));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_aes_gcm_decrypt(&gcm_ctx, iv, 12, NULL, 0, buf, 16, NULL, tag));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_aes_gcm_decrypt(&gcm_ctx, iv, 12, NULL, 0, buf, 16, buf, NULL));
}

void run_aes_tests(void)
{
    RUN_TEST(test_aes_ecb_nist_vectors);
    RUN_TEST(test_aes_cbc_nist_vectors);
    RUN_TEST(test_aes_ctr_nist_vectors);
    RUN_TEST(test_aes_gcm_nist_vectors);
    RUN_TEST(test_aes_param_validation);
}
