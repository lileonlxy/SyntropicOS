/**
 * @file test_ccp.c
 * @brief Unit tests for ASAM CCP v2.1 slave protocol implementation.
 */

#include "syntropic/proto/syn_ccp.h"
#include "unity/unity.h"

#include <string.h>

static SYN_CCP_Slave g_ccp_slave;

static void test_ccp_init_and_connect_disconnect(void)
{
    syn_ccp_init(&g_ccp_slave, 0x1234U);
    uint8_t cro[8] = {0};
    uint8_t dto[8] = {0};

    /* Attempt command prior to connection -> fails with NOT_CONNECTED */
    cro[0] = SYN_CCP_CMD_GET_CCP_VERSION;
    cro[1] = 0x01;
    TEST_ASSERT_TRUE(syn_ccp_process_cro(&g_ccp_slave, cro, dto));
    TEST_ASSERT_EQUAL_HEX8(SYN_CCP_PID_CRM, dto[0]);
    TEST_ASSERT_EQUAL_HEX8(SYN_CCP_ERR_NOT_CONNECTED, dto[1]);
    TEST_ASSERT_EQUAL_HEX8(0x01, dto[2]);

    /* CONNECT with station address mismatch -> ignored */
    cro[0] = SYN_CCP_CMD_CONNECT;
    cro[1] = 0x02;
    cro[2] = 0x99;
    cro[3] = 0x99;
    TEST_ASSERT_FALSE(syn_ccp_process_cro(&g_ccp_slave, cro, dto));
    TEST_ASSERT_FALSE(g_ccp_slave.connected);

    /* CONNECT with matching station address -> SUCCESS */
    cro[2] = 0x34;
    cro[3] = 0x12;
    TEST_ASSERT_TRUE(syn_ccp_process_cro(&g_ccp_slave, cro, dto));
    TEST_ASSERT_EQUAL_HEX8(SYN_CCP_PID_CRM, dto[0]);
    TEST_ASSERT_EQUAL_HEX8(SYN_CCP_ERR_SUCCESS, dto[1]);
    TEST_ASSERT_EQUAL_HEX8(0x02, dto[2]);
    TEST_ASSERT_TRUE(g_ccp_slave.connected);

    /* GET_CCP_VERSION */
    cro[0] = SYN_CCP_CMD_GET_CCP_VERSION;
    cro[1] = 0x03;
    TEST_ASSERT_TRUE(syn_ccp_process_cro(&g_ccp_slave, cro, dto));
    TEST_ASSERT_EQUAL_HEX8(SYN_CCP_ERR_SUCCESS, dto[1]);
    TEST_ASSERT_EQUAL_HEX8(0x02, dto[3]); /* Main version 2 */
    TEST_ASSERT_EQUAL_HEX8(0x01, dto[4]); /* Release version 1 */

    /* EXCHANGE_ID */
    cro[0] = SYN_CCP_CMD_EXCHANGE_ID;
    cro[1] = 0x04;
    TEST_ASSERT_TRUE(syn_ccp_process_cro(&g_ccp_slave, cro, dto));
    TEST_ASSERT_EQUAL_HEX8(SYN_CCP_ERR_SUCCESS, dto[1]);
    TEST_ASSERT_EQUAL_HEX8(0x08, dto[3]);

    /* DISCONNECT */
    cro[0] = SYN_CCP_CMD_DISCONNECT;
    cro[1] = 0x05;
    cro[2] = 0x00; /* Temporary disconnect */
    cro[3] = 0x34;
    cro[4] = 0x12;
    TEST_ASSERT_TRUE(syn_ccp_process_cro(&g_ccp_slave, cro, dto));
    TEST_ASSERT_EQUAL_HEX8(SYN_CCP_ERR_SUCCESS, dto[1]);
    TEST_ASSERT_FALSE(g_ccp_slave.connected);
}

