/**
 * @file test_lwm2m_task.c
 * @brief Unit tests for Autonomous OMA LwM2M Client Supervisor Task.
 */

#include "syntropic/net/syn_coap.h"
#include "syntropic/net/syn_transport.h"
#include "syntropic/proto/syn_lwm2m.h"
#include "syntropic/proto/syn_lwm2m_task.h"
#include "syntropic/pt/syn_pt.h"
#include "syntropic/sched/syn_sched.h"
#include "unity/unity.h"

#include <string.h>

/* ── Mock Transport ──────────────────────────────────────────────────────── */

#define MOCK_TX_RING_SZ 2048U
#define MOCK_RX_RING_SZ 2048U

static uint8_t g_mock_tx_buf[MOCK_TX_RING_SZ];
static size_t g_mock_tx_len = 0U;

static uint8_t g_mock_rx_buf[MOCK_RX_RING_SZ];
static size_t g_mock_rx_len = 0U;
static size_t g_mock_rx_read_pos = 0U;

static bool mock_transport_send(const uint8_t *data, size_t len, void *ctx)
{
    (void)ctx;
    if (len > sizeof(g_mock_tx_buf)) {
        return false;
    }
    (void)memcpy(g_mock_tx_buf, data, len);
    g_mock_tx_len = len;
    return true;
}

static bool mock_transport_recv(uint8_t *data, size_t max_len, size_t *out_len, void *ctx)
{
    (void)ctx;
    if (g_mock_rx_len == 0U || g_mock_rx_read_pos >= g_mock_rx_len) {
        if (out_len != NULL) {
            *out_len = 0U;
        }
        return false;
    }

    size_t avail = g_mock_rx_len - g_mock_rx_read_pos;
    size_t copy_len = (avail < max_len) ? avail : max_len;
    (void)memcpy(data, &g_mock_rx_buf[g_mock_rx_read_pos], copy_len);
    g_mock_rx_read_pos += copy_len;
    if (out_len != NULL) {
        *out_len = copy_len;
    }
    return true;
}

static void mock_transport_feed_rx(const void *data, size_t len)
{
    if (len <= sizeof(g_mock_rx_buf)) {
        (void)memcpy(g_mock_rx_buf, data, len);
        g_mock_rx_len = len;
        g_mock_rx_read_pos = 0U;
    }
}

static void mock_transport_reset(void)
{
    g_mock_tx_len = 0U;
    g_mock_rx_len = 0U;
    g_mock_rx_read_pos = 0U;
    (void)memset(g_mock_tx_buf, 0, sizeof(g_mock_tx_buf));
    (void)memset(g_mock_rx_buf, 0, sizeof(g_mock_rx_buf));
}

static SYN_Transport g_transport = {
    .send = mock_transport_send,
    .recv = mock_transport_recv,
    .has_data = NULL,
    .ctx = NULL,
};

/* ── Callback Recording State ────────────────────────────────────────────── */

static uint32_t g_reboot_called = 0U;
static uint32_t g_reset_called = 0U;
static uint32_t g_fw_update_called = 0U;
static char g_fw_update_uri[64];
static uint32_t g_state_changes = 0U;
static SYN_LwM2M_ClientState g_last_old_state;
static SYN_LwM2M_ClientState g_last_new_state;

static void on_reboot_cb(void *user_data)
{
    (void)user_data;
    g_reboot_called++;
}

static void on_reset_cb(void *user_data)
{
    (void)user_data;
    g_reset_called++;
}

static void on_fw_update_cb(const char *uri, void *user_data)
{
    (void)user_data;
    g_fw_update_called++;
    if (uri != NULL) {
        (void)strncpy(g_fw_update_uri, uri, sizeof(g_fw_update_uri) - 1U);
        g_fw_update_uri[sizeof(g_fw_update_uri) - 1U] = '\0';
    }
}

static void on_state_cb(SYN_LwM2M_ClientState old_st, SYN_LwM2M_ClientState new_st, void *user_data)
{
    (void)user_data;
    g_state_changes++;
    g_last_old_state = old_st;
    g_last_new_state = new_st;
}

/* ── Test Fixture Setup / Teardown ───────────────────────────────────────── */

