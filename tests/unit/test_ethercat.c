/**
 * @file test_ethercat.c
 * @brief Unity tests for EtherCAT Master/Slave Protocol Engine (syn_ethercat).
 */

#include "mocks/mock_port.h"
#include "syntropic/proto/syn_ethercat.h"
#include "syntropic/syntropic.h"
#include "unity/unity.h"

#include <string.h>

static void test_ecat_init_and_esm(void)
{
    SYN_EcatNode node;
    syn_ecat_init(&node, 0x1001, NULL);

    TEST_ASSERT_EQUAL(SYN_ECAT_STATE_INIT, node.state);
    TEST_ASSERT_EQUAL(SYN_ECAT_STATE_INIT, node.target_state);
    TEST_ASSERT_EQUAL_UINT16(0x1001, node.station_addr);

    /* Request PREOP state */
    TEST_ASSERT_EQUAL(SYN_OK, syn_ecat_set_state(&node, SYN_ECAT_STATE_PREOP));
    TEST_ASSERT_EQUAL(SYN_ECAT_STATE_PREOP, node.target_state);

    syn_ecat_update(&node);
    TEST_ASSERT_EQUAL(SYN_ECAT_STATE_PREOP, node.state);

    /* Request SAFEOP state */
    TEST_ASSERT_EQUAL(SYN_OK, syn_ecat_set_state(&node, SYN_ECAT_STATE_SAFEOP));
    syn_ecat_update(&node);
    TEST_ASSERT_EQUAL(SYN_ECAT_STATE_SAFEOP, node.state);

    /* Request OP state */
    TEST_ASSERT_EQUAL(SYN_OK, syn_ecat_set_state(&node, SYN_ECAT_STATE_OP));
    syn_ecat_update(&node);
    TEST_ASSERT_EQUAL(SYN_ECAT_STATE_OP, node.state);

    /* Invalid state request */
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_ecat_set_state(&node, (SYN_EcatState)0xFF));
}

static void test_ecat_datagram_build_and_parse(void)
{
    SYN_EcatNode node;
    syn_ecat_init(&node, 0x1001, NULL);

    uint8_t frame_buf[64];
    SYN_EcatDatagram dg = {.cmd = SYN_ECAT_CMD_FPRD,
                           .idx = 0x42,
                           .addr = 0x10010000,
                           .m = 0,
                           .circ = 0,
                           .irq = 0x0000};

    uint8_t payload[4] = {0x11, 0x22, 0x33, 0x44};
    size_t frame_len = syn_ecat_build_datagram_frame(frame_buf, sizeof(frame_buf), &dg, payload, 4);

    /* Total = 2 (header) + 10 (dg header) + 4 (data) + 2 (wkc) = 18 bytes */
    TEST_ASSERT_EQUAL_INT(18, frame_len);

    /* Simulate Master/Slave incrementing WKC on datagram read */
    frame_buf[16] = 0x01;
    frame_buf[17] = 0x00;

    uint16_t wkc = 0;
    SYN_Status st = syn_ecat_parse_frame(&node, frame_buf, frame_len, &wkc);
    TEST_ASSERT_EQUAL(SYN_OK, st);
    TEST_ASSERT_EQUAL_UINT16(1, wkc);
    TEST_ASSERT_EQUAL_UINT16(1, node.wkc_last);

    /* Parse corrupted/short frame */
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_ecat_parse_frame(&node, frame_buf, 5, &wkc));

    /* Parse frame with WKC mismatch */
    frame_buf[16] = 0x00;
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_ecat_parse_frame(&node, frame_buf, frame_len, &wkc));
}

static void test_ecat_coe_sdo_mailbox(void)
{
    uint8_t mbox_buf[32];

    /* SDO Download (Write) Request */
    uint32_t val = 0x12345678;
    size_t len = syn_ecat_coe_encode_sdo_download(mbox_buf, sizeof(mbox_buf), 0x6040, 0x00, &val,
                                                  sizeof(val));
    TEST_ASSERT_EQUAL_INT(10, len);

    /* Check Mailbox CoE header (Type 3 SDO Req) */
    uint16_t coe_hdr = (uint16_t)(mbox_buf[0] | (mbox_buf[1] << 8));
    TEST_ASSERT_EQUAL_UINT16((SYN_ECAT_COE_TYPE_SDO_REQ & 0x0F) << 12, coe_hdr);
    TEST_ASSERT_EQUAL_HEX8(0x40, mbox_buf[3]); /* Index LSB (0x6040 -> 0x40) */
    TEST_ASSERT_EQUAL_HEX8(0x60, mbox_buf[4]); /* Index MSB (0x6040 -> 0x60) */
    TEST_ASSERT_EQUAL_HEX8(0x00, mbox_buf[5]); /* Subindex */

    /* SDO Upload (Read) Request */
    len = syn_ecat_coe_encode_sdo_upload(mbox_buf, sizeof(mbox_buf), 0x6041, 0x00);
    TEST_ASSERT_EQUAL_INT(10, len);
    TEST_ASSERT_EQUAL_HEX8(0x40, mbox_buf[2]); /* 0x40 Upload Command */
    TEST_ASSERT_EQUAL_HEX8(0x41, mbox_buf[3]);
    TEST_ASSERT_EQUAL_HEX8(0x60, mbox_buf[4]);
}

static void test_ecat_edge_cases(void)
{
    SYN_EcatNode node;
    syn_ecat_init(&node, 0x1001, NULL);

    uint8_t buf[64];
    SYN_EcatDatagram dg = {
        .cmd = SYN_ECAT_CMD_APWR, .idx = 0x01, .addr = 0x0000, .m = 1, .circ = 1, .irq = 0x0000};

    /* Buffer capacity error */
    TEST_ASSERT_EQUAL_INT(0, syn_ecat_build_datagram_frame(buf, 10, &dg, NULL, 0));

    /* Build datagram with m=1 and circ=1 flags set */
    size_t len = syn_ecat_build_datagram_frame(buf, sizeof(buf), &dg, NULL, 0);
    TEST_ASSERT_EQUAL_INT(14, len);

    /* Invalid frame type in parser */
    buf[1] = 0x20; /* Frame type = 2 instead of 1 */
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_ecat_parse_frame(&node, buf, len, NULL));

    /* Truncated datagram length in header */
    buf[0] = 0xFF; /* dg_len = 0x07FF (2047 bytes) */
    buf[1] = 0x17;
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_ecat_parse_frame(&node, buf, len, NULL));

    /* SDO Download buffer capacity / payload size overflow */
    uint32_t val = 123;
    TEST_ASSERT_EQUAL_INT(
        0, syn_ecat_coe_encode_sdo_download(buf, sizeof(buf), 0x1000, 0x00, &val, 8));
    TEST_ASSERT_EQUAL_INT(0, syn_ecat_coe_encode_sdo_download(buf, 5, 0x1000, 0x00, &val, 4));

    /* SDO Upload buffer capacity overflow */
    TEST_ASSERT_EQUAL_INT(0, syn_ecat_coe_encode_sdo_upload(buf, 5, 0x1000, 0x00));
}

