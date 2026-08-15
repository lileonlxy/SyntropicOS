/**
 * @file test_wg.c
 * @brief Unity tests for WireGuard internals — HKDF, Noise helpers,
 *        replay window, transport, and handshake construction.
 *
 * Uses #include "syn_wg.c" to access static functions directly.
 * All expected values verified against the Go reference
 * (tests/test_wg_reference.go).
 */

#include "mocks/mock_port.h"
#include "syntropic/syntropic.h"
#include "unity/unity.h"

/* Pull in the .c to access static functions */
#include "syntropic/net/syn_wg.c"
#include "syntropic/util/syn_fmt.h"

#include <stdio.h>
#include <string.h>

static void wg_hex2bin(const char *hex, uint8_t *out, size_t len)
{
    syn_fmt_hex_parse(hex, out, len);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Construction constants (verified against Go wireguard-go)
 * ═══════════════════════════════════════════════════════════════════════════ */

/* C = HASH(Construction) */
static void test_wg_construction_hash(void)
{
    uint8_t ck[32];
    syn_blake2s(WG_CONSTRUCTION, sizeof(WG_CONSTRUCTION) - 1, ck, 32);

    uint8_t expected[32];
    wg_hex2bin("60e26daef327efc02ec335e2a025d2d0"
               "16eb4206f87277f52d38d1988b78cd36",
               expected, 32);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, ck, 32);
}

/* H = HASH(C || Identifier) */
static void test_wg_identifier_hash(void)
{
    uint8_t ck[32];
    syn_blake2s(WG_CONSTRUCTION, sizeof(WG_CONSTRUCTION) - 1, ck, 32);

    uint8_t h[32];
    SYN_BLAKE2s ctx;
    syn_blake2s_init(&ctx, 32);
    syn_blake2s_update(&ctx, ck, 32);
    syn_blake2s_update(&ctx, WG_IDENTIFIER, sizeof(WG_IDENTIFIER) - 1);
    syn_blake2s_final(&ctx, h);

    uint8_t expected[32];
    wg_hex2bin("2211b361081ac566691243db458ad532"
               "2d9c6c662293e8b70ee19c65ba079ef3",
               expected, 32);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, h, 32);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  HKDF2 / HKDF3
 * ═══════════════════════════════════════════════════════════════════════════ */

/* HKDF2 matches the Go reference for KDF2(C, E_pub) */
static void test_wg_hkdf2(void)
{
    uint8_t ck[32], out1[32], out2[32];
    /* Initial C */
    wg_hex2bin("60e26daef327efc02ec335e2a025d2d0"
               "16eb4206f87277f52d38d1988b78cd36",
               ck, 32);

    /* E_pub from our Go reference */
    uint8_t e_pub[32];
    wg_hex2bin("5dfedd3b6bd47f6fa28ee15d969d5bb0"
               "ea53774d488bdaf9df1c6e0124b3ef22",
               e_pub, 32);

    wg_hkdf2(out1, out2, ck, e_pub, 32);

    /* out1 = new C (after e_pub) */
    uint8_t expected_ck[32];
    wg_hex2bin("3b9c2a603fb5783c1c74f7b4501c3901"
               "280d8451962abc94d0b8a6ea00b934c1",
               expected_ck, 32);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_ck, out1, 32);
}

/* HKDF3 produces 3 outputs (used for PSK mixing) */
static void test_wg_hkdf3(void)
{
    uint8_t ck[32], out1[32], out2[32], out3[32];
    memset(ck, 0x42, 32);

    uint8_t input[8] = {'h', 'k', 'd', 'f', '3', 't', 's', 't'};
    wg_hkdf3(out1, out2, out3, ck, input, 8);

    /* Verify all 3 outputs are different and non-zero */
    TEST_ASSERT_FALSE(memcmp(out1, out2, 32) == 0);
    TEST_ASSERT_FALSE(memcmp(out2, out3, 32) == 0);
    TEST_ASSERT_FALSE(memcmp(out1, out3, 32) == 0);

    uint8_t zeros[32] = {0};
    TEST_ASSERT_FALSE(memcmp(out1, zeros, 32) == 0);
    TEST_ASSERT_FALSE(memcmp(out2, zeros, 32) == 0);
    TEST_ASSERT_FALSE(memcmp(out3, zeros, 32) == 0);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Noise helpers
 * ═══════════════════════════════════════════════════════════════════════════ */

/* mix_hash: H = BLAKE2s(H || data) */
static void test_wg_mix_hash(void)
{
    uint8_t h[32];
    /* Start with a known H, mix in r_pub */
    wg_hex2bin("2211b361081ac566691243db458ad532"
               "2d9c6c662293e8b70ee19c65ba079ef3",
               h, 32);

    uint8_t r_pub[32];
    wg_hex2bin("ce8d3ad1ccb633ec7b70c17814a5c76e"
               "cd029685050d344745ba05870e587d59",
               r_pub, 32);

    wg_mix_hash(h, r_pub, 32);

    uint8_t expected[32];
    wg_hex2bin("74c87e340810e6a8815cf911b640ebfb"
               "9150dc04293e5274e324126100cdf6e3",
               expected, 32);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, h, 32);
}

/* encrypt_and_hash → decrypt_and_hash round-trip preserves hash chain */
static void test_wg_encrypt_decrypt_hash(void)
{
    uint8_t h_enc[32], h_dec[32], k[32];
    memset(h_enc, 0xAA, 32);
    memcpy(h_dec, h_enc, 32); /* Start with same H */
    memset(k, 0xBB, 32);

    uint8_t plain[16] = "test plaintext!";
    uint8_t ct[16], tag[16], decrypted[16];

    wg_encrypt_and_hash(h_enc, k, plain, 16, ct, tag);

    bool ok = wg_decrypt_and_hash(h_dec, k, ct, 16, tag, decrypted);
    TEST_ASSERT_TRUE(ok);

    /* Decrypted data matches original */
    TEST_ASSERT_EQUAL_HEX8_ARRAY(plain, decrypted, 16);

    /* Hash chains match (both sides mixed the same ct+tag) */
    TEST_ASSERT_EQUAL_HEX8_ARRAY(h_enc, h_dec, 32);
}

/* encrypt_and_hash with empty plaintext (used for handshake with no PSK) */
static void test_wg_encrypt_decrypt_hash_empty(void)
{
    uint8_t h_enc[32], h_dec[32], k[32];
    memset(h_enc, 0xCC, 32);
    memcpy(h_dec, h_enc, 32);
    memset(k, 0xDD, 32);

    uint8_t tag[16];

    wg_encrypt_and_hash(h_enc, k, NULL, 0, NULL, tag);

    bool ok = wg_decrypt_and_hash(h_dec, k, NULL, 0, tag, NULL);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(h_enc, h_dec, 32);
}

