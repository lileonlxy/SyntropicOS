/**
 * @file test_dnssd.c
 * @brief Unit tests for DNS-SD (RFC 6763 / RFC 6762) service discovery engine.
 */

#include "mocks/mock_port.h"
#include "syntropic/net/syn_dnssd.h"
#include "unity/unity.h"

#include <string.h>

void test_dnssd_init_and_registration(void)
{
    mock_port_reset();

    SYN_DnsSd sd;
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_dnssd_init(&sd));

    /* Null checks on init */
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_dnssd_init(NULL));

    /* Mock socket open failure */
    mock_udp_open_ok = false;
    SYN_DnsSd fail_sd;
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_dnssd_init(&fail_sd));
    mock_udp_open_ok = true;

    /* Mock multicast join failure */
    mock_udp_multicast_join_ok = false;
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_dnssd_init(&fail_sd));
    mock_udp_multicast_join_ok = true;

    SYN_DnsSd_Service svc1 = {.instance_name = "Kitchen Light",
                              .service_type = "_http._tcp",
                              .hostname = "kitchen-node",
                              .port = 80,
                              .ip = {192, 168, 1, 100},
                              .txt_records = {"version=1.0", "vendor=Syntropic"},
                              .txt_count = 2};

    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_dnssd_register(&sd, &svc1));

    /* Null parameter checks */
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_dnssd_register(NULL, &svc1));
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_dnssd_register(&sd, NULL));

    SYN_DnsSd_Service bad_svc = svc1;
    bad_svc.instance_name = NULL;
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_dnssd_register(&sd, &bad_svc));

    bad_svc = svc1;
    bad_svc.service_type = NULL;
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_dnssd_register(&sd, &bad_svc));

    bad_svc = svc1;
    bad_svc.hostname = NULL;
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_dnssd_register(&sd, &bad_svc));

    bad_svc = svc1;
    bad_svc.port = 0;
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_dnssd_register(&sd, &bad_svc));

    /* Single label service type without dots */
    SYN_DnsSd_Service svc2 = svc1;
    svc2.service_type = "myservice";
    svc2.port = 5683;
    svc2.txt_count = 0;
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_dnssd_register(&sd, &svc2));

    SYN_DnsSd_Service svc3 = svc1;
    svc3.service_type = "_mqtt._tcp";
    svc3.port = 1883;
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_dnssd_register(&sd, &svc3));

    SYN_DnsSd_Service svc4 = svc1;
    svc4.service_type = "_ws._tcp";
    svc4.port = 8080;
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_dnssd_register(&sd, &svc4));

    /* 5th registration must fail (table max = 4) */
    SYN_DnsSd_Service svc5 = svc1;
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_dnssd_register(&sd, &svc5));
}