static SYN_LwM2M_Client g_client;
static SYN_LwM2M_Task g_task;
static SYN_LwM2M_DeviceContext g_dev_ctx;
static SYN_LwM2M_FirmwareContext g_fw_ctx;
static SYN_LwM2M_SensorContext g_temp_ctx;
static SYN_LwM2M_Object g_dev_obj;
static SYN_LwM2M_Object g_fw_obj;
static SYN_LwM2M_Object g_temp_obj;

static uint8_t g_rx_scratch[512];
static uint8_t g_tx_scratch[512];

static void lwm2m_task_test_setup(void)
{
    mock_transport_reset();

    g_reboot_called = 0U;
    g_reset_called = 0U;
    g_fw_update_called = 0U;
    g_fw_update_uri[0] = '\0';
    g_state_changes = 0U;

    (void)memset(&g_dev_ctx, 0, sizeof(g_dev_ctx));
    g_dev_ctx.manufacturer = "SyntropicOS";
    g_dev_ctx.model_number = "Edge-v1";
    g_dev_ctx.serial_number = "SN-9999";
    g_dev_ctx.firmware_ver = "1.0.0";

    (void)memset(&g_fw_ctx, 0, sizeof(g_fw_ctx));
    (void)memset(&g_temp_ctx, 0, sizeof(g_temp_ctx));
    g_temp_ctx.sensor_value = 23.5;
    g_temp_ctx.unit = "Cel";

    (void)syn_lwm2m_client_init(&g_client, "syn-edge-01", 300U, &g_transport);

    g_dev_obj = syn_lwm2m_make_device_object(&g_dev_ctx);
    g_fw_obj = syn_lwm2m_make_firmware_object(&g_fw_ctx);
    g_temp_obj = syn_lwm2m_make_temperature_object(&g_temp_ctx);

    (void)syn_lwm2m_register_object(&g_client, &g_dev_obj);
    (void)syn_lwm2m_register_object(&g_client, &g_fw_obj);
    (void)syn_lwm2m_register_object(&g_client, &g_temp_obj);

    SYN_LwM2M_TaskConfig cfg = {
        .client = &g_client,
        .transport = &g_transport,
        .coaps_client = NULL,
        .retry_backoff_ms = 2000U,
        .on_reboot = on_reboot_cb,
        .on_reset = on_reset_cb,
        .on_fw_update = on_fw_update_cb,
        .on_state = on_state_cb,
        .user_data = (void *)0x1234,
        .rx_buf = g_rx_scratch,
        .rx_buf_size = sizeof(g_rx_scratch),
        .tx_buf = g_tx_scratch,
        .tx_buf_size = sizeof(g_tx_scratch),
    };

    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_lwm2m_task_init(&g_task, &cfg));
}

/* ── Unit Test Cases ─────────────────────────────────────────────────────── */

void test_lwm2m_task_init_null_and_validation(void)
{
    SYN_LwM2M_Task task;
    SYN_LwM2M_TaskConfig cfg;
    (void)memset(&cfg, 0, sizeof(cfg));

    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_lwm2m_task_init(NULL, &cfg));
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_lwm2m_task_init(&task, NULL));

    /* Missing client / transport */
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_lwm2m_task_init(&task, &cfg));
    cfg.client = &g_client;
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_lwm2m_task_init(&task, &cfg));
    cfg.transport = &g_transport;
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_lwm2m_task_init(&task, &cfg));

    /* Missing or too small scratch buffers */
    cfg.rx_buf = g_rx_scratch;
    cfg.rx_buf_size = 32U; /* Too small (<64) */
    cfg.tx_buf = g_tx_scratch;
    cfg.tx_buf_size = 512U;
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_lwm2m_task_init(&task, &cfg));

    cfg.rx_buf_size = 512U;
    cfg.tx_buf = NULL;
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_lwm2m_task_init(&task, &cfg));

    cfg.tx_buf = g_tx_scratch;
    cfg.tx_buf_size = 32U; /* Too small (<64) */
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_lwm2m_task_init(&task, &cfg));

    cfg.tx_buf_size = 512U;
    cfg.retry_backoff_ms = 0U; /* Should default to 5000 */
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_lwm2m_task_init(&task, &cfg));
    TEST_ASSERT_EQUAL_UINT32(5000U, task.cfg.retry_backoff_ms);

    /* Test trigger functions with NULL */
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_lwm2m_task_trigger_update(NULL));
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_lwm2m_task_deregister(NULL));
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_lwm2m_task_notify_changed(NULL, 3303, 0, 5700));
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_lwm2m_task_step(NULL, 1000U));
}

