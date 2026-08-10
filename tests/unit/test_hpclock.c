/**
 * @file test_hpclock.c
 * @brief Unity tests for syn_hpclock — capture, resolve, conversion, elapsed.
 *
 * Tests cover every overflow timing scenario:
 *   - No overflow (common case)
 *   - Overflow before lsb read  (lsb is small, post-wrap)
 *   - Overflow after lsb read   (lsb is large, pre-wrap)
 *   - Edge: lsb exactly at the half-range boundary
 *   - Elapsed across an overflow boundary
 *   - Tick-to-nanosecond integer conversion
 */

#include "mocks/mock_port.h"
#include "syntropic/drivers/syn_hpclock.h"
#include "syntropic/syntropic.h"
#include "unity/unity.h"

/* ── Simulated hardware ────────────────────────────────────────────────── */

/* ── Resolve tests ─────────────────────────────────────────────────────── */

static void test_resolve_no_overflow(void)
{
    /* msb_1 == msb_2 → clean, no overflow in the window */
    SYN_HPTimestamp ts;
    ts.msb_1 = 5;
    ts.lsb = 1000;
    ts.msb_2 = 5;

    TEST_ASSERT_EQUAL_UINT64(0, syn_hpclock_resolve(NULL));

    uint64_t ticks = syn_hpclock_resolve(&ts);
    uint64_t expected = ((uint64_t)5 << 32) | 1000ULL;
    TEST_ASSERT_EQUAL_UINT64(expected, ticks);
}

static void test_resolve_overflow_before_lsb(void)
{
    /*
     * Overflow fires between msb_1 and lsb reads.
     * Counter has already wrapped → lsb is small (post-wrap).
     * Correct MSB is msb_2 (the post-overflow value).
     */
    SYN_HPTimestamp ts;
    ts.msb_1 = 5;        /* read before overflow */
    ts.lsb = 0x00000010; /* small → post-wrap */
    ts.msb_2 = 6;        /* read after overflow */

    uint64_t ticks = syn_hpclock_resolve(&ts);
    uint64_t expected = ((uint64_t)6 << 32) | 0x00000010ULL;
    TEST_ASSERT_EQUAL_UINT64(expected, ticks);
}

static void test_resolve_overflow_after_lsb(void)
{
    /*
     * Overflow fires between lsb and msb_2 reads.
     * Counter hadn't wrapped yet → lsb is large (pre-wrap).
     * Correct MSB is msb_1 (the pre-overflow value).
     */
    SYN_HPTimestamp ts;
    ts.msb_1 = 5;        /* read before overflow */
    ts.lsb = 0xFFFFFFF0; /* large → pre-wrap */
    ts.msb_2 = 6;        /* read after overflow */

    uint64_t ticks = syn_hpclock_resolve(&ts);
    uint64_t expected = ((uint64_t)5 << 32) | 0xFFFFFFF0ULL;
    TEST_ASSERT_EQUAL_UINT64(expected, ticks);
}

static void test_resolve_overflow_lsb_at_half_range(void)
{
    /*
     * Edge case: lsb is exactly 0x80000000 (the half-range boundary).
     * Since lsb >= 0x80000000, this is classified as pre-wrap → msb_1.
     * This is correct: a value at 0x80000000 is in the upper half of
     * the counter range, meaning the wrap hasn't happened yet.
     */
    SYN_HPTimestamp ts;
    ts.msb_1 = 5;
    ts.lsb = 0x80000000;
    ts.msb_2 = 6;

    uint64_t ticks = syn_hpclock_resolve(&ts);
    uint64_t expected = ((uint64_t)5 << 32) | 0x80000000ULL;
    TEST_ASSERT_EQUAL_UINT64(expected, ticks);
}

static void test_resolve_overflow_lsb_just_below_half(void)
{
    /*
     * lsb = 0x7FFFFFFF — just below half range.
     * Classified as post-wrap → use msb_2.
     */
    SYN_HPTimestamp ts;
    ts.msb_1 = 5;
    ts.lsb = 0x7FFFFFFF;
    ts.msb_2 = 6;

    uint64_t ticks = syn_hpclock_resolve(&ts);
    uint64_t expected = ((uint64_t)6 << 32) | 0x7FFFFFFFULL;
    TEST_ASSERT_EQUAL_UINT64(expected, ticks);
}

/* ── Conversion tests ──────────────────────────────────────────────────── */

