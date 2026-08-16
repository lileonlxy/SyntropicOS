/**
 * @file test_cose.c
 * @brief Unit tests for zero-heap CBOR Object Signing and Encryption (COSE - RFC 9052).
 */

#include "syntropic/proto/syn_cose.h"
#include "unity/unity.h"

#include <string.h>

void test_cose_sign1_eddsa_roundtrip_and_tamper(void)
{
    /* Generate Ed25519 keypair */
    uint8_t seed[32] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB,
                        0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
                        0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};
    uint8_t pubkey[32], seckey[32];
    TEST_ASSERT_TRUE(syn_ed25519_create_keypair(pubkey, seckey, seed));

    static const uint8_t payload[] = "Sensor Telemetry: Temperature=21.5C, Humidity=45%";
    static const uint8_t kid[] = "node-01";
    static const uint8_t aad[] = "coap://sensors/temp";

    uint8_t cose_msg[512];
    size_t cose_msg_len = 0;

    /* Sign with EdDSA (-8) */
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_cose_sign1_create(SYN_COSE_ALGO_EDDSA, seckey, pubkey, kid,
                                                        sizeof(kid) - 1, payload,
                                                        sizeof(payload) - 1, aad, sizeof(aad) - 1,
                                                        cose_msg, sizeof(cose_msg), &cose_msg_len));
    TEST_ASSERT_GREATER_THAN(0, cose_msg_len);

    /* Verify with correct public key and AAD */
    SYN_COSE_Sign1Message parsed;
    memset(&parsed, 0, sizeof(parsed));
    TEST_ASSERT_EQUAL_INT(SYN_OK,
                          syn_cose_sign1_verify(cose_msg, cose_msg_len, pubkey, sizeof(pubkey), aad,
                                                sizeof(aad) - 1, &parsed));

    TEST_ASSERT_EQUAL_INT(SYN_COSE_ALGO_EDDSA, parsed.alg);
    TEST_ASSERT_EQUAL(sizeof(kid) - 1, parsed.kid_len);
    TEST_ASSERT_EQUAL_MEMORY(kid, parsed.kid, parsed.kid_len);
    TEST_ASSERT_EQUAL(sizeof(payload) - 1, parsed.payload_len);
    TEST_ASSERT_EQUAL_MEMORY(payload, parsed.payload, parsed.payload_len);
    TEST_ASSERT_EQUAL(64, parsed.signature_len);

    /* Verify without parsed output struct (NULL) */
    TEST_ASSERT_EQUAL_INT(SYN_OK,
                          syn_cose_sign1_verify(cose_msg, cose_msg_len, pubkey, sizeof(pubkey), aad,
                                                sizeof(aad) - 1, NULL));

    /* Verify with mismatched external AAD -> must fail */
    static const uint8_t bad_aad[] = "coap://sensors/wrong";
    TEST_ASSERT_EQUAL_INT(SYN_ERROR,
                          syn_cose_sign1_verify(cose_msg, cose_msg_len, pubkey, sizeof(pubkey),
                                                bad_aad, sizeof(bad_aad) - 1, NULL));

    /* Verify with wrong public key -> must fail */
    uint8_t wrong_pubkey[32];
    memcpy(wrong_pubkey, pubkey, sizeof(pubkey));
    wrong_pubkey[0] ^= 0x01;
    TEST_ASSERT_EQUAL_INT(SYN_ERROR,
                          syn_cose_sign1_verify(cose_msg, cose_msg_len, wrong_pubkey,
                                                sizeof(wrong_pubkey), aad, sizeof(aad) - 1, NULL));

    /* Verify with corrupted signature -> must fail */
    cose_msg[cose_msg_len - 1] ^= 0x01;
    TEST_ASSERT_EQUAL_INT(SYN_ERROR,
                          syn_cose_sign1_verify(cose_msg, cose_msg_len, pubkey, sizeof(pubkey), aad,
                                                sizeof(aad) - 1, NULL));
}

