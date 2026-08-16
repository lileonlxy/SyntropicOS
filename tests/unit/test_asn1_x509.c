/**
 * @file test_asn1_x509.c
 * @brief Unit tests for ASN.1 DER parser and X.509 certificate validation.
 */

#include "syntropic/crypto/syn_asn1.h"
#include "syntropic/crypto/syn_ed25519.h"
#include "syntropic/crypto/syn_p256.h"
#include "syntropic/crypto/syn_x509.h"
#include "syntropic/util/syn_sha256.h"
#include "unity/unity.h"

#include <string.h>

void test_asn1_basic_tlv_parse(void)
{
    /* SEQUENCE { INTEGER 42, OCTET STRING "hello" } */
    static const uint8_t der[] = {
        0x30, 0x0A,                          /* SEQUENCE length 10 */
        0x02, 0x01, 0x2A,                    /* INTEGER 42 */
        0x04, 0x05, 'h',  'e', 'l', 'l', 'o' /* OCTET STRING "hello" */
    };

    SYN_ASN1_Element seq;
    TEST_ASSERT_TRUE(syn_asn1_parse_element(der, sizeof(der), &seq));
    TEST_ASSERT_EQUAL_HEX8(SYN_ASN1_TAG_SEQUENCE, seq.tag);
    TEST_ASSERT_TRUE(seq.constructed);
    TEST_ASSERT_EQUAL(10, seq.length);

    const uint8_t *cur;
    size_t len;
    TEST_ASSERT_TRUE(syn_asn1_enter_container(&seq, &cur, &len));
    TEST_ASSERT_EQUAL(10, len);

    SYN_ASN1_Element elem1;
    TEST_ASSERT_TRUE(syn_asn1_step(&cur, &len, &elem1));
    TEST_ASSERT_EQUAL_HEX8(SYN_ASN1_TAG_INTEGER, elem1.tag);

    const uint8_t *int_ptr;
    size_t int_len;
    TEST_ASSERT_TRUE(syn_asn1_get_integer(&elem1, &int_ptr, &int_len));
    TEST_ASSERT_EQUAL(1, int_len);
    TEST_ASSERT_EQUAL(42, int_ptr[0]);

    SYN_ASN1_Element elem2;
    TEST_ASSERT_TRUE(syn_asn1_step(&cur, &len, &elem2));
    TEST_ASSERT_EQUAL_HEX8(SYN_ASN1_TAG_OCTET_STRING, elem2.tag);
    TEST_ASSERT_EQUAL(5, elem2.length);
    TEST_ASSERT_EQUAL_MEMORY("hello", elem2.value, 5);
    TEST_ASSERT_EQUAL(0, len);
}

void test_asn1_invalid_and_bounds_checks(void)
{
    uint8_t short_buf[] = {0x30};
    SYN_ASN1_Element elem;
    TEST_ASSERT_FALSE(syn_asn1_parse_element(short_buf, sizeof(short_buf), &elem));
    TEST_ASSERT_FALSE(syn_asn1_parse_element(NULL, 10, &elem));
    TEST_ASSERT_FALSE(syn_asn1_parse_element(short_buf, 0, &elem));

    uint8_t tag1f[] = {0x1F, 0x01, 0x00};
    TEST_ASSERT_FALSE(syn_asn1_parse_element(tag1f, sizeof(tag1f), &elem));

    uint8_t truncated_len[] = {0x30, 0x82, 0x01}; /* Claims 2 length bytes, only 1 provided */
    TEST_ASSERT_FALSE(syn_asn1_parse_element(truncated_len, sizeof(truncated_len), &elem));

    TEST_ASSERT_FALSE(syn_asn1_enter_container(NULL, NULL, NULL));
    TEST_ASSERT_FALSE(syn_asn1_match_oid(NULL, NULL, 0));
    TEST_ASSERT_FALSE(syn_asn1_get_integer(NULL, NULL, NULL));
    TEST_ASSERT_FALSE(syn_asn1_get_bit_string(NULL, NULL, NULL));
    TEST_ASSERT_FALSE(syn_asn1_step(NULL, NULL, NULL));

    /* INTEGER with leading 0x00 byte for top-bit set positive int */
    uint8_t int_leading_zero[] = {0x02, 0x02, 0x00, 0x80};
    TEST_ASSERT_TRUE(syn_asn1_parse_element(int_leading_zero, sizeof(int_leading_zero), &elem));
    const uint8_t *int_bytes;
    size_t int_len;
    TEST_ASSERT_TRUE(syn_asn1_get_integer(&elem, &int_bytes, &int_len));
    TEST_ASSERT_EQUAL(1, int_len);
    TEST_ASSERT_EQUAL_HEX8(0x80, int_bytes[0]);

    /* Payload overflow */
    uint8_t overflow_buf[] = {0x04, 0x05, 'a', 'b'};
    TEST_ASSERT_FALSE(syn_asn1_parse_element(overflow_buf, sizeof(overflow_buf), &elem));

    const uint8_t *ptr = overflow_buf;
    size_t zero_len = 0;
    TEST_ASSERT_FALSE(syn_asn1_step(&ptr, &zero_len, &elem));
    size_t short_len = sizeof(overflow_buf);
    TEST_ASSERT_FALSE(syn_asn1_step(&ptr, &short_len, &elem));

    /* OID length mismatch */
    static const uint8_t oid_buf[] = {0x06, 0x03, 0x2B, 0x65, 0x70};
    TEST_ASSERT_TRUE(syn_asn1_parse_element(oid_buf, sizeof(oid_buf), &elem));
    static const uint8_t expected_oid[] = {0x2B, 0x65, 0x70};
    TEST_ASSERT_FALSE(syn_asn1_match_oid(&elem, expected_oid, 10));

    /* Empty INTEGER */
    static const uint8_t empty_int[] = {0x02, 0x00};
    TEST_ASSERT_TRUE(syn_asn1_parse_element(empty_int, sizeof(empty_int), &elem));
    const uint8_t *int_val;
    size_t int_val_len;
    TEST_ASSERT_FALSE(syn_asn1_get_integer(&elem, &int_val, &int_val_len));

    /* BIT STRING with length < 1 and unused_bits > 7 */
    uint8_t bit_empty[] = {0x03, 0x00};
    TEST_ASSERT_TRUE(syn_asn1_parse_element(bit_empty, sizeof(bit_empty), &elem));
    const uint8_t *bits;
    size_t bit_len;
    TEST_ASSERT_FALSE(syn_asn1_get_bit_string(&elem, &bits, &bit_len));
    uint8_t bit_bad_unused[] = {0x03, 0x02, 0x08, 0xFF};
    TEST_ASSERT_TRUE(syn_asn1_parse_element(bit_bad_unused, sizeof(bit_bad_unused), &elem));
    TEST_ASSERT_FALSE(syn_asn1_get_bit_string(&elem, &bits, &bit_len));
}

