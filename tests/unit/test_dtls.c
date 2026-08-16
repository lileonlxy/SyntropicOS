/**
 * @file test_dtls.c
 * @brief Unit tests for DTLS 1.3 Datagram Security Protocol (RFC 9147).
 */

#include "syntropic/net/syn_dtls.h"
#include "unity/unity.h"

#include <string.h>

typedef struct {
    uint8_t buf[2048];
    size_t len;
} DatagramWire;

static bool loopback_send(const uint8_t *data, size_t len, void *ctx)
{
    DatagramWire *wire = (DatagramWire *)ctx;
    if (len > sizeof(wire->buf)) {
        return false;
    }
    memcpy(wire->buf, data, len);
    wire->len = len;
    return true;
}

static bool loopback_recv(uint8_t *data, size_t max_len, size_t *out_len, void *ctx)
{
    DatagramWire *wire = (DatagramWire *)ctx;
    if (wire->len == 0) {
        return false;
    }
    size_t copy_len = (wire->len > max_len) ? max_len : wire->len;
    memcpy(data, wire->buf, copy_len);
    *out_len = copy_len;
    wire->len = 0;
    return true;
}

void test_dtls_sliding_window_anti_replay(void)
{
    SYN_DTLS_ReplayWindow win;
    memset(&win, 0, sizeof(win));

    /* 1. First packet */
    TEST_ASSERT_TRUE(syn_dtls_replay_check(&win, 0));
    syn_dtls_replay_update(&win, 0);
    TEST_ASSERT_TRUE(win.initialized);
    TEST_ASSERT_EQUAL_UINT64(0, win.max_seq);

    /* 2. In-order packet */
    TEST_ASSERT_TRUE(syn_dtls_replay_check(&win, 1));
    syn_dtls_replay_update(&win, 1);
    TEST_ASSERT_EQUAL_UINT64(1, win.max_seq);

    /* 3. Duplicate packet 0 & 1 -> rejected */
    TEST_ASSERT_FALSE(syn_dtls_replay_check(&win, 0));
    TEST_ASSERT_FALSE(syn_dtls_replay_check(&win, 1));

    /* 4. Forward jump out-of-order within window */
    TEST_ASSERT_TRUE(syn_dtls_replay_check(&win, 5));
    syn_dtls_replay_update(&win, 5);
    TEST_ASSERT_EQUAL_UINT64(5, win.max_seq);

    /* Out-of-order previous unreceived packets */
    TEST_ASSERT_TRUE(syn_dtls_replay_check(&win, 3));
    syn_dtls_replay_update(&win, 3);
    TEST_ASSERT_TRUE(syn_dtls_replay_check(&win, 2));
    syn_dtls_replay_update(&win, 2);
    TEST_ASSERT_TRUE(syn_dtls_replay_check(&win, 4));
    syn_dtls_replay_update(&win, 4);

    /* Now 2, 3, 4 are duplicates */
    TEST_ASSERT_FALSE(syn_dtls_replay_check(&win, 2));
    TEST_ASSERT_FALSE(syn_dtls_replay_check(&win, 3));
    TEST_ASSERT_FALSE(syn_dtls_replay_check(&win, 4));
    TEST_ASSERT_FALSE(syn_dtls_replay_check(&win, 5));

    /* 5. Advance window forward by 70 packets */
    TEST_ASSERT_TRUE(syn_dtls_replay_check(&win, 75));
    syn_dtls_replay_update(&win, 75);
    TEST_ASSERT_EQUAL_UINT64(75, win.max_seq);

    /* Packet 5 is now 70 packets old (> 64-packet window) -> rejected */
    TEST_ASSERT_FALSE(syn_dtls_replay_check(&win, 5));
    TEST_ASSERT_FALSE(syn_dtls_replay_check(&win, 10));

    /* Packet 74 is within window */
    TEST_ASSERT_TRUE(syn_dtls_replay_check(&win, 74));
    syn_dtls_replay_update(&win, 74);
    TEST_ASSERT_FALSE(syn_dtls_replay_check(&win, 74));

    /* 6. Huge forward jump (> 64) resets bitmap */
    TEST_ASSERT_TRUE(syn_dtls_replay_check(&win, 200));
    syn_dtls_replay_update(&win, 200);
    TEST_ASSERT_EQUAL_UINT64(200, win.max_seq);
    TEST_ASSERT_FALSE(syn_dtls_replay_check(&win, 75));
}

