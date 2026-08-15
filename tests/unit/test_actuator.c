/**
 * @file test_actuator.c
 * @brief Unity tests for syn_actuator.
 */

#include "mocks/mock_port.h"
#include "syntropic/motor/syn_actuator.h"
#include "syntropic/syntropic.h"
#include "unity/unity.h"

static int32_t act_mock_pos = 500;
static int32_t act_read(void *c)
{
    (void)c;
    return act_mock_pos;
}

static void test_actuator(void)
{
    static SYN_DCMotor act_motor;
    syn_dc_motor_init(&act_motor, 10, 11, SYN_DC_MODE_PWM_DIR);
    act_mock_pos = 500;
    SYN_Actuator act;
    SYN_Actuator_Config acfg = {
        .dc_motor = &act_motor,
        .read_pos = act_read,
        .read_ctx = NULL,
        .stroke_min = 100,
        .stroke_max = 900,
        .update_hz = 50,
        .pid_kp = 100,
        .pid_scale = 4,
    };
    syn_actuator_init(&act, &acfg);
    TEST_ASSERT_EQUAL_INT32(500, syn_actuator_position(&act));
    syn_actuator_set_position(&act, 750);
    act_mock_pos = 700;
    syn_actuator_update(&act);
    TEST_ASSERT_EQUAL_INT32(750, syn_actuator_position(&act));
    syn_actuator_stop(&act);
    TEST_ASSERT_EQUAL_INT(SYN_MCTRL_STOPPED, act.ctrl.state);
    syn_actuator_set_position(&act, 0);
    act_mock_pos = 100;
    syn_actuator_update(&act);
    TEST_ASSERT_EQUAL_INT32(0, syn_actuator_position(&act));
    syn_actuator_set_position(&act, 1000);
    act_mock_pos = 900;
    syn_actuator_update(&act);
    TEST_ASSERT_EQUAL_INT32(1000, syn_actuator_position(&act));
    syn_actuator_set_position(&act, 2000);
    TEST_ASSERT_EQUAL_INT32(1000, act.target_pct);
    syn_actuator_set_position(&act, -500);
    TEST_ASSERT_EQUAL_INT32(0, act.target_pct);
}

/** syn_actuator_clear_stall — exercises lines 120-124 */
static void test_actuator_clear_stall(void)
{
    static SYN_DCMotor motor;
    syn_dc_motor_init(&motor, 12, 13, SYN_DC_MODE_PWM_DIR);
    SYN_Actuator act;
    SYN_Actuator_Config cfg = {
        .dc_motor = &motor,
        .read_pos = act_read,
        .read_ctx = NULL,
        .stroke_min = 0,
        .stroke_max = 1000,
        .update_hz = 50,
        .pid_kp = 100,
        .pid_scale = 4,
    };
    act_mock_pos = 500;
    syn_actuator_init(&act, &cfg);

    /* Manually mark it as stalled and verify clear works */
    act.ctrl.stall_active = true;
    syn_actuator_clear_stall(&act);
    TEST_ASSERT_FALSE(syn_actuator_is_stalled(&act));

    /* Test default pid_kp fallback branch (pid_kp <= 0) */
    cfg.pid_kp = 0;
    syn_actuator_init(&act, &cfg);
    TEST_ASSERT_EQUAL_INT32(100, act.ctrl.pid.cfg.kp);
}

static void test_actuator_at_target_tolerance(void)
{
    SYN_Actuator act;
    memset(&act, 0, sizeof(act));

    /* Exactly equal */
    act.target_pct = 500;
    act.current_pct = 500;
    TEST_ASSERT_TRUE(syn_actuator_at_target(&act));

    /* Positive diff within tolerance (target 505, current 500 -> diff 5) */
    act.target_pct = 505;
    TEST_ASSERT_TRUE(syn_actuator_at_target(&act));

    /* Positive diff outside tolerance (target 506, current 500 -> diff 6) */
    act.target_pct = 506;
    TEST_ASSERT_FALSE(syn_actuator_at_target(&act));

    /* Negative diff within tolerance (target 495, current 500 -> diff -5) */
    act.target_pct = 495;
    TEST_ASSERT_TRUE(syn_actuator_at_target(&act));

    /* Negative diff outside tolerance (target 494, current 500 -> diff -6) */
    act.target_pct = 494;
    TEST_ASSERT_FALSE(syn_actuator_at_target(&act));
}

void run_actuator_tests(void)
{
    RUN_TEST(test_actuator);
    RUN_TEST(test_actuator_clear_stall);
    RUN_TEST(test_actuator_at_target_tolerance);
}
