/**
 * @file test_ocpp.c
 * @brief Unit tests for Open Charge Point Protocol (OCPP-J 1.6 / 2.0.1) Dual-Role Engine.
 */

#include "syntropic/proto/syn_ocpp.h"
#include "unity/unity.h"

#include <string.h>

static SYN_OCPP_Client g_ocpp_client;
static SYN_OCPP_Server g_ocpp_server;
static bool g_reg_cb_called = false;
static bool g_auth_cb_called = false;
static bool g_start_tx_cb_called = false;
static bool g_remote_start_called = false;
static bool g_remote_stop_called = false;
static bool g_server_boot_called = false;

static void on_ocpp_reg(SYN_OCPP_RegistrationStatus status, uint32_t interval, void *ctx)
{
    (void)status;
    (void)interval;
    (void)ctx;
    g_reg_cb_called = true;
}

static void on_ocpp_auth(const char *id_tag, SYN_OCPP_AuthorizationStatus status, void *ctx)
{
    (void)id_tag;
    (void)status;
    (void)ctx;
    g_auth_cb_called = true;
}

static void on_ocpp_start_tx(int32_t tx_id, SYN_OCPP_AuthorizationStatus status, void *ctx)
{
    (void)tx_id;
    (void)status;
    (void)ctx;
    g_start_tx_cb_called = true;
}

static bool on_ocpp_remote_start(uint32_t conn_id, const char *id_tag, void *ctx)
{
    (void)conn_id;
    (void)id_tag;
    (void)ctx;
    g_remote_start_called = true;
    return true;
}

static bool on_ocpp_remote_stop(int32_t tx_id, void *ctx)
{
    (void)tx_id;
    (void)ctx;
    g_remote_stop_called = true;
    return true;
}

static SYN_OCPP_RegistrationStatus on_server_boot(const SYN_OCPP_ChargePointInfo *info,
                                                  uint32_t *hb_sec, void *ctx)
{
    (void)info;
    (void)ctx;
    g_server_boot_called = true;
    if (hb_sec)
        *hb_sec = 120U;
    return SYN_OCPP_REGISTRATION_ACCEPTED;
}

static SYN_OCPP_AuthorizationStatus on_server_auth(const char *id_tag, void *ctx)
{
    (void)id_tag;
    (void)ctx;
    return SYN_OCPP_AUTH_ACCEPTED;
}

static int32_t on_server_start_tx(uint32_t conn_id, const char *id_tag, uint32_t meter_start,
                                  void *ctx)
{
    (void)conn_id;
    (void)id_tag;
    (void)meter_start;
    (void)ctx;
    return 2026;
}

void test_ocpp_init_and_null_checks(void)
{
    char buf[128];
    size_t len = 0;
    SYN_OCPP_ChargePointInfo info = {"Vendor", "Model", "SN123", "v1.0"};

    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_ocpp_init(NULL));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_ocpp_set_callbacks(NULL, NULL, NULL, NULL, NULL, NULL, NULL));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_ocpp_server_init(NULL));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_ocpp_server_set_callbacks(NULL, NULL, NULL, NULL, NULL));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_ocpp_format_boot_notification(NULL, &info, buf, sizeof(buf), &len));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_ocpp_format_heartbeat(NULL, buf, sizeof(buf), &len));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_ocpp_format_heartbeat(&g_ocpp_client, buf, 15, &len));
    syn_ocpp_tick(NULL, 10, buf, sizeof(buf), &len);
}

void test_ocpp_boot_notification_formatting(void)
{
    syn_ocpp_init(&g_ocpp_client);
    SYN_OCPP_ChargePointInfo info = {"SyntropicVendor", "EVSE-v2", "SN-9999", "2.0.1"};
    char buf[256];
    size_t len = 0;

    TEST_ASSERT_EQUAL(
        SYN_OK, syn_ocpp_format_boot_notification(&g_ocpp_client, &info, buf, sizeof(buf), &len));
    TEST_ASSERT_TRUE(len > 0);
    TEST_ASSERT_NOT_NULL(strstr(buf, "BootNotification"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "SyntropicVendor"));
}

