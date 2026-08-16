/**
 * @file test_mqttsn.c
 * @brief Unit tests for MQTT-SN v1.2 Protocol Client Engine (syn_mqttsn).
 */

#include "mocks/mock_port.h"
#include "syntropic/proto/syn_mqttsn.h"
#include "unity/unity.h"

#include <stdio.h>
#include <string.h>

static uint8_t s_loopback_buf[512];
static size_t s_loopback_len = 0U;
static bool s_mock_mqttsn_send_fail = false;

static bool mock_mqttsn_send(const uint8_t *data, size_t len, void *ctx)
{
    (void)ctx;
    if (s_mock_mqttsn_send_fail || len > sizeof(s_loopback_buf)) {
        return false;
    }
    (void)memcpy(s_loopback_buf, data, len);
    s_loopback_len = len;
    return true;
}

static bool mock_mqttsn_recv(uint8_t *buf, size_t max_len, size_t *out_len, void *ctx)
{
    (void)ctx;
    if (s_loopback_len == 0U) {
        return false;
    }
    size_t copy_len = (s_loopback_len < max_len) ? s_loopback_len : max_len;
    (void)memcpy(buf, s_loopback_buf, copy_len);
    *out_len = copy_len;
    s_loopback_len = 0U;
    return true;
}

static SYN_Transport s_transport = {
    .send = mock_mqttsn_send, .recv = mock_mqttsn_recv, .ctx = NULL};

static uint8_t s_rx_buf[512];
static uint8_t s_tx_buf[512];
static uint16_t s_last_topic_id = 0U;
static size_t s_msg_count = 0U;

static void on_test_mqttsn_msg(SYN_MQTTSN_Client *client, uint16_t topic_id, uint8_t topic_type,
                               const uint8_t *payload, size_t len, void *user_data)
{
    (void)client;
    (void)topic_type;
    (void)payload;
    (void)len;
    (void)user_data;
    s_last_topic_id = topic_id;
    s_msg_count++;
}

void test_mqttsn_init_and_validation(void)
{
    SYN_MQTTSN_Client client;
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_mqttsn_client_init(NULL, NULL));

    SYN_MQTTSN_Config cfg = {
        .transport = &s_transport,
        .client_id = "test_sensor",
        .duration_s = 30U,
        .clean_session = true,
        .rx_buf = s_rx_buf,
        .rx_buf_size = sizeof(s_rx_buf),
        .tx_buf = s_tx_buf,
        .tx_buf_size = sizeof(s_tx_buf),
    };

    /* Small buffer error */
    SYN_MQTTSN_Config bad_cfg = cfg;
    bad_cfg.rx_buf_size = 64U;
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_mqttsn_client_init(&client, &bad_cfg));

    bad_cfg = cfg;
    bad_cfg.tx_buf_size = 64U;
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_mqttsn_client_init(&client, &bad_cfg));

    bad_cfg = cfg;
    bad_cfg.transport = NULL;
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_mqttsn_client_init(&client, &bad_cfg));

    bad_cfg = cfg;
    bad_cfg.rx_buf = NULL;
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_mqttsn_client_init(&client, &bad_cfg));

    bad_cfg = cfg;
    bad_cfg.tx_buf = NULL;
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_mqttsn_client_init(&client, &bad_cfg));

    /* Duration_s = 0 defaults to 30 */
    bad_cfg = cfg;
    bad_cfg.duration_s = 0U;
    TEST_ASSERT_EQUAL(SYN_OK, syn_mqttsn_client_init(&client, &bad_cfg));
    TEST_ASSERT_EQUAL(SYN_MQTTSN_DEFAULT_DURATION_S, client.cfg.duration_s);

    TEST_ASSERT_EQUAL(SYN_OK, syn_mqttsn_client_init(&client, &cfg));
    TEST_ASSERT_EQUAL(SYN_MQTTSN_STATE_DISCONNECTED, client.state);

    /* Test null parameters on all functions */
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_mqttsn_client_searchgw(NULL, 1U));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_mqttsn_client_connect(NULL));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_mqttsn_client_disconnect(NULL));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_mqttsn_client_register_topic(NULL, "a", NULL));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_mqttsn_client_publish(NULL, 1U, 0, false, (const uint8_t *)"a", 1U));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_mqttsn_client_publish_short(NULL, "ab", 0, false,
                                                                         (const uint8_t *)"a", 1U));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_mqttsn_client_publish_short(&client, NULL, 0, false,
                                                                         (const uint8_t *)"a", 1U));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_mqttsn_client_publish_predefined(
                                             NULL, 1U, 0, false, (const uint8_t *)"a", 1U));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_mqttsn_client_subscribe(NULL, "a", 1U, 0));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_mqttsn_client_unsubscribe(NULL, "a", 1U));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_mqttsn_client_sleep(NULL, 10U));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_mqttsn_client_wake(NULL));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_mqttsn_client_step(NULL, 0U));

    SYN_MQTTSN_Client bad_transport_client;
    (void)memset(&bad_transport_client, 0, sizeof(bad_transport_client));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_mqttsn_client_searchgw(&bad_transport_client, 1U));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_mqttsn_client_connect(&bad_transport_client));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_mqttsn_client_disconnect(&bad_transport_client));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_mqttsn_client_register_topic(&bad_transport_client, "a", NULL));
    TEST_ASSERT_EQUAL(
        SYN_INVALID_PARAM,
        syn_mqttsn_client_publish(&bad_transport_client, 1U, 0, false, (const uint8_t *)"a", 1U));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_mqttsn_client_subscribe(&bad_transport_client, "a", 1U, 0));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_mqttsn_client_unsubscribe(&bad_transport_client, "a", 1U));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_mqttsn_client_sleep(&bad_transport_client, 10U));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_mqttsn_client_wake(&bad_transport_client));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_mqttsn_client_step(&bad_transport_client, 0U));
}