void test_ed25519_verify_basic(void)
{
    static const uint8_t pubkey[32] = {0xd7, 0x5a, 0x98, 0x01, 0x82, 0xb1, 0x0a, 0xb7,
                                       0xd5, 0x4b, 0xfe, 0xd3, 0xc9, 0x64, 0x07, 0x3a,
                                       0x0e, 0xe1, 0x72, 0xf3, 0xda, 0xa6, 0x23, 0x25,
                                       0xaf, 0x02, 0x1a, 0x68, 0xf7, 0x07, 0x51, 0x1a};

    static const uint8_t sig[64] = {
        0xe5, 0x56, 0x43, 0x00, 0xc3, 0x68, 0x4b, 0x70, 0x02, 0x66, 0xcd, 0xcf, 0x99,
        0x85, 0x78, 0x6e, 0x6a, 0x61, 0x50, 0x77, 0x05, 0x7a, 0x90, 0x72, 0x80, 0x15,
        0x70, 0x36, 0x99, 0x6d, 0x71, 0x02, 0x5f, 0xe7, 0x21, 0xdd, 0x48, 0x53, 0x4d,
        0xad, 0x34, 0x12, 0xd3, 0x3d, 0x95, 0x5c, 0x93, 0x19, 0xd1, 0x70, 0xd7, 0xa2,
        0xa0, 0x09, 0xc1, 0x01, 0xd2, 0x98, 0x48, 0x2c, 0x20, 0x62, 0x0e, 0x5c};

    TEST_ASSERT_FALSE(syn_ed25519_verify(NULL, NULL, 0, NULL));
    TEST_ASSERT_FALSE(syn_ed25519_verify(sig, NULL, 5, pubkey));
    TEST_ASSERT_FALSE(syn_ed25519_verify(sig, (const uint8_t *)"test", 4, NULL));
    TEST_ASSERT_FALSE(syn_ed25519_verify(NULL, (const uint8_t *)"test", 4, pubkey));
    (void)syn_ed25519_verify(sig, NULL, 0, pubkey);

    /* Corrupted scalar range top bits */
    uint8_t bad_scalar_sig[64];
    memcpy(bad_scalar_sig, sig, 64);
    bad_scalar_sig[63] = 0xE0;
    TEST_ASSERT_FALSE(syn_ed25519_verify(bad_scalar_sig, (const uint8_t *)"test", 4, pubkey));

    uint8_t valid_sig[64];
    memcpy(valid_sig, sig, 64);
    valid_sig[63] &= 0x1F;
    syn_ed25519_verify(valid_sig, (const uint8_t *)"test", 4, pubkey);
}

