#include "mocks/mock_port.h"
#include "syntropic/net/syn_dns.h"
#include "syntropic/port/syn_port_system.h"
#include "unity/unity.h"

#include <string.h>

void test_dns_resolve(void)
{
    mock_port_reset();

    /* DNS response for "google.com" resolving to 1.2.3.4 */
    /* TransID: 0x1234, Flags: 0x8180 (response, standard), Questions: 1, Answers: 1 */
    uint8_t response[] = {
        0x00, 0x00,             /* ID */
        0x81, 0x80,             /* Flags */
        0x00, 0x01,             /* Questions */
        0x00, 0x01,             /* Answers */
        0x00, 0x00, 0x00, 0x00, /* Authority, Additional */
        /* Question: "google.com" */
        6, 'g', 'o', 'o', 'g', 'l', 'e', 3, 'c', 'o', 'm', 0, 0x00, 0x01, /* QTYPE = A */
        0x00, 0x01,                                                       /* QCLASS = IN */
        /* Answer: pointer to google.com (0xC00C), Type: A (1), Class: IN (1), TTL: 300, RDLen: 4,
           Addr: 1.2.3.4 */
        0xC0, 0x0C, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x01, 0x2C, 0x00, 0x04, 1, 2, 3, 4};

    SYN_SockAddr from;
    from.ip[0] = 8;
    from.ip[1] = 8;
    from.ip[2] = 8;
    from.ip[3] = 8;
    from.port = 53;
    mock_udp_set_response(response, sizeof(response), &from);

    SYN_SockAddr resolved;
    SYN_DnsResolver r;
    r.dns_server = NULL;
    r.hostname = "google.com";
    r.addr_out = &resolved;
    r.timeout_ms = 1000;

    SYN_PT pt;
    PT_INIT(&pt);
    SYN_Task task;
    task.user_data = &r;

    while (syn_dns_resolve_task(&pt, &task) == PT_WAITING) {
        syn_port_delay_ms(1);
    }

    TEST_ASSERT_EQUAL(SYN_OK, r.status);
    TEST_ASSERT_EQUAL_UINT8(1, resolved.ip[0]);
    TEST_ASSERT_EQUAL_UINT8(2, resolved.ip[1]);
    TEST_ASSERT_EQUAL_UINT8(3, resolved.ip[2]);
    TEST_ASSERT_EQUAL_UINT8(4, resolved.ip[3]);

    /* Verify query packet sent */
    TEST_ASSERT_TRUE(mock_udp_tx_len > 12);
    TEST_ASSERT_EQUAL_UINT8(8, mock_udp_tx_to.ip[0]);
    TEST_ASSERT_EQUAL_UINT8(8, mock_udp_tx_to.ip[1]);
    TEST_ASSERT_EQUAL_UINT8(8, mock_udp_tx_to.ip[2]);
    TEST_ASSERT_EQUAL_UINT8(8, mock_udp_tx_to.ip[3]);
    TEST_ASSERT_EQUAL_UINT16(53, mock_udp_tx_to.port);
}

void test_mdns_responder(void)
{
    mock_port_reset();

    SYN_Mdns mdns;
    uint8_t ip[] = {192, 168, 1, 100};
    SYN_Status init_st = syn_mdns_init(&mdns, "mydevice", ip);
    TEST_ASSERT_EQUAL(SYN_OK, init_st);
    TEST_ASSERT_EQUAL(20, mdns.sock);

    /* Simulate incoming mDNS query for "mydevice.local" on port 5353 */
    /* TransID: 0, Flags: 0, Questions: 1, Answers: 0 */
    uint8_t query[] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 8,    'm',  'y',  'd',  'e',  'v',  'i',  'c',
        'e',  5,    'l',  'o',  'c',  'a',  'l',  0,    0x00, 0x01, /* QTYPE = A */
        0x00, 0x01                                                  /* QCLASS = IN */
    };

    SYN_SockAddr from;
    from.ip[0] = 192;
    from.ip[1] = 168;
    from.ip[2] = 1;
    from.ip[3] = 50;
    from.port = 5353;
    mock_udp_set_response(query, sizeof(query), &from);

    SYN_PT pt;
    PT_INIT(&pt);

    SYN_Task task;
    task.user_data = &mdns;

    /* Run task */
    syn_mdns_task(&pt, &task);

    /* Verify responder sent a reply to 224.0.0.251:5353 */
    TEST_ASSERT_TRUE(mock_udp_tx_len > 12);
    TEST_ASSERT_EQUAL_UINT8(224, mock_udp_tx_to.ip[0]);
    TEST_ASSERT_EQUAL_UINT8(0, mock_udp_tx_to.ip[1]);
    TEST_ASSERT_EQUAL_UINT8(0, mock_udp_tx_to.ip[2]);
    TEST_ASSERT_EQUAL_UINT8(251, mock_udp_tx_to.ip[3]);
    TEST_ASSERT_EQUAL_UINT16(5353, mock_udp_tx_to.port);

    /* Verify response flags (Authoritative, Response) */
    TEST_ASSERT_EQUAL_UINT8(0x84, mock_udp_tx_buf[2]);
    TEST_ASSERT_EQUAL_UINT8(0x00, mock_udp_tx_buf[3]);
    TEST_ASSERT_EQUAL_UINT8(0x01, mock_udp_tx_buf[7]); /* Answers = 1 */

    /* Verify IP address in answer payload */
    uint8_t *addr_ptr = &mock_udp_tx_buf[mock_udp_tx_len - 4];
    TEST_ASSERT_EQUAL_UINT8(192, addr_ptr[0]);
    TEST_ASSERT_EQUAL_UINT8(168, addr_ptr[1]);
    TEST_ASSERT_EQUAL_UINT8(1, addr_ptr[2]);
    TEST_ASSERT_EQUAL_UINT8(100, addr_ptr[3]);
}
/** DNS resolve with custom server — exercises line 142 */
static void test_dns_resolve_custom_server(void)
{
    mock_port_reset();

    /* Response matching ID 0 */
    uint8_t response[] = {0x00, 0x00, 0x81, 0x80, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00,
                          0x00, 3,    'f',  'o',  'o',  3,    'c',  'o',  'm',  0,    0x00,
                          0x01, 0x00, 0x01, 0xC0, 0x0C, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00,
                          0x00, 0x3C, 0x00, 0x04, 10,   20,   30,   40};
    SYN_SockAddr from = {{1, 1, 1, 1}, 53};
    mock_udp_set_response(response, sizeof(response), &from);

    SYN_SockAddr custom = {{1, 1, 1, 1}, 53};
    SYN_SockAddr resolved;
    SYN_DnsResolver r;
    r.dns_server = &custom;
    r.hostname = "foo.com";
    r.addr_out = &resolved;
    r.timeout_ms = 1000;

    SYN_PT pt;
    PT_INIT(&pt);
    SYN_Task task;
    task.user_data = &r;
    while (syn_dns_resolve_task(&pt, &task) == PT_WAITING) {
        syn_port_delay_ms(1);
    }
    TEST_ASSERT_EQUAL(SYN_OK, r.status);
    TEST_ASSERT_EQUAL_UINT8(10, resolved.ip[0]);
}