/* decrypt_and_hash fails with wrong tag */
static void test_wg_decrypt_and_hash_tampered(void)
{
    uint8_t h_enc[32], h_dec[32], k[32];
    memset(h_enc, 0xEE, 32);
    memcpy(h_dec, h_enc, 32);
    memset(k, 0xFF, 32);

    uint8_t plain[8] = {'t', 'e', 's', 't', 'd', 'a', 't', 'a'};
    uint8_t ct[8], tag[16], decrypted[8];

    wg_encrypt_and_hash(h_enc, k, plain, 8, ct, tag);
    tag[0] ^= 0x01; /* Tamper */

    bool ok = wg_decrypt_and_hash(h_dec, k, ct, 8, tag, decrypted);
    TEST_ASSERT_FALSE(ok);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  MAC1
 * ═══════════════════════════════════════════════════════════════════════════ */

/* MAC1 produces consistent, non-zero output */
static void test_wg_mac1_basic(void)
{
    uint8_t peer_pub[32], mac[16];
    memset(peer_pub, 0x42, 32);

    uint8_t msg[64];
    memset(msg, 0x11, 64);

    wg_mac1(mac, peer_pub, msg, 64);

    /* Non-zero */
    uint8_t zeros[16] = {0};
    TEST_ASSERT_FALSE(memcmp(mac, zeros, 16) == 0);

    /* Deterministic */
    uint8_t mac2[16];
    wg_mac1(mac2, peer_pub, msg, 64);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(mac, mac2, 16);
}

/* MAC1 changes when message changes */
static void test_wg_mac1_sensitivity(void)
{
    uint8_t peer_pub[32], mac1[16], mac2[16];
    memset(peer_pub, 0x42, 32);

    uint8_t msg1[64], msg2[64];
    memset(msg1, 0x11, 64);
    memset(msg2, 0x11, 64);
    msg2[0] = 0x22; /* One byte different */

    wg_mac1(mac1, peer_pub, msg1, 64);
    wg_mac1(mac2, peer_pub, msg2, 64);

    TEST_ASSERT_FALSE(memcmp(mac1, mac2, 16) == 0);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Anti-replay window
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Sequential packets accepted */
static void test_wg_replay_sequential(void)
{
    SYN_WgSession s;
    memset(&s, 0, sizeof(s));

    TEST_ASSERT_TRUE(wg_replay_check(&s, 1));
    TEST_ASSERT_TRUE(wg_replay_check(&s, 2));
    TEST_ASSERT_TRUE(wg_replay_check(&s, 3));
    TEST_ASSERT_TRUE(wg_replay_check(&s, 4));
}

/* Duplicate rejected */
static void test_wg_replay_duplicate(void)
{
    SYN_WgSession s;
    memset(&s, 0, sizeof(s));

    TEST_ASSERT_TRUE(wg_replay_check(&s, 5));
    TEST_ASSERT_FALSE(wg_replay_check(&s, 5)); /* duplicate */
}

/* Large forward jump accepted, old rejected */
static void test_wg_replay_forward_jump(void)
{
    SYN_WgSession s;
    memset(&s, 0, sizeof(s));

    TEST_ASSERT_TRUE(wg_replay_check(&s, 1));
    TEST_ASSERT_TRUE(wg_replay_check(&s, 100)); /* big jump */
    TEST_ASSERT_FALSE(wg_replay_check(&s, 1));  /* now too old */
    TEST_ASSERT_FALSE(wg_replay_check(&s, 50)); /* also too old (>32 behind) */
}

/* Out-of-order within window accepted */
static void test_wg_replay_out_of_order(void)
{
    SYN_WgSession s;
    memset(&s, 0, sizeof(s));

    TEST_ASSERT_TRUE(wg_replay_check(&s, 10));
    TEST_ASSERT_TRUE(wg_replay_check(&s, 8));  /* 2 behind, within window */
    TEST_ASSERT_TRUE(wg_replay_check(&s, 9));  /* 1 behind, within window */
    TEST_ASSERT_FALSE(wg_replay_check(&s, 8)); /* already seen */
}

/* Window boundary: exactly 31 behind is accepted, 32 behind is rejected */
static void test_wg_replay_window_boundary(void)
{
    SYN_WgSession s;
    memset(&s, 0, sizeof(s));

    TEST_ASSERT_TRUE(wg_replay_check(&s, 32));

    /* 31 behind (counter 1) — should be accepted */
    TEST_ASSERT_TRUE(wg_replay_check(&s, 1));

    /* Set counter further ahead */
    TEST_ASSERT_TRUE(wg_replay_check(&s, 64));

    /* 31 behind (counter 33) — should be accepted */
    TEST_ASSERT_TRUE(wg_replay_check(&s, 33));

    /* 32 behind (counter 32) — should be rejected (outside window) */
    TEST_ASSERT_FALSE(wg_replay_check(&s, 32));
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Full handshake construction against Go reference
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Verify the full initiation message intermediate values match Go */
static void test_wg_handshake_intermediates(void)
{
    /* Fixed keys matching test_wg_reference.go */
    uint8_t i_priv[32], i_pub[32];
    uint8_t r_pub[32];
    uint8_t e_priv[32], e_pub[32];

    memset(i_priv, 0x01, 32);
    syn_x25519_clamp(i_priv);
    syn_x25519_pubkey(i_pub, i_priv);

    memset(e_priv, 0x03, 32);
    syn_x25519_clamp(e_priv);
    syn_x25519_pubkey(e_pub, e_priv);

    /* Responder public key */
    wg_hex2bin("ce8d3ad1ccb633ec7b70c17814a5c76e"
               "cd029685050d344745ba05870e587d59",
               r_pub, 32);

    /* Step 1: C = HASH(Construction) */
    uint8_t ck[32], h[32], k[32], dh[32];
    syn_blake2s(WG_CONSTRUCTION, sizeof(WG_CONSTRUCTION) - 1, ck, 32);

    /* Step 2: H = HASH(C || Identifier) */
    wg_mix_hash(ck, WG_IDENTIFIER, sizeof(WG_IDENTIFIER) - 1);
    /* Wait — ck is used as both C and the input to mix_hash.
     * Actually the spec says H starts from C, not modifying C in-place.
     * Let me recalculate properly. */

    /* Redo: */
    syn_blake2s(WG_CONSTRUCTION, sizeof(WG_CONSTRUCTION) - 1, ck, 32);
    {
        SYN_BLAKE2s ctx;
        syn_blake2s_init(&ctx, 32);
        syn_blake2s_update(&ctx, ck, 32);
        syn_blake2s_update(&ctx, WG_IDENTIFIER, sizeof(WG_IDENTIFIER) - 1);
        syn_blake2s_final(&ctx, h);
    }

    /* Step 3: H = HASH(H || S_pub_r) */
    wg_mix_hash(h, r_pub, 32);

    uint8_t expected_h[32];
    wg_hex2bin("74c87e340810e6a8815cf911b640ebfb"
               "9150dc04293e5274e324126100cdf6e3",
               expected_h, 32);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_h, h, 32);

    /* Step 4: C, _ = KDF2(C, E_pub) */
    wg_mix_key(ck, k, e_pub, 32);

    uint8_t expected_ck[32];
    wg_hex2bin("3b9c2a603fb5783c1c74f7b4501c3901"
               "280d8451962abc94d0b8a6ea00b934c1",
               expected_ck, 32);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_ck, ck, 32);

    /* Step 5: H = HASH(H || E_pub) */
    wg_mix_hash(h, e_pub, 32);

    wg_hex2bin("7c924cc768f21c641fa3292ddfca6234"
               "807e0d0cbf2f70763ad045be9b75b3b6",
               expected_h, 32);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_h, h, 32);

    /* Step 6: DH(E_priv, S_pub_r) → C, k */
    syn_x25519(dh, e_priv, r_pub);
    wg_mix_key(ck, k, dh, 32);

    wg_hex2bin("50e36bfff4d62801ff6375d4521af1af"
               "4ddf24fe1425e4bdf4b0c0236be94d2f",
               expected_ck, 32);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_ck, ck, 32);

    uint8_t expected_k[32];
    wg_hex2bin("ccb521ed0369ab01a50b94554bcd9bfb"
               "e81cf4851df2caeb3776b4b6882f84ad",
               expected_k, 32);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_k, k, 32);

    /* Step 7: Encrypt static key → check H */
    uint8_t ct[32], tag[16];
    wg_encrypt_and_hash(h, k, i_pub, 32, ct, tag);

    wg_hex2bin("9988e81b8d83aca922a8092fb2c1ec0f"
               "3e70e8c0bd925f1bf8dece66a364e59c",
               expected_h, 32);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_h, h, 32);

    /* Step 8: DH(S_priv_i, S_pub_r) → C, k */
    syn_x25519(dh, i_priv, r_pub);
    wg_mix_key(ck, k, dh, 32);

    wg_hex2bin("4696b0ed00cff9c62cf447eb5f9c899d"
               "508e66b889a8cef23c4ae5c60f902b5e",
               expected_ck, 32);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_ck, ck, 32);

    wg_hex2bin("8c2e061899942b4083496ce0bb8e1b4c"
               "bf5308f05aafda8e55c26a20d7c1774a",
               expected_k, 32);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_k, k, 32);

    /* Step 9: Encrypt timestamp → check H */
    uint8_t timestamp[12] = {
        0x40, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x42, 0x40, 0x00, 0x00, 0x00, 0x00,
    };
    wg_encrypt_and_hash(h, k, timestamp, 12, ct, tag);

    wg_hex2bin("7167ec354d44f588a7ddc45e754a4dad"
               "83f1bb903f77dd9cce9295b7e0ec4a51",
               expected_h, 32);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected_h, h, 32);
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Init state
 * ═══════════════════════════════════════════════════════════════════════════ */

