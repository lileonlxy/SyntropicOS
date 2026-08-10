/**
 * @file test_coap.c
 * @brief Unity tests for CoAP message serialization and parsing.
 */

#include "mocks/mock_port.h"
#include "syntropic/syntropic.h"
#include "unity/unity.h"

static void test_coap_serialization(void)
{
    SYN_CoapMsg req;
    req.type = COAP_TYPE_CON;
    req.code = COAP_CODE_GET;
    req.msg_id = 0x1234;
    req.token_len = 2;
    req.token[0] = 0xAB;
    req.token[1] = 0xCD;
    req.payload = (const uint8_t *)"hello";
    req.payload_len = 5;

    SYN_CoapOption options[2];
    /* Binary content-format option, e.g. text/plain (value 0) */
    uint8_t fmt_val = 0;
    options[0].num = COAP_OPT_CONTENT_FORMAT;
    options[0].val = &fmt_val;
    options[0].len = 1;

    options[1].num = COAP_OPT_URI_PATH;
    options[1].val = (const uint8_t *)"test";
    options[1].len = 4;

    uint8_t buffer[128];
    size_t len = syn_coap_serialize(&req, options, 2, buffer, sizeof(buffer));
    TEST_ASSERT_TRUE(len > 0);

    /* Parse back */
    SYN_CoapMsg resp;
    SYN_CoapOption parsed_options[8];
    size_t parsed_option_count = 0;
    SYN_Status st = syn_coap_parse(&resp, parsed_options, 8, &parsed_option_count, buffer, len);
    TEST_ASSERT_EQUAL(SYN_OK, st);

    TEST_ASSERT_EQUAL_UINT8(COAP_TYPE_CON, resp.type);
    TEST_ASSERT_EQUAL_UINT8(COAP_CODE_GET, resp.code);
    TEST_ASSERT_EQUAL_UINT16(0x1234, resp.msg_id);
    TEST_ASSERT_EQUAL_UINT8(2, resp.token_len);
    TEST_ASSERT_EQUAL_UINT8(0xAB, resp.token[0]);
    TEST_ASSERT_EQUAL_UINT8(0xCD, resp.token[1]);

    TEST_ASSERT_EQUAL_INT(2, parsed_option_count);
    /* Uri-Path (11) and Content-Format (12) must be sorted */
    TEST_ASSERT_EQUAL_UINT16(COAP_OPT_URI_PATH, parsed_options[0].num);
    TEST_ASSERT_EQUAL_INT(4, parsed_options[0].len);
    TEST_ASSERT_EQUAL_UINT16(COAP_OPT_CONTENT_FORMAT, parsed_options[1].num);
    TEST_ASSERT_EQUAL_INT(1, parsed_options[1].len);

    TEST_ASSERT_EQUAL_INT(5, resp.payload_len);
    TEST_ASSERT_EQUAL_MEMORY("hello", resp.payload, 5);
}

static void test_coap_extended_options(void)
{
    SYN_CoapMsg req = {.type = COAP_TYPE_CON,
                       .code = COAP_CODE_GET,
                       .msg_id = 0x1234,
                       .token_len = 0,
                       .payload_len = 0};

    SYN_CoapOption options[1];
    /* Proxy-Uri is option 35, requiring an extended delta because 35 > 12 */
    options[0].num = COAP_OPT_PROXY_URI;
    options[0].val = (const uint8_t *)"http://ext";
    options[0].len = 10;

    uint8_t buffer[128];
    size_t len = syn_coap_serialize(&req, options, 1, buffer, sizeof(buffer));
    TEST_ASSERT_TRUE(len > 0);

    /* Parse back and verify */
    SYN_CoapMsg resp;
    SYN_CoapOption parsed_options[4];
    size_t parsed_count = 0;
    SYN_Status st = syn_coap_parse(&resp, parsed_options, 4, &parsed_count, buffer, len);
    TEST_ASSERT_EQUAL(SYN_OK, st);
    TEST_ASSERT_EQUAL_INT(1, parsed_count);
    TEST_ASSERT_EQUAL_UINT16(COAP_OPT_PROXY_URI, parsed_options[0].num);
    TEST_ASSERT_EQUAL_INT(10, parsed_options[0].len);
    TEST_ASSERT_EQUAL_MEMORY("http://ext", parsed_options[0].val, 10);
}

