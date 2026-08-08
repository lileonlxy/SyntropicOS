/**
 * @file syn_ntp_server.h
 * @brief Zero-Heap NTP v4 Server Protocol Engine over UDP port 123.
 * @ingroup syn_net
 *
 * Implements a lightweight NTP v4 (RFC 5905) time server allowing SyntropicOS
 * microcontrollers acting as network gateways to serve time synchronization
 * to local network nodes using system RTC or GPS time.
 */

#ifndef SYN_NTP_SERVER_H
#define SYN_NTP_SERVER_H

#include "syntropic/common/syn_defs.h"
#include "syntropic/port/syn_port_socket.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SYN_NTP_SERVER_PORT 123U          /**< Standard NTP UDP Server Port (123) */
#define SYN_NTP_PACKET_LEN 48U            /**< Standard 48-byte NTP Packet Length */
#define SYN_NTP_EPOCH_OFFSET 2208988800UL /**< Seconds offset between 1900 and 1970 UTC epochs */

/** NTP Server Context */
typedef struct {
    SYN_Socket sock;                      /**< Bound UDP socket handle */
    uint8_t stratum;                      /**< Server stratum (1..15) */
    uint32_t (*get_epoch_sec_cb)(void);   /**< Callback for UTC epoch seconds */
    uint8_t req_buf[SYN_NTP_PACKET_LEN];  /**< Input request buffer */
    uint8_t resp_buf[SYN_NTP_PACKET_LEN]; /**< Output response buffer */
} SYN_NTPServer;

/**
 * @brief Initialize NTP Server Instance.
 * @param server Pointer to NTP server instance.
 * @param stratum Server stratum level (e.g. 1 for GPS, 2 for SNTP relay).
 * @param get_epoch_sec_cb Callback returning current UTC epoch seconds.
 * @return SYN_OK on success, SYN_INVALID_PARAM if NULL.
 */
SYN_Status syn_ntp_server_init(SYN_NTPServer *server, uint8_t stratum,
                               uint32_t (*get_epoch_sec_cb)(void));

/**
 * @brief Process an incoming 48-byte NTP client request packet and format response.
 * @param server Pointer to NTP server context.
 * @param request_pkt Raw 48-byte input request packet.
 * @param response_pkt Output 48-byte response packet buffer.
 * @return SYN_OK on success, SYN_INVALID_PARAM if NULL or invalid.
 */
SYN_Status syn_ntp_server_process_packet(SYN_NTPServer *server, const uint8_t request_pkt[48],
                                         uint8_t response_pkt[48]);

#ifdef __cplusplus
}
#endif

#endif /* SYN_NTP_SERVER_H */