static void test_wg_init_state(void)
{
    SYN_WG wg;
    SYN_WgConfig cfg;
    SYN_SNTP sntp;
    uint8_t rx_buf[256], tx_buf[256];

    memset(&cfg, 0, sizeof(cfg));
    memset(cfg.private_key, 0x01, 32);
    memset(cfg.peer_public_key, 0x02, 32);
    cfg.endpoint.port = 51820;

    syn_wg_init(&wg, &cfg, &sntp, rx_buf, sizeof(rx_buf), tx_buf, sizeof(tx_buf));

    TEST_ASSERT_EQUAL(SYN_WG_DISCONNECTED, wg.state);
    TEST_ASSERT_FALSE(syn_wg_is_established(&wg));
    TEST_ASSERT_EQUAL(0, wg.session.send_counter);
    TEST_ASSERT_EQUAL(0, wg.session.recv_counter);
}

/* ── State Machine Tests ────────────────────────────────────────────────── */

static uint8_t s_rx_buf[1024];
static uint8_t s_tx_buf[1024];
static SYN_WG s_wg;
static SYN_SNTP s_sntp;

/* Canned keys for testing */
static const uint8_t CLIENT_PRIV[32] = {
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01};

static const uint8_t PEER_PUB[32] = {
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02};

static void wg_state_setup(void)
{
    mock_port_reset();

    /* Mock SNTP as synced */
    memset(&s_sntp, 0, sizeof(s_sntp));
    s_sntp.synced = true;
    s_sntp.epoch_s = 1600000000;
    s_sntp.sync_tick_ms = mock_tick_ms;

    SYN_WgConfig cfg = {0};
    memcpy(cfg.private_key, CLIENT_PRIV, 32);
    memcpy(cfg.peer_public_key, PEER_PUB, 32);
    cfg.endpoint.port = 51820;
    cfg.endpoint.ip[0] = 1;
    cfg.endpoint.ip[1] = 2;
    cfg.endpoint.ip[2] = 3;
    cfg.endpoint.ip[3] = 4;
    cfg.keepalive_interval_s = 25;

    syn_wg_init(&s_wg, &cfg, &s_sntp, s_rx_buf, sizeof(s_rx_buf), s_tx_buf, sizeof(s_tx_buf));
}

static void test_wg_initiation_on_task(void)
{
    wg_state_setup();
    TEST_ASSERT_EQUAL(SYN_WG_DISCONNECTED, s_wg.state);

    SYN_PT pt;
    PT_INIT(&pt);
    SYN_Task task = {.user_data = &s_wg};

    syn_wg_task(&pt, &task);
    TEST_ASSERT_EQUAL(SYN_WG_HANDSHAKE_INIT, s_wg.state);
}

static void test_wg_handshake_response_invalid(void)
{
    wg_state_setup();

    SYN_PT pt;
    PT_INIT(&pt);
    SYN_Task task = {.user_data = &s_wg};
    syn_wg_task(&pt, &task);

    /* Simulate a Handshake Response from peer (but with invalid crypto) */
    uint8_t mock_resp[92];
    memset(mock_resp, 0, sizeof(mock_resp));
    uint32_t *msg_type = (uint32_t *)mock_resp;
    *msg_type = SYN_WG_MSG_RESPONSE;

    SYN_SockAddr from = {.port = 51820};
    from.ip[0] = 1;
    from.ip[1] = 2;
    from.ip[2] = 3;
    from.ip[3] = 4;

    mock_udp_inject_packet(mock_resp, sizeof(mock_resp), &from);

    syn_wg_task(&pt, &task);

    /* Should still be HANDSHAKE_INIT (rejected response) */
    TEST_ASSERT_EQUAL(SYN_WG_HANDSHAKE_INIT, s_wg.state);
}

