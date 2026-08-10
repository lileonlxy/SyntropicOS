#include "syntropic/ble/syn_ble_gatt.h"
#include "unity/unity.h"

#include <string.h>

static uint8_t mock_temp = 25;
static bool mock_read_fail = false;
static bool mock_write_fail = false;
static uint8_t large_static_buf[100];

static SYN_Status temp_read_cb(SYN_BLE_GATT *gatt, uint16_t conn_handle, uint16_t attr_handle,
                               uint8_t *val, uint16_t *val_len, void *user_data)
{
    (void)gatt;
    (void)conn_handle;
    (void)attr_handle;
    (void)user_data;
    if (mock_read_fail) {
        return SYN_ERROR;
    }
    val[0] = mock_temp;
    *val_len = 1;
    return SYN_OK;
}

static SYN_Status temp_write_cb(SYN_BLE_GATT *gatt, uint16_t conn_handle, uint16_t attr_handle,
                                const uint8_t *val, uint16_t val_len, void *user_data)
{
    (void)gatt;
    (void)conn_handle;
    (void)attr_handle;
    (void)user_data;
    if (mock_write_fail) {
        return SYN_ERROR;
    }
    if (val_len > 0) {
        mock_temp = val[0];
    }
    return SYN_OK;
}

static const SYN_BLE_GATT_Attr test_gatt_table[] = {
    {0x0001, SYN_BLE_UUID_PRIMARY_SERVICE, SYN_BLE_PROP_READ, NULL, NULL,
     (const uint8_t *)"\x09\x18", 2},
    {0x0002, 0x2A6E, SYN_BLE_PROP_READ | SYN_BLE_PROP_WRITE, temp_read_cb, temp_write_cb, NULL, 0},
    {0x0003, 0x2A00, 0, NULL, NULL, NULL, 0}, /* No read, no write, no static val */
    {0x0004, 0x2A01, SYN_BLE_PROP_READ, NULL, NULL, large_static_buf,
     100} /* Oversized static val (>64B) */
};

void test_ble_gatt_null_params(void)
{
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_ble_gatt_init(NULL, NULL, 0, NULL));

    SYN_BLE_GATT gatt;
    TEST_ASSERT_EQUAL(SYN_OK, syn_ble_gatt_init(&gatt, test_gatt_table, 4, NULL));

    uint8_t resp[128];
    uint16_t resp_len = 0;
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_ble_gatt_process_att_pdu(NULL, 1, NULL, 0, resp, &resp_len));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_ble_gatt_notify(NULL, 1, 0x0002, NULL, 0, NULL, &resp_len));
}

void test_ble_gatt_read_request(void)
{
    SYN_BLE_GATT gatt;
    syn_ble_gatt_init(&gatt, test_gatt_table, 4, NULL);

    /* Static val read */
    uint8_t static_read_req[] = {SYN_BLE_ATT_OP_READ_REQ, 0x01, 0x00};
    uint8_t resp[128];
    uint16_t resp_len = 0;
    TEST_ASSERT_EQUAL(SYN_OK,
                      syn_ble_gatt_process_att_pdu(&gatt, 1, static_read_req,
                                                   sizeof(static_read_req), resp, &resp_len));
    TEST_ASSERT_EQUAL_UINT16(3, resp_len);
    TEST_ASSERT_EQUAL_UINT8(SYN_BLE_ATT_OP_READ_RSP, resp[0]);

    /* Oversized static val read (clamped to 64B) */
    uint8_t large_read_req[] = {SYN_BLE_ATT_OP_READ_REQ, 0x04, 0x00};
    TEST_ASSERT_EQUAL(SYN_OK,
                      syn_ble_gatt_process_att_pdu(&gatt, 1, large_read_req, sizeof(large_read_req),
                                                   resp, &resp_len));
    TEST_ASSERT_EQUAL_UINT16(65, resp_len); /* 1B opcode + 64B val */

    /* Callback read success */
    mock_read_fail = false;
    uint8_t read_req[] = {SYN_BLE_ATT_OP_READ_REQ, 0x02, 0x00};
    mock_temp = 25;
    TEST_ASSERT_EQUAL(SYN_OK, syn_ble_gatt_process_att_pdu(&gatt, 1, read_req, sizeof(read_req),
                                                           resp, &resp_len));
    TEST_ASSERT_EQUAL_UINT16(2, resp_len);
    TEST_ASSERT_EQUAL_UINT8(25, resp[1]);

    /* Callback read failure */
    mock_read_fail = true;
    TEST_ASSERT_EQUAL(SYN_OK, syn_ble_gatt_process_att_pdu(&gatt, 1, read_req, sizeof(read_req),
                                                           resp, &resp_len));
    TEST_ASSERT_EQUAL_UINT8(SYN_BLE_ATT_OP_ERROR_RSP, resp[0]);
    mock_read_fail = false;

    /* Not found handle read */
    uint8_t not_found_req[] = {SYN_BLE_ATT_OP_READ_REQ, 0x99, 0x00};
    TEST_ASSERT_EQUAL(SYN_OK, syn_ble_gatt_process_att_pdu(&gatt, 1, not_found_req,
                                                           sizeof(not_found_req), resp, &resp_len));
    TEST_ASSERT_EQUAL_UINT8(SYN_BLE_ATT_OP_ERROR_RSP, resp[0]);

    /* Read not permitted attribute */
    uint8_t no_perm_req[] = {SYN_BLE_ATT_OP_READ_REQ, 0x03, 0x00};
    TEST_ASSERT_EQUAL(SYN_OK, syn_ble_gatt_process_att_pdu(&gatt, 1, no_perm_req,
                                                           sizeof(no_perm_req), resp, &resp_len));
    TEST_ASSERT_EQUAL_UINT8(SYN_BLE_ATT_OP_ERROR_RSP, resp[0]);
}

