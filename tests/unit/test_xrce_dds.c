/**
 * @file test_xrce_dds.c
 * @brief Complete unit tests for Micro-ROS & XRCE-DDS Client and CDR Codec (100% coverage).
 */

#include "unity/unity.h"

#include <string.h>
#include <syntropic/proto/syn_xrce_dds.h>

/* ── Loopback / Mock Transport ───────────────────────────────────────────── */

typedef struct {
    uint8_t tx_buf[512];
    size_t tx_len;
    uint8_t rx_buf[512];
    size_t rx_len;
    bool fail_send;
    bool fail_recv;
} MockXRCETransport;

static bool mock_send(const uint8_t *data, size_t len, void *user_ctx)
{
    MockXRCETransport *mock = (MockXRCETransport *)user_ctx;
    if (mock->fail_send) {
        return false;
    }
    if (len > sizeof(mock->tx_buf)) {
        return false;
    }
    (void)memcpy(mock->tx_buf, data, len);
    mock->tx_len = len;
    return true;
}

static bool mock_recv(uint8_t *buf, size_t max_len, size_t *out_len, void *user_ctx)
{
    MockXRCETransport *mock = (MockXRCETransport *)user_ctx;
    if (mock->fail_recv || mock->rx_len == 0U) {
        return false;
    }
    size_t copy_len = (mock->rx_len < max_len) ? mock->rx_len : max_len;
    (void)memcpy(buf, mock->rx_buf, copy_len);
    *out_len = copy_len;
    mock->rx_len = 0U; /* consume */
    return true;
}

/* ── Test Callbacks ──────────────────────────────────────────────────────── */

static uint16_t g_last_req_id = 0U;
static uint8_t g_last_status = 0xFFU;
static uint16_t g_last_reader_id = 0U;
static uint8_t g_last_data_payload[128];
static size_t g_last_data_len = 0U;

static void on_test_status(uint16_t req_id, uint8_t status, void *user_data)
{
    (void)user_data;
    g_last_req_id = req_id;
    g_last_status = status;
}

static void on_test_data(uint16_t reader_id, const uint8_t *payload, size_t len, void *user_data)
{
    (void)user_data;
    g_last_reader_id = reader_id;
    g_last_data_len = (len < sizeof(g_last_data_payload)) ? len : sizeof(g_last_data_payload);
    (void)memcpy(g_last_data_payload, payload, g_last_data_len);
}

/* ── Tests ───────────────────────────────────────────────────────────────── */