static void test_ecat_multi_datagram(void)
{
    uint8_t buf[128];
    syn_ecat_frame_begin(buf, sizeof(buf));

    uint8_t d1_payload[2] = {0xAB, 0xCD};
    uint8_t d2_payload[4] = {0x11, 0x22, 0x33, 0x44};

    size_t sz1 = syn_ecat_frame_add_datagram(buf, sizeof(buf), SYN_ECAT_CMD_APRD, 0x01, 0x00000000,
                                             d1_payload, sizeof(d1_payload), false);
    TEST_ASSERT_TRUE(sz1 > 0);

    size_t sz2 = syn_ecat_frame_add_datagram(buf, sizeof(buf), SYN_ECAT_CMD_FPRD, 0x02, 0x10010010,
                                             d2_payload, sizeof(d2_payload), true);
    TEST_ASSERT_TRUE(sz2 > sz1);

    size_t final_sz = syn_ecat_frame_finalize(buf);
    TEST_ASSERT_EQUAL_INT(sz2, final_sz);

    /* Simulate responses on frame (set WKCs) */
    /* Datagram 1 WKC offset = 2 + 10 + 2 = 14 */
    buf[14] = 0x01;
    buf[15] = 0x00;

    /* Datagram 2 WKC offset = 16 + 10 + 4 = 30 */
    buf[30] = 0x02;
    buf[31] = 0x00;

    size_t offset = 2;
    SYN_EcatDatagramResult res;

    TEST_ASSERT_TRUE(syn_ecat_frame_parse_next(buf, final_sz, &offset, &res));
    TEST_ASSERT_EQUAL_HEX8(SYN_ECAT_CMD_APRD, res.cmd);
    TEST_ASSERT_EQUAL_HEX8(0x01, res.idx);
    TEST_ASSERT_EQUAL_HEX32(0x00000000, res.addr);
    TEST_ASSERT_EQUAL_INT(2, res.data_len);
    TEST_ASSERT_EQUAL_HEX8(0xAB, res.data[0]);
    TEST_ASSERT_EQUAL_HEX8(0xCD, res.data[1]);
    TEST_ASSERT_EQUAL_UINT16(1, res.wkc);

    TEST_ASSERT_TRUE(syn_ecat_frame_parse_next(buf, final_sz, &offset, &res));
    TEST_ASSERT_EQUAL_HEX8(SYN_ECAT_CMD_FPRD, res.cmd);
    TEST_ASSERT_EQUAL_HEX8(0x02, res.idx);
    TEST_ASSERT_EQUAL_HEX32(0x10010010, res.addr);
    TEST_ASSERT_EQUAL_INT(4, res.data_len);
    TEST_ASSERT_EQUAL_HEX8(0x11, res.data[0]);
    TEST_ASSERT_EQUAL_UINT16(2, res.wkc);

    TEST_ASSERT_FALSE(syn_ecat_frame_parse_next(buf, final_sz, &offset, &res));
}

static void test_ecat_master_phase2(void)
{
    SYN_EcatMaster m;
    uint8_t tx[128];
    uint8_t rx[128];
    uint8_t out_img[32];
    uint8_t in_img[32];

    /* Invalid init params */
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_ecat_master_init(NULL, tx, sizeof(tx), rx, sizeof(rx), out_img,
                                           sizeof(out_img), in_img, sizeof(in_img)));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM,
                      syn_ecat_master_init(&m, NULL, sizeof(tx), rx, sizeof(rx), out_img,
                                           sizeof(out_img), in_img, sizeof(in_img)));

    /* Valid init */
    TEST_ASSERT_EQUAL(SYN_OK, syn_ecat_master_init(&m, tx, sizeof(tx), rx, sizeof(rx), out_img,
                                                   sizeof(out_img), in_img, sizeof(in_img)));

    /* Scan bus encode / decode */
    size_t len = syn_ecat_encode_scan_bus(&m);
    TEST_ASSERT_TRUE(len > 0);

    /* Copy tx to rx, set WKC to 3 */
    memcpy(rx, tx, len);
    rx[len - 2] = 0x03;
    rx[len - 1] = 0x00;

    uint8_t count = syn_ecat_decode_scan_bus(&m, len);
    TEST_ASSERT_EQUAL_UINT8(3, count);
    TEST_ASSERT_EQUAL_UINT8(3, m.slave_count);

    /* Assign address */
    len = syn_ecat_encode_assign_addr(&m, 0, 0x1001);
    TEST_ASSERT_TRUE(len > 0);

    /* Read SII */
    len = syn_ecat_encode_read_sii(&m, 0x1001, 0x0008);
    TEST_ASSERT_TRUE(len > 0);

    /* Construct SII response in rx: payload 4 bytes = 0x000000E0 */
    syn_ecat_frame_begin(rx, sizeof(rx));
    uint8_t sii_resp[4] = {0xE0, 0x00, 0x00, 0x00};
    size_t rx_len = syn_ecat_frame_add_datagram(rx, sizeof(rx), SYN_ECAT_CMD_FPWR, 0x02, 0x10010502,
                                                sii_resp, 4, false);
    rx[rx_len - 2] = 0x01; /* WKC = 1 */

    uint32_t sii_val = 0;
    TEST_ASSERT_EQUAL(SYN_OK, syn_ecat_decode_read_sii(&m, rx_len, &sii_val));
    TEST_ASSERT_EQUAL_HEX32(0x000000E0, sii_val);

    /* Write SM */
    SYN_EcatSMConfig sm_cfg = {
        .start_addr = 0x1000, .length = 128, .control = 0x26, .enable = 0x01};
    len = syn_ecat_encode_write_sm(&m, 0x1001, 0, &sm_cfg);
    TEST_ASSERT_TRUE(len > 0);

    /* Write FMMU */
    SYN_EcatFMMUConfig fmmu_cfg = {.logical_start = 0x00010000,
                                   .length = 16,
                                   .logical_start_bit = 0,
                                   .logical_end_bit = 7,
                                   .phys_start_addr = 0x1000,
                                   .phys_start_bit = 0,
                                   .type = 0x03,
                                   .enable = true};
    len = syn_ecat_encode_write_fmmu(&m, 0x1001, 0, &fmmu_cfg);
    TEST_ASSERT_TRUE(len > 0);

    /* Read AL Status */
    len = syn_ecat_encode_read_al_status(&m, 0x1001);
    TEST_ASSERT_TRUE(len > 0);

    /* Construct AL Status response in rx (state = PREOP 0x02, status_code = 0x0000) */
    syn_ecat_frame_begin(rx, sizeof(rx));
    uint8_t al_resp[4] = {0x02, 0x00, 0x00, 0x00};
    rx_len = syn_ecat_frame_add_datagram(rx, sizeof(rx), SYN_ECAT_CMD_FPRD, 0x05, 0x10010130,
                                         al_resp, 4, false);
    rx[rx_len - 2] = 0x01;

    SYN_EcatState state = SYN_ECAT_STATE_NONE;
    uint16_t status_code = 0xFFFF;
    TEST_ASSERT_EQUAL(SYN_OK, syn_ecat_decode_read_al_status(&m, rx_len, &state, &status_code));
    TEST_ASSERT_EQUAL(SYN_ECAT_STATE_PREOP, state);
    TEST_ASSERT_EQUAL_UINT16(0x0000, status_code);
}