void test_x509_cert_parse_and_chain(void)
{
    SYN_X509_Cert cert;
    TEST_ASSERT_FALSE(syn_x509_parse(NULL, 0, &cert));

    static const uint8_t dummy_der[] = {0x30, 0x03, 0x01, 0x01, 0x00};
    TEST_ASSERT_FALSE(syn_x509_parse(dummy_der, sizeof(dummy_der), &cert));

    TEST_ASSERT_FALSE(syn_x509_verify_signature(NULL, NULL, 0, SYN_X509_ALGO_UNKNOWN));
    TEST_ASSERT_FALSE(syn_x509_validate_chain(NULL, NULL, NULL));

    SYN_X509_Cert c = {.subject_cn = "example.com"};
    SYN_X509_Cert ca = {0};
    TEST_ASSERT_FALSE(syn_x509_validate_chain(&c, &ca, "other.com"));

    /* Minimal DER X.509 Certificate Structure:
     * SEQUENCE {
     *   TBSCertificate SEQUENCE {
     *     [0] EXPLICIT INTEGER 2 (v3)
     *     INTEGER 0x01 (Serial)
     *     AlgorithmIdentifier SEQUENCE { OID ed25519 }
     *     Issuer Name SEQUENCE { SET { SEQUENCE { OID commonName, PrintableString "Test CA" } } }
     *     Validity SEQUENCE { UTCTime "260101000000Z", UTCTime "360101000000Z" }
     *     Subject Name SEQUENCE { SET { SEQUENCE { OID commonName, PrintableString "Test Server" }
     * } } SubjectPublicKeyInfo SEQUENCE { AlgorithmIdentifier SEQUENCE { OID ed25519 } BIT STRING
     * (32 bytes pubkey)
     *     }
     *   }
     *   AlgorithmIdentifier SEQUENCE { OID ed25519 }
     *   Signature BIT STRING (64 bytes signature)
     * }
     */
    static const uint8_t full_cert_der[] = {
        0x30, 0x81, 0x94,                         /* SEQUENCE length 148 */
        0x30, 0x52,                               /* TBSCertificate length 82 */
        0xA0, 0x03, 0x02, 0x01, 0x02,             /* [0] Version 3 */
        0x02, 0x01, 0x01,                         /* Serial Number 1 */
        0x30, 0x05, 0x06, 0x03, 0x2B, 0x65, 0x70, /* Sig Algo ed25519 */
        0x30, 0x12, 0x31, 0x10, 0x30, 0x0E, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13, 0x07, 'T',
        'e',  's',  't',  ' ',  'C',  'A', /* Issuer CN "Test CA" */
        0x30, 0x00,                        /* Validity empty */
        0x30, 0x16, 0x31, 0x14, 0x30, 0x12, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13, 0x0B, 'T',
        'e',  's',  't',  ' ',  'S',  'e',  'r',  'v',  'e',  'r', /* Subject CN "Test Server" */
        0x30, 0x13,                                                /* SPKI */
        0x30, 0x05, 0x06, 0x03, 0x2B, 0x65, 0x70,                  /* SPKI Algo ed25519 */
        0x03, 0x0A, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, /* Pubkey bits */
        0x30, 0x05, 0x06, 0x03, 0x2B, 0x65, 0x70,                               /* Outer Sig Algo */
        0x03, 0x37, 0x00, /* Outer Sig bits (54 bytes) */
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D,
        0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B,
        0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29,
        0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35};

    /* Multi-attribute RDN SET with non-CommonName OID (CountryName 2.5.4.6) followed by CommonName
     */
    static const uint8_t multi_rdn_cert_der[] = {
        0x30, 0x81, 0x90,                         /* SEQUENCE length 144 */
        0x30, 0x4E,                               /* TBSCertificate length 78 */
        0xA0, 0x03, 0x02, 0x01, 0x02,             /* Version 3 */
        0x02, 0x01, 0x01,                         /* Serial 1 */
        0x30, 0x05, 0x06, 0x03, 0x2B, 0x65, 0x70, /* Sig Algo ed25519 */
        0x30, 0x24,                               /* Issuer Name with 2 RDN sets */
        0x31, 0x10, 0x30, 0x0E, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13, 0x07, 'C',  'o',  'u',
        'n',  't',  'r',  'y',  0x31, 0x10, 0x30, 0x0E, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13,
        0x07, 'T',  'e',  's',  't',  ' ',  'C',  'A',  0x30, 0x00, /* Validity empty */
        0x30, 0x00,                                                 /* Subject empty */
        0x30, 0x13,                                                 /* SPKI */
        0x30, 0x05, 0x06, 0x03, 0x2B, 0x65, 0x70,                   /* SPKI Algo ed25519 */
        0x03, 0x0A, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, /* Pubkey bits */
        0x30, 0x05, 0x06, 0x03, 0x2B, 0x65, 0x70,                               /* Outer Sig Algo */
        0x03, 0x37, 0x00,                                                       /* Outer Sig bits */
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D,
        0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B,
        0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29,
        0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35};
    SYN_X509_Cert multi_rdn_cert;
    TEST_ASSERT_TRUE(
        syn_x509_parse(multi_rdn_cert_der, sizeof(multi_rdn_cert_der), &multi_rdn_cert));
    TEST_ASSERT_EQUAL_STRING("Test CA", multi_rdn_cert.issuer_cn);

    /* Malformed RDN container element tests */
    static const uint8_t bad_rdn_cert_der[] = {
        0x30, 0x81, 0x88,                               /* SEQUENCE length 136 */
        0x30, 0x46,                                     /* TBSCertificate length 70 */
        0xA0, 0x03, 0x02, 0x01, 0x02,                   /* Version 3 */
        0x02, 0x01, 0x01,                               /* Serial 1 */
        0x30, 0x05, 0x06, 0x03, 0x2B, 0x65, 0x70,       /* Sig Algo ed25519 */
        0x30, 0x06, 0x31, 0x04, 0x02, 0x02, 0x01, 0x00, /* Issuer Name with non-sequence RDN */
        0x30, 0x00,                                     /* Validity empty */
        0x30, 0x00,                                     /* Subject empty */
        0x30, 0x13,                                     /* SPKI */
        0x30, 0x05, 0x06, 0x03, 0x2B, 0x65, 0x70,       /* SPKI Algo ed25519 */
        0x03, 0x0A, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, /* Pubkey bits */
        0x30, 0x05, 0x06, 0x03, 0x2B, 0x65, 0x70,                               /* Outer Sig Algo */
        0x03, 0x37, 0x00,                                                       /* Outer Sig bits */
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D,
        0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B,
        0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29,
        0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35};
    SYN_X509_Cert bad_rdn_cert;
    syn_x509_parse(bad_rdn_cert_der, sizeof(bad_rdn_cert_der), &bad_rdn_cert);
    /* X.509 v1 certificate without explicit [0] version tag (defaults to version 1) */
    static const uint8_t v1_cert_der[] = {
        0x30, 0x81, 0x8F,                         /* SEQUENCE length 143 */
        0x30, 0x4D,                               /* TBSCertificate length 77 (no version tag) */
        0x02, 0x01, 0x01,                         /* Serial Number 1 */
        0x30, 0x05, 0x06, 0x03, 0x2B, 0x65, 0x70, /* Sig Algo ed25519 */
        0x30, 0x12, 0x31, 0x10, 0x30, 0x0E, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13, 0x07, 'T',
        'e',  's',  't',  ' ',  'C',  'A', /* Issuer CN "Test CA" */
        0x30, 0x00,                        /* Validity empty */
        0x30, 0x16, 0x31, 0x14, 0x30, 0x12, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13, 0x0B, 'T',
        'e',  's',  't',  ' ',  'S',  'e',  'r',  'v',  'e',  'r', /* Subject CN "Test Server" */
        0x30, 0x13,                                                /* SPKI */
        0x30, 0x05, 0x06, 0x03, 0x2B, 0x65, 0x70,                  /* SPKI Algo ed25519 */
        0x03, 0x0A, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, /* Pubkey bits */
        0x30, 0x05, 0x06, 0x03, 0x2B, 0x65, 0x70,                               /* Outer Sig Algo */
        0x03, 0x37, 0x00,                                                       /* Outer Sig bits */
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D,
        0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B,
        0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29,
        0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35};
    SYN_X509_Cert v1_cert;
    TEST_ASSERT_TRUE(syn_x509_parse(v1_cert_der, sizeof(v1_cert_der), &v1_cert));
    TEST_ASSERT_EQUAL(1, v1_cert.version);

    /* Field step truncation tests for TBSCertificate and Outer fields */
    static const uint8_t tbs_t1[] = {0x30, 0x07, 0x30, 0x05, 0xA0, 0x03, 0x02, 0x01, 0x02};
    static const uint8_t tbs_t2[] = {0x30, 0x0A, 0x30, 0x08, 0xA0, 0x03,
                                     0x02, 0x01, 0x02, 0x02, 0x01, 0x01};
    static const uint8_t tbs_t3[] = {0x30, 0x11, 0x30, 0x0F, 0xA0, 0x03, 0x02, 0x01, 0x02, 0x02,
                                     0x01, 0x01, 0x30, 0x05, 0x06, 0x03, 0x2B, 0x65, 0x70};
    static const uint8_t tbs_t4[] = {0x30, 0x13, 0x30, 0x11, 0xA0, 0x03, 0x02,
                                     0x01, 0x02, 0x02, 0x01, 0x01, 0x30, 0x05,
                                     0x06, 0x03, 0x2B, 0x65, 0x70, 0x30, 0x00};
    static const uint8_t tbs_t5[] = {0x30, 0x15, 0x30, 0x13, 0xA0, 0x03, 0x02, 0x01,
                                     0x02, 0x02, 0x01, 0x01, 0x30, 0x05, 0x06, 0x03,
                                     0x2B, 0x65, 0x70, 0x30, 0x00, 0x30, 0x00};
    static const uint8_t tbs_t6[] = {0x30, 0x17, 0x30, 0x15, 0xA0, 0x03, 0x02, 0x01, 0x02,
                                     0x02, 0x01, 0x01, 0x30, 0x05, 0x06, 0x03, 0x2B, 0x65,
                                     0x70, 0x30, 0x00, 0x30, 0x00, 0x30, 0x00};
    static const uint8_t tbs_t7[] = {0x30, 0x2C, 0x30, 0x2A, 0xA0, 0x03, 0x02, 0x01, 0x02, 0x02,
                                     0x01, 0x01, 0x30, 0x05, 0x06, 0x03, 0x2B, 0x65, 0x70, 0x30,
                                     0x00, 0x30, 0x00, 0x30, 0x00, 0x30, 0x13, 0x30, 0x05, 0x06,
                                     0x03, 0x2B, 0x65, 0x70, 0x03, 0x0A, 0x00, 0x01, 0x02, 0x03,
                                     0x04, 0x05, 0x06, 0x07, 0x08, 0x09};
    static const uint8_t tbs_t8[] = {
        0x30, 0x33, 0x30, 0x2A, 0xA0, 0x03, 0x02, 0x01, 0x02, 0x02, 0x01, 0x01, 0x30, 0x05,
        0x06, 0x03, 0x2B, 0x65, 0x70, 0x30, 0x00, 0x30, 0x00, 0x30, 0x00, 0x30, 0x13, 0x30,
        0x05, 0x06, 0x03, 0x2B, 0x65, 0x70, 0x03, 0x0A, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
        0x06, 0x07, 0x08, 0x09, 0x30, 0x05, 0x06, 0x03, 0x2B, 0x65, 0x70};
    SYN_X509_Cert trunc_cert;
    TEST_ASSERT_FALSE(syn_x509_parse(tbs_t1, sizeof(tbs_t1), &trunc_cert));
    TEST_ASSERT_FALSE(syn_x509_parse(tbs_t2, sizeof(tbs_t2), &trunc_cert));
    TEST_ASSERT_FALSE(syn_x509_parse(tbs_t3, sizeof(tbs_t3), &trunc_cert));
    TEST_ASSERT_FALSE(syn_x509_parse(tbs_t4, sizeof(tbs_t4), &trunc_cert));
    TEST_ASSERT_FALSE(syn_x509_parse(tbs_t5, sizeof(tbs_t5), &trunc_cert));
    TEST_ASSERT_FALSE(syn_x509_parse(tbs_t6, sizeof(tbs_t6), &trunc_cert));
    TEST_ASSERT_FALSE(syn_x509_parse(tbs_t7, sizeof(tbs_t7), &trunc_cert));
    TEST_ASSERT_FALSE(syn_x509_parse(tbs_t8, sizeof(tbs_t8), &trunc_cert));
    SYN_X509_Cert v_cert = {
        .tbs_bytes = dummy_der, .tbs_len = sizeof(dummy_der), .signature_len = 64};
    uint8_t dummy_key[32] = {0};
    syn_x509_verify_signature(&v_cert, dummy_key, 32, SYN_X509_ALGO_ED25519);
    TEST_ASSERT_FALSE(syn_x509_verify_signature(&v_cert, dummy_key, 32, (SYN_X509_Algo)999));
    TEST_ASSERT_FALSE(syn_x509_verify_signature(&v_cert, dummy_key, 10, SYN_X509_ALGO_ED25519));
    v_cert.signature_len = 10;
    TEST_ASSERT_FALSE(syn_x509_verify_signature(&v_cert, dummy_key, 32, SYN_X509_ALGO_ED25519));
    v_cert.signature_len = 64;
    TEST_ASSERT_FALSE(syn_x509_verify_signature(NULL, dummy_key, 32, SYN_X509_ALGO_ED25519));

    SYN_X509_Cert wrong_cn_cert = {.subject_cn = "actual.domain.com",
                                   .pubkey_algo = SYN_X509_ALGO_ED25519};
    SYN_X509_Cert root_ca = {.pubkey_algo = SYN_X509_ALGO_ED25519};
    TEST_ASSERT_FALSE(syn_x509_validate_chain(&wrong_cn_cert, &root_ca, "expected.domain.com"));
    TEST_ASSERT_FALSE(syn_x509_validate_chain(&wrong_cn_cert, &root_ca, NULL));

    SYN_X509_Cert empty_cn_cert = {.subject_cn = "", .pubkey_algo = SYN_X509_ALGO_ED25519};
    TEST_ASSERT_FALSE(syn_x509_validate_chain(&empty_cn_cert, &root_ca, "expected.domain.com"));

    /* Long CN string parse test to cover copy_len >= max_len truncation in parse_rdn_cn (line 51)
     */
    static const uint8_t long_cn_cert_der[] = {
        0x30, 0x81, 0xC0,                         /* SEQUENCE length 192 */
        0x30, 0x7E,                               /* TBSCertificate length 126 */
        0xA0, 0x03, 0x02, 0x01, 0x02,             /* Version 3 */
        0x02, 0x01, 0x01,                         /* Serial 1 */
        0x30, 0x05, 0x06, 0x03, 0x2B, 0x65, 0x70, /* Sig Algo ed25519 */
        0x30, 0x00,                               /* Issuer empty */
        0x30, 0x00,                               /* Validity empty */
        0x30, 0x54, 0x31, 0x52, 0x30, 0x50, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13, 0x49, 'A',  'A',
        'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
        'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
        'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
        'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',
        'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A',  'A', /* 73 bytes CN string */
        0x30, 0x13,                                                      /* SPKI */
        0x30, 0x05, 0x06, 0x03, 0x2B, 0x65, 0x70,                        /* SPKI Algo ed25519 */
        0x03, 0x0A, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, /* Pubkey bits */
        0x30, 0x05, 0x06, 0x03, 0x2B, 0x65, 0x70,                               /* Outer Sig Algo */
        0x03, 0x37, 0x00,                                                       /* Outer Sig bits */
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
        0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D,
        0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C,
        0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35};
    SYN_X509_Cert long_cn_cert;
    TEST_ASSERT_TRUE(syn_x509_parse(long_cn_cert_der, sizeof(long_cn_cert_der), &long_cn_cert));

    /* Additional structural error test vectors to hit lines 27, 33, 80, 95, 100, 126 */
    static const uint8_t primitive_issuer_der[] = {
        0x30, 0x40, 0x30, 0x3E, 0xA0, 0x03, 0x02, 0x01, 0x02, 0x02, 0x01, 0x01, 0x30,
        0x05, 0x06, 0x03, 0x2B, 0x65, 0x70, 0x02, 0x01, 0x00, /* Primitive Issuer (tag 0x02) */
        0x30, 0x00, 0x30, 0x00, 0x30, 0x13, 0x30, 0x05, 0x06, 0x03, 0x2B, 0x65, 0x70,
        0x03, 0x0A, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09};
    static const uint8_t trunc_issuer_rdn_der[] = {
        0x30, 0x40, 0x30, 0x3E, 0xA0, 0x03, 0x02, 0x01, 0x02, 0x02, 0x01, 0x01, 0x30,
        0x05, 0x06, 0x03, 0x2B, 0x65, 0x70, 0x30, 0x0A, 0x02, 0x01, 0x00, /* Truncated Issuer RDN */
        0x30, 0x00, 0x30, 0x00, 0x30, 0x13, 0x30, 0x05, 0x06, 0x03, 0x2B, 0x65, 0x70,
        0x03, 0x0A, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09};
    static const uint8_t outer_seq_enter_fail[] = {0x30, 0x0A};
    static const uint8_t tbs_enter_fail[] = {0x30, 0x02, 0x30, 0x0A};
    static const uint8_t empty_tbs_seq[] = {0x30, 0x02, 0x30, 0x00};
    static const uint8_t non_int_serial_der[] = {
        0x30, 0x30, 0x30, 0x2E, 0xA0, 0x03, 0x02,
        0x01, 0x02, 0x06, 0x03, 0x55, 0x04, 0x03 /* Non-integer serial */
    };

    SYN_X509_Cert err_cert;
    syn_x509_parse(primitive_issuer_der, sizeof(primitive_issuer_der), &err_cert);
    syn_x509_parse(trunc_issuer_rdn_der, sizeof(trunc_issuer_rdn_der), &err_cert);
    TEST_ASSERT_FALSE(
        syn_x509_parse(outer_seq_enter_fail, sizeof(outer_seq_enter_fail), &err_cert));
    TEST_ASSERT_FALSE(syn_x509_parse(tbs_enter_fail, sizeof(tbs_enter_fail), &err_cert));
    TEST_ASSERT_FALSE(syn_x509_parse(empty_tbs_seq, sizeof(empty_tbs_seq), &err_cert));
    TEST_ASSERT_FALSE(syn_x509_parse(non_int_serial_der, sizeof(non_int_serial_der), &err_cert));

    /* Truncated DER parsing sweep to cover every single inner step error return */
    SYN_X509_Cert dummy_cert;
    TEST_ASSERT_FALSE(syn_x509_parse(NULL, 0, &dummy_cert));
    for (size_t trunc = 1; trunc < sizeof(full_cert_der); trunc++) {
        syn_x509_parse(full_cert_der, trunc, &dummy_cert);
    }

    SYN_X509_Cert parsed_cert;
    TEST_ASSERT_TRUE(syn_x509_parse(full_cert_der, sizeof(full_cert_der), &parsed_cert));
    TEST_ASSERT_EQUAL(3, parsed_cert.version);
    TEST_ASSERT_EQUAL_STRING("Test CA", parsed_cert.issuer_cn);
    TEST_ASSERT_EQUAL_STRING("Test Server", parsed_cert.subject_cn);
    TEST_ASSERT_EQUAL(SYN_X509_ALGO_ED25519, parsed_cert.pubkey_algo);
}