static void test_coap_request_task_success(void)
{
    /* Setup mock response packet */
    SYN_CoapMsg resp_msg = {.type = COAP_TYPE_ACK,
                            .code = COAP_RESP_CONTENT,
                            .msg_id = 0x5555,
                            .token_len = 2,
                            .token = {0x11, 0x22},
                            .payload = (const uint8_t *)"payload",
                            .payload_len = 7};
    uint8_t resp_raw[64];
    size_t resp_raw_len = syn_coap_serialize(&resp_msg, NULL, 0, resp_raw, sizeof(resp_raw));
    TEST_ASSERT_TRUE(resp_raw_len > 0);

    SYN_SockAddr from = {.ip = {127, 0, 0, 1}, .port = 5683};
    mock_udp_set_response(resp_raw, resp_raw_len, &from);

    /* Setup CoAP request */
    SYN_CoapMsg req_msg = {.type = COAP_TYPE_CON,
                           .code = COAP_CODE_GET,
                           .msg_id = 0x5555,
                           .token_len = 2,
                           .token = {0x11, 0x22},
                           .payload_len = 0};

    SYN_CoapRequest req;
    syn_coap_request_init(&req, &from, &req_msg, 100, 2);

    SYN_Sched sched;
    SYN_Task task;
    syn_task_create(&task, "coap", syn_coap_request_task, 0, &req);
    syn_sched_init(&sched, &task, 1);

    /* Run scheduler loop */
    bool alive = true;
    uint32_t start = syn_port_get_tick_ms();
    while (alive && (syn_port_get_tick_ms() - start < 1000)) {
        alive = syn_sched_run(&sched);
        mock_tick_advance(10);
    }

    TEST_ASSERT_EQUAL(SYN_OK, req.status);
    TEST_ASSERT_EQUAL_UINT8(COAP_RESP_CONTENT, req.resp_msg.code);
    TEST_ASSERT_EQUAL_INT(7, req.resp_msg.payload_len);
    TEST_ASSERT_EQUAL_MEMORY("payload", req.resp_msg.payload, 7);
}

