#include "mock_port.h"
#include "syntropic/proto/syn_canopen.h"
#include "syntropic/proto/syn_cia402.h"
#include "unity/unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void setUp(void)
{
}
void tearDown(void)
{
}

void test_canopen_cia402_integration(void)
{
    printf("[Integration Test] Testing CANopen CiA 402 Drive Profile State Machine...\n");

    static uint32_t dev_type = 0x00020192;
    SYN_CANOpenODEntry od_table[] = {
        {0x1000, 0x00, 0x07, SYN_CANOPEN_ACCESS_RO, &dev_type, sizeof(dev_type)}};

    SYN_CANOpenNode node;
    SYN_CANOpenNodeConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.node_id = 1;
    cfg.heartbeat_ms = 1000;

    SYN_Status status = syn_canopen_init(&node, &cfg, od_table, 1);
    TEST_ASSERT_EQUAL_INT(SYN_OK, status);

    /* Process NMT Command (COB-ID 0x000, Payload {0x01, 0x01} -> Start Node 1) */
    uint8_t nmt_cmd[2] = {0x01, 0x01};
    status = syn_canopen_process_rx(&node, 0x000, nmt_cmd, sizeof(nmt_cmd));
    TEST_ASSERT_EQUAL_INT(SYN_OK, status);
    TEST_ASSERT_EQUAL_INT(SYN_CANOPEN_NMT_STATE_OPERATIONAL, node.nmt_state);

    /* Initialize CiA 402 Motion Drive Profile */
    SYN_CiA402Drive drive;
    SYN_CiA402Config drive_cfg;
    memset(&drive_cfg, 0, sizeof(drive_cfg));
    drive_cfg.max_profile_velocity = 1000;
    drive_cfg.profile_acceleration = 500;

    status = syn_cia402_init(&drive, &drive_cfg);
    TEST_ASSERT_EQUAL_INT(SYN_OK, status);

    /* Controlword Shutdown: Bits (ENABLE_VOLTAGE | QUICK_STOP) = 0x0006 */
    uint16_t cw_shutdown = SYN_CIA402_CW_ENABLE_VOLTAGE | SYN_CIA402_CW_QUICK_STOP;
    status = syn_cia402_set_controlword(&drive, cw_shutdown);
    TEST_ASSERT_EQUAL_INT(SYN_OK, status);

    uint16_t statusword = syn_cia402_get_statusword(&drive);
    printf("[Integration Test] CiA 402 Statusword: 0x%04X\n", statusword);

    /* Controlword Enable Operation: Bits (SWITCH_ON | ENABLE_VOLTAGE | QUICK_STOP | ENABLE_OP) =
     * 0x000F */
    uint16_t cw_enable = SYN_CIA402_CW_SWITCH_ON | SYN_CIA402_CW_ENABLE_VOLTAGE |
                         SYN_CIA402_CW_QUICK_STOP | SYN_CIA402_CW_ENABLE_OP;
    status = syn_cia402_set_controlword(&drive, cw_enable);
    TEST_ASSERT_EQUAL_INT(SYN_OK, status);
    TEST_ASSERT_EQUAL_INT(SYN_CIA402_STATE_OPERATION_ENABLED, drive.state);

    /* Quick Stop Transition: Active Low bit 2 cleared -> 0x000B */
    uint16_t cw_quickstop =
        SYN_CIA402_CW_SWITCH_ON | SYN_CIA402_CW_ENABLE_VOLTAGE | SYN_CIA402_CW_ENABLE_OP;
    status = syn_cia402_set_controlword(&drive, cw_quickstop);
    TEST_ASSERT_EQUAL_INT(SYN_OK, status);
    TEST_ASSERT_EQUAL_INT(SYN_CIA402_STATE_QUICK_STOP_ACTIVE, drive.state);

    /* Trigger Fault condition */
    status = syn_cia402_trigger_fault(&drive, 0x2310);
    TEST_ASSERT_EQUAL_INT(SYN_OK, status);
    TEST_ASSERT_EQUAL_INT(SYN_CIA402_STATE_FAULT, drive.state);
    statusword = syn_cia402_get_statusword(&drive);
    TEST_ASSERT_TRUE((statusword & SYN_CIA402_SW_FAULT) != 0);

    /* Fault Reset Transition (Bit 7 rising edge 0 -> 1) */
    status = syn_cia402_set_controlword(&drive, SYN_CIA402_CW_FAULT_RESET);
    TEST_ASSERT_EQUAL_INT(SYN_OK, status);
    TEST_ASSERT_EQUAL_INT(SYN_CIA402_STATE_SWITCH_ON_DISABLED, drive.state);

    printf("[Integration Test] End-to-End CANopen CiA 402 Integration PASS!\n");
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_canopen_cia402_integration);
    return UNITY_END();
}