void test_cose_sign1_es256_roundtrip_and_tamper(void)
{
    /* Generate P-256 keypair */
    static const uint8_t PRIV[32] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0,
                                     0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
                                     0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x11,
                                     0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99};
    uint8_t pub_x[32], pub_y[32];
    TEST_ASSERT_TRUE(syn_p256_base_mul(PRIV, pub_x, pub_y));

    uint8_t pubkey_65[65];
    pubkey_65[0] = 0x04;
    memcpy(pubkey_65 + 1, pub_x, 32);
    memcpy(pubkey_65 + 33, pub_y, 32);

    uint8_t pubkey_64[64];
    memcpy(pubkey_64, pub_x, 32);
    memcpy(pubkey_64 + 32, pub_y, 32);

    static const uint8_t payload[] = "Actuator Command: Valve=OPEN";
    static const uint8_t kid[] = "act-p256";

    uint8_t cose_msg[512];
    size_t cose_msg_len = 0;

    /* Sign with ES256 (-7) */
    TEST_ASSERT_EQUAL_INT(SYN_OK,
                          syn_cose_sign1_create(SYN_COSE_ALGO_ES256, PRIV, pubkey_65, kid,
                                                sizeof(kid) - 1, payload, sizeof(payload) - 1, NULL,
                                                0, cose_msg, sizeof(cose_msg), &cose_msg_len));

    /* Verify with 65-byte uncompressed pubkey */
    TEST_ASSERT_EQUAL_INT(
        SYN_OK, syn_cose_sign1_verify(cose_msg, cose_msg_len, pubkey_65, 65, NULL, 0, NULL));

    /* Verify with 64-byte raw pubkey */
    TEST_ASSERT_EQUAL_INT(
        SYN_OK, syn_cose_sign1_verify(cose_msg, cose_msg_len, pubkey_64, 64, NULL, 0, NULL));

    /* Wrong key length -> error */
    TEST_ASSERT_EQUAL_INT(
        SYN_ERROR, syn_cose_sign1_verify(cose_msg, cose_msg_len, pubkey_65, 63, NULL, 0, NULL));

    /* Tampered signature -> error */
    cose_msg[cose_msg_len - 5] ^= 0x01;
    TEST_ASSERT_EQUAL_INT(
        SYN_ERROR, syn_cose_sign1_verify(cose_msg, cose_msg_len, pubkey_65, 65, NULL, 0, NULL));
}

void test_cose_encrypt0_chacha20poly1305_roundtrip_and_tamper(void)
{
    static const uint8_t key[32] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                                    0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
                                    0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
                                    0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20};
    static const uint8_t iv[12] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
                                   0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B};
    static const uint8_t kid[] = "sym-node";
    static const uint8_t plaintext[] = "Confidential Industrial Control Message 0x12345678";
    static const uint8_t aad[] = "coap://valve/control";

    uint8_t cose_msg[512];
    size_t cose_msg_len = 0;

    /* Create COSE_Encrypt0 */
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_cose_encrypt0_create(
                                      SYN_COSE_ALGO_CHACHA20_POLY1305, key, iv, sizeof(iv), kid,
                                      sizeof(kid) - 1, plaintext, sizeof(plaintext) - 1, aad,
                                      sizeof(aad) - 1, cose_msg, sizeof(cose_msg), &cose_msg_len));
    TEST_ASSERT_GREATER_THAN(0, cose_msg_len);

    /* Decrypt COSE_Encrypt0 */
    uint8_t dec_buf[128];
    size_t dec_len = 0;
    SYN_COSE_Encrypt0Message parsed;
    memset(&parsed, 0, sizeof(parsed));

    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_cose_encrypt0_decrypt(cose_msg, cose_msg_len, key, aad,
                                                            sizeof(aad) - 1, dec_buf,
                                                            sizeof(dec_buf), &dec_len, &parsed));

    TEST_ASSERT_EQUAL(sizeof(plaintext) - 1, dec_len);
    TEST_ASSERT_EQUAL_MEMORY(plaintext, dec_buf, dec_len);
    TEST_ASSERT_EQUAL_INT(SYN_COSE_ALGO_CHACHA20_POLY1305, parsed.alg);
    TEST_ASSERT_EQUAL(12, parsed.iv_len);
    TEST_ASSERT_EQUAL_MEMORY(iv, parsed.iv, 12);
    TEST_ASSERT_EQUAL(sizeof(kid) - 1, parsed.kid_len);
    TEST_ASSERT_EQUAL_MEMORY(kid, parsed.kid, parsed.kid_len);

    /* Decrypt with wrong key -> must fail authentication */
    uint8_t bad_key[32];
    memcpy(bad_key, key, 32);
    bad_key[0] ^= 0x01;
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_cose_encrypt0_decrypt(cose_msg, cose_msg_len, bad_key, aad,
                                                               sizeof(aad) - 1, dec_buf,
                                                               sizeof(dec_buf), &dec_len, NULL));

    /* Decrypt with mismatched AAD -> must fail */
    static const uint8_t wrong_aad[] = "coap://valve/wrong";
    TEST_ASSERT_EQUAL_INT(SYN_ERROR,
                          syn_cose_encrypt0_decrypt(cose_msg, cose_msg_len, key, wrong_aad,
                                                    sizeof(wrong_aad) - 1, dec_buf, sizeof(dec_buf),
                                                    &dec_len, NULL));

    /* Corrupted ciphertext -> must fail */
    cose_msg[cose_msg_len - 1] ^= 0x01;
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_cose_encrypt0_decrypt(cose_msg, cose_msg_len, key, aad,
                                                               sizeof(aad) - 1, dec_buf,
                                                               sizeof(dec_buf), &dec_len, NULL));
}