static inline uint16_t ecat_test_load16_le(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static inline void ecat_test_store16_le(uint8_t *p, uint16_t val)
{
    p[0] = (uint8_t)(val & 0xFF);
    p[1] = (uint8_t)((val >> 8) & 0xFF);
}

static inline void ecat_test_store32_le(uint8_t *p, uint32_t val)
{
    p[0] = (uint8_t)(val & 0xFF);
    p[1] = (uint8_t)((val >> 8) & 0xFF);
    p[2] = (uint8_t)((val >> 16) & 0xFF);
    p[3] = (uint8_t)((val >> 24) & 0xFF);
}

static void test_ecat_master_phases3_to_6(void)
{
    SYN_EcatMaster m;
    uint8_t tx[128];
    uint8_t rx[128];
    uint8_t out_img[16] = {0xDE, 0xAD, 0xBE, 0xEF};
    uint8_t in_img[16] = {0};

    syn_ecat_master_init(&m, tx, sizeof(tx), rx, sizeof(rx), out_img, 4, in_img, 4);
    m.logical_addr = 0x00010000;
    m.wkc_expected = 1;

    /* Phase 3: Mailbox CoE SDO write / read */
    uint32_t val = 0x12345678;
    size_t len = syn_ecat_encode_coe_sdo_write(&m, 0x1001, 0x6040, 0x00, &val, 4);
    TEST_ASSERT_TRUE(len > 0);

    len = syn_ecat_encode_coe_sdo_read(&m, 0x1001, 0x6041, 0x00);
    TEST_ASSERT_TRUE(len > 0);

    /* Construct CoE SDO upload response in rx */
    syn_ecat_frame_begin(rx, sizeof(rx));
    uint8_t sdo_resp_payload[16] = {0};
    ecat_test_store16_le(&sdo_resp_payload[0], 10);
    ecat_test_store16_le(&sdo_resp_payload[2], 0x1001);
    sdo_resp_payload[4] = 0x00;
    sdo_resp_payload[5] = 0x03; /* Mailbox type CoE */
    ecat_test_store16_le(&sdo_resp_payload[6], (SYN_ECAT_COE_TYPE_SDO_RESP & 0x0F) << 12);
    sdo_resp_payload[8] = 0x43; /* 0x43 = expedited 4B */
    ecat_test_store16_le(&sdo_resp_payload[9], 0x6041);
    sdo_resp_payload[11] = 0x00;
    ecat_test_store32_le(&sdo_resp_payload[12], 0x87654321);

    size_t rx_len = syn_ecat_frame_add_datagram(rx, sizeof(rx), SYN_ECAT_CMD_FPRD, 0x07, 0x10011000,
                                                sdo_resp_payload, 16, false);
    rx[rx_len - 2] = 0x01;

    uint32_t read_val = 0;
    size_t out_len = 0;
    TEST_ASSERT_EQUAL(SYN_OK, syn_ecat_decode_coe_sdo_response(&m, rx_len, &read_val,
                                                               sizeof(read_val), &out_len));
    TEST_ASSERT_EQUAL_INT(4, out_len);
    TEST_ASSERT_EQUAL_HEX32(0x87654321, read_val);

    /* Phase 4: Cyclic process exchange */
    len = syn_ecat_encode_cyclic(&m);
    TEST_ASSERT_TRUE(len > 0);

    /* Construct LRW response in rx with input image {0xCA, 0xFE, 0xBA, 0xBE} */
    syn_ecat_frame_begin(rx, sizeof(rx));
    uint8_t cyclic_resp[4] = {0xCA, 0xFE, 0xBA, 0xBE};
    rx_len = syn_ecat_frame_add_datagram(rx, sizeof(rx), SYN_ECAT_CMD_LRW, 0x10, m.logical_addr,
                                         cyclic_resp, 4, false);
    rx[rx_len - 2] = 0x01; /* WKC = 1 (matches expected) */

    TEST_ASSERT_EQUAL(SYN_OK, syn_ecat_decode_cyclic(&m, rx_len));
    TEST_ASSERT_EQUAL_HEX8(0xCA, in_img[0]);
    TEST_ASSERT_EQUAL_HEX8(0xFE, in_img[1]);
    TEST_ASSERT_EQUAL_UINT32(1, m.cycle_count);

    /* Phase 5: Set AL Control */
    len = syn_ecat_encode_set_al_control(&m, 0x1001, SYN_ECAT_STATE_OP);
    TEST_ASSERT_TRUE(len > 0);

    /* Phase 6: DC read system time and configure */
    len = syn_ecat_encode_dc_read_system_time(&m);
    TEST_ASSERT_TRUE(len > 0);

    len = syn_ecat_encode_dc_configure(&m, 0x1001, 1000000U, 0);
    TEST_ASSERT_TRUE(len > 0);
}

static void test_ecat_master_protothread_tasks(void)
{
    SYN_EcatMaster m;
    uint8_t tx[128];
    uint8_t rx[128];
    uint8_t out_img[16] = {0x11, 0x22};
    uint8_t in_img[16] = {0};

    syn_ecat_master_init(&m, tx, sizeof(tx), rx, sizeof(rx), out_img, 2, in_img, 2);
    m.logical_addr = 0x00010000;
    m.wkc_expected = 1;

    SYN_PT pt;
    PT_INIT(&pt);

    /* Null params safety */
    TEST_ASSERT_EQUAL(PT_ENDED, syn_ecat_master_scan_task(NULL, &m));
    TEST_ASSERT_EQUAL(PT_ENDED, syn_ecat_master_transition_task(NULL, &m, SYN_ECAT_STATE_PREOP));
    TEST_ASSERT_EQUAL(PT_ENDED, syn_ecat_master_cyclic_task(NULL, &m));

    /* 1. Step scan task (step 1: sends BRD scan) */
    SYN_PT_Status status = syn_ecat_master_scan_task(&pt, &m);
    TEST_ASSERT_EQUAL(PT_WAITING, status);

    const uint8_t *tx_ptr = NULL;
    size_t tx_len = 0;
    TEST_ASSERT_TRUE(syn_ecat_master_pop_tx_frame(&m, &tx_ptr, &tx_len));
    TEST_ASSERT_TRUE(tx_len > 0);

    /* Feed mock scan response (WKC = 1 slave) */
    memcpy(rx, tx_ptr, tx_len);
    rx[tx_len - 2] = 0x01;
    TEST_ASSERT_EQUAL(SYN_OK, syn_ecat_master_set_rx_frame(&m, rx, tx_len));

    /* Step scan task (step 2: assigns address for slave 0) */
    status = syn_ecat_master_scan_task(&pt, &m);
    TEST_ASSERT_EQUAL(PT_WAITING, status);
    TEST_ASSERT_TRUE(syn_ecat_master_pop_tx_frame(&m, &tx_ptr, &tx_len));

    /* Feed mock assign address response */
    memcpy(rx, tx_ptr, tx_len);
    rx[tx_len - 2] = 0x01;
    TEST_ASSERT_EQUAL(SYN_OK, syn_ecat_master_set_rx_frame(&m, rx, tx_len));

    /* Complete scan task */
    status = syn_ecat_master_scan_task(&pt, &m);
    TEST_ASSERT_EQUAL(PT_EXITED, status);
    TEST_ASSERT_EQUAL_UINT8(1, m.slave_count);
    TEST_ASSERT_EQUAL_HEX16(0x1001, m.slaves[0].station_addr);

    /* 2. Transition task to PREOP */
    PT_INIT(&pt);
    status = syn_ecat_master_transition_task(&pt, &m, SYN_ECAT_STATE_PREOP);
    TEST_ASSERT_EQUAL(PT_WAITING, status);

    /* Process pending TX frames for SM0, SM1, AL Control, AL Status */
    int guard1 = 0;
    while (status == PT_WAITING && guard1++ < 1000) {
        if (syn_ecat_master_pop_tx_frame(&m, &tx_ptr, &tx_len)) {
            memcpy(rx, tx_ptr, tx_len);
            /* If AL status read datagram (4B response), set AL status = PREOP 0x02 */
            if (tx_len >= 18 && rx[2] == SYN_ECAT_CMD_FPRD) {
                syn_ecat_frame_begin(rx, sizeof(rx));
                uint8_t al_resp[4] = {0x02, 0x00, 0x00, 0x00};
                tx_len = syn_ecat_frame_add_datagram(rx, sizeof(rx), SYN_ECAT_CMD_FPRD, 0x05,
                                                     0x10010130, al_resp, 4, false);
            }
            rx[tx_len - 2] = 0x01;
            syn_ecat_master_set_rx_frame(&m, rx, tx_len);
        }
        status = syn_ecat_master_transition_task(&pt, &m, SYN_ECAT_STATE_PREOP);
    }
    TEST_ASSERT_EQUAL(PT_EXITED, status);
    TEST_ASSERT_EQUAL(SYN_ECAT_STATE_PREOP, m.master_state);

    /* 3. Cyclic task */
    PT_INIT(&pt);
    status = syn_ecat_master_cyclic_task(&pt, &m);
    TEST_ASSERT_EQUAL(PT_WAITING, status);
    TEST_ASSERT_TRUE(syn_ecat_master_pop_tx_frame(&m, &tx_ptr, &tx_len));

    syn_ecat_frame_begin(rx, sizeof(rx));
    uint8_t c_resp[2] = {0x99, 0x88};
    size_t rx_l = syn_ecat_frame_add_datagram(rx, sizeof(rx), SYN_ECAT_CMD_LRW, 0x10,
                                              m.logical_addr, c_resp, 2, false);
    rx[rx_l - 2] = 0x01;
    syn_ecat_master_set_rx_frame(&m, rx, rx_l);

    status = syn_ecat_master_cyclic_task(&pt, &m);
    TEST_ASSERT_EQUAL(PT_EXITED, status);
    TEST_ASSERT_EQUAL_HEX8(0x99, in_img[0]);
    TEST_ASSERT_EQUAL_HEX8(0x88, in_img[1]);
}

static void test_ecat_master_sii_and_sdo_tasks(void)
{
    SYN_EcatMaster m;
    uint8_t tx[256];
    uint8_t rx[256];
    uint8_t out_img[16] = {0};
    uint8_t in_img[16] = {0};

    syn_ecat_master_init(&m, tx, sizeof(tx), rx, sizeof(rx), out_img, 2, in_img, 2);

    SYN_PT pt;
    PT_INIT(&pt);

    uint32_t sii_data = 0;
    /* Null params safety */
    TEST_ASSERT_EQUAL(PT_ENDED, syn_ecat_master_read_sii_task(NULL, &m, 0x1001, 0x08, &sii_data));

    /* 1. SII Read task step 1: write address offset */
    SYN_PT_Status status = syn_ecat_master_read_sii_task(&pt, &m, 0x1001, 0x08, &sii_data);
    TEST_ASSERT_EQUAL(PT_WAITING, status);

    const uint8_t *tx_ptr = NULL;
    size_t tx_len = 0;
    TEST_ASSERT_TRUE(syn_ecat_master_pop_tx_frame(&m, &tx_ptr, &tx_len));
    memcpy(rx, tx_ptr, tx_len);
    rx[tx_len - 2] = 0x01;
    syn_ecat_master_set_rx_frame(&m, rx, tx_len);

    /* SII Read task step 2: write command */
    status = syn_ecat_master_read_sii_task(&pt, &m, 0x1001, 0x08, &sii_data);
    TEST_ASSERT_EQUAL(PT_WAITING, status);
    TEST_ASSERT_TRUE(syn_ecat_master_pop_tx_frame(&m, &tx_ptr, &tx_len));
    memcpy(rx, tx_ptr, tx_len);
    rx[tx_len - 2] = 0x01;
    syn_ecat_master_set_rx_frame(&m, rx, tx_len);

    /* SII Read task step 3: poll status (send 0x0000 = not busy) */
    status = syn_ecat_master_read_sii_task(&pt, &m, 0x1001, 0x08, &sii_data);
    TEST_ASSERT_EQUAL(PT_WAITING, status);
    TEST_ASSERT_TRUE(syn_ecat_master_pop_tx_frame(&m, &tx_ptr, &tx_len));

    syn_ecat_frame_begin(rx, sizeof(rx));
    uint8_t not_busy[2] = {0x00, 0x00};
    size_t rx_len = syn_ecat_frame_add_datagram(rx, sizeof(rx), SYN_ECAT_CMD_FPRD, 0x01, 0x10010502,
                                                not_busy, 2, false);
    rx[rx_len - 2] = 0x01;
    syn_ecat_master_set_rx_frame(&m, rx, rx_len);

    /* SII Read task step 4: read data 0x12345678 */
    status = syn_ecat_master_read_sii_task(&pt, &m, 0x1001, 0x08, &sii_data);
    TEST_ASSERT_EQUAL(PT_WAITING, status);
    TEST_ASSERT_TRUE(syn_ecat_master_pop_tx_frame(&m, &tx_ptr, &tx_len));

    syn_ecat_frame_begin(rx, sizeof(rx));
    uint8_t sii_val[4] = {0x78, 0x56, 0x34, 0x12};
    rx_len = syn_ecat_frame_add_datagram(rx, sizeof(rx), SYN_ECAT_CMD_FPRD, 0x01, 0x10010508,
                                         sii_val, 4, false);
    rx[rx_len - 2] = 0x01;
    syn_ecat_master_set_rx_frame(&m, rx, rx_len);

    status = syn_ecat_master_read_sii_task(&pt, &m, 0x1001, 0x08, &sii_data);
    TEST_ASSERT_EQUAL(PT_EXITED, status);
    TEST_ASSERT_EQUAL_HEX32(0x12345678, sii_data);

    /* 2. CoE SDO Read task */
    PT_INIT(&pt);
    uint32_t sdo_data = 0;
    size_t out_len = 0;
    TEST_ASSERT_EQUAL(PT_ENDED, syn_ecat_master_sdo_read_task(NULL, &m, 0x1001, 0x6041, 0,
                                                              &sdo_data, 4, &out_len));

    /* SDO read step 1: write request to SM0 */
    status = syn_ecat_master_sdo_read_task(&pt, &m, 0x1001, 0x6041, 0, &sdo_data, 4, &out_len);
    TEST_ASSERT_EQUAL(PT_WAITING, status);
    TEST_ASSERT_TRUE(syn_ecat_master_pop_tx_frame(&m, &tx_ptr, &tx_len));
    memcpy(rx, tx_ptr, tx_len);
    rx[tx_len - 2] = 0x01;
    syn_ecat_master_set_rx_frame(&m, rx, tx_len);

    /* SDO read step 2: poll SM1 status (0x08 = full) */
    status = syn_ecat_master_sdo_read_task(&pt, &m, 0x1001, 0x6041, 0, &sdo_data, 4, &out_len);
    TEST_ASSERT_EQUAL(PT_WAITING, status);
    TEST_ASSERT_TRUE(syn_ecat_master_pop_tx_frame(&m, &tx_ptr, &tx_len));

    syn_ecat_frame_begin(rx, sizeof(rx));
    uint8_t sm1_full[1] = {0x08};
    rx_len = syn_ecat_frame_add_datagram(rx, sizeof(rx), SYN_ECAT_CMD_FPRD, 0x01, 0x1001080D,
                                         sm1_full, 1, false);
    rx[rx_len - 2] = 0x01;
    syn_ecat_master_set_rx_frame(&m, rx, rx_len);

    /* SDO read step 3: read SM1 response (0x87654321) */
    status = syn_ecat_master_sdo_read_task(&pt, &m, 0x1001, 0x6041, 0, &sdo_data, 4, &out_len);
    TEST_ASSERT_EQUAL(PT_WAITING, status);
    TEST_ASSERT_TRUE(syn_ecat_master_pop_tx_frame(&m, &tx_ptr, &tx_len));

    uint8_t sdo_resp_payload[16] = {0};
    ecat_test_store16_le(&sdo_resp_payload[0], 0x000A);
    ecat_test_store16_le(&sdo_resp_payload[2], 0x1001);
    sdo_resp_payload[4] = 0x00;
    sdo_resp_payload[5] = 0x03;
    ecat_test_store16_le(&sdo_resp_payload[6], (SYN_ECAT_COE_TYPE_SDO_RESP & 0x0F) << 12);
    sdo_resp_payload[8] = 0x43;
    ecat_test_store16_le(&sdo_resp_payload[9], 0x6041);
    sdo_resp_payload[11] = 0x00;
    ecat_test_store32_le(&sdo_resp_payload[12], 0x87654321);

    syn_ecat_frame_begin(rx, sizeof(rx));
    rx_len = syn_ecat_frame_add_datagram(rx, sizeof(rx), SYN_ECAT_CMD_FPRD, 0x01, 0x10011080,
                                         sdo_resp_payload, 16, false);
    rx[rx_len - 2] = 0x01;
    syn_ecat_master_set_rx_frame(&m, rx, rx_len);

    status = syn_ecat_master_sdo_read_task(&pt, &m, 0x1001, 0x6041, 0, &sdo_data, 4, &out_len);
    TEST_ASSERT_EQUAL(PT_EXITED, status);
    TEST_ASSERT_EQUAL_HEX32(0x87654321, sdo_data);
}

static void test_ecat_reg_and_task_edge_cases(void)
{
    SYN_EcatMaster m;
    uint8_t tx[256];
    uint8_t rx[256];
    uint8_t out_img[16] = {0};
    uint8_t in_img[16] = {0};

    syn_ecat_master_init(&m, tx, sizeof(tx), rx, sizeof(rx), out_img, 2, in_img, 2);

    /* Null / zero edge cases for reg encode/decode */
    TEST_ASSERT_EQUAL_INT(0, syn_ecat_encode_read_reg(NULL, 0x1001, 0x0502, 2));
    TEST_ASSERT_EQUAL_INT(0, syn_ecat_encode_read_reg(&m, 0x1001, 0x0502, 0));
    TEST_ASSERT_EQUAL_INT(0, syn_ecat_encode_write_reg(NULL, 0x1001, 0x0502, tx, 2));
    TEST_ASSERT_EQUAL_INT(0, syn_ecat_encode_write_reg(&m, 0x1001, 0x0502, NULL, 2));
    TEST_ASSERT_EQUAL_INT(0, syn_ecat_encode_write_reg(&m, 0x1001, 0x0502, tx, 0));

    uint8_t dummy[4] = {0};
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_ecat_decode_read_reg(NULL, 18, dummy, 2));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_ecat_decode_read_reg(&m, 10, dummy, 2));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_ecat_decode_read_reg(&m, 18, NULL, 2));

    /* Decode reg failure when WKC is 0 */
    syn_ecat_frame_begin(rx, sizeof(rx));
    uint8_t reg_val[2] = {0x12, 0x34};
    size_t rx_len = syn_ecat_frame_add_datagram(rx, sizeof(rx), SYN_ECAT_CMD_FPRD, 0x01, 0x10010502,
                                                reg_val, 2, false);
    rx[rx_len - 2] = 0x00; /* WKC = 0 (failure) */
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_ecat_decode_read_reg(&m, rx_len, dummy, 2));

    /* Test SII polling loop with 1 busy response (0x8000) then 1 clear response (0x0000) */
    SYN_PT pt;
    PT_INIT(&pt);
    uint32_t sii_val = 0;
    syn_ecat_master_read_sii_task(&pt, &m, 0x1001, 0x08, &sii_val); /* step 1 */
    const uint8_t *tx_ptr = NULL;
    size_t tx_len = 0;
    syn_ecat_master_pop_tx_frame(&m, &tx_ptr, &tx_len);
    memcpy(rx, tx_ptr, tx_len);
    rx[tx_len - 2] = 1;
    syn_ecat_master_set_rx_frame(&m, rx, tx_len);

    syn_ecat_master_read_sii_task(&pt, &m, 0x1001, 0x08, &sii_val); /* step 2 */
    syn_ecat_master_pop_tx_frame(&m, &tx_ptr, &tx_len);
    memcpy(rx, tx_ptr, tx_len);
    rx[tx_len - 2] = 1;
    syn_ecat_master_set_rx_frame(&m, rx, tx_len);

    syn_ecat_master_read_sii_task(&pt, &m, 0x1001, 0x08, &sii_val); /* step 3 poll (busy) */
    syn_ecat_master_pop_tx_frame(&m, &tx_ptr, &tx_len);
    syn_ecat_frame_begin(rx, sizeof(rx));
    uint8_t busy[2] = {0x00, 0x80}; /* Busy bit set */
    rx_len = syn_ecat_frame_add_datagram(rx, sizeof(rx), SYN_ECAT_CMD_FPRD, 0x01, 0x10010502, busy,
                                         2, false);
    rx[rx_len - 2] = 1;
    syn_ecat_master_set_rx_frame(&m, rx, rx_len);

    /* Poll again (not busy) */
    syn_ecat_master_read_sii_task(&pt, &m, 0x1001, 0x08, &sii_val);
    syn_ecat_master_pop_tx_frame(&m, &tx_ptr, &tx_len);
    syn_ecat_frame_begin(rx, sizeof(rx));
    uint8_t idle[2] = {0x00, 0x00};
    rx_len = syn_ecat_frame_add_datagram(rx, sizeof(rx), SYN_ECAT_CMD_FPRD, 0x01, 0x10010502, idle,
                                         2, false);
    rx[rx_len - 2] = 1;
    syn_ecat_master_set_rx_frame(&m, rx, rx_len);

    /* Read data */
    syn_ecat_master_read_sii_task(&pt, &m, 0x1001, 0x08, &sii_val);
    syn_ecat_master_pop_tx_frame(&m, &tx_ptr, &tx_len);
    syn_ecat_frame_begin(rx, sizeof(rx));
    uint8_t val32[4] = {0x11, 0x22, 0x33, 0x44};
    rx_len = syn_ecat_frame_add_datagram(rx, sizeof(rx), SYN_ECAT_CMD_FPRD, 0x01, 0x10010508, val32,
                                         4, false);
    rx[rx_len - 2] = 1;
    syn_ecat_master_set_rx_frame(&m, rx, rx_len);

    SYN_PT_Status status = syn_ecat_master_read_sii_task(&pt, &m, 0x1001, 0x08, &sii_val);
    TEST_ASSERT_EQUAL(PT_EXITED, status);
    TEST_ASSERT_EQUAL_HEX32(0x44332211, sii_val);

    /* SDO Read task 16-bit output buffer branch */
    PT_INIT(&pt);
    uint16_t sdo16 = 0;
    size_t out16 = 0;
    syn_ecat_master_sdo_read_task(&pt, &m, 0x1001, 0x1000, 0, &sdo16, 2, &out16); /* step 1 */
    syn_ecat_master_pop_tx_frame(&m, &tx_ptr, &tx_len);
    memcpy(rx, tx_ptr, tx_len);
    rx[tx_len - 2] = 1;
    syn_ecat_master_set_rx_frame(&m, rx, tx_len);

    syn_ecat_master_sdo_read_task(&pt, &m, 0x1001, 0x1000, 0, &sdo16, 2,
                                  &out16); /* step 2 poll empty then full */
    syn_ecat_master_pop_tx_frame(&m, &tx_ptr, &tx_len);
    syn_ecat_frame_begin(rx, sizeof(rx));
    uint8_t sm1_empty[1] = {0x00};
    rx_len = syn_ecat_frame_add_datagram(rx, sizeof(rx), SYN_ECAT_CMD_FPRD, 0x01, 0x1001080D,
                                         sm1_empty, 1, false);
    rx[rx_len - 2] = 1;
    syn_ecat_master_set_rx_frame(&m, rx, rx_len);

    syn_ecat_master_sdo_read_task(&pt, &m, 0x1001, 0x1000, 0, &sdo16, 2, &out16);
    syn_ecat_master_pop_tx_frame(&m, &tx_ptr, &tx_len);
    syn_ecat_frame_begin(rx, sizeof(rx));
    uint8_t sm1_ready[1] = {0x08};
    rx_len = syn_ecat_frame_add_datagram(rx, sizeof(rx), SYN_ECAT_CMD_FPRD, 0x01, 0x1001080D,
                                         sm1_ready, 1, false);
    rx[rx_len - 2] = 1;
    syn_ecat_master_set_rx_frame(&m, rx, rx_len);

    syn_ecat_master_sdo_read_task(&pt, &m, 0x1001, 0x1000, 0, &sdo16, 2,
                                  &out16); /* step 3 read response */
    syn_ecat_master_pop_tx_frame(&m, &tx_ptr, &tx_len);
    uint8_t payload16[16] = {0};
    ecat_test_store16_le(&payload16[0], 0x000A);
    ecat_test_store16_le(&payload16[2], 0x1001);
    payload16[5] = 0x03;
    ecat_test_store16_le(&payload16[6], (SYN_ECAT_COE_TYPE_SDO_RESP & 0x0F) << 12);
    payload16[8] = 0x47; /* 2-byte expedited */
    ecat_test_store16_le(&payload16[9], 0x1000);
    ecat_test_store16_le(&payload16[12], 0xABCD);
    syn_ecat_frame_begin(rx, sizeof(rx));
    rx_len = syn_ecat_frame_add_datagram(rx, sizeof(rx), SYN_ECAT_CMD_FPRD, 0x01, 0x10011080,
                                         payload16, 16, false);
    rx[rx_len - 2] = 1;
    syn_ecat_master_set_rx_frame(&m, rx, rx_len);

    status = syn_ecat_master_sdo_read_task(&pt, &m, 0x1001, 0x1000, 0, &sdo16, 2, &out16);
    TEST_ASSERT_EQUAL(PT_EXITED, status);
    TEST_ASSERT_EQUAL_HEX16(0xABCD, sdo16);

    /* 8-bit output buffer branch */
    PT_INIT(&pt);
    uint8_t sdo8 = 0;
    size_t out8 = 0;
    syn_ecat_master_sdo_read_task(&pt, &m, 0x1001, 0x1000, 0, &sdo8, 1, &out8);
    syn_ecat_master_pop_tx_frame(&m, &tx_ptr, &tx_len);
    memcpy(rx, tx_ptr, tx_len);
    rx[tx_len - 2] = 1;
    syn_ecat_master_set_rx_frame(&m, rx, tx_len);
    syn_ecat_master_sdo_read_task(&pt, &m, 0x1001, 0x1000, 0, &sdo8, 1, &out8);
    syn_ecat_master_pop_tx_frame(&m, &tx_ptr, &tx_len);
    syn_ecat_frame_begin(rx, sizeof(rx));
    uint8_t sm1_r[1] = {0x08};
    rx_len = syn_ecat_frame_add_datagram(rx, sizeof(rx), SYN_ECAT_CMD_FPRD, 0x01, 0x1001080D, sm1_r,
                                         1, false);
    rx[rx_len - 2] = 1;
    syn_ecat_master_set_rx_frame(&m, rx, rx_len);
    syn_ecat_master_sdo_read_task(&pt, &m, 0x1001, 0x1000, 0, &sdo8, 1, &out8);
    syn_ecat_master_pop_tx_frame(&m, &tx_ptr, &tx_len);
    uint8_t payload8[16] = {0};
    ecat_test_store16_le(&payload8[0], 0x000A);
    ecat_test_store16_le(&payload8[2], 0x1001);
    payload8[5] = 0x03;
    ecat_test_store16_le(&payload8[6], (SYN_ECAT_COE_TYPE_SDO_RESP & 0x0F) << 12);
    payload8[8] = 0x4F;
    payload8[12] = 0x55;
    syn_ecat_frame_begin(rx, sizeof(rx));
    rx_len = syn_ecat_frame_add_datagram(rx, sizeof(rx), SYN_ECAT_CMD_FPRD, 0x01, 0x10011080,
                                         payload8, 16, false);
    rx[rx_len - 2] = 1;
    syn_ecat_master_set_rx_frame(&m, rx, rx_len);
    status = syn_ecat_master_sdo_read_task(&pt, &m, 0x1001, 0x1000, 0, &sdo8, 1, &out8);
    TEST_ASSERT_EQUAL(PT_EXITED, status);
    TEST_ASSERT_EQUAL_HEX8(0x55, sdo8);
}

