# STM32 + BLE Environmental Sensor Example

Demonstrates using SyntropicOS's zero-heap BLE host stack (`syn_ble_hci`, `syn_ble_gap`, `syn_ble_gatt`) on an STM32 MCU paired with an external BLE controller (e.g. nRF52840, BlueNRG-2, or ESP32) connected via UART HCI (H:4 transport).

## Features
- **GAP Peripheral Advertising**: Advertises device name "SyntropicBLE".
- **GATT Environmental Sensing Service**: Exposes Temperature characteristic (`0x2A6E`) with read and notify capability.
- **Zero Heap**: All HCI framing, GATT tables, and L2CAP state machines use caller-owned static RAM.

## Hardware Wiring
- **STM32 USART TX** → BLE Controller HCI RX
- **STM32 USART RX** ← BLE Controller HCI TX
- **STM32 GPIO** → BLE Controller RESET (optional)