void test_xrce_cdr_primitives_and_alignments(void)
{
    uint8_t buffer[256];
    SYN_CDR_Writer w;
    syn_cdr_writer_init(&w, buffer, sizeof(buffer));

    TEST_ASSERT_TRUE(syn_cdr_write_u8(&w, 0x12U));
    TEST_ASSERT_TRUE(syn_cdr_write_u16(&w, 0x3456U));
    TEST_ASSERT_TRUE(syn_cdr_write_u32(&w, 0x789ABCDEU));
    TEST_ASSERT_TRUE(syn_cdr_write_u64(&w, 0x1122334455667788ULL));
    TEST_ASSERT_TRUE(syn_cdr_write_i8(&w, -42));
    TEST_ASSERT_TRUE(syn_cdr_write_i16(&w, -1234));
    TEST_ASSERT_TRUE(syn_cdr_write_i32(&w, -567890));
    TEST_ASSERT_TRUE(syn_cdr_write_i64(&w, -999888777666LL));
    TEST_ASSERT_TRUE(syn_cdr_write_float(&w, 3.14159f));
    TEST_ASSERT_TRUE(syn_cdr_write_double(&w, 2.718281828459));
    TEST_ASSERT_TRUE(syn_cdr_write_bool(&w, true));
    TEST_ASSERT_TRUE(syn_cdr_write_string(&w, "ROS2_Topic"));

    uint8_t test_bytes[] = {0xAA, 0xBB, 0xCC};
    TEST_ASSERT_TRUE(syn_cdr_write_bytes(&w, test_bytes, sizeof(test_bytes)));
    TEST_ASSERT_FALSE(w.error);

    /* Read back and verify */
    SYN_CDR_Reader r;
    syn_cdr_reader_init(&r, buffer, w.pos);

    uint8_t u8_val = 0;
    uint16_t u16_val = 0;
    uint32_t u32_val = 0;
    uint64_t u64_val = 0;
    int8_t i8_val = 0;
    int16_t i16_val = 0;
    int32_t i32_val = 0;
    int64_t i64_val = 0;
    float f_val = 0.0f;
    double d_val = 0.0;
    bool b_val = false;
    char str_buf[32] = {0};
    uint8_t bytes_buf[4] = {0};

    TEST_ASSERT_TRUE(syn_cdr_read_u8(&r, &u8_val));
    TEST_ASSERT_EQUAL_HEX8(0x12, u8_val);

    TEST_ASSERT_TRUE(syn_cdr_read_u16(&r, &u16_val));
    TEST_ASSERT_EQUAL_HEX16(0x3456, u16_val);

    TEST_ASSERT_TRUE(syn_cdr_read_u32(&r, &u32_val));
    TEST_ASSERT_EQUAL_HEX32(0x789ABCDE, u32_val);

    TEST_ASSERT_TRUE(syn_cdr_read_u64(&r, &u64_val));
    TEST_ASSERT_EQUAL_HEX64(0x1122334455667788ULL, u64_val);

    TEST_ASSERT_TRUE(syn_cdr_read_i8(&r, &i8_val));
    TEST_ASSERT_EQUAL_INT8(-42, i8_val);

    TEST_ASSERT_TRUE(syn_cdr_read_i16(&r, &i16_val));
    TEST_ASSERT_EQUAL_INT16(-1234, i16_val);

    TEST_ASSERT_TRUE(syn_cdr_read_i32(&r, &i32_val));
    TEST_ASSERT_EQUAL_INT32(-567890, i32_val);

    TEST_ASSERT_TRUE(syn_cdr_read_i64(&r, &i64_val));
    TEST_ASSERT_EQUAL_INT64(-999888777666LL, i64_val);

    TEST_ASSERT_TRUE(syn_cdr_read_float(&r, &f_val));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 3.14159f, f_val);

    TEST_ASSERT_TRUE(syn_cdr_read_double(&r, &d_val));
    TEST_ASSERT_FLOAT_WITHIN(0.000001, 2.718281828459, (float)d_val);

    TEST_ASSERT_TRUE(syn_cdr_read_bool(&r, &b_val));
    TEST_ASSERT_TRUE(b_val);

    TEST_ASSERT_TRUE(syn_cdr_read_string(&r, str_buf, sizeof(str_buf)));
    TEST_ASSERT_EQUAL_STRING("ROS2_Topic", str_buf);

    TEST_ASSERT_TRUE(syn_cdr_read_bytes(&r, bytes_buf, sizeof(test_bytes)));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(test_bytes, bytes_buf, sizeof(test_bytes));
}

