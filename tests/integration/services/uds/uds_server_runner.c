#include "syntropic/crypto/syn_aes128.h"
#include "syntropic/proto/syn_uds.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static uint8_t g_vin[17] = "SYN12345678901234";
static uint8_t g_part_no[11] = "SYN-UDS-001";
static uint8_t g_sys_status[4] = {0x11, 0x22, 0x33, 0x44};
static uint8_t g_prog_did[2] = {0xAA, 0xBB};

static uint8_t g_memory_store[256];

static bool memory_handler_cb(bool is_write, uint32_t address, uint32_t size, uint8_t *data,
                              void *user_data)
{
    (void)user_data;
    if (address + size > sizeof(g_memory_store)) {
        return false;
    }
    if (is_write) {
        memcpy(&g_memory_store[address], data, size);
    } else {
        memcpy(data, &g_memory_store[address], size);
    }
    return true;
}

static bool comm_control_cb(SYN_UDS_CommControlType ctrl_type, uint8_t comm_type, void *user_data)
{
    (void)ctrl_type;
    (void)comm_type;
    (void)user_data;
    return true;
}

static bool access_timing_cb(SYN_UDS_AccessTimingType timing_type, uint16_t *p2_max_ms,
                             uint16_t *p2_star_max_10ms, void *user_data)
{
    (void)timing_type;
    if (p2_max_ms != NULL)
        *p2_max_ms = 50U;
    if (p2_star_max_10ms != NULL)
        *p2_star_max_10ms = 500U;
    (void)user_data;
    return true;
}

static bool auth_cb(uint8_t sub_func, const uint8_t *req, uint16_t req_len, uint8_t *resp,
                    uint16_t max_resp_len, uint16_t *resp_len, void *user_data)
{
    (void)sub_func;
    (void)req;
    (void)req_len;
    (void)max_resp_len;
    (void)user_data;
    resp[0] = 0x69;
    resp[1] = 0x00;
    *resp_len = 2;
    return true;
}

int main(void)
{
    printf(
        "[SyntropicOS UDS C Server] Starting ISO 14229-1 UDS Server Daemon on 0.0.0.0:10886...\n");

    SYN_UDS_Server server;
    if (!syn_uds_init(&server)) {
        fprintf(stderr, "[SyntropicOS UDS C Server] Failed to initialize UDS server!\n");
        return 1;
    }

    /* Register standard automotive DIDs */
    syn_uds_register_did(&server, 0xF190U, g_vin, sizeof(g_vin), false);
    syn_uds_register_did(&server, 0xF187U, g_part_no, sizeof(g_part_no), false);
    syn_uds_register_did(&server, 0x0100U, g_sys_status, sizeof(g_sys_status), true);

    /* Register DID 0x0300 with Programming Session mask only (Issue #87) */
    syn_uds_register_did_ext(&server, 0x0300U, g_prog_did, sizeof(g_prog_did), true,
                             SYN_UDS_SESSION_MASK_PROGRAMMING, SYN_UDS_SECURITY_MASK_ALL);

    /* Register DTC 0x123456 */
    syn_uds_register_dtc(&server, 0x123456U, 0x24U, 0x40U);

    /* Register Handlers */
    syn_uds_register_memory_handler(&server, memory_handler_cb, NULL);
    syn_uds_register_comm_control(&server, comm_control_cb, NULL);
    syn_uds_register_access_timing(&server, access_timing_cb, NULL);
    syn_uds_register_auth_handler(&server, auth_cb, NULL);

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(10886);

    if (bind(listen_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("bind");
        close(listen_fd);
        return 1;
    }

    if (listen(listen_fd, 5) < 0) {
        perror("listen");
        close(listen_fd);
        return 1;
    }

    printf("[SyntropicOS UDS C Server] Listening on port 10886...\n");

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int conn_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (conn_fd < 0) {
            perror("accept");
            continue;
        }

        printf("[SyntropicOS UDS C Server] Client connected!\n");

        uint8_t rx_buf[1024];
        uint8_t tx_buf[1024];

        while (1) {
            ssize_t n = recv(conn_fd, rx_buf, sizeof(rx_buf), 0);
            if (n <= 0) {
                break;
            }

            uint16_t resp_len = 0;
            bool ok = syn_uds_process_request(&server, rx_buf, (uint16_t)n, tx_buf, sizeof(tx_buf),
                                              &resp_len, SYN_UDS_ADDR_PHYSICAL);
            if (ok && resp_len > 0) {
                send(conn_fd, tx_buf, resp_len, 0);
            }
        }

        close(conn_fd);
        printf("[SyntropicOS UDS C Server] Client disconnected.\n");
    }

    close(listen_fd);
    return 0;
}