static void test_ccp_mta_upload_download(void)
{
    syn_ccp_init(&g_ccp_slave, 0x1234U);
    uint8_t cro[8] = {0};
    uint8_t dto[8] = {0};
    uint8_t buffer[16] = {0};

    /* Connect */
    cro[0] = SYN_CCP_CMD_CONNECT;
    cro[1] = 0x01;
    cro[2] = 0x34;
    cro[3] = 0x12;
    syn_ccp_process_cro(&g_ccp_slave, cro, dto);

    /* SET_MTA1 via CRO */
    cro[0] = SYN_CCP_CMD_SET_MTA;
    cro[1] = 0x02;
    cro[2] = 0x01; /* MTA1 */
    cro[3] = 0x02; /* Ext */
    cro[4] = 0;
    cro[5] = 0;
    cro[6] = 0;
    cro[7] = 0;
    TEST_ASSERT_TRUE(syn_ccp_process_cro(&g_ccp_slave, cro, dto));
    TEST_ASSERT_EQUAL_HEX8(SYN_CCP_ERR_SUCCESS, dto[1]);
    TEST_ASSERT_EQUAL_HEX8(0x02, g_ccp_slave.mta1_ext);

    /* SET_MTA0 via CRO with non-zero dummy low bits */
    cro[2] = 0x00; /* MTA0 */
    cro[4] = 0x10;
    TEST_ASSERT_TRUE(syn_ccp_process_cro(&g_ccp_slave, cro, dto));
    TEST_ASSERT_EQUAL_HEX8(SYN_CCP_ERR_SUCCESS, dto[1]);

    /* Restore MTA0 address */
    uintptr_t target_addr = (uintptr_t)buffer;
    syn_ccp_set_mta(&g_ccp_slave, 0, 0x01, target_addr);

    /* DNLOAD 4 bytes */
    cro[0] = SYN_CCP_CMD_DNLOAD;
    cro[1] = 0x03;
    cro[2] = 0x04; /* Size */
    cro[3] = 0xAA;
    cro[4] = 0xBB;
    cro[5] = 0xCC;
    cro[6] = 0xDD;
    TEST_ASSERT_TRUE(syn_ccp_process_cro(&g_ccp_slave, cro, dto));
    TEST_ASSERT_EQUAL_HEX8(SYN_CCP_ERR_SUCCESS, dto[1]);
    TEST_ASSERT_EQUAL_HEX8(0xAA, buffer[0]);
    TEST_ASSERT_EQUAL_HEX8(0xBB, buffer[1]);
    TEST_ASSERT_EQUAL_HEX8(0xCC, buffer[2]);
    TEST_ASSERT_EQUAL_HEX8(0xDD, buffer[3]);

    /* DNLOAD_6 */
    cro[0] = SYN_CCP_CMD_DNLOAD_6;
    cro[1] = 0x04;
    cro[2] = 0x11;
    cro[3] = 0x22;
    cro[4] = 0x33;
    cro[5] = 0x44;
    cro[6] = 0x55;
    cro[7] = 0x66;
    TEST_ASSERT_TRUE(syn_ccp_process_cro(&g_ccp_slave, cro, dto));
    TEST_ASSERT_EQUAL_HEX8(SYN_CCP_ERR_SUCCESS, dto[1]);
    TEST_ASSERT_EQUAL_HEX8(0x11, buffer[4]);
    TEST_ASSERT_EQUAL_HEX8(0x66, buffer[9]);

    /* UPLOAD */
    syn_ccp_set_mta(&g_ccp_slave, 0, 0x01, (uintptr_t)buffer);
    cro[0] = SYN_CCP_CMD_UPLOAD;
    cro[1] = 0x05;
    cro[2] = 0x04;
    TEST_ASSERT_TRUE(syn_ccp_process_cro(&g_ccp_slave, cro, dto));
    TEST_ASSERT_EQUAL_HEX8(SYN_CCP_ERR_SUCCESS, dto[1]);
    TEST_ASSERT_EQUAL_HEX8(0xAA, dto[3]);
    TEST_ASSERT_EQUAL_HEX8(0xBB, dto[4]);
    TEST_ASSERT_EQUAL_HEX8(0xCC, dto[5]);
    TEST_ASSERT_EQUAL_HEX8(0xDD, dto[6]);

    /* SHORT_UP via CRO */
    syn_ccp_set_mta(&g_ccp_slave, 0, 0x01, (uintptr_t)(buffer + 4));
    cro[0] = SYN_CCP_CMD_SHORT_UP;
    cro[1] = 0x06;
    cro[2] = 0x02; /* Size */
    cro[3] = 0x01; /* Ext */
    cro[4] = 0;
    cro[5] = 0;
    cro[6] = 0;
    cro[7] = 0;
    TEST_ASSERT_TRUE(syn_ccp_process_cro(&g_ccp_slave, cro, dto));
    TEST_ASSERT_EQUAL_HEX8(SYN_CCP_ERR_SUCCESS, dto[1]);
    TEST_ASSERT_EQUAL_HEX8(0x11, dto[3]);
    TEST_ASSERT_EQUAL_HEX8(0x22, dto[4]);

    /* SHORT_UP with mta0_addr = 0 and non-zero addr offset with size = 0 */
    g_ccp_slave.mta0_addr = 0;
    cro[2] = 0x00; /* Size = 0 */
    cro[4] = 0x10;
    TEST_ASSERT_TRUE(syn_ccp_process_cro(&g_ccp_slave, cro, dto));
    TEST_ASSERT_EQUAL_HEX8(SYN_CCP_ERR_SUCCESS, dto[1]);

    /* Null MTA setter test */
    syn_ccp_set_mta(NULL, 0, 0, 0);

    /* DISCONNECT with address mismatch (0x5555) */
    cro[0] = SYN_CCP_CMD_DISCONNECT;
    cro[1] = 0x99;
    cro[2] = 0x00;
    cro[3] = 0x55;
    cro[4] = 0x55;
    TEST_ASSERT_TRUE(syn_ccp_process_cro(&g_ccp_slave, cro, dto));
    TEST_ASSERT_EQUAL_HEX8(SYN_CCP_ERR_SUCCESS, dto[1]);
    TEST_ASSERT_TRUE(g_ccp_slave.connected); /* Still connected due to mismatch */
}