void test_dtls_psk_mode_all_cipher_suites(void)
{
    DatagramWire wire = {0};
    SYN_Transport tr = {.send = loopback_send, .recv = loopback_recv, .ctx = &wire};

    static const uint8_t psk[32] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                                    0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
                                    0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
                                    0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20};

    uint8_t rx_record_buf[2560];
    uint8_t tx_record_buf[2560];
    uint8_t rx_buf[256];
    size_t rx_len = 0;
    static const char sample_payload[] = "DTLS 1.3 CoAP Protected Telemetry Datagram";

    SYN_DTLS_CipherSuite suites[] = {
        SYN_DTLS_CIPHER_SUITE_CHACHA20_POLY1305_SHA256, SYN_DTLS_CIPHER_SUITE_AES_128_GCM_SHA256,
        SYN_DTLS_CIPHER_SUITE_AES_256_GCM_SHA384, SYN_DTLS_CIPHER_SUITE_AES_128_CCM_SHA256,
        SYN_DTLS_CIPHER_SUITE_AES_128_CCM_8_SHA256};

    for (size_t i = 0; i < sizeof(suites) / sizeof(suites[0]); i++) {
        SYN_DTLS_Config cfg = {.mode = SYN_DTLS_AUTH_MODE_PSK,
                               .cipher_suite = suites[i],
                               .server_name = "sensor.dtls.local",
                               .psk_identity = (const uint8_t *)"sensor-node-01",
                               .psk_identity_len = 14,
                               .psk_secret = psk,
                               .psk_secret_len = sizeof(psk)};
        SYN_DTLS_Context dtls;
        wire.len = 0;
        TEST_ASSERT_TRUE(syn_dtls_init(&dtls, &cfg, &tr, rx_record_buf, sizeof(rx_record_buf),
                                       tx_record_buf, sizeof(tx_record_buf)));
        TEST_ASSERT_TRUE(syn_dtls_handshake(&dtls));
        TEST_ASSERT_EQUAL(SYN_DTLS_STATE_ESTABLISHED, dtls.state);

        /* Send application data */
        TEST_ASSERT_TRUE(
            syn_dtls_send(&dtls, (const uint8_t *)sample_payload, strlen(sample_payload)));
        TEST_ASSERT_GREATER_THAN(0, wire.len);

        /* Receive and decrypt application data */
        memset(rx_buf, 0, sizeof(rx_buf));
        TEST_ASSERT_TRUE(syn_dtls_recv(&dtls, rx_buf, sizeof(rx_buf), &rx_len));
        TEST_ASSERT_EQUAL(strlen(sample_payload), rx_len);
        TEST_ASSERT_EQUAL_MEMORY(sample_payload, rx_buf, rx_len);
    }
}

