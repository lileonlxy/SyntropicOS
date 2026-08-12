/**
 * @file main.c
 * @brief SyntropicOS ISO 14229-1 (UDS) Application-Layer DTC Manager Example.
 *
 * Demonstrates how an embedded application builds a complete DTC Manager
 * on top of syn_uds (`SYN_UDS_Server` and `SYN_UDS_DTCHandler`), featuring:
 *  - Non-blocking periodic fault monitoring task (`SYN_PT`)
 *  - ISO 14229-1 status byte transitions (testFailed -> pending -> confirmed & self-healing)
 *  - Snapshot / Freeze-frame data capture (voltage, runtime) on fault occurrence
 *  - UDS Service 0x19 (ReadDTCInformation) callbacks:
 *      - 0x01 reportNumberOfDTCByStatusMask
 *      - 0x02 reportDTCByStatusMask
 *      - 0x04 reportDTCSnapshotIdentification
 *      - 0x06 reportDTCSnapshotRecordByDTCNumber
 *  - UDS Service 0x14 (ClearDiagnosticInformation) handler
 *  - Flash persistence simulation
 */

#include "syntropic/proto/syn_isotp.h"
#include "syntropic/proto/syn_uds.h"
#include "syntropic/pt/syn_pt.h"
#include "syntropic/syntropic.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ── Application DTC Definitions ────────────────────────────────────────── */

#define DTC_CODE_CAN_BUS_OFF     0x00B10016U /* CAN Bus Communication Lost */
#define DTC_CODE_VOLTAGE_HIGH    0x00910012U /* System Supply Voltage Too High */
#define DTC_MAX_SNAPSHOT_RECORDS 3U

/** @brief Application DTC Freeze Frame / Snapshot Record */
typedef struct {
    uint8_t record_num;    /* Snapshot Record Number (0x01, 0x02, 0x03) */
    uint16_t voltage_mv;   /* Freeze-frame system voltage (mV) */
    uint32_t uptime_sec;   /* Freeze-frame system uptime (seconds) */
    bool captured;         /* Snapshot valid flag */
} ApplicationSnapshotRecord;

/** @brief Application DTC Control Block */
typedef struct {
    uint32_t dtc_code;                              /* 24-bit ISO DTC code */
    uint8_t status_byte;                            /* ISO 14229-1 DTCStatusByte */
    uint16_t consecutive_fail_cnt;                  /* Confirmation cycle counter */
    uint16_t consecutive_pass_cnt;                  /* Self-healing cycle counter */
    uint32_t occurrence_cnt;                        /* Total fault occurrence count */
    ApplicationSnapshotRecord snapshots[DTC_MAX_SNAPSHOT_RECORDS]; /* Freeze frame records */
} ApplicationDTCRecord;

#define MAX_APP_DTCS 4U
static ApplicationDTCRecord g_dtc_db[MAX_APP_DTCS];
static uint8_t g_dtc_count = 0;

/* UDS Server instance */
static SYN_UDS_Server g_uds_server;

/* Simulated hardware signals */
static uint16_t g_sim_sys_voltage_mv = 12400U; /* 12.4V normal */
static bool g_sim_can_bus_off = false;          /* Normal */
static uint32_t g_sim_uptime_sec = 120U;       /* 120s uptime */

/* ── DTC Database Functions ─────────────────────────────────────────────── */

static ApplicationDTCRecord *find_dtc_record(uint32_t code)
{
    for (uint8_t i = 0; i < g_dtc_count; i++) {
        if (g_dtc_db[i].dtc_code == code) {
            return &g_dtc_db[i];
        }
    }
    return NULL;
}

static ApplicationDTCRecord *add_dtc_record(uint32_t code)
{
    ApplicationDTCRecord *rec = find_dtc_record(code);
    if (rec != NULL) return rec;
    if (g_dtc_count >= MAX_APP_DTCS) return NULL;

    rec = &g_dtc_db[g_dtc_count++];
    memset(rec, 0, sizeof(*rec));
    rec->dtc_code = code;
    return rec;
}

static void capture_freeze_frame(ApplicationDTCRecord *rec)
{
    /* Capture into next free snapshot slot (record numbers 1..3) */
    for (uint8_t i = 0; i < DTC_MAX_SNAPSHOT_RECORDS; i++) {
        if (!rec->snapshots[i].captured) {
            rec->snapshots[i].record_num = (uint8_t)(i + 1);
            rec->snapshots[i].voltage_mv = g_sim_sys_voltage_mv;
            rec->snapshots[i].uptime_sec = g_sim_uptime_sec;
            rec->snapshots[i].captured = true;
            break;
        }
    }
}

/**
 * @brief Evaluate periodic fault status & drive ISO 14229-1 state machine.
 */