void test_ocpp_heartbeat_and_status_formatting(void)
{
    syn_ocpp_init(&g_ocpp_client);
    char buf[256];
    size_t len = 0;

    TEST_ASSERT_EQUAL(SYN_OK, syn_ocpp_format_heartbeat(&g_ocpp_client, buf, sizeof(buf), &len));
    TEST_ASSERT_NOT_NULL(strstr(buf, "Heartbeat"));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_ocpp_format_heartbeat(&g_ocpp_client, buf, 10, &len));

    TEST_ASSERT_EQUAL(
        SYN_OK, syn_ocpp_format_status_notification(&g_ocpp_client, 1, SYN_OCPP_STATUS_CHARGING,
                                                    "NoError", buf, sizeof(buf), &len));
    TEST_ASSERT_NOT_NULL(strstr(buf, "StatusNotification"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "Charging"));
}

void test_ocpp_transaction_formatting(void)
{
    syn_ocpp_init(&g_ocpp_client);
    char buf[256];
    size_t len = 0;

    TEST_ASSERT_EQUAL(
        SYN_OK, syn_ocpp_format_authorize(&g_ocpp_client, "RFID-TAG-123", buf, sizeof(buf), &len));
    TEST_ASSERT_NOT_NULL(strstr(buf, "Authorize"));

    TEST_ASSERT_EQUAL(SYN_OK, syn_ocpp_format_start_transaction(&g_ocpp_client, 1, "RFID-TAG-123",
                                                                1000, buf, sizeof(buf), &len));
    TEST_ASSERT_NOT_NULL(strstr(buf, "StartTransaction"));

    TEST_ASSERT_EQUAL(SYN_OK,
                      syn_ocpp_format_stop_transaction(&g_ocpp_client, 42, 12500, "EVDisconnected",
                                                       buf, sizeof(buf), &len));
    TEST_ASSERT_NOT_NULL(strstr(buf, "StopTransaction"));
}

void test_ocpp_meter_values_formatting(void)
{
    syn_ocpp_init(&g_ocpp_client);
    SYN_OCPP_MeterValues mv = {12500, 230, 160, 3680, 75};
    char buf[256];
    size_t len = 0;

    TEST_ASSERT_EQUAL(SYN_OK,
                      syn_ocpp_format_meter_values(&g_ocpp_client, 1, &mv, buf, sizeof(buf), &len));
    TEST_ASSERT_NOT_NULL(strstr(buf, "MeterValues"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "12500"));
}

void test_ocpp_process_call_result(void)
{
    syn_ocpp_init(&g_ocpp_client);
    g_reg_cb_called = false;
    syn_ocpp_set_callbacks(&g_ocpp_client, on_ocpp_reg, on_ocpp_auth, on_ocpp_start_tx,
                           on_ocpp_remote_start, on_ocpp_remote_stop, NULL);

    const char *resp_json = "[3,\"1\",{\"status\":\"Accepted\",\"interval\":60}]";
    TEST_ASSERT_EQUAL(SYN_OK, syn_ocpp_process_message(&g_ocpp_client, resp_json, strlen(resp_json),
                                                       NULL, 0, NULL));
    TEST_ASSERT_EQUAL(SYN_OCPP_REGISTRATION_ACCEPTED, g_ocpp_client.registration_status);
    TEST_ASSERT_TRUE(g_reg_cb_called);

    const char *resp_status_json = "[3,\"2\",{\"status\":\"Accepted\"}]";
    TEST_ASSERT_EQUAL(SYN_OK, syn_ocpp_process_message(&g_ocpp_client, resp_status_json,
                                                       strlen(resp_status_json), NULL, 0, NULL));
}

void test_ocpp_process_remote_start_stop(void)
{
    syn_ocpp_init(&g_ocpp_client);
    g_remote_start_called = false;
    g_remote_stop_called = false;
    syn_ocpp_set_callbacks(&g_ocpp_client, on_ocpp_reg, on_ocpp_auth, on_ocpp_start_tx,
                           on_ocpp_remote_start, on_ocpp_remote_stop, NULL);

    const char *cmd_start =
        "[2,\"10\",\"RemoteStartTransaction\",{\"connectorId\":1,\"idTag\":\"RFID-101\"}]";
    char resp[128];
    size_t resp_len = 0;

    TEST_ASSERT_EQUAL(SYN_OK, syn_ocpp_process_message(&g_ocpp_client, cmd_start, strlen(cmd_start),
                                                       resp, sizeof(resp), &resp_len));
    TEST_ASSERT_TRUE(g_remote_start_called);
    TEST_ASSERT_NOT_NULL(strstr(resp, "Accepted"));

    const char *cmd_stop = "[2,\"11\",\"RemoteStopTransaction\",{\"transactionId\":42}]";
    TEST_ASSERT_EQUAL(SYN_OK, syn_ocpp_process_message(&g_ocpp_client, cmd_stop, strlen(cmd_stop),
                                                       resp, sizeof(resp), &resp_len));
    TEST_ASSERT_TRUE(g_remote_stop_called);

    syn_ocpp_init(&g_ocpp_client);
    TEST_ASSERT_EQUAL(SYN_OK, syn_ocpp_process_message(&g_ocpp_client, cmd_start, strlen(cmd_start),
                                                       resp, sizeof(resp), &resp_len));
    TEST_ASSERT_NOT_NULL(strstr(resp, "Accepted"));
    TEST_ASSERT_EQUAL(SYN_OK, syn_ocpp_process_message(&g_ocpp_client, cmd_stop, strlen(cmd_stop),
                                                       resp, sizeof(resp), &resp_len));
    TEST_ASSERT_NOT_NULL(strstr(resp, "Accepted"));
}