void test_dtls_seq_16bit_and_anti_replay_rejection(void)
{
    DatagramWire wire = {0};
    SYN_Transport tr = {.send = loopback_send, .recv = loopback_recv, .ctx = &wire};

    static const uint8_t psk[32] = {0x42};
    SYN_DTLS_Config cfg = {.mode = SYN_DTLS_AUTH_MODE_PSK,
                           .cipher_suite = SYN_DTLS_CIPHER_SUITE_AES_128_GCM_SHA256,
                           .server_name = "test.local",
                           .psk_identity = (const uint8_t *)"psk",
                           .psk_identity_len = 3,
                           .psk_secret = psk,
                           .psk_secret_len = sizeof(psk)};

    uint8_t rx_record_buf[2560];
    uint8_t tx_record_buf[2560];
    SYN_DTLS_Context dtls;
    TEST_ASSERT_TRUE(syn_dtls_init(&dtls, &cfg, &tr, rx_record_buf, sizeof(rx_record_buf),
                                   tx_record_buf, sizeof(tx_record_buf)));
    TEST_ASSERT_TRUE(syn_dtls_handshake(&dtls));

    /* 1. Test 16-bit sequence number (seq > 255) */
    dtls.client_seq_num = 500;
    static const char msg16[] = "16-bit sequence number test payload";
    TEST_ASSERT_TRUE(syn_dtls_send(&dtls, (const uint8_t *)msg16, strlen(msg16)));

    uint8_t saved_packet[2048];
    size_t saved_len = wire.len;
    memcpy(saved_packet, wire.buf, saved_len);

    uint8_t rx_buf[128];
    size_t rx_len = 0;
    TEST_ASSERT_TRUE(syn_dtls_recv(&dtls, rx_buf, sizeof(rx_buf), &rx_len));
    TEST_ASSERT_EQUAL(strlen(msg16), rx_len);
    TEST_ASSERT_EQUAL_MEMORY(msg16, rx_buf, rx_len);

    /* 2. Replay the same packet -> must be rejected by anti-replay window */
    memcpy(wire.buf, saved_packet, saved_len);
    wire.len = saved_len;
    TEST_ASSERT_FALSE(syn_dtls_recv(&dtls, rx_buf, sizeof(rx_buf), &rx_len));

    /* 3. Tampered MAC tag -> rejected */
    TEST_ASSERT_TRUE(syn_dtls_send(&dtls, (const uint8_t *)msg16, strlen(msg16)));
    wire.buf[wire.len - 1] ^= 0xFF; /* Corrupt authentication tag */
    TEST_ASSERT_FALSE(syn_dtls_recv(&dtls, rx_buf, sizeof(rx_buf), &rx_len));

    /* 4. Corrupted header fixed bit -> rejected */
    TEST_ASSERT_TRUE(syn_dtls_send(&dtls, (const uint8_t *)msg16, strlen(msg16)));
    wire.buf[0] &= (uint8_t)~SYN_DTLS_UNIFIED_FIXED_BIT;
    TEST_ASSERT_FALSE(syn_dtls_recv(&dtls, rx_buf, sizeof(rx_buf), &rx_len));
}

void test_dtls_transport_binding_and_task(void)
{
    DatagramWire wire = {0};
    SYN_Transport raw_tr = {.send = loopback_send, .recv = loopback_recv, .ctx = &wire};

    static const uint8_t psk[32] = {0x77};
    SYN_DTLS_Config cfg = {.mode = SYN_DTLS_AUTH_MODE_PSK,
                           .cipher_suite = SYN_DTLS_CIPHER_SUITE_CHACHA20_POLY1305_SHA256,
                           .psk_secret = psk,
                           .psk_secret_len = sizeof(psk)};

    uint8_t rx_record_buf[2560];
    uint8_t tx_record_buf[2560];
    SYN_DTLS_Context dtls;
    TEST_ASSERT_TRUE(syn_dtls_init(&dtls, &cfg, &raw_tr, rx_record_buf, sizeof(rx_record_buf),
                                   tx_record_buf, sizeof(tx_record_buf)));

    SYN_Transport dtls_tr;
    syn_dtls_bind_transport(&dtls, &dtls_tr);

    static const char sample_msg[] = "Protected over SYN_Transport adapter";
    TEST_ASSERT_TRUE(syn_transport_send(&dtls_tr, (const uint8_t *)sample_msg, strlen(sample_msg)));

    uint8_t rx[128];
    size_t rx_len = 0;
    TEST_ASSERT_TRUE(syn_transport_recv(&dtls_tr, rx, sizeof(rx), &rx_len));
    TEST_ASSERT_EQUAL(strlen(sample_msg), rx_len);
    TEST_ASSERT_EQUAL_MEMORY(sample_msg, rx, rx_len);

    /* Test protothread task */
    SYN_PT pt;
    PT_INIT(&pt);
    SYN_Task task = {.user_data = &dtls};
    TEST_ASSERT_EQUAL(PT_YIELDED, syn_dtls_task(&pt, &task));
    TEST_ASSERT_EQUAL(SYN_DTLS_STATE_ESTABLISHED, dtls.state);
}