void test_lwm2m_task_registration_success_flow(void)
{
    lwm2m_task_test_setup();

    /* Initial state: DEREGISTERED */
    TEST_ASSERT_EQUAL_INT(SYN_LWM2M_STATE_DEREGISTERED, g_client.state);
    TEST_ASSERT_EQUAL_UINT32(0U, g_mock_tx_len);

    /* Step 1: Task initiates registration request */
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_lwm2m_task_step(&g_task, 1000U));
    TEST_ASSERT_EQUAL_INT(SYN_LWM2M_STATE_REGISTERING, g_client.state);
    TEST_ASSERT_TRUE(g_mock_tx_len > 0U);
    TEST_ASSERT_EQUAL_UINT32(1U, g_state_changes);
    TEST_ASSERT_EQUAL_INT(SYN_LWM2M_STATE_DEREGISTERED, g_last_old_state);
    TEST_ASSERT_EQUAL_INT(SYN_LWM2M_STATE_REGISTERING, g_last_new_state);

    /* Inspect outgoing CoAP message */
    SYN_CoapMsg tx_coap;
    SYN_CoapOption tx_opts[8];
    size_t tx_opt_cnt = 0U;
    TEST_ASSERT_EQUAL_INT(
        SYN_OK, syn_coap_parse(&tx_coap, tx_opts, 8U, &tx_opt_cnt, g_mock_tx_buf, g_mock_tx_len));
    TEST_ASSERT_EQUAL_UINT8(COAP_TYPE_CON, tx_coap.type);
    TEST_ASSERT_EQUAL_UINT8(COAP_CODE_POST, tx_coap.code);

    /* Simulate Server 2.01 Created response with Location-Path: rd/demo9876 */
    SYN_CoapMsg resp;
    (void)memset(&resp, 0, sizeof(resp));
    resp.type = COAP_TYPE_ACK;
    resp.code = COAP_RESP_CREATED;
    resp.msg_id = tx_coap.msg_id;
    resp.token_len = tx_coap.token_len;
    (void)memcpy(resp.token, tx_coap.token, tx_coap.token_len);

    SYN_CoapOption resp_opts[2];
    resp_opts[0].num = COAP_OPT_LOCATION_PATH;
    resp_opts[0].val = (const uint8_t *)"rd";
    resp_opts[0].len = 2U;
    resp_opts[1].num = COAP_OPT_LOCATION_PATH;
    resp_opts[1].val = (const uint8_t *)"demo9876";
    resp_opts[1].len = 8U;

    uint8_t server_resp_frame[128];
    size_t srv_len =
        syn_coap_serialize(&resp, resp_opts, 2U, server_resp_frame, sizeof(server_resp_frame));
    TEST_ASSERT_TRUE(srv_len > 0U);

    /* Feed response into mock transport */
    mock_transport_reset();
    mock_transport_feed_rx(server_resp_frame, srv_len);

    /* Step 2: Task processes 2.01 Created response */
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_lwm2m_task_step(&g_task, 1100U));
    TEST_ASSERT_EQUAL_INT(SYN_LWM2M_STATE_REGISTERED, g_client.state);
    TEST_ASSERT_EQUAL_STRING("rd/demo9876", g_client.location_path);
    TEST_ASSERT_EQUAL_UINT32(2U, g_state_changes);
    TEST_ASSERT_EQUAL_INT(SYN_LWM2M_STATE_REGISTERING, g_last_old_state);
    TEST_ASSERT_EQUAL_INT(SYN_LWM2M_STATE_REGISTERED, g_last_new_state);
}