/** DNS resolve: UDP open fail — exercises lines 153-154 */
static void test_dns_resolve_udp_open_fail(void)
{
    mock_port_reset();
    mock_udp_open_ok = false;

    SYN_SockAddr resolved;
    SYN_DnsResolver r;
    r.dns_server = NULL;
    r.hostname = "test.com";
    r.addr_out = &resolved;
    r.timeout_ms = 1000;

    SYN_PT pt;
    PT_INIT(&pt);
    SYN_Task task;
    task.user_data = &r;
    while (syn_dns_resolve_task(&pt, &task) == PT_WAITING) {
        syn_port_delay_ms(1);
    }
    TEST_ASSERT_EQUAL(SYN_ERROR, r.status);
    mock_udp_open_ok = true;
}

/** DNS resolve: send fails — exercises lines 174-177 */
static void test_dns_resolve_send_fail(void)
{
    mock_port_reset();
    mock_udp_sendto_fail = true;

    SYN_SockAddr resolved;
    SYN_DnsResolver r;
    r.dns_server = NULL;
    r.hostname = "test.com";
    r.addr_out = &resolved;
    r.timeout_ms = 1000;

    SYN_PT pt;
    PT_INIT(&pt);
    SYN_Task task;
    task.user_data = &r;
    while (syn_dns_resolve_task(&pt, &task) == PT_WAITING) {
        syn_port_delay_ms(1);
    }
    TEST_ASSERT_EQUAL(SYN_ERROR, r.status);
    mock_udp_sendto_fail = false;
}

/** DNS resolve: timeout — exercises lines 192-194 */
static void test_dns_resolve_timeout(void)
{
    mock_port_reset();
    /* No UDP response loaded → recv returns -1 every time → timeout */

    SYN_SockAddr resolved;
    SYN_DnsResolver r;
    r.dns_server = NULL;
    r.hostname = "timeout.com";
    r.addr_out = &resolved;
    r.timeout_ms = 10; /* Very short timeout */

    SYN_PT pt;
    PT_INIT(&pt);
    SYN_Task task;
    task.user_data = &r;
    for (int i = 0; i < 200; i++) {
        if (syn_dns_resolve_task(&pt, &task) >= PT_EXITED)
            break;
        mock_tick_ms += 1; /* advance time */
    }
    TEST_ASSERT_EQUAL(SYN_TIMEOUT, r.status);
}

