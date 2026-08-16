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

static void test_coap_blockwise_and_observe(void)
{
    /* 1. Block1 & Block2 encoding/decoding */
    SYN_CoapBlock blk1 = {.num = 0, .more = true, .szx = 6}; /* 1024 bytes block */
    uint8_t buf[3];
    size_t enc_len = syn_coap_encode_block_opt(&blk1, buf);
    TEST_ASSERT_EQUAL_UINT(1, enc_len);

    SYN_CoapBlock dec_blk1;
    TEST_ASSERT_TRUE(syn_coap_decode_block_opt(buf, enc_len, &dec_blk1));
    TEST_ASSERT_EQUAL_UINT32(0, dec_blk1.num);
    TEST_ASSERT_TRUE(dec_blk1.more);
    TEST_ASSERT_EQUAL_UINT8(6, dec_blk1.szx);

    /* Multibyte block parameter */
    SYN_CoapBlock blk2 = {.num = 1234, .more = false, .szx = 4}; /* 256 bytes block */
    enc_len = syn_coap_encode_block_opt(&blk2, buf);
    TEST_ASSERT_TRUE(enc_len >= 2);

    SYN_CoapBlock dec_blk2;
    TEST_ASSERT_TRUE(syn_coap_decode_block_opt(buf, enc_len, &dec_blk2));
    TEST_ASSERT_EQUAL_UINT32(1234, dec_blk2.num);
    TEST_ASSERT_FALSE(dec_blk2.more);
    TEST_ASSERT_EQUAL_UINT8(4, dec_blk2.szx);

    /* 3-byte block parameter */
    SYN_CoapBlock blk3 = {.num = 100000, .more = true, .szx = 5};
    enc_len = syn_coap_encode_block_opt(&blk3, buf);
    TEST_ASSERT_EQUAL_UINT(3, enc_len);

    SYN_CoapBlock dec_blk3;
    TEST_ASSERT_TRUE(syn_coap_decode_block_opt(buf, enc_len, &dec_blk3));
    TEST_ASSERT_EQUAL_UINT32(100000, dec_blk3.num);
    TEST_ASSERT_TRUE(dec_blk3.more);
    TEST_ASSERT_EQUAL_UINT8(5, dec_blk3.szx);

    /* Huge block number overflow */
    SYN_CoapBlock blk_huge = {.num = 0x1000000, .more = false, .szx = 0};
    TEST_ASSERT_EQUAL_UINT(0, syn_coap_encode_block_opt(&blk_huge, buf));

    /* Error bounds */
    TEST_ASSERT_EQUAL_UINT(0, syn_coap_encode_block_opt(NULL, buf));
    SYN_CoapBlock invalid_szx = {.num = 0, .more = false, .szx = 7};
    TEST_ASSERT_EQUAL_UINT(0, syn_coap_encode_block_opt(&invalid_szx, buf));
    TEST_ASSERT_FALSE(syn_coap_decode_block_opt(NULL, 1, &dec_blk2));
    TEST_ASSERT_FALSE(syn_coap_decode_block_opt(buf, 4, &dec_blk2)); /* > 3 bytes */
    TEST_ASSERT_FALSE(syn_coap_decode_block_opt(buf, 1, NULL));

    /* Invalid szx in decode buffer */
    uint8_t bad_szx_buf[1] = {0x07}; /* szx = 7 */
    TEST_ASSERT_FALSE(syn_coap_decode_block_opt(bad_szx_buf, 1, &dec_blk2));

    /* 2. CoAP message roundtrip with Block2 and Observe options */
    SYN_CoapMsg req = {
        .type = COAP_TYPE_CON, .code = COAP_CODE_GET, .msg_id = 0x4321, .token_len = 0};
    SYN_CoapOption opts[2];
    uint8_t obs_val = 0;
    opts[0].num = COAP_OPT_OBSERVE;
    opts[0].val = &obs_val;
    opts[0].len = 1;

    uint8_t blk_val[3];
    size_t blk_len = syn_coap_encode_block_opt(&blk1, blk_val);
    opts[1].num = COAP_OPT_BLOCK2;
    opts[1].val = blk_val;
    opts[1].len = blk_len;

    uint8_t ser_buf[128];
    size_t ser_len = syn_coap_serialize(&req, opts, 2, ser_buf, sizeof(ser_buf));
    TEST_ASSERT_TRUE(ser_len > 0);

    SYN_CoapMsg parsed_msg;
    SYN_CoapOption parsed_opts[4];
    size_t parsed_cnt = 0;
    TEST_ASSERT_EQUAL(SYN_OK,
                      syn_coap_parse(&parsed_msg, parsed_opts, 4, &parsed_cnt, ser_buf, ser_len));
    TEST_ASSERT_EQUAL_INT(2, parsed_cnt);
    TEST_ASSERT_EQUAL_UINT16(COAP_OPT_OBSERVE, parsed_opts[0].num);
    TEST_ASSERT_EQUAL_UINT16(COAP_OPT_BLOCK2, parsed_opts[1].num);
}

