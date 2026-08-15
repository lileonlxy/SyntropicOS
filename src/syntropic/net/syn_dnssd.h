/**
 * @file syn_dnssd.h
 * @brief DNS-Based Service Discovery (DNS-SD - RFC 6763 / RFC 6762).
 * @ingroup syn_net
 *
 * Provides zero-allocation, cleanroom multicast DNS-SD service announcement
 * and query responding for embedded devices (PTR, SRV, TXT, and A records).
 */

#ifndef SYN_DNSSD_H
#define SYN_DNSSD_H

#include "../common/syn_defs.h"
#include "../port/syn_port_socket.h"
#include "../pt/syn_pt.h"
#include "../sched/syn_task.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Maximum registered services per DNS-SD instance. */
#define SYN_DNSSD_MAX_SERVICES 4U
/** @brief Maximum TXT key-value attributes per service. */
#define SYN_DNSSD_MAX_TXT_RECORDS 8U
/** @brief Standard mDNS / DNS-SD Multicast Port. */
#define SYN_DNSSD_PORT 5353U

/**
 * @brief DNS-SD Service Definition.
 */
typedef struct {
    const char *instance_name; /**< Service Instance Name (e.g. "Syntropic Node") */
    const char *service_type;  /**< Service Type (e.g. "_http._tcp")               */
    const char *hostname;      /**< Target Hostname without .local (e.g. "node")  */
    uint16_t port;             /**< TCP/UDP Port number (e.g. 80)                 */
    uint8_t ip[4];             /**< IPv4 address to announce                      */
    const char *txt_records[SYN_DNSSD_MAX_TXT_RECORDS]; /**< Key-value TXT strings */
    size_t txt_count; /**< Number of TXT strings                         */
} SYN_DnsSd_Service;

/**
 * @brief DNS-SD Daemon Context.
 */
typedef struct {
    SYN_DnsSd_Service services[SYN_DNSSD_MAX_SERVICES]; /**< Service registry     */
    size_t service_count;                               /**< Registered count     */
    SYN_Socket sock;                                    /**< Multicast UDP socket */
} SYN_DnsSd;

/**
 * @brief Initialize DNS-SD daemon context.
 *
 * @param sd DNS-SD instance.
 * @return SYN_OK on success, SYN_ERROR on socket/multicast error.
 */
SYN_Status syn_dnssd_init(SYN_DnsSd *sd);

/**
 * @brief Register a service for DNS-SD broadcast/discovery.
 *
 * @param sd  DNS-SD instance.
 * @param svc Service configuration to register.
 * @return SYN_OK on success, SYN_ERROR if table is full or invalid parameters.
 */
SYN_Status syn_dnssd_register(SYN_DnsSd *sd, const SYN_DnsSd_Service *svc);

/**
 * @brief Process an incoming mDNS / DNS-SD query packet and format response.
 *
 * @param sd           DNS-SD context containing registered services.
 * @param query_buf    Incoming raw DNS packet.
 * @param query_len    Query packet byte length.
 * @param resp_buf     [out] Buffer to receive formatted response packet.
 * @param max_resp_len Capacity of resp_buf.
 * @param resp_len     [out] Number of bytes written to resp_buf.
 * @return SYN_OK if query matched and response generated, SYN_ERROR/SYN_NOT_FOUND otherwise.
 */
SYN_Status syn_dnssd_process_query(const SYN_DnsSd *sd, const uint8_t *query_buf, size_t query_len,
                                   uint8_t *resp_buf, size_t max_resp_len, size_t *resp_len);

/**
 * @brief Format and send gratuitous multicast DNS-SD announcement (RFC 6762 §8.3).
 *
 * @param sd            DNS-SD context.
 * @param service_index Index of service to announce.
 * @param resp_buf      [out] Buffer to receive formatted response packet.
 * @param max_resp_len  Capacity of resp_buf.
 * @param resp_len      [out] Number of bytes written to resp_buf.
 * @return SYN_OK on success, SYN_ERROR on invalid parameters or send failure.
 */
SYN_Status syn_dnssd_announce(const SYN_DnsSd *sd, size_t service_index, uint8_t *resp_buf,
                              size_t max_resp_len, size_t *resp_len);

/**
 * @brief Cooperative protothread task for responding to DNS-SD discovery queries.
 *
 * @param pt   Protothread pointer.
 * @param task Task structure with user_data pointing to SYN_DnsSd instance.
 * @return PT_WAITING while running, PT_EXITED when done.
 */
SYN_PT_Status syn_dnssd_task(SYN_PT *pt, SYN_Task *task);

#ifdef __cplusplus
}
#endif

#endif /* SYN_DNSSD_H */