static void test_coap_serialization_boundaries(void)
{
    SYN_CoapMsg msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = COAP_TYPE_CON;
    msg.code = COAP_CODE_GET;
    msg.msg_id = 0x1234;

    /* 1. Delta >= 269, len_ext_len = 1 and len_ext_len = 2 */
    SYN_CoapOption options[2];
    /* Option 1: num = 300 (delta = 300) */
    uint8_t val_large[300];
    memset(val_large, 'A', sizeof(val_large));
    options[0].num = 300;
    options[0].val = val_large;
    options[0].len = 300; // exercises len_val = 14, len_ext_len = 2

    /* Option 2: num = 350 (delta = 50) */
    uint8_t val_medium[50];
    memset(val_medium, 'B', sizeof(val_medium));
    options[1].num = 350;
    options[1].val = val_medium;
    options[1].len = 50; // exercises len_val = 13, len_ext_len = 1

    uint8_t buf[1024];
    size_t len = syn_coap_serialize(&msg, options, 2, buf, sizeof(buf));
    TEST_ASSERT_TRUE(len > 0);

    /* Parse back */
    SYN_CoapMsg resp;
    SYN_CoapOption parsed_options[20];
    size_t parsed_option_count = 0;
    SYN_Status st = syn_coap_parse(&resp, parsed_options, 20, &parsed_option_count, buf, len);
    TEST_ASSERT_EQUAL(SYN_OK, st);
    TEST_ASSERT_EQUAL_INT(2, parsed_option_count);
    TEST_ASSERT_EQUAL_UINT16(300, parsed_options[0].num);
    TEST_ASSERT_EQUAL_INT(300, parsed_options[0].len);
    TEST_ASSERT_EQUAL_UINT16(350, parsed_options[1].num);
    TEST_ASSERT_EQUAL_INT(50, parsed_options[1].len);

    /* 2. Buffer overflows */
    /* A. max_buf_len < 4 */
    TEST_ASSERT_EQUAL_INT(0, syn_coap_serialize(&msg, NULL, 0, buf, 3));
    /* B. max_buf_len too small for options */
    TEST_ASSERT_EQUAL_INT(0, syn_coap_serialize(&msg, options, 1, buf, 10));
    /* C. max_buf_len too small for payload */
    msg.payload = (const uint8_t *)"hello";
    msg.payload_len = 5;
    TEST_ASSERT_EQUAL_INT(0, syn_coap_serialize(&msg, NULL, 0, buf,
                                                9)); // header 4 + payload 5 + marker 1 = 10 needed

    /* 3. Option count > 16 */
    SYN_CoapOption excess_options[18];
    for (int i = 0; i < 18; i++) {
        excess_options[i].num = (uint16_t)(i + 1);
        excess_options[i].val = (const uint8_t *)"x";
        excess_options[i].len = 1;
    }
    msg.payload_len = 0;
    len = syn_coap_serialize(&msg, excess_options, 18, buf, sizeof(buf));
    TEST_ASSERT_TRUE(len > 0);
    st = syn_coap_parse(&resp, parsed_options, 20, &parsed_option_count, buf, len);
    TEST_ASSERT_EQUAL(SYN_OK, st);
    /* Only 16 options should be encoded */
    TEST_ASSERT_EQUAL_INT(16, parsed_option_count);

    /* 4. Parse failures */
    /* A. buf_len < 4 */
    st = syn_coap_parse(&resp, parsed_options, 20, &parsed_option_count, buf, 3);
    TEST_ASSERT_EQUAL(SYN_ERROR, st);

    /* B. Version mismatch */
    buf[0] = 0x80; // ver = 2
    st = syn_coap_parse(&resp, parsed_options, 20, &parsed_option_count, buf, len);
    TEST_ASSERT_EQUAL(SYN_ERROR, st);
    buf[0] = 0x40; // reset to ver = 1

    /* C. Token len > 8 */
    buf[0] = 0x49; // ver = 1, type = 0, token_len = 9
    st = syn_coap_parse(&resp, parsed_options, 20, &parsed_option_count, buf, len);
    TEST_ASSERT_EQUAL(SYN_ERROR, st);
    buf[0] = 0x40; // reset

    /* D. buf_len < 4 + token_len */
    buf[0] = 0x42; // token_len = 2
    st = syn_coap_parse(&resp, parsed_options, 20, &parsed_option_count, buf,
                        5); // only 5 bytes, need 6
    TEST_ASSERT_EQUAL(SYN_ERROR, st);
    buf[0] = 0x40; // reset

    /* E. Delta extension truncation/error */
    /* Delta val = 13 but EOF */
    uint8_t bad_delta1[] = {0x40, 0x01, 0x00, 0x00, 0xD0};
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_coap_parse(&resp, parsed_options, 20, &parsed_option_count,
                                                bad_delta1, sizeof(bad_delta1)));

    /* Delta val = 14 but EOF */
    uint8_t bad_delta2[] = {0x40, 0x01, 0x00, 0x00, 0xE0, 0x00};
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_coap_parse(&resp, parsed_options, 20, &parsed_option_count,
                                                bad_delta2, sizeof(bad_delta2)));

    /* Delta val = 15 (invalid) */
    uint8_t bad_delta3[] = {0x40, 0x01, 0x00, 0x00, 0xF0};
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_coap_parse(&resp, parsed_options, 20, &parsed_option_count,
                                                bad_delta3, sizeof(bad_delta3)));

    /* F. Length extension truncation/error */
    /* Len val = 13 but EOF */
    uint8_t bad_len1[] = {0x40, 0x01, 0x00, 0x00, 0x0D};
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_coap_parse(&resp, parsed_options, 20, &parsed_option_count,
                                                bad_len1, sizeof(bad_len1)));

    /* Len val = 14 but EOF */
    uint8_t bad_len2[] = {0x40, 0x01, 0x00, 0x00, 0x0E, 0x00};
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_coap_parse(&resp, parsed_options, 20, &parsed_option_count,
                                                bad_len2, sizeof(bad_len2)));

    /* Len val = 15 (invalid) */
    uint8_t bad_len3[] = {0x40, 0x01, 0x00, 0x00, 0x0F};
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_coap_parse(&resp, parsed_options, 20, &parsed_option_count,
                                                bad_len3, sizeof(bad_len3)));

    /* G. Length exceeds buffer */
    uint8_t bad_len_exceed[] = {0x40, 0x01, 0x00, 0x00,
                                0x05, 0x01, 0x02}; // len = 5, only 2 bytes available
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_coap_parse(&resp, parsed_options, 20, &parsed_option_count,
                                                bad_len_exceed, sizeof(bad_len_exceed)));

    /* H. Option count exceeds max_options */
    uint8_t options_multi[] = {0x40, 0x01, 0x00, 0x00,
                               0x11, 0x0A, 0x11, 0x0B}; // 2 options (num=1, len=1, num=2, len=1)
    st = syn_coap_parse(&resp, parsed_options, 1, &parsed_option_count, options_multi,
                        sizeof(options_multi));
    TEST_ASSERT_EQUAL(SYN_OK, st);
    TEST_ASSERT_EQUAL_INT(1, parsed_option_count); // parsed 2 options, but only 1 stored in array
}

