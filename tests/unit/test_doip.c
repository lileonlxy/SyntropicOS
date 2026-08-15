/**
 * @file test_doip.c
 * @brief Unity unit tests for SyntropicOS ISO 13400-2 (DoIP) Protocol Engine.
 */

#include "syntropic/proto/syn_doip.h"
#include "syntropic/proto/syn_uds.h"
#include "unity/unity.h"

#include <string.h>

static SYN_DoIP_Server g_doip;
static SYN_UDS_Server g_uds;

static void setup_doip_test(void)
{
    syn_doip_init(&g_doip, 0x1001);
    syn_uds_init(&g_uds);
}

void test_doip_init_and_identifiers(void)
{
    setup_doip_test();
    TEST_ASSERT_FALSE(syn_doip_init(NULL, 0x1001));
    TEST_ASSERT_FALSE(syn_doip_set_identifiers(NULL, NULL, NULL, NULL));

    uint8_t vin[17] = {'S', 'Y', 'N', 'T', 'R', 'O', 'P', 'I', 'C',
                       'O', 'S', '1', '2', '3', '4', '5', '6'};
    uint8_t eid[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    uint8_t gid[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

    TEST_ASSERT_TRUE(syn_doip_set_identifiers(&g_doip, vin, eid, gid));
    TEST_ASSERT_EQUAL_MEMORY(vin, g_doip.vin, 17);
    TEST_ASSERT_EQUAL_MEMORY(eid, g_doip.eid, 6);
    TEST_ASSERT_EQUAL_MEMORY(gid, g_doip.gid, 6);
}

void test_doip_header_parse_encode(void)
{
    setup_doip_test();
    SYN_DoIP_Header hdr = {.protocol_version = SYN_DOIP_PROTOCOL_VERSION,
                           .inverse_protocol_version = SYN_DOIP_INVERSE_PROTOCOL_VERSION,
                           .payload_type = SYN_DOIP_TYPE_VEHICLE_ID_REQ,
                           .payload_length = 0};

    uint8_t buf[16] = {0};
    TEST_ASSERT_EQUAL_UINT16(8, syn_doip_encode_header(&hdr, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_UINT16(0, syn_doip_encode_header(NULL, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_UINT16(0, syn_doip_encode_header(&hdr, NULL, sizeof(buf)));
    TEST_ASSERT_EQUAL_UINT16(0, syn_doip_encode_header(&hdr, buf, 7));

    SYN_DoIP_Header parsed_hdr;
    TEST_ASSERT_FALSE(syn_doip_parse_header(NULL, sizeof(buf), &parsed_hdr));
    TEST_ASSERT_FALSE(syn_doip_parse_header(buf, 7, &parsed_hdr));
    TEST_ASSERT_TRUE(syn_doip_parse_header(buf, sizeof(buf), &parsed_hdr));
    TEST_ASSERT_EQUAL_HEX8(SYN_DOIP_PROTOCOL_VERSION, parsed_hdr.protocol_version);
    TEST_ASSERT_EQUAL_HEX16(SYN_DOIP_TYPE_VEHICLE_ID_REQ, parsed_hdr.payload_type);

    /* Test invalid protocol version */
    buf[0] = 0xFF;
    TEST_ASSERT_FALSE(syn_doip_parse_header(buf, sizeof(buf), &parsed_hdr));
}

void test_doip_vehicle_identification_request(void)
{
    setup_doip_test();
    uint8_t vin[17] = {'S', 'Y', 'N', 'T', 'R', 'O', 'P', 'I', 'C',
                       'O', 'S', '1', '2', '3', '4', '5', '6'};
    uint8_t eid[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    uint8_t gid[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    syn_doip_set_identifiers(&g_doip, vin, eid, gid);

    uint8_t rx[8] = {0x02, 0xFD, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00};
    uint8_t tx[64] = {0};
    uint16_t tx_len = 0;

    TEST_ASSERT_TRUE(
        syn_doip_process_msg(&g_doip, &g_uds, rx, sizeof(rx), tx, sizeof(tx), &tx_len));
    TEST_ASSERT_EQUAL_UINT16(41, tx_len); /* 8-byte header + 33-byte payload */

    SYN_DoIP_Header resp_hdr;
    TEST_ASSERT_TRUE(syn_doip_parse_header(tx, tx_len, &resp_hdr));
    TEST_ASSERT_EQUAL_HEX16(SYN_DOIP_TYPE_VEHICLE_ANNOUNCEMENT, resp_hdr.payload_type);
    TEST_ASSERT_EQUAL_MEMORY(vin, &tx[8], 17);
}

void test_doip_routing_activation_request(void)
{
    setup_doip_test();
    uint8_t rx[15] = {
        0x02, 0xFD, 0x00, 0x05, 0x00, 0x00, 0x00, 0x07, /* Header (payload_len = 7) */
        0x0E, 0x80,                                     /* Tester logical address 0x0E80 */
        0x00,                                           /* Activation type 0x00 */
        0x00, 0x00, 0x00, 0x00                          /* Reserved 4 bytes */
    };
    uint8_t tx[64] = {0};
    uint16_t tx_len = 0;

    TEST_ASSERT_TRUE(
        syn_doip_process_msg(&g_doip, &g_uds, rx, sizeof(rx), tx, sizeof(tx), &tx_len));
    TEST_ASSERT_EQUAL_UINT16(17, tx_len); /* 8-byte header + 9-byte payload */
    TEST_ASSERT_TRUE(g_doip.routing_activated);
    TEST_ASSERT_EQUAL_HEX16(0x0E80, g_doip.active_tester_address);

    SYN_DoIP_Header resp_hdr;
    TEST_ASSERT_TRUE(syn_doip_parse_header(tx, tx_len, &resp_hdr));
    TEST_ASSERT_EQUAL_HEX16(SYN_DOIP_TYPE_ROUTING_ACTIVATION_RESP, resp_hdr.payload_type);
}

void test_doip_diagnostic_message_uds_loopback(void)
{
    setup_doip_test();
    /* Register VIN DID 0xF190 in UDS */
    uint8_t vin_data[] = "SYNTROPICOS123456";
    syn_uds_register_did(&g_uds, 0xF190, vin_data, sizeof(vin_data) - 1, false);

    /* DoIP Diagnostic Request: Tester 0x0E80 -> ECU 0x1001, UDS ReadDID 0xF190 (0x22 0xF1 0x90) */
    uint8_t rx[15] = {
        0x02, 0xFD, 0x80, 0x01, 0x00, 0x00, 0x00, 0x07, /* Header (payload_len = 7) */
        0x0E, 0x80,                                     /* Source Address 0x0E80 */
        0x10, 0x01,                                     /* Target Address 0x1001 */
        0x22, 0xF1, 0x90                                /* UDS Request: 0x22 ReadDID 0xF190 */
    };
    uint8_t tx[256] = {0};
    uint16_t tx_len = 0;

    TEST_ASSERT_TRUE(
        syn_doip_process_msg(&g_doip, &g_uds, rx, sizeof(rx), tx, sizeof(tx), &tx_len));
    TEST_ASSERT_EQUAL_UINT16(8 + 4 + 3 + 17, tx_len); /* 8 hdr + 4 addresses + 3 UDS hdr + 17 VIN */

    SYN_DoIP_Header resp_hdr;
    TEST_ASSERT_TRUE(syn_doip_parse_header(tx, tx_len, &resp_hdr));
    TEST_ASSERT_EQUAL_HEX16(SYN_DOIP_TYPE_DIAGNOSTIC_MSG, resp_hdr.payload_type);

    /* Verify UDS Positive Response (0x62 0xF1 0x90 ...) at tx[12..] */
    TEST_ASSERT_EQUAL_HEX8(0x62, tx[12]);
    TEST_ASSERT_EQUAL_HEX8(0xF1, tx[13]);
    TEST_ASSERT_EQUAL_HEX8(0x90, tx[14]);
}

void test_doip_error_nacks(void)
{
    setup_doip_test();
    uint8_t tx[64] = {0};
    uint16_t tx_len = 0;

    /* Invalid protocol version pattern */
    uint8_t rx_bad_ver[8] = {0xFF, 0xFD, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00};
    TEST_ASSERT_TRUE(syn_doip_process_msg(&g_doip, &g_uds, rx_bad_ver, 8, tx, sizeof(tx), &tx_len));
    TEST_ASSERT_EQUAL_HEX8(SYN_DOIP_NACK_INCORRECT_PATTERN, tx[8]);

    /* Invalid payload type */
    uint8_t rx_bad_type[8] = {0x02, 0xFD, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00};
    TEST_ASSERT_TRUE(
        syn_doip_process_msg(&g_doip, &g_uds, rx_bad_type, 8, tx, sizeof(tx), &tx_len));
    TEST_ASSERT_EQUAL_HEX8(SYN_DOIP_NACK_UNKNOWN_PAYLOAD_TYPE, tx[8]);
    TEST_ASSERT_FALSE(syn_doip_process_msg(&g_doip, &g_uds, rx_bad_type, 8, tx, 8, &tx_len));

    /* Null pointer check */
    TEST_ASSERT_FALSE(syn_doip_process_msg(NULL, &g_uds, rx_bad_type, 8, tx, sizeof(tx), &tx_len));

    /* Truncated payload length vs header claim */
    uint8_t rx_trunc[8] = {0x02, 0xFD, 0x00, 0x01, 0x00, 0x00, 0x00, 0x05};
    TEST_ASSERT_TRUE(syn_doip_process_msg(&g_doip, &g_uds, rx_trunc, 8, tx, sizeof(tx), &tx_len));
    TEST_ASSERT_EQUAL_HEX8(SYN_DOIP_NACK_INVALID_PAYLOAD_LENGTH, tx[8]);

    /* Buffer capacity too small for vehicle announcement */
    uint8_t rx_veh_id[8] = {0x02, 0xFD, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00};
    TEST_ASSERT_FALSE(syn_doip_process_msg(&g_doip, &g_uds, rx_veh_id, 8, tx, 10, &tx_len));

    /* Routing activation short payload (<7) */
    uint8_t rx_route_short[10] = {0x02, 0xFD, 0x00, 0x05, 0x00, 0x00, 0x00, 0x02, 0x0E, 0x80};
    TEST_ASSERT_TRUE(
        syn_doip_process_msg(&g_doip, &g_uds, rx_route_short, 10, tx, sizeof(tx), &tx_len));
    TEST_ASSERT_EQUAL_HEX8(SYN_DOIP_NACK_INVALID_PAYLOAD_LENGTH, tx[8]);

    /* Buffer capacity too small for routing activation response */
    uint8_t rx_route[15] = {0x02, 0xFD, 0x00, 0x05, 0x00, 0x00, 0x00, 0x07,
                            0x0E, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00};
    TEST_ASSERT_FALSE(syn_doip_process_msg(&g_doip, &g_uds, rx_route, 15, tx, 10, &tx_len));

    /* Diagnostic message short payload (<4) */
    uint8_t rx_diag_short[10] = {0x02, 0xFD, 0x80, 0x01, 0x00, 0x00, 0x00, 0x02, 0x0E, 0x80};
    TEST_ASSERT_TRUE(
        syn_doip_process_msg(&g_doip, &g_uds, rx_diag_short, 10, tx, sizeof(tx), &tx_len));
    TEST_ASSERT_EQUAL_HEX8(SYN_DOIP_NACK_INVALID_PAYLOAD_LENGTH, tx[8]);

    /* Target address mismatch diagnostic message NACK */
    uint8_t rx_wrong_ta[15] = {0x02, 0xFD, 0x80, 0x01, 0x00, 0x00, 0x00, 0x07,
                               0x0E, 0x80, 0x99, 0x99, 0x22, 0xF1, 0x90};
    TEST_ASSERT_TRUE(
        syn_doip_process_msg(&g_doip, &g_uds, rx_wrong_ta, 15, tx, sizeof(tx), &tx_len));
    TEST_ASSERT_EQUAL_UINT16(13, tx_len);
    TEST_ASSERT_EQUAL_HEX8(0x02, tx[12]); /* NACK code: Unknown target address */

    /* Buffer capacity too small for diagnostic message NACK */
    TEST_ASSERT_FALSE(syn_doip_process_msg(&g_doip, &g_uds, rx_wrong_ta, 15, tx, 10, &tx_len));

    /* Buffer capacity too small for diagnostic message response */
    uint8_t rx_diag[15] = {0x02, 0xFD, 0x80, 0x01, 0x00, 0x00, 0x00, 0x07,
                           0x0E, 0x80, 0x10, 0x01, 0x22, 0xF1, 0x90};
    TEST_ASSERT_FALSE(syn_doip_process_msg(&g_doip, &g_uds, rx_diag, 15, tx, 10, &tx_len));
}

void test_doip_payload_length_integer_overflow(void)
{
    setup_doip_test();
    /* Craft a DoIP packet claiming 0xFFFFFFFF payload bytes in an 8-byte packet */
    uint8_t rx_overflow[8] = {0x02, 0xFD, 0x80, 0x01, 0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t tx[64] = {0};
    uint16_t tx_len = 0;

    TEST_ASSERT_TRUE(syn_doip_process_msg(&g_doip, &g_uds, rx_overflow, sizeof(rx_overflow), tx,
                                          sizeof(tx), &tx_len));
    TEST_ASSERT_EQUAL_UINT16(9, tx_len);
    TEST_ASSERT_EQUAL_HEX8(SYN_DOIP_NACK_INVALID_PAYLOAD_LENGTH, tx[8]);
}

void run_doip_tests(void)
{
    RUN_TEST(test_doip_init_and_identifiers);
    RUN_TEST(test_doip_header_parse_encode);
    RUN_TEST(test_doip_vehicle_identification_request);
    RUN_TEST(test_doip_routing_activation_request);
    RUN_TEST(test_doip_diagnostic_message_uds_loopback);
    RUN_TEST(test_doip_error_nacks);
    RUN_TEST(test_doip_payload_length_integer_overflow);
}