void test_ocpp_tick(void)
{
    syn_ocpp_init(&g_ocpp_client);
    g_ocpp_client.heartbeat_timer_ms = 100;

    char hb_buf[128];
    size_t hb_len = 0;

    syn_ocpp_tick(&g_ocpp_client, 50, hb_buf, sizeof(hb_buf), &hb_len);
    TEST_ASSERT_EQUAL(0, hb_len);

    syn_ocpp_tick(&g_ocpp_client, 60, hb_buf, sizeof(hb_buf), &hb_len);
    TEST_ASSERT_TRUE(hb_len > 0);
    TEST_ASSERT_NOT_NULL(strstr(hb_buf, "Heartbeat"));
}

void test_ocpp_all_status_enums_and_small_buffers(void)
{
    syn_ocpp_init(&g_ocpp_client);
    char buf[256];
    size_t len = 0;

    SYN_OCPP_ChargePointStatus statuses[] = {
        SYN_OCPP_STATUS_AVAILABLE,     SYN_OCPP_STATUS_PREPARING,      SYN_OCPP_STATUS_CHARGING,
        SYN_OCPP_STATUS_SUSPENDED_EV,  SYN_OCPP_STATUS_SUSPENDED_EVSE, SYN_OCPP_STATUS_FINISHING,
        SYN_OCPP_STATUS_RESERVED,      SYN_OCPP_STATUS_UNAVAILABLE,    SYN_OCPP_STATUS_FAULTED,
        (SYN_OCPP_ChargePointStatus)99};

    for (size_t i = 0; i < sizeof(statuses) / sizeof(statuses[0]); i++) {
        TEST_ASSERT_EQUAL(SYN_OK,
                          syn_ocpp_format_status_notification(&g_ocpp_client, 1, statuses[i],
                                                              "NoError", buf, sizeof(buf), &len));
    }

    SYN_OCPP_ChargePointInfo info = {"Vendor", "Model", "SN", "v1"};
    SYN_OCPP_MeterValues mv = {100, 230, 16, 3600, 50};

    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_ocpp_format_boot_notification(&g_ocpp_client, &info, buf, 10, &len));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_ocpp_format_heartbeat(&g_ocpp_client, buf, 10, &len));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_ocpp_format_status_notification(
                          &g_ocpp_client, 1, SYN_OCPP_STATUS_CHARGING, "NoError", buf, 10, &len));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_ocpp_format_authorize(&g_ocpp_client, "TAG", buf, 10, &len));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_ocpp_format_start_transaction(&g_ocpp_client, 1, "TAG",
                                                                           100, buf, 10, &len));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_ocpp_format_stop_transaction(&g_ocpp_client, 1, 100,
                                                                          "Reason", buf, 10, &len));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_ocpp_format_meter_values(&g_ocpp_client, 1, &mv, buf, 10, &len));

    /* NULL info fields default check */
    SYN_OCPP_ChargePointInfo null_info = {NULL, NULL, NULL, NULL};
    TEST_ASSERT_EQUAL(SYN_OK, syn_ocpp_format_boot_notification(&g_ocpp_client, &null_info, buf,
                                                                sizeof(buf), &len));
    TEST_ASSERT_NOT_NULL(strstr(buf, "SyntropicOS"));

    /* Buffer capacity truncation tests (SYN_ERROR) */
    TEST_ASSERT_EQUAL(SYN_ERROR,
                      syn_ocpp_format_boot_notification(&g_ocpp_client, &info, buf, 70, &len));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_ocpp_format_heartbeat(&g_ocpp_client, buf, 20, &len));
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_ocpp_format_status_notification(&g_ocpp_client, 1,
                                                                     SYN_OCPP_STATUS_CHARGING,
                                                                     "NoError", buf, 70, &len));
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_ocpp_format_authorize(
                                     &g_ocpp_client, "TAG-VERY-LONG-STRING-12345", buf, 50, &len));
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_ocpp_format_start_transaction(&g_ocpp_client, 1,
                                                                   "TAG-VERY-LONG-STRING-12345",
                                                                   100, buf, 100, &len));
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_ocpp_format_stop_transaction(&g_ocpp_client, 1, 100, "Reason",
                                                                  buf, 100, &len));
    TEST_ASSERT_EQUAL(SYN_ERROR,
                      syn_ocpp_format_meter_values(&g_ocpp_client, 1, &mv, buf, 130, &len));
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_ocpp_server_format_remote_start(&g_ocpp_server, 1,
                                                                     "TAG-VERY-LONG-STRING-12345",
                                                                     buf, 65, &len));
    TEST_ASSERT_EQUAL(SYN_ERROR,
                      syn_ocpp_server_format_remote_stop(&g_ocpp_server, 100, buf, 50, &len));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_ocpp_server_format_remote_start(
                                             &g_ocpp_server, 1, NULL, buf, sizeof(buf), &len));
}