static void test_dns_resolve_bad_packet_header(void)
{
    mock_port_reset();
    /* Short packet (< 12 bytes) -> line 96 */
    uint8_t short_rx[] = {0x00, 0x00, 0x81, 0x80};
    mock_udp_set_response(short_rx, sizeof(short_rx), NULL);

    SYN_SockAddr resolved;
    SYN_DnsResolver r;
    r.dns_server = NULL;
    r.hostname = "example.com";
    r.addr_out = &resolved;
    r.timeout_ms = 1000;

    SYN_PT pt;
    PT_INIT(&pt);
    SYN_Task task;
    task.user_data = &r;
    while (syn_dns_resolve_task(&pt, &task) == PT_WAITING) {
        mock_tick_ms += 1;
    }
    TEST_ASSERT_EQUAL(SYN_TIMEOUT, r.status);

    /* TXID mismatch -> line 99 */
    mock_port_reset();
    uint8_t wrong_txid_rx[] = {0x99, 0x99, 0x81, 0x80, 0x00, 0x01,
                               0x00, 0x01, 0x00, 0x00, 0x00, 0x00};
    mock_udp_set_response(wrong_txid_rx, sizeof(wrong_txid_rx), NULL);
    PT_INIT(&pt);
    while (syn_dns_resolve_task(&pt, &task) == PT_WAITING) {
        mock_tick_ms += 1;
    }
    TEST_ASSERT_EQUAL(SYN_TIMEOUT, r.status);

    /* Answers == 0 -> line 107 */
    mock_port_reset();
    uint8_t zero_answers_rx[] = {0x00, 0x00, 0x81, 0x80, 0x00, 0x01,
                                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    mock_udp_set_response(zero_answers_rx, sizeof(zero_answers_rx), NULL);
    PT_INIT(&pt);
    while (syn_dns_resolve_task(&pt, &task) == PT_WAITING) {
        mock_tick_ms += 1;
    }
    TEST_ASSERT_EQUAL(SYN_ERROR, r.status);
}

static void test_dns_resolve_cname(void)
{
    mock_port_reset();
    uint8_t rx[] = {0x00, 0x00, 0x81, 0x80, /* ID, flags */
                    0x00, 0x01, 0x00, 0x01, /* 1 question, 1 answer */
                    0x00, 0x00, 0x00, 0x00, /* 0 auth, 0 add */
                    /* Question: "example.com" */
                    7, 'e', 'x', 'a', 'm', 'p', 'l', 'e', 3, 'c', 'o', 'm', 0, 0x00, 0x01, 0x00,
                    0x01, /* QTYPE A, QCLASS IN */
                    /* Answer: CNAME (type 5), length 4, "abcd" */
                    0xC0, 0x0C,             /* Pointer to question */
                    0x00, 0x05,             /* TYPE CNAME */
                    0x00, 0x01,             /* CLASS IN */
                    0x00, 0x00, 0x00, 0x3C, /* TTL */
                    0x00, 0x04,             /* RDLEN = 4 */
                    'a', 'b', 'c', 'd'};
    mock_udp_set_response(rx, sizeof(rx), NULL);

    SYN_SockAddr resolved;
    SYN_DnsResolver r;
    r.dns_server = NULL;
    r.hostname = "example.com";
    r.addr_out = &resolved;
    r.timeout_ms = 1000;

    SYN_PT pt;
    PT_INIT(&pt);
    SYN_Task task;
    task.user_data = &r;
    for (int i = 0; i < 2000; i++) {
        if (syn_dns_resolve_task(&pt, &task) >= PT_EXITED)
            break;
        mock_tick_ms += 1;
    }
    TEST_ASSERT_EQUAL(SYN_ERROR, r.status);
}

static void test_mdns_join_fail(void)
{
    mock_port_reset();
    mock_udp_multicast_join_ok = false;
    SYN_Mdns mdns;
    uint8_t ip[4] = {192, 168, 1, 100};
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_mdns_init(&mdns, "device", ip));
    mock_udp_multicast_join_ok = true;
}

static void test_mdns_malformed_query(void)
{
    mock_port_reset();
    SYN_Mdns mdns;
    uint8_t ip[4] = {192, 168, 1, 100};
    TEST_ASSERT_EQUAL(SYN_OK, syn_mdns_init(&mdns, "device", ip));

    /* Malformed query: valid header, 1 question, but qname exceeds length */
    uint8_t rx[] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 64,   'a' /* Label length 64, but buffer ends here */
    };
    mock_udp_set_response(rx, sizeof(rx), NULL);

    SYN_PT pt;
    PT_INIT(&pt);
    SYN_Task task;
    task.user_data = &mdns;
    syn_mdns_task(&pt, &task);
}

static void test_mdns_no_match(void)
{
    mock_port_reset();
    SYN_Mdns mdns;
    uint8_t ip[4] = {192, 168, 1, 100};
    TEST_ASSERT_EQUAL(SYN_OK, syn_mdns_init(&mdns, "device", ip));

    /* Query for "other.local", 1 question */
    uint8_t rx[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 5,    'o',  't',  'h',  'e',  'r',  5,    'l',
                    'o',  'c',  'a',  'l',  0,    0x00, 0x01, 0x00, 0x01};
    mock_udp_set_response(rx, sizeof(rx), NULL);

    SYN_PT pt;
    PT_INIT(&pt);
    SYN_Task task;
    task.user_data = &mdns;
    syn_mdns_task(&pt, &task);
}

static void test_dns_resolve_malformed_responses(void)
{
    mock_port_reset();

    /* Response shorter than 12 bytes */
    uint8_t short_rx[] = {0x00, 0x01, 0x81};
    mock_udp_set_response(short_rx, sizeof(short_rx), NULL);

    SYN_SockAddr resolved;
    SYN_DnsResolver r;
    r.dns_server = NULL;
    r.hostname = "google.com";
    r.addr_out = &resolved;
    r.timeout_ms = 50;

    SYN_PT pt;
    PT_INIT(&pt);
    SYN_Task task;
    task.user_data = &r;

    while (syn_dns_resolve_task(&pt, &task) == PT_WAITING) {
        syn_port_delay_ms(1);
    }
    /* Runt response (n < 12) is ignored and loop times out → SYN_TIMEOUT */
    TEST_ASSERT_EQUAL(SYN_TIMEOUT, r.status);
}

