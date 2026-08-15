#include "mock_port.h"
#include "syntropic/proto/syn_modbus.h"
#include "syntropic/util/syn_pack.h"
#include "unity/unity.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

void setUp(void)
{
}
void tearDown(void)
{
}

void test_modbus_tcp_integration(void)
{
    const char *host = getenv("MODBUS_HOST");
    if (!host)
        host = "127.0.0.1";
    uint16_t port = 5020;

    printf("[Integration Test] Connecting to Modbus TCP Server at %s:%d...\n", host, port);

    int sock = syn_port_sock_connect_host(host, port);
    if (sock < 0) {
        printf("[Integration Test] Notice: Modbus TCP server at %s:%d not reachable (skipping "
               "loopback test)\n",
               host, port);
        return;
    }
    printf("[Integration Test] Connected to Modbus TCP Server!\n");

    /* 1. Read Holding Registers (FC 0x03, Reg 0, Count 2) */
    uint8_t req[12] = {
        0x00,
        0x01, /* TxID */
        0x00,
        0x00, /* ProtoID */
        0x00,
        0x06,                   /* Length */
        0x01,                   /* UnitID */
        SYN_MB_FC_READ_HOLDING, /* FC 0x03 */
        0x00,
        0x00, /* Start Address */
        0x00,
        0x02 /* Count */
    };

    ssize_t sent = send(sock, req, sizeof(req), 0);
    TEST_ASSERT_EQUAL_INT(sizeof(req), sent);

    uint8_t resp[32];
    ssize_t recvd = recv(sock, resp, sizeof(resp), 0);
    TEST_ASSERT_TRUE(recvd >= 11);

    /* Verify MBAP & FC */
    TEST_ASSERT_EQUAL_UINT8(0x01, resp[6]);                   /* UnitID */
    TEST_ASSERT_EQUAL_UINT8(SYN_MB_FC_READ_HOLDING, resp[7]); /* FC 0x03 */
    TEST_ASSERT_EQUAL_UINT8(4, resp[8]);                      /* Byte count: 4 bytes for 2 regs */

    /* Reg 0 = 0x1234, Reg 1 = 0x5678 */
    uint16_t reg0 = (resp[9] << 8) | resp[10];
    uint16_t reg1 = (resp[11] << 8) | resp[12];

    printf("[Integration Test] Read Holding Reg 0: 0x%04X, Reg 1: 0x%04X\n", reg0, reg1);
    TEST_ASSERT_EQUAL_HEX16(0x1234, reg0);
    TEST_ASSERT_EQUAL_HEX16(0x5678, reg1);

    /* 2. Write Single Register (FC 0x06, Reg 2 = 0xCAFE) */
    uint8_t write_req[12] = {
        0x00,
        0x02, /* TxID */
        0x00,
        0x00, /* ProtoID */
        0x00,
        0x06,                   /* Length */
        0x01,                   /* UnitID */
        SYN_MB_FC_WRITE_SINGLE, /* FC 0x06 */
        0x00,
        0x02, /* Register Address 2 */
        0xCA,
        0xFE /* Value 0xCAFE */
    };
    sent = send(sock, write_req, sizeof(write_req), 0);
    TEST_ASSERT_EQUAL_INT(sizeof(write_req), sent);

    recvd = recv(sock, resp, sizeof(resp), 0);
    TEST_ASSERT_EQUAL_INT(12, recvd);
    TEST_ASSERT_EQUAL_UINT8(SYN_MB_FC_WRITE_SINGLE, resp[7]);
    TEST_ASSERT_EQUAL_UINT8(0xCA, resp[10]);
    TEST_ASSERT_EQUAL_UINT8(0xFE, resp[11]);

    /* Read back Reg 2 to verify persistence */
    uint8_t read_req2[12] = {0x00, 0x03, 0x00, 0x00, 0x00, 0x06, 0x01, SYN_MB_FC_READ_HOLDING,
                             0x00, 0x02, 0x00, 0x01};
    send(sock, read_req2, sizeof(read_req2), 0);
    recvd = recv(sock, resp, sizeof(resp), 0);
    TEST_ASSERT_TRUE(recvd >= 9);
    uint16_t reg2 = (resp[9] << 8) | resp[10];
    printf("[Integration Test] Verified Written Reg 2: 0x%04X\n", reg2);
    TEST_ASSERT_EQUAL_HEX16(0xCAFE, reg2);

    /* 3. Write Single Coil (FC 0x05, Coil 1 = ON 0xFF00) */
    uint8_t coil_req[12] = {
        0x00,
        0x04, /* TxID */
        0x00,
        0x00, /* ProtoID */
        0x00,
        0x06,                        /* Length */
        0x01,                        /* UnitID */
        SYN_MB_FC_WRITE_SINGLE_COIL, /* FC 0x05 */
        0x00,
        0x01, /* Coil Address 1 */
        0xFF,
        0x00 /* ON */
    };
    sent = send(sock, coil_req, sizeof(coil_req), 0);
    TEST_ASSERT_EQUAL_INT(sizeof(coil_req), sent);
    recvd = recv(sock, resp, sizeof(resp), 0);
    TEST_ASSERT_EQUAL_INT(12, recvd);
    TEST_ASSERT_EQUAL_UINT8(SYN_MB_FC_WRITE_SINGLE_COIL, resp[7]);
    TEST_ASSERT_EQUAL_UINT8(0xFF, resp[10]);

    /* 4. Exception Response for unsupported FC (e.g. FC 0x2B) */
    uint8_t invalid_req[12] = {0x00, 0x05, 0x00, 0x00, 0x00, 0x06,
                               0x01, 0x2B, 0x00, 0x00, 0x00, 0x01};
    send(sock, invalid_req, sizeof(invalid_req), 0);
    recvd = recv(sock, resp, sizeof(resp), 0);
    TEST_ASSERT_TRUE(recvd >= 9);
    TEST_ASSERT_EQUAL_UINT8(0x2B | 0x80, resp[7]); /* Error FC 0xAB */
    TEST_ASSERT_EQUAL_UINT8(0x01, resp[8]);        /* Exception code: Illegal function */

    close(sock);
    printf("[Integration Test] End-to-End Modbus TCP Integration PASS!\n");
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_modbus_tcp_integration);
    return UNITY_END();
}