void test_cose_null_and_boundary_checks(void)
{
    uint8_t dummy[64] = {0};
    size_t out_len = 0;

    /* syn_cose_sign1_create null checks */
    TEST_ASSERT_EQUAL_INT(SYN_ERROR,
                          syn_cose_sign1_create(SYN_COSE_ALGO_EDDSA, NULL, dummy, NULL, 0, dummy,
                                                10, NULL, 0, dummy, sizeof(dummy), &out_len));
    TEST_ASSERT_EQUAL_INT(SYN_ERROR,
                          syn_cose_sign1_create(SYN_COSE_ALGO_EDDSA, dummy, dummy, NULL, 0, dummy,
                                                10, NULL, 0, NULL, sizeof(dummy), &out_len));
    TEST_ASSERT_EQUAL_INT(SYN_ERROR,
                          syn_cose_sign1_create(SYN_COSE_ALGO_EDDSA, dummy, dummy, NULL, 0, dummy,
                                                10, NULL, 0, dummy, sizeof(dummy), NULL));
    TEST_ASSERT_EQUAL_INT(SYN_ERROR,
                          syn_cose_sign1_create(SYN_COSE_ALGO_EDDSA, dummy, dummy, NULL, 0, NULL,
                                                10, NULL, 0, dummy, sizeof(dummy), &out_len));
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_cose_sign1_create((SYN_COSE_Algorithm)999, dummy, dummy,
                                                           NULL, 0, dummy, 10, NULL, 0, dummy,
                                                           sizeof(dummy), &out_len));

    /* Buffer too small for sign1 create */
    TEST_ASSERT_EQUAL_INT(SYN_ERROR,
                          syn_cose_sign1_create(SYN_COSE_ALGO_EDDSA, dummy, dummy, NULL, 0, dummy,
                                                10, NULL, 0, dummy, 5, &out_len));

    /* syn_cose_sign1_verify null checks */
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_cose_sign1_verify(NULL, 10, dummy, 32, NULL, 0, NULL));
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_cose_sign1_verify(dummy, 0, dummy, 32, NULL, 0, NULL));
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_cose_sign1_verify(dummy, 10, NULL, 32, NULL, 0, NULL));

    /* Invalid CBOR / non-array message */
    uint8_t non_array_cbor[] = {0x01, 0x02, 0x03};
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_cose_sign1_verify(non_array_cbor, sizeof(non_array_cbor),
                                                           dummy, 32, NULL, 0, NULL));

    /* syn_cose_encrypt0_create null checks */
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_cose_encrypt0_create(SYN_COSE_ALGO_CHACHA20_POLY1305, NULL,
                                                              dummy, 12, NULL, 0, dummy, 10, NULL,
                                                              0, dummy, sizeof(dummy), &out_len));
    TEST_ASSERT_EQUAL_INT(
        SYN_ERROR, syn_cose_encrypt0_create(SYN_COSE_ALGO_CHACHA20_POLY1305, dummy, NULL, 12, NULL,
                                            0, dummy, 10, NULL, 0, dummy, sizeof(dummy), &out_len));
    TEST_ASSERT_EQUAL_INT(
        SYN_ERROR, syn_cose_encrypt0_create(SYN_COSE_ALGO_CHACHA20_POLY1305, dummy, dummy, 10, NULL,
                                            0, dummy, 10, NULL, 0, dummy, sizeof(dummy), &out_len));
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_cose_encrypt0_create((SYN_COSE_Algorithm)999, dummy, dummy,
                                                              12, NULL, 0, dummy, 10, NULL, 0,
                                                              dummy, sizeof(dummy), &out_len));

    /* syn_cose_encrypt0_decrypt null checks */
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_cose_encrypt0_decrypt(NULL, 10, dummy, NULL, 0, dummy,
                                                               sizeof(dummy), &out_len, NULL));
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_cose_encrypt0_decrypt(dummy, 0, dummy, NULL, 0, dummy,
                                                               sizeof(dummy), &out_len, NULL));
    /* Empty plaintext encryption and decryption (payload = NULL, length = 0) */
    uint8_t empty_ct_msg[256];
    size_t empty_ct_len = 0;
    static const uint8_t key32[32] = {0x42};
    static const uint8_t iv12[12] = {0x24};
    TEST_ASSERT_EQUAL_INT(SYN_OK,
                          syn_cose_encrypt0_create(SYN_COSE_ALGO_CHACHA20_POLY1305, key32, iv12, 12,
                                                   NULL, 0, NULL, 0, NULL, 0, empty_ct_msg,
                                                   sizeof(empty_ct_msg), &empty_ct_len));
    uint8_t empty_dec[16];
    size_t empty_dec_len = 99;
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_cose_encrypt0_decrypt(empty_ct_msg, empty_ct_len, key32, NULL,
                                                            0, empty_dec, sizeof(empty_dec),
                                                            &empty_dec_len, NULL));
    TEST_ASSERT_EQUAL(0, empty_dec_len);

    /* Output plaintext buffer too small -> error */
    uint8_t small_dec[2];
    size_t small_dec_len = 0;
    uint8_t non_empty_msg[256];
    size_t non_empty_len = 0;
    static const uint8_t sample_pt[] = "0123456789";
    TEST_ASSERT_EQUAL_INT(SYN_OK,
                          syn_cose_encrypt0_create(SYN_COSE_ALGO_CHACHA20_POLY1305, key32, iv12, 12,
                                                   NULL, 0, sample_pt, 10, NULL, 0, non_empty_msg,
                                                   sizeof(non_empty_msg), &non_empty_len));
    TEST_ASSERT_EQUAL_INT(SYN_ERROR,
                          syn_cose_encrypt0_decrypt(non_empty_msg, non_empty_len, key32, NULL, 0,
                                                    small_dec, 5, &small_dec_len, NULL));

    /* Empty payload signing (payload = NULL, len = 0) */
    uint8_t ed_seed[32] = {0x99};
    uint8_t ed_pub[32], ed_sec[32];
    syn_ed25519_create_keypair(ed_pub, ed_sec, ed_seed);
    uint8_t empty_sign_msg[256];
    size_t empty_sign_len = 0;
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_cose_sign1_create(SYN_COSE_ALGO_EDDSA, ed_sec, ed_pub, NULL,
                                                        0, NULL, 0, NULL, 0, empty_sign_msg,
                                                        sizeof(empty_sign_msg), &empty_sign_len));
    TEST_ASSERT_EQUAL_INT(
        SYN_OK, syn_cose_sign1_verify(empty_sign_msg, empty_sign_len, ed_pub, 32, NULL, 0, NULL));

    /* Invalid protected alg for sign1 (e.g. alg = 99) */
    uint8_t bad_alg_sign1[64] = {
        0x84,                         /* Array of 4 */
        0x44, 0xA1, 0x01, 0x18, 0x63, /* Protected: { 1: 99 } */
        0xA0,                         /* Unprotected: {} */
        0x40,                         /* Payload: "" */
        0x40                          /* Signature */
    };
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_cose_sign1_verify(bad_alg_sign1, sizeof(bad_alg_sign1),
                                                           ed_pub, 32, NULL, 0, NULL));

    /* Invalid protected alg for encrypt0 (e.g. alg = -8) */
    uint8_t bad_alg_enc0[64] = {
        0x83,                                                    /* Array of 3 */
        0x43, 0xA1, 0x01, 0x27,                                  /* Protected: { 1: -8 } */
        0xA1, 0x05, 0x4C, 0,    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* Unprotected: { 5: 12-byte IV }
                                                                  */
        0x50, 0,    0,    0,    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 /* 16-byte tag */
    };
    TEST_ASSERT_EQUAL_INT(
        SYN_ERROR, syn_cose_encrypt0_decrypt(bad_alg_enc0, sizeof(bad_alg_enc0), key32, NULL, 0,
                                             small_dec, sizeof(small_dec), &small_dec_len, NULL));

    /* Invalid IV length (e.g. 8 bytes instead of 12) */
    uint8_t bad_iv_enc0[64] = {
        0x83,                                           /* Array of 3 */
        0x44, 0xA1, 0x01, 0x18, 0x18,                   /* Protected: { 1: 24 } */
        0xA1, 0x05, 0x48, 0,    0,    0, 0, 0, 0, 0, 0, /* Unprotected: { 5: 8-byte IV } */
        0x50, 0,    0,    0,    0,    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 /* 16-byte tag */
    };
    TEST_ASSERT_EQUAL_INT(
        SYN_ERROR, syn_cose_encrypt0_decrypt(bad_iv_enc0, sizeof(bad_iv_enc0), key32, NULL, 0,
                                             small_dec, sizeof(small_dec), &small_dec_len, NULL));

    /* Truncated ciphertext (< 16 bytes) */
    uint8_t short_ct_enc0[64] = {
        0x83,                         /* Array of 3 */
        0x44, 0xA1, 0x01, 0x18, 0x18, /* Protected: { 1: 24 } */
        0xA1, 0x05, 0x4C, 0,    0,    0, 0, 0, 0,
        0,    0,    0,    0,    0,    0,         /* Unprotected: { 5: 12-byte IV } */
        0x48, 0,    0,    0,    0,    0, 0, 0, 0 /* 8-byte ciphertext */
    };
    TEST_ASSERT_EQUAL_INT(
        SYN_ERROR, syn_cose_encrypt0_decrypt(short_ct_enc0, sizeof(short_ct_enc0), key32, NULL, 0,
                                             small_dec, sizeof(small_dec), &small_dec_len, NULL));

    /* Extra/unknown parameters in header test */
    uint8_t custom_sign1[64] = {
        0x84,                               /* Array of 4 */
        0x45, 0xA2, 0x01, 0x27, 0x0A, 0x05, /* Protected: { 1: -8, 10: 5 } */
        0xA1, 0x0B, 0x06,                   /* Unprotected: { 11: 6 } */
        0x40,                               /* Empty payload */
        0x40                                /* Short signature -> fails sig read */
    };
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_cose_sign1_verify(custom_sign1, sizeof(custom_sign1),
                                                           ed_pub, 32, NULL, 0, NULL));

    /* Large payload exceeding 1024-byte sig_structure buffer */
    uint8_t huge_payload[1100];
    memset(huge_payload, 0xAA, sizeof(huge_payload));
    uint8_t huge_out[1200];
    size_t huge_len = 0;
    TEST_ASSERT_EQUAL_INT(SYN_ERROR,
                          syn_cose_sign1_create(SYN_COSE_ALGO_EDDSA, ed_sec, ed_pub, NULL, 0,
                                                huge_payload, sizeof(huge_payload), NULL, 0,
                                                huge_out, sizeof(huge_out), &huge_len));

    /* Large plaintext exceeding 1024-byte ct_tag_buf */
    TEST_ASSERT_EQUAL_INT(SYN_ERROR,
                          syn_cose_encrypt0_create(SYN_COSE_ALGO_CHACHA20_POLY1305, key32, iv12, 12,
                                                   NULL, 0, huge_payload, sizeof(huge_payload),
                                                   NULL, 0, huge_out, sizeof(huge_out), &huge_len));

    /* Small output buffer for encrypt0 create */
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_cose_encrypt0_create(SYN_COSE_ALGO_CHACHA20_POLY1305,
                                                              key32, iv12, 12, NULL, 0, sample_pt,
                                                              10, NULL, 0, huge_out, 5, &huge_len));

    /* Protected header is not a bstr in sign1 */
    uint8_t not_bstr_prot_sign1[] = {0x84, 0x01, 0xA0, 0x40, 0x40};
    TEST_ASSERT_EQUAL_INT(SYN_ERROR,
                          syn_cose_sign1_verify(not_bstr_prot_sign1, sizeof(not_bstr_prot_sign1),
                                                ed_pub, 32, NULL, 0, NULL));

    /* Unprotected header is not a map in sign1 */
    uint8_t not_map_unprot_sign1[] = {0x84, 0x43, 0xA1, 0x01, 0x27, 0x01, 0x40, 0x40};
    TEST_ASSERT_EQUAL_INT(SYN_ERROR,
                          syn_cose_sign1_verify(not_map_unprot_sign1, sizeof(not_map_unprot_sign1),
                                                ed_pub, 32, NULL, 0, NULL));

    /* Payload is not a bstr in sign1 */
    uint8_t not_bstr_payload_sign1[] = {0x84, 0x43, 0xA1, 0x01, 0x27, 0xA0, 0x01, 0x40};
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_cose_sign1_verify(not_bstr_payload_sign1,
                                                           sizeof(not_bstr_payload_sign1), ed_pub,
                                                           32, NULL, 0, NULL));

    /* Protected header is not a bstr in encrypt0 */
    uint8_t not_bstr_prot_enc0[] = {0x83, 0x01, 0xA0, 0x40};
    TEST_ASSERT_EQUAL_INT(SYN_ERROR,
                          syn_cose_encrypt0_decrypt(not_bstr_prot_enc0, sizeof(not_bstr_prot_enc0),
                                                    key32, NULL, 0, small_dec, sizeof(small_dec),
                                                    &small_dec_len, NULL));

    /* Encrypt0 array item count != 3 (e.g. array of 2) */
    uint8_t wrong_array_enc0[] = {0x82, 0x44, 0xA1, 0x01, 0x18, 0x18, 0xA0};
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_cose_encrypt0_decrypt(
                                         wrong_array_enc0, sizeof(wrong_array_enc0), key32, NULL, 0,
                                         small_dec, sizeof(small_dec), &small_dec_len, NULL));

    /* Huge external AAD > 512 bytes for Encrypt0 create -> build_enc_structure fails */
    TEST_ASSERT_EQUAL_INT(
        SYN_ERROR, syn_cose_encrypt0_create(SYN_COSE_ALGO_CHACHA20_POLY1305, key32, iv12, 12, NULL,
                                            0, sample_pt, 10, huge_payload, sizeof(huge_payload),
                                            huge_out, sizeof(huge_out), &huge_len));

    /* EdDSA verify with invalid public key length (!= 32) */
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_cose_sign1_verify(empty_sign_msg, empty_sign_len, ed_pub,
                                                           16, NULL, 0, NULL));

    /* Protected header with extra/unknown labels in Encrypt0 */
    uint8_t extra_prot_enc0[64] = {
        0x83,                                     /* Array of 3 */
        0x45, 0xA2, 0x01, 0x18, 0x18, 0x0A, 0x05, /* Protected: { 1: 24, 10: 5 } */
        0xA1, 0x05, 0x4C, 0,    0,    0,    0,    0, 0, 0, 0, 0, 0, 0, 0,      /* Unprotected: { 5:
                                                                                * 12-byte IV }
                                                                                */
        0x50, 0,    0,    0,    0,    0,    0,    0, 0, 0, 0, 0, 0, 0, 0, 0, 0 /* 16-byte tag */
    };
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_cose_encrypt0_decrypt(
                                         extra_prot_enc0, sizeof(extra_prot_enc0), key32, NULL, 0,
                                         small_dec, sizeof(small_dec), &small_dec_len, NULL));

    /* Unprotected header is not a map in Encrypt0 */
    uint8_t not_map_unprot_enc0[] = {
        0x83,                         /* Array of 3 */
        0x44, 0xA1, 0x01, 0x18, 0x18, /* Protected: { 1: 24 } */
        0x01,                         /* Unprotected: 1 (not a map) */
        0x50, 0,    0,    0,    0,    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 /* 16-byte tag */
    };
    TEST_ASSERT_EQUAL_INT(
        SYN_ERROR,
        syn_cose_encrypt0_decrypt(not_map_unprot_enc0, sizeof(not_map_unprot_enc0), key32, NULL, 0,
                                  small_dec, sizeof(small_dec), &small_dec_len, NULL));

    /* Unprotected header with extra/unknown labels in Encrypt0 */
    uint8_t extra_unprot_enc0[64] = {
        0x83,                                                       /* Array of 3 */
        0x44, 0xA1, 0x01, 0x18, 0x18,                               /* Protected: { 1: 24 } */
        0xA2, 0x05, 0x4C, 0,    0,    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* Unprotected: { 5: 12-byte IV,
                                                                    10: 5 } */
        0x0A, 0x05, 0x50, 0,    0,    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 /* 16-byte tag */
    };
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_cose_encrypt0_decrypt(
                                         extra_unprot_enc0, sizeof(extra_unprot_enc0), key32, NULL,
                                         0, small_dec, sizeof(small_dec), &small_dec_len, NULL));

    /* Huge external AAD > 512 bytes for Encrypt0 decrypt -> build_enc_structure fails */
    uint8_t dec_buf[256];
    size_t dec_len = 0;
    TEST_ASSERT_EQUAL_INT(SYN_ERROR,
                          syn_cose_encrypt0_decrypt(non_empty_msg, non_empty_len, key32,
                                                    huge_payload, sizeof(huge_payload), dec_buf,
                                                    sizeof(dec_buf), &dec_len, NULL));

    /* Huge external AAD > 1024 bytes for Sign1 verify */
    TEST_ASSERT_EQUAL_INT(SYN_ERROR,
                          syn_cose_sign1_verify(empty_sign_msg, empty_sign_len, ed_pub, 32,
                                                huge_payload, sizeof(huge_payload), NULL));
}

