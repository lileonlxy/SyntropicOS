/**
 * @file test_timer_wheel.c
 * @brief Unit test suite for Hashed Timing Wheel Scheduler (syn_timer_wheel).
 */

#include "syntropic/sched/syn_timer_wheel.h"
#include "unity/unity.h"

#include <string.h>

static int s_timer1_fired = 0;
static int s_timer2_fired = 0;

static void timer1_cb(void *arg)
{
    (void)arg;
    s_timer1_fired++;
}

static void timer2_cb(void *arg)
{
    (void)arg;
    s_timer2_fired++;
}

void test_timer_wheel_add_step_cancel(void)
{
    SYN_TimerWheel wheel;
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_timer_wheel_init(&wheel));

    SYN_TimerWheelNode n1, n2;
    memset(&n1, 0, sizeof(n1));
    memset(&n2, 0, sizeof(n2));
    s_timer1_fired = 0;
    s_timer2_fired = 0;

    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_timer_wheel_add(&wheel, &n1, 3, timer1_cb, NULL));
    TEST_ASSERT_EQUAL_INT(
        SYN_OK, syn_timer_wheel_add(&wheel, &n2, 70, timer2_cb, NULL)); /* Multi-rotation timer */

    /* Step 1 & 2: no timers fire */
    TEST_ASSERT_EQUAL_UINT32(0, syn_timer_wheel_step(&wheel));
    TEST_ASSERT_EQUAL_UINT32(0, syn_timer_wheel_step(&wheel));
    TEST_ASSERT_EQUAL_INT(0, s_timer1_fired);

    /* Step 3: timer 1 fires */
    TEST_ASSERT_EQUAL_UINT32(1, syn_timer_wheel_step(&wheel));
    TEST_ASSERT_EQUAL_INT(1, s_timer1_fired);

    /* Cancel timer 2 */
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_timer_wheel_cancel(&wheel, &n2));

    /* Advance wheel by 70 ticks */
    for (int i = 0; i < 70; i++) {
        syn_timer_wheel_step(&wheel);
    }
    TEST_ASSERT_EQUAL_INT(0, s_timer2_fired); /* Canceled, should not fire */

    /* Test collision: two timers in the same bucket */
    SYN_TimerWheelNode t_a, t_b, t_c;
    memset(&t_a, 0, sizeof(t_a));
    memset(&t_b, 0, sizeof(t_b));
    memset(&t_c, 0, sizeof(t_c));

    syn_timer_wheel_add(&wheel, &t_a, 5, timer1_cb, NULL);
    syn_timer_wheel_add(&wheel, &t_b, 5, timer2_cb, NULL);
    syn_timer_wheel_add(&wheel, &t_c, 5, timer1_cb, NULL);

    /* Re-add active timer t_b (triggers cancel before re-add) */
    syn_timer_wheel_add(&wheel, &t_b, 5, timer2_cb, NULL);

    /* Cancel middle element t_b */
    syn_timer_wheel_cancel(&wheel, &t_b);

    /* Step 5 ticks to fire t_a and t_c */
    for (int i = 0; i < 4; i++)
        syn_timer_wheel_step(&wheel);
    TEST_ASSERT_EQUAL_UINT32(2, syn_timer_wheel_step(&wheel));

    /* Null & param checks */
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_timer_wheel_init(NULL));
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_timer_wheel_add(NULL, &n1, 5, timer1_cb, NULL));
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_timer_wheel_add(&wheel, &n1, 0, timer1_cb, NULL));
    TEST_ASSERT_EQUAL_INT(SYN_INVALID_PARAM, syn_timer_wheel_cancel(NULL, &n1));
    TEST_ASSERT_EQUAL_UINT32(0, syn_timer_wheel_step(NULL));

    /* Multi-rotation timer rotation_count decrement branch (line 90) */
    SYN_TimerWheelNode t_rot, t_rot2;
    memset(&t_rot, 0, sizeof(t_rot));
    memset(&t_rot2, 0, sizeof(t_rot2));
    syn_timer_wheel_add(&wheel, &t_rot2, 5, timer2_cb,
                        NULL); /* Added first -> ends up behind head */
    syn_timer_wheel_add(&wheel, &t_rot, SYN_TIMER_WHEEL_BUCKETS + 5, timer1_cb,
                        NULL); /* Prepended as head */
    for (int i = 0; i < 4; i++)
        syn_timer_wheel_step(&wheel);
    TEST_ASSERT_EQUAL_UINT32(1, syn_timer_wheel_step(&wheel)); /* t_rot2 fires, line 94 executed */
    TEST_ASSERT_TRUE(t_rot.active);                            /* Still active after 1st rotation */
    syn_timer_wheel_cancel(&wheel, &t_rot);
}

void test_timer_wheel_exact_bucket_multiple(void)
{
    SYN_TimerWheel wheel;
    TEST_ASSERT_EQUAL_INT(SYN_OK, syn_timer_wheel_init(&wheel));

    SYN_TimerWheelNode n_64, n_128;
    memset(&n_64, 0, sizeof(n_64));
    memset(&n_128, 0, sizeof(n_128));
    s_timer1_fired = 0;
    s_timer2_fired = 0;

    /* Add timer 1 with delay = SYN_TIMER_WHEEL_BUCKETS (64 ticks) */
    TEST_ASSERT_EQUAL_INT(
        SYN_OK, syn_timer_wheel_add(&wheel, &n_64, SYN_TIMER_WHEEL_BUCKETS, timer1_cb, NULL));

    /* Add timer 2 with delay = 2 * SYN_TIMER_WHEEL_BUCKETS (128 ticks) */
    TEST_ASSERT_EQUAL_INT(
        SYN_OK, syn_timer_wheel_add(&wheel, &n_128, 2 * SYN_TIMER_WHEEL_BUCKETS, timer2_cb, NULL));

    /* Step ticks 1..63: neither should fire */
    for (uint32_t i = 1; i < SYN_TIMER_WHEEL_BUCKETS; i++) {
        TEST_ASSERT_EQUAL_UINT32(0, syn_timer_wheel_step(&wheel));
        TEST_ASSERT_EQUAL_INT(0, s_timer1_fired);
        TEST_ASSERT_EQUAL_INT(0, s_timer2_fired);
    }

    /* Step tick 64: timer 1 MUST fire! */
    TEST_ASSERT_EQUAL_UINT32(1, syn_timer_wheel_step(&wheel));
    TEST_ASSERT_EQUAL_INT(1, s_timer1_fired);
    TEST_ASSERT_EQUAL_INT(0, s_timer2_fired);

    /* Step ticks 65..127: neither should fire */
    for (uint32_t i = SYN_TIMER_WHEEL_BUCKETS + 1; i < 2 * SYN_TIMER_WHEEL_BUCKETS; i++) {
        TEST_ASSERT_EQUAL_UINT32(0, syn_timer_wheel_step(&wheel));
        TEST_ASSERT_EQUAL_INT(1, s_timer1_fired);
        TEST_ASSERT_EQUAL_INT(0, s_timer2_fired);
    }

    /* Step tick 128: timer 2 MUST fire! */
    TEST_ASSERT_EQUAL_UINT32(1, syn_timer_wheel_step(&wheel));
    TEST_ASSERT_EQUAL_INT(1, s_timer1_fired);
    TEST_ASSERT_EQUAL_INT(1, s_timer2_fired);
}

void run_timer_wheel_tests(void)
{
    RUN_TEST(test_timer_wheel_add_step_cancel);
    RUN_TEST(test_timer_wheel_exact_bucket_multiple);
}