static void test_ecat_pdo_mapping_discovery_task(void)
{
    SYN_EcatMaster m;
    uint8_t tx[256];
    uint8_t rx[256];
    uint8_t out_img[16] = {0};
    uint8_t in_img[16] = {0};

    syn_ecat_master_init(&m, tx, sizeof(tx), rx, sizeof(rx), out_img, 2, in_img, 2);

    SYN_PT pt;
    PT_INIT(&pt);

    uint16_t rx_bytes = 0;
    uint16_t tx_bytes = 0;

    /* Null params safety */
    TEST_ASSERT_EQUAL(PT_ENDED, syn_ecat_master_discover_pdo_mapping_task(NULL, &m, 0x1001,
                                                                          &rx_bytes, &tx_bytes));

    /* Run PDO discovery with 1 RxPDO (0x1600 mapping 16-bit) and 1 TxPDO (0x1A00 mapping 16-bit) */
    SYN_PT_Status status =
        syn_ecat_master_discover_pdo_mapping_task(&pt, &m, 0x1001, &rx_bytes, &tx_bytes);

    const uint8_t *tx_ptr = NULL;
    size_t tx_len = 0;
    uint16_t req_idx = 0;
    uint8_t req_sub = 0;

    int guard2 = 0;
    while (status == PT_WAITING && guard2++ < 1000) {
        if (syn_ecat_master_pop_tx_frame(&m, &tx_ptr, &tx_len)) {
            memcpy(rx, tx_ptr, tx_len);
            /* If writing SM0 request, capture index/subindex */
            if (tx_len >= 24 && rx[2] == SYN_ECAT_CMD_FPWR &&
                ecat_test_load16_le(&rx[4]) == 0x1000) {
                req_idx = ecat_test_load16_le(&rx[21]);
                req_sub = rx[23];
            }
            /* If reading SM1 mailbox status (0x080D), return SM1 full (0x08) */
            else if (tx_len >= 15 && rx[2] == SYN_ECAT_CMD_FPRD &&
                     ecat_test_load16_le(&rx[4]) == 0x080D) {
                syn_ecat_frame_begin(rx, sizeof(rx));
                uint8_t ready[1] = {0x08};
                tx_len = syn_ecat_frame_add_datagram(rx, sizeof(rx), SYN_ECAT_CMD_FPRD, 0x01,
                                                     0x1001080D, ready, 1, false);
            }
            /* If reading SM1 mailbox buffer (0x1080), return appropriate response */
            else if (tx_len >= 18 && rx[2] == SYN_ECAT_CMD_FPRD &&
                     ecat_test_load16_le(&rx[4]) == 0x1080) {
                uint8_t payload[16] = {0};
                ecat_test_store16_le(&payload[0], 0x000A);
                ecat_test_store16_le(&payload[2], 0x1001);
                payload[5] = 0x03;
                ecat_test_store16_le(&payload[6], (SYN_ECAT_COE_TYPE_SDO_RESP & 0x0F) << 12);
                ecat_test_store16_le(&payload[9], req_idx);
                payload[11] = req_sub;

                if (req_idx == 0x1C12 && req_sub == 0) {
                    payload[8] = 0x4F;
                    payload[12] = 0x01; /* 1 RxPDO */
                } else if (req_idx == 0x1C12 && req_sub == 1) {
                    payload[8] = 0x47;
                    ecat_test_store16_le(&payload[12], 0x1600); /* mapping obj 0x1600 */
                } else if (req_idx == 0x1600 && req_sub == 0) {
                    payload[8] = 0x4F;
                    payload[12] = 0x01; /* 1 entry mapped */
                } else if (req_idx == 0x1600 && req_sub == 1) {
                    payload[8] = 0x43;
                    ecat_test_store32_le(&payload[12], 0x60400010); /* 16 bits */
                } else if (req_idx == 0x1C13 && req_sub == 0) {
                    payload[8] = 0x4F;
                    payload[12] = 0x01; /* 1 TxPDO */
                } else if (req_idx == 0x1C13 && req_sub == 1) {
                    payload[8] = 0x47;
                    ecat_test_store16_le(&payload[12], 0x1A00); /* mapping obj 0x1A00 */
                } else if (req_idx == 0x1A00 && req_sub == 0) {
                    payload[8] = 0x4F;
                    payload[12] = 0x01; /* 1 entry mapped */
                } else if (req_idx == 0x1A00 && req_sub == 1) {
                    payload[8] = 0x43;
                    ecat_test_store32_le(&payload[12], 0x60410010); /* 16 bits */
                }

                syn_ecat_frame_begin(rx, sizeof(rx));
                tx_len = syn_ecat_frame_add_datagram(rx, sizeof(rx), SYN_ECAT_CMD_FPRD, 0x01,
                                                     0x10011080, payload, 16, false);
            }
            rx[tx_len - 2] = 0x01;
            syn_ecat_master_set_rx_frame(&m, rx, tx_len);
        }
        status = syn_ecat_master_discover_pdo_mapping_task(&pt, &m, 0x1001, &rx_bytes, &tx_bytes);
    }

    TEST_ASSERT_EQUAL(PT_EXITED, status);
    TEST_ASSERT_EQUAL_UINT16(2, rx_bytes);
    TEST_ASSERT_EQUAL_UINT16(2, tx_bytes);
}