void test_dtls_null_and_bounds_checks(void)
{
    uint8_t buf[256];
    size_t out_len = 0;

    TEST_ASSERT_FALSE(syn_dtls_replay_check(NULL, 0));
    syn_dtls_replay_update(NULL, 0);

    TEST_ASSERT_FALSE(syn_dtls_init(NULL, NULL, NULL, NULL, 0, NULL, 0));
    TEST_ASSERT_FALSE(syn_dtls_handshake(NULL));
    TEST_ASSERT_FALSE(syn_dtls_send(NULL, buf, 10));
    TEST_ASSERT_FALSE(syn_dtls_recv(NULL, buf, sizeof(buf), &out_len));

    syn_dtls_bind_transport(NULL, NULL);
    TEST_ASSERT_EQUAL(PT_EXITED, syn_dtls_task(NULL, NULL));
    SYN_PT pt_dummy;
    PT_INIT(&pt_dummy);
    SYN_Task null_task = {.user_data = NULL};
    TEST_ASSERT_EQUAL(PT_EXITED, syn_dtls_task(&pt_dummy, &null_task));

    /* Zero length send and auto-handshake on unestablished state */
    DatagramWire wire = {0};
    SYN_Transport tr = {.send = loopback_send, .recv = loopback_recv, .ctx = &wire};
    static const uint8_t psk[32] = {0x01};
    SYN_DTLS_Config cfg = {.mode = SYN_DTLS_AUTH_MODE_PSK,
                           .cipher_suite = SYN_DTLS_CIPHER_SUITE_AES_128_GCM_SHA256,
                           .psk_secret = psk,
                           .psk_secret_len = sizeof(psk)};
    SYN_DTLS_Context dtls;
    uint8_t tx_buf[256];
    uint8_t rx_buf[256];
    TEST_ASSERT_TRUE(
        syn_dtls_init(&dtls, &cfg, &tr, tx_buf, sizeof(tx_buf), rx_buf, sizeof(rx_buf)));
    TEST_ASSERT_TRUE(syn_dtls_send(&dtls, NULL, 0));

    /* Auto-handshake on send */
    dtls.state = SYN_DTLS_STATE_CLIENT_HELLO_SENT;
    uint8_t msg[] = "auto-handshake test";
    TEST_ASSERT_TRUE(syn_dtls_send(&dtls, msg, sizeof(msg)));

    /* Buffer too small to send */
    dtls.tx_buf_size = 10;
    TEST_ASSERT_FALSE(syn_dtls_send(&dtls, msg, sizeof(msg)));
    dtls.tx_buf_size = sizeof(tx_buf);

    /* Auto-handshake on recv */
    dtls.state = SYN_DTLS_STATE_CLIENT_HELLO_SENT;
    uint8_t recv_out[256];
    TEST_ASSERT_TRUE(syn_dtls_recv(&dtls, recv_out, sizeof(recv_out), &out_len));

    /* Short rx packet (< 4 bytes) */
    wire.len = 2;
    wire.buf[0] = 0x20;
    TEST_ASSERT_FALSE(syn_dtls_recv(&dtls, recv_out, sizeof(recv_out), &out_len));

    /* Invalid fixed bit in header */
    wire.len = 20;
    wire.buf[0] = 0x00; /* Missing SYN_DTLS_UNIFIED_FIXED_BIT */
    TEST_ASSERT_FALSE(syn_dtls_recv(&dtls, recv_out, sizeof(recv_out), &out_len));

    /* Truncated header check */
    wire.len = 5;
    wire.buf[0] = SYN_DTLS_UNIFIED_FIXED_BIT | SYN_DTLS_UNIFIED_LEN_BIT;
    wire.buf[1] = 0x01;
    wire.buf[2] = 0x00;
    wire.buf[3] = 0x20;
    TEST_ASSERT_FALSE(syn_dtls_recv(&dtls, recv_out, sizeof(recv_out), &out_len));

    /* Transport recv failure */
    wire.len = 0;
    TEST_ASSERT_FALSE(syn_dtls_recv(&dtls, recv_out, sizeof(recv_out), &out_len));

    /* Handshake failure inside send and recv */
    dtls.state = SYN_DTLS_STATE_CLIENT_HELLO_SENT;
    dtls.config.mode = SYN_DTLS_AUTH_MODE_RAW_PUBKEY;
    dtls.config.peer_pubkey = NULL;
    dtls.config.root_ca = NULL;
    TEST_ASSERT_FALSE(syn_dtls_send(&dtls, msg, sizeof(msg)));
    TEST_ASSERT_FALSE(syn_dtls_recv(&dtls, recv_out, sizeof(recv_out), &out_len));

    /* Task handshake failure */
    SYN_PT pt_fail;
    PT_INIT(&pt_fail);
    SYN_Task task_fail = {.user_data = &dtls};
    TEST_ASSERT_EQUAL(PT_ENDED, syn_dtls_task(&pt_fail, &task_fail));
    TEST_ASSERT_EQUAL(SYN_DTLS_STATE_ERROR, dtls.state);

    /* Restore valid config */
    dtls.config = cfg;
    TEST_ASSERT_TRUE(syn_dtls_handshake(&dtls));

    /* Task yield and exit when transitioning away from ESTABLISHED */
    SYN_PT pt_loop;
    PT_INIT(&pt_loop);
    TEST_ASSERT_EQUAL(PT_YIELDED, syn_dtls_task(&pt_loop, &task_fail));
    dtls.state = SYN_DTLS_STATE_UNINITIALIZED;
    TEST_ASSERT_EQUAL(PT_EXITED, syn_dtls_task(&pt_loop, &task_fail));

    /* Large payload send check (when tx_buf is large enough) */
    uint8_t tx_large[4096];
    dtls.tx_buf = tx_large;
    dtls.tx_buf_size = sizeof(tx_large);
    uint8_t large_msg[2049];
    memset(large_msg, 0x55, sizeof(large_msg));
    TEST_ASSERT_FALSE(syn_dtls_send(&dtls, large_msg, sizeof(large_msg)));
    dtls.tx_buf = tx_buf;
    dtls.tx_buf_size = sizeof(tx_buf);

    /* Receive datagram without length bit (has_len = false) */
    uint64_t seq_nolen = dtls.client_seq_num++;
    uint8_t hdr_nolen[2];
    hdr_nolen[0] = SYN_DTLS_UNIFIED_FIXED_BIT | ((uint8_t)dtls.epoch & SYN_DTLS_UNIFIED_EPOCH_MASK);
    hdr_nolen[1] = (uint8_t)(seq_nolen & 0xFFU);

    uint8_t pt_nolen[32];
    static const char nolen_text[] = "no-length-bit";
    size_t nolen_text_len = strlen(nolen_text);
    memcpy(pt_nolen, nolen_text, nolen_text_len);
    pt_nolen[nolen_text_len] = 0x17U; /* Inner content type */

    uint8_t nonce[12];
    memcpy(nonce, dtls.client_app_iv, 12);
    uint64_t full_seq = ((uint64_t)dtls.epoch << 48U) | (seq_nolen & 0xFFFFFFFFFFFFULL);
    for (int i = 0; i < 8; i++) {
        nonce[11 - i] ^= (uint8_t)((full_seq >> (i * 8)) & 0xFF);
    }

    wire.buf[0] = hdr_nolen[0];
    wire.buf[1] = hdr_nolen[1];
    SYN_AES_GCM_Context gcm;
    syn_aes_gcm_init(&gcm, dtls.client_app_key, 16U);
    syn_aes_gcm_encrypt(&gcm, nonce, 12U, hdr_nolen, 2U, pt_nolen, nolen_text_len + 1U,
                        &wire.buf[2], &wire.buf[2 + nolen_text_len + 1U]);
    wire.len = 2U + (nolen_text_len + 1U) + 16U;

    TEST_ASSERT_TRUE(syn_dtls_recv(&dtls, recv_out, sizeof(recv_out), &out_len));
    TEST_ASSERT_EQUAL(nolen_text_len, out_len);
    TEST_ASSERT_EQUAL_MEMORY(nolen_text, recv_out, out_len);

    /* Datagram with wrong inner content type (0x16 instead of 0x17) and fresh sequence number */
    uint64_t seq_wrong_ct = dtls.client_seq_num++;
    hdr_nolen[1] = (uint8_t)(seq_wrong_ct & 0xFFU);
    memcpy(nonce, dtls.client_app_iv, 12);
    full_seq = ((uint64_t)dtls.epoch << 48U) | (seq_wrong_ct & 0xFFFFFFFFFFFFULL);
    for (int i = 0; i < 8; i++) {
        nonce[11 - i] ^= (uint8_t)((full_seq >> (i * 8)) & 0xFF);
    }
    wire.buf[0] = hdr_nolen[0];
    wire.buf[1] = hdr_nolen[1];
    pt_nolen[nolen_text_len] = 0x16U;
    syn_aes_gcm_init(&gcm, dtls.client_app_key, 16U);
    syn_aes_gcm_encrypt(&gcm, nonce, 12U, hdr_nolen, 2U, pt_nolen, nolen_text_len + 1U,
                        &wire.buf[2], &wire.buf[2 + nolen_text_len + 1U]);
    wire.len = 2U + (nolen_text_len + 1U) + 16U;
    TEST_ASSERT_FALSE(syn_dtls_recv(&dtls, recv_out, sizeof(recv_out), &out_len));

    /* Datagram with length field exceeding rx_len */
    wire.len = 30;
    wire.buf[0] = SYN_DTLS_UNIFIED_FIXED_BIT | SYN_DTLS_UNIFIED_LEN_BIT;
    wire.buf[1] = 0x01;
    wire.buf[2] = 0x01;
    wire.buf[3] = 0x00; /* claims 256 bytes > rx_len 30 */
    TEST_ASSERT_FALSE(syn_dtls_recv(&dtls, recv_out, sizeof(recv_out), &out_len));

    /* Datagram with ct_tag_len < tag_sz + 1 */
    wire.len = 30;
    wire.buf[0] = SYN_DTLS_UNIFIED_FIXED_BIT | SYN_DTLS_UNIFIED_LEN_BIT;
    wire.buf[1] = 0x01;
    wire.buf[2] = 0x00;
    wire.buf[3] = 0x05; /* ct_tag_len = 5 < 17 */
    TEST_ASSERT_FALSE(syn_dtls_recv(&dtls, recv_out, sizeof(recv_out), &out_len));
}

void run_dtls_tests(void)
{
    RUN_TEST(test_dtls_sliding_window_anti_replay);
    RUN_TEST(test_dtls_psk_mode_all_cipher_suites);
    RUN_TEST(test_dtls_seq_16bit_and_anti_replay_rejection);
    RUN_TEST(test_dtls_transport_binding_and_task);
    RUN_TEST(test_dtls_null_and_bounds_checks);
}