void test_ocpp_process_message_extended(void)
{
    syn_ocpp_init(&g_ocpp_client);
    g_start_tx_cb_called = false;
    syn_ocpp_set_callbacks(&g_ocpp_client, on_ocpp_reg, on_ocpp_auth, on_ocpp_start_tx, NULL, NULL,
                           NULL);

    const char *resp_tx =
        "[3,\"100\",{\"idTagInfo\":{\"status\":\"Accepted\"},\"transactionId\":1001}]";
    TEST_ASSERT_EQUAL(
        SYN_OK, syn_ocpp_process_message(&g_ocpp_client, resp_tx, strlen(resp_tx), NULL, 0, NULL));
    TEST_ASSERT_EQUAL(1001, g_ocpp_client.active_transaction_id);
    TEST_ASSERT_TRUE(g_start_tx_cb_called);

    const char *cmd_unknown = "[2,\"200\",\"DataTransfer\",{\"vendorId\":\"Vendor\"}]";
    char resp[128];
    size_t resp_len = 0;
    TEST_ASSERT_EQUAL(SYN_OK,
                      syn_ocpp_process_message(&g_ocpp_client, cmd_unknown, strlen(cmd_unknown),
                                               resp, sizeof(resp), &resp_len));
    TEST_ASSERT_NOT_NULL(strstr(resp, "[3,\"200\",{}]"));

    const char *err_msg = "[4,\"300\",\"NotSupported\",\"Action not supported\",{}]";
    TEST_ASSERT_EQUAL(
        SYN_OK, syn_ocpp_process_message(&g_ocpp_client, err_msg, strlen(err_msg), NULL, 0, NULL));

    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_ocpp_process_message(&g_ocpp_client, "123", 3, resp,
                                                                  sizeof(resp), &resp_len));
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_ocpp_process_message(&g_ocpp_client, "INVALID_FRAME", 13, resp,
                                                          sizeof(resp), &resp_len));
}