static void test_dns_resolve_rcode_error_and_txid_mismatch(void)
{
    mock_port_reset();

    /* Response with RCODE=3 (NXDOMAIN error response) */
    uint8_t err_resp[] = {0x00, 0x00, /* ID (will match generated ID) */
                          0x81, 0x83, /* Flags: Response + RCODE=3 (NXDomain) */
                          0x00, 0x01, /* Questions: 1 */
                          0x00, 0x00, /* Answers: 0 */
                          0x00, 0x00, 0x00, 0x00, 6,   'g', 'o',  'o',  'g',  'l',
                          'e',  3,    'c',  'o',  'm', 0,   0x00, 0x01, 0x00, 0x01};

    /* Intercept tx_id from sent query to test TXID check */
    mock_udp_set_response(err_resp, sizeof(err_resp), NULL);

    SYN_SockAddr resolved;
    SYN_DnsResolver r;
    r.dns_server = NULL;
    r.hostname = "google.com";
    r.addr_out = &resolved;
    r.timeout_ms = 50;

    SYN_PT pt;
    PT_INIT(&pt);
    SYN_Task task;
    task.user_data = &r;

    while (syn_dns_resolve_task(&pt, &task) == PT_WAITING) {
        syn_port_delay_ms(1);
    }
    TEST_ASSERT_EQUAL(SYN_ERROR, r.status);
}

static void test_mdns_qname_local_mismatch_branches(void)
{
    mock_port_reset();

    SYN_Mdns mdns;
    uint8_t ip[] = {192, 168, 1, 100};
    syn_mdns_init(&mdns, "mydevice", ip);

    /* 1. Mismatched hostname length (h_len != host_len) */
    uint8_t q_bad_len[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
                           0x00, 0x00, 0x00, 3,    'b',  'a',  'd',  5,    'l',
                           'o',  'c',  'a',  'l',  0,    0x00, 0x01, 0x00, 0x01};
    SYN_SockAddr from = {.ip = {192, 168, 1, 50}, .port = 5353};
    mock_udp_set_response(q_bad_len, sizeof(q_bad_len), &from);

    SYN_PT pt;
    PT_INIT(&pt);
    SYN_Task task = {.user_data = &mdns};
    syn_mdns_task(&pt, &task);

    /* 2. Mismatched domain (not "local", e.g. "other") */
    uint8_t q_bad_domain[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
                              0x00, 8,    'm',  'y',  'd',  'e',  'v',  'i',  'c',  'e',  5,
                              'o',  't',  'h',  'e',  'r',  0,    0x00, 0x01, 0x00, 0x01};
    mock_udp_set_response(q_bad_domain, sizeof(q_bad_domain), &from);
    PT_INIT(&pt);
    syn_mdns_task(&pt, &task);
}

static void test_dns_parse_response_error_branches(void)
{
    mock_port_reset();
    SYN_SockAddr from = {.port = 53};
    SYN_SockAddr resolved;
    SYN_DnsResolver r = {
        .dns_server = NULL, .hostname = "example.com", .addr_out = &resolved, .timeout_ms = 1000};
    SYN_PT pt;
    SYN_Task task = {.user_data = &r};

    /* 1. TxID mismatch (line 99) */
    uint8_t rx_bad_txid[12] = {0xFF, 0xFF, 0x81, 0x80, 0, 0, 0, 1, 0, 0, 0, 0};
    mock_udp_set_response(rx_bad_txid, sizeof(rx_bad_txid), &from);
    PT_INIT(&pt);
    while (syn_dns_resolve_task(&pt, &task) == PT_WAITING) {
        mock_tick_ms += 1100;
    }

    /* 2. Answers == 0 (line 107) */
    uint8_t rx_no_answers[12] = {0x00, 0x00, 0x81, 0x80, 0, 1, 0, 0, 0, 0, 0, 0};
    mock_udp_set_response(rx_no_answers, sizeof(rx_no_answers), &from);
    PT_INIT(&pt);
    while (syn_dns_resolve_task(&pt, &task) == PT_WAITING) {
        mock_tick_ms += 1100;
    }
}

static void test_dns_resolve_malformed_qname_and_truncated_records(void)
{
    mock_port_reset();
    SYN_SockAddr from = {.ip = {8, 8, 8, 8}, .port = 53};
    SYN_SockAddr resolved;
    SYN_DnsResolver r = {
        .dns_server = &from, .hostname = "myhost.com", .addr_out = &resolved, .timeout_ms = 1000};
    SYN_PT pt;
    SYN_Task task = {.user_data = &r};

    /* 1. Malformed QNAME in question section: offset 12 has invalid label length 0x80 */
    uint8_t rx_bad_qname[20] = {0x00, 0x00, 0x81, 0x80, 0x00, 0x01, 0x00, 0x01, 0, 0,
                                0,    0,    0x80, 0x01, 0,    0,    0,    0,    0, 0};
    mock_udp_set_response(rx_bad_qname, sizeof(rx_bad_qname), &from);
    PT_INIT(&pt);
    while (syn_dns_resolve_task(&pt, &task) == PT_WAITING) {
        mock_tick_ms += 1100;
    }

    /* 2. Truncated answer header (< 10 bytes after QNAME) */
    uint8_t rx_trunc_ans[18] = {0x00, 0x00, 0x81, 0x80, 0x00, 0x00, 0x00, 0x01, 0,
                                0,    0,    0,    0x03, 'f',  'o',  'o',  0x00, 0x01};
    mock_udp_set_response(rx_trunc_ans, sizeof(rx_trunc_ans), &from);
    PT_INIT(&pt);
    while (syn_dns_resolve_task(&pt, &task) == PT_WAITING) {
        mock_tick_ms += 1100;
    }
}

