/**
 * @file test_protobuf.c
 * @brief Unity tests for Zero-Allocation Protocol Buffers Serializer & Deserializer.
 */

#include "mocks/mock_port.h"
#include "syntropic/syntropic.h"
#include "syntropic/util/syn_protobuf.h"
#include "unity/unity.h"

#include <string.h>

static uint8_t g_buf[128];
static SYN_PB_Encoder g_enc;
static SYN_PB_Decoder g_dec;

void test_protobuf_init_and_null_checks(void)
{
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_pb_encoder_init(NULL, g_buf, sizeof(g_buf)));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_pb_encoder_init(&g_enc, NULL, sizeof(g_buf)));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_pb_encoder_init(&g_enc, g_buf, 0));

    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_pb_decoder_init(NULL, g_buf, sizeof(g_buf)));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_pb_decoder_init(&g_dec, NULL, sizeof(g_buf)));

    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_pb_encode_varint(NULL, 0));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_pb_encode_tag(NULL, 1, 0));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_pb_encode_tag(&g_enc, 0, 0));

    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_pb_encode_int32(NULL, 1, 10));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_pb_encode_uint32(NULL, 1, 10));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_pb_encode_fixed32(NULL, 1, 10));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_pb_encode_bytes(NULL, 1, g_buf, 4));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_pb_encode_bytes(&g_enc, 1, NULL, 4));

    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_pb_encode_string(NULL, 1, "test"));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_pb_encode_string(&g_enc, 1, NULL));

    SYN_PB_Tag tag;
    uint64_t v64;
    uint32_t v32;
    const uint8_t *ptr;
    size_t len;

    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_pb_decode_tag(NULL, &tag));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_pb_decode_tag(&g_dec, NULL));

    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_pb_decode_varint(NULL, &v64));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_pb_decode_varint(&g_dec, NULL));

    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_pb_decode_fixed32(NULL, &v32));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_pb_decode_fixed32(&g_dec, NULL));

    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_pb_decode_bytes(NULL, &ptr, &len));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_pb_decode_bytes(&g_dec, NULL, &len));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_pb_decode_bytes(&g_dec, &ptr, NULL));

    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_pb_skip_field(NULL, 0));
}

void test_protobuf_encode_decode_roundtrip(void)
{
    syn_pb_encoder_init(&g_enc, g_buf, sizeof(g_buf));

    /* Encode fields: 1=int32(150), 2=string("Syntropic"), 3=fixed32(0x12345678), 4=bool(true) */
    TEST_ASSERT_EQUAL(SYN_OK, syn_pb_encode_int32(&g_enc, 1, 150));
    TEST_ASSERT_EQUAL(SYN_OK, syn_pb_encode_string(&g_enc, 2, "Syntropic"));
    TEST_ASSERT_EQUAL(SYN_OK, syn_pb_encode_fixed32(&g_enc, 3, 0x12345678));
    TEST_ASSERT_EQUAL(SYN_OK, syn_pb_encode_bool(&g_enc, 4, true));

    TEST_ASSERT_TRUE(g_enc.offset > 0);

    /* Decode fields */
    syn_pb_decoder_init(&g_dec, g_buf, g_enc.offset);
    SYN_PB_Tag tag;

    /* Field 1: int32 */
    TEST_ASSERT_EQUAL(SYN_OK, syn_pb_decode_tag(&g_dec, &tag));
    TEST_ASSERT_EQUAL(1, tag.field_number);
    TEST_ASSERT_EQUAL(SYN_PB_WIRE_VARINT, tag.wire_type);
    uint64_t val1 = 0;
    TEST_ASSERT_EQUAL(SYN_OK, syn_pb_decode_varint(&g_dec, &val1));
    TEST_ASSERT_EQUAL(150, (int32_t)val1);

    /* Field 2: string */
    TEST_ASSERT_EQUAL(SYN_OK, syn_pb_decode_tag(&g_dec, &tag));
    TEST_ASSERT_EQUAL(2, tag.field_number);
    TEST_ASSERT_EQUAL(SYN_PB_WIRE_LENGTH_DELIMITED, tag.wire_type);
    const uint8_t *str_bytes = NULL;
    size_t str_len = 0;
    TEST_ASSERT_EQUAL(SYN_OK, syn_pb_decode_bytes(&g_dec, &str_bytes, &str_len));
    TEST_ASSERT_EQUAL(9, str_len);
    TEST_ASSERT_EQUAL_MEMORY("Syntropic", str_bytes, 9);

    /* Field 3: fixed32 */
    TEST_ASSERT_EQUAL(SYN_OK, syn_pb_decode_tag(&g_dec, &tag));
    TEST_ASSERT_EQUAL(3, tag.field_number);
    TEST_ASSERT_EQUAL(SYN_PB_WIRE_FIXED32, tag.wire_type);
    uint32_t val3 = 0;
    TEST_ASSERT_EQUAL(SYN_OK, syn_pb_decode_fixed32(&g_dec, &val3));
    TEST_ASSERT_EQUAL(0x12345678, val3);

    /* Field 4: bool */
    TEST_ASSERT_EQUAL(SYN_OK, syn_pb_decode_tag(&g_dec, &tag));
    TEST_ASSERT_EQUAL(4, tag.field_number);
    TEST_ASSERT_EQUAL(SYN_PB_WIRE_VARINT, tag.wire_type);
    uint64_t val4 = 0;
    TEST_ASSERT_EQUAL(SYN_OK, syn_pb_decode_varint(&g_dec, &val4));
    TEST_ASSERT_EQUAL(1, val4);
}

