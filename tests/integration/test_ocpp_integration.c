/**
 * @file test_ocpp_integration.c
 * @brief Containerized 3rd-Party OCPP 1.6-J Integration Test Driver for SyntropicOS.
 *
 * Tests syn_ocpp EVSE Client and CSMS Server roles against official 3rd-party
 * Python OCPP reference services over POSIX TCP sockets / WebSockets.
 */

#include "syntropic/net/syn_websocket.h"
#include "syntropic/proto/syn_ocpp.h"
#include "unity/unity.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static SYN_OCPP_Client g_client;
static SYN_OCPP_Server g_server;

void setUp(void)
{
    syn_ocpp_init(&g_client);
    syn_ocpp_server_init(&g_server);
}

void tearDown(void)
{
}

static int connect_tcp(const char *host, uint16_t port)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
        return -1;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, host, &addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }
    return sock;
}

static const char *recv_ocpp_json(int sock, char *buf, size_t buf_size)
{
    size_t total = 0;
    while (total < buf_size - 1) {
        ssize_t n = recv(sock, buf + total, buf_size - 1 - total, 0);
        if (n <= 0) {
            printf("[Integration C] recv returned n=%zd\n", n);
            break;
        }
        total += n;
        buf[total] = '\0';
        const char *json = strchr(buf, '[');
        if (json != NULL) {
            return json;
        }
    }
    printf("[Integration C] recv_ocpp_json failed, total=%zu, buf='%.100s'\n", total, buf);
    return NULL;
}

static size_t format_client_ws_frame(const char *payload, size_t payload_len, char *out_frame)
{
    uint8_t mask_key[4] = {0x12, 0x34, 0x56, 0x78};
    size_t header_len = 0;
    out_frame[0] = (char)0x81; /* FIN + Text Frame */
    if (payload_len < 126) {
        out_frame[1] = (char)(0x80 | payload_len);
        memcpy(out_frame + 2, mask_key, 4);
        header_len = 6;
    } else {
        out_frame[1] = (char)(0x80 | 126);
        out_frame[2] = (char)((payload_len >> 8) & 0xFF);
        out_frame[3] = (char)(payload_len & 0xFF);
        memcpy(out_frame + 4, mask_key, 4);
        header_len = 8;
    }
    for (size_t i = 0; i < payload_len; i++) {
        out_frame[header_len + i] = payload[i] ^ mask_key[i % 4];
    }
    return payload_len + header_len;
}