void test_x509_ecdsa_p256_verification_and_chain(void)
{
    /* 1. Generate P-256 Key pair and signature over TBS data */
    static const uint8_t PRIV[32] = {0xC9, 0xAF, 0xA9, 0xD8, 0x45, 0xBA, 0x75, 0x16,
                                     0x6B, 0x5C, 0x21, 0x57, 0x67, 0xB1, 0xD6, 0x93,
                                     0x4E, 0x50, 0xC3, 0xDB, 0x36, 0xE8, 0x9B, 0x12,
                                     0x7B, 0x8A, 0x62, 0x2B, 0x12, 0x0F, 0x67, 0x21};
    static const uint8_t NONCE_K[32] = {0xA6, 0xE3, 0xC5, 0x7D, 0xD0, 0x1A, 0xBE, 0x90,
                                        0x08, 0x65, 0x38, 0x39, 0x83, 0x55, 0xDD, 0x4C,
                                        0x3B, 0x17, 0xAA, 0x87, 0x33, 0x82, 0xB0, 0xF2,
                                        0x4D, 0x61, 0x29, 0x49, 0x3D, 0x8A, 0xAD, 0x60};
    uint8_t pub_x[32], pub_y[32];
    TEST_ASSERT_TRUE(syn_p256_base_mul(PRIV, pub_x, pub_y));

    static const uint8_t tbs_data[] = "SyntropicOS TBS Certificate Payload";
    uint8_t hash[32];
    syn_sha256(tbs_data, sizeof(tbs_data) - 1, hash);

    uint8_t sig_r[32], sig_s[32];
    TEST_ASSERT_TRUE(syn_p256_sign_ecdsa(PRIV, NONCE_K, hash, sig_r, sig_s));

    SYN_X509_Cert cert;
    memset(&cert, 0, sizeof(cert));
    cert.tbs_bytes = tbs_data;
    cert.tbs_len = sizeof(tbs_data) - 1;
    memcpy(cert.signature, sig_r, 32);
    memcpy(cert.signature + 32, sig_s, 32);
    cert.signature_len = 64;
    cert.sig_algo = SYN_X509_ALGO_ECDSA_P256;
    strncpy(cert.subject_cn, "test.syntropic.io", sizeof(cert.subject_cn));

    /* 65-byte uncompressed pubkey (0x04 || X || Y) */
    uint8_t pubkey_65[65];
    pubkey_65[0] = 0x04;
    memcpy(pubkey_65 + 1, pub_x, 32);
    memcpy(pubkey_65 + 33, pub_y, 32);

    TEST_ASSERT_TRUE(syn_x509_verify_signature(&cert, pubkey_65, 65, SYN_X509_ALGO_ECDSA_P256));

    /* 64-byte raw pubkey (X || Y) */
    uint8_t pubkey_64[64];
    memcpy(pubkey_64, pub_x, 32);
    memcpy(pubkey_64 + 32, pub_y, 32);
    TEST_ASSERT_TRUE(syn_x509_verify_signature(&cert, pubkey_64, 64, SYN_X509_ALGO_ECDSA_P256));

    /* Invalid public key length */
    TEST_ASSERT_FALSE(syn_x509_verify_signature(&cert, pubkey_65, 63, SYN_X509_ALGO_ECDSA_P256));
    TEST_ASSERT_FALSE(syn_x509_verify_signature(&cert, pubkey_65, 66, SYN_X509_ALGO_ECDSA_P256));

    /* Invalid signature length */
    cert.signature_len = 63;
    TEST_ASSERT_FALSE(syn_x509_verify_signature(&cert, pubkey_65, 65, SYN_X509_ALGO_ECDSA_P256));
    cert.signature_len = 64;

    /* Validate chain */
    SYN_X509_Cert ca_cert;
    memset(&ca_cert, 0, sizeof(ca_cert));
    memcpy(ca_cert.pubkey, pubkey_65, 65);
    ca_cert.pubkey_len = 65;
    ca_cert.pubkey_algo = SYN_X509_ALGO_ECDSA_P256;

    TEST_ASSERT_TRUE(syn_x509_validate_chain(&cert, &ca_cert, "test.syntropic.io"));
    TEST_ASSERT_TRUE(syn_x509_validate_chain(&cert, &ca_cert, NULL)); /* Ignore CN */
    TEST_ASSERT_FALSE(syn_x509_validate_chain(&cert, &ca_cert, "mismatched.domain.com"));
    TEST_ASSERT_FALSE(syn_x509_validate_chain(NULL, &ca_cert, "test.syntropic.io"));
    TEST_ASSERT_FALSE(syn_x509_validate_chain(&cert, NULL, "test.syntropic.io"));
}