static void test_ccp_daq_list_streaming(void)
{
    syn_ccp_init(&g_ccp_slave, 0x1234U);
    uint8_t cro[8] = {0};
    uint8_t dto[8] = {0};
    uint32_t sample_val = 0x12345678U;

    /* Connect */
    cro[0] = SYN_CCP_CMD_CONNECT;
    cro[1] = 0x01;
    cro[2] = 0x34;
    cro[3] = 0x12;
    syn_ccp_process_cro(&g_ccp_slave, cro, dto);

    /* GET_DAQ_SIZE */
    cro[0] = SYN_CCP_CMD_GET_DAQ_SIZE;
    cro[1] = 0x02;
    cro[2] = 0x00; /* DAQ List 0 */
    TEST_ASSERT_TRUE(syn_ccp_process_cro(&g_ccp_slave, cro, dto));
    TEST_ASSERT_EQUAL_HEX8(SYN_CCP_ERR_SUCCESS, dto[1]);
    TEST_ASSERT_EQUAL_HEX8(SYN_CCP_MAX_ODT_PER_DAQ, dto[3]);

    /* SET_DAQ_SIZE */
    cro[0] = SYN_CCP_CMD_SET_DAQ_SIZE;
    cro[1] = 0x03;
    cro[2] = 0x00; /* DAQ 0 */
    cro[4] = 0x02; /* 2 ODTs */
    TEST_ASSERT_TRUE(syn_ccp_process_cro(&g_ccp_slave, cro, dto));
    TEST_ASSERT_EQUAL_HEX8(SYN_CCP_ERR_SUCCESS, dto[1]);

    /* BUILD_PKT (Add ODT 0 entry 0) */
    uintptr_t target_addr = (uintptr_t)&sample_val;
    g_ccp_slave.daq_lists[0].odts[0].entries[0].address = target_addr;
    g_ccp_slave.daq_lists[0].odts[0].entries[0].size = 4;
    g_ccp_slave.daq_lists[0].odts[0].entry_count = 1;

    /* START_STOP (Start DAQ 0) */
    cro[0] = SYN_CCP_CMD_START_STOP;
    cro[1] = 0x05;
    cro[2] = 0x01; /* Start */
    cro[3] = 0x00; /* DAQ 0 */
    cro[5] = 0x01; /* Event channel 1 */
    cro[6] = 0x01; /* Prescaler 1 (LSB) */
    cro[7] = 0x00; /* Prescaler 1 (MSB) */
    TEST_ASSERT_TRUE(syn_ccp_process_cro(&g_ccp_slave, cro, dto));
    TEST_ASSERT_EQUAL_HEX8(SYN_CCP_ERR_SUCCESS, dto[1]);

    /* Service DAQ tick */
    uint8_t list_idx = 0;
    uint8_t odt_idx = 0;
    TEST_ASSERT_TRUE(syn_ccp_service_daq(&g_ccp_slave, 0x01, dto, &list_idx, &odt_idx));
    TEST_ASSERT_EQUAL_HEX8(0x00, dto[0]); /* Packet ID 0 */
    TEST_ASSERT_EQUAL_HEX8(0x78, dto[1]);
    TEST_ASSERT_EQUAL_HEX8(0x56, dto[2]);
    TEST_ASSERT_EQUAL_HEX8(0x34, dto[3]);
    TEST_ASSERT_EQUAL_HEX8(0x12, dto[4]);

    /* START_STOP_ALL (Stop all) */
    cro[0] = SYN_CCP_CMD_START_STOP_ALL;
    cro[1] = 0x06;
    cro[2] = 0x00; /* Stop */
    TEST_ASSERT_TRUE(syn_ccp_process_cro(&g_ccp_slave, cro, dto));
    TEST_ASSERT_EQUAL_HEX8(SYN_CCP_ERR_SUCCESS, dto[1]);
    TEST_ASSERT_FALSE(syn_ccp_service_daq(&g_ccp_slave, 0x01, dto, &list_idx, &odt_idx));
}

