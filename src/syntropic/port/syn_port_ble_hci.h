/**
 * @file syn_port_ble_hci.h
 * @brief Port Abstraction Header for Bluetooth HCI Transport (UART H:4 / SPI).
 */

#ifndef SYN_PORT_BLE_HCI_H
#define SYN_PORT_BLE_HCI_H

#include "../common/syn_defs.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Transmit raw bytes to external BLE controller via UART/SPI hardware.
 * @param data Data buffer pointer.
 * @param len Data length in bytes.
 * @return SYN_OK on success.
 */
SYN_Status syn_port_ble_hci_tx(const uint8_t *data, uint16_t len);

/**
 * @brief Reset external BLE controller hardware via GPIO reset pin (if present).
 * @return SYN_OK on success, or SYN_ERR_NOT_SUPPORTED if unmapped.
 */
SYN_Status syn_port_ble_hci_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* SYN_PORT_BLE_HCI_H */