void test_cose_encrypt0_aes_gcm_roundtrip_and_tamper(void)
{
    static const uint8_t key128[16] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                                       0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};
    static const uint8_t key192[24] = {0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
                                       0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
                                       0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27};
    static const uint8_t key256[32] = {0xfe, 0xff, 0xe9, 0x92, 0x86, 0x65, 0x73, 0x1c,
                                       0x6d, 0x6a, 0x8f, 0x94, 0x67, 0x30, 0x83, 0x08,
                                       0xfe, 0xff, 0xe9, 0x92, 0x86, 0x65, 0x73, 0x1c,
                                       0x6d, 0x6a, 0x8f, 0x94, 0x67, 0x30, 0x83, 0x08};
    static const uint8_t iv[12] = {0xca, 0xfe, 0xba, 0xbe, 0xfa, 0xce,
                                   0xdb, 0xad, 0xde, 0xca, 0xf8, 0x88};
    static const uint8_t kid[] = "gcm-node-1";
    static const uint8_t plaintext[] =
        "Confidential AES-GCM Encrypted Industrial Telemetry Payload";
    static const uint8_t aad[] = "coap://valve/status";

    uint8_t cose_msg[512];
    size_t cose_msg_len = 0;
    uint8_t dec_buf[128];
    size_t dec_len = 0;
    SYN_COSE_Encrypt0Message parsed;

    /* 1. Test AES-128-GCM (A128GCM, alg = 1) */
    memset(&parsed, 0, sizeof(parsed));
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_cose_encrypt0_create(
                                      SYN_COSE_ALGO_A128GCM, key128, iv, sizeof(iv), kid,
                                      sizeof(kid) - 1, plaintext, sizeof(plaintext) - 1, aad,
                                      sizeof(aad) - 1, cose_msg, sizeof(cose_msg), &cose_msg_len));
    TEST_ASSERT_GREATER_THAN(0, cose_msg_len);

    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_cose_encrypt0_decrypt(cose_msg, cose_msg_len, key128, aad,
                                                            sizeof(aad) - 1, dec_buf,
                                                            sizeof(dec_buf), &dec_len, &parsed));
    TEST_ASSERT_EQUAL(sizeof(plaintext) - 1, dec_len);
    TEST_ASSERT_EQUAL_MEMORY(plaintext, dec_buf, dec_len);
    TEST_ASSERT_EQUAL_INT(SYN_COSE_ALGO_A128GCM, parsed.alg);

    /* 2. Test AES-192-GCM (A192GCM, alg = 2) */
    memset(&parsed, 0, sizeof(parsed));
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_cose_encrypt0_create(
                                      SYN_COSE_ALGO_A192GCM, key192, iv, sizeof(iv), kid,
                                      sizeof(kid) - 1, plaintext, sizeof(plaintext) - 1, aad,
                                      sizeof(aad) - 1, cose_msg, sizeof(cose_msg), &cose_msg_len));
    TEST_ASSERT_GREATER_THAN(0, cose_msg_len);

    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_cose_encrypt0_decrypt(cose_msg, cose_msg_len, key192, aad,
                                                            sizeof(aad) - 1, dec_buf,
                                                            sizeof(dec_buf), &dec_len, &parsed));
    TEST_ASSERT_EQUAL(sizeof(plaintext) - 1, dec_len);
    TEST_ASSERT_EQUAL_MEMORY(plaintext, dec_buf, dec_len);
    TEST_ASSERT_EQUAL_INT(SYN_COSE_ALGO_A192GCM, parsed.alg);

    /* 3. Test AES-256-GCM (A256GCM, alg = 3) */
    memset(&parsed, 0, sizeof(parsed));
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_cose_encrypt0_create(
                                      SYN_COSE_ALGO_A256GCM, key256, iv, sizeof(iv), kid,
                                      sizeof(kid) - 1, plaintext, sizeof(plaintext) - 1, aad,
                                      sizeof(aad) - 1, cose_msg, sizeof(cose_msg), &cose_msg_len));
    TEST_ASSERT_GREATER_THAN(0, cose_msg_len);

    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_cose_encrypt0_decrypt(cose_msg, cose_msg_len, key256, aad,
                                                            sizeof(aad) - 1, dec_buf,
                                                            sizeof(dec_buf), &dec_len, &parsed));
    TEST_ASSERT_EQUAL(sizeof(plaintext) - 1, dec_len);
    TEST_ASSERT_EQUAL_MEMORY(plaintext, dec_buf, dec_len);
    TEST_ASSERT_EQUAL_INT(SYN_COSE_ALGO_A256GCM, parsed.alg);

    /* 4. Tampering & verification failure tests */
    /* Wrong key */
    uint8_t bad_key[32];
    memcpy(bad_key, key256, 32);
    bad_key[0] ^= 0x01;
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_cose_encrypt0_decrypt(cose_msg, cose_msg_len, bad_key, aad,
                                                               sizeof(aad) - 1, dec_buf,
                                                               sizeof(dec_buf), &dec_len, NULL));

    /* Corrupted ciphertext byte */
    cose_msg[cose_msg_len - 1] ^= 0x55;
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_cose_encrypt0_decrypt(cose_msg, cose_msg_len, key256, aad,
                                                               sizeof(aad) - 1, dec_buf,
                                                               sizeof(dec_buf), &dec_len, NULL));

    /* Empty plaintext with A256GCM */
    uint8_t empty_msg[256];
    size_t empty_len = 0;
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_cose_encrypt0_create(
                                      SYN_COSE_ALGO_A256GCM, key256, iv, sizeof(iv), NULL, 0, NULL,
                                      0, NULL, 0, empty_msg, sizeof(empty_msg), &empty_len));
    TEST_ASSERT_EQUAL_INT(SYN_OK,
                          syn_cose_encrypt0_decrypt(empty_msg, empty_len, key256, NULL, 0, dec_buf,
                                                    sizeof(dec_buf), &dec_len, NULL));
    TEST_ASSERT_EQUAL(0, dec_len);
}

void run_cose_tests(void)
{
    RUN_TEST(test_cose_sign1_eddsa_roundtrip_and_tamper);
    RUN_TEST(test_cose_sign1_es256_roundtrip_and_tamper);
    RUN_TEST(test_cose_encrypt0_chacha20poly1305_roundtrip_and_tamper);
    RUN_TEST(test_cose_encrypt0_aes_gcm_roundtrip_and_tamper);
    RUN_TEST(test_cose_null_and_boundary_checks);
}