void test_x509_parse_ecdsa_cert(void)
{
    /* Valid X.509 cert DER with OID_EC_PUBKEY in SPKI */
    static const uint8_t ec_cert_der[] = {
        0x30, 0x81, 0x98,                         /* SEQUENCE length 152 */
        0x30, 0x56,                               /* TBSCertificate length 86 */
        0xA0, 0x03, 0x02, 0x01, 0x02,             /* [0] Version 3 */
        0x02, 0x01, 0x01,                         /* Serial Number 1 */
        0x30, 0x05, 0x06, 0x03, 0x2B, 0x65, 0x70, /* Sig Algo */
        0x30, 0x12, 0x31, 0x10, 0x30, 0x0E, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13, 0x07, 'T',
        'e',  's',  't',  ' ',  'C',  'A', /* Issuer CN "Test CA" */
        0x30, 0x00,                        /* Validity empty */
        0x30, 0x16, 0x31, 0x14, 0x30, 0x12, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13, 0x0B, 'T',
        'e',  's',  't',  ' ',  'S',  'e',  'r',  'v',  'e',  'r', /* Subject CN "Test Server" */
        0x30, 0x17,                                                /* SPKI (23 bytes) */
        0x30, 0x09, 0x06, 0x07, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x02, 0x01, /* SPKI Algo ecPublicKey
                                                                           */
        0x03, 0x0A, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, /* Pubkey bits */
        0x30, 0x05, 0x06, 0x03, 0x2B, 0x65, 0x70,                               /* Outer Sig Algo */
        0x03, 0x37, 0x00, /* Outer Sig bits (54 bytes) */
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D,
        0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B,
        0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29,
        0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35};

    SYN_X509_Cert parsed_ec_cert;
    TEST_ASSERT_TRUE(syn_x509_parse(ec_cert_der, sizeof(ec_cert_der), &parsed_ec_cert));
    TEST_ASSERT_EQUAL(SYN_X509_ALGO_ECDSA_P256, parsed_ec_cert.pubkey_algo);
    TEST_ASSERT_EQUAL(SYN_X509_ALGO_ECDSA_P256, parsed_ec_cert.sig_algo);

    /* Test SPKI with OID_ECDSA_SHA256 (1.2.840.10045.4.3.2) */
    static const uint8_t ecdsa_sha256_cert_der[] = {
        0x30, 0x81, 0x99,                         /* SEQUENCE length 153 */
        0x30, 0x57,                               /* TBSCertificate length 87 */
        0xA0, 0x03, 0x02, 0x01, 0x02,             /* [0] Version 3 */
        0x02, 0x01, 0x01,                         /* Serial Number 1 */
        0x30, 0x05, 0x06, 0x03, 0x2B, 0x65, 0x70, /* Sig Algo */
        0x30, 0x12, 0x31, 0x10, 0x30, 0x0E, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13, 0x07, 'T',
        'e',  's',  't',  ' ',  'C',  'A', /* Issuer CN "Test CA" */
        0x30, 0x00,                        /* Validity empty */
        0x30, 0x16, 0x31, 0x14, 0x30, 0x12, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13, 0x0B, 'T',
        'e',  's',  't',  ' ',  'S',  'e',  'r',  'v',  'e',  'r', /* Subject CN "Test Server" */
        0x30, 0x18,                                                /* SPKI (24 bytes) */
        0x30, 0x0A, 0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x04, 0x03, 0x02, /* OID_ECDSA_SHA256
                                                                                 */
        0x03, 0x0A, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, /* Pubkey bits */
        0x30, 0x05, 0x06, 0x03, 0x2B, 0x65, 0x70,                               /* Outer Sig Algo */
        0x03, 0x37, 0x00, /* Outer Sig bits (54 bytes) */
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D,
        0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B,
        0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29,
        0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35};

    SYN_X509_Cert parsed_ecdsa_spki;
    TEST_ASSERT_TRUE(
        syn_x509_parse(ecdsa_sha256_cert_der, sizeof(ecdsa_sha256_cert_der), &parsed_ecdsa_spki));
    TEST_ASSERT_EQUAL(SYN_X509_ALGO_ECDSA_P256, parsed_ecdsa_spki.pubkey_algo);
}