typedef struct {
    uint8_t tx_buf[512];
    size_t tx_len;
    uint8_t rx_buf[512];
    size_t rx_len;
    bool send_fail;
    bool recv_fail;
} MockTransportCtx;

static bool mock_tr_send(const uint8_t *data, size_t len, void *ctx)
{
    MockTransportCtx *m = (MockTransportCtx *)ctx;
    if (m == NULL || m->send_fail || len > sizeof(m->tx_buf)) {
        return false;
    }
    memcpy(m->tx_buf, data, len);
    m->tx_len = len;
    return true;
}

static bool mock_tr_recv(uint8_t *data, size_t max_len, size_t *out_len, void *ctx)
{
    MockTransportCtx *m = (MockTransportCtx *)ctx;
    if (m == NULL || m->recv_fail || m->rx_len == 0) {
        return false;
    }
    size_t copy_len = (m->rx_len > max_len) ? max_len : m->rx_len;
    memcpy(data, m->rx_buf, copy_len);
    *out_len = copy_len;
    m->rx_len = 0;
    return true;
}

static void test_coap_transport_request_task_and_direct_send(void)
{
    MockTransportCtx tr_ctx = {0};
    SYN_Transport tr = {.send = mock_tr_send, .recv = mock_tr_recv, .ctx = &tr_ctx};

    SYN_CoapMsg req = {.type = COAP_TYPE_CON,
                       .code = COAP_CODE_GET,
                       .msg_id = 0x8899,
                       .token_len = 2,
                       .token = {0xAA, 0x55},
                       .payload = NULL,
                       .payload_len = 0};

    SYN_CoapOption req_opt = {
        .num = COAP_OPT_URI_PATH, .val = (const uint8_t *)"telemetry", .len = 9};

    /* 1. Direct one-shot transport send and receive */
    SYN_CoapMsg sim_resp = {.type = COAP_TYPE_ACK,
                            .code = COAP_RESP_CONTENT,
                            .msg_id = 0x8899,
                            .token_len = 2,
                            .token = {0xAA, 0x55},
                            .payload = (const uint8_t *)"23.5C",
                            .payload_len = 5};
    uint8_t sim_buf[128];
    size_t sim_len = syn_coap_serialize(&sim_resp, NULL, 0, sim_buf, sizeof(sim_buf));
    TEST_ASSERT_TRUE(sim_len > 0);

    /* Timeout when rx_buf is empty */
    SYN_CoapMsg resp;
    SYN_CoapOption resp_opts[4];
    size_t resp_opt_cnt = 0;
    uint8_t resp_buf[128];
    TEST_ASSERT_EQUAL(SYN_TIMEOUT,
                      syn_coap_transport_send_request(&tr, &req, &req_opt, 1, &resp, resp_opts, 4,
                                                      &resp_opt_cnt, resp_buf, sizeof(resp_buf)));

    /* Transport send error */
    tr_ctx.send_fail = true;
    TEST_ASSERT_EQUAL(SYN_ERROR,
                      syn_coap_transport_send_request(&tr, &req, &req_opt, 1, &resp, resp_opts, 4,
                                                      &resp_opt_cnt, resp_buf, sizeof(resp_buf)));
    tr_ctx.send_fail = false;

    /* Stage response in tr_ctx.rx_buf */
    memcpy(tr_ctx.rx_buf, sim_buf, sim_len);
    tr_ctx.rx_len = sim_len;

    TEST_ASSERT_EQUAL(SYN_OK,
                      syn_coap_transport_send_request(&tr, &req, &req_opt, 1, &resp, resp_opts, 4,
                                                      &resp_opt_cnt, resp_buf, sizeof(resp_buf)));
    TEST_ASSERT_EQUAL(COAP_RESP_CONTENT, resp.code);
    TEST_ASSERT_EQUAL(5, resp.payload_len);
    TEST_ASSERT_EQUAL_MEMORY("23.5C", resp.payload, 5);

    /* Token mismatch returns SYN_ERROR */
    SYN_CoapMsg bad_token_resp = sim_resp;
    bad_token_resp.token[0] ^= 0xFF;
    uint8_t bad_tok_buf[128];
    size_t bad_tok_len =
        syn_coap_serialize(&bad_token_resp, NULL, 0, bad_tok_buf, sizeof(bad_tok_buf));
    memcpy(tr_ctx.rx_buf, bad_tok_buf, bad_tok_len);
    tr_ctx.rx_len = bad_tok_len;
    TEST_ASSERT_EQUAL(SYN_ERROR,
                      syn_coap_transport_send_request(&tr, &req, &req_opt, 1, &resp, resp_opts, 4,
                                                      &resp_opt_cnt, resp_buf, sizeof(resp_buf)));

    /* 2. Cooperative Protothread Transport Request Task */
    SYN_CoapTransportRequest treq;
    syn_coap_transport_request_init(&treq, &tr, &req, 50, 2);
    treq.req_options = &req_opt;
    treq.req_option_count = 1;

    SYN_PT pt;
    PT_INIT(&pt);
    SYN_Task task = {.user_data = &treq};

    /* First step yields because rx_buf is empty */
    tr_ctx.rx_len = 0;
    SYN_PT_Status pst_yield = syn_coap_transport_request_task(&pt, &task);
    TEST_ASSERT_EQUAL(PT_YIELDED, pst_yield);

    /* Stage valid response in tr_ctx.rx_buf and resume task */
    memcpy(tr_ctx.rx_buf, sim_buf, sim_len);
    tr_ctx.rx_len = sim_len;

    SYN_PT_Status pst = syn_coap_transport_request_task(&pt, &task);
    TEST_ASSERT_EQUAL(PT_EXITED, pst);
    TEST_ASSERT_EQUAL(SYN_OK, treq.status);
    TEST_ASSERT_EQUAL(COAP_RESP_CONTENT, treq.resp_msg.code);
    TEST_ASSERT_EQUAL_MEMORY("23.5C", treq.resp_msg.payload, 5);
}

