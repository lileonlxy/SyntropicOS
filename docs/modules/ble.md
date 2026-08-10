# Bluetooth Low Energy (BLE) Host Stack

The SyntropicOS BLE module (`syn_ble`) provides a zero-heap, deterministic Bluetooth Low Energy (BLE) host stack for microcontrollers paired with external BLE controllers over UART (H:4) or SPI.

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                 BLE Application / Profile               │
├─────────────────────────────────────────────────────────┤
│                   GATT Server (syn_ble_gatt)            │ Static attribute table
├─────────────────────────────────────────────────────────┤
│                   GAP Layer (syn_ble_gap)               │ Adv, scan, connect, disconnect
├─────────────────────────────────────────────────────────┤
│                   ATT Protocol (syn_ble_att)            │ PDU encoding & decoding
├─────────────────────────────────────────────────────────┤
│                  L2CAP Channel (syn_ble_l2cap)          │ CID 0x0004 (ATT), reassembly
├─────────────────────────────────────────────────────────┤
│                HCI Transport Driver (syn_ble_hci)       │ H:4 UART framing (Cmd, Evt, ACL)
├─────────────────────────────────────────────────────────┤
│                HCI Port Adapter (syn_port_ble_hci)      │ Hardware UART / SPI transport
└─────────────────────────────────────────────────────────┘
```

## Key Features

- **Zero Dynamic Memory**: All HCI RX reassembly buffers, L2CAP connection tables, and GATT attribute entries are statically allocated.
- **H:4 Transport Layer**: Full compliance with Bluetooth Core Specification Vol 4, Part A.
- **GATT Server**: Define static attribute tables for custom or standard SIG services/characteristics.
- **GAP Peripheral & Central**: Supports connectable/non-connectable advertising, scanning, and connection parameter handling.

## Usage Example

```c
#include "syntropic/syntropic.h"

static SYN_BLE_HCI hci;
static SYN_BLE_GAP gap;
static SYN_BLE_GATT gatt;

/* Define GATT attribute table */
static const SYN_BLE_GATT_Attr gatt_table[] = {
    { 0x0001, SYN_BLE_UUID_PRIMARY_SERVICE, SYN_BLE_PROP_READ, NULL, NULL, (const uint8_t*)"\x09\x18", 2 },
    { 0x0002, 0x2A6E, SYN_BLE_PROP_READ, temp_read_cb, NULL, NULL, 0 }
};

void ble_init(void) {
    syn_ble_hci_init(&hci, NULL);
    syn_ble_gap_init(&gap, &hci, gap_event_cb, NULL);
    syn_ble_gatt_init(&gatt, gatt_table, 2, NULL);
}
```

## Configuration Defines

| Define | Default | Description |
|---|---|---|
| `SYN_USE_BLE` | `1` | Enable BLE module |
| `SYN_BLE_MAX_CONNECTIONS` | `2` | Maximum simultaneous active BLE connections |
| `SYN_BLE_MAX_ATTRIBUTES` | `32` | Maximum GATT attribute table capacity |
| `SYN_BLE_HCI_RX_BUF_SIZE` | `256` | HCI RX packet capacity (bytes) |
| `SYN_BLE_HCI_TX_BUF_SIZE` | `256` | HCI TX packet capacity (bytes) |