void test_mqttsn_discovery_and_connect(void)
{
    SYN_MQTTSN_Client client;
    SYN_MQTTSN_Config cfg = {
        .transport = &s_transport,
        .client_id = "test_sensor_very_long_client_id_exceeding_limits",
        .duration_s = 30U,
        .clean_session = false,
        .on_message = on_test_mqttsn_msg,
        .rx_buf = s_rx_buf,
        .rx_buf_size = sizeof(s_rx_buf),
        .tx_buf = s_tx_buf,
        .tx_buf_size = sizeof(s_tx_buf),
    };
    TEST_ASSERT_EQUAL(SYN_OK, syn_mqttsn_client_init(&client, &cfg));

    /* 1. SEARCHGW with transport send failure */
    s_mock_mqttsn_send_fail = true;
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_mqttsn_client_searchgw(&client, 1U));
    s_mock_mqttsn_send_fail = false;

    TEST_ASSERT_EQUAL(SYN_OK, syn_mqttsn_client_searchgw(&client, 1U));
    TEST_ASSERT_EQUAL(SYN_MQTTSN_STATE_SEARCHING_GW, client.state);

    /* Connect with send failure */
    s_mock_mqttsn_send_fail = true;
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_mqttsn_client_connect(&client));
    s_mock_mqttsn_send_fail = false;

    /* Connect with NULL client_id */
    client.cfg.client_id = NULL;
    client.cfg.clean_session = true;
    TEST_ASSERT_EQUAL(SYN_OK, syn_mqttsn_client_connect(&client));

    /* Simulate GWINFO response */
    uint8_t gwinfo[3] = {0x03, SYN_MQTTSN_MSG_GWINFO, 0x01}; /* Gateway ID = 1 */
    (void)memcpy(s_loopback_buf, gwinfo, sizeof(gwinfo));
    s_loopback_len = sizeof(gwinfo);

    (void)syn_mqttsn_client_step(&client, 100U);
    TEST_ASSERT_EQUAL(1U, client.gateway_id);

    /* Simulate CONNACK response */
    uint8_t connack[3] = {0x03, SYN_MQTTSN_MSG_CONNACK, SYN_MQTTSN_RC_ACCEPTED};
    (void)memcpy(s_loopback_buf, connack, sizeof(connack));
    s_loopback_len = sizeof(connack);

    (void)syn_mqttsn_client_step(&client, 200U);
    TEST_ASSERT_EQUAL(SYN_MQTTSN_STATE_CONNECTED, client.state);

    /* Simulate ADVERTISE packet from Gateway while in SEARCHING_GW state */
    client.state = SYN_MQTTSN_STATE_SEARCHING_GW;
    uint8_t adv[5] = {0x05, SYN_MQTTSN_MSG_ADVERTISE, 0x02, 0x00, 0x0A};
    (void)memcpy(s_loopback_buf, adv, sizeof(adv));
    s_loopback_len = sizeof(adv);
    (void)syn_mqttsn_client_step(&client, 210U);
    TEST_ASSERT_EQUAL(2U, client.gateway_id);
    TEST_ASSERT_EQUAL(SYN_MQTTSN_STATE_CONNECTING, client.state);
}