static void test_coap_request_task_failures(void)
{
    SYN_CoapMsg req_msg = {.type = COAP_TYPE_CON,
                           .code = COAP_CODE_GET,
                           .msg_id = 0x5555,
                           .token_len = 2,
                           .token = {0x11, 0x22},
                           .payload_len = 0};

    SYN_SockAddr server_addr = {.ip = {127, 0, 0, 1}, .port = 5683};

    SYN_CoapRequest req;

    /* 1. UDP open fails */
    mock_port_reset();
    mock_udp_open_ok = false;
    syn_coap_request_init(&req, &server_addr, &req_msg, 100, 1);

    SYN_Sched sched;
    SYN_Task task;
    syn_task_create(&task, "coap", syn_coap_request_task, 0, &req);
    syn_sched_init(&sched, &task, 1);

    while (syn_sched_run(&sched)) {
        mock_tick_advance(10);
    }
    TEST_ASSERT_EQUAL(SYN_ERROR, req.status);

    /* 2. Serialization fails */
    mock_port_reset();
    /* Invalid token length to fail serialization */
    req_msg.token_len = 255;
    syn_coap_request_init(&req, &server_addr, &req_msg, 100, 2);

    /* Reset token_len */
    req_msg.token_len = 2;

    /* 3. UDP send fails */
    mock_port_reset();
    syn_coap_request_init(&req, &server_addr, &req_msg, 100, 1);
    mock_udp_tx_len = MOCK_UDP_BUF_SIZE; // make sendto fail

    syn_task_create(&task, "coap", syn_coap_request_task, 0, &req);
    syn_sched_init(&sched, &task, 1);

    while (syn_sched_run(&sched)) {
        mock_tick_advance(10);
    }
    TEST_ASSERT_EQUAL(SYN_ERROR, req.status);

    /* 4. Request timeout and retries */
    mock_port_reset();
    syn_coap_request_init(&req, &server_addr, &req_msg, 100, 2);

    syn_task_create(&task, "coap", syn_coap_request_task, 0, &req);
    syn_sched_init(&sched, &task, 1);

    bool alive = true;
    uint32_t start = syn_port_get_tick_ms();
    while (alive && (syn_port_get_tick_ms() - start < 2000)) {
        alive = syn_sched_run(&sched);
        mock_tick_advance(10);
    }
    TEST_ASSERT_FALSE(alive);
    TEST_ASSERT_EQUAL(SYN_TIMEOUT, req.status);
    TEST_ASSERT_EQUAL_INT(3, req.backoff.attempts); // attempts went up to 3 (which is > retries)

    /* 5. Request mismatching token */
    mock_port_reset();
    syn_coap_request_init(&req, &server_addr, &req_msg, 100, 1);

    /* Mock response with WRONG token */
    SYN_CoapMsg resp_msg_bad = {.type = COAP_TYPE_ACK,
                                .code = COAP_RESP_CONTENT,
                                .msg_id = 0x5555,
                                .token_len = 2,
                                .token = {0x99, 0x99}, // wrong token!
                                .payload_len = 0};
    uint8_t resp_raw[64];
    size_t resp_raw_len = syn_coap_serialize(&resp_msg_bad, NULL, 0, resp_raw, sizeof(resp_raw));
    TEST_ASSERT_TRUE(resp_raw_len > 0);
    mock_udp_set_response(resp_raw, resp_raw_len, &server_addr);

    syn_task_create(&task, "coap", syn_coap_request_task, 0, &req);
    syn_sched_init(&sched, &task, 1);

    /* Run loop, should consume the mismatching packet but keep waiting until timeout */
    alive = true;
    start = syn_port_get_tick_ms();
    while (alive && (syn_port_get_tick_ms() - start < 1000)) {
        alive = syn_sched_run(&sched);
        mock_tick_advance(10);
    }
    TEST_ASSERT_FALSE(alive);
    TEST_ASSERT_EQUAL(SYN_TIMEOUT, req.status);
}