void test_x509_edge_cases_and_corrupt_structures(void)
{
    SYN_X509_Cert cert;

    /* Non-constructed root tag */
    static const uint8_t primitive_root[] = {0x04, 0x02, 0x00, 0x00};
    TEST_ASSERT_FALSE(syn_x509_parse(primitive_root, sizeof(primitive_root), &cert));

    /* SEQUENCE with non-constructed TBS */
    static const uint8_t primitive_tbs[] = {0x30, 0x04, 0x04, 0x02, 0x00, 0x00};
    TEST_ASSERT_FALSE(syn_x509_parse(primitive_tbs, sizeof(primitive_tbs), &cert));

    /* TBS with non-integer serial (e.g. OCTET STRING instead of INTEGER) */
    static const uint8_t bad_serial_cert[] = {0x30, 0x0A, 0x30, 0x08, 0x04,
                                              0x01, 0x01, /* OCTET STRING 1 instead of INTEGER */
                                              0x30, 0x03, 0x01, 0x01, 0x00};
    TEST_ASSERT_FALSE(syn_x509_parse(bad_serial_cert, sizeof(bad_serial_cert), &cert));
}

void run_asn1_x509_tests(void)
{
    RUN_TEST(test_asn1_basic_tlv_parse);
    RUN_TEST(test_asn1_invalid_and_bounds_checks);
    RUN_TEST(test_ed25519_verify_basic);
    RUN_TEST(test_x509_cert_parse_and_chain);
    RUN_TEST(test_x509_ecdsa_p256_verification_and_chain);
    RUN_TEST(test_x509_parse_ecdsa_cert);
    RUN_TEST(test_x509_edge_cases_and_corrupt_structures);
}