void test_dnssd_query_and_response_generation(void)
{
    mock_port_reset();

    SYN_DnsSd sd;
    syn_dnssd_init(&sd);

    SYN_DnsSd_Service http_svc = {.instance_name = "Syntropic Sensor",
                                  .service_type = "_http._tcp",
                                  .hostname = "sensor-node",
                                  .port = 8080,
                                  .ip = {10, 0, 0, 42},
                                  .txt_records = {"id=123", "model=PRO"},
                                  .txt_count = 2};
    syn_dnssd_register(&sd, &http_svc);

    SYN_DnsSd_Service empty_txt_svc = {.instance_name = "Empty TXT",
                                       .service_type = "_coap._udp",
                                       .hostname = "coap-node",
                                       .port = 5683,
                                       .ip = {10, 0, 0, 43},
                                       .txt_count = 0};
    syn_dnssd_register(&sd, &empty_txt_svc);

    /* Construct query for _http._tcp.local */
    uint8_t query[64] = {
        0x00, 0x00, /* ID 0 */
        0x00, 0x00, /* Flags: Standard Query */
        0x00, 0x01, /* QDCOUNT = 1 */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 5,   '_', 'h', 't', 't', 'p',
        4,    '_',  't',  'c',  'p',  5,    'l', 'o', 'c', 'a', 'l', 0, /* Root */
        0x00, 0x0C,                                                     /* QTYPE = PTR (12) */
        0x00, 0x01                                                      /* QCLASS = IN (1) */
    };
    size_t qlen = 12 + 6 + 5 + 6 + 1 + 4;

    uint8_t resp[1024];
    size_t resp_len = 0;

    /* Query matching registered service */
    TEST_ASSERT_EQUAL_INT(SYN_OK,
                          syn_dnssd_process_query(&sd, query, qlen, resp, sizeof(resp), &resp_len));
    TEST_ASSERT_TRUE(resp_len > 0);

    /* Verify response header */
    uint16_t resp_flags = ((uint16_t)resp[2] << 8) | resp[3];
    TEST_ASSERT_EQUAL_HEX16(0x8400, resp_flags); /* Authoritative Response */
    uint16_t ancount = ((uint16_t)resp[6] << 8) | resp[7];
    TEST_ASSERT_EQUAL_UINT16(1, ancount); /* 1 PTR Answer */
    uint16_t arcount = ((uint16_t)resp[10] << 8) | resp[11];
    TEST_ASSERT_EQUAL_UINT16(3, arcount); /* 3 Additional Records (SRV, TXT, A) */

    /* Query for empty TXT service */
    uint8_t coap_query[64] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
                              0x00, 0x00, 0x00, 5,    '_',  'c',  'o',  'a',  'p',
                              4,    '_',  'u',  'd',  'p',  5,    'l',  'o',  'c',
                              'a',  'l',  0,    0x00, 0x0C, 0x00, 0x01};
    size_t coap_qlen = 12 + 6 + 5 + 6 + 1 + 4;
    TEST_ASSERT_EQUAL_INT(
        SYN_OK, syn_dnssd_process_query(&sd, coap_query, coap_qlen, resp, sizeof(resp), &resp_len));

    /* Register and query single-label service (no dot) */
    SYN_DnsSd_Service single_svc = {.instance_name = "Single",
                                    .service_type = "single",
                                    .hostname = "single-node",
                                    .port = 80,
                                    .ip = {10, 0, 0, 50},
                                    .txt_count = 0};
    syn_dnssd_register(&sd, &single_svc);
    uint8_t single_query[64] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
                                0x00, 0x00, 6,    's',  'i',  'n',  'g',  'l',  'e',  5,
                                'l',  'o',  'c',  'a',  'l',  0,    0x00, 0x0C, 0x00, 0x01};
    size_t single_qlen = 12 + 7 + 6 + 1 + 4;
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_dnssd_process_query(&sd, single_query, single_qlen, resp,
                                                          sizeof(resp), &resp_len));

    /* Query for unknown service */
    uint8_t unk_query[64] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
                             0x00, 4,    '_',  'f',  'o',  'o',  4,    '_',  't',  'c',  'p',
                             5,    'l',  'o',  'c',  'a',  'l',  0,    0x00, 0x0C, 0x00, 0x01};
    size_t unk_qlen = 12 + 5 + 5 + 6 + 1 + 4;
    TEST_ASSERT_EQUAL_INT(SYN_NOT_FOUND, syn_dnssd_process_query(&sd, unk_query, unk_qlen, resp,
                                                                 sizeof(resp), &resp_len));

    /* Malformed label query */
    uint8_t bad_label_query[64] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 70,   '_',  'h',  't',  't',  'p' /* Label > 63 */
    };
    TEST_ASSERT_EQUAL_INT(SYN_NOT_FOUND, syn_dnssd_process_query(&sd, bad_label_query, 18, resp,
                                                                 sizeof(resp), &resp_len));

    /* Null and error bounds */
    TEST_ASSERT_EQUAL_INT(
        SYN_ERROR, syn_dnssd_process_query(NULL, query, qlen, resp, sizeof(resp), &resp_len));
    TEST_ASSERT_EQUAL_INT(SYN_ERROR,
                          syn_dnssd_process_query(&sd, NULL, qlen, resp, sizeof(resp), &resp_len));
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_dnssd_process_query(&sd, query, 5, resp, sizeof(resp),
                                                             &resp_len)); /* Truncated */
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_dnssd_process_query(&sd, query, qlen, resp, 64,
                                                             &resp_len)); /* Small buffer */

    /* Non-query flag reject */
    uint8_t resp_flag_pkt[64];
    memcpy(resp_flag_pkt, query, qlen);
    resp_flag_pkt[2] = 0x80; /* Set QR bit to 1 */
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_dnssd_process_query(&sd, resp_flag_pkt, qlen, resp,
                                                             sizeof(resp), &resp_len));

    /* qdcount = 0 reject */
    uint8_t zero_qd_pkt[64];
    memcpy(zero_qd_pkt, query, qlen);
    zero_qd_pkt[4] = 0x00;
    zero_qd_pkt[5] = 0x00;
    TEST_ASSERT_EQUAL_INT(
        SYN_ERROR, syn_dnssd_process_query(&sd, zero_qd_pkt, qlen, resp, sizeof(resp), &resp_len));
}