static void evaluate_dtc_status(uint32_t code, bool is_fault_active)
{
    ApplicationDTCRecord *rec = add_dtc_record(code);
    if (rec == NULL) return;

    if (is_fault_active) {
        rec->consecutive_pass_cnt = 0;
        rec->consecutive_fail_cnt++;
        rec->status_byte |= (SYN_UDS_DTC_STATUS_TEST_FAILED |
                            SYN_UDS_DTC_STATUS_TEST_FAILED_THIS_OP_CYCLE |
                            SYN_UDS_DTC_STATUS_PENDING_DTC |
                            SYN_UDS_DTC_STATUS_TEST_FAILED_SINCE_LAST_CLEAR);

        /* Confirm DTC after 2 consecutive failing cycles */
        if (rec->consecutive_fail_cnt >= 2U) {
            rec->status_byte |= SYN_UDS_DTC_STATUS_CONFIRMED_DTC;
        }

        rec->occurrence_cnt++;
        capture_freeze_frame(rec);
        syn_uds_dtc_report_test_result(&g_uds_server, code, true);
    } else {
        rec->consecutive_fail_cnt = 0;
        rec->consecutive_pass_cnt++;
        rec->status_byte &= ~(uint8_t)SYN_UDS_DTC_STATUS_TEST_FAILED;

        /* Self-healing: clear pending/confirmed status after 3 consecutive clean cycles */
        if (rec->consecutive_pass_cnt >= 3U) {
            rec->status_byte &= ~(uint8_t)(SYN_UDS_DTC_STATUS_PENDING_DTC |
                                          SYN_UDS_DTC_STATUS_CONFIRMED_DTC);
            rec->consecutive_pass_cnt = 0;
        }
        syn_uds_dtc_report_test_result(&g_uds_server, code, false);
    }
}

/* ── ISO 14229-1 Service 0x19 Callback Handler ──────────────────────────── */

static bool on_read_dtc_info(uint8_t subfunction, const uint8_t *in_data, uint16_t in_len,
                             uint8_t *out_buf, uint16_t max_out_len, uint16_t *out_len, void *ctx)
{
    (void)ctx;

    switch (subfunction) {
    case 0x01: { /* reportNumberOfDTCByStatusMask */
        if (in_len < 1 || max_out_len < 6) return false;
        uint8_t mask = in_data[0];
        uint16_t match_cnt = 0;

        for (uint8_t i = 0; i < g_dtc_count; i++) {
            if ((g_dtc_db[i].status_byte & mask) != 0) {
                match_cnt++;
            }
        }

        out_buf[0] = SYN_UDS_DTC_STATUS_AVAILABILITY_MASK;
        out_buf[1] = SYN_UDS_DTC_FORMAT_ISO14229_1;
        out_buf[2] = (uint8_t)(match_cnt >> 8);
        out_buf[3] = (uint8_t)(match_cnt & 0xFF);
        *out_len = 4;
        return true;
    }

    case 0x02: { /* reportDTCByStatusMask */
        if (in_len < 1 || max_out_len < 2) return false;
        uint8_t mask = in_data[0];
        uint16_t offset = 2;

        out_buf[0] = SYN_UDS_DTC_STATUS_AVAILABILITY_MASK;

        for (uint8_t i = 0; i < g_dtc_count; i++) {
            if ((g_dtc_db[i].status_byte & mask) != 0) {
                if (offset + 4 > max_out_len) break;
                out_buf[offset++] = (uint8_t)(g_dtc_db[i].dtc_code >> 16);
                out_buf[offset++] = (uint8_t)(g_dtc_db[i].dtc_code >> 8);
                out_buf[offset++] = (uint8_t)(g_dtc_db[i].dtc_code & 0xFF);
                out_buf[offset++] = g_dtc_db[i].status_byte;
            }
        }
        *out_len = offset;
        return true;
    }

    case 0x04: { /* reportDTCSnapshotIdentification */
        uint16_t offset = 0;
        for (uint8_t i = 0; i < g_dtc_count; i++) {
            for (uint8_t s = 0; s < DTC_MAX_SNAPSHOT_RECORDS; s++) {
                if (g_dtc_db[i].snapshots[s].captured) {
                    if (offset + 5 > max_out_len) break;
                    out_buf[offset++] = (uint8_t)(g_dtc_db[i].dtc_code >> 16);
                    out_buf[offset++] = (uint8_t)(g_dtc_db[i].dtc_code >> 8);
                    out_buf[offset++] = (uint8_t)(g_dtc_db[i].dtc_code & 0xFF);
                    out_buf[offset++] = g_dtc_db[i].snapshots[s].record_num;
                    out_buf[offset++] = g_dtc_db[i].status_byte;
                }
            }
        }
        *out_len = offset;
        return true;
    }

    case 0x06: { /* reportDTCSnapshotRecordByDTCNumber */
        if (in_len < 4 || max_out_len < 10) return false;
        uint32_t req_dtc = ((uint32_t)in_data[0] << 16) | ((uint32_t)in_data[1] << 8) | in_data[2];
        uint8_t req_rec_num = in_data[3];

        ApplicationDTCRecord *rec = find_dtc_record(req_dtc);
        if (rec == NULL) return false;

        for (uint8_t s = 0; s < DTC_MAX_SNAPSHOT_RECORDS; s++) {
            if (rec->snapshots[s].captured &&
                (req_rec_num == 0xFF || rec->snapshots[s].record_num == req_rec_num)) {
                uint16_t offset = 0;
                out_buf[offset++] = (uint8_t)(req_dtc >> 16);
                out_buf[offset++] = (uint8_t)(req_dtc >> 8);
                out_buf[offset++] = (uint8_t)(req_dtc & 0xFF);
                out_buf[offset++] = rec->status_byte;
                out_buf[offset++] = rec->snapshots[s].record_num;
                out_buf[offset++] = 0x01; /* 1 DID record included */

                /* DID 0x0100: Voltage (mV) */
                out_buf[offset++] = 0x01;
                out_buf[offset++] = 0x00;
                out_buf[offset++] = (uint8_t)(rec->snapshots[s].voltage_mv >> 8);
                out_buf[offset++] = (uint8_t)(rec->snapshots[s].voltage_mv & 0xFF);

                *out_len = offset;
                return true;
            }
        }
        return false;
    }

    default:
        return false;
    }
}