void test_xrce_cdr_error_and_boundaries(void)
{
    /* Null context init */
    syn_cdr_writer_init(NULL, NULL, 0);
    syn_cdr_reader_init(NULL, NULL, 0);

    uint8_t tiny_buf[4];
    SYN_CDR_Writer w;
    syn_cdr_writer_init(&w, tiny_buf, sizeof(tiny_buf));
    TEST_ASSERT_TRUE(syn_cdr_write_bytes(&w, NULL, 0));

    TEST_ASSERT_FALSE(syn_cdr_write_string(NULL, "test"));
    TEST_ASSERT_FALSE(syn_cdr_write_string(&w, NULL));
    TEST_ASSERT_FALSE(syn_cdr_write_string(&w, "VeryLongStringThatOverflows"));
    TEST_ASSERT_TRUE(w.error);

    TEST_ASSERT_FALSE(syn_cdr_write_u8(NULL, 1));
    TEST_ASSERT_FALSE(syn_cdr_write_u16(NULL, 1));
    TEST_ASSERT_FALSE(syn_cdr_write_u32(NULL, 1));
    TEST_ASSERT_FALSE(syn_cdr_write_u64(NULL, 1));
    TEST_ASSERT_FALSE(syn_cdr_write_i8(NULL, 1));
    TEST_ASSERT_FALSE(syn_cdr_write_i16(NULL, 1));
    TEST_ASSERT_FALSE(syn_cdr_write_i32(NULL, 1));
    TEST_ASSERT_FALSE(syn_cdr_write_i64(NULL, 1));
    TEST_ASSERT_FALSE(syn_cdr_write_float(NULL, 1.0f));
    TEST_ASSERT_FALSE(syn_cdr_write_double(NULL, 1.0));
    TEST_ASSERT_FALSE(syn_cdr_write_bool(NULL, true));
    TEST_ASSERT_FALSE(syn_cdr_write_bytes(NULL, tiny_buf, 1));
    TEST_ASSERT_FALSE(syn_cdr_write_bytes(&w, NULL, 1));

    /* Test writer padding overflow when 1 byte left */
    uint8_t pad_buf[3];
    syn_cdr_writer_init(&w, pad_buf, sizeof(pad_buf));
    syn_cdr_write_u8(&w, 1);                         /* pos is now 1 */
    TEST_ASSERT_FALSE(syn_cdr_write_u32(&w, 12345)); /* needs 3 bytes pad + 4 bytes = 8 > 3 */
    TEST_ASSERT_TRUE(w.error);

    syn_cdr_writer_init(&w, pad_buf, 2);
    syn_cdr_write_u8(&w, 1);
    TEST_ASSERT_FALSE(syn_cdr_write_u16(&w, 1234));
    TEST_ASSERT_TRUE(w.error);

    /* Reader errors */
    SYN_CDR_Reader r;
    syn_cdr_reader_init(&r, tiny_buf, 2);
    TEST_ASSERT_TRUE(syn_cdr_read_bytes(&r, tiny_buf, 0));

    uint8_t u8;
    uint16_t u16;
    uint32_t val32 = 0;
    uint64_t u64;
    float f;
    double d;
    bool b;
    int8_t i8;
    int16_t i16;
    int32_t i32;
    int64_t i64;
    TEST_ASSERT_FALSE(syn_cdr_read_u8(NULL, &u8));
    TEST_ASSERT_FALSE(syn_cdr_read_u8(&r, NULL));
    TEST_ASSERT_FALSE(syn_cdr_read_u16(NULL, &u16));
    TEST_ASSERT_FALSE(syn_cdr_read_u16(&r, NULL));
    TEST_ASSERT_FALSE(syn_cdr_read_u32(NULL, &val32));
    TEST_ASSERT_FALSE(syn_cdr_read_u32(&r, NULL));
    TEST_ASSERT_FALSE(syn_cdr_read_u64(NULL, &u64));
    TEST_ASSERT_FALSE(syn_cdr_read_u64(&r, NULL));
    TEST_ASSERT_FALSE(syn_cdr_read_i8(NULL, &i8));
    TEST_ASSERT_FALSE(syn_cdr_read_i8(&r, NULL));
    TEST_ASSERT_FALSE(syn_cdr_read_i16(NULL, &i16));
    TEST_ASSERT_FALSE(syn_cdr_read_i16(&r, NULL));
    TEST_ASSERT_FALSE(syn_cdr_read_i32(NULL, &i32));
    TEST_ASSERT_FALSE(syn_cdr_read_i32(&r, NULL));
    TEST_ASSERT_FALSE(syn_cdr_read_i64(NULL, &i64));
    TEST_ASSERT_FALSE(syn_cdr_read_i64(&r, NULL));
    TEST_ASSERT_FALSE(syn_cdr_read_float(NULL, &f));
    TEST_ASSERT_FALSE(syn_cdr_read_float(&r, NULL));
    TEST_ASSERT_FALSE(syn_cdr_read_double(NULL, &d));
    TEST_ASSERT_FALSE(syn_cdr_read_double(&r, NULL));
    TEST_ASSERT_FALSE(syn_cdr_read_bool(NULL, &b));
    TEST_ASSERT_FALSE(syn_cdr_read_bool(&r, NULL));
    TEST_ASSERT_FALSE(syn_cdr_read_bytes(NULL, tiny_buf, 1));
    TEST_ASSERT_FALSE(syn_cdr_read_bytes(&r, NULL, 1));

    /* Reader padding overflow */
    syn_cdr_reader_init(&r, pad_buf, sizeof(pad_buf));
    syn_cdr_read_u8(&r, &u8);
    TEST_ASSERT_FALSE(syn_cdr_read_u32(&r, &val32));
    TEST_ASSERT_TRUE(r.error);

    syn_cdr_reader_init(&r, pad_buf, 2);
    syn_cdr_read_u8(&r, &u8);
    TEST_ASSERT_FALSE(syn_cdr_read_u16(&r, &u16));
    TEST_ASSERT_TRUE(r.error);

    /* Test string payload overflow when u32 length header fits */
    uint8_t str_overflow_buf[6];
    syn_cdr_writer_init(&w, str_overflow_buf, sizeof(str_overflow_buf));
    TEST_ASSERT_FALSE(syn_cdr_write_string(&w, "Hello"));
    TEST_ASSERT_TRUE(w.error);

    /* Test write bytes overflow */
    syn_cdr_writer_init(&w, str_overflow_buf, 2);
    TEST_ASSERT_FALSE(syn_cdr_write_bytes(&w, tiny_buf, 4));
    TEST_ASSERT_TRUE(w.error);

    /* Reader u64 overflow without padding */
    syn_cdr_reader_init(&r, tiny_buf, 4);
    TEST_ASSERT_FALSE(syn_cdr_read_u64(&r, &u64));
    TEST_ASSERT_TRUE(r.error);

    /* Reader string zero length and overflow */
    uint8_t zero_str_buf[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    syn_cdr_reader_init(&r, zero_str_buf, sizeof(zero_str_buf));
    char str[16];
    TEST_ASSERT_FALSE(syn_cdr_read_string(&r, str, sizeof(str)));

    uint8_t overflow_str_buf[8] = {20, 0, 0, 0, 'a', 'b', 'c', 0};
    syn_cdr_reader_init(&r, overflow_str_buf, sizeof(overflow_str_buf));
    TEST_ASSERT_FALSE(syn_cdr_read_string(&r, str, sizeof(str)));

    /* Writer u64 overflow */
    syn_cdr_writer_init(&w, str_overflow_buf, 4);
    TEST_ASSERT_FALSE(syn_cdr_write_u64(&w, 0x12345678U));
    TEST_ASSERT_TRUE(w.error);

    /* Writer u8 overflow */
    syn_cdr_writer_init(&w, str_overflow_buf, 1);
    TEST_ASSERT_TRUE(syn_cdr_write_u8(&w, 0x11));
    TEST_ASSERT_FALSE(syn_cdr_write_u8(&w, 0x22));
    TEST_ASSERT_TRUE(w.error);

    /* Reader bytes overflow */
    syn_cdr_reader_init(&r, tiny_buf, 2);
    uint8_t dummy_out[8];
    TEST_ASSERT_FALSE(syn_cdr_read_bytes(&r, dummy_out, 5));
    TEST_ASSERT_TRUE(r.error);

    /* Reader string with buffer shorter than u32 length header */
    syn_cdr_reader_init(&r, tiny_buf, 2);
    TEST_ASSERT_FALSE(syn_cdr_read_string(&r, str, sizeof(str)));

    TEST_ASSERT_FALSE(syn_cdr_read_string(NULL, str, sizeof(str)));
    TEST_ASSERT_FALSE(syn_cdr_read_string(&r, NULL, sizeof(str)));
    TEST_ASSERT_FALSE(syn_cdr_read_string(&r, str, 0));
    TEST_ASSERT_FALSE(syn_cdr_read_string(&r, str, sizeof(str)));
}

void test_xrce_client_init_and_validation(void)
{
    SYN_XRCE_Client client;
    SYN_XRCE_Config cfg;
    (void)memset(&cfg, 0, sizeof(cfg));

    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_xrce_client_init(NULL, &cfg));
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_xrce_client_init(&client, NULL));
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_xrce_client_init(&client, &cfg));

    MockXRCETransport mock;
    (void)memset(&mock, 0, sizeof(mock));
    SYN_Transport transport = {.send = mock_send, .recv = mock_recv, .ctx = &mock};
    cfg.transport = &transport;

    uint8_t rx[256], tx[256];
    cfg.rx_buf = rx;
    cfg.rx_buf_size = sizeof(rx);
    cfg.tx_buf = tx;
    cfg.tx_buf_size = sizeof(tx);

    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_xrce_client_init(&client, &cfg));
    TEST_ASSERT_EQUAL_INT(SYN_XRCE_STATE_DISCONNECTED, client.state);

    /* Buffer boundary error */
    cfg.rx_buf_size = 64;
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_xrce_client_init(&client, &cfg));
}