static void test_ccp_seed_unlock_and_cal_page(void)
{
    syn_ccp_init(&g_ccp_slave, 0x1234U);
    uint8_t cro[8] = {0};
    uint8_t dto[8] = {0};

    /* Connect */
    cro[0] = SYN_CCP_CMD_CONNECT;
    cro[1] = 0x01;
    cro[2] = 0x34;
    cro[3] = 0x12;
    syn_ccp_process_cro(&g_ccp_slave, cro, dto);

    /* GET_SEED */
    cro[0] = SYN_CCP_CMD_GET_SEED;
    cro[1] = 0x02;
    cro[2] = SYN_CCP_RESOURCE_CAL;
    TEST_ASSERT_TRUE(syn_ccp_process_cro(&g_ccp_slave, cro, dto));
    TEST_ASSERT_EQUAL_HEX8(SYN_CCP_ERR_SUCCESS, dto[1]);
    TEST_ASSERT_EQUAL_HEX8(0x00, dto[3]); /* Resource already unlocked */

    cro[2] = 0x80; /* Locked resource bit */
    TEST_ASSERT_TRUE(syn_ccp_process_cro(&g_ccp_slave, cro, dto));
    TEST_ASSERT_EQUAL_HEX8(0x01, dto[3]); /* Seed needed */

    /* UNLOCK */
    cro[0] = SYN_CCP_CMD_UNLOCK;
    cro[1] = 0x03;
    TEST_ASSERT_TRUE(syn_ccp_process_cro(&g_ccp_slave, cro, dto));
    TEST_ASSERT_EQUAL_HEX8(SYN_CCP_ERR_SUCCESS, dto[1]);

    /* SET_CAL_PAGE and GET_ACTIVE_CAL_PAGE */
    cro[0] = SYN_CCP_CMD_SET_CAL_PAGE;
    cro[1] = 0x04;
    cro[2] = 0x02; /* Page 2 */
    TEST_ASSERT_TRUE(syn_ccp_process_cro(&g_ccp_slave, cro, dto));
    TEST_ASSERT_EQUAL_HEX8(SYN_CCP_ERR_SUCCESS, dto[1]);

    cro[0] = SYN_CCP_CMD_GET_ACTIVE_CAL_PAGE;
    cro[1] = 0x05;
    TEST_ASSERT_TRUE(syn_ccp_process_cro(&g_ccp_slave, cro, dto));
    TEST_ASSERT_EQUAL_HEX8(SYN_CCP_ERR_SUCCESS, dto[1]);
    TEST_ASSERT_EQUAL_HEX8(0x02, dto[4]);
}

static uint32_t ccp_seed_generator(uint8_t resource, void *ctx)
{
    (void)ctx;
    return (uint32_t)resource * 0x11111111U;
}

static bool ccp_unlock_validator(uint8_t resource, uint32_t key, void *ctx)
{
    (void)ctx;
    return key == ((uint32_t)resource * 0x11111111U);
}

static bool ccp_erase_handler(uint32_t addr, uint32_t size, void *ctx)
{
    (void)addr;
    (void)ctx;
    return size > 0;
}