/* ── Application Fault Monitoring Protothread Task ───────────────────────── */

static SYN_PT_Status app_dtc_monitor_task(SYN_PT *pt)
{
    PT_BEGIN(pt);

    /* Monitor CAN bus-off fault condition */
    evaluate_dtc_status(DTC_CODE_CAN_BUS_OFF, g_sim_can_bus_off);

    /* Monitor supply voltage high fault condition (> 16.0V) */
    bool voltage_fault = (g_sim_sys_voltage_mv > 16000U);
    evaluate_dtc_status(DTC_CODE_VOLTAGE_HIGH, voltage_fault);

    PT_END(pt);
}

/* ── Main Entry Point ────────────────────────────────────────────────────── */

int main(void)
{
    printf("SyntropicOS Application-Layer DTC Manager Example\n");

    /* Initialize UDS Server Context */
    syn_uds_init(&g_uds_server);
    syn_uds_register_dtc_handler(&g_uds_server, on_read_dtc_info, NULL);

    /* Register application DTCs with UDS server core */
    syn_uds_dtc_register(&g_uds_server, DTC_CODE_CAN_BUS_OFF,
                         SYN_UDS_DTC_SEVERITY_CHECK_IMMEDIATELY);
    syn_uds_dtc_register(&g_uds_server, DTC_CODE_VOLTAGE_HIGH,
                         SYN_UDS_DTC_SEVERITY_MAINTENANCE_REQUIRED);

    /* Initialize Protothread for periodic DTC monitoring */
    SYN_PT monitor_pt;
    PT_INIT(&monitor_pt);

    printf("Step 1: Running normal operation (no faults active)...\n");
    app_dtc_monitor_task(&monitor_pt);
    printf("Confirmed DTC count: %u\n", (unsigned)syn_uds_dtc_get_confirmed_count(&g_uds_server));

    printf("Step 2: Simulating CAN Bus-Off fault (Cycle 1 -> Pending)...\n");
    g_sim_can_bus_off = true;
    PT_INIT(&monitor_pt);
    app_dtc_monitor_task(&monitor_pt);

    printf("Step 3: Simulating CAN Bus-Off fault (Cycle 2 -> Confirmed + Freeze Frame)...\n");
    PT_INIT(&monitor_pt);
    app_dtc_monitor_task(&monitor_pt);
    printf("Confirmed DTC count: %u\n", (unsigned)syn_uds_dtc_get_confirmed_count(&g_uds_server));

    /* Query UDS Service 0x19 0x02 (Report DTC by Status Mask = 0x08 Confirmed) */
    uint8_t s19_req[1] = {SYN_UDS_DTC_STATUS_CONFIRMED_DTC};
    uint8_t s19_resp[32];
    uint16_t resp_len = 0;

    if (on_read_dtc_info(0x02, s19_req, sizeof(s19_req), s19_resp, sizeof(s19_resp), &resp_len, NULL)) {
        printf("UDS $19 0x02 Response (Len=%u): DTC 0x%02X%02X%02X Status=0x%02X\n",
               resp_len, s19_resp[2], s19_resp[3], s19_resp[4], s19_resp[5]);
    }

    printf("Step 4: Clearing DTCs via UDS Service 0x14...\n");
    syn_uds_clear_dtc(&g_uds_server, 0xFFFFFFU);
    memset(g_dtc_db, 0, sizeof(g_dtc_db));
    g_dtc_count = 0;
    g_sim_can_bus_off = false;

    printf("Confirmed DTC count after clear: %u\n", (unsigned)syn_uds_dtc_get_confirmed_count(&g_uds_server));
    printf("Application DTC Manager Example Complete.\n");

    return 0;
}