void test_dnssd_task_execution(void)
{
    mock_port_reset();

    SYN_DnsSd sd;
    syn_dnssd_init(&sd);

    SYN_DnsSd_Service svc = {.instance_name = "Syntropic CoAP",
                             .service_type = "_coap._udp",
                             .hostname = "coap-node",
                             .port = 5683,
                             .ip = {192, 168, 1, 50},
                             .txt_count = 0};
    syn_dnssd_register(&sd, &svc);

    SYN_PT pt;
    PT_INIT(&pt);
    SYN_Task task = {.user_data = &sd};

    /* Run one step with no packets */
    SYN_PT_Status st = syn_dnssd_task(&pt, &task);
    TEST_ASSERT_EQUAL(PT_WAITING, st);

    /* Inject packet to UDP socket and run task */
    uint8_t query[64] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                         5,    '_',  'c',  'o',  'a',  'p',  4,    '_',  'u',  'd',  'p',  5,
                         'l',  'o',  'c',  'a',  'l',  0,    0x00, 0x0C, 0x00, 0x01};
    size_t qlen = 12 + 6 + 5 + 6 + 1 + 4;
    SYN_SockAddr from_addr = {.ip = {192, 168, 1, 1}, .port = 5353};
    mock_udp_inject_packet(query, qlen, &from_addr);

    st = syn_dnssd_task(&pt, &task);
    TEST_ASSERT_EQUAL(PT_WAITING, st);
}

void test_dnssd_announce(void)
{
    mock_port_reset();

    SYN_DnsSd sd;
    syn_dnssd_init(&sd);

    SYN_DnsSd_Service svc = {.instance_name = "Syntropic Announce",
                             .service_type = "_http._tcp",
                             .hostname = "http-node",
                             .port = 8080,
                             .ip = {192, 168, 1, 100},
                             .txt_count = 0};
    syn_dnssd_register(&sd, &svc);

    uint8_t buf[1024];
    size_t len = 0;

    /* Valid announcement */
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_dnssd_announce(&sd, 0, buf, sizeof(buf), &len));
    TEST_ASSERT_TRUE(len > 0);

    /* Null and bounds checks */
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_dnssd_announce(NULL, 0, buf, sizeof(buf), &len));
    TEST_ASSERT_EQUAL_INT(SYN_ERROR,
                          syn_dnssd_announce(&sd, 1, buf, sizeof(buf), &len)); /* Out of range */
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_dnssd_announce(&sd, 0, NULL, sizeof(buf), &len));
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_dnssd_announce(&sd, 0, buf, sizeof(buf), NULL));
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_dnssd_announce(&sd, 0, buf, 64, &len)); /* Small buffer */
}