void test_ocpp_client_against_python_csms(void)
{
    int sock = connect_tcp("127.0.0.1", 9001);
    if (sock < 0) {
        TEST_IGNORE_MESSAGE("Python CSMS server on port 9001 not reachable");
        return;
    }

    /* 1. Send WebSocket Upgrade Request */
    const char *ws_handshake = "GET /CP001 HTTP/1.1\r\n"
                               "Host: 127.0.0.1:9001\r\n"
                               "Upgrade: websocket\r\n"
                               "Connection: Upgrade\r\n"
                               "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                               "Sec-WebSocket-Protocol: ocpp1.6\r\n"
                               "Sec-WebSocket-Version: 13\r\n\r\n";

    send(sock, ws_handshake, strlen(ws_handshake), 0);

    char rx_buf[2048];
    ssize_t n = recv(sock, rx_buf, sizeof(rx_buf) - 1, 0);
    TEST_ASSERT_TRUE(n > 0);
    rx_buf[n] = '\0';
    TEST_ASSERT_NOT_NULL(strstr(rx_buf, "101 Switching Protocols"));

    char tx_payload[512];
    char ws_frame[1024];
    size_t payload_len = 0;

    /* 2. Send BootNotification */
    SYN_OCPP_ChargePointInfo info = {"SyntropicOS-Vendor", "EVSE-v1", "SN-1001", "v1.0.0"};
    TEST_ASSERT_EQUAL(SYN_OK, syn_ocpp_format_boot_notification(&g_client, &info, tx_payload,
                                                                sizeof(tx_payload), &payload_len));

    size_t frame_len = format_client_ws_frame(tx_payload, payload_len, ws_frame);
    send(sock, ws_frame, frame_len, 0);

    const char *json = recv_ocpp_json(sock, rx_buf, sizeof(rx_buf));
    TEST_ASSERT_NOT_NULL(json);
    TEST_ASSERT_EQUAL(SYN_OK,
                      syn_ocpp_process_message(&g_client, json, strlen(json), NULL, 0, NULL));
    TEST_ASSERT_EQUAL(SYN_OCPP_REGISTRATION_ACCEPTED, g_client.registration_status);

    /* 3. Send Heartbeat */
    TEST_ASSERT_EQUAL(
        SYN_OK, syn_ocpp_format_heartbeat(&g_client, tx_payload, sizeof(tx_payload), &payload_len));
    frame_len = format_client_ws_frame(tx_payload, payload_len, ws_frame);
    send(sock, ws_frame, frame_len, 0);

    json = recv_ocpp_json(sock, rx_buf, sizeof(rx_buf));
    TEST_ASSERT_NOT_NULL(json);

    /* 4. Send StatusNotification */
    TEST_ASSERT_EQUAL(SYN_OK, syn_ocpp_format_status_notification(
                                  &g_client, 1, SYN_OCPP_STATUS_CHARGING, "NoError", tx_payload,
                                  sizeof(tx_payload), &payload_len));
    frame_len = format_client_ws_frame(tx_payload, payload_len, ws_frame);
    send(sock, ws_frame, frame_len, 0);

    json = recv_ocpp_json(sock, rx_buf, sizeof(rx_buf));
    TEST_ASSERT_NOT_NULL(json);

    /* 5. Send Authorize */
    TEST_ASSERT_EQUAL(SYN_OK, syn_ocpp_format_authorize(&g_client, "RFID-TAG-123", tx_payload,
                                                        sizeof(tx_payload), &payload_len));
    frame_len = format_client_ws_frame(tx_payload, payload_len, ws_frame);
    send(sock, ws_frame, frame_len, 0);

    json = recv_ocpp_json(sock, rx_buf, sizeof(rx_buf));
    TEST_ASSERT_NOT_NULL(json);

    /* 6. Send StartTransaction */
    TEST_ASSERT_EQUAL(SYN_OK, syn_ocpp_format_start_transaction(&g_client, 1, "RFID-TAG-123", 1000,
                                                                tx_payload, sizeof(tx_payload),
                                                                &payload_len));
    frame_len = format_client_ws_frame(tx_payload, payload_len, ws_frame);
    send(sock, ws_frame, frame_len, 0);

    json = recv_ocpp_json(sock, rx_buf, sizeof(rx_buf));
    TEST_ASSERT_NOT_NULL(json);
    TEST_ASSERT_EQUAL(SYN_OK,
                      syn_ocpp_process_message(&g_client, json, strlen(json), NULL, 0, NULL));
    TEST_ASSERT_EQUAL(1001, g_client.active_transaction_id);

    /* 7. Send MeterValues */
    SYN_OCPP_MeterValues meter = {5000, 230, 160, 3680, 50};
    TEST_ASSERT_EQUAL(SYN_OK, syn_ocpp_format_meter_values(&g_client, 1, &meter, tx_payload,
                                                           sizeof(tx_payload), &payload_len));
    frame_len = format_client_ws_frame(tx_payload, payload_len, ws_frame);
    send(sock, ws_frame, frame_len, 0);

    json = recv_ocpp_json(sock, rx_buf, sizeof(rx_buf));
    TEST_ASSERT_NOT_NULL(json);

    /* 8. Send StopTransaction */
    TEST_ASSERT_EQUAL(SYN_OK, syn_ocpp_format_stop_transaction(&g_client, 1001, 15000,
                                                               "EVDisconnected", tx_payload,
                                                               sizeof(tx_payload), &payload_len));
    frame_len = format_client_ws_frame(tx_payload, payload_len, ws_frame);
    send(sock, ws_frame, frame_len, 0);

    json = recv_ocpp_json(sock, rx_buf, sizeof(rx_buf));
    TEST_ASSERT_NOT_NULL(json);

    /* 7. Send OCPP 2.1 DisplayMessage */
    SYN_OCPP_DisplayMessage disp = {201, "Rate Alert", "Off-peak energy tariff active", 60};
    TEST_ASSERT_EQUAL(SYN_OK, syn_ocpp_format_display_message(&g_client, &disp, tx_payload,
                                                              sizeof(tx_payload), &payload_len));
    frame_len = format_client_ws_frame(tx_payload, payload_len, ws_frame);
    send(sock, ws_frame, frame_len, 0);

    json = recv_ocpp_json(sock, rx_buf, sizeof(rx_buf));
    TEST_ASSERT_NOT_NULL(json);

    /* 8. Send OCPP 2.1 ISO 15118-20 V2G Energy Transfer */
    TEST_ASSERT_EQUAL(
        SYN_OK, syn_ocpp_format_v2g_energy_transfer(&g_client, SYN_OCPP_V2G_DYNAMIC, -10000,
                                                    tx_payload, sizeof(tx_payload), &payload_len));
    frame_len = format_client_ws_frame(tx_payload, payload_len, ws_frame);
    send(sock, ws_frame, frame_len, 0);

    json = recv_ocpp_json(sock, rx_buf, sizeof(rx_buf));
    TEST_ASSERT_NOT_NULL(json);

    close(sock);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_ocpp_client_against_python_csms);
    return UNITY_END();
}