void test_lwm2m_task_registration_failure_and_retry_backoff(void)
{
    lwm2m_task_test_setup();

    /* Step 1: Send registration request */
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_lwm2m_task_step(&g_task, 1000U));
    TEST_ASSERT_EQUAL_INT(SYN_LWM2M_STATE_REGISTERING, g_client.state);

    /* Simulate Server 4.00 Bad Request error response */
    SYN_CoapMsg resp;
    (void)memset(&resp, 0, sizeof(resp));
    resp.type = COAP_TYPE_ACK;
    resp.code = COAP_RESP_BAD_REQ;
    resp.msg_id = 1U;
    uint8_t server_resp_frame[64];
    size_t srv_len =
        syn_coap_serialize(&resp, NULL, 0U, server_resp_frame, sizeof(server_resp_frame));
    mock_transport_feed_rx(server_resp_frame, srv_len);

    /* Step 2: Handle rejection */
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_lwm2m_task_step(&g_task, 1100U));
    TEST_ASSERT_EQUAL_INT(SYN_LWM2M_STATE_DEREGISTERED, g_client.state);
    TEST_ASSERT_EQUAL_UINT32(1U, g_task.retry_count);

    /* Step 3: Step before retry_backoff_ms (2000ms) has elapsed - should NOT transmit */
    mock_transport_reset();
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_lwm2m_task_step(&g_task, 2000U));
    TEST_ASSERT_EQUAL_INT(SYN_LWM2M_STATE_DEREGISTERED, g_client.state);
    TEST_ASSERT_EQUAL_UINT32(0U, g_mock_tx_len);

    /* Step 4: Step after retry backoff has elapsed (1000 + 2000 = 3000ms) - should retry */
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_lwm2m_task_step(&g_task, 3100U));
    TEST_ASSERT_EQUAL_INT(SYN_LWM2M_STATE_REGISTERING, g_client.state);
    TEST_ASSERT_TRUE(g_mock_tx_len > 0U);
}

void test_lwm2m_task_lifetime_update_and_force_trigger(void)
{
    test_lwm2m_task_registration_success_flow();

    /* Lifetime is 300s (300,000ms), 80% renewal is at 240,000ms */
    /* Step at 200,000ms: no update needed */
    mock_transport_reset();
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_lwm2m_task_step(&g_task, 200000U));
    TEST_ASSERT_EQUAL_INT(SYN_LWM2M_STATE_REGISTERED, g_client.state);
    TEST_ASSERT_EQUAL_UINT32(0U, g_mock_tx_len);

    /* Step at 242,000ms: exceeds 80% threshold -> sends update request */
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_lwm2m_task_step(&g_task, 242000U));
    TEST_ASSERT_EQUAL_INT(SYN_LWM2M_STATE_UPDATING, g_client.state);
    TEST_ASSERT_TRUE(g_mock_tx_len > 0U);

    /* Verify outgoing update is POST rd/demo9876 */
    SYN_CoapMsg update_coap;
    SYN_CoapOption update_opts[8];
    size_t update_opt_cnt = 0U;
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_coap_parse(&update_coap, update_opts, 8U, &update_opt_cnt,
                                                 g_mock_tx_buf, g_mock_tx_len));
    TEST_ASSERT_EQUAL_UINT8(COAP_CODE_POST, update_coap.code);

    /* Simulate 2.04 Changed response from server */
    SYN_CoapMsg resp;
    (void)memset(&resp, 0, sizeof(resp));
    resp.type = COAP_TYPE_ACK;
    resp.code = COAP_RESP_CHANGED;
    resp.msg_id = update_coap.msg_id;
    uint8_t server_resp_frame[64];
    size_t srv_len =
        syn_coap_serialize(&resp, NULL, 0U, server_resp_frame, sizeof(server_resp_frame));
    mock_transport_reset();
    mock_transport_feed_rx(server_resp_frame, srv_len);

    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_lwm2m_task_step(&g_task, 242100U));
    TEST_ASSERT_EQUAL_INT(SYN_LWM2M_STATE_REGISTERED, g_client.state);

    /* Test Force Update Trigger */
    mock_transport_reset();
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_lwm2m_task_trigger_update(&g_task));
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_lwm2m_task_step(&g_task, 242200U));
    TEST_ASSERT_EQUAL_INT(SYN_LWM2M_STATE_UPDATING, g_client.state);
    TEST_ASSERT_TRUE(g_mock_tx_len > 0U);

    /* Test update rejection (4.04 Not Found) */
    (void)memset(&resp, 0, sizeof(resp));
    resp.type = COAP_TYPE_ACK;
    resp.code = COAP_RESP_NOT_FOUND;
    resp.msg_id = 99U;
    srv_len = syn_coap_serialize(&resp, NULL, 0U, server_resp_frame, sizeof(server_resp_frame));
    mock_transport_reset();
    mock_transport_feed_rx(server_resp_frame, srv_len);

    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_lwm2m_task_step(&g_task, 242300U));
    TEST_ASSERT_EQUAL_INT(SYN_LWM2M_STATE_DEREGISTERED, g_client.state);
}

