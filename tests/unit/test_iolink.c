/**
 * @file test_iolink.c
 * @brief Complete unit tests for IO-Link (IEC 61131-9) Master & Device Protocol Engine (100%
 * coverage).
 */

#include "unity/unity.h"

#include <string.h>
#include <syntropic/proto/syn_iolink.h>

/* ── Loopback Transport between Master and Device ────────────────────────── */

typedef struct {
    SYN_IOLink_Device *dev;
    uint8_t master_tx[128];
    size_t master_tx_len;
    uint8_t master_rx[128];
    size_t master_rx_len;
    bool fail_send;
    bool fail_recv;
    bool corrupt_rx;
    bool short_rx;
    uint32_t recv_count;
    bool fail_second_recv;
} IOLinkLoopbackTransport;

static bool loopback_send(const uint8_t *data, size_t len, void *user_ctx)
{
    IOLinkLoopbackTransport *lb = (IOLinkLoopbackTransport *)user_ctx;
    if (lb->fail_send) {
        return false;
    }
    if (len > sizeof(lb->master_tx)) {
        return false;
    }
    (void)memcpy(lb->master_tx, data, len);
    lb->master_tx_len = len;

    /* Process immediately on device */
    if (lb->dev != NULL) {
        lb->master_rx_len = 0U;
        (void)syn_iolink_device_process_frame(lb->dev, lb->master_tx, lb->master_tx_len,
                                              lb->master_rx, sizeof(lb->master_rx),
                                              &lb->master_rx_len);
        if (lb->corrupt_rx && lb->master_rx_len > 0U) {
            lb->master_rx[lb->master_rx_len - 1U] ^= 0xFFU; /* Corrupt checksum */
        }
    }
    return true;
}

static bool loopback_recv(uint8_t *buf, size_t max_len, size_t *out_len, void *user_ctx)
{
    IOLinkLoopbackTransport *lb = (IOLinkLoopbackTransport *)user_ctx;
    lb->recv_count++;
    if (lb->fail_recv || lb->master_rx_len == 0U ||
        (lb->fail_second_recv && lb->recv_count == 2U)) {
        return false;
    }
    size_t copy_len = (lb->master_rx_len < max_len) ? lb->master_rx_len : max_len;
    if (lb->short_rx && copy_len > 1U) {
        copy_len = 1U;
    }
    (void)memcpy(buf, lb->master_rx, copy_len);
    *out_len = copy_len;
    lb->master_rx_len = 0U;
    return true;
}

/* ── ISDU Callbacks ──────────────────────────────────────────────────────── */

static uint16_t g_device_serial = 12345U;

static SYN_Status on_dev_isdu_read(uint16_t index, uint8_t subindex, uint8_t *out_data,
                                   size_t max_len, size_t *out_len, void *user_data)
{
    (void)user_data;
    if (index == 0x0015U && subindex == 0U && max_len >= 2U) { /* Serial Number Index */
        out_data[0] = (uint8_t)((g_device_serial >> 8U) & 0xFFU);
        out_data[1] = (uint8_t)(g_device_serial & 0xFFU);
        *out_len = 2U;
        return SYN_OK;
    }
    return SYN_ERROR;
}

static SYN_Status on_dev_isdu_write(uint16_t index, uint8_t subindex, const uint8_t *data,
                                    size_t len, void *user_data)
{
    (void)user_data;
    if (index == 0x0015U && subindex == 0U && len >= 2U) {
        g_device_serial = (uint16_t)(((uint16_t)data[0] << 8U) | data[1]);
        return SYN_OK;
    }
    return SYN_ERROR;
}

/* ── Tests ───────────────────────────────────────────────────────────────── */

void test_iolink_checksum_calculation(void)
{
    uint8_t test_data[] = {0x80U, 0x01U, 0x11U};
    uint8_t cks = syn_iolink_calc_checksum(test_data, sizeof(test_data));
    TEST_ASSERT_NOT_EQUAL(0, cks);

    /* Null data handling */
    TEST_ASSERT_EQUAL_HEX8(0, syn_iolink_calc_checksum(NULL, 0));
    TEST_ASSERT_EQUAL_HEX8(0, syn_iolink_calc_checksum(test_data, 0));
}