static void test_dns_mdns_match_qname_local_boundary_mismatches(void)
{
    SYN_Mdns mdns;
    uint8_t ip[4] = {192, 168, 1, 100};
    syn_mdns_init(&mdns, "myhost", ip);
    SYN_SockAddr from = {.ip = {192, 168, 1, 50}, .port = 5353};
    SYN_PT pt;
    SYN_Task task = {.user_data = &mdns};

    /* Truncated buffer right before host label */
    uint8_t buf1[12] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
    mock_udp_set_response(buf1, sizeof(buf1), &from);
    PT_INIT(&pt);
    syn_mdns_task(&pt, &task);

    /* "local" length mismatch (e.g. 0x04 instead of 0x05) */
    uint8_t buf2[] = {0,   0,   0,   0,   0, 1,   0,   0,   0,   0, 0, 0, 6, 'm', 'y',
                      'h', 'o', 's', 't', 4, 'l', 'o', 'c', 'a', 0, 0, 1, 0, 1};
    mock_udp_set_response(buf2, sizeof(buf2), &from);
    PT_INIT(&pt);
    syn_mdns_task(&pt, &task);
}

static void test_dns_mdns_invalid_qtype(void)
{
    SYN_Mdns mdns;
    uint8_t ip[4] = {10, 0, 0, 1};
    syn_mdns_init(&mdns, "device", ip);

    SYN_SockAddr from = {.port = 5353};
    uint8_t query[] = {0,   0,   0,   0,   0, 1,   0,   0,   0,   0,   0, 0, 6,  'd', 'e',
                       'v', 'i', 'c', 'e', 5, 'l', 'o', 'c', 'a', 'l', 0, 0, 16, 0,   1};
    mock_udp_set_response(query, sizeof(query), &from);

    SYN_PT pt;
    PT_INIT(&pt);
    SYN_Task task = {.user_data = &mdns};
    syn_mdns_task(&pt, &task);
}

static void test_dns_resolve_null_params(void)
{
    SYN_PT pt;
    PT_INIT(&pt);
    SYN_DnsResolver res;
    memset(&res, 0, sizeof(res));
    res.hostname = "example.com";
    SYN_Task task;
    memset(&task, 0, sizeof(task));
    task.user_data = &res;
    while (syn_dns_resolve_task(&pt, &task) == PT_WAITING) {
        mock_tick_ms += 1100;
    }
    TEST_ASSERT_EQUAL(SYN_TIMEOUT, res.status);
}

