/**
 * @file test_dipswitch.c
 * @brief Unity tests for syn_dipswitch module.
 */

#include "mocks/mock_port.h"
#include "syntropic/input/syn_dipswitch.h"
#include "unity/unity.h"

static void test_dipswitch_reading(void)
{
    mock_port_reset();
    SYN_DipSwitch ds;

    SYN_GPIO_Pin pins[] = {0, 1, 2, 3};

    /* Active HIGH dip switches */
    SYN_Status st = syn_dipswitch_init(&ds, pins, 4, false);
    TEST_ASSERT_EQUAL(SYN_OK, st);
    TEST_ASSERT_EQUAL_UINT32(0, syn_dipswitch_get_value(&ds));

    /* Turn ON switch 0 and 2 (bits 0 and 2 -> 0x05) */
    mock_gpio_states[0] = 1;
    mock_gpio_states[2] = 1;

    syn_dipswitch_read(&ds);
    TEST_ASSERT_TRUE(syn_dipswitch_has_changed(&ds));
    TEST_ASSERT_EQUAL_UINT32(0x05, syn_dipswitch_get_value(&ds));

    /* Read again without changes */
    syn_dipswitch_read(&ds);
    TEST_ASSERT_FALSE(syn_dipswitch_has_changed(&ds));
}

static void test_dipswitch_active_low_and_null_checks(void)
{
    mock_port_reset();
    SYN_DipSwitch ds;
    SYN_GPIO_Pin pins[] = {4, 5};

    SYN_Status st = syn_dipswitch_init(&ds, pins, 2, true);
    TEST_ASSERT_EQUAL(SYN_OK, st);
    /* By default GPIO states are 0; active_low means 0 is ON (both ON -> 0x03) */
    TEST_ASSERT_EQUAL_UINT32(0x03, syn_dipswitch_get_value(&ds));

    /* Set switch 1 HIGH (OFF), switch 0 stays LOW (ON) -> value 0x01 */
    mock_gpio_states[5] = 1;
    syn_dipswitch_read(&ds);
    TEST_ASSERT_TRUE(syn_dipswitch_has_changed(&ds));
    TEST_ASSERT_EQUAL_UINT32(0x01, syn_dipswitch_get_value(&ds));
    TEST_ASSERT_EQUAL_UINT32(0x03, ds.previous_value);

    /* Set switch 1 LOW (ON) -> value 0x03 */
    mock_gpio_states[5] = 0;
    syn_dipswitch_read(&ds);
    TEST_ASSERT_TRUE(syn_dipswitch_has_changed(&ds));
    TEST_ASSERT_EQUAL_UINT32(0x03, syn_dipswitch_get_value(&ds));
    TEST_ASSERT_EQUAL_UINT32(0x01, ds.previous_value);

    /* NULL checks */
    TEST_ASSERT_EQUAL_UINT32(0, syn_dipswitch_get_value(NULL));
    TEST_ASSERT_FALSE(syn_dipswitch_has_changed(NULL));
}

void run_dipswitch_tests(void)
{
    RUN_TEST(test_dipswitch_reading);
    RUN_TEST(test_dipswitch_active_low_and_null_checks);
}