void test_protobuf_skip_field(void)
{
    syn_pb_encoder_init(&g_enc, g_buf, sizeof(g_buf));
    syn_pb_encode_uint32(&g_enc, 1, 42);
    syn_pb_encode_fixed32(&g_enc, 2, 0xABCDEF00);
    syn_pb_encode_string(&g_enc, 3, "SkipMe");
    syn_pb_encode_uint32(&g_enc, 4, 99);

    /* Manually append fixed64 tag + 8 bytes data */
    syn_pb_encode_tag(&g_enc, 5, SYN_PB_WIRE_FIXED64);
    for (int i = 0; i < 8; i++) {
        g_buf[g_enc.offset++] = (uint8_t)i;
    }

    syn_pb_decoder_init(&g_dec, g_buf, g_enc.offset);
    SYN_PB_Tag tag;

    /* Decode field 1 */
    syn_pb_decode_tag(&g_dec, &tag);
    uint64_t v = 0;
    syn_pb_decode_varint(&g_dec, &v);
    TEST_ASSERT_EQUAL(42, v);

    /* Skip field 2 (fixed32) */
    syn_pb_decode_tag(&g_dec, &tag);
    TEST_ASSERT_EQUAL(SYN_OK, syn_pb_skip_field(&g_dec, tag.wire_type));

    /* Skip field 3 (string/bytes) */
    syn_pb_decode_tag(&g_dec, &tag);
    TEST_ASSERT_EQUAL(SYN_OK, syn_pb_skip_field(&g_dec, tag.wire_type));

    /* Decode field 4 */
    syn_pb_decode_tag(&g_dec, &tag);
    TEST_ASSERT_EQUAL(4, tag.field_number);
    syn_pb_decode_varint(&g_dec, &v);
    TEST_ASSERT_EQUAL(99, v);

    /* Skip field 5 (fixed64) */
    syn_pb_decode_tag(&g_dec, &tag);
    TEST_ASSERT_EQUAL(SYN_PB_WIRE_FIXED64, tag.wire_type);
    TEST_ASSERT_EQUAL(SYN_OK, syn_pb_skip_field(&g_dec, tag.wire_type));

    /* Invalid wire type skip */
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_pb_skip_field(&g_dec, 7));
}