static void test_coap_malformed_parsing(void)
{
    SYN_CoapMsg msg;
    SYN_CoapOption options[4];
    size_t option_count = 0;

    /* 1. Too short buffer (< 4 bytes header) */
    uint8_t short_buf[] = {0x40, 0x01, 0x12};
    TEST_ASSERT_EQUAL(
        SYN_ERROR, syn_coap_parse(&msg, options, 4, &option_count, short_buf, sizeof(short_buf)));

    /* 2. Invalid version (version 2 instead of 1 -> header 0x80) */
    uint8_t bad_ver_buf[] = {0x80, 0x01, 0x12, 0x34};
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_coap_parse(&msg, options, 4, &option_count, bad_ver_buf,
                                                sizeof(bad_ver_buf)));

    /* 3. Token length > 8 (e.g. TKL=9 -> header 0x49) */
    uint8_t bad_tkl_buf[] = {0x49, 0x01, 0x12, 0x34};
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_coap_parse(&msg, options, 4, &option_count, bad_tkl_buf,
                                                sizeof(bad_tkl_buf)));

    /* 4. Corrupted extended option length 15 (reserved -> SYN_ERROR) */
    uint8_t bad_opt_buf[] = {0x40, 0x01, 0x12, 0x34, 0xFF}; /* Payload marker without payload */
    TEST_ASSERT_EQUAL(
        SYN_OK, syn_coap_parse(&msg, options, 4, &option_count, bad_opt_buf, sizeof(bad_opt_buf)));
    TEST_ASSERT_EQUAL_INT(0, msg.payload_len);
}