void test_ocpp_server_role(void)
{
    syn_ocpp_server_init(&g_ocpp_server);
    g_server_boot_called = false;
    syn_ocpp_server_set_callbacks(&g_ocpp_server, on_server_boot, on_server_auth,
                                  on_server_start_tx, NULL);

    char resp[256];
    size_t resp_len = 0;

    /* 1. Server processes station BootNotification */
    const char *req_boot = "[2,\"1\",\"BootNotification\",{\"chargePointVendor\":\"Vendor\"}]";
    TEST_ASSERT_EQUAL(SYN_OK,
                      syn_ocpp_server_process_message(&g_ocpp_server, req_boot, strlen(req_boot),
                                                      resp, sizeof(resp), &resp_len));
    TEST_ASSERT_TRUE(g_server_boot_called);
    TEST_ASSERT_NOT_NULL(strstr(resp, "Accepted"));
    TEST_ASSERT_NOT_NULL(strstr(resp, "120"));

    /* 2. Server processes station Authorize */
    const char *req_auth = "[2,\"2\",\"Authorize\",{\"idTag\":\"RFID-1\"}]";
    TEST_ASSERT_EQUAL(SYN_OK,
                      syn_ocpp_server_process_message(&g_ocpp_server, req_auth, strlen(req_auth),
                                                      resp, sizeof(resp), &resp_len));
    TEST_ASSERT_NOT_NULL(strstr(resp, "Accepted"));

    /* 3. Server processes station StartTransaction */
    const char *req_tx = "[2,\"3\",\"StartTransaction\",{\"connectorId\":1,\"idTag\":\"RFID-1\"}]";
    TEST_ASSERT_EQUAL(SYN_OK,
                      syn_ocpp_server_process_message(&g_ocpp_server, req_tx, strlen(req_tx), resp,
                                                      sizeof(resp), &resp_len));
    TEST_ASSERT_NOT_NULL(strstr(resp, "2026"));

    /* 4. Server processes Heartbeat call */
    const char *req_hb = "[2,\"4\",\"Heartbeat\",{}]";
    TEST_ASSERT_EQUAL(SYN_OK,
                      syn_ocpp_server_process_message(&g_ocpp_server, req_hb, strlen(req_hb), resp,
                                                      sizeof(resp), &resp_len));

    /* 5. Server formats RemoteStart / RemoteStop commands */
    char cmd_buf[128];
    size_t cmd_len = 0;
    TEST_ASSERT_EQUAL(SYN_OK,
                      syn_ocpp_server_format_remote_start(&g_ocpp_server, 1, "REMOTE-TAG", cmd_buf,
                                                          sizeof(cmd_buf), &cmd_len));
    TEST_ASSERT_NOT_NULL(strstr(cmd_buf, "RemoteStartTransaction"));

    TEST_ASSERT_EQUAL(SYN_OK, syn_ocpp_server_format_remote_stop(&g_ocpp_server, 2026, cmd_buf,
                                                                 sizeof(cmd_buf), &cmd_len));
    TEST_ASSERT_NOT_NULL(strstr(cmd_buf, "RemoteStopTransaction"));

    /* Null & error checks */
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_ocpp_server_format_remote_start(
                                             NULL, 1, "TAG", cmd_buf, sizeof(cmd_buf), &cmd_len));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_ocpp_server_format_remote_stop(
                                             NULL, 1, cmd_buf, sizeof(cmd_buf), &cmd_len));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_ocpp_server_process_message(NULL, req_boot, strlen(req_boot), resp,
                                                      sizeof(resp), &resp_len));
    TEST_ASSERT_EQUAL(SYN_ERROR,
                      syn_ocpp_server_process_message(&g_ocpp_server, "INVALID_FRAME", 13, resp,
                                                      sizeof(resp), &resp_len));
    TEST_ASSERT_EQUAL(SYN_OK, syn_ocpp_server_process_message(&g_ocpp_server, "[3,\"1\",{}]", 10,
                                                              resp, sizeof(resp), &resp_len));

    /* Server process message without callbacks */
    syn_ocpp_server_init(&g_ocpp_server);
    TEST_ASSERT_EQUAL(SYN_OK,
                      syn_ocpp_server_process_message(&g_ocpp_server, req_boot, strlen(req_boot),
                                                      resp, sizeof(resp), &resp_len));
    TEST_ASSERT_NOT_NULL(strstr(resp, "Accepted"));
    TEST_ASSERT_EQUAL(SYN_OK,
                      syn_ocpp_server_process_message(&g_ocpp_server, req_auth, strlen(req_auth),
                                                      resp, sizeof(resp), &resp_len));
    TEST_ASSERT_EQUAL(SYN_OK,
                      syn_ocpp_server_process_message(&g_ocpp_server, req_tx, strlen(req_tx), resp,
                                                      sizeof(resp), &resp_len));
}

void test_ocpp_call_error_formatting(void)
{
    char buf[128];
    size_t len = 0;

    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_ocpp_format_call_error(NULL, SYN_OCPP_ERROR_NOT_IMPLEMENTED, "Desc", buf,
                                                 sizeof(buf), &len));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_ocpp_format_call_error("123", NULL, "Desc", buf, sizeof(buf), &len));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_ocpp_format_call_error("123", SYN_OCPP_ERROR_NOT_IMPLEMENTED, "Desc",
                                                 NULL, sizeof(buf), &len));
    TEST_ASSERT_EQUAL(
        SYN_INVALID_PARAM,
        syn_ocpp_format_call_error("123", SYN_OCPP_ERROR_NOT_IMPLEMENTED, "Desc", buf, 10, &len));

    TEST_ASSERT_EQUAL(SYN_OK, syn_ocpp_format_call_error("1001", SYN_OCPP_ERROR_NOT_IMPLEMENTED,
                                                         "Action unknown", buf, sizeof(buf), &len));
    TEST_ASSERT_NOT_NULL(strstr(buf, "[4,\"1001\",\"NotImplemented\",\"Action unknown\",{}]"));

    TEST_ASSERT_EQUAL(SYN_ERROR, syn_ocpp_format_call_error(
                                     "1001", SYN_OCPP_ERROR_NOT_IMPLEMENTED,
                                     "Very long error description string that exceeds capacity",
                                     buf, 40, &len));
}