void test_lwm2m_task_downlink_requests_and_execute_hooks(void)
{
    test_lwm2m_task_registration_success_flow();

    /* 1. Downlink GET /3/0/0 (Device Manufacturer) */
    SYN_CoapMsg get_req;
    (void)memset(&get_req, 0, sizeof(get_req));
    get_req.type = COAP_TYPE_CON;
    get_req.code = COAP_CODE_GET;
    get_req.msg_id = 0x1010;

    SYN_CoapOption get_opts[3];
    get_opts[0].num = COAP_OPT_URI_PATH;
    get_opts[0].val = (const uint8_t *)"3";
    get_opts[0].len = 1U;
    get_opts[1].num = COAP_OPT_URI_PATH;
    get_opts[1].val = (const uint8_t *)"0";
    get_opts[1].len = 1U;
    get_opts[2].num = COAP_OPT_URI_PATH;
    get_opts[2].val = (const uint8_t *)"0";
    get_opts[2].len = 1U;

    uint8_t req_frame[128];
    size_t req_len = syn_coap_serialize(&get_req, get_opts, 3U, req_frame, sizeof(req_frame));
    mock_transport_reset();
    mock_transport_feed_rx(req_frame, req_len);

    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_lwm2m_task_step(&g_task, 3000U));
    TEST_ASSERT_TRUE(g_mock_tx_len > 0U);

    SYN_CoapMsg resp;
    SYN_CoapOption resp_opts[8];
    size_t resp_opt_cnt = 0U;
    TEST_ASSERT_EQUAL_INT(
        SYN_OK, syn_coap_parse(&resp, resp_opts, 8U, &resp_opt_cnt, g_mock_tx_buf, g_mock_tx_len));
    TEST_ASSERT_EQUAL_UINT8(COAP_RESP_CONTENT, resp.code);

    /* 2. Downlink POST /3/0/4 (Device Reboot Command) */
    SYN_CoapMsg reboot_req;
    (void)memset(&reboot_req, 0, sizeof(reboot_req));
    reboot_req.type = COAP_TYPE_CON;
    reboot_req.code = COAP_CODE_POST;
    reboot_req.msg_id = 0x1011;
    get_opts[2].val = (const uint8_t *)"4";
    req_len = syn_coap_serialize(&reboot_req, get_opts, 3U, req_frame, sizeof(req_frame));
    mock_transport_reset();
    mock_transport_feed_rx(req_frame, req_len);

    TEST_ASSERT_EQUAL_UINT32(0U, g_reboot_called);
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_lwm2m_task_step(&g_task, 3100U));
    TEST_ASSERT_EQUAL_UINT32(1U, g_reboot_called);

    /* 3. Downlink POST /3/0/5 (Device Factory Reset Command) */
    get_opts[2].val = (const uint8_t *)"5";
    req_len = syn_coap_serialize(&reboot_req, get_opts, 3U, req_frame, sizeof(req_frame));
    mock_transport_reset();
    mock_transport_feed_rx(req_frame, req_len);

    TEST_ASSERT_EQUAL_UINT32(0U, g_reset_called);
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_lwm2m_task_step(&g_task, 3200U));
    TEST_ASSERT_EQUAL_UINT32(1U, g_reset_called);

    /* 4. Downlink POST /5/0/2 (Firmware Update Command) */
    (void)strncpy(g_fw_ctx.package_uri, "coap://ota.syntropic.io/app_v2.bin",
                  sizeof(g_fw_ctx.package_uri) - 1U);
    get_opts[0].val = (const uint8_t *)"5";
    get_opts[1].val = (const uint8_t *)"0";
    get_opts[2].val = (const uint8_t *)"2";
    req_len = syn_coap_serialize(&reboot_req, get_opts, 3U, req_frame, sizeof(req_frame));
    mock_transport_reset();
    mock_transport_feed_rx(req_frame, req_len);

    TEST_ASSERT_EQUAL_UINT32(0U, g_fw_update_called);
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_lwm2m_task_step(&g_task, 3300U));
    TEST_ASSERT_EQUAL_UINT32(1U, g_fw_update_called);
    TEST_ASSERT_EQUAL_STRING("coap://ota.syntropic.io/app_v2.bin", g_fw_update_uri);
}

