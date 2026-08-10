/**
 * @file syn_ble_gap.h
 * @brief Zero-Heap Generic Access Profile (GAP) Layer.
 */

#ifndef SYN_BLE_GAP_H
#define SYN_BLE_GAP_H

#include "../common/syn_defs.h"
#include "syn_ble_hci.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @name GAP Advertising Types */
/**@{*/
#define SYN_BLE_ADV_IND 0x00U         /**< Connectable undirected advertising */
#define SYN_BLE_ADV_DIRECT_IND 0x01U  /**< Connectable directed advertising */
#define SYN_BLE_ADV_SCAN_IND 0x02U    /**< Scannable undirected advertising */
#define SYN_BLE_ADV_NONCONN_IND 0x03U /**< Non-connectable undirected advertising */
/**@}*/

/** GAP Event Type */
typedef enum {
    SYN_BLE_GAP_EVT_CONNECTED = 0, /**< GAP Connected Event */
    SYN_BLE_GAP_EVT_DISCONNECTED,  /**< GAP Disconnected Event */
    SYN_BLE_GAP_EVT_ADV_REPORT     /**< GAP Advertising Report Event */
} SYN_BLE_GAP_EventType;

/** Connection Info */
typedef struct {
    uint16_t conn_handle;         /**< Connection handle */
    uint8_t peer_addr_type;       /**< Peer address type */
    uint8_t peer_addr[6];         /**< 6-byte peer MAC address */
    uint16_t conn_interval;       /**< Connection interval (units 1.25ms) */
    uint16_t conn_latency;        /**< Slave latency */
    uint16_t supervision_timeout; /**< Supervision timeout (units 10ms) */
} SYN_BLE_GAP_ConnInfo;

/** Advertising Report Info */
typedef struct {
    uint8_t evt_type;    /**< Event type (e.g. SYN_BLE_ADV_IND) */
    uint8_t addr_type;   /**< Address type (public/random) */
    uint8_t addr[6];     /**< 6-byte advertiser MAC address */
    const uint8_t *data; /**< Pointer to advertising payload bytes */
    uint8_t data_len;    /**< Advertising payload length */
    int8_t rssi;         /**< RSSI signal strength (dBm) */
} SYN_BLE_GAP_AdvReport;

typedef struct SYN_BLE_GAP_s SYN_BLE_GAP;

/** GAP Event Callback */
typedef void (*SYN_BLE_GAP_EventCb)(SYN_BLE_GAP *gap, SYN_BLE_GAP_EventType evt_type,
                                    const void *evt_data, void *user_data);

/** Advertising Parameters */
typedef struct {
    uint16_t interval_min;  /**< Advertising interval min (units 0.625ms) */
    uint16_t interval_max;  /**< Advertising interval max (units 0.625ms) */
    uint8_t type;           /**< Advertising type (e.g. SYN_BLE_ADV_IND) */
    uint8_t own_addr_type;  /**< Own address type */
    uint8_t peer_addr_type; /**< Peer address type */
    uint8_t peer_addr[6];   /**< Peer MAC address */
    uint8_t channel_map;    /**< Channel map bitmask (default 0x07 for ch 37,38,39) */
    uint8_t filter_policy;  /**< Advertising filter policy */
} SYN_BLE_GAP_AdvParams;

/** GAP Engine Context */
struct SYN_BLE_GAP_s {
    SYN_BLE_HCI *hci;           /**< Associated HCI driver handle */
    SYN_BLE_GAP_EventCb evt_cb; /**< GAP event callback */
    void *user_data;            /**< User context pointer */
    bool advertising;           /**< True if currently advertising */
    bool scanning;              /**< True if currently scanning */
};

/**
 * @brief Initialize GAP engine context.
 * @param gap Context handle pointer.
 * @param hci Initialized HCI driver handle.
 * @param evt_cb GAP event callback.
 * @param user_data User context pointer.
 * @return SYN_OK on success.
 */
SYN_Status syn_ble_gap_init(SYN_BLE_GAP *gap, SYN_BLE_HCI *hci, SYN_BLE_GAP_EventCb evt_cb,
                            void *user_data);

/**
 * @brief Set Advertising Data.
 * @param gap Context handle pointer.
 * @param data AD data payload.
 * @param len Data length (max 31 bytes).
 * @param tx_buf HCI TX staging buffer (min 36 bytes).
 * @param tx_len Output length pointer.
 * @return SYN_OK on success.
 */
SYN_Status syn_ble_gap_set_adv_data(SYN_BLE_GAP *gap, const uint8_t *data, uint8_t len,
                                    uint8_t *tx_buf, uint16_t *tx_len);

/**
 * @brief Enable or Disable Advertising.
 * @param gap Context handle pointer.
 * @param enable True to start, false to stop.
 * @param tx_buf HCI TX staging buffer (min 6 bytes).
 * @param tx_len Output length pointer.
 * @return SYN_OK on success.
 */
SYN_Status syn_ble_gap_set_adv_enable(SYN_BLE_GAP *gap, bool enable, uint8_t *tx_buf,
                                      uint16_t *tx_len);

/**
 * @brief Process HCI Event for GAP state updates.
 * @param gap Context handle pointer.
 * @param evt_code HCI Event code.
 * @param payload Event payload.
 * @param len Event length.
 * @return SYN_OK on success.
 */
SYN_Status syn_ble_gap_process_hci_evt(SYN_BLE_GAP *gap, uint8_t evt_code, const uint8_t *payload,
                                       uint8_t len);

#ifdef __cplusplus
}
#endif

#endif /* SYN_BLE_GAP_H */