void test_ocpp_21_features(void)
{
    char buf[256];
    size_t len = 0;

    /* Verify subprotocol strings */
    TEST_ASSERT_EQUAL_STRING("ocpp2.1", SYN_OCPP_SUBPROTOCOL_2_1);
    TEST_ASSERT_EQUAL_STRING("ocpp2.0.1", SYN_OCPP_SUBPROTOCOL_2_0_1);
    TEST_ASSERT_EQUAL_STRING("ocpp1.6", SYN_OCPP_SUBPROTOCOL_1_6);

    /* Verify DisplayMessage formatting */
    SYN_OCPP_DisplayMessage msg = {101, "Tariff Notice", "Off-peak rate active", 30};
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_ocpp_format_display_message(NULL, &msg, buf, sizeof(buf), &len));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_ocpp_format_display_message(&g_ocpp_client, NULL, buf,
                                                                         sizeof(buf), &len));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_ocpp_format_display_message(&g_ocpp_client, &msg, buf, 10, &len));
    TEST_ASSERT_EQUAL(
        SYN_OK, syn_ocpp_format_display_message(&g_ocpp_client, &msg, buf, sizeof(buf), &len));
    TEST_ASSERT_NOT_NULL(strstr(buf, "DisplayMessage"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "Off-peak rate active"));
    TEST_ASSERT_EQUAL(SYN_ERROR,
                      syn_ocpp_format_display_message(&g_ocpp_client, &msg, buf, 35, &len));

    /* Verify V2G Energy Transfer formatting */
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_ocpp_format_v2g_energy_transfer(NULL, SYN_OCPP_V2G_SCHEDULED, 11000, buf,
                                                          sizeof(buf), &len));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_ocpp_format_v2g_energy_transfer(&g_ocpp_client, SYN_OCPP_V2G_SCHEDULED,
                                                          11000, buf, 10, &len));
    TEST_ASSERT_EQUAL(SYN_OK,
                      syn_ocpp_format_v2g_energy_transfer(&g_ocpp_client, SYN_OCPP_V2G_SCHEDULED,
                                                          11000, buf, sizeof(buf), &len));
    TEST_ASSERT_EQUAL(SYN_OCPP_V2G_SCHEDULED, g_ocpp_client.v2g_mode);
    TEST_ASSERT_NOT_NULL(strstr(buf, "V2GEnergyTransfer"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "Scheduled"));

    TEST_ASSERT_EQUAL(SYN_OK,
                      syn_ocpp_format_v2g_energy_transfer(&g_ocpp_client, SYN_OCPP_V2G_DYNAMIC,
                                                          -5000, buf, sizeof(buf), &len));
    TEST_ASSERT_EQUAL(SYN_OCPP_V2G_DYNAMIC, g_ocpp_client.v2g_mode);
    TEST_ASSERT_NOT_NULL(strstr(buf, "Dynamic"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "-5000"));

    TEST_ASSERT_EQUAL(SYN_ERROR, syn_ocpp_format_v2g_energy_transfer(
                                     &g_ocpp_client, SYN_OCPP_V2G_DYNAMIC, -5000, buf, 35, &len));
}

void run_ocpp_tests(void)
{
    RUN_TEST(test_ocpp_init_and_null_checks);
    RUN_TEST(test_ocpp_boot_notification_formatting);
    RUN_TEST(test_ocpp_heartbeat_and_status_formatting);
    RUN_TEST(test_ocpp_transaction_formatting);
    RUN_TEST(test_ocpp_meter_values_formatting);
    RUN_TEST(test_ocpp_process_call_result);
    RUN_TEST(test_ocpp_process_remote_start_stop);
    RUN_TEST(test_ocpp_tick);
    RUN_TEST(test_ocpp_all_status_enums_and_small_buffers);
    RUN_TEST(test_ocpp_process_message_extended);
    RUN_TEST(test_ocpp_server_role);
    RUN_TEST(test_ocpp_call_error_formatting);
    RUN_TEST(test_ocpp_21_features);
}