static void test_dns_mdns_init_open_failure_and_truncated_records(void)
{
    /* 1. syn_mdns_init open failure (line 235) */
    mock_port_reset();
    mock_udp_open_ok = false;
    SYN_Mdns mdns;
    uint8_t ip[4] = {192, 168, 1, 100};
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_mdns_init(&mdns, "mydev", ip));
    mock_udp_open_ok = true;

    /* 2. Truncated A record payload in parse_response (line 131) */
    mock_port_reset();
    uint8_t trunc_a[] = {0x00, 0x00, 0x81, 0x80, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
                         /* Question: "a.com" */
                         1, 'a', 3, 'c', 'o', 'm', 0, 0x00, 0x01, 0x00, 0x01,
                         /* Answer: Type A, rdlen 4, but only 2 bytes payload */
                         0xC0, 0x0C, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x3C, 0x00, 0x04, 1,
                         2};
    mock_udp_set_response(trunc_a, sizeof(trunc_a), NULL);
    SYN_SockAddr resolved;
    SYN_DnsResolver r;
    r.dns_server = NULL;
    r.hostname = "a.com";
    r.addr_out = &resolved;
    r.timeout_ms = 10;
    SYN_PT pt;
    PT_INIT(&pt);
    SYN_Task task = {.user_data = &r};
    while (syn_dns_resolve_task(&pt, &task) == PT_WAITING) {
        mock_tick_ms += 10;
    }
    TEST_ASSERT_EQUAL(SYN_ERROR, r.status);

    /* 3. RCODE != 0 error response (line 100) */
    mock_port_reset();
    uint8_t rcode_err[] = {0x00, 0x00, 0x81, 0x82, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00};
    mock_udp_set_response(rcode_err, sizeof(rcode_err), NULL);
    PT_INIT(&pt);
    task.user_data = &r;
    while (syn_dns_resolve_task(&pt, &task) == PT_WAITING) {
        mock_tick_ms += 10;
    }
    TEST_ASSERT_EQUAL(SYN_ERROR, r.status);

    /* 4. Truncated answer record header (pos + 10 > rx_len, line 121) */
    mock_port_reset();
    uint8_t trunc_ans_hdr[] = {0x00, 0x00, 0x81, 0x80, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00,
                               0x00, 0x00, 1,    'a',  3,    'c',  'o',  'm',  0,    0x00,
                               0x01, 0x00, 0x01, 0xC0, 0x0C, 0x00, 0x01}; /* only 4 bytes instead of
                                                                             10 for answer header */
    mock_udp_set_response(trunc_ans_hdr, sizeof(trunc_ans_hdr), NULL);
    PT_INIT(&pt);
    task.user_data = &r;
    while (syn_dns_resolve_task(&pt, &task) == PT_WAITING) {
        mock_tick_ms += 10;
    }
    TEST_ASSERT_EQUAL(SYN_ERROR, r.status);

    /* 5. mDNS QNAME "local" string mismatch (line 281) */
    SYN_Mdns mdns2;
    syn_mdns_init(&mdns2, "mydev", ip);
    SYN_SockAddr from = {.port = 5353};
    /* Query with label "other" instead of "local" */
    uint8_t bad_local[] = {0,   0,   0,   0, 0,   1,   0,   0,   0,   0, 0, 0, 5, 'm', 'y',
                           'd', 'e', 'v', 5, 'o', 't', 'h', 'e', 'r', 0, 0, 1, 0, 1};
    mock_udp_set_response(bad_local, sizeof(bad_local), &from);
    PT_INIT(&pt);
    task.user_data = &mdns2;
    syn_mdns_task(&pt, &task);

    /* 6. Truncated answer QNAME (parse_qname fails in answer loop, line 120) */
    mock_port_reset();
    uint8_t bad_ans_qname[] = {0x00, 0x00, 0x81, 0x80, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00,
                               0x00,
                               /* Question: "a.com" */
                               1, 'a', 3, 'c', 'o', 'm', 0, 0x00, 0x01, 0x00, 0x01,
                               /* Answer: invalid QNAME label length pointing out of bounds */
                               0x3F, 'x'};
    mock_udp_set_response(bad_ans_qname, sizeof(bad_ans_qname), NULL);
    PT_INIT(&pt);
    task.user_data = &r;
    syn_dns_resolve_task(&pt, &task);

    /* 7. Short packet (<12 bytes, line 96) & TxID mismatch (line 99) */
    uint8_t short_pkt[10] = {0};
    mock_udp_set_response(short_pkt, sizeof(short_pkt), NULL);
    PT_INIT(&pt);
    task.user_data = &r;
    syn_dns_resolve_task(&pt, &task);
    syn_dns_resolve_task(&pt, &task);

    uint8_t wrong_txid_pkt[12] = {0xFF, 0xFF, 0x81, 0x80, 0, 0, 0, 0, 0, 0, 0, 0};
    mock_udp_set_response(wrong_txid_pkt, sizeof(wrong_txid_pkt), NULL);
    PT_INIT(&pt);
    syn_dns_resolve_task(&pt, &task);
    syn_dns_resolve_task(&pt, &task);

    /* 7. mDNS match_qname_local length & terminator mismatches (lines 259, 267, 269, 274, 279,
     * 286) */
    /* Truncated hostname label payload */
    uint8_t mdns_trunc_host[] = {0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 5, 'm', 'y'};
    mock_udp_set_response(mdns_trunc_host, sizeof(mdns_trunc_host), &from);
    PT_INIT(&pt);
    syn_mdns_task(&pt, &task);

    /* Mismatched hostname characters */
    uint8_t mdns_wrong_host[] = {0,   0,   0,   0, 0,   1,   0,   0,   0,   0, 0, 0, 5, 'x', 'y',
                                 'd', 'e', 'v', 5, 'l', 'o', 'c', 'a', 'l', 0, 0, 1, 0, 1};
    mock_udp_set_response(mdns_wrong_host, sizeof(mdns_wrong_host), &from);
    PT_INIT(&pt);
    syn_mdns_task(&pt, &task);

    /* Truncated local label payload */
    uint8_t mdns_trunc_local[] = {0, 0, 0,   0,   0,   1,   0,   0, 0,   0,  0,
                                  0, 5, 'm', 'y', 'd', 'e', 'v', 5, 'l', 'o'};
    mock_udp_set_response(mdns_trunc_local, sizeof(mdns_trunc_local), &from);
    PT_INIT(&pt);
    syn_mdns_task(&pt, &task);

    /* Non-zero terminator byte */
    uint8_t mdns_bad_term[] = {0,   0,   0,   0, 0,   1,   0,   0,   0,   0, 0, 0, 5, 'm', 'y',
                               'd', 'e', 'v', 5, 'l', 'o', 'c', 'a', 'l', 1, 0, 1, 0, 1};
    mock_udp_set_response(mdns_bad_term, sizeof(mdns_bad_term), &from);
    PT_INIT(&pt);
    syn_mdns_task(&pt, &task);

    /* mDNS task receiving response packet (flags & 0x8000 != 0) */
    uint8_t mdns_resp_pkt[] = {0x00, 0x00, 0x84, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
                               0x00, 0x00, 5,    'm',  'y',  'd',  'e',  'v',  5,    'l',
                               'o',  'c',  'a',  'l',  0,    0,    1,    0,    1};
    mock_udp_set_response(mdns_resp_pkt, sizeof(mdns_resp_pkt), &from);
    PT_INIT(&pt);
    syn_mdns_task(&pt, &task);

    /* mDNS with NULL hostname */
    SYN_Mdns null_host_mdns = {.sock = 20, .hostname = NULL};
    SYN_Task null_host_task = {.user_data = &null_host_mdns};
    uint8_t q_null_host[] = {0,   0,   0, 0,   0,   1,   0,   0,   0, 0, 0, 0, 3, 'b',
                             'a', 'd', 5, 'l', 'o', 'c', 'a', 'l', 0, 0, 1, 0, 1};
    mock_udp_set_response(q_null_host, sizeof(q_null_host), &from);
    PT_INIT(&pt);
    syn_mdns_task(&pt, &null_host_task);

    /* DNS response with RCODE != 0 (e.g. Server Failure RCODE=2) */
    mock_port_reset();
    uint8_t rcode_sf_resp[] = {0x00, 0x00, 0x81, 0x82, 0x00, 0x01,
                               0x00, 0x01, 0x00, 0x00, 0x00, 0x00};
    mock_udp_set_response(rcode_sf_resp, sizeof(rcode_sf_resp), &from);
    SYN_DnsResolver r_sf = {
        .dns_server = NULL, .hostname = "a.com", .addr_out = &resolved, .timeout_ms = 10};
    PT_INIT(&pt);
    task.user_data = &r_sf;
    syn_dns_resolve_task(&pt, &task);

    /* DNS response with non-A record (e.g. type 28 AAAA or type 16 TXT) */
    mock_port_reset();
    uint8_t txt_ans[] = {0x00, 0x00, 0x81, 0x80, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00,
                         0x00, 0x00, 1,    'a',  3,    'c',  'o',  'm',  0,    0x00,
                         0x01, 0x00, 0x01, 0xC0, 0x0C, 0x00, 0x10, 0x00, 0x01, 0x00,
                         0x00, 0x00, 0x3C, 0x00, 0x04, 't',  'x',  't',  '1'};
    mock_udp_set_response(txt_ans, sizeof(txt_ans), &from);
    PT_INIT(&pt);
    syn_dns_resolve_task(&pt, &task);

    /* Runt UDP packet (len 5 < 12) in resolve loop (line 204) */
    mock_port_reset();
    uint8_t runt_udp[5] = {0x01, 0x02, 0x03, 0x04, 0x05};
    mock_udp_set_response(runt_udp, sizeof(runt_udp), &from);
    PT_INIT(&pt);
    syn_dns_resolve_task(&pt, &task);

    /* DNS response with 0 answers */
    mock_port_reset();
    uint8_t zero_ans_resp[] = {0x00, 0x00, 0x81, 0x80, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
                               0x00, 0x00, 1,    'a',  0,    0x00, 0x01, 0x00, 0x01};
    mock_udp_set_response(zero_ans_resp, sizeof(zero_ans_resp), &from);
    PT_INIT(&pt);
    task.user_data = &r_sf;
    syn_dns_resolve_task(&pt, &task);
    TEST_ASSERT_EQUAL(SYN_ERROR, r_sf.status);

    /* DNS response with truncated answer header */
    mock_port_reset();
    uint8_t trunc_ans_resp[] = {0x00, 0x00, 0x81, 0x80, 0x00, 0x00, 0x00, 0x01, 0x00,
                                0x00, 0x00, 0x00, 1,    'a',  0,    0x00, 0x01};
    mock_udp_set_response(trunc_ans_resp, sizeof(trunc_ans_resp), &from);
    PT_INIT(&pt);
    task.user_data = &r_sf;
    syn_dns_resolve_task(&pt, &task);
    TEST_ASSERT_EQUAL(SYN_ERROR, r_sf.status);

    /* mDNS query with second label length 3 ("com") instead of 5 ("local") */
    SYN_Mdns mdns_dev;
    uint8_t dev_ip[] = {192, 168, 1, 10};
    syn_mdns_init(&mdns_dev, "mydev", dev_ip);
    uint8_t com_query[] = {0,   0,   0,   0,   0, 1,   0,   0,   0, 0, 0, 0, 5, 'm',
                           'y', 'd', 'e', 'v', 3, 'c', 'o', 'm', 0, 0, 1, 0, 1};
    mock_udp_set_response(com_query, sizeof(com_query), &from);
    PT_INIT(&pt);
    task.user_data = &mdns_dev;
    syn_mdns_task(&pt, &task);

    /* mDNS packet with Response flag QR bit set (0x8400) */
    uint8_t resp_flag_pkt[] = {0, 0,   0x84, 0,   0,   1,   0, 0, 0, 0, 0, 0,
                               5, 'm', 'y',  'd', 'e', 'v', 0, 0, 1, 0, 1};
    mock_udp_set_response(resp_flag_pkt, sizeof(resp_flag_pkt), &from);
    PT_INIT(&pt);
    syn_mdns_task(&pt, &task);

    /* mDNS packet with 0 questions */
    uint8_t zero_q_pkt[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 'x'};
    mock_udp_set_response(zero_q_pkt, sizeof(zero_q_pkt), &from);
    PT_INIT(&pt);
    syn_mdns_task(&pt, &task);

    /* mDNS packet with 2 questions: Q1 malformed, Q2 valid match */
    uint8_t multi_q_pkt[] = {0,   0,   0,   0,   0,   2,   0,   0,   0,
                             0,   0,   0,   55,  'b', 'a', 'd', /* invalid label len 55 > buffer */
                             5,   'm', 'y', 'd', 'e', 'v', 5,   'l', 'o',
                             'c', 'a', 'l', 0,   0,   1,   0,   1};
    mock_udp_set_response(multi_q_pkt, sizeof(multi_q_pkt), &from);
    PT_INIT(&pt);
    syn_mdns_task(&pt, &task);

    /* DNS resolver with dns_server == NULL (lines 160-166) */
    SYN_DnsResolver r_null_srv;
    SYN_SockAddr addr_out;
    memset(&r_null_srv, 0, sizeof(r_null_srv));
    r_null_srv.hostname = "example.com";
    r_null_srv.dns_server = NULL;
    r_null_srv.addr_out = &addr_out;
    r_null_srv.timeout_ms = 100;
    PT_INIT(&pt);
    task.user_data = &r_null_srv;
    syn_dns_resolve_task(&pt, &task);

    /* mDNS init failure when udp_open returns invalid (line 242) */
    SYN_Mdns mdns_fail;
    mock_udp_open_ok = false;
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_mdns_init(&mdns_fail, "dev", dev_ip));
    mock_udp_open_ok = true;
}