static void test_coaps_client_e2e_dtls13(void)
{
    MockTransportCtx client_wire = {0};
    SYN_Transport client_raw_tr = {.send = mock_tr_send, .recv = mock_tr_recv, .ctx = &client_wire};

    static const uint8_t psk[32] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                                    0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
                                    0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
                                    0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20};

    SYN_DTLS_Config dtls_cfg = {
        .mode = SYN_DTLS_AUTH_MODE_PSK,
        .cipher_suite = SYN_DTLS_CIPHER_SUITE_CHACHA20_POLY1305_SHA256,
        .server_name = "coaps.syntropic.local",
        .psk_identity = (const uint8_t *)"client_1",
        .psk_identity_len = 8,
        .psk_secret = psk,
        .psk_secret_len = sizeof(psk),
    };

    uint8_t dtls_rx[512];
    uint8_t dtls_tx[512];

    SYN_CoapsClient client;
    TEST_ASSERT_TRUE(syn_coaps_client_init(&client, &dtls_cfg, &client_raw_tr, dtls_rx,
                                           sizeof(dtls_rx), dtls_tx, sizeof(dtls_tx)));

    TEST_ASSERT_TRUE(syn_coaps_client_handshake(&client));
    TEST_ASSERT_EQUAL(SYN_DTLS_STATE_ESTABLISHED, client.dtls.state);

    /* Construct CoAP GET /sensors/temperature request */
    SYN_CoapMsg req = {.type = COAP_TYPE_CON,
                       .code = COAP_CODE_GET,
                       .msg_id = 0x9001,
                       .token_len = 4,
                       .token = {0xDE, 0xAD, 0xBE, 0xEF},
                       .payload = NULL,
                       .payload_len = 0};
    SYN_CoapOption req_opts[2];
    req_opts[0].num = COAP_OPT_URI_PATH;
    req_opts[0].val = (const uint8_t *)"sensors";
    req_opts[0].len = 7;
    req_opts[1].num = COAP_OPT_URI_PATH;
    req_opts[1].val = (const uint8_t *)"temperature";
    req_opts[1].len = 11;

    /* 1. Client serializes and sends encrypted CoAPS request over dtls_transport */
    uint8_t client_coap_tx[256];
    size_t coap_tx_len =
        syn_coap_serialize(&req, req_opts, 2, client_coap_tx, sizeof(client_coap_tx));
    TEST_ASSERT_TRUE(coap_tx_len > 0);

    TEST_ASSERT_TRUE(syn_transport_send(&client.dtls_transport, client_coap_tx, coap_tx_len));
    TEST_ASSERT_TRUE(client_wire.tx_len > 0);

    /* Verify encrypted datagram header */
    TEST_ASSERT_EQUAL_HEX8(SYN_DTLS_UNIFIED_FIXED_BIT | SYN_DTLS_UNIFIED_LEN_BIT |
                               SYN_DTLS_EPOCH_APP_DATA,
                           client_wire.tx_buf[0]);

    /* 2. Decrypt on server side */
    memcpy(client_wire.rx_buf, client_wire.tx_buf, client_wire.tx_len);
    client_wire.rx_len = client_wire.tx_len;

    uint8_t server_decrypted[256];
    size_t server_dec_len = 0;
    TEST_ASSERT_TRUE(syn_transport_recv(&client.dtls_transport, server_decrypted,
                                        sizeof(server_decrypted), &server_dec_len));
    TEST_ASSERT_EQUAL(coap_tx_len, server_dec_len);
    TEST_ASSERT_EQUAL_MEMORY(client_coap_tx, server_decrypted, server_dec_len);

    /* Parse CoAP on server */
    SYN_CoapMsg server_parsed_req;
    SYN_CoapOption server_parsed_opts[4];
    size_t server_opt_cnt = 0;
    TEST_ASSERT_EQUAL(SYN_OK, syn_coap_parse(&server_parsed_req, server_parsed_opts, 4,
                                             &server_opt_cnt, server_decrypted, server_dec_len));
    TEST_ASSERT_EQUAL(COAP_CODE_GET, server_parsed_req.code);
    TEST_ASSERT_EQUAL(2, server_opt_cnt);

    /* 3. Server generates 2.05 Content response */
    static const char resp_payload[] = "{\"temp\": 24.2}";
    SYN_CoapMsg s_resp = {.type = COAP_TYPE_ACK,
                          .code = COAP_RESP_CONTENT,
                          .msg_id = req.msg_id,
                          .token_len = req.token_len,
                          .payload = (const uint8_t *)resp_payload,
                          .payload_len = strlen(resp_payload)};
    memcpy(s_resp.token, req.token, req.token_len);

    uint8_t server_resp_coap[256];
    size_t server_resp_coap_len =
        syn_coap_serialize(&s_resp, NULL, 0, server_resp_coap, sizeof(server_resp_coap));
    TEST_ASSERT_TRUE(server_resp_coap_len > 0);

    /* Server encrypts CoAP response and stages it into client_wire.rx_buf */
    TEST_ASSERT_TRUE(
        syn_transport_send(&client.dtls_transport, server_resp_coap, server_resp_coap_len));
    memcpy(client_wire.rx_buf, client_wire.tx_buf, client_wire.tx_len);
    client_wire.rx_len = client_wire.tx_len;

    /* 4. Client receives response */
    SYN_CoapMsg client_resp;
    SYN_CoapOption client_resp_opts[4];
    size_t client_resp_opt_cnt = 0;
    uint8_t client_resp_buf[256];

    size_t rx_len = 0;
    TEST_ASSERT_TRUE(syn_transport_recv(&client.dtls_transport, client_resp_buf,
                                        sizeof(client_resp_buf), &rx_len));
    TEST_ASSERT_EQUAL(server_resp_coap_len, rx_len);
    TEST_ASSERT_EQUAL(SYN_OK, syn_coap_parse(&client_resp, client_resp_opts, 4,
                                             &client_resp_opt_cnt, client_resp_buf, rx_len));
    TEST_ASSERT_EQUAL(COAP_RESP_CONTENT, client_resp.code);
    TEST_ASSERT_EQUAL(strlen(resp_payload), client_resp.payload_len);
    TEST_ASSERT_EQUAL_MEMORY(resp_payload, client_resp.payload, client_resp.payload_len);

    /* 5. End-to-end syn_coaps_client_send_request with pre-staged encrypted response */
    TEST_ASSERT_TRUE(
        syn_transport_send(&client.dtls_transport, server_resp_coap, server_resp_coap_len));
    memcpy(client_wire.rx_buf, client_wire.tx_buf, client_wire.tx_len);
    client_wire.rx_len = client_wire.tx_len;

    TEST_ASSERT_EQUAL(SYN_OK, syn_coaps_client_send_request(
                                  &client, &req, req_opts, 2, &client_resp, client_resp_opts, 4,
                                  &client_resp_opt_cnt, client_resp_buf, sizeof(client_resp_buf)));
    TEST_ASSERT_EQUAL(COAP_RESP_CONTENT, client_resp.code);
    TEST_ASSERT_EQUAL_MEMORY(resp_payload, client_resp.payload, client_resp.payload_len);
}