void test_xrce_client_session_and_entity_lifecycle(void)
{
    MockXRCETransport mock;
    (void)memset(&mock, 0, sizeof(mock));
    SYN_Transport transport = {.send = mock_send, .recv = mock_recv, .ctx = &mock};

    uint8_t rx[512], tx[512];
    SYN_XRCE_Config cfg = {
        .client_key = 0xAABBCCDD,
        .session_id = 0x81U,
        .transport = &transport,
        .heartbeat_period_ms = 1000U,
        .on_status = on_test_status,
        .on_data = on_test_data,
        .rx_buf = rx,
        .rx_buf_size = sizeof(rx),
        .tx_buf = tx,
        .tx_buf_size = sizeof(tx),
    };

    SYN_XRCE_Client client;
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_xrce_client_init(&client, &cfg));

    /* 1. Create Session */
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_xrce_client_create_session(&client));
    TEST_ASSERT_EQUAL_INT(SYN_XRCE_STATE_CONNECTING, client.state);
    TEST_ASSERT_GREATER_THAN_UINT(0, mock.tx_len);

    /* 2. Simulate Agent STATUS OK reply */
    SYN_CDR_Writer agent_w;
    syn_cdr_writer_init(&agent_w, mock.rx_buf, sizeof(mock.rx_buf));
    /* Header */
    syn_cdr_write_u8(&agent_w, 0x81U); /* session_id */
    syn_cdr_write_u8(&agent_w, 0x01U); /* stream_id */
    syn_cdr_write_u16(&agent_w, 1U);   /* seq_num */
    syn_cdr_write_u32(&agent_w, 0xAABBCCDD);
    /* Submessage STATUS */
    syn_cdr_write_u8(&agent_w, SYN_XRCE_SUBMSG_STATUS);
    syn_cdr_write_u8(&agent_w, 0x01U); /* flags */
    syn_cdr_write_u16(&agent_w, 6U);   /* length */
    syn_cdr_write_u16(&agent_w, 0U);   /* req_id */
    syn_cdr_write_u16(&agent_w, 0U);   /* obj_id */
    syn_cdr_write_u8(&agent_w, SYN_XRCE_STATUS_OK);
    syn_cdr_write_u8(&agent_w, 0U);
    mock.rx_len = agent_w.pos;

    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_xrce_client_step(&client, 100U));
    TEST_ASSERT_EQUAL_INT(SYN_XRCE_STATE_CONNECTED, client.state);

    /* 3. Create Participant, Topic, Publisher, Subscriber, DataWriter, DataReader */
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_xrce_client_create_participant(&client, 1U));
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_xrce_client_create_topic(&client, 2U, 1U, "sensors/imu",
                                                               "sensor_msgs::msg::dds_::Imu_"));
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_xrce_client_create_publisher(&client, 3U, 1U));
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_xrce_client_create_subscriber(&client, 4U, 1U));
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_xrce_client_create_datawriter(&client, 5U, 3U, 2U));
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_xrce_client_create_datareader(&client, 6U, 4U, 2U));

    /* 4. Publish Topic Data */
    uint8_t imu_payload[] = {0x00, 0x01, 0x02, 0x03, 0x04};
    TEST_ASSERT_EQUAL_INT(
        SYN_OK, syn_xrce_client_write_data(&client, 5U, imu_payload, sizeof(imu_payload)));

    /* 5. Request Data */
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_xrce_client_read_data(&client, 6U));

    /* 6. Simulate incoming DATA submessage */
    syn_cdr_writer_init(&agent_w, mock.rx_buf, sizeof(mock.rx_buf));
    syn_cdr_write_u8(&agent_w, 0x81U);
    syn_cdr_write_u8(&agent_w, 0x01U);
    syn_cdr_write_u16(&agent_w, 2U);
    syn_cdr_write_u32(&agent_w, 0xAABBCCDD);

    syn_cdr_write_u8(&agent_w, SYN_XRCE_SUBMSG_DATA);
    syn_cdr_write_u8(&agent_w, 0x01U);
    syn_cdr_write_u16(&agent_w, 5U + sizeof(imu_payload));
    syn_cdr_write_u16(&agent_w, 10U); /* req_id */
    syn_cdr_write_u16(&agent_w, 6U);  /* reader_id */
    syn_cdr_write_u8(&agent_w, 0U);   /* data_format */
    syn_cdr_write_bytes(&agent_w, imu_payload, sizeof(imu_payload));
    mock.rx_len = agent_w.pos;

    g_last_reader_id = 0U;
    g_last_data_len = 0U;
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_xrce_client_step(&client, 200U));
    TEST_ASSERT_EQUAL_HEX16(6U, g_last_reader_id);
    TEST_ASSERT_EQUAL_UINT(sizeof(imu_payload), g_last_data_len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(imu_payload, g_last_data_payload, sizeof(imu_payload));

    /* 7. Heartbeat trigger */
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_xrce_client_step(&client, 1500U));
}