static void test_ccp_callbacks_seed_unlock_erase(void)
{
    syn_ccp_init(&g_ccp_slave, 0x1234U);
    g_ccp_slave.seed_cb = ccp_seed_generator;
    g_ccp_slave.unlock_cb = ccp_unlock_validator;
    g_ccp_slave.erase_cb = ccp_erase_handler;

    uint8_t cro[8] = {0};
    uint8_t dto[8] = {0};

    /* Connect */
    cro[0] = SYN_CCP_CMD_CONNECT;
    cro[1] = 0x01;
    cro[2] = 0x34;
    cro[3] = 0x12;
    syn_ccp_process_cro(&g_ccp_slave, cro, dto);

    /* GET_SEED using callback */
    cro[0] = SYN_CCP_CMD_GET_SEED;
    cro[1] = 0x02;
    cro[2] = SYN_CCP_RESOURCE_PGM;
    TEST_ASSERT_TRUE(syn_ccp_process_cro(&g_ccp_slave, cro, dto));
    TEST_ASSERT_EQUAL_HEX8(SYN_CCP_ERR_SUCCESS, dto[1]);

    /* UNLOCK with invalid key -> ACCESS_DENIED */
    cro[0] = SYN_CCP_CMD_UNLOCK;
    cro[1] = 0x03;
    cro[2] = SYN_CCP_RESOURCE_PGM;
    cro[4] = 0x00;
    cro[5] = 0x00;
    cro[6] = 0x00;
    cro[7] = 0x00;
    TEST_ASSERT_TRUE(syn_ccp_process_cro(&g_ccp_slave, cro, dto));
    TEST_ASSERT_EQUAL_HEX8(SYN_CCP_ERR_ACCESS_DENIED, dto[1]);

    /* UNLOCK with valid key -> SUCCESS */
    uint32_t valid_key = (uint32_t)SYN_CCP_RESOURCE_PGM * 0x11111111U;
    cro[4] = (uint8_t)(valid_key);
    cro[5] = (uint8_t)(valid_key >> 8);
    cro[6] = (uint8_t)(valid_key >> 16);
    cro[7] = (uint8_t)(valid_key >> 24);
    TEST_ASSERT_TRUE(syn_ccp_process_cro(&g_ccp_slave, cro, dto));
    TEST_ASSERT_EQUAL_HEX8(SYN_CCP_ERR_SUCCESS, dto[1]);

    /* CLEAR_MEMORY with callback success */
    cro[0] = SYN_CCP_CMD_CLEAR_MEMORY;
    cro[1] = 0x04;
    cro[2] = 0x00;
    cro[3] = 0x10; /* Size 0x1000 */
    cro[4] = 0x00;
    cro[5] = 0x00;
    TEST_ASSERT_TRUE(syn_ccp_process_cro(&g_ccp_slave, cro, dto));
    TEST_ASSERT_EQUAL_HEX8(SYN_CCP_ERR_SUCCESS, dto[1]);

    /* CLEAR_MEMORY with callback failure (size = 0) -> ACCESS_DENIED */
    cro[3] = 0x00; /* Size 0 */
    TEST_ASSERT_TRUE(syn_ccp_process_cro(&g_ccp_slave, cro, dto));
    TEST_ASSERT_EQUAL_HEX8(SYN_CCP_ERR_ACCESS_DENIED, dto[1]);
}