void test_ble_gatt_write_request(void)
{
    SYN_BLE_GATT gatt;
    syn_ble_gatt_init(&gatt, test_gatt_table, 4, NULL);

    /* Write success */
    mock_write_fail = false;
    uint8_t write_req[] = {SYN_BLE_ATT_OP_WRITE_REQ, 0x02, 0x00, 30};
    uint8_t resp[128];
    uint16_t resp_len = 0;

    TEST_ASSERT_EQUAL(SYN_OK, syn_ble_gatt_process_att_pdu(&gatt, 1, write_req, sizeof(write_req),
                                                           resp, &resp_len));
    TEST_ASSERT_EQUAL_UINT16(1, resp_len);
    TEST_ASSERT_EQUAL_UINT8(SYN_BLE_ATT_OP_WRITE_RSP, resp[0]);
    TEST_ASSERT_EQUAL_UINT8(30, mock_temp);

    /* Write request on non-existent handle */
    uint8_t write_unmapped_req[] = {SYN_BLE_ATT_OP_WRITE_REQ, 0x99, 0x00, 30};
    TEST_ASSERT_EQUAL(SYN_OK,
                      syn_ble_gatt_process_att_pdu(&gatt, 1, write_unmapped_req,
                                                   sizeof(write_unmapped_req), resp, &resp_len));
    TEST_ASSERT_EQUAL_UINT8(SYN_BLE_ATT_OP_ERROR_RSP, resp[0]);
    TEST_ASSERT_EQUAL_UINT8(SYN_BLE_ATT_ERR_ATTR_NOT_FOUND, resp[4]);

    /* Write failure */
    mock_write_fail = true;
    TEST_ASSERT_EQUAL(SYN_OK, syn_ble_gatt_process_att_pdu(&gatt, 1, write_req, sizeof(write_req),
                                                           resp, &resp_len));
    TEST_ASSERT_EQUAL_UINT8(SYN_BLE_ATT_OP_ERROR_RSP, resp[0]);
    mock_write_fail = false;

    /* Write command on unknown handle */
    uint8_t write_cmd_unknown[] = {SYN_BLE_ATT_OP_WRITE_CMD, 0x99, 0x00, 35};
    TEST_ASSERT_EQUAL(SYN_OK,
                      syn_ble_gatt_process_att_pdu(&gatt, 1, write_cmd_unknown,
                                                   sizeof(write_cmd_unknown), resp, &resp_len));
    TEST_ASSERT_EQUAL_UINT16(0, resp_len);

    /* Write command (no response) */
    uint8_t write_cmd[] = {SYN_BLE_ATT_OP_WRITE_CMD, 0x02, 0x00, 35};
    TEST_ASSERT_EQUAL(SYN_OK, syn_ble_gatt_process_att_pdu(&gatt, 1, write_cmd, sizeof(write_cmd),
                                                           resp, &resp_len));
    TEST_ASSERT_EQUAL_UINT16(0, resp_len);
    TEST_ASSERT_EQUAL_UINT8(35, mock_temp);

    /* Exchange MTU request */
    uint8_t mtu_req[] = {SYN_BLE_ATT_OP_EXCHANGE_MTU_REQ, 0x40, 0x00};
    TEST_ASSERT_EQUAL(
        SYN_OK, syn_ble_gatt_process_att_pdu(&gatt, 1, mtu_req, sizeof(mtu_req), resp, &resp_len));
    TEST_ASSERT_EQUAL_UINT8(SYN_BLE_ATT_OP_EXCHANGE_MTU_RSP, resp[0]);

    /* Unsupported PDU opcode */
    uint8_t unknown_pdu[] = {0xFE, 0x00, 0x00};
    TEST_ASSERT_EQUAL(SYN_OK, syn_ble_gatt_process_att_pdu(&gatt, 1, unknown_pdu,
                                                           sizeof(unknown_pdu), resp, &resp_len));
    TEST_ASSERT_EQUAL_UINT8(SYN_BLE_ATT_OP_ERROR_RSP, resp[0]);
}

void test_ble_gatt_notification(void)
{
    SYN_BLE_GATT gatt;
    syn_ble_gatt_init(&gatt, test_gatt_table, 4, NULL);

    uint8_t val = 42;
    uint8_t tx_buf[128];
    uint16_t tx_len = 0;

    TEST_ASSERT_EQUAL(SYN_OK, syn_ble_gatt_notify(&gatt, 1, 0x0002, &val, 1, tx_buf, &tx_len));
    TEST_ASSERT_TRUE(tx_len > 0);
    TEST_ASSERT_EQUAL_UINT8(SYN_BLE_HCI_PKT_ACL, tx_buf[0]);

    /* Oversized notification payload error */
    uint8_t huge_val[300];
    uint8_t huge_tx[500];
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_ble_gatt_notify(&gatt, 1, 0x0002, huge_val,
                                                             sizeof(huge_val), huge_tx, &tx_len));
}