static void test_mdns_fqdn_hostname(void)
{
    mock_port_reset();

    SYN_Mdns mdns;
    uint8_t ip[] = {192, 168, 1, 100};
    /* Initialize with FQDN containing ".local" */
    SYN_Status init_st = syn_mdns_init(&mdns, "mydevice.local", ip);
    TEST_ASSERT_EQUAL(SYN_OK, init_st);

    /* Simulate query for "mydevice.local" */
    uint8_t query[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 8,    'm',  'y',  'd',  'e',  'v',  'i',  'c',  'e',  5,
                       'l',  'o',  'c',  'a',  'l',  0,    0x00, 0x01, 0x00, 0x01};

    SYN_SockAddr from = {.ip = {192, 168, 1, 50}, .port = 5353};
    mock_udp_set_response(query, sizeof(query), &from);

    SYN_PT pt;
    PT_INIT(&pt);
    SYN_Task task = {.user_data = &mdns};

    syn_mdns_task(&pt, &task);

    /* Verify response sent to multicast group without duplicate .local.local */
    TEST_ASSERT_TRUE(mock_udp_tx_len > 12);
    TEST_ASSERT_EQUAL_UINT8(8, mock_udp_tx_buf[12]); /* Base label len = 8 ("mydevice") */
    TEST_ASSERT_EQUAL_MEMORY("mydevice", &mock_udp_tx_buf[13], 8);
    TEST_ASSERT_EQUAL_UINT8(5, mock_udp_tx_buf[21]); /* Second label len = 5 ("local") */
    TEST_ASSERT_EQUAL_MEMORY("local", &mock_udp_tx_buf[22], 5);
}