static void test_ticks_to_ns(void)
{
    /* Zero freq_hz guard test */
    mock_hpclock_freq_hz = 0;
    TEST_ASSERT_EQUAL_UINT64(0, syn_hpclock_ticks_to_ns(100));

    /* Reset to 1 MHz (1 tick = 1,000 ns = 1 µs) */
    mock_hpclock_freq_hz = 1000000;

    TEST_ASSERT_EQUAL_UINT64(0ULL, syn_hpclock_ticks_to_ns(0));
    TEST_ASSERT_EQUAL_UINT64(1000ULL, syn_hpclock_ticks_to_ns(1));
    TEST_ASSERT_EQUAL_UINT64(1000000ULL, syn_hpclock_ticks_to_ns(1000));
    TEST_ASSERT_EQUAL_UINT64(1000000000ULL, syn_hpclock_ticks_to_ns(1000000));
    /* Reset to 16 MHz default for other tests */
    mock_hpclock_freq_hz = 16000000UL;
}

static void test_ticks_to_ns_large(void)
{
    /* 10 seconds worth of ticks at 16 MHz */
    uint64_t ticks = 160000000ULL;
    uint64_t ns = syn_hpclock_ticks_to_ns(ticks);
    TEST_ASSERT_EQUAL_UINT64(10000000000ULL, ns);
}

/* ── Elapsed tests ─────────────────────────────────────────────────────── */

static void test_elapsed(void)
{
    SYN_HPTimestamp start, end;

    /* Start: tick 100, msb 0 — no overflow */
    start.msb_1 = 0;
    start.lsb = 100;
    start.msb_2 = 0;

    /* End: tick 500, msb 0 — no overflow */
    end.msb_1 = 0;
    end.lsb = 500;
    end.msb_2 = 0;

    uint64_t dt = syn_hpclock_elapsed(&start, &end);
    TEST_ASSERT_EQUAL_UINT64(400, dt);
}

static void test_elapsed_across_overflow(void)
{
    SYN_HPTimestamp start, end;

    /* Start: near end of msb=2 epoch — no overflow */
    start.msb_1 = 2;
    start.lsb = 0xFFFFFF00;
    start.msb_2 = 2;

    /* End: early in msb=3 epoch — no overflow */
    end.msb_1 = 3;
    end.lsb = 0x00000100;
    end.msb_2 = 3;

    uint64_t dt = syn_hpclock_elapsed(&start, &end);

    /* start = (2 << 32) | 0xFFFFFF00 */
    /* end   = (3 << 32) | 0x00000100 */
    /* diff  = 0x100 + 0x100 = 0x200  */
    TEST_ASSERT_EQUAL_UINT64(0x200, dt);
}

/* ── Capture & utility tests ───────────────────────────────────────────── */

static void test_capture_macro(void)
{
    syn_hpclock_msb = 42;
    mock_hpclock_lsb = 12345;

    SYN_HPTimestamp ts;
    SYN_HPCLOCK_CAPTURE(ts);

    TEST_ASSERT_EQUAL_UINT32(42, ts.msb_1);
    TEST_ASSERT_EQUAL_UINT32(12345, ts.lsb);
    TEST_ASSERT_EQUAL_UINT32(42, ts.msb_2);
}

static void test_is_zero(void)
{
    SYN_HPTimestamp ts = SYN_HPTIMESTAMP_INIT;
    TEST_ASSERT_TRUE(syn_hpclock_is_zero(&ts));

    ts.lsb = 1;
    TEST_ASSERT_FALSE(syn_hpclock_is_zero(&ts));

    ts = (SYN_HPTimestamp){.msb_1 = 1, .lsb = 0, .msb_2 = 0};
    TEST_ASSERT_FALSE(syn_hpclock_is_zero(&ts));

    ts = (SYN_HPTimestamp){.msb_1 = 0, .lsb = 0, .msb_2 = 1};
    TEST_ASSERT_FALSE(syn_hpclock_is_zero(&ts));
}

static void test_overflow_tick_macro(void)
{
    syn_hpclock_msb = 0;
    SYN_HPCLOCK_OVERFLOW_TICK();
    TEST_ASSERT_EQUAL_UINT32(1, syn_hpclock_msb);

    SYN_HPCLOCK_OVERFLOW_TICK();
    SYN_HPCLOCK_OVERFLOW_TICK();
    TEST_ASSERT_EQUAL_UINT32(3, syn_hpclock_msb);
}

void run_hpclock_tests(void)
{
    syn_hpclock_msb = 0;

    /* Resolve */
    RUN_TEST(test_resolve_no_overflow);
    RUN_TEST(test_resolve_overflow_before_lsb);
    RUN_TEST(test_resolve_overflow_after_lsb);
    RUN_TEST(test_resolve_overflow_lsb_at_half_range);
    RUN_TEST(test_resolve_overflow_lsb_just_below_half);

    /* Conversion */
    RUN_TEST(test_ticks_to_ns);
    RUN_TEST(test_ticks_to_ns_large);

    /* Elapsed */
    RUN_TEST(test_elapsed);
    RUN_TEST(test_elapsed_across_overflow);

    /* Capture & utility */
    RUN_TEST(test_capture_macro);
    RUN_TEST(test_is_zero);
    RUN_TEST(test_overflow_tick_macro);
}