static void test_coap_transport_and_coaps_null_and_bounds_checks(void)
{
    /* Null checks for syn_coap_transport_request_init */
    syn_coap_transport_request_init(NULL, NULL, NULL, 0, 0);

    /* Null checks for syn_coap_transport_request_task */
    SYN_PT pt;
    PT_INIT(&pt);
    TEST_ASSERT_EQUAL(PT_EXITED, syn_coap_transport_request_task(NULL, NULL));
    TEST_ASSERT_EQUAL(PT_EXITED, syn_coap_transport_request_task(&pt, NULL));
    SYN_Task empty_task = {0};
    TEST_ASSERT_EQUAL(PT_EXITED, syn_coap_transport_request_task(&pt, &empty_task));

    SYN_CoapTransportRequest r;
    syn_coap_transport_request_init(&r, NULL, NULL, 0, 0);
    SYN_Task task_with_r = {.user_data = &r};
    TEST_ASSERT_EQUAL(PT_ENDED, syn_coap_transport_request_task(&pt, &task_with_r));
    TEST_ASSERT_EQUAL(SYN_ERROR, r.status);

    /* Task serialize error */
    SYN_Transport dummy_tr = {0};
    SYN_CoapMsg huge_msg = {.type = COAP_TYPE_CON, .token_len = 9};
    syn_coap_transport_request_init(&r, &dummy_tr, &huge_msg, 0, 0);
    PT_INIT(&pt);
    TEST_ASSERT_EQUAL(PT_ENDED, syn_coap_transport_request_task(&pt, &task_with_r));
    TEST_ASSERT_EQUAL(SYN_ERROR, r.status);

    /* Task send fail */
    MockTransportCtx tr_ctx = {.send_fail = true};
    SYN_Transport fail_tr = {.send = mock_tr_send, .recv = mock_tr_recv, .ctx = &tr_ctx};
    SYN_CoapMsg ok_msg = {.type = COAP_TYPE_CON, .token_len = 0};
    syn_coap_transport_request_init(&r, &fail_tr, &ok_msg, 10, 0);
    PT_INIT(&pt);
    TEST_ASSERT_EQUAL(PT_EXITED, syn_coap_transport_request_task(&pt, &task_with_r));
    TEST_ASSERT_EQUAL(SYN_ERROR, r.status);

    /* Null checks for syn_coap_transport_send_request */
    SYN_CoapMsg msg = {0};
    uint8_t buf[64];
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_coap_transport_send_request(NULL, &msg, NULL, 0, &msg, NULL, 0,
                                                                 NULL, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_coap_transport_send_request(&dummy_tr, NULL, NULL, 0, &msg,
                                                                 NULL, 0, NULL, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_coap_transport_send_request(&dummy_tr, &msg, NULL, 0, NULL,
                                                                 NULL, 0, NULL, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_coap_transport_send_request(&dummy_tr, &msg, NULL, 0, &msg,
                                                                 NULL, 0, NULL, NULL, 0));
    TEST_ASSERT_EQUAL(SYN_ERROR,
                      syn_coap_transport_send_request(&dummy_tr, &huge_msg, NULL, 0, &msg, NULL, 0,
                                                      NULL, buf, sizeof(buf)));

    /* Parse error check in syn_coap_transport_send_request (corrupt response) */
    MockTransportCtx corrupt_ctx = {0};
    corrupt_ctx.rx_buf[0] = 0xFF; /* invalid version */
    corrupt_ctx.rx_len = 10;
    SYN_Transport corrupt_tr = {.send = mock_tr_send, .recv = mock_tr_recv, .ctx = &corrupt_ctx};
    TEST_ASSERT_EQUAL(SYN_ERROR,
                      syn_coap_transport_send_request(&corrupt_tr, &ok_msg, NULL, 0, &msg, NULL, 0,
                                                      NULL, buf, sizeof(buf)));

    /* Null checks for syn_coaps_client_* */
    TEST_ASSERT_FALSE(syn_coaps_client_init(NULL, NULL, NULL, NULL, 0, NULL, 0));
    SYN_CoapsClient client;
    SYN_DTLS_Config valid_cfg = {
        .mode = SYN_DTLS_AUTH_MODE_PSK,
        .cipher_suite = SYN_DTLS_CIPHER_SUITE_CHACHA20_POLY1305_SHA256,
        .psk_identity = (const uint8_t *)"id",
        .psk_identity_len = 2,
    };
    TEST_ASSERT_FALSE(
        syn_coaps_client_init(&client, NULL, &dummy_tr, buf, sizeof(buf), buf, sizeof(buf)));
    TEST_ASSERT_FALSE(
        syn_coaps_client_init(&client, &valid_cfg, NULL, buf, sizeof(buf), buf, sizeof(buf)));
    TEST_ASSERT_FALSE(syn_coaps_client_init(&client, &valid_cfg, &dummy_tr, buf, 10, buf,
                                            10)); /* buffer too small */
    TEST_ASSERT_FALSE(syn_coaps_client_handshake(NULL));
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_coaps_client_send_request(NULL, &msg, NULL, 0, &msg, NULL, 0,
                                                               NULL, buf, sizeof(buf)));

    memset(&client, 0, sizeof(client));
    client.dtls.state = SYN_DTLS_STATE_CLIENT_HELLO_SENT;
    client.dtls.config.mode =
        SYN_DTLS_AUTH_MODE_RAW_PUBKEY; /* peer_pubkey == NULL causes handshake fail */
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_coaps_client_send_request(&client, &msg, NULL, 0, &msg, NULL,
                                                               0, NULL, buf, sizeof(buf)));
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
    RUN_TEST(test_coap_blockwise_and_observe);
    RUN_TEST(test_coap_transport_request_task_and_direct_send);
    RUN_TEST(test_coaps_client_e2e_dtls13);
    RUN_TEST(test_coap_transport_and_coaps_null_and_bounds_checks);
}