void test_lwm2m_task_observe_and_notify_scheduler(void)
{
    test_lwm2m_task_registration_success_flow();

    /* Send Observe registration: GET /3303/0/5700 with Observe=0 */
    SYN_CoapMsg obs_req;
    (void)memset(&obs_req, 0, sizeof(obs_req));
    obs_req.type = COAP_TYPE_CON;
    obs_req.code = COAP_CODE_GET;
    obs_req.msg_id = 0x2020;
    obs_req.token_len = 2U;
    obs_req.token[0] = 0xAA;
    obs_req.token[1] = 0x55;

    uint8_t obs_val = 0U;
    SYN_CoapOption obs_opts[4];
    obs_opts[0].num = COAP_OPT_OBSERVE;
    obs_opts[0].val = &obs_val;
    obs_opts[0].len = 0U;
    obs_opts[1].num = COAP_OPT_URI_PATH;
    obs_opts[1].val = (const uint8_t *)"3303";
    obs_opts[1].len = 4U;
    obs_opts[2].num = COAP_OPT_URI_PATH;
    obs_opts[2].val = (const uint8_t *)"0";
    obs_opts[2].len = 1U;
    obs_opts[3].num = COAP_OPT_URI_PATH;
    obs_opts[3].val = (const uint8_t *)"5700";
    obs_opts[3].len = 4U;

    uint8_t req_frame[128];
    size_t req_len = syn_coap_serialize(&obs_req, obs_opts, 4U, req_frame, sizeof(req_frame));
    mock_transport_reset();
    mock_transport_feed_rx(req_frame, req_len);

    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_lwm2m_task_step(&g_task, 5000U));
    TEST_ASSERT_TRUE(g_mock_tx_len > 0U);

    /* Verify observation slot is active and configure pmin=2s, pmax=10s */
    TEST_ASSERT_TRUE(g_client.observations[0].active);
    g_client.observations[0].pmin = 2U;
    g_client.observations[0].pmax = 10U;
    g_client.observations[0].last_notify_ms = 5000U;

    /* Notify changed within pmin window (1 second later at 6000ms): suppressed */
    mock_transport_reset();
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_lwm2m_task_notify_changed(&g_task, 3303U, 0U, 5700));
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_lwm2m_task_step(&g_task, 6000U));
    TEST_ASSERT_EQUAL_UINT32(0U, g_mock_tx_len);

    /* Notify changed after pmin elapsed (3 seconds later at 8000ms): notification fires */
    mock_transport_reset();
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_lwm2m_task_notify_changed(&g_task, 3303U, 0U, 5700));
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_lwm2m_task_step(&g_task, 8000U));
    TEST_ASSERT_TRUE(g_mock_tx_len > 0U);

    /* Verify notification packet */
    SYN_CoapMsg notif;
    SYN_CoapOption notif_opts[8];
    size_t notif_opt_cnt = 0U;
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_coap_parse(&notif, notif_opts, 8U, &notif_opt_cnt,
                                                 g_mock_tx_buf, g_mock_tx_len));
    TEST_ASSERT_EQUAL_UINT8(COAP_RESP_CONTENT, notif.code);

    /* Test periodic pmax expiration (10 seconds later at 19000ms) without manual change */
    mock_transport_reset();
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_lwm2m_task_step(&g_task, 19000U));
    TEST_ASSERT_TRUE(g_mock_tx_len > 0U);
}

void test_lwm2m_task_graceful_deregistration(void)
{
    test_lwm2m_task_registration_success_flow();

    mock_transport_reset();
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_lwm2m_task_deregister(&g_task));

    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_lwm2m_task_step(&g_task, 10000U));
    TEST_ASSERT_EQUAL_INT(SYN_LWM2M_STATE_DEREGISTERED, g_client.state);
    TEST_ASSERT_EQUAL_STRING("", g_client.location_path);
    TEST_ASSERT_TRUE(g_mock_tx_len > 0U);

    /* Verify outgoing DELETE rd/demo9876 message */
    SYN_CoapMsg del_coap;
    SYN_CoapOption del_opts[8];
    size_t del_opt_cnt = 0U;
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_coap_parse(&del_coap, del_opts, 8U, &del_opt_cnt,
                                                 g_mock_tx_buf, g_mock_tx_len));
    TEST_ASSERT_EQUAL_UINT8(COAP_CODE_DELETE, del_coap.code);
}