void test_mqttsn_register_and_publish(void)
{
    SYN_MQTTSN_Client client;
    SYN_MQTTSN_Config cfg = {
        .transport = &s_transport,
        .client_id = "test_sensor",
        .duration_s = 30U,
        .clean_session = true,
        .on_message = on_test_mqttsn_msg,
        .rx_buf = s_rx_buf,
        .rx_buf_size = sizeof(s_rx_buf),
        .tx_buf = s_tx_buf,
        .tx_buf_size = sizeof(s_tx_buf),
    };
    (void)syn_mqttsn_client_init(&client, &cfg);
    client.state = SYN_MQTTSN_STATE_CONNECTED;

    /* Bad topic name length */
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_mqttsn_client_register_topic(&client, "", NULL));
    char long_name[80];
    (void)memset(long_name, 'a', sizeof(long_name));
    long_name[sizeof(long_name) - 1] = '\0';
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_mqttsn_client_register_topic(&client, long_name, NULL));

    /* Send failure on register */
    s_mock_mqttsn_send_fail = true;
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_mqttsn_client_register_topic(&client, "sensors/temp", NULL));
    s_mock_mqttsn_send_fail = false;

    /* 1. Register Topic */
    uint16_t msg_id = 0U;
    TEST_ASSERT_EQUAL(SYN_OK, syn_mqttsn_client_register_topic(&client, "sensors/temp", &msg_id));

    /* Register same topic again (find existing topic path) */
    TEST_ASSERT_EQUAL(SYN_OK, syn_mqttsn_client_register_topic(&client, "sensors/temp", &msg_id));

    /* Simulate REGACK with 0 topic count */
    SYN_MQTTSN_Client zero_topic_client = client;
    zero_topic_client.topic_count = 0U;
    uint8_t regack_0[7] = {0x07, SYN_MQTTSN_MSG_REGACK, 0x00, 0x01, 0x00,
                           0x01, SYN_MQTTSN_RC_ACCEPTED};
    (void)memcpy(s_loopback_buf, regack_0, sizeof(regack_0));
    s_loopback_len = sizeof(regack_0);
    (void)syn_mqttsn_client_step(&zero_topic_client, 250U);

    /* Simulate REGACK */
    uint8_t regack[7] = {0x07, SYN_MQTTSN_MSG_REGACK, 0x00, 0x01, 0x00,
                         0x01, SYN_MQTTSN_RC_ACCEPTED};
    (void)memcpy(s_loopback_buf, regack, sizeof(regack));
    s_loopback_len = sizeof(regack);

    (void)syn_mqttsn_client_step(&client, 300U);
    TEST_ASSERT_EQUAL(1U, client.topic_count);
    TEST_ASSERT_EQUAL(1U, client.topics[0].topic_id);

    /* 2. Publish registered */
    uint8_t payload[] = "23.5";
    TEST_ASSERT_EQUAL(SYN_OK, syn_mqttsn_client_publish(&client, 1U, 0, false, payload, 4U));

    /* Publish with retain = true, empty payload (len = 0) */
    TEST_ASSERT_EQUAL(SYN_OK, syn_mqttsn_client_publish(&client, 1U, 0, true, NULL, 0U));

    /* Publish QoS 1 with next_msg_id rollover */
    client.next_msg_id = 0xFFFFU;
    TEST_ASSERT_EQUAL(SYN_OK, syn_mqttsn_client_publish(&client, 1U, 1, false, payload, 4U));
    TEST_ASSERT_EQUAL(1U, client.next_msg_id);

    /* Publish short 2-character topic */
    TEST_ASSERT_EQUAL(SYN_OK,
                      syn_mqttsn_client_publish_short(&client, "t1", 0, false, payload, 4U));

    /* Publish predefined QoS -1 */
    TEST_ASSERT_EQUAL(SYN_OK,
                      syn_mqttsn_client_publish_predefined(&client, 100U, -1, false, payload, 4U));

    /* Send failure on publish */
    s_mock_mqttsn_send_fail = true;
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_mqttsn_client_publish(&client, 1U, 0, false, payload, 4U));
    s_mock_mqttsn_send_fail = false;

    /* 3. Subscribe & Unsubscribe (normal, short, predefined, QoS 1, QoS -1, send failure) */
    TEST_ASSERT_EQUAL(SYN_OK, syn_mqttsn_client_subscribe(&client, "sensors/temp", 0U, 0));
    TEST_ASSERT_EQUAL(SYN_OK, syn_mqttsn_client_subscribe(&client, "t1", 0U, -1));
    TEST_ASSERT_EQUAL(SYN_OK, syn_mqttsn_client_subscribe(&client, NULL, 50U, 1));

    s_mock_mqttsn_send_fail = true;
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_mqttsn_client_subscribe(&client, "sensors/temp", 0U, 0));
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_mqttsn_client_subscribe(&client, NULL, 50U, 1));
    s_mock_mqttsn_send_fail = false;

    TEST_ASSERT_EQUAL(SYN_OK, syn_mqttsn_client_unsubscribe(&client, "sensors/temp", 0U));
    TEST_ASSERT_EQUAL(SYN_OK, syn_mqttsn_client_unsubscribe(&client, "t1", 0U));
    TEST_ASSERT_EQUAL(SYN_OK, syn_mqttsn_client_unsubscribe(&client, NULL, 50U));

    s_mock_mqttsn_send_fail = true;
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_mqttsn_client_unsubscribe(&client, "sensors/temp", 0U));
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_mqttsn_client_unsubscribe(&client, NULL, 50U));
    s_mock_mqttsn_send_fail = false;

    /* 4. Incoming PUBLISH 1-byte header packet with QoS 1 */
    uint8_t rx_pub[11] = {
        0x0B, SYN_MQTTSN_MSG_PUBLISH, SYN_MQTTSN_FLAG_QOS_1, 0x00, 0x01, 0x00, 0x02, 'O', 'K', '!',
        '!'};
    (void)memcpy(s_loopback_buf, rx_pub, sizeof(rx_pub));
    s_loopback_len = sizeof(rx_pub);

    s_msg_count = 0U;
    (void)syn_mqttsn_client_step(&client, 400U);
    TEST_ASSERT_EQUAL(1U, s_msg_count);
    TEST_ASSERT_EQUAL(1U, s_last_topic_id);

    /* 5. Incoming 3-byte header packet (length >= 256) */
    uint8_t rx_pub_3b[300];
    rx_pub_3b[0] = 0x01;
    rx_pub_3b[1] = 0x01;
    rx_pub_3b[2] = 0x2C; /* 300 bytes total */
    rx_pub_3b[3] = SYN_MQTTSN_MSG_PUBLISH;
    rx_pub_3b[4] = SYN_MQTTSN_FLAG_TOPIC_SHORT;
    rx_pub_3b[5] = 's';
    rx_pub_3b[6] = '1';
    rx_pub_3b[7] = 0x00;
    rx_pub_3b[8] = 0x03;
    (void)memset(&rx_pub_3b[9], 'A', 291U);
    (void)memcpy(s_loopback_buf, rx_pub_3b, 300U);
    s_loopback_len = 300U;

    (void)syn_mqttsn_client_step(&client, 450U);
    TEST_ASSERT_EQUAL(2U, s_msg_count);

    /* 6. Outgoing 3-byte header publish */
    uint8_t large_payload[300];
    (void)memset(large_payload, 0x55, sizeof(large_payload));
    TEST_ASSERT_EQUAL(SYN_OK,
                      syn_mqttsn_client_publish(&client, 1U, 0, false, large_payload, 300U));

    /* 3-byte header publish buffer overflow */
    SYN_MQTTSN_Client tight_3b_client = client;
    tight_3b_client.cfg.tx_buf_size = 280U;
    TEST_ASSERT_EQUAL(
        SYN_ERROR, syn_mqttsn_client_publish(&tight_3b_client, 1U, 0, false, large_payload, 300U));

    /* msg_id rollover in register_topic */
    client.next_msg_id = 0xFFFFU;
    uint16_t roll_id = 0U;
    TEST_ASSERT_EQUAL(SYN_OK, syn_mqttsn_client_register_topic(&client, "rollover/test", &roll_id));
    TEST_ASSERT_EQUAL(1U, client.next_msg_id);

    /* 7. Topic table fill & overflow */
    for (uint8_t i = 2; i < SYN_MQTTSN_MAX_REGISTRATIONS; i++) {
        char name[16];
        (void)snprintf(name, sizeof(name), "top/%u", i);
        uint16_t tid = 0U;
        (void)syn_mqttsn_client_register_topic(&client, name, &tid);
        uint8_t regack_i[7] = {0x07,
                               SYN_MQTTSN_MSG_REGACK,
                               0x00,
                               (uint8_t)(i + 1),
                               (uint8_t)((tid >> 8) & 0xFF),
                               (uint8_t)(tid & 0xFF),
                               SYN_MQTTSN_RC_ACCEPTED};
        (void)memcpy(s_loopback_buf, regack_i, sizeof(regack_i));
        s_loopback_len = sizeof(regack_i);
        (void)syn_mqttsn_client_step(&client, 500U + i);
    }
    /* Attempting 17th topic registration fails due to table capacity */
    uint16_t tid_overflow = 0U;
    TEST_ASSERT_EQUAL(SYN_ERROR,
                      syn_mqttsn_client_register_topic(&client, "overflow_top", &tid_overflow));
}