void test_dns_parse_truncated_compression_pointer(void)
{
    mock_port_reset();

    /* Craft a DNS response where the answer QNAME is a compression pointer
     * (0xC0 0x0C) but the packet is truncated after the 0xC0 byte.
     * The parser must reject this instead of reading past buffer end. */
    uint8_t truncated_resp[] = {
        0x00,
        0x00, /* ID (will be patched) */
        0x81,
        0x80, /* Flags: response */
        0x00,
        0x01, /* Questions: 1 */
        0x00,
        0x01, /* Answers: 1 */
        0x00,
        0x00,
        0x00,
        0x00, /* Authority, Additional */
        /* Question: "a.b" */
        1,
        'a',
        1,
        'b',
        0,
        0x00,
        0x01,
        0x00,
        0x01,
        /* Answer: truncated compression pointer — only 0xC0, missing 2nd byte */
        0xC0,
    };

    SYN_SockAddr from = {{8, 8, 8, 8}, 53};
    mock_udp_set_response(truncated_resp, sizeof(truncated_resp), &from);

    SYN_SockAddr resolved;
    SYN_DnsResolver r;
    r.dns_server = NULL;
    r.hostname = "a.b";
    r.addr_out = &resolved;
    r.timeout_ms = 500;

    SYN_PT pt;
    PT_INIT(&pt);
    SYN_Task task;
    task.user_data = &r;

    while (syn_dns_resolve_task(&pt, &task) == PT_WAITING) {
        syn_port_delay_ms(1);
    }

    /* Must not succeed — truncated packet should be rejected */
    TEST_ASSERT_NOT_EQUAL(SYN_OK, r.status);
}

void test_dns_resolve_hostname_too_long(void)
{
    mock_port_reset();

    /* Build a hostname that exceeds the 256-byte resolver buffer.
     * 240 chars of "a" + ".com" = 244 chars → QNAME ≈ 246 bytes + 12 header + 4 footer = 262.
     * encode_qname should return 0, and resolve should fail with SYN_ERROR. */
    char long_hostname[256];
    memset(long_hostname, 'a', 240);
    long_hostname[240] = '.';
    long_hostname[241] = 'c';
    long_hostname[242] = 'o';
    long_hostname[243] = 'm';
    long_hostname[244] = '\0';

    SYN_SockAddr resolved;
    SYN_DnsResolver r;
    r.dns_server = NULL;
    r.hostname = long_hostname;
    r.addr_out = &resolved;
    r.timeout_ms = 500;

    SYN_PT pt;
    PT_INIT(&pt);
    SYN_Task task;
    task.user_data = &r;

    while (syn_dns_resolve_task(&pt, &task) == PT_WAITING) {
        syn_port_delay_ms(1);
    }

    TEST_ASSERT_EQUAL(SYN_ERROR, r.status);
}

void run_dns_tests(void)
{
    RUN_TEST(test_dns_resolve);
    RUN_TEST(test_mdns_responder);
    RUN_TEST(test_dns_resolve_custom_server);
    RUN_TEST(test_dns_resolve_udp_open_fail);
    RUN_TEST(test_dns_resolve_send_fail);
    RUN_TEST(test_dns_resolve_timeout);
    RUN_TEST(test_dns_resolve_cname);
    RUN_TEST(test_mdns_join_fail);
    RUN_TEST(test_mdns_malformed_query);
    RUN_TEST(test_mdns_no_match);
    RUN_TEST(test_dns_resolve_malformed_responses);
    RUN_TEST(test_dns_resolve_rcode_error_and_txid_mismatch);
    RUN_TEST(test_mdns_qname_local_mismatch_branches);
    RUN_TEST(test_dns_parse_response_error_branches);
    RUN_TEST(test_dns_resolve_malformed_qname_and_truncated_records);
    RUN_TEST(test_dns_mdns_match_qname_local_boundary_mismatches);
    RUN_TEST(test_dns_mdns_invalid_qtype);
    RUN_TEST(test_dns_resolve_null_params);
    RUN_TEST(test_dns_mdns_init_open_failure_and_truncated_records);
    RUN_TEST(test_dns_resolve_bad_packet_header);
    RUN_TEST(test_mdns_fqdn_hostname);
    RUN_TEST(test_dns_parse_truncated_compression_pointer);
    RUN_TEST(test_dns_resolve_hostname_too_long);
}
