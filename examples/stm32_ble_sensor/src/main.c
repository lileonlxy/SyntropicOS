/**
 * @file main.c
 * @brief STM32 + External BLE Module (UART HCI) Environmental Sensor Sketch.
 */

#include "syntropic/syntropic.h"
#include <stdio.h>
#include <string.h>

static SYN_BLE_HCI hci;
static SYN_BLE_GAP gap;
static SYN_BLE_GATT gatt;

static int16_t mock_temperature_c = 25;

static SYN_Status temp_read_cb(SYN_BLE_GATT *g, uint16_t conn, uint16_t handle, uint8_t *val, uint16_t *val_len, void *ctx)
{
    (void)g;
    (void)conn;
    (void)handle;
    (void)ctx;
    val[0] = (uint8_t)(mock_temperature_c & 0xFF);
    *val_len = 1;
    return SYN_OK;
}

static const SYN_BLE_GATT_Attr gatt_table[] = {
    { 0x0001, SYN_BLE_UUID_PRIMARY_SERVICE, SYN_BLE_PROP_READ, NULL, NULL, (const uint8_t*)"\x09\x18", 2 },
    { 0x0002, 0x2A6E, SYN_BLE_PROP_READ | SYN_BLE_PROP_NOTIFY, temp_read_cb, NULL, NULL, 0 }
};

static void gap_event_cb(SYN_BLE_GAP *g, SYN_BLE_GAP_EventType evt_type, const void *evt_data, void *ctx)
{
    (void)g;
    (void)ctx;
    if (evt_type == SYN_BLE_GAP_EVT_CONNECTED) {
        const SYN_BLE_GAP_ConnInfo *conn = (const SYN_BLE_GAP_ConnInfo *)evt_data;
        (void)conn;
    }
}

int main(void)
{
    SYN_BLE_HCI_Config hci_cfg = {
        .evt_cb = NULL,
        .acl_cb = NULL,
        .user_data = NULL
    };

    syn_ble_hci_init(&hci, &hci_cfg);
    syn_ble_gap_init(&gap, &hci, gap_event_cb, NULL);
    syn_ble_gatt_init(&gatt, gatt_table, 2, NULL);

    uint8_t adv_data[] = {
        0x02, 0x01, 0x06,
        0x0C, 0x09, 'S', 'y', 'n', 't', 'r', 'o', 'p', 'i', 'c', 'B', 'L', 'E'
    };

    uint8_t tx_buf[64];
    uint16_t tx_len = 0;
    syn_ble_gap_set_adv_data(&gap, adv_data, sizeof(adv_data), tx_buf, &tx_len);
    syn_ble_gap_set_adv_enable(&gap, true, tx_buf, &tx_len);

    while (1) {
        /* Main loop: read sensor & process HCI UART feed */
    }

    return 0;
}