static void test_ccp_program_and_clear_memory(void)
{
    syn_ccp_init(&g_ccp_slave, 0x1234U);
    uint8_t cro[8] = {0};
    uint8_t dto[8] = {0};
    uint8_t buffer[16] = {0};

    /* Connect */
    cro[0] = SYN_CCP_CMD_CONNECT;
    cro[1] = 0x01;
    cro[2] = 0x34;
    cro[3] = 0x12;
    syn_ccp_process_cro(&g_ccp_slave, cro, dto);

    /* SET_MTA1 */
    syn_ccp_set_mta(&g_ccp_slave, 1, 0x01, (uintptr_t)buffer);
    TEST_ASSERT_EQUAL_PTR((void *)(uintptr_t)buffer, (void *)g_ccp_slave.mta1_addr);

    /* CLEAR_MEMORY */
    cro[0] = SYN_CCP_CMD_CLEAR_MEMORY;
    cro[1] = 0x02;
    TEST_ASSERT_TRUE(syn_ccp_process_cro(&g_ccp_slave, cro, dto));
    TEST_ASSERT_EQUAL_HEX8(SYN_CCP_ERR_SUCCESS, dto[1]);

    /* PROGRAM */
    syn_ccp_set_mta(&g_ccp_slave, 0, 0x01, (uintptr_t)buffer);
    cro[0] = SYN_CCP_CMD_PROGRAM;
    cro[1] = 0x03;
    cro[2] = 0x02; /* 2 bytes */
    cro[3] = 0x55;
    cro[4] = 0xAA;
    TEST_ASSERT_TRUE(syn_ccp_process_cro(&g_ccp_slave, cro, dto));
    TEST_ASSERT_EQUAL_HEX8(SYN_CCP_ERR_SUCCESS, dto[1]);
    TEST_ASSERT_EQUAL_HEX8(0x55, buffer[0]);
    TEST_ASSERT_EQUAL_HEX8(0xAA, buffer[1]);

    /* PROGRAM via DNLOAD_6 */
    cro[0] = SYN_CCP_CMD_DNLOAD_6;
    cro[1] = 0x04;
    cro[2] = 0x11;
    cro[3] = 0x22;
    cro[4] = 0x33;
    cro[5] = 0x44;
    cro[6] = 0x55;
    cro[7] = 0x66;
    TEST_ASSERT_TRUE(syn_ccp_process_cro(&g_ccp_slave, cro, dto));
    TEST_ASSERT_EQUAL_HEX8(SYN_CCP_ERR_SUCCESS, dto[1]);
    TEST_ASSERT_EQUAL_HEX8(0x11, buffer[2]);
    TEST_ASSERT_EQUAL_HEX8(0x66, buffer[7]);
}

static void test_ccp_extended_cro_and_daq(void)
{
    syn_ccp_init(&g_ccp_slave, 0x1234U);
    uint8_t cro[8] = {0};
    uint8_t dto[8] = {0};
    uint8_t buffer[16] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60};

    /* Connect */
    cro[0] = SYN_CCP_CMD_CONNECT;
    cro[1] = 0x01;
    cro[2] = 0x34;
    cro[3] = 0x12;
    syn_ccp_process_cro(&g_ccp_slave, cro, dto);

    /* SET_DAQ_SIZE */
    cro[0] = SYN_CCP_CMD_SET_DAQ_SIZE;
    cro[1] = 0x02;
    cro[2] = 0x00; /* DAQ 0 */
    cro[4] = 0x01; /* 1 ODT */
    TEST_ASSERT_TRUE(syn_ccp_process_cro(&g_ccp_slave, cro, dto));

    /* BUILD_PKT via CRO (uses MTA0) */
    syn_ccp_set_mta(&g_ccp_slave, 0, 0x01, (uintptr_t)buffer);
    cro[0] = SYN_CCP_CMD_BUILD_PKT;
    cro[1] = 0x03;
    cro[2] = 0x00; /* DAQ 0 */
    cro[3] = 0x00; /* ODT 0 */
    cro[4] = 0x00; /* Element 0 */
    cro[5] = 0x02; /* 2 bytes */
    TEST_ASSERT_TRUE(syn_ccp_process_cro(&g_ccp_slave, cro, dto));
    TEST_ASSERT_EQUAL_HEX8(SYN_CCP_ERR_SUCCESS, dto[1]);

    /* START_STOP (prescaler = 2) */
    cro[0] = SYN_CCP_CMD_START_STOP;
    cro[1] = 0x03;
    cro[2] = 0x01; /* Start */
    cro[3] = 0x00; /* DAQ 0 */
    cro[5] = 0x01; /* Event channel 1 */
    cro[6] = 0x02; /* Prescaler 2 */
    cro[7] = 0x00;
    TEST_ASSERT_TRUE(syn_ccp_process_cro(&g_ccp_slave, cro, dto));
    TEST_ASSERT_EQUAL_HEX8(SYN_CCP_ERR_SUCCESS, dto[1]);

    uint8_t list_idx = 0;
    uint8_t odt_idx = 0;
    /* Tick 1: prescaler counter incremented, no frame sent */
    TEST_ASSERT_FALSE(syn_ccp_service_daq(&g_ccp_slave, 0x01, dto, &list_idx, &odt_idx));
    /* Tick 2: prescaler threshold reached -> frame sent */
    TEST_ASSERT_TRUE(syn_ccp_service_daq(&g_ccp_slave, 0x01, dto, &list_idx, &odt_idx));
    TEST_ASSERT_EQUAL_HEX8(0x00, dto[0]);
    TEST_ASSERT_EQUAL_HEX8(0x10, dto[1]);
    TEST_ASSERT_EQUAL_HEX8(0x20, dto[2]);

    /* Stop DAQ 0 */
    cro[0] = SYN_CCP_CMD_START_STOP;
    cro[1] = 0x04;
    cro[2] = 0x00; /* Stop */
    cro[3] = 0x00;
    TEST_ASSERT_TRUE(syn_ccp_process_cro(&g_ccp_slave, cro, dto));
    TEST_ASSERT_EQUAL_HEX8(SYN_CCP_ERR_SUCCESS, dto[1]);
    TEST_ASSERT_FALSE(g_ccp_slave.daq_lists[0].running);
}

