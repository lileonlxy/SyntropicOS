/**
 * @file test_usb_msc.c
 * @brief Unity tests for Zero-Heap USB MSC Class Device Driver.
 */

#include "mocks/mock_port.h"
#include "syntropic/drivers/syn_usb_msc.h"
#include "syntropic/syntropic.h"
#include "unity/unity.h"

static SYN_USB_MSC g_msc;
static SYN_USB_Device g_usb_dev;

static SYN_Status mock_read_blocks(uint32_t lba, uint8_t *buf, uint16_t count)
{
    (void)lba;
    (void)buf;
    (void)count;
    return SYN_OK;
}

static SYN_Status mock_write_blocks(uint32_t lba, const uint8_t *buf, uint16_t count)
{
    (void)lba;
    (void)buf;
    (void)count;
    return SYN_OK;
}

void test_usb_msc_init_and_null_checks(void)
{
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_usb_msc_init(NULL));
    TEST_ASSERT_EQUAL(SYN_OK, syn_usb_msc_init(&g_msc));
    TEST_ASSERT_EQUAL(0x82U, g_msc.ep_in);
    TEST_ASSERT_EQUAL(0x02U, g_msc.ep_out);

    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_usb_msc_register(NULL, &g_msc));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_usb_msc_register(&g_usb_dev, NULL));

    static const uint8_t dev_desc[18] = {18,   1,    0x00, 0x02, 0, 0, 0, 64, 0xFE,
                                         0xCA, 0x01, 0x00, 0,    1, 1, 2, 0,  1};
    syn_usb_init(&g_usb_dev, dev_desc);
    TEST_ASSERT_EQUAL(SYN_OK, syn_usb_msc_register(&g_usb_dev, &g_msc));

    /* Test setup callback */
    SYN_USB_ClassDriver *drv = &g_usb_dev.classes[0];
    SYN_USB_SetupPacket pkt = {.bRequest = 0xFEU};
    uint8_t resp[16] = {0};
    uint16_t rlen = 0;
    TEST_ASSERT_EQUAL(SYN_OK, drv->setup(&g_msc, &pkt, resp, &rlen));
    TEST_ASSERT_EQUAL(1, rlen);

    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, drv->setup(NULL, &pkt, resp, &rlen));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, drv->setup(&g_msc, NULL, resp, &rlen));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, drv->setup(&g_msc, &pkt, NULL, &rlen));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, drv->setup(&g_msc, &pkt, resp, NULL));

    pkt.bRequest = 0x00U;
    TEST_ASSERT_EQUAL(SYN_OK, drv->setup(&g_msc, &pkt, resp, &rlen));

    TEST_ASSERT_EQUAL(SYN_OK, drv->configured(&g_msc, 1));

    uint8_t cbw_dummy[31] = {'U', 'S', 'B', 'C'};
    drv->data_out(NULL, 0x02, cbw_dummy, 31);
    drv->data_out(&g_msc, 0x02, NULL, 31);
    drv->data_out(&g_msc, 0x01, cbw_dummy, 31);
    drv->data_out(&g_msc, 0x02, cbw_dummy, 10);
    drv->data_out(&g_msc, 0x02, cbw_dummy, 31);

    drv->data_in(NULL, 0x82);
    drv->data_in(&g_msc, 0x81);
    g_msc.tx_len = 5;
    drv->data_in(&g_msc, 0x82);
    TEST_ASSERT_EQUAL(0, g_msc.tx_len);

    SYN_USB_MSC_Media media = {
        .block_count = 2048,
        .block_size = 512,
        .read_blocks = mock_read_blocks,
        .write_blocks = mock_write_blocks,
    };
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_usb_msc_set_media(NULL, &media));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_usb_msc_set_media(&g_msc, NULL));
    TEST_ASSERT_EQUAL(SYN_OK, syn_usb_msc_set_media(&g_msc, &media));
}

void test_usb_msc_process_cbw_scsi_commands(void)
{
    syn_usb_msc_init(&g_msc);

    /* Construct valid CBW packet: Signature "USBC" = 0x43425355 */
    uint8_t cbw[31] = {0};
    cbw[0] = 'U';
    cbw[1] = 'S';
    cbw[2] = 'B';
    cbw[3] = 'C';
    cbw[4] = 0x01;  /* Tag = 1 */
    cbw[14] = 0x06; /* SCSI opcode length */
    cbw[15] = SYN_SCSI_TEST_UNIT_READY;

    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_usb_msc_process_cbw(NULL, cbw));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_usb_msc_process_cbw(&g_msc, NULL));

    TEST_ASSERT_EQUAL(SYN_OK, syn_usb_msc_process_cbw(&g_msc, cbw));
    TEST_ASSERT_EQUAL(0x53425355U, g_msc.csw.dCSWSignature); /* "USBS" */
    TEST_ASSERT_EQUAL(1U, g_msc.csw.dCSWTag);
    TEST_ASSERT_EQUAL(0U, g_msc.csw.bCSWStatus); /* Success */

    /* Test invalid CBW signature */
    cbw[0] = 'X';
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_usb_msc_process_cbw(&g_msc, cbw));

    /* Test unsupported opcode */
    cbw[0] = 'U';
    cbw[15] = 0xFF;
    TEST_ASSERT_EQUAL(SYN_OK, syn_usb_msc_process_cbw(&g_msc, cbw));
    TEST_ASSERT_EQUAL(1U, g_msc.csw.bCSWStatus); /* Failed status */
}

void run_usb_msc_tests(void)
{
    RUN_TEST(test_usb_msc_init_and_null_checks);
    RUN_TEST(test_usb_msc_process_cbw_scsi_commands);
}
