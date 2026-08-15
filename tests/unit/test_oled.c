/**
 * @file test_oled.c
 * @brief Unity tests for syn_oled module.
 */

#include "mocks/mock_port.h"
#include "syntropic/display/syn_oled.h"
#include "unity/unity.h"

static void test_oled_controllers(void)
{
    mock_port_reset();
    SYN_OLED oled;

    /* SSD1306 Initialization */
    SYN_Status st = syn_oled_init(&oled, 0, 1, 0x3C, 128, 64, SYN_OLED_SSD1306);
    TEST_ASSERT_EQUAL(SYN_OK, st);

    uint8_t buf[128 * 64 / 8];
    SYN_Canvas c;
    syn_canvas_init(&c, buf, 128, 64, 1, syn_oled_canvas_flush_cb, &oled);

    syn_canvas_fill(&c, 1);
    syn_oled_flush_canvas(&oled, &c);

    /* Test control functions */
    syn_oled_set_contrast(&oled, 200);
    TEST_ASSERT_EQUAL_UINT8(200, oled.contrast);

    syn_oled_set_invert(&oled, true);
    TEST_ASSERT_TRUE(oled.inverted);

    syn_oled_set_display_on(&oled, false);
    TEST_ASSERT_FALSE(oled.display_on);

    /* SH1106 Initialization (with 2-pixel column shift) */
    st = syn_oled_init(&oled, 0, 1, 0x3C, 128, 64, SYN_OLED_SH1106);
    TEST_ASSERT_EQUAL(SYN_OK, st);
    TEST_ASSERT_EQUAL_UINT8(2, oled.col_offset);
    syn_oled_flush_canvas(&oled, &c);

    /* SSD1309 Initialization */
    st = syn_oled_init(&oled, 0, 1, 0x3C, 128, 64, SYN_OLED_SSD1309);
    TEST_ASSERT_EQUAL(SYN_OK, st);
    /* SSD1306 128x32 Initialization (non-64 height COM pin config branch) */
    st = syn_oled_init(&oled, 0, 1, 0x3C, 128, 32, SYN_OLED_SSD1306);
    TEST_ASSERT_EQUAL(SYN_OK, st);
    TEST_ASSERT_EQUAL_UINT16(32, oled.height);

    /* NULL safety guards */
    syn_oled_set_contrast(NULL, 0);
    syn_oled_set_invert(NULL, false);
    syn_oled_set_display_on(NULL, false);
    syn_oled_flush_canvas(NULL, NULL);
    syn_oled_canvas_flush_cb(NULL, 0, NULL);
}

void run_oled_tests(void)
{
    RUN_TEST(test_oled_controllers);
}