void test_xrce_client_errors_and_edge_cases(void)
{
    MockXRCETransport mock;
    (void)memset(&mock, 0, sizeof(mock));
    SYN_Transport transport = {.send = mock_send, .recv = mock_recv, .ctx = &mock};

    uint8_t rx[256], tx[256];
    SYN_XRCE_Config cfg = {
        .transport = &transport,
        .rx_buf = rx,
        .rx_buf_size = sizeof(rx),
        .tx_buf = tx,
        .tx_buf_size = sizeof(tx),
    };

    SYN_XRCE_Client client;
    syn_xrce_client_init(&client, &cfg);

    /* Null validation */
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_xrce_client_create_session(NULL));
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_xrce_client_create_participant(NULL, 1));
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_xrce_client_create_topic(NULL, 1, 1, "a", "b"));
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM,
                          syn_xrce_client_create_topic(&client, 1, 1, NULL, "b"));
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM,
                          syn_xrce_client_create_topic(&client, 1, 1, "a", NULL));
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_xrce_client_create_publisher(NULL, 1, 1));
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_xrce_client_create_subscriber(NULL, 1, 1));
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_xrce_client_create_datawriter(NULL, 1, 1, 1));
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_xrce_client_create_datareader(NULL, 1, 1, 1));
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_xrce_client_write_data(NULL, 1, rx, 1));
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_xrce_client_write_data(&client, 1, NULL, 1));
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_xrce_client_read_data(NULL, 1));
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_xrce_client_step(NULL, 0));

    /* Transport send failure */
    mock.fail_send = true;
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_xrce_client_create_session(&client));
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_xrce_client_create_participant(&client, 1));
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_xrce_client_create_topic(&client, 2, 1, "t", "T"));
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_xrce_client_create_publisher(&client, 3, 1));
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_xrce_client_create_subscriber(&client, 4, 1));
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_xrce_client_create_datawriter(&client, 5, 3, 2));
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_xrce_client_create_datareader(&client, 6, 4, 2));
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_xrce_client_write_data(&client, 5, rx, 0));
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_xrce_client_read_data(&client, 6));
    mock.fail_send = false;

    /* Entity Table Overflow */
    client.object_count = SYN_XRCE_MAX_OBJECTS;
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_xrce_client_create_participant(&client, 100));

    /* Large data write causing buffer overflow during submessage finish */
    uint8_t large_payload[512];
    (void)memset(large_payload, 0xAA, sizeof(large_payload));
    TEST_ASSERT_EQUAL_INT(
        SYN_ERROR, syn_xrce_client_write_data(&client, 5, large_payload, sizeof(large_payload)));

    /* Buffer size boundary validation in client init */
    uint8_t small_tx[64];
    SYN_XRCE_Config small_cfg = {
        .transport = &transport,
        .rx_buf = rx,
        .rx_buf_size = sizeof(rx),
        .tx_buf = small_tx,
        .tx_buf_size = sizeof(small_tx),
    };
    SYN_XRCE_Client small_client;
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_xrce_client_init(&small_client, &small_cfg));
}