static void test_ccp_null_checks_and_bounds(void)
{
    syn_ccp_init(&g_ccp_slave, 0x1234U);
    uint8_t cro[8] = {0};
    uint8_t dto[8] = {0};
    uint8_t list_idx = 0;
    uint8_t odt_idx = 0;

    syn_ccp_set_mta(NULL, 0, 0, 0);

    TEST_ASSERT_FALSE(syn_ccp_process_cro(NULL, cro, dto));
    TEST_ASSERT_FALSE(syn_ccp_process_cro(&g_ccp_slave, NULL, dto));
    TEST_ASSERT_FALSE(syn_ccp_process_cro(&g_ccp_slave, cro, NULL));

    TEST_ASSERT_FALSE(syn_ccp_service_daq(NULL, 0, dto, &list_idx, &odt_idx));
    TEST_ASSERT_FALSE(syn_ccp_service_daq(&g_ccp_slave, 0, NULL, &list_idx, &odt_idx));

    /* Connect */
    cro[0] = SYN_CCP_CMD_CONNECT;
    cro[1] = 0x01;
    cro[2] = 0x34;
    cro[3] = 0x12;
    syn_ccp_process_cro(&g_ccp_slave, cro, dto);

    /* Unknown command code */
    cro[0] = 0x99;
    TEST_ASSERT_TRUE(syn_ccp_process_cro(&g_ccp_slave, cro, dto));
    TEST_ASSERT_EQUAL_HEX8(SYN_CCP_ERR_CMD_UNKNOWN, dto[1]);

    /* Oversized DNLOAD length */
    cro[0] = SYN_CCP_CMD_DNLOAD;
    cro[2] = 0x08; /* > 5 */
    TEST_ASSERT_TRUE(syn_ccp_process_cro(&g_ccp_slave, cro, dto));
    TEST_ASSERT_EQUAL_HEX8(SYN_CCP_ERR_PARAM_OUT_OF_RANGE, dto[1]);

    /* Oversized UPLOAD length */
    cro[0] = SYN_CCP_CMD_UPLOAD;
    cro[2] = 0x08; /* > 5 */
    TEST_ASSERT_TRUE(syn_ccp_process_cro(&g_ccp_slave, cro, dto));
    TEST_ASSERT_EQUAL_HEX8(SYN_CCP_ERR_PARAM_OUT_OF_RANGE, dto[1]);

    /* Oversized SHORT_UP length */
    cro[0] = SYN_CCP_CMD_SHORT_UP;
    cro[2] = 0x08; /* > 5 */
    TEST_ASSERT_TRUE(syn_ccp_process_cro(&g_ccp_slave, cro, dto));
    TEST_ASSERT_EQUAL_HEX8(SYN_CCP_ERR_PARAM_OUT_OF_RANGE, dto[1]);

    /* Oversized PROGRAM length */
    cro[0] = SYN_CCP_CMD_PROGRAM;
    cro[2] = 0x08; /* > 5 */
    TEST_ASSERT_TRUE(syn_ccp_process_cro(&g_ccp_slave, cro, dto));
    TEST_ASSERT_EQUAL_HEX8(SYN_CCP_ERR_PARAM_OUT_OF_RANGE, dto[1]);

    /* Out of bounds DAQ list size */
    cro[0] = SYN_CCP_CMD_SET_DAQ_SIZE;
    cro[2] = 0x80; /* Invalid DAQ index */
    TEST_ASSERT_TRUE(syn_ccp_process_cro(&g_ccp_slave, cro, dto));
    TEST_ASSERT_EQUAL_HEX8(SYN_CCP_ERR_PARAM_OUT_OF_RANGE, dto[1]);

    /* Out of bounds GET_DAQ_SIZE index */
    cro[0] = SYN_CCP_CMD_GET_DAQ_SIZE;
    cro[2] = 0x80;
    TEST_ASSERT_TRUE(syn_ccp_process_cro(&g_ccp_slave, cro, dto));
    TEST_ASSERT_EQUAL_HEX8(SYN_CCP_ERR_PARAM_OUT_OF_RANGE, dto[1]);

    /* Out of bounds BUILD_PKT index */
    cro[0] = SYN_CCP_CMD_BUILD_PKT;
    cro[2] = 0x80;
    TEST_ASSERT_TRUE(syn_ccp_process_cro(&g_ccp_slave, cro, dto));
    TEST_ASSERT_EQUAL_HEX8(SYN_CCP_ERR_PARAM_OUT_OF_RANGE, dto[1]);

    /* Out of bounds START_STOP index */
    cro[0] = SYN_CCP_CMD_START_STOP;
    cro[3] = 0x80;
    TEST_ASSERT_TRUE(syn_ccp_process_cro(&g_ccp_slave, cro, dto));
    TEST_ASSERT_EQUAL_HEX8(SYN_CCP_ERR_PARAM_OUT_OF_RANGE, dto[1]);

    /* SET_MTA with addr = 0 & mta_num = 0 (line 119) */
    cro[0] = SYN_CCP_CMD_SET_MTA;
    cro[2] = 0;    /* mta_num = 0 */
    cro[3] = 0x55; /* ext */
    cro[4] = 0;
    cro[5] = 0;
    cro[6] = 0;
    cro[7] = 0; /* addr = 0 */
    TEST_ASSERT_TRUE(syn_ccp_process_cro(&g_ccp_slave, cro, dto));
    TEST_ASSERT_EQUAL_HEX8(0x55, g_ccp_slave.mta0_ext);

    /* daq_event when slave disconnected (line 307) */
    g_ccp_slave.connected = false;
    uint8_t daq_dto[8], l_idx, o_idx;
    TEST_ASSERT_FALSE(syn_ccp_service_daq(&g_ccp_slave, 0x01, daq_dto, &l_idx, &o_idx));

    /* DAQ service with current_odt_idx >= odt_count (line 324) and odt_count == 0 (line 329) */
    g_ccp_slave.connected = true;
    g_ccp_slave.daq_lists[0].running = true;
    g_ccp_slave.daq_lists[0].event_channel = 0x02;
    g_ccp_slave.daq_lists[0].prescaler = 1;
    g_ccp_slave.daq_lists[0].cycle_counter = 0;
    g_ccp_slave.daq_lists[0].odt_count = 0; /* zero ODT count -> line 329 */
    TEST_ASSERT_FALSE(syn_ccp_service_daq(&g_ccp_slave, 0x02, daq_dto, &l_idx, &o_idx));

    g_ccp_slave.daq_lists[0].odt_count = 1;
    g_ccp_slave.daq_lists[0].current_odt_idx =
        5; /* invalid current_odt_idx >= odt_count -> lines 324-325 */
    TEST_ASSERT_TRUE(syn_ccp_service_daq(&g_ccp_slave, 0x02, daq_dto, &l_idx, &o_idx));
}

void run_ccp_tests(void)
{
    RUN_TEST(test_ccp_init_and_connect_disconnect);
    RUN_TEST(test_ccp_mta_upload_download);
    RUN_TEST(test_ccp_daq_list_streaming);
    RUN_TEST(test_ccp_seed_unlock_and_cal_page);
    RUN_TEST(test_ccp_callbacks_seed_unlock_erase);
    RUN_TEST(test_ccp_program_and_clear_memory);
    RUN_TEST(test_ccp_extended_cro_and_daq);
    RUN_TEST(test_ccp_null_checks_and_bounds);
}