void test_lwm2m_task_scheduler_protothread_integration(void)
{
    lwm2m_task_test_setup();

    SYN_PT pt;
    PT_INIT(&pt);

    SYN_Task sched_task;
    (void)memset(&sched_task, 0, sizeof(sched_task));
    sched_task.user_data = &g_task;

    /* NULL protection */
    TEST_ASSERT_EQUAL_INT(PT_ENDED, syn_lwm2m_task_pt(NULL, &sched_task));
    TEST_ASSERT_EQUAL_INT(PT_ENDED, syn_lwm2m_task_pt(&pt, NULL));
    sched_task.user_data = NULL;
    TEST_ASSERT_EQUAL_INT(PT_ENDED, syn_lwm2m_task_pt(&pt, &sched_task));

    /* Execution yield step */
    sched_task.user_data = &g_task;
    SYN_PT_Status status = syn_lwm2m_task_pt(&pt, &sched_task);
    TEST_ASSERT_EQUAL_INT(PT_YIELDED, status);
    TEST_ASSERT_EQUAL_INT(SYN_LWM2M_STATE_REGISTERING, g_client.state);

    /* Test protothread loop exit on step failure */
    g_task.cfg.client = NULL;
    status = syn_lwm2m_task_pt(&pt, &sched_task);
    TEST_ASSERT_EQUAL_INT(PT_EXITED, status);
}

void test_lwm2m_task_dtls_coaps_integration(void)
{
    lwm2m_task_test_setup();

    SYN_DTLS_Config dtls_cfg = {
        .mode = SYN_DTLS_AUTH_MODE_MTLS,
        .server_name = "coaps.syntropic.local",
        .peer_pubkey = NULL,
        .root_ca = NULL,
    };

    uint8_t dtls_rx[512];
    uint8_t dtls_tx[512];
    SYN_CoapsClient coaps_client;
    TEST_ASSERT_TRUE(syn_coaps_client_init(&coaps_client, &dtls_cfg, &g_transport, dtls_rx,
                                           sizeof(dtls_rx), dtls_tx, sizeof(dtls_tx)));

    g_task.cfg.coaps_client = &coaps_client;

    /* Handshake incomplete: step returns SYN_OK and does not send register packet */
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_lwm2m_task_step(&g_task, 1000U));
    TEST_ASSERT_EQUAL_INT(SYN_LWM2M_STATE_DEREGISTERED, g_client.state);
    TEST_ASSERT_EQUAL_UINT32(0U, g_mock_tx_len);

    /* Handshake established: step proceeds with registration */
    uint8_t psk[32] = {0x01};
    SYN_DTLS_Config psk_cfg = {
        .mode = SYN_DTLS_AUTH_MODE_PSK,
        .cipher_suite = SYN_DTLS_CIPHER_SUITE_CHACHA20_POLY1305_SHA256,
        .server_name = "coaps.syntropic.local",
        .psk_identity = (const uint8_t *)"client_1",
        .psk_identity_len = 8,
        .psk_secret = psk,
        .psk_secret_len = sizeof(psk),
    };
    TEST_ASSERT_TRUE(syn_coaps_client_init(&coaps_client, &psk_cfg, &g_transport, dtls_rx,
                                           sizeof(dtls_rx), dtls_tx, sizeof(dtls_tx)));
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_lwm2m_task_step(&g_task, 2000U));
    TEST_ASSERT_EQUAL_INT(SYN_LWM2M_STATE_REGISTERING, g_client.state);
    TEST_ASSERT_TRUE(g_mock_tx_len > 0U);
}