void test_iolink_master_and_device_startup_handshake(void)
{
    SYN_IOLink_Device dev;
    SYN_IOLink_DeviceConfig dev_cfg = {
        .params =
            {
                .min_cycle_time = 0x18U, /* 2.4 ms */
                .revision_id = SYN_IOLINK_REV_1_1,
                .vendor_id = 0x0123U,
                .device_id = 0x00045678U,
                .pd_in_len = 2U,
                .pd_out_len = 2U,
            },
        .on_read = on_dev_isdu_read,
        .on_write = on_dev_isdu_write,
    };
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_iolink_device_init(&dev, &dev_cfg));

    /* Null validation */
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_iolink_device_init(NULL, &dev_cfg));
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_iolink_device_init(&dev, NULL));
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_iolink_master_init(NULL, NULL));

    IOLinkLoopbackTransport lb;
    (void)memset(&lb, 0, sizeof(lb));
    lb.dev = &dev;

    SYN_Transport transport = {.send = loopback_send, .recv = loopback_recv, .ctx = &lb};

    uint8_t rx[128], tx[128];
    SYN_IOLink_MasterConfig master_cfg = {
        .target_baud = SYN_IOLINK_BAUD_COM3,
        .transport = &transport,
        .cycle_time_ms = 5U,
        .rx_buf = rx,
        .rx_buf_size = sizeof(rx),
        .tx_buf = tx,
        .tx_buf_size = sizeof(tx),
    };

    SYN_IOLink_Master master;
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_iolink_master_init(&master, &master_cfg));
    TEST_ASSERT_EQUAL_INT(SYN_IOLINK_PORT_INACTIVE, master.state);

    /* Buffer boundary check */
    master_cfg.rx_buf_size = 16;
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_iolink_master_init(&master, &master_cfg));
    master_cfg.rx_buf_size = sizeof(rx);

    /* Start handshake */
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_iolink_master_start(&master));
    TEST_ASSERT_EQUAL_INT(SYN_IOLINK_PORT_OPERATE, master.state);
    TEST_ASSERT_EQUAL_HEX16(0x0123, master.dev_params.vendor_id);
    TEST_ASSERT_EQUAL_HEX32(0x00045678, master.dev_params.device_id);
    TEST_ASSERT_EQUAL_HEX8(SYN_IOLINK_REV_1_1, master.dev_params.revision_id);
}

void test_iolink_cyclic_process_data_exchange(void)
{
    SYN_IOLink_Device dev;
    SYN_IOLink_DeviceConfig dev_cfg = {
        .params =
            {
                .min_cycle_time = 0x18U,
                .revision_id = SYN_IOLINK_REV_1_1,
                .vendor_id = 0x0567U,
                .device_id = 0x00112233U,
                .pd_in_len = 2U,
                .pd_out_len = 2U,
            },
    };
    syn_iolink_device_init(&dev, &dev_cfg);

    /* Device getters / setters null checks */
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_iolink_device_set_pd_in(NULL, NULL, 0));
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_iolink_device_get_pd_out(NULL, NULL, 0, NULL));

    IOLinkLoopbackTransport lb;
    (void)memset(&lb, 0, sizeof(lb));
    lb.dev = &dev;

    SYN_Transport transport = {.send = loopback_send, .recv = loopback_recv, .ctx = &lb};
    uint8_t rx[128], tx[128];
    SYN_IOLink_MasterConfig master_cfg = {
        .transport = &transport,
        .rx_buf = rx,
        .rx_buf_size = sizeof(rx),
        .tx_buf = tx,
        .tx_buf_size = sizeof(tx),
    };

    SYN_IOLink_Master master;
    syn_iolink_master_init(&master, &master_cfg);
    syn_iolink_master_start(&master);

    /* Set device sensor reading (PD_IN) */
    uint8_t sensor_reading[] = {0x04U, 0xD2U}; /* 1234 */
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_iolink_device_set_pd_in(&dev, sensor_reading, 2U));

    /* Master exchanges PD: sends actuator command 0x55, 0xAA */
    uint8_t actuator_cmd[] = {0x55U, 0xAAU};
    uint8_t in_buf[16];
    size_t in_len = 0U;

    TEST_ASSERT_EQUAL_INT(
        SYN_OK, syn_iolink_master_exchange_pd(&master, actuator_cmd, 2U, in_buf, &in_len));
    TEST_ASSERT_EQUAL_UINT(2, in_len);
    TEST_ASSERT_EQUAL_HEX8(0x04, in_buf[0]);
    TEST_ASSERT_EQUAL_HEX8(0xD2, in_buf[1]);

    /* Verify device received actuator output */
    uint8_t dev_pd_out[16];
    size_t dev_pd_out_len = 0U;
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_iolink_device_get_pd_out(&dev, dev_pd_out, sizeof(dev_pd_out),
                                                               &dev_pd_out_len));
    TEST_ASSERT_EQUAL_HEX8(0x55, dev_pd_out[0]);
    TEST_ASSERT_EQUAL_HEX8(0xAA, dev_pd_out[1]);
}