void test_mqttsn_sleep_and_lifecycle(void)
{
    SYN_MQTTSN_Client client;
    SYN_MQTTSN_Config cfg = {
        .transport = &s_transport,
        .client_id = "test_sensor_with_long_identifier_string",
        .duration_s = 2U,
        .rx_buf = s_rx_buf,
        .rx_buf_size = sizeof(s_rx_buf),
        .tx_buf = s_tx_buf,
        .tx_buf_size = sizeof(s_tx_buf),
    };
    (void)syn_mqttsn_client_init(&client, &cfg);
    client.state = SYN_MQTTSN_STATE_CONNECTED;

    /* Sleep with send failure */
    s_mock_mqttsn_send_fail = true;
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_mqttsn_client_sleep(&client, 60U));
    s_mock_mqttsn_send_fail = false;

    /* Sleep */
    TEST_ASSERT_EQUAL(SYN_OK, syn_mqttsn_client_sleep(&client, 60U));
    TEST_ASSERT_EQUAL(SYN_MQTTSN_STATE_ASLEEP, client.state);

    /* Wake with send failure */
    s_mock_mqttsn_send_fail = true;
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_mqttsn_client_wake(&client));
    s_mock_mqttsn_send_fail = false;

    /* Wake with NULL client_id */
    client.cfg.client_id = NULL;
    TEST_ASSERT_EQUAL(SYN_OK, syn_mqttsn_client_wake(&client));
    TEST_ASSERT_EQUAL(SYN_MQTTSN_STATE_AWAKE, client.state);

    /* PINGRESP */
    uint8_t pingresp[2] = {0x02, SYN_MQTTSN_MSG_PINGRESP};
    (void)memcpy(s_loopback_buf, pingresp, sizeof(pingresp));
    s_loopback_len = sizeof(pingresp);
    (void)syn_mqttsn_client_step(&client, 500U);
    TEST_ASSERT_EQUAL(SYN_MQTTSN_STATE_ASLEEP, client.state);

    /* Disconnect */
    TEST_ASSERT_EQUAL(SYN_OK, syn_mqttsn_client_disconnect(&client));
    TEST_ASSERT_EQUAL(SYN_MQTTSN_STATE_DISCONNECTED, client.state);

    /* Periodic Keepalive step in CONNECTED state */
    s_loopback_len = 0U;
    client.state = SYN_MQTTSN_STATE_CONNECTED;
    client.last_activity_ms = 1000U;
    (void)syn_mqttsn_client_step(&client, 5000U);
    TEST_ASSERT_EQUAL(5000U, client.last_activity_ms);

    /* Step with 1-byte rx length (triggers len < 2 in decode_header) */
    s_loopback_buf[0] = 0x05;
    s_loopback_len = 1U;
    (void)syn_mqttsn_client_step(&client, 5050U);

    /* Test malformed header variants */
    uint8_t mal_1b_underflow[2] = {0x00, 0x01}; /* total_len < 2 */
    (void)memcpy(s_loopback_buf, mal_1b_underflow, 2U);
    s_loopback_len = 2U;
    (void)syn_mqttsn_client_step(&client, 5060U);

    uint8_t mal_3b_short[3] = {0x01, 0x01, 0x00}; /* 3b header with len < 4 */
    (void)memcpy(s_loopback_buf, mal_3b_short, 3U);
    s_loopback_len = 3U;
    (void)syn_mqttsn_client_step(&client, 5065U);

    uint8_t mal_3b_underflow[4] = {0x01, 0x00, 0x02, 0x0C}; /* total_len < 4 */
    (void)memcpy(s_loopback_buf, mal_3b_underflow, 4U);
    s_loopback_len = 4U;
    (void)syn_mqttsn_client_step(&client, 5070U);

    /* Test TX buffer overflow on all APIs */
    SYN_MQTTSN_Client small_tx_client = client;
    small_tx_client.cfg.tx_buf_size = 2U;
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_mqttsn_client_searchgw(&small_tx_client, 1U));
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_mqttsn_client_connect(&small_tx_client));
    TEST_ASSERT_EQUAL(SYN_ERROR,
                      syn_mqttsn_client_register_topic(&small_tx_client, "sensors/temp", NULL));
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_mqttsn_client_publish(&small_tx_client, 1U, 0, false,
                                                           (const uint8_t *)"a", 1U));
    TEST_ASSERT_EQUAL(SYN_ERROR,
                      syn_mqttsn_client_subscribe(&small_tx_client, "sensors/temp", 0U, 0));
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_mqttsn_client_subscribe(&small_tx_client, NULL, 10U, 0));
    TEST_ASSERT_EQUAL(SYN_ERROR,
                      syn_mqttsn_client_unsubscribe(&small_tx_client, "sensors/temp", 0U));
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_mqttsn_client_unsubscribe(&small_tx_client, NULL, 10U));
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_mqttsn_client_sleep(&small_tx_client, 60U));
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_mqttsn_client_wake(&small_tx_client));
    small_tx_client.cfg.tx_buf_size = 1U;
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_mqttsn_client_disconnect(&small_tx_client));

    /* Test msg_id rollover in subscribe and unsubscribe */
    client.next_msg_id = 0xFFFFU;
    TEST_ASSERT_EQUAL(SYN_OK, syn_mqttsn_client_subscribe(&client, "sensors/temp", 0U, 0));
    TEST_ASSERT_EQUAL(1U, client.next_msg_id);

    client.next_msg_id = 0xFFFFU;
    TEST_ASSERT_EQUAL(SYN_OK, syn_mqttsn_client_unsubscribe(&client, "sensors/temp", 0U));
    TEST_ASSERT_EQUAL(1U, client.next_msg_id);

    /* Protothread test */
    SYN_Task task = {.user_data = &client};
    SYN_PT pt;
    PT_INIT(&pt);
    TEST_ASSERT_EQUAL(PT_YIELDED, syn_mqttsn_client_pt(&pt, &task));
    TEST_ASSERT_EQUAL(PT_YIELDED, syn_mqttsn_client_pt(&pt, &task));
    client.cfg.transport = NULL;
    TEST_ASSERT_EQUAL(PT_EXITED, syn_mqttsn_client_pt(&pt, &task));
    client.cfg.transport = &s_transport;

    TEST_ASSERT_EQUAL(PT_ENDED, syn_mqttsn_client_pt(NULL, &task));
    TEST_ASSERT_EQUAL(PT_ENDED, syn_mqttsn_client_pt(&pt, NULL));
    SYN_Task null_task = {.user_data = NULL};
    TEST_ASSERT_EQUAL(PT_ENDED, syn_mqttsn_client_pt(&pt, &null_task));

    /* Protothread termination when step returns error */
    SYN_MQTTSN_Client uninit_client;
    (void)memset(&uninit_client, 0, sizeof(uninit_client));
    SYN_Task uninit_task = {.user_data = &uninit_client};
    SYN_PT pt_err;
    PT_INIT(&pt_err);
    TEST_ASSERT_EQUAL(PT_EXITED, syn_mqttsn_client_pt(&pt_err, &uninit_task));
}

void run_mqttsn_tests(void)
{
    RUN_TEST(test_mqttsn_init_and_validation);
    RUN_TEST(test_mqttsn_discovery_and_connect);
    RUN_TEST(test_mqttsn_register_and_publish);
    RUN_TEST(test_mqttsn_sleep_and_lifecycle);
}