void test_lwm2m_task_edge_cases_and_branches(void)
{
    lwm2m_task_test_setup();

    /* Step with NULL client / transport in config */
    SYN_LwM2M_Task broken_task = g_task;
    broken_task.cfg.client = NULL;
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_lwm2m_task_step(&broken_task, 1000U));

    broken_task = g_task;
    broken_task.cfg.transport = NULL;
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_lwm2m_task_step(&broken_task, 1000U));

    /* Lifetime = 0 edge case */
    g_client.lifetime_s = 0U;
    g_client.state = SYN_LWM2M_STATE_REGISTERED;
    g_client.last_update_ms = 0U;
    mock_transport_reset();
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_lwm2m_task_step(&g_task, 61000U));
    TEST_ASSERT_EQUAL_INT(SYN_LWM2M_STATE_UPDATING, g_client.state);

    /* Deregister while in UPDATING state */
    mock_transport_reset();
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_lwm2m_task_deregister(&g_task));
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_lwm2m_task_step(&g_task, 62000U));
    TEST_ASSERT_EQUAL_INT(SYN_LWM2M_STATE_DEREGISTERED, g_client.state);

    /* Deregister while already in DEREGISTERED state */
    mock_transport_reset();
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_lwm2m_task_deregister(&g_task));
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_lwm2m_task_step(&g_task, 63000U));
    TEST_ASSERT_EQUAL_INT(SYN_LWM2M_STATE_DEREGISTERED, g_client.state);
    TEST_ASSERT_EQUAL_UINT32(0U, g_mock_tx_len);

    /* Inbound request while in DEREGISTERED state and within backoff window */
    g_task.last_action_ms = 63100U;
    SYN_CoapMsg get_req = {.type = COAP_TYPE_CON, .code = COAP_CODE_GET, .msg_id = 0x5050};
    uint8_t req_frame[64];
    size_t req_len = syn_coap_serialize(&get_req, NULL, 0U, req_frame, sizeof(req_frame));
    mock_transport_reset();
    mock_transport_feed_rx(req_frame, req_len);
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_lwm2m_task_step(&g_task, 63100U));
    TEST_ASSERT_EQUAL_UINT32(0U, g_mock_tx_len);

    /* Unsolicited response in REGISTERED state */
    g_client.state = SYN_LWM2M_STATE_REGISTERED;
    SYN_CoapMsg unsol_resp = {.type = COAP_TYPE_ACK, .code = COAP_RESP_CHANGED, .msg_id = 0x7777};
    req_len = syn_coap_serialize(&unsol_resp, NULL, 0U, req_frame, sizeof(req_frame));
    mock_transport_reset();
    mock_transport_feed_rx(req_frame, req_len);
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_lwm2m_task_step(&g_task, 63200U));
    TEST_ASSERT_EQUAL_INT(SYN_LWM2M_STATE_REGISTERED, g_client.state);

    /* Inbound ping / empty message */
    SYN_CoapMsg empty_msg = {.type = COAP_TYPE_CON, .code = 0U, .msg_id = 0x8888};
    req_len = syn_coap_serialize(&empty_msg, NULL, 0U, req_frame, sizeof(req_frame));
    mock_transport_reset();
    mock_transport_feed_rx(req_frame, req_len);
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_lwm2m_task_step(&g_task, 63300U));

    /* Object hooks with NULL object pointer and zero instances */
    SYN_LwM2M_Object empty_obj = {.id = 9999, .instance_count = 0};
    g_client.objects[0] = NULL;
    g_client.objects[1] = &empty_obj;
    g_task.cfg.client = &g_client;
    /* Step downlink request to trigger hook check */
    get_req.code = COAP_CODE_GET;
    get_req.msg_id = 0x5051;
    SYN_CoapOption path_opt = {.num = COAP_OPT_URI_PATH, .val = (const uint8_t *)"3303", .len = 4};
    req_len = syn_coap_serialize(&get_req, &path_opt, 1U, req_frame, sizeof(req_frame));
    mock_transport_reset();
    mock_transport_feed_rx(req_frame, req_len);
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_lwm2m_task_step(&g_task, 63400U));

    /* Unhandled client state default branch */
    g_client.state = (SYN_LwM2M_ClientState)99;
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_lwm2m_task_step(&g_task, 63500U));
}

/* ── Runner Entry Point ─────────────────────────────────────────────────── */

void run_lwm2m_task_tests(void)
{
    RUN_TEST(test_lwm2m_task_init_null_and_validation);
    RUN_TEST(test_lwm2m_task_registration_success_flow);
    RUN_TEST(test_lwm2m_task_registration_failure_and_retry_backoff);
    RUN_TEST(test_lwm2m_task_lifetime_update_and_force_trigger);
    RUN_TEST(test_lwm2m_task_downlink_requests_and_execute_hooks);
    RUN_TEST(test_lwm2m_task_observe_and_notify_scheduler);
    RUN_TEST(test_lwm2m_task_graceful_deregistration);
    RUN_TEST(test_lwm2m_task_scheduler_protothread_integration);
    RUN_TEST(test_lwm2m_task_dtls_coaps_integration);
    RUN_TEST(test_lwm2m_task_edge_cases_and_branches);
}