static void test_ecat_task_poll_timeouts(void)
{
    SYN_EcatMaster m;
    uint8_t tx[256];
    uint8_t rx[256];
    uint8_t out_img[16] = {0};
    uint8_t in_img[16] = {0};

    syn_ecat_master_init(&m, tx, sizeof(tx), rx, sizeof(rx), out_img, 2, in_img, 2);

    /* 1. SII task timeout test */
    SYN_PT pt;
    PT_INIT(&pt);
    uint32_t sii_val = 0;
    syn_ecat_master_read_sii_task(&pt, &m, 0x1001, 0x08, &sii_val); /* step 1 write addr */
    const uint8_t *tx_ptr = NULL;
    size_t tx_len = 0;
    syn_ecat_master_pop_tx_frame(&m, &tx_ptr, &tx_len);
    memcpy(rx, tx_ptr, tx_len);
    rx[tx_len - 2] = 1;
    syn_ecat_master_set_rx_frame(&m, rx, tx_len);

    syn_ecat_master_read_sii_task(&pt, &m, 0x1001, 0x08, &sii_val); /* step 2 write cmd */
    syn_ecat_master_pop_tx_frame(&m, &tx_ptr, &tx_len);
    memcpy(rx, tx_ptr, tx_len);
    rx[tx_len - 2] = 1;
    syn_ecat_master_set_rx_frame(&m, rx, tx_len);

    /* Force poll_retries = 1 to test immediate timeout on next poll step */
    syn_ecat_master_read_sii_task(&pt, &m, 0x1001, 0x08, &sii_val); /* step 3 poll start */
    syn_ecat_master_pop_tx_frame(&m, &tx_ptr, &tx_len);
    m.poll_retries = 1;

    syn_ecat_frame_begin(rx, sizeof(rx));
    uint8_t busy[2] = {0x00, 0x80}; /* Busy bit set */
    size_t rx_len = syn_ecat_frame_add_datagram(rx, sizeof(rx), SYN_ECAT_CMD_FPRD, 0x01, 0x10010502,
                                                busy, 2, false);
    rx[rx_len - 2] = 1;
    syn_ecat_master_set_rx_frame(&m, rx, rx_len);

    SYN_PT_Status status = syn_ecat_master_read_sii_task(&pt, &m, 0x1001, 0x08, &sii_val);
    TEST_ASSERT_EQUAL(PT_ENDED, status);

    /* 2. SDO task timeout test */
    PT_INIT(&pt);
    uint32_t sdo_val = 0;
    size_t out_len = 0;
    syn_ecat_master_sdo_read_task(&pt, &m, 0x1001, 0x6040, 0, &sdo_val, 4,
                                  &out_len); /* step 1 write req */
    syn_ecat_master_pop_tx_frame(&m, &tx_ptr, &tx_len);
    memcpy(rx, tx_ptr, tx_len);
    rx[tx_len - 2] = 1;
    syn_ecat_master_set_rx_frame(&m, rx, tx_len);

    syn_ecat_master_sdo_read_task(&pt, &m, 0x1001, 0x6040, 0, &sdo_val, 4,
                                  &out_len); /* step 2 poll start */
    syn_ecat_master_pop_tx_frame(&m, &tx_ptr, &tx_len);
    m.poll_retries = 1;

    syn_ecat_frame_begin(rx, sizeof(rx));
    uint8_t not_ready[1] = {0x00}; /* SM1 not ready */
    rx_len = syn_ecat_frame_add_datagram(rx, sizeof(rx), SYN_ECAT_CMD_FPRD, 0x01, 0x1001080D,
                                         not_ready, 1, false);
    rx[rx_len - 2] = 1;
    syn_ecat_master_set_rx_frame(&m, rx, rx_len);

    status = syn_ecat_master_sdo_read_task(&pt, &m, 0x1001, 0x6040, 0, &sdo_val, 4, &out_len);
    TEST_ASSERT_EQUAL(PT_ENDED, status);
}

