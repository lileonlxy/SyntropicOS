/**
 * @file test_ntp_server.c
 * @brief Unity tests for Zero-Heap NTP Server Protocol Engine.
 */

#include "mocks/mock_port.h"
#include "syntropic/net/syn_ntp_server.h"
#include "syntropic/syntropic.h"
#include "unity/unity.h"

static SYN_NTPServer g_ntp_server;

static uint32_t mock_get_epoch(void)
{
    return 1700000000UL; /* 2023-11-14 22:13:20 UTC */
}

void test_ntp_server_init_and_null_checks(void)
{
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_ntp_server_init(NULL, 1, mock_get_epoch));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_ntp_server_init(&g_ntp_server, 0, mock_get_epoch));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_ntp_server_init(&g_ntp_server, 16, mock_get_epoch));

    TEST_ASSERT_EQUAL(SYN_OK, syn_ntp_server_init(&g_ntp_server, 2, mock_get_epoch));

    uint8_t req[48] = {0};
    uint8_t resp[48] = {0};
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_ntp_server_process_packet(NULL, req, resp));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_ntp_server_process_packet(&g_ntp_server, NULL, resp));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_ntp_server_process_packet(&g_ntp_server, req, NULL));
}

void test_ntp_server_process_packet(void)
{
    syn_ntp_server_init(&g_ntp_server, 2, mock_get_epoch);

    /* Format valid client request (mode = 3) */
    uint8_t req[48] = {0};
    req[0] = 0x23; /* NTPv4 Client request */
    req[40] = 0xAA;
    req[41] = 0xBB;
    req[42] = 0xCC;
    req[43] = 0xDD;

    uint8_t resp[48] = {0};
    TEST_ASSERT_EQUAL(SYN_OK, syn_ntp_server_process_packet(&g_ntp_server, req, resp));

    /* Verify response fields */
    TEST_ASSERT_EQUAL(0x24, resp[0]); /* Server response mode */
    TEST_ASSERT_EQUAL(2, resp[1]);    /* Stratum 2 */
    TEST_ASSERT_EQUAL('L', resp[12]);
    TEST_ASSERT_EQUAL('O', resp[13]);
    TEST_ASSERT_EQUAL('C', resp[14]);
    TEST_ASSERT_EQUAL('L', resp[15]);

    /* Verify Originate Timestamp matches Client Transmit Timestamp */
    TEST_ASSERT_EQUAL(0xAA, resp[24]);
    TEST_ASSERT_EQUAL(0xBB, resp[25]);
    TEST_ASSERT_EQUAL(0xCC, resp[26]);
    TEST_ASSERT_EQUAL(0xDD, resp[27]);

    /* Verify invalid client mode rejection */
    req[0] = 0x24; /* Mode 4 Server packet, invalid as request */
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_ntp_server_process_packet(&g_ntp_server, req, resp));

    /* Verify symmetric active client request mode (mode = 1) */
    req[0] = 0x21; /* Mode 1 */
    TEST_ASSERT_EQUAL(SYN_OK, syn_ntp_server_process_packet(&g_ntp_server, req, resp));

    /* Verify processing when get_epoch_sec_cb is NULL */
    syn_ntp_server_init(&g_ntp_server, 1, NULL);
    req[0] = 0x23;
    TEST_ASSERT_EQUAL(SYN_OK, syn_ntp_server_process_packet(&g_ntp_server, req, resp));
}

void run_ntp_server_tests(void)
{
    RUN_TEST(test_ntp_server_init_and_null_checks);
    RUN_TEST(test_ntp_server_process_packet);
}