void test_protobuf_overflow_and_errors(void)
{
    uint8_t tiny[1];
    syn_pb_encoder_init(&g_enc, tiny, sizeof(tiny));
    TEST_ASSERT_EQUAL(SYN_ERROR,
                      syn_pb_encode_varint(&g_enc, 300)); /* multi-byte varint overflow */
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_pb_encode_int32(&g_enc, 1, 100));
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_pb_encode_uint32(&g_enc, 1, 100));
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_pb_encode_fixed32(&g_enc, 1, 100));
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_pb_encode_bytes(&g_enc, 1, g_buf, 10));
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_pb_encode_string(&g_enc, 1, "LongerString"));

    /* End of stream decoder tests */
    syn_pb_decoder_init(&g_dec, tiny, 0);
    SYN_PB_Tag tag;
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_pb_decode_tag(&g_dec, &tag));

    uint32_t val32;
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_pb_decode_fixed32(&g_dec, &val32));

    const uint8_t *ptr;
    size_t len;
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_pb_decode_bytes(&g_dec, &ptr, &len));

    /* Truncated bytes length */
    uint8_t trunc_bytes[2] = {0x0A, 0x10}; /* Field 1, wire 2, length 16 (but payload size is 2) */
    syn_pb_decoder_init(&g_dec, trunc_bytes, sizeof(trunc_bytes));
    syn_pb_decode_tag(&g_dec, &tag);
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_pb_decode_bytes(&g_dec, &ptr, &len));

    /* Truncated fixed64 skip */
    uint8_t trunc_f64[1] = {0x09}; /* Field 1, wire 1 (fixed64) */
    syn_pb_decoder_init(&g_dec, trunc_f64, sizeof(trunc_f64));
    syn_pb_decode_tag(&g_dec, &tag);
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_pb_skip_field(&g_dec, tag.wire_type));

    /* Corrupt varint overflow (10 bytes with MSB set) */
    uint8_t bad_varint[10] = {0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80};
    syn_pb_decoder_init(&g_dec, bad_varint, sizeof(bad_varint));
    uint64_t v64;
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_pb_decode_varint(&g_dec, &v64));

    /* Decode tag varint error */
    syn_pb_decoder_init(&g_dec, bad_varint, sizeof(bad_varint));
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_pb_decode_tag(&g_dec, &tag));
}

void test_protobuf_decode_bytes_large_length(void)
{
    /* Craft a varint encoding SIZE_MAX (or near it) as the length prefix.
     * On 64-bit: 0xFF x9 + 0x01 encodes (2^63 - 1 + ...) → huge value.
     * On 32-bit: 0xFF x4 + 0x0F encodes 0xFFFFFFFF.
     * Either way, offset + length wraps past dec.size if using additive check. */
    uint8_t crafted[] = {0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0xAA};
    syn_pb_decoder_init(&g_dec, crafted, sizeof(crafted));

    const uint8_t *ptr;
    size_t len;
    /* Must reject: decoded length (0xFFFFFFFF) > remaining bytes */
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_pb_decode_bytes(&g_dec, &ptr, &len));

    /* Encode side: attempt to encode bytes with length > remaining capacity */
    uint8_t enc_buf[16];
    syn_pb_encoder_init(&g_enc, enc_buf, sizeof(enc_buf));
    /* Fill most of the buffer first */
    g_enc.offset = 14U;
    /* Attempt to encode a 10-byte payload with only 2 bytes remaining.
     * Even if tag+varint somehow succeed, the memcpy guard must catch it. */
    uint8_t payload[10] = {0};
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_pb_encode_bytes(&g_enc, 1, payload, sizeof(payload)));
}

void run_protobuf_tests(void)
{
    RUN_TEST(test_protobuf_init_and_null_checks);
    RUN_TEST(test_protobuf_encode_decode_roundtrip);
    RUN_TEST(test_protobuf_skip_field);
    RUN_TEST(test_protobuf_overflow_and_errors);
    RUN_TEST(test_protobuf_decode_bytes_large_length);
}
