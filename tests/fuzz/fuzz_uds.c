/**
 * @file fuzz_uds.c
 * @brief libFuzzer target for syn_uds UDS diagnostic server.
 */

#include "mock_port.h"
#include "syntropic/proto/syn_uds.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size == 0 || size > 512) {
        return 0;
    }

    mock_port_reset();

    SYN_UDS_Server server;
    if (!syn_uds_init(&server)) {
        return 0;
    }

    uint8_t resp_buf[512];
    uint16_t resp_len = 0;

    /* Feed arbitrary fuzzed request payload into UDS server processor */
    syn_uds_process_request(&server, data, (uint16_t)size, resp_buf, sizeof(resp_buf), &resp_len,
                            (SYN_UDS_AddrMode)(data[0] & 0x01));

    /* Step S3 timer with fuzzed tick */
    syn_uds_tick(&server, (uint32_t)(data[0] & 0x7F));

    return 0;
}