void test_xrce_client_protothread(void)
{
    MockXRCETransport mock;
    (void)memset(&mock, 0, sizeof(mock));
    SYN_Transport transport = {.send = mock_send, .recv = mock_recv, .ctx = &mock};

    uint8_t rx[256], tx[256];
    SYN_XRCE_Config cfg = {
        .transport = &transport,
        .rx_buf = rx,
        .rx_buf_size = sizeof(rx),
        .tx_buf = tx,
        .tx_buf_size = sizeof(tx),
    };

    SYN_XRCE_Client client;
    syn_xrce_client_init(&client, &cfg);

    SYN_PT pt;
    PT_INIT(&pt);
    SYN_Task task = {.user_data = &client};

    SYN_PT_Status st = syn_xrce_client_pt(&pt, &task);
    TEST_ASSERT_EQUAL_INT(PT_YIELDED, st);

    /* Null checks */
    TEST_ASSERT_EQUAL_INT(PT_ENDED, syn_xrce_client_pt(NULL, &task));
    TEST_ASSERT_EQUAL_INT(PT_ENDED, syn_xrce_client_pt(&pt, NULL));
    SYN_Task null_task = {.user_data = NULL};
    TEST_ASSERT_EQUAL_INT(PT_ENDED, syn_xrce_client_pt(&pt, &null_task));

    /* Break on step error */
    client.cfg.transport = NULL;
    st = syn_xrce_client_pt(&pt, &task);
    TEST_ASSERT_EQUAL_INT(PT_EXITED, st);
}

void run_xrce_dds_tests(void)
{
    RUN_TEST(test_xrce_cdr_primitives_and_alignments);
    RUN_TEST(test_xrce_cdr_error_and_boundaries);
    RUN_TEST(test_xrce_client_init_and_validation);
    RUN_TEST(test_xrce_client_session_and_entity_lifecycle);
    RUN_TEST(test_xrce_client_errors_and_edge_cases);
    RUN_TEST(test_xrce_client_protothread);
}