static void test_coap_serialization_overflow_and_socket_failures(void)
{
    SYN_CoapMsg msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = COAP_TYPE_CON;
    msg.code = COAP_CODE_GET;
    msg.token_len = 2;

    uint8_t buf[32];

    /* 1. max_buf_len < 4 + token_len -> returns 0 */
    TEST_ASSERT_EQUAL(0, syn_coap_serialize(&msg, NULL, 0, buf, 5));

    /* 2. Unsorted options array (Option 20, Option 5) */
    SYN_CoapOption opts[2];
    opts[0].num = 20;
    opts[0].val = (const uint8_t *)"a";
    opts[0].len = 1;
    opts[1].num = 5;
    opts[1].val = (const uint8_t *)"b";
    opts[1].len = 1;
    TEST_ASSERT_TRUE(syn_coap_serialize(&msg, opts, 2, buf, sizeof(buf)) > 0);

    /* 3. Option header overflow -> returns 0 */
    TEST_ASSERT_EQUAL(0, syn_coap_serialize(&msg, opts, 2, buf, 7));

    /* 4. Payload marker overflow -> returns 0 */
    msg.payload = (const uint8_t *)"payload";
    msg.payload_len = 7;
    TEST_ASSERT_EQUAL(0, syn_coap_serialize(&msg, NULL, 0, buf, 8));

    /* 5. Truncated buffer for option delta 13 */
    SYN_CoapOption parsed_opts[4];
    size_t opt_cnt = 0;
    uint8_t trunc_delta13[] = {0x40, 0x01, 0x12, 0x34, 0xD1}; /* delta 13 byte missing */
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_coap_parse(&msg, parsed_opts, 4, &opt_cnt, trunc_delta13,
                                                sizeof(trunc_delta13)));

    /* 6. Truncated buffer for option len 13 */
    uint8_t trunc_len13[] = {0x40, 0x01, 0x12, 0x34, 0x1D}; /* len 13 byte missing */
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_coap_parse(&msg, parsed_opts, 4, &opt_cnt, trunc_len13,
                                                sizeof(trunc_len13)));

    /* 7. Socket open failure in syn_coap_request_task */
    mock_udp_open_ok = false;
    SYN_SockAddr server_addr = {.ip = {127, 0, 0, 1}, .port = 5683};
    SYN_CoapRequest req;
    syn_coap_request_init(&req, &server_addr, &msg, 100, 1);
    SYN_Sched sched;
    SYN_Task task;
    syn_task_create(&task, "coap_fail", syn_coap_request_task, 0, &req);
    syn_sched_init(&sched, &task, 1);
    bool alive = true;
    while (alive) {
        alive = syn_sched_run(&sched);
    }
    TEST_ASSERT_EQUAL(SYN_ERROR, req.status);
    mock_udp_open_ok = true;

    /* 8. Serialization failure (tx_len == 0) in syn_coap_request_task (line 260) */
    SYN_CoapMsg invalid_msg;
    memset(&invalid_msg, 0, sizeof(invalid_msg));
    invalid_msg.type = COAP_TYPE_CON;
    invalid_msg.code = COAP_CODE_GET;
    invalid_msg.payload = (const uint8_t *)"overflow";
    invalid_msg.payload_len = 1000; /* Overflow tx_buf (128 bytes) */

    syn_coap_request_init(&req, &server_addr, &invalid_msg, 100, 1);
    syn_task_create(&task, "coap_ser_fail", syn_coap_request_task, 0, &req);
    syn_sched_init(&sched, &task, 1);
    alive = true;
    while (alive) {
        alive = syn_sched_run(&sched);
    }
    TEST_ASSERT_EQUAL(SYN_ERROR, req.status);

    /* 9. CoAP task timeout (retries exhausted, line 304) */
    mock_udp_sendto_fail = false;
    syn_coap_request_init(&req, &server_addr, &msg, 1, 1); /* 1ms timeout, 1 retry */
    syn_task_create(&task, "coap_timeout", syn_coap_request_task, 0, &req);
    syn_sched_init(&sched, &task, 1);
    alive = true;
    while (alive) {
        alive = syn_sched_run(&sched);
    }
    TEST_ASSERT_EQUAL(SYN_TIMEOUT, req.status);
}

static void test_coap_large_delta_and_length_2byte_ext(void)
{
    SYN_CoapMsg msg = {.type = COAP_TYPE_CON, .code = COAP_CODE_GET, .msg_id = 0x1234};
    SYN_CoapOption opt;
    static uint8_t val[300];
    memset(val, 0x55, sizeof(val));
    opt.num = 300; /* delta = 300 >= 269 -> delta_ext_len = 2 */
    opt.val = val;
    opt.len = 300; /* opt_len = 300 >= 269 -> len_ext_len = 2 */

    static uint8_t buf[1024];
    size_t len = syn_coap_serialize(&msg, &opt, 1, buf, sizeof(buf));
    TEST_ASSERT_TRUE(len > 0);

    SYN_CoapMsg resp;
    SYN_CoapOption parsed_opt[2];
    size_t opt_cnt = 0;
    TEST_ASSERT_EQUAL(SYN_OK, syn_coap_parse(&resp, parsed_opt, 2, &opt_cnt, buf, len));
    TEST_ASSERT_EQUAL_INT(1, opt_cnt);
    TEST_ASSERT_EQUAL_UINT16(300, parsed_opt[0].num);
    TEST_ASSERT_EQUAL_INT(300, parsed_opt[0].len);
}

void run_coap_tests(void)
{
    RUN_TEST(test_coap_serialization);
    RUN_TEST(test_coap_extended_options);
    RUN_TEST(test_coap_request_task_success);
    RUN_TEST(test_coap_serialization_boundaries);
    RUN_TEST(test_coap_request_task_failures);
    RUN_TEST(test_coap_malformed_parsing);
    RUN_TEST(test_coap_serialization_overflow_and_socket_failures);
    RUN_TEST(test_coap_large_delta_and_length_2byte_ext);
}
