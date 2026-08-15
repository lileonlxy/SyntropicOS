#include "syntropic/ble/syn_ble_att.h"
#include "unity/unity.h"

#include <string.h>

void test_ble_att_encoders(void)
{
    uint8_t buf[64];
    uint16_t len = 0;

    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_ble_att_encode_error_rsp(0, 0, 0, NULL, NULL));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_ble_att_encode_mtu_rsp(0, NULL, NULL));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_ble_att_encode_read_rsp(NULL, 1, buf, &len));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_ble_att_encode_write_rsp(NULL, &len));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_ble_att_encode_notification(1, NULL, 1, buf, &len));

    TEST_ASSERT_EQUAL(SYN_OK,
                      syn_ble_att_encode_error_rsp(SYN_BLE_ATT_OP_READ_REQ, 0x0002,
                                                   SYN_BLE_ATT_ERR_ATTR_NOT_FOUND, buf, &len));
    TEST_ASSERT_EQUAL_UINT16(5, len);
    TEST_ASSERT_EQUAL_UINT8(SYN_BLE_ATT_OP_ERROR_RSP, buf[0]);
    TEST_ASSERT_EQUAL_UINT8(SYN_BLE_ATT_OP_READ_REQ, buf[1]);
    TEST_ASSERT_EQUAL_UINT8(0x02, buf[2]);
    TEST_ASSERT_EQUAL_UINT8(SYN_BLE_ATT_ERR_ATTR_NOT_FOUND, buf[4]);

    TEST_ASSERT_EQUAL(SYN_OK, syn_ble_att_encode_mtu_rsp(64, buf, &len));
    TEST_ASSERT_EQUAL_UINT16(3, len);
    TEST_ASSERT_EQUAL_UINT8(SYN_BLE_ATT_OP_EXCHANGE_MTU_RSP, buf[0]);
    TEST_ASSERT_EQUAL_UINT8(64, buf[1]);

    uint8_t val[] = {0x01, 0x02};
    TEST_ASSERT_EQUAL(SYN_OK, syn_ble_att_encode_read_rsp(val, 2, buf, &len));
    TEST_ASSERT_EQUAL_UINT16(3, len);
    TEST_ASSERT_EQUAL_UINT8(SYN_BLE_ATT_OP_READ_RSP, buf[0]);
    TEST_ASSERT_EQUAL_UINT8(0x01, buf[1]);

    TEST_ASSERT_EQUAL(SYN_OK, syn_ble_att_encode_write_rsp(buf, &len));
    TEST_ASSERT_EQUAL_UINT16(1, len);
    TEST_ASSERT_EQUAL_UINT8(SYN_BLE_ATT_OP_WRITE_RSP, buf[0]);

    /* 0-length value payloads */
    TEST_ASSERT_EQUAL(SYN_OK, syn_ble_att_encode_read_rsp(NULL, 0, buf, &len));
    TEST_ASSERT_EQUAL_UINT16(1, len);
    TEST_ASSERT_EQUAL_UINT8(SYN_BLE_ATT_OP_READ_RSP, buf[0]);

    TEST_ASSERT_EQUAL(SYN_OK, syn_ble_att_encode_notification(0x0004, NULL, 0, buf, &len));
    TEST_ASSERT_EQUAL_UINT16(3, len);
    TEST_ASSERT_EQUAL_UINT8(SYN_BLE_ATT_OP_HANDLE_VAL_NOTIF, buf[0]);

    /* Oversized payload (> 250 bytes) */
    uint8_t large_val[255];
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_ble_att_encode_read_rsp(large_val, 251, buf, &len));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_ble_att_encode_notification(0x0005, large_val, 251, buf, &len));
}