void test_dnssd_query_and_response_parsing(void)
{
    uint8_t qbuf[256];
    size_t qlen = 0;

    /* Build query */
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_dnssd_build_query("_http._tcp", qbuf, sizeof(qbuf), &qlen));
    TEST_ASSERT_TRUE(qlen > 0);

    /* Null and bounds checks */
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_dnssd_build_query(NULL, qbuf, sizeof(qbuf), &qlen));
    TEST_ASSERT_EQUAL_INT(SYN_ERROR,
                          syn_dnssd_build_query("_http._tcp", NULL, sizeof(qbuf), &qlen));
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_dnssd_build_query("_http._tcp", qbuf, sizeof(qbuf), NULL));
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_dnssd_build_query("_http._tcp", qbuf, 32, &qlen));

    /* Build response using announce and then parse it back */
    SYN_DnsSd sd;
    syn_dnssd_init(&sd);
    SYN_DnsSd_Service svc = {.instance_name = "Discovered Node",
                             .service_type = "_coap._udp",
                             .hostname = "coap-host",
                             .port = 5683,
                             .ip = {10, 0, 0, 42},
                             .txt_records = {"path=/sensors", "ver=2"},
                             .txt_count = 2};
    syn_dnssd_register(&sd, &svc);

    uint8_t resp[1024];
    size_t rlen = 0;
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_dnssd_announce(&sd, 0, resp, sizeof(resp), &rlen));

    SYN_DnsSd_Discovered disc;
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_dnssd_parse_response(resp, rlen, &disc));
    TEST_ASSERT_EQUAL_STRING("Discovered Node", disc.instance_name);
    TEST_ASSERT_EQUAL_STRING("_coap._udp", disc.service_type);
    TEST_ASSERT_EQUAL_STRING("coap-host", disc.hostname);
    TEST_ASSERT_EQUAL_UINT16(5683, disc.port);
    TEST_ASSERT_EQUAL_UINT8(10, disc.ip[0]);
    TEST_ASSERT_EQUAL_UINT8(42, disc.ip[3]);
    TEST_ASSERT_EQUAL_STRING("path=/sensors", disc.txt);

    /* Discover transmit */
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_dnssd_discover(&sd, "_coap._udp"));
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_dnssd_discover(NULL, "_coap._udp"));
    SYN_DnsSd bad_sd = sd;
    bad_sd.sock = SYN_SOCKET_INVALID;
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_dnssd_discover(&bad_sd, "_coap._udp"));
    uint8_t small_qbuf[64];
    size_t small_qlen = 0;
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_dnssd_build_query("_coap._udp", small_qbuf,
                                                           sizeof(small_qbuf), &small_qlen));

    /* Error parsing checks */
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_dnssd_parse_response(NULL, rlen, &disc));
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_dnssd_parse_response(resp, 5, &disc));
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_dnssd_parse_response(resp, rlen, NULL));
    resp[2] = 0x00; /* Force QR=0 (query, not response) */
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_dnssd_parse_response(resp, rlen, &disc));

    /* Response with total_rr == 0 */
    uint8_t zero_rr_resp[12] = {0, 0, 0x84, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    TEST_ASSERT_EQUAL_INT(SYN_ERROR,
                          syn_dnssd_parse_response(zero_rr_resp, sizeof(zero_rr_resp), &disc));

    /* Response with pointer compression and truncated labels */
    uint8_t ptr_resp[32] = {
        0,    0,    0x84, 0,    0,    0,    0,    1,    0,    0,    0, 0, /* Header: 1 answer */
        0xC0, 0x0C,                                                       /* Pointer to offset 12 */
        0x00, 0x0C, 0x00, 0x01, 0x00, 0x00, 0x00, 0x78, 0x00, 0x04,       /* PTR record, rdlen 4 */
        0x03, 'f',  'o',  'o'};
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_dnssd_parse_response(ptr_resp, sizeof(ptr_resp), &disc));

    /* Truncated RR header and truncated RDATA */
    uint8_t trunc_rr_resp[16] = {
        0, 0, 0x84, 0, 0,    0,   0,   1,
        0, 0, 0,    0, 0x03, 'f', 'o', 'o' /* No room for 10-byte RR header */
    };
    TEST_ASSERT_EQUAL_INT(SYN_OK,
                          syn_dnssd_parse_response(trunc_rr_resp, sizeof(trunc_rr_resp), &disc));

    uint8_t trunc_rdata_resp[24] = {
        0,    0,    0x84, 0,    0,    0,    0,    1,    0,    0,   0, 0, 0x00, /* Null name */
        0x00, 0x0C, 0x00, 0x01, 0x00, 0x00, 0x00, 0x78, 0x00, 0x20 /* rdlen 32 > remaining */
    };
    TEST_ASSERT_EQUAL_INT(
        SYN_OK, syn_dnssd_parse_response(trunc_rdata_resp, sizeof(trunc_rdata_resp), &disc));

    /* Truncated label length */
    uint8_t trunc_label_resp[20] = {
        0, 0, 0x84, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0x20, 'a', 'b' /* label length 32 > buffer */
    };
    TEST_ASSERT_EQUAL_INT(
        SYN_OK, syn_dnssd_parse_response(trunc_label_resp, sizeof(trunc_label_resp), &disc));
}