void test_iolink_acyclic_isdu_read_write(void)
{
    SYN_IOLink_Device dev;
    SYN_IOLink_DeviceConfig dev_cfg = {
        .params = {.revision_id = SYN_IOLINK_REV_1_1, .pd_in_len = 2, .pd_out_len = 2},
        .on_read = on_dev_isdu_read,
        .on_write = on_dev_isdu_write,
    };
    syn_iolink_device_init(&dev, &dev_cfg);

    IOLinkLoopbackTransport lb;
    (void)memset(&lb, 0, sizeof(lb));
    lb.dev = &dev;

    SYN_Transport transport = {.send = loopback_send, .recv = loopback_recv, .ctx = &lb};
    uint8_t rx[128], tx[128];
    SYN_IOLink_MasterConfig master_cfg = {
        .transport = &transport,
        .rx_buf = rx,
        .rx_buf_size = sizeof(rx),
        .tx_buf = tx,
        .tx_buf_size = sizeof(tx),
    };

    SYN_IOLink_Master master;
    syn_iolink_master_init(&master, &master_cfg);
    syn_iolink_master_start(&master);

    /* Read ISDU */
    uint8_t isdu_data[16];
    size_t isdu_len = 0U;
    g_device_serial = 12345U;
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_iolink_master_read_isdu(&master, 0x0015U, 0U, isdu_data,
                                                              sizeof(isdu_data), &isdu_len));
    TEST_ASSERT_EQUAL_UINT(2, isdu_len);
    uint16_t read_serial = (uint16_t)(((uint16_t)isdu_data[0] << 8U) | isdu_data[1]);
    TEST_ASSERT_EQUAL_UINT16(12345, read_serial);

    /* Write ISDU */
    uint8_t new_serial_bytes[] = {0x27, 0x10}; /* 10000 */
    TEST_ASSERT_EQUAL_INT(SYN_OK,
                          syn_iolink_master_write_isdu(&master, 0x0015U, 0U, new_serial_bytes, 2U));
    TEST_ASSERT_EQUAL_UINT16(10000, g_device_serial);
}