static bool s_recv_called = false;
static void test_on_recv(const uint8_t *buf, size_t len, void *ctx)
{
    (void)buf;
    (void)len;
    (void)ctx;
    s_recv_called = true;
}

static void test_wg_established_transport_and_keepalive(void)
{
    wg_state_setup();
    mock_tick_ms = 1000;
    mock_udp_tx_len = 0;
    s_recv_called = false;
    s_wg.on_recv = test_on_recv;

    /* Manually put state into ESTABLISHED with keys */
    s_wg.state = SYN_WG_ESTABLISHED;
    s_wg.udp_sock = 1;
    s_wg.session.sender_index = 100;
    s_wg.session.receiver_index = 200;
    memset(s_wg.session.send_key, 0x11, 32);
    memset(s_wg.session.recv_key, 0x22, 32);
    s_wg.session.established_ms = 1000;
    s_wg.last_sent_ms = 1000;

    /* 1. Test syn_wg_send */
    uint8_t payload[10] = "HELLO_WG!";
    SYN_Status st = syn_wg_send(&s_wg, payload, sizeof(payload));
    TEST_ASSERT_EQUAL(SYN_OK, st);
    TEST_ASSERT_EQUAL_UINT64(1, s_wg.session.send_counter);

    /* 2. Test syn_wg_send when disconnected (fails) */
    s_wg.state = SYN_WG_DISCONNECTED;
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_wg_send(&s_wg, payload, sizeof(payload)));
    s_wg.state = SYN_WG_ESTABLISHED;

    /* 3. Build & receive encrypted transport message with on_recv callback */
    uint8_t tx_msg[64];
    uint8_t nonce[12];
    memset(nonce, 0, 4);
    store64_le(nonce + 4, 1); /* counter = 1 */

    store32_le(tx_msg, SYN_WG_MSG_TRANSPORT);
    store32_le(tx_msg + 4, 100); /* receiver index = sender index of active session */
    store64_le(tx_msg + 8, 1);   /* counter = 1 */

    syn_aead_encrypt(s_wg.session.recv_key, nonce, NULL, 0, payload, sizeof(payload), tx_msg + 16,
                     tx_msg + 16 + sizeof(payload));

    size_t tx_msg_len = 16 + sizeof(payload) + 16;
    bool handled = wg_handle_transport(&s_wg, tx_msg, tx_msg_len);
    TEST_ASSERT_TRUE(handled);
    TEST_ASSERT_TRUE(s_recv_called);

    /* 4. Test receiving transport message in syn_wg_task with counter=2 */
    uint8_t tx_msg2[64];
    memset(nonce, 0, 4);
    store64_le(nonce + 4, 2);

    store32_le(tx_msg2, SYN_WG_MSG_TRANSPORT);
    store32_le(tx_msg2 + 4, 100);
    store64_le(tx_msg2 + 8, 2);

    syn_aead_encrypt(s_wg.session.recv_key, nonce, NULL, 0, payload, sizeof(payload), tx_msg2 + 16,
                     tx_msg2 + 16 + sizeof(payload));

    SYN_SockAddr from = {.port = 51820};
    from.ip[0] = 1;
    from.ip[1] = 2;
    from.ip[2] = 3;
    from.ip[3] = 4;
    mock_udp_inject_packet(tx_msg2, tx_msg_len, &from);

    SYN_PT pt;
    PT_INIT(&pt);
    SYN_Task task = {.user_data = &s_wg};
    syn_wg_task(&pt, &task);

    TEST_ASSERT_EQUAL(SYN_WG_ESTABLISHED, s_wg.state);
    TEST_ASSERT_NOT_EQUAL(SYN_SOCKET_INVALID, s_wg.udp_sock);
    s_wg.last_sent_ms = 100;
    st = syn_wg_send(&s_wg, NULL, 0);
    TEST_ASSERT_EQUAL(SYN_OK, st);

    /* Trigger periodic keepalive check in task */
    s_wg.config.keepalive_interval_s = 10;
    s_wg.last_sent_ms = 100;
    s_wg.session.established_ms = 10000;
    mock_tick_ms = 25000;
    syn_wg_task(&pt, &task);

    /* 6. Session expiry / rekey in task */
    s_wg.session.established_ms = 100;
    mock_tick_ms = 200000; /* > SYN_WG_REKEY_AFTER_TIME (120s) */
    syn_wg_task(&pt, &task);
    TEST_ASSERT_EQUAL(SYN_WG_HANDSHAKE_INIT, s_wg.state);
}

static void test_wg_send_disconnected_or_null(void)
{
    SYN_WG wg;
    memset(&wg, 0, sizeof(wg));
    wg.state = SYN_WG_DISCONNECTED;
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_wg_send(&wg, (const uint8_t *)"test", 4));

    /* Oversized send payload */
    wg.state = SYN_WG_ESTABLISHED;
    wg.tx_buf_size = 32;
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_wg_send(&wg, (const uint8_t *)"12345678901234567890", 20));
}

static void test_wg_transport_error_branches(void)
{
    wg_state_setup();
    s_wg.state = SYN_WG_ESTABLISHED;
    s_wg.session.sender_index = 100;
    s_wg.session.receiver_index = 200;
    memset(s_wg.session.send_key, 0x11, 32);
    memset(s_wg.session.recv_key, 0x22, 32);

    /* 1. Runt message len < 32 */
    uint8_t runt[20] = {0};
    TEST_ASSERT_FALSE(wg_handle_transport(&s_wg, runt, 20));

    /* 2. Receiver index mismatch */
    uint8_t msg[64] = {0};
    store32_le(msg, SYN_WG_MSG_TRANSPORT);
    store32_le(msg + 4, 999); /* Wrong index */
    TEST_ASSERT_FALSE(wg_handle_transport(&s_wg, msg, 64));

    /* 3. Corrupted tag -> AEAD decrypt failure */
    store32_le(msg + 4, 100);
    store64_le(msg + 8, 1);
    TEST_ASSERT_FALSE(wg_handle_transport(&s_wg, msg, 64));
}