static void on_discovered_service(const SYN_DnsSd_Discovered *svc, void *user_data)
{
    int *count = (int *)user_data;
    if (svc != NULL && count != NULL) {
        (*count)++;
    }
}

void test_dnssd_browser_task_coroutine(void)
{
    mock_port_reset();

    SYN_DnsSd sd;
    syn_dnssd_init(&sd);

    SYN_DnsSd_Browser browser;
    int discovered = 0;

    /* Init browser */
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_dnssd_browser_init(&browser, &sd, "_coap._udp",
                                                         on_discovered_service, &discovered, 1000));
    TEST_ASSERT_TRUE(browser.active);

    /* Null and bounds checks */
    TEST_ASSERT_EQUAL_INT(
        SYN_ERROR,
        syn_dnssd_browser_init(NULL, &sd, "_coap._udp", on_discovered_service, &discovered, 1000));
    TEST_ASSERT_EQUAL_INT(SYN_ERROR,
                          syn_dnssd_browser_init(&browser, NULL, "_coap._udp",
                                                 on_discovered_service, &discovered, 1000));
    TEST_ASSERT_EQUAL_INT(
        SYN_ERROR,
        syn_dnssd_browser_init(&browser, &sd, NULL, on_discovered_service, &discovered, 1000));
    TEST_ASSERT_EQUAL_INT(
        SYN_ERROR, syn_dnssd_browser_init(&browser, &sd, "_coap._udp", NULL, &discovered, 1000));

    SYN_PT pt;
    PT_INIT(&pt);
    SYN_Task task = {.user_data = &browser};

    /* Run coroutine step - no packets, should wait */
    SYN_PT_Status st = syn_dnssd_browse_task(&pt, &task);
    TEST_ASSERT_EQUAL(PT_WAITING, st);
    TEST_ASSERT_EQUAL_INT(0, discovered);

    /* Create response packet and inject */
    SYN_DnsSd_Service svc = {.instance_name = "Sensors Node",
                             .service_type = "_coap._udp",
                             .hostname = "sensors-1",
                             .port = 5683,
                             .ip = {192, 168, 1, 50},
                             .txt_count = 0};
    SYN_DnsSd remote_sd;
    syn_dnssd_init(&remote_sd);
    syn_dnssd_register(&remote_sd, &svc);

    uint8_t resp[1024];
    size_t rlen = 0;
    syn_dnssd_announce(&remote_sd, 0, resp, sizeof(resp), &rlen);

    SYN_SockAddr from_addr = {.ip = {192, 168, 1, 50}, .port = 5353};
    mock_udp_inject_packet(resp, rlen, &from_addr);

    /* Run coroutine step - should receive and invoke callback */
    st = syn_dnssd_browse_task(&pt, &task);
    TEST_ASSERT_EQUAL(PT_WAITING, st);
    TEST_ASSERT_EQUAL_INT(1, discovered);
    TEST_ASSERT_EQUAL_UINT32(1, browser.discovered_count);

    /* Expire timeout and run coroutine */
    mock_tick_advance(2000);
    st = syn_dnssd_browse_task(&pt, &task);
    TEST_ASSERT_EQUAL(PT_EXITED, st);
    TEST_ASSERT_FALSE(browser.active);
}

void run_dnssd_tests(void)
{
    RUN_TEST(test_dnssd_init_and_registration);
    RUN_TEST(test_dnssd_query_and_response_generation);
    RUN_TEST(test_dnssd_task_execution);
    RUN_TEST(test_dnssd_announce);
    RUN_TEST(test_dnssd_query_and_response_parsing);
    RUN_TEST(test_dnssd_browser_task_coroutine);
}