void test_iolink_master_errors_and_edge_cases(void)
{
    SYN_IOLink_Device dev;
    SYN_IOLink_DeviceConfig dev_cfg = {
        .params = {.revision_id = SYN_IOLINK_REV_1_1, .pd_in_len = 2, .pd_out_len = 2},
        .on_read = on_dev_isdu_read,
        .on_write = on_dev_isdu_write,
    };
    syn_iolink_device_init(&dev, &dev_cfg);

    IOLinkLoopbackTransport lb;
    (void)memset(&lb, 0, sizeof(lb));
    lb.dev = &dev;

    SYN_Transport transport = {.send = loopback_send, .recv = loopback_recv, .ctx = &lb};
    uint8_t rx[128], tx[128];
    SYN_IOLink_MasterConfig master_cfg = {
        .transport = &transport,
        .rx_buf = rx,
        .rx_buf_size = sizeof(rx),
        .tx_buf = tx,
        .tx_buf_size = sizeof(tx),
    };

    SYN_IOLink_Master master;
    syn_iolink_master_init(&master, &master_cfg);

    /* Null validation */
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_iolink_master_start(NULL));
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM,
                          syn_iolink_master_exchange_pd(NULL, NULL, 0, NULL, NULL));
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM,
                          syn_iolink_master_read_isdu(NULL, 0, 0, NULL, 0, NULL));
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_iolink_master_write_isdu(NULL, 0, 0, NULL, 0));
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_iolink_master_step(NULL, 0));

    /* Transport failure during start */
    lb.fail_send = true;
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_iolink_master_start(&master));
    TEST_ASSERT_EQUAL_INT(SYN_IOLINK_PORT_FAULT, master.state);
    lb.fail_send = false;

    /* Checksum corruption during start */
    lb.corrupt_rx = true;
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_iolink_master_start(&master));
    lb.corrupt_rx = false;

    /* Checksum corruption and send failure during PD exchange */
    syn_iolink_master_start(&master);
    lb.corrupt_rx = true;
    uint8_t dummy_in[8];
    size_t in_len = 0;
    TEST_ASSERT_EQUAL_INT(SYN_ERROR,
                          syn_iolink_master_exchange_pd(&master, rx, 1, dummy_in, &in_len));
    lb.corrupt_rx = false;

    lb.fail_send = true;
    TEST_ASSERT_EQUAL_INT(SYN_ERROR,
                          syn_iolink_master_exchange_pd(&master, rx, 1, dummy_in, &in_len));
    lb.fail_send = false;

    /* ISDU Read/Write errors */
    lb.fail_send = true;
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_iolink_master_read_isdu(&master, 0x0015U, 0U, dummy_in,
                                                                 sizeof(dummy_in), &in_len));
    TEST_ASSERT_EQUAL_INT(SYN_ERROR,
                          syn_iolink_master_write_isdu(&master, 0x0015U, 0U, dummy_in, 1U));
    lb.fail_send = false;

    lb.fail_recv = true;
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_iolink_master_read_isdu(&master, 0x0015U, 0U, dummy_in,
                                                                 sizeof(dummy_in), &in_len));
    TEST_ASSERT_EQUAL_INT(SYN_ERROR,
                          syn_iolink_master_write_isdu(&master, 0x0015U, 0U, dummy_in, 1U));
    lb.fail_recv = false;

    master.state = SYN_IOLINK_PORT_OPERATE;
    lb.corrupt_rx = true;
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_iolink_master_read_isdu(&master, 0x0015U, 0U, dummy_in,
                                                                 sizeof(dummy_in), &in_len));
    master.state = SYN_IOLINK_PORT_OPERATE;
    uint8_t write_ser[2] = {0x11, 0x22};
    TEST_ASSERT_EQUAL_INT(SYN_ERROR,
                          syn_iolink_master_write_isdu(&master, 0x0015U, 0U, write_ser, 2U));
    lb.corrupt_rx = false;

    /* Short rx (rx_len < 2) tests */
    lb.short_rx = true;
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_iolink_master_start(&master));
    master.state = SYN_IOLINK_PORT_OPERATE;
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_iolink_master_read_isdu(&master, 0x0015U, 0U, dummy_in,
                                                                 sizeof(dummy_in), &in_len));
    master.state = SYN_IOLINK_PORT_OPERATE;
    TEST_ASSERT_EQUAL_INT(SYN_ERROR,
                          syn_iolink_master_write_isdu(&master, 0x0015U, 0U, dummy_in, 1U));
    lb.short_rx = false;

    /* Failure on Revision ID during startup */
    lb.fail_second_recv = true;
    lb.recv_count = 0;
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_iolink_master_start(&master));
    lb.fail_second_recv = false;

    /* Device frame error validations */
    uint8_t dev_tx[64];
    size_t dev_tx_len = 0;
    TEST_ASSERT_EQUAL_INT(
        SYN_INVALID_PARAM,
        syn_iolink_device_process_frame(NULL, rx, 10, dev_tx, sizeof(dev_tx), &dev_tx_len));
    TEST_ASSERT_EQUAL_INT(
        SYN_INVALID_PARAM,
        syn_iolink_device_process_frame(&dev, NULL, 10, dev_tx, sizeof(dev_tx), &dev_tx_len));
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_iolink_device_process_frame(
                                                 &dev, rx, 10, NULL, sizeof(dev_tx), &dev_tx_len));
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_iolink_device_process_frame(&dev, rx, 10, dev_tx,
                                                                             sizeof(dev_tx), NULL));
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_iolink_device_process_frame(
                                                 &dev, rx, 1, dev_tx, sizeof(dev_tx), &dev_tx_len));
    uint8_t bad_frame[4] = {0x80, 0x01, 0x00, 0x00}; /* Corrupt CRC */
    TEST_ASSERT_EQUAL_INT(SYN_ERROR, syn_iolink_device_process_frame(&dev, bad_frame, 4, dev_tx,
                                                                     sizeof(dev_tx), &dev_tx_len));

    /* Direct Parameter Page default address (0x3F) */
    uint8_t unk_param_frame[2] = {0xBF, 0x00}; /* Read address 0x3F */
    unk_param_frame[1] = syn_iolink_calc_checksum(unk_param_frame, 1);
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_iolink_device_process_frame(&dev, unk_param_frame, 2, dev_tx,
                                                                  sizeof(dev_tx), &dev_tx_len));

    /* PD exchange with max_tx too small */
    uint8_t pd_frame[3] = {0x00, 0x11, 0x00};
    pd_frame[2] = syn_iolink_calc_checksum(pd_frame, 2);
    TEST_ASSERT_EQUAL_INT(
        SYN_ERROR, syn_iolink_device_process_frame(&dev, pd_frame, 3, dev_tx, 1, &dev_tx_len));
}