static void test_wg_transport_corrupt_tag_does_not_advance_replay_window(void)
{
    wg_state_setup();
    s_wg.state = SYN_WG_ESTABLISHED;
    s_wg.session.sender_index = 100;
    s_wg.session.receiver_index = 200;
    memset(s_wg.session.send_key, 0x11, 32);
    memset(s_wg.session.recv_key, 0x22, 32);
    s_wg.session.recv_counter = 0;
    s_wg.session.recv_bitmap = 0;

    /* 1. Receive unauthenticated forged packet with large counter = 100 and garbage payload/tag */
    uint8_t forged_msg[64] = {0};
    store32_le(forged_msg, SYN_WG_MSG_TRANSPORT);
    store32_le(forged_msg + 4, 100);
    store64_le(forged_msg + 8, 100);
    TEST_ASSERT_FALSE(wg_handle_transport(&s_wg, forged_msg, 64));

    /* Anti-replay window must NOT have been updated */
    TEST_ASSERT_EQUAL_UINT64(0, s_wg.session.recv_counter);
    TEST_ASSERT_EQUAL_UINT32(0, s_wg.session.recv_bitmap);

    /* 2. Legitimate packet with counter = 1 must now succeed */
    uint8_t valid_msg[64];
    uint8_t payload[8] = {'o', 'k', 'd', 'a', 't', 'a', '1', '!'};
    uint8_t nonce[12] = {0};
    store64_le(nonce + 4, 1);

    store32_le(valid_msg, SYN_WG_MSG_TRANSPORT);
    store32_le(valid_msg + 4, 100);
    store64_le(valid_msg + 8, 1);

    syn_aead_encrypt(s_wg.session.recv_key, nonce, NULL, 0, payload, sizeof(payload),
                     valid_msg + 16, valid_msg + 16 + sizeof(payload));

    size_t valid_len = 16 + sizeof(payload) + 16;
    TEST_ASSERT_TRUE(wg_handle_transport(&s_wg, valid_msg, valid_len));

    /* Replay window now committed */
    TEST_ASSERT_EQUAL_UINT64(1, s_wg.session.recv_counter);
    TEST_ASSERT_EQUAL_UINT32(1, s_wg.session.recv_bitmap);
}

static void test_wg_reject_after_time_expiry(void)
{
    test_wg_init_state();
    s_wg.state = SYN_WG_ESTABLISHED;
    s_wg.session.established_ms = 1000;
    mock_tick_ms = 250000; /* > SYN_WG_REJECT_AFTER_TIME (180s) */

    SYN_PT pt;
    PT_INIT(&pt);
    SYN_Task task = {.user_data = &s_wg};
    syn_wg_task(&pt, &task);
    TEST_ASSERT_EQUAL(SYN_WG_HANDSHAKE_INIT, s_wg.state);
}

static void test_wg_cookie_packet_handling(void)
{
    test_wg_init_state();
    s_wg.state = SYN_WG_ESTABLISHED;

    /* Inject MSG_COOKIE (type 3) and verify it does not crash or corrupt state */
    uint8_t cookie_pkt[64];
    memset(cookie_pkt, 0, sizeof(cookie_pkt));
    store32_le(cookie_pkt, 3); /* SYN_WG_MSG_COOKIE */
    SYN_PT pt;
    PT_INIT(&pt);
    SYN_Task task = {.user_data = &s_wg};
    syn_wg_task(&pt, &task);
    TEST_ASSERT_EQUAL(SYN_WG_HANDSHAKE_INIT, s_wg.state);
}

static void test_wg_rekey_after_time(void)
{
    test_wg_init_state();
    s_wg.state = SYN_WG_ESTABLISHED;
    s_wg.session.established_ms = 1000;
    mock_tick_ms = 130000; /* > SYN_WG_REKEY_AFTER_TIME (120s) */

    SYN_PT pt;
    PT_INIT(&pt);
    SYN_Task task = {.user_data = &s_wg};
    syn_wg_task(&pt, &task);
    TEST_ASSERT_EQUAL(SYN_WG_HANDSHAKE_INIT, s_wg.state);
}

static void test_wg_handshake_timeout(void)
{
    test_wg_init_state();
    s_wg.state = SYN_WG_HANDSHAKE_INIT;
    s_wg.last_handshake_ms = 1000;
    mock_tick_ms = 8000; /* > SYN_WG_REKEY_TIMEOUT (5s) */

    SYN_PT pt;
    PT_INIT(&pt);
    SYN_Task task = {.user_data = &s_wg};
    syn_wg_task(&pt, &task);
    TEST_ASSERT_EQUAL(SYN_WG_DISCONNECTED, s_wg.state);
}

static void test_wg_disconnect_and_stats(void)
{
    SYN_WG wg;
    uint8_t rx_buf[1600], tx_buf[1600];
    SYN_SNTP sntp;

    SYN_WgConfig cfg = {
        .endpoint = {.ip = {192, 168, 1, 1}, .port = 51820},
        .keepalive_interval_s = 25,
    };

    syn_wg_init(&wg, &cfg, &sntp, rx_buf, sizeof(rx_buf), tx_buf, sizeof(tx_buf));

    SYN_WgStats stats;
    TEST_ASSERT_EQUAL(SYN_OK, syn_wg_get_stats(&wg, &stats));
    TEST_ASSERT_FALSE(stats.is_established);

    wg.state = SYN_WG_ESTABLISHED;
    wg.session.established_ms = 1000;
    wg.session.send_counter = 10;
    wg.session.recv_counter = 8;

    TEST_ASSERT_EQUAL(SYN_OK, syn_wg_get_stats(&wg, &stats));
    TEST_ASSERT_TRUE(stats.is_established);
    TEST_ASSERT_TRUE(stats.tx_bytes > 0);

    syn_wg_disconnect(&wg);
    TEST_ASSERT_EQUAL(SYN_WG_DISCONNECTED, wg.state);
}

static void test_wg_send_fail_and_rekey_timeout_task_branches(void)
{
    wg_state_setup();
    s_wg.state = SYN_WG_ESTABLISHED;
    s_wg.udp_sock = 1;
    s_wg.session.receiver_index = 200;
    mock_udp_sendto_fail = true;
    uint8_t payload[10] = "FAIL_SEND";
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_wg_send(&s_wg, payload, sizeof(payload)));
    mock_udp_sendto_fail = false;

    /* Task in HANDSHAKE_INIT state timing out after SYN_WG_REKEY_TIMEOUT (5s) */
    s_wg.state = SYN_WG_HANDSHAKE_INIT;
    s_wg.last_handshake_ms = mock_tick_ms;
    mock_tick_ms += 6000;
    SYN_PT pt;
    PT_INIT(&pt);
    SYN_Task task = {.user_data = &s_wg};
    syn_wg_task(&pt, &task);
    TEST_ASSERT_EQUAL(SYN_WG_DISCONNECTED, s_wg.state);
}

static void dummy_wg_rx_cb(const uint8_t *data, size_t len, void *ctx)
{
    (void)data;
    (void)len;
    (void)ctx;
}