void test_ecat_null_and_bounds_coverage(void)
{
    uint8_t buf[64] = {0};
    size_t offset = 0;
    SYN_EcatDatagramResult res;

    syn_ecat_frame_begin(NULL, 64);
    syn_ecat_frame_begin(buf, 1);
    TEST_ASSERT_EQUAL_size_t(
        0, syn_ecat_frame_add_datagram(NULL, 64, SYN_ECAT_CMD_NOP, 0, 0, NULL, 0, false));
    TEST_ASSERT_EQUAL_size_t(
        0, syn_ecat_frame_add_datagram(buf, 10, SYN_ECAT_CMD_NOP, 0, 0, NULL, 0, false));

    syn_ecat_frame_begin(buf, sizeof(buf));
    TEST_ASSERT_EQUAL_size_t(
        0, syn_ecat_frame_add_datagram(buf, 14, SYN_ECAT_CMD_NOP, 0, 0, buf, 100, false));

    TEST_ASSERT_EQUAL_size_t(0, syn_ecat_frame_finalize(NULL));

    TEST_ASSERT_FALSE(syn_ecat_frame_parse_next(NULL, 64, &offset, &res));
    TEST_ASSERT_FALSE(syn_ecat_frame_parse_next(buf, 10, &offset, &res));

    buf[0] = 0x00;
    buf[1] = 0x10;
    offset = 2;
    TEST_ASSERT_FALSE(syn_ecat_frame_parse_next(buf, 64, &offset, &res));

    syn_ecat_frame_begin(buf, sizeof(buf));
    buf[0] = 0x20;
    buf[1] = 0x00;
    offset = 2;
    TEST_ASSERT_FALSE(syn_ecat_frame_parse_next(buf, 14, &offset, &res));

    offset = 1;
    TEST_ASSERT_FALSE(syn_ecat_frame_parse_next(buf, 64, &offset, &res));
    offset = 100;
    TEST_ASSERT_FALSE(syn_ecat_frame_parse_next(buf, 64, &offset, &res));

    syn_ecat_frame_begin(buf, sizeof(buf));
    buf[0] = 0x05;
    buf[1] = 0x00;
    offset = 2;
    TEST_ASSERT_FALSE(syn_ecat_frame_parse_next(buf, 64, &offset, &res));

    /* Null & size checks for scan_bus, assign_addr, read_sii */
    SYN_EcatMaster m;
    uint32_t val = 0;
    syn_ecat_master_init(&m, buf, sizeof(buf), buf, sizeof(buf), buf, sizeof(buf), buf,
                         sizeof(buf));

    TEST_ASSERT_EQUAL_size_t(0, syn_ecat_encode_scan_bus(NULL));
    TEST_ASSERT_EQUAL_UINT8(0, syn_ecat_decode_scan_bus(NULL, 64));
    TEST_ASSERT_EQUAL_UINT8(0, syn_ecat_decode_scan_bus(&m, 10));

    TEST_ASSERT_EQUAL_size_t(0, syn_ecat_encode_assign_addr(NULL, 0, 0));
    TEST_ASSERT_EQUAL_size_t(0, syn_ecat_encode_read_sii(NULL, 0, 0));

    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_ecat_decode_read_sii(NULL, 64, NULL));
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_ecat_decode_read_sii(&m, 10, &val));

    /* Multiple datagram chain traversal (3 datagrams) */
    syn_ecat_frame_begin(buf, sizeof(buf));
    syn_ecat_frame_add_datagram(buf, sizeof(buf), SYN_ECAT_CMD_NOP, 0, 0, NULL, 0, false);
    syn_ecat_frame_add_datagram(buf, sizeof(buf), SYN_ECAT_CMD_NOP, 1, 0, NULL, 0, false);
    syn_ecat_frame_add_datagram(buf, sizeof(buf), SYN_ECAT_CMD_NOP, 2, 0, NULL, 0, false);

    /* Datagram length exceeds total datagram boundary */
    syn_ecat_frame_begin(buf, sizeof(buf));
    buf[0] = 0x0E;
    buf[1] = 0x00;
    buf[2] = SYN_ECAT_CMD_NOP;
    buf[3] = 0x00;
    buf[4] = 0;
    buf[5] = 0;
    buf[6] = 0;
    buf[7] = 0;
    buf[8] = 0x64;
    buf[9] = 0x00;
    offset = 2;
    TEST_ASSERT_FALSE(syn_ecat_frame_parse_next(buf, 64, &offset, &res));

    /* Null & parameter bounds checks for SM, FMMU, AL status, CoE SDO write */
    SYN_EcatSMConfig sm_cfg = {0};
    SYN_EcatFMMUConfig fmmu_cfg = {0};
    SYN_EcatState state;
    uint16_t status_code;

    TEST_ASSERT_EQUAL_size_t(0, syn_ecat_encode_write_sm(NULL, 0, 0, NULL));
    TEST_ASSERT_EQUAL_size_t(0, syn_ecat_encode_write_sm(&m, 0, 5, &sm_cfg));
    TEST_ASSERT_EQUAL_size_t(0, syn_ecat_encode_write_fmmu(NULL, 0, 0, NULL));
    TEST_ASSERT_EQUAL_size_t(0, syn_ecat_encode_write_fmmu(&m, 0, 5, &fmmu_cfg));
    TEST_ASSERT_EQUAL_size_t(0, syn_ecat_encode_read_al_status(NULL, 0));
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_ecat_decode_read_al_status(NULL, 64, NULL, NULL));
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM,
                          syn_ecat_decode_read_al_status(&m, 10, &state, &status_code));
    TEST_ASSERT_EQUAL_size_t(0, syn_ecat_encode_coe_sdo_write(NULL, 0, 0, 0, NULL, 5));

    /* Additional coverage for lines 154, 162, 172, 388, 394, 446, 524, 570, 601, 611, 630, 638 */
    /* Line 154: frame_len < 2 + total_dg_len */
    syn_ecat_frame_begin(buf, sizeof(buf));
    buf[0] = 0x14;
    buf[1] = 0x00;
    offset = 2;
    TEST_ASSERT_FALSE(syn_ecat_frame_parse_next(buf, 10, &offset, &res));

    /* Line 162: *offset + 12 > 2 + total_dg_len */
    syn_ecat_frame_begin(buf, sizeof(buf));
    buf[0] = 0x05;
    buf[1] = 0x10;
    offset = 2;
    TEST_ASSERT_FALSE(syn_ecat_frame_parse_next(buf, 20, &offset, &res));

    /* Line 172: *offset + 12 + data_len > 2 + total_dg_len */
    syn_ecat_frame_begin(buf, sizeof(buf));
    buf[0] = 0x0F;
    buf[1] = 0x10;
    buf[2] = SYN_ECAT_CMD_NOP;
    buf[8] = 0x0A;
    buf[9] = 0x00;
    offset = 2;
    TEST_ASSERT_FALSE(syn_ecat_frame_parse_next(buf, 30, &offset, &res));

    /* Line 388: wkc > SYN_ECAT_MAX_SLAVES cap in scan_bus */
    syn_ecat_frame_begin(buf, sizeof(buf));
    syn_ecat_frame_add_datagram(buf, sizeof(buf), SYN_ECAT_CMD_BRD, 0x01, SYN_ESC_REG_TYPE, buf, 2,
                                false);
    buf[14] = 0xFF;
    buf[15] = 0x00;
    size_t scan_len = syn_ecat_frame_finalize(buf);
    syn_ecat_master_set_rx_frame(&m, buf, scan_len);
    TEST_ASSERT_EQUAL_UINT8(SYN_ECAT_MAX_SLAVES, syn_ecat_decode_scan_bus(&m, scan_len));

    /* Line 394: decode_scan_bus error return */
    TEST_ASSERT_EQUAL_UINT8(0, syn_ecat_decode_scan_bus(&m, 14));

    /* Line 446: decode_read_sii error return */
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_ecat_decode_read_sii(&m, 14, &val));

    /* Line 524: decode_read_al_status error return */
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_ecat_decode_read_al_status(&m, 14, &state, &status_code));

    /* Line 570: encode_coe_sdo_read null check */
    TEST_ASSERT_EQUAL_size_t(0, syn_ecat_encode_coe_sdo_read(NULL, 0, 0, 0));

    /* Line 601, 611, 630: decode_coe_sdo_response error branches */
    size_t out_len = 0;
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM,
                          syn_ecat_decode_coe_sdo_response(NULL, 0, NULL, 0, NULL));

    /* mbox_type != 3 (line 611) */
    uint8_t sdo_payload[16] = {0};
    sdo_payload[5] = 0x01;
    syn_ecat_frame_begin(buf, sizeof(buf));
    syn_ecat_frame_add_datagram(buf, sizeof(buf), SYN_ECAT_CMD_FPRD, 0x01, 0x10011080, sdo_payload,
                                16, false);
    size_t sdo_len = syn_ecat_frame_finalize(buf);
    syn_ecat_master_set_rx_frame(&m, buf, sdo_len);
    TEST_ASSERT_EQUAL_INT(SYN_ERROR,
                          syn_ecat_decode_coe_sdo_response(&m, sdo_len, &val, 4, &out_len));

    /* sdo_hdr != 0x40 (line 630) */
    sdo_payload[5] = 0x03;
    sdo_payload[8] = 0x80;
    syn_ecat_frame_begin(buf, sizeof(buf));
    syn_ecat_frame_add_datagram(buf, sizeof(buf), SYN_ECAT_CMD_FPRD, 0x01, 0x10011080, sdo_payload,
                                16, false);
    sdo_len = syn_ecat_frame_finalize(buf);
    syn_ecat_master_set_rx_frame(&m, buf, sdo_len);
    TEST_ASSERT_EQUAL_INT(SYN_ERROR,
                          syn_ecat_decode_coe_sdo_response(&m, sdo_len, &val, 4, &out_len));

    /* Line 638: encode_cyclic null check */
    TEST_ASSERT_EQUAL_size_t(0, syn_ecat_encode_cyclic(NULL));

    /* Lines 649, 663, 664, 668: decode_cyclic null & wkc error */
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_ecat_decode_cyclic(NULL, 64));
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_ecat_decode_cyclic(&m, 14));

    /* wkc < wkc_expected */
    m.wkc_expected = 1;
    syn_ecat_frame_begin(buf, sizeof(buf));
    syn_ecat_frame_add_datagram(buf, sizeof(buf), SYN_ECAT_CMD_LRW, 0x10, 0x10000, buf, 4, false);
    size_t cyc_len = syn_ecat_frame_finalize(buf);
    syn_ecat_master_set_rx_frame(&m, buf, cyc_len);
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_ecat_decode_cyclic(&m, cyc_len));

    /* Lines 676, 691, 704: AL control & DC null checks */
    TEST_ASSERT_EQUAL_size_t(0, syn_ecat_encode_set_al_control(NULL, 0, SYN_ECAT_STATE_INIT));
    TEST_ASSERT_EQUAL_size_t(0, syn_ecat_encode_dc_read_system_time(NULL));
    TEST_ASSERT_EQUAL_size_t(0, syn_ecat_encode_dc_configure(NULL, 0, 0, 0));

    /* Lines 722, 734: set_rx_frame and pop_tx_frame null checks */
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_ecat_master_set_rx_frame(NULL, NULL, 0));
    const uint8_t *dummy_ptr = NULL;
    size_t dummy_len = 0;
    TEST_ASSERT_FALSE(syn_ecat_master_pop_tx_frame(NULL, &dummy_ptr, &dummy_len));

    /* Line 777: get_slave_station_addr with slave_idx >= 32 */
    SYN_PT pt_trans;
    PT_INIT(&pt_trans);
    m.slave_count = 35;
    m.frame_rx_ready = true;
    while (syn_ecat_master_transition_task(&pt_trans, &m, SYN_ECAT_STATE_PREOP) == PT_WAITING) {
        m.frame_rx_ready = true;
    }
}

void run_ethercat_tests(void)
{
    RUN_TEST(test_ecat_init_and_esm);
    RUN_TEST(test_ecat_datagram_build_and_parse);
    RUN_TEST(test_ecat_coe_sdo_mailbox);
    RUN_TEST(test_ecat_edge_cases);
    RUN_TEST(test_ecat_multi_datagram);
    RUN_TEST(test_ecat_master_phase2);
    RUN_TEST(test_ecat_master_phases3_to_6);
    RUN_TEST(test_ecat_master_protothread_tasks);
    RUN_TEST(test_ecat_master_sii_and_sdo_tasks);
    RUN_TEST(test_ecat_reg_and_task_edge_cases);
    RUN_TEST(test_ecat_pdo_mapping_discovery_task);
    RUN_TEST(test_ecat_task_poll_timeouts);
    RUN_TEST(test_ecat_null_and_bounds_coverage);
}