void test_iolink_master_step_and_protothread(void)
{
    IOLinkLoopbackTransport lb;
    (void)memset(&lb, 0, sizeof(lb));
    SYN_Transport transport = {.send = loopback_send, .recv = loopback_recv, .ctx = &lb};

    uint8_t rx[64], tx[64];
    SYN_IOLink_MasterConfig master_cfg = {
        .transport = &transport,
        .cycle_time_ms = 10U,
        .rx_buf = rx,
        .rx_buf_size = sizeof(rx),
        .tx_buf = tx,
        .tx_buf_size = sizeof(tx),
    };

    SYN_IOLink_Master master;
    syn_iolink_master_init(&master, &master_cfg);
    master.state = SYN_IOLINK_PORT_OPERATE;

    /* Step initial (hits last_cycle_ms == 0 with small now_ms) and non-periodic */
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_iolink_master_step(&master, 5U));
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_iolink_master_step(&master, 6U));
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_iolink_master_step(&master, 120U));

    SYN_PT pt;
    PT_INIT(&pt);
    SYN_Task task = {.user_data = &master};

    SYN_PT_Status st = syn_iolink_master_pt(&pt, &task);
    TEST_ASSERT_EQUAL_INT(PT_YIELDED, st);

    /* Null checks */
    TEST_ASSERT_EQUAL_INT(PT_ENDED, syn_iolink_master_pt(NULL, &task));
    TEST_ASSERT_EQUAL_INT(PT_ENDED, syn_iolink_master_pt(&pt, NULL));
    SYN_Task null_task = {.user_data = NULL};
    TEST_ASSERT_EQUAL_INT(PT_ENDED, syn_iolink_master_pt(&pt, &null_task));

    /* Step error termination */
    master.cfg.transport = NULL;
    st = syn_iolink_master_pt(&pt, &task);
    TEST_ASSERT_EQUAL_INT(PT_EXITED, st);
}

void run_iolink_tests(void)
{
    RUN_TEST(test_iolink_checksum_calculation);
    RUN_TEST(test_iolink_master_and_device_startup_handshake);
    RUN_TEST(test_iolink_cyclic_process_data_exchange);
    RUN_TEST(test_iolink_acyclic_isdu_read_write);
    RUN_TEST(test_iolink_master_errors_and_edge_cases);
    RUN_TEST(test_iolink_master_step_and_protothread);
}