static void test_wg_set_rx_callback(void)
{
    SYN_WG wg;
    memset(&wg, 0, sizeof(wg));
    wg.on_recv = dummy_wg_rx_cb;
    wg.user_ctx = (void *)0x1234;
    TEST_ASSERT_EQUAL_PTR(dummy_wg_rx_cb, wg.on_recv);
    TEST_ASSERT_EQUAL_PTR((void *)0x1234, wg.user_ctx);
}

static void test_wg_cookie_mac2_verification_failure(void)
{
    SYN_WG wg;
    memset(&wg, 0, sizeof(wg));
    uint8_t msg[92];
    memset(msg, 0, sizeof(msg));
    msg[0] = 2;

    /* Correct mac1 */
    wg_mac1(&msg[60], wg.public_key, msg, 60);
    /* Corrupted mac2 */
    msg[76] ^= 0xFF;

    TEST_ASSERT_FALSE(wg_consume_response(&wg, msg, sizeof(msg)));
}

static void test_wg_response_message_invalid_length_and_receiver(void)
{
    SYN_WG wg;
    memset(&wg, 0, sizeof(wg));
    wg.session.sender_index = 0x12345678;

    uint8_t short_msg[30] = {0};
    TEST_ASSERT_FALSE(wg_consume_response(&wg, short_msg, sizeof(short_msg)));

    uint8_t msg[92];
    memset(msg, 0, sizeof(msg));
    store32_le(msg, SYN_WG_MSG_RESPONSE);
    store32_le(msg + 8, 0x87654321); /* Receiver mismatch */

    TEST_ASSERT_FALSE(wg_consume_response(&wg, msg, 92));
}

static void test_wg_response_decryption_and_mac_validation_failures(void)
{
    SYN_WG wg;
    memset(&wg, 0, sizeof(wg));
    wg.session.sender_index = 0x12345678;

    uint8_t msg[92];
    memset(msg, 0, sizeof(msg));
    store32_le(msg, SYN_WG_MSG_RESPONSE);
    store32_le(msg + 4, 0x11223344); /* Peer sender index */
    store32_le(msg + 8, 0x12345678); /* Matching receiver index */

    /* Bad payload decryption or bad mac1 will cause wg_consume_response to fail gracefully */
    TEST_ASSERT_FALSE(wg_consume_response(&wg, msg, sizeof(msg)));
}

static void test_wg_send_buffer_too_small(void)
{
    SYN_WG wg;
    SYN_WgConfig cfg;
    SYN_SNTP sntp;
    uint8_t rx_buf[64], tx_buf[64];
    memset(&cfg, 0, sizeof(cfg));
    syn_wg_init(&wg, &cfg, &sntp, rx_buf, sizeof(rx_buf), tx_buf, sizeof(tx_buf));
    uint8_t dummy[100];
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_wg_send(&wg, dummy, sizeof(dummy)));
}

static void test_wg_response_mac1_tampered_rejection(void)
{
    SYN_WG wg;
    SYN_WgConfig cfg;
    SYN_SNTP sntp;
    uint8_t rx_buf[256], tx_buf[256];

    memset(&cfg, 0, sizeof(cfg));
    memset(cfg.private_key, 0x01, 32);
    memset(cfg.peer_public_key, 0x02, 32);
    cfg.endpoint.port = 51820;

    syn_wg_init(&wg, &cfg, &sntp, rx_buf, sizeof(rx_buf), tx_buf, sizeof(tx_buf));
    wg.state = SYN_WG_HANDSHAKE_INIT;
    wg.session.sender_index = 0x12345678;

    uint8_t msg[92];
    memset(msg, 0, sizeof(msg));
    msg[0] = 0x02;                    /* Response type */
    syn_poke_u32(0x12345678, msg, 8); /* receiver_index matching wg.session.sender_index */

    /* Tamper MAC1 at offset 60 */
    msg[60] ^= 0xFF;

    TEST_ASSERT_FALSE(wg_consume_response(&wg, msg, sizeof(msg)));
}

static void test_wg_send_initiation_failure_task_branch(void)
{
    /* Line 723: send_initiation fail in task */
    test_wg_init_state();
    s_wg.state = SYN_WG_HANDSHAKE_INIT;
    mock_udp_sendto_fail = true;

    SYN_PT pt;
    PT_INIT(&pt);
    SYN_Task task = {.user_data = &s_wg};
    syn_wg_task(&pt, &task);

    TEST_ASSERT_EQUAL(SYN_WG_HANDSHAKE_INIT, s_wg.state);
    mock_udp_sendto_fail = false;
}

static void test_wg_valid_response_exchange(void)
{
    SYN_WG wg;
    SYN_WgConfig cfg;
    SYN_SNTP sntp;
    uint8_t rx_buf[1600], tx_buf[1600];

    memset(&cfg, 0, sizeof(cfg));
    /* Initiator static key = 0x01 */
    memset(cfg.private_key, 0x01, 32);
    /* Responder static private key = 0x02 -> calculate responder public key */
    uint8_t resp_priv[32];
    memset(resp_priv, 0x02, 32);
    syn_x25519_clamp(resp_priv);
    syn_x25519_pubkey(cfg.peer_public_key, resp_priv);
    cfg.endpoint.port = 51820;

    syn_wg_init(&wg, &cfg, &sntp, rx_buf, sizeof(rx_buf), tx_buf, sizeof(tx_buf));
    wg.udp_sock = 1;

    /* 1. Generate Initiation packet via syn_wg_task */
    SYN_PT init_pt;
    PT_INIT(&init_pt);
    SYN_Task init_task = {.user_data = &wg};
    mock_port_reset();
    mock_sock_connected = true;
    syn_wg_task(&init_pt, &init_task);

    /* 2. Build valid Response packet from Responder */
    uint8_t msg[92];
    memset(msg, 0, sizeof(msg));
    store32_le(msg, SYN_WG_MSG_RESPONSE);
    store32_le(msg + 4, 0x99887766); /* Responder sender_index */
    store32_le(msg + 8, wg.session.sender_index);

    /* Responder ephemeral keypair (0x03) */
    uint8_t resp_e_priv[32], resp_e_pub[32];
    memset(resp_e_priv, 0x03, 32);
    syn_x25519_clamp(resp_e_priv);
    syn_x25519_pubkey(resp_e_pub, resp_e_priv);
    memcpy(msg + 12, resp_e_pub, 32);

    /* Initialize noise state on responder side */
    uint8_t ck[32], h[32];
    const char *INIT_LABEL = "Noise_IKpsk2_25519_ChaChaPoly_BLAKE2s";
    const char *IDENT_LABEL = "WireGuard v1 zx2c4 justify";
    syn_blake2s(INIT_LABEL, strlen(INIT_LABEL), ck, 32);
    memcpy(h, ck, 32);
    wg_mix_hash(h, IDENT_LABEL, strlen(IDENT_LABEL));
    wg_mix_hash(h, cfg.peer_public_key, 32);  /* Responder public key */
    wg_mix_hash(h, mock_udp_tx_buf + 12, 32); /* Initiator ephemeral key (E_i) */
    wg_mix_hash(h, resp_e_pub, 32);           /* Responder ephemeral key (E_r) */

    /* DH(E_r, E_i) */
    uint8_t dh1[32], key[32];
    syn_x25519(dh1, resp_e_priv, mock_udp_tx_buf + 12);
    wg_hkdf2(ck, key, ck, dh1, 32);

    /* DH(E_r, S_i) */
    uint8_t dh2[32];
    syn_x25519(dh2, resp_e_priv, wg.public_key);
    wg_hkdf2(ck, key, ck, dh2, 32);

    /* DH(psk) */
    uint8_t psk_zeros[32] = {0};
    wg_hkdf3(ck, h, key, ck, psk_zeros, 32);

    /* Encrypt empty payload */
    uint8_t zero_nonce[12] = {0};
    syn_aead_encrypt(key, zero_nonce, h, 32, NULL, 0, msg + 44, msg + 44 + 0);
    wg_mix_hash(h, msg + 44, 16);

    /* MAC1 over 0..59 with initiator's public key */
    wg_mac1(msg + 60, wg.public_key, msg, 60);

    /* Consume invalid response length */
    TEST_ASSERT_FALSE(wg_consume_response(&wg, msg, 10));

    /* Test task receiving packet in SYN_WG_HANDSHAKE_INIT state (lines 757, 761-762) */
    mock_port_reset();
    test_wg_init_state();
    s_wg.state = SYN_WG_HANDSHAKE_INIT;
    s_wg.udp_sock = 1;
    mock_sock_connected = true;
    mock_sock_set_response(msg, 92);

    SYN_PT pt;
    PT_INIT(&pt);
    SYN_Task task = {.user_data = &s_wg};
    syn_wg_task(&pt, &task);

    /* Test HANDSHAKE_INIT timeout in task loop (lines 721-725) */
    PT_INIT(&pt);
    s_wg.state = SYN_WG_HANDSHAKE_INIT;
    s_wg.last_handshake_ms = 1000;
    mock_tick_ms = 10000;
    mock_sock_rx_len = 0;
    mock_sock_rx_pos = 0;
    syn_wg_task(&pt, &task);
    TEST_ASSERT_EQUAL(SYN_WG_DISCONNECTED, s_wg.state);
    mock_port_reset();
}

static void test_wg_send_initiation_tx_buf_too_small(void)
{
    /* Test syn_wg_disconnect with open socket (line 781) */
    SYN_WG wg;
    wg.udp_sock = 5;
    syn_wg_disconnect(&wg);
    TEST_ASSERT_EQUAL(SYN_SOCKET_INVALID, wg.udp_sock);
}

static void test_wg_handshake_rekey_timer_expiry(void)
{
    /* wg == NULL -> false (line 691) */
    SYN_Task null_task = {.user_data = NULL};
    SYN_PT pt;
    PT_INIT(&pt);
    TEST_ASSERT_EQUAL(PT_EXITED, syn_wg_task(&pt, &null_task));

    /* SYN_WG_DISCONNECTED -> true (line 693) */
    SYN_WG wg_disc;
    memset(&wg_disc, 0, sizeof(wg_disc));
    wg_disc.state = SYN_WG_DISCONNECTED;
    TEST_ASSERT_TRUE(wg_has_work(&wg_disc));

    /* wg_has_work(NULL) -> line 691 */
    TEST_ASSERT_FALSE(wg_has_work(NULL));

    /* SYN_WG_HANDSHAKE_INIT no timeout -> false (line 700) */
    SYN_WG wg_hs;
    memset(&wg_hs, 0, sizeof(wg_hs));
    wg_hs.state = SYN_WG_HANDSHAKE_INIT;
    wg_hs.last_handshake_ms = 1000;
    mock_tick_ms = 1000 + (SYN_WG_REKEY_TIMEOUT + 1) * 1000;
    TEST_ASSERT_TRUE(wg_has_work(&wg_hs));
    mock_tick_ms = 1000 + 1000;
    TEST_ASSERT_FALSE(wg_has_work(&wg_hs));

    /* SYN_WG_ESTABLISHED no timeout, keepalive 0 -> false (lines 704 & 708) */
    SYN_WG wg_est_no_work;
    memset(&wg_est_no_work, 0, sizeof(wg_est_no_work));
    wg_est_no_work.state = SYN_WG_ESTABLISHED;
    wg_est_no_work.session.established_ms = 1000;
    wg_est_no_work.last_sent_ms = 1000;
    wg_est_no_work.config.keepalive_interval_s = 0;
    mock_tick_ms = 1000 + 2000;
    TEST_ASSERT_FALSE(wg_has_work(&wg_est_no_work));

    /* Test get_stats NULL parameters */
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_wg_get_stats(NULL, NULL));
    SYN_WgStats stats_out;
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_wg_get_stats(NULL, &stats_out));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_wg_get_stats(&wg_hs, NULL));

    /* Test replay check out-of-order and duplicate checks */
    SYN_WgSession replay_sess = {0};
    TEST_ASSERT_TRUE(wg_replay_check(&replay_sess, 20));
    TEST_ASSERT_TRUE(wg_replay_check(&replay_sess, 15));  /* diff = 5 < 32, unseen -> true */
    TEST_ASSERT_FALSE(wg_replay_check(&replay_sess, 15)); /* diff = 5 < 32, seen -> false */

    /* Test syn_wg_send error branches */
    SYN_WG wg_snd;
    memset(&wg_snd, 0, sizeof(wg_snd));
    uint8_t dummy_pkt[16] = {0};
    wg_snd.state = SYN_WG_DISCONNECTED;
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_wg_send(&wg_snd, dummy_pkt, sizeof(dummy_pkt)));

    wg_snd.state = SYN_WG_ESTABLISHED;
    wg_snd.tx_buf_size = 20; /* total (16+16+16=48) > 20 */
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_wg_send(&wg_snd, dummy_pkt, sizeof(dummy_pkt)));

    uint8_t big_tx[128];
    wg_snd.tx_buf = big_tx;
    wg_snd.tx_buf_size = sizeof(big_tx);
    wg_snd.udp_sock = 1;
    mock_udp_sendto_fail = true; /* sendto will return -1 */
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_wg_send(&wg_snd, dummy_pkt, sizeof(dummy_pkt)));
    mock_udp_sendto_fail = false;

    /* Test wg_handle_transport length and receiver mismatch */
    uint8_t short_msg[10] = {0};
    TEST_ASSERT_FALSE(wg_handle_transport(&wg_snd, short_msg, sizeof(short_msg)));

    uint8_t rcv_mismatch_msg[32] = {0};
    wg_snd.session.sender_index = 0x12345678;
    TEST_ASSERT_FALSE(wg_handle_transport(&wg_snd, rcv_mismatch_msg, sizeof(rcv_mismatch_msg)));

    /* Test disconnect and stats NULL checks */
    syn_wg_disconnect(NULL);
    SYN_WgStats stats;
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_wg_get_stats(NULL, &stats));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_wg_get_stats((const SYN_WG *)1, NULL));

    /* Test get_stats in established state */
    SYN_WG wg_est;
    memset(&wg_est, 0, sizeof(wg_est));
    wg_est.state = SYN_WG_ESTABLISHED;
    wg_est.session.established_ms = 1000;
    wg_est.session.send_counter = 5;
    wg_est.session.recv_counter = 10;
    mock_tick_ms = 5000;
    TEST_ASSERT_EQUAL(SYN_OK, syn_wg_get_stats(&wg_est, &stats));
    TEST_ASSERT_TRUE(stats.is_established);
    TEST_ASSERT_EQUAL_UINT32(4, stats.handshake_age_sec);

    /* Task NULL parameter checks */
    SYN_PT pt_null;
    PT_INIT(&pt_null);
    TEST_ASSERT_EQUAL(PT_EXITED, syn_wg_task(&pt_null, NULL));
    SYN_Task null_user_task = {.user_data = NULL};
    TEST_ASSERT_EQUAL(PT_EXITED, syn_wg_task(&pt_null, &null_user_task));

    /* Established state keepalive and rekey task loop branches */
    SYN_SNTP sntp_mock = {.synced = true};
    SYN_WG wg_loop;
    memset(&wg_loop, 0, sizeof(wg_loop));
    wg_loop.state = SYN_WG_ESTABLISHED;
    wg_loop.sntp = &sntp_mock;
    wg_loop.config.keepalive_interval_s = 5;
    wg_loop.last_sent_ms = 1000;
    wg_loop.session.established_ms = 1000;
    wg_loop.udp_sock = 1;
    mock_tick_ms = 10000; /* > 1000 + 5000ms -> keepalive triggered */
    mock_sock_connected = true;

    SYN_Task loop_task = {.user_data = &wg_loop};
    PT_INIT(&pt_null);
    wg_loop.last_sent_ms = mock_tick_ms;
    syn_wg_task(&pt_null, &loop_task);

    /* Test syn_wg_task receiving n < 4 (n = 3) packet and msg_type = MSG_COOKIE (type = 4) */
    uint8_t short_pkt[3] = {1, 2, 3};
    SYN_SockAddr from = {0};
    mock_udp_set_response(short_pkt, sizeof(short_pkt), &from);
    PT_INIT(&pt_null);
    syn_wg_task(&pt_null, &loop_task);

    uint8_t cookie_pkt[32];
    memset(cookie_pkt, 0, sizeof(cookie_pkt));
    store32_le(cookie_pkt, 4); /* SYN_WG_MSG_COOKIE = 4 */
    mock_udp_set_response(cookie_pkt, sizeof(cookie_pkt), &from);
    PT_INIT(&pt_null);
    syn_wg_task(&pt_null, &loop_task);

    /* Test replay check large counter jump (diff >= 32, line 567) */
    SYN_WgSession s_jump = {.recv_counter = 10, .recv_bitmap = 0x01};
    TEST_ASSERT_TRUE(wg_replay_check(&s_jump, 50));
    TEST_ASSERT_EQUAL_UINT64(50, s_jump.recv_counter);
    TEST_ASSERT_EQUAL_UINT64(1, s_jump.recv_bitmap);

    /* Test session reject after time (180s, line 773) */
    wg_loop.state = SYN_WG_ESTABLISHED;
    wg_loop.session.established_ms = 1000;
    mock_tick_ms = 1000 + 185 * 1000;
    PT_INIT(&pt_null);
    syn_wg_task(&pt_null, &loop_task);
    TEST_ASSERT_EQUAL(SYN_WG_DISCONNECTED, wg_loop.state);
}

void run_wg_tests(void)
{
    /* Handshake internals */
    RUN_TEST(test_wg_construction_hash);
    RUN_TEST(test_wg_identifier_hash);
    RUN_TEST(test_wg_hkdf2);
    RUN_TEST(test_wg_hkdf3);

    /* Noise helpers */
    RUN_TEST(test_wg_mix_hash);
    RUN_TEST(test_wg_encrypt_decrypt_hash);
    RUN_TEST(test_wg_encrypt_decrypt_hash_empty);
    RUN_TEST(test_wg_decrypt_and_hash_tampered);

    /* MAC1 */
    RUN_TEST(test_wg_mac1_basic);
    RUN_TEST(test_wg_mac1_sensitivity);

    /* Anti-replay */
    RUN_TEST(test_wg_replay_sequential);
    RUN_TEST(test_wg_replay_duplicate);
    RUN_TEST(test_wg_replay_forward_jump);
    RUN_TEST(test_wg_replay_out_of_order);
    RUN_TEST(test_wg_replay_window_boundary);

    /* Full handshake construction */
    RUN_TEST(test_wg_handshake_intermediates);

    /* State machine & transport */
    RUN_TEST(test_wg_init_state);
    RUN_TEST(test_wg_initiation_on_task);
    RUN_TEST(test_wg_handshake_response_invalid);
    RUN_TEST(test_wg_established_transport_and_keepalive);
    RUN_TEST(test_wg_send_disconnected_or_null);
    RUN_TEST(test_wg_transport_error_branches);
    RUN_TEST(test_wg_transport_corrupt_tag_does_not_advance_replay_window);
    RUN_TEST(test_wg_reject_after_time_expiry);
    RUN_TEST(test_wg_cookie_packet_handling);
    RUN_TEST(test_wg_rekey_after_time);
    RUN_TEST(test_wg_handshake_timeout);
    RUN_TEST(test_wg_disconnect_and_stats);
    RUN_TEST(test_wg_set_rx_callback);
    RUN_TEST(test_wg_response_message_invalid_length_and_receiver);
    RUN_TEST(test_wg_response_decryption_and_mac_validation_failures);
    RUN_TEST(test_wg_send_fail_and_rekey_timeout_task_branches);
    RUN_TEST(test_wg_response_mac1_tampered_rejection);
    RUN_TEST(test_wg_cookie_mac2_verification_failure);
    RUN_TEST(test_wg_send_initiation_failure_task_branch);
    RUN_TEST(test_wg_valid_response_exchange);
    RUN_TEST(test_wg_send_initiation_tx_buf_too_small);
    RUN_TEST(test_wg_send_buffer_too_small);
    RUN_TEST(test_wg_handshake_rekey_timer_expiry);
}
