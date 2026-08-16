/**
 * @file test_sched.c
 * @brief Unity tests for syn_sched.
 */

#include "mocks/mock_port.h"
#include "syntropic/sched/syn_sched.h"
#include "syntropic/syntropic.h"
#include "syntropic/util/syn_event.h"
#include "unity/unity.h"

static int sched_order[10];
static int sched_order_idx = 0;

static SYN_PT_Status sched_task_a(SYN_PT *pt, SYN_Task *task)
{
    (void)task;
    PT_BEGIN(pt);

    sched_order[sched_order_idx++] = 1;
    PT_YIELD(pt);
    sched_order[sched_order_idx++] = 1;

    PT_END(pt);
}

static SYN_PT_Status sched_task_b(SYN_PT *pt, SYN_Task *task)
{
    (void)task;
    PT_BEGIN(pt);

    sched_order[sched_order_idx++] = 2;
    PT_YIELD(pt);
    sched_order[sched_order_idx++] = 2;

    PT_END(pt);
}

static void test_scheduler(void)
{
    SYN_Task tasks[2];
    SYN_Sched sched;

    syn_task_create(&tasks[0], "a", sched_task_a, 0, NULL);
    syn_task_create(&tasks[1], "b", sched_task_b, 0, NULL);
    syn_sched_init(&sched, tasks, 2);

    sched_order_idx = 0;
    memset(sched_order, 0, sizeof(sched_order));

    bool alive;

    alive = syn_sched_run(&sched);
    TEST_ASSERT_TRUE(alive);
    TEST_ASSERT_EQUAL_INT(1, sched_order_idx);

    alive = syn_sched_run(&sched);
    TEST_ASSERT_TRUE(alive);
    TEST_ASSERT_EQUAL_INT(2, sched_order_idx);

    alive = syn_sched_run(&sched);
    TEST_ASSERT_TRUE(alive);
    TEST_ASSERT_EQUAL_INT(3, sched_order_idx);

    alive = syn_sched_run(&sched);
    TEST_ASSERT_TRUE(alive);
    TEST_ASSERT_EQUAL_INT(4, sched_order_idx);

    alive = syn_sched_run(&sched);
    TEST_ASSERT_FALSE(alive);
    TEST_ASSERT_EQUAL_INT(0, syn_sched_alive_count(&sched));
}

static int suspend_counter = 0;

static SYN_PT_Status suspend_task_func(SYN_PT *pt, SYN_Task *task)
{
    (void)task;
    PT_BEGIN(pt);

    for (;;) {
        suspend_counter++;
        PT_YIELD(pt);
    }

    PT_END(pt);
}

static void test_suspend_resume(void)
{
    SYN_Task tasks[1];
    SYN_Sched sched;
    suspend_counter = 0;

    syn_task_create(&tasks[0], "cnt", suspend_task_func, 0, NULL);
    syn_sched_init(&sched, tasks, 1);

    syn_sched_run(&sched);
    TEST_ASSERT_EQUAL_INT(1, suspend_counter);

    syn_task_suspend(&tasks[0]);
    syn_sched_run(&sched);
    TEST_ASSERT_EQUAL_INT(1, suspend_counter);

    syn_task_resume(&tasks[0]);
    syn_sched_run(&sched);
    TEST_ASSERT_EQUAL_INT(2, suspend_counter);

    syn_task_restart(&tasks[0]);
    syn_sched_run(&sched);
    TEST_ASSERT_EQUAL_INT(3, suspend_counter);
}

/** syn_sched_run with 0 tasks — exercises line 68: return false */
static void test_sched_empty(void)
{
    mock_tick_ms = 0;
    SYN_Task tasks[2];
    SYN_Sched sched;
    syn_sched_init(&sched, tasks, 0);
    bool alive = syn_sched_run(&sched);
    TEST_ASSERT_FALSE(alive);
}

/** Task with delay_until in future — exercises line 103: continue (still waiting) */
static void test_sched_delayed_task(void)
{
    mock_tick_ms = 0;
    SYN_Task tasks[2];
    SYN_Sched sched;

    syn_task_create(&tasks[0], "a", sched_task_a, 0, NULL);
    syn_sched_init(&sched, tasks, 1);

    /* Set the task delay to 100ms in the future */
    tasks[0].delay_until = mock_tick_ms + 100;

    /* Run before delay expires — task should not execute */
    sched_order_idx = 0;
    syn_sched_run(&sched); /* still waiting — line 103 hit */
    TEST_ASSERT_EQUAL_INT(0, sched_order_idx);

    /* Advance past delay */
    mock_tick_advance(150);
    syn_sched_run(&sched);
    TEST_ASSERT_EQUAL_INT(1, sched_order_idx);
}

/** syn_sched_alive_count — exercises line 167 */
static void test_sched_alive_count(void)
{
    mock_tick_ms = 0;
    SYN_Task tasks[4];
    SYN_Sched sched;

    syn_task_create(&tasks[0], "a", sched_task_a, 0, NULL);
    syn_task_create(&tasks[1], "b", sched_task_b, 0, NULL);
    syn_sched_init(&sched, tasks, 2);
    TEST_ASSERT_EQUAL_size_t(2, syn_sched_alive_count(&sched));

    /* Run task_a to completion (2 steps + final dead check) */
    syn_sched_run(&sched);
    syn_sched_run(&sched);
    syn_sched_run(&sched);

    /* task_a should be DEAD now, task_b still alive */
    size_t alive = syn_sched_alive_count(&sched);
    TEST_ASSERT_TRUE(alive >= 1);
}

#include <setjmp.h>
static jmp_buf g_sched_jmp;

static SYN_PT_Status task_longjmp(SYN_PT *pt, SYN_Task *task)
{
    (void)pt;
    (void)task;
    longjmp(g_sched_jmp, 1);
    return PT_ENDED;
}

static void test_sched_run_forever(void)
{
    SYN_Sched sched;
    SYN_Task tasks[1];
    syn_task_create(&tasks[0], "jmp", task_longjmp, 0, NULL);
    syn_sched_init(&sched, tasks, 1);

    if (setjmp(g_sched_jmp) == 0) {
        syn_sched_run_forever(&sched);
        TEST_FAIL_MESSAGE("syn_sched_run_forever should not return normally");
    } else {
        TEST_ASSERT_TRUE(true);
    }
}

/* ── PT_DEFER and per-priority round-robin tests ─────────────────────── */

static int run_log[32];
static int run_log_idx;

static void log_reset(void)
{
    run_log_idx = 0;
    memset(run_log, 0, sizeof(run_log));
}

/* Task that logs its ID and defers every call */
static SYN_PT_Status defer_task(SYN_PT *pt, SYN_Task *task)
{
    int id = *(int *)task->user_data;
    PT_BEGIN(pt);
    for (;;) {
        run_log[run_log_idx++] = id;
        PT_DEFER(pt, task);
    }
    PT_END(pt);
}

/* Task that logs its ID and yields every call */
static SYN_PT_Status yield_task(SYN_PT *pt, SYN_Task *task)
{
    int id = *(int *)task->user_data;
    PT_BEGIN(pt);
    for (;;) {
        run_log[run_log_idx++] = id;
        PT_YIELD(pt);
    }
    PT_END(pt);
}

/**
 * Basic defer: A (pri 0, defers) should let B (pri 1) run every other pass.
 * Expected pattern: A, B, A, B, ...
 */
static void test_defer_basic(void)
{
    mock_tick_ms = 0;
    log_reset();

    SYN_Task tasks[2];
    SYN_Sched sched;
    static int id_a = 1, id_b = 2;

    syn_task_create(&tasks[0], "a", defer_task, 0, &id_a);
    syn_task_create(&tasks[1], "b", yield_task, 1, &id_b);
    syn_sched_init(&sched, tasks, 2);

    for (int i = 0; i < 6; i++) {
        syn_sched_run(&sched);
    }

    /* A, B, A, B, A, B */
    TEST_ASSERT_EQUAL_INT(1, run_log[0]);
    TEST_ASSERT_EQUAL_INT(2, run_log[1]);
    TEST_ASSERT_EQUAL_INT(1, run_log[2]);
    TEST_ASSERT_EQUAL_INT(2, run_log[3]);
    TEST_ASSERT_EQUAL_INT(1, run_log[4]);
    TEST_ASSERT_EQUAL_INT(2, run_log[5]);
}

/**
 * Per-priority round-robin: A (pri 0, defers) with B1, B2 (pri 1, yield).
 * B1 and B2 should alternate fairly: A, B1, A, B2, A, B1, ...
 */
static void test_defer_rr_fairness(void)
{
    mock_tick_ms = 0;
    log_reset();

    SYN_Task tasks[3];
    SYN_Sched sched;
    static int id_a = 1, id_b1 = 2, id_b2 = 3;

    syn_task_create(&tasks[0], "a", defer_task, 0, &id_a);
    syn_task_create(&tasks[1], "b1", yield_task, 1, &id_b1);
    syn_task_create(&tasks[2], "b2", yield_task, 1, &id_b2);
    syn_sched_init(&sched, tasks, 3);

    for (int i = 0; i < 8; i++) {
        syn_sched_run(&sched);
    }

    /* A, B1, A, B2, A, B1, A, B2 */
    TEST_ASSERT_EQUAL_INT(1, run_log[0]); /* A */
    TEST_ASSERT_EQUAL_INT(2, run_log[1]); /* B1 */
    TEST_ASSERT_EQUAL_INT(1, run_log[2]); /* A */
    TEST_ASSERT_EQUAL_INT(3, run_log[3]); /* B2 */
    TEST_ASSERT_EQUAL_INT(1, run_log[4]); /* A */
    TEST_ASSERT_EQUAL_INT(2, run_log[5]); /* B1 */
    TEST_ASSERT_EQUAL_INT(1, run_log[6]); /* A */
    TEST_ASSERT_EQUAL_INT(3, run_log[7]); /* B2 */
}

/**
 * Same-priority round-robin still works without defer.
 * Two pri-0 tasks that yield should alternate: A, B, A, B, ...
 */
static void test_rr_same_priority(void)
{
    mock_tick_ms = 0;
    log_reset();

    SYN_Task tasks[2];
    SYN_Sched sched;
    static int id_a = 1, id_b = 2;

    syn_task_create(&tasks[0], "a", yield_task, 0, &id_a);
    syn_task_create(&tasks[1], "b", yield_task, 0, &id_b);
    syn_sched_init(&sched, tasks, 2);

    for (int i = 0; i < 6; i++) {
        syn_sched_run(&sched);
    }

    TEST_ASSERT_EQUAL_INT(1, run_log[0]);
    TEST_ASSERT_EQUAL_INT(2, run_log[1]);
    TEST_ASSERT_EQUAL_INT(1, run_log[2]);
    TEST_ASSERT_EQUAL_INT(2, run_log[3]);
    TEST_ASSERT_EQUAL_INT(1, run_log[4]);
    TEST_ASSERT_EQUAL_INT(2, run_log[5]);
}

/**
 * Strict priority without defer: A (pri 0, yields) starves B (pri 1).
 * This verifies that defer is needed and that strict priority is preserved.
 */
static void test_strict_priority_no_defer(void)
{
    mock_tick_ms = 0;
    log_reset();

    SYN_Task tasks[2];
    SYN_Sched sched;
    static int id_a = 1, id_b = 2;

    syn_task_create(&tasks[0], "a", yield_task, 0, &id_a);
    syn_task_create(&tasks[1], "b", yield_task, 1, &id_b);
    syn_sched_init(&sched, tasks, 2);

    for (int i = 0; i < 4; i++) {
        syn_sched_run(&sched);
    }

    /* Only A runs — B starves (strict priority, no defer) */
    TEST_ASSERT_EQUAL_INT(1, run_log[0]);
    TEST_ASSERT_EQUAL_INT(1, run_log[1]);
    TEST_ASSERT_EQUAL_INT(1, run_log[2]);
    TEST_ASSERT_EQUAL_INT(1, run_log[3]);
}

/**
 * Defer with 3 lower-priority tasks: A (pri 0, defers), B1/B2/B3 (pri 1).
 * All three should get fair rotation: A, B1, A, B2, A, B3, A, B1, ...
 */
static void test_defer_rr_three_lower(void)
{
    mock_tick_ms = 0;
    log_reset();

    SYN_Task tasks[4];
    SYN_Sched sched;
    static int id_a = 1, id_b1 = 2, id_b2 = 3, id_b3 = 4;

    syn_task_create(&tasks[0], "a", defer_task, 0, &id_a);
    syn_task_create(&tasks[1], "b1", yield_task, 1, &id_b1);
    syn_task_create(&tasks[2], "b2", yield_task, 1, &id_b2);
    syn_task_create(&tasks[3], "b3", yield_task, 1, &id_b3);
    syn_sched_init(&sched, tasks, 4);

    for (int i = 0; i < 8; i++) {
        syn_sched_run(&sched);
    }

    /* A, B1, A, B2, A, B3, A, B1 */
    TEST_ASSERT_EQUAL_INT(1, run_log[0]); /* A */
    TEST_ASSERT_EQUAL_INT(2, run_log[1]); /* B1 */
    TEST_ASSERT_EQUAL_INT(1, run_log[2]); /* A */
    TEST_ASSERT_EQUAL_INT(3, run_log[3]); /* B2 */
    TEST_ASSERT_EQUAL_INT(1, run_log[4]); /* A */
    TEST_ASSERT_EQUAL_INT(4, run_log[5]); /* B3 */
    TEST_ASSERT_EQUAL_INT(1, run_log[6]); /* A */
    TEST_ASSERT_EQUAL_INT(2, run_log[7]); /* B1 */
}

/**
 * DEFERRED state lifecycle: task defers → state = DEFERRED → skipped
 * one pass → cleared back to READY.
 */
static void test_defer_state_lifecycle(void)
{
    mock_tick_ms = 0;

    SYN_Task tasks[2];
    SYN_Sched sched;
    static int id_a = 1, id_b = 2;

    syn_task_create(&tasks[0], "a", defer_task, 0, &id_a);
    syn_task_create(&tasks[1], "b", yield_task, 1, &id_b);
    syn_sched_init(&sched, tasks, 2);

    /* Pass 1: A runs and defers */
    log_reset();
    syn_sched_run(&sched);
    TEST_ASSERT_EQUAL_UINT8(SYN_TASK_DEFERRED, tasks[0].state);
    TEST_ASSERT_EQUAL_UINT8(SYN_TASK_READY, tasks[1].state);

    /* Pass 2: A skipped (DEFERRED), B runs, A cleared to READY */
    syn_sched_run(&sched);
    TEST_ASSERT_EQUAL_UINT8(SYN_TASK_READY, tasks[0].state);
    TEST_ASSERT_EQUAL_UINT8(SYN_TASK_READY, tasks[1].state);
}

/**
 * Defer is compatible with suspend: a deferred task can be suspended,
 * and when resumed it resumes as READY, not DEFERRED.
 */
static void test_defer_then_suspend(void)
{
    mock_tick_ms = 0;
    log_reset();

    SYN_Task tasks[2];
    SYN_Sched sched;
    static int id_a = 1, id_b = 2;

    syn_task_create(&tasks[0], "a", defer_task, 0, &id_a);
    syn_task_create(&tasks[1], "b", yield_task, 1, &id_b);
    syn_sched_init(&sched, tasks, 2);

    /* A runs and defers */
    syn_sched_run(&sched);
    TEST_ASSERT_EQUAL_UINT8(SYN_TASK_DEFERRED, tasks[0].state);

    /* Suspend A while it's deferred */
    syn_task_suspend(&tasks[0]);
    TEST_ASSERT_EQUAL_UINT8(SYN_TASK_SUSPENDED, tasks[0].state);

    /* B should run while A is suspended */
    syn_sched_run(&sched);
    TEST_ASSERT_EQUAL_INT(2, run_log[1]);

    /* Resume A — should come back as READY */
    syn_task_resume(&tasks[0]);
    TEST_ASSERT_EQUAL_UINT8(SYN_TASK_READY, tasks[0].state);
}

/* ── PT_BLOCK_EVENT tests ────────────────────────────────────────────────── */

#define EVT_DATA SYN_BIT(0)
#define EVT_DONE SYN_BIT(1)

static int block_counter;

/* Task that blocks on an event, increments counter when woken */
static SYN_PT_Status block_task_fn(SYN_PT *pt, SYN_Task *task)
{
    SYN_EventGroup *evt = (SYN_EventGroup *)task->user_data;
    PT_BEGIN(pt);
    for (;;) {
        PT_BLOCK_EVENT(pt, task, evt, EVT_DATA);
        block_counter++;
    }
    PT_END(pt);
}

/**
 * Basic: task blocks, event set, task wakes and runs.
 */
static void test_block_event_basic(void)
{
    mock_tick_ms = 0;
    block_counter = 0;

    SYN_EventGroup evt;
    syn_event_init(&evt);

    SYN_Task tasks[1];
    SYN_Sched sched;
    syn_task_create(&tasks[0], "blk", block_task_fn, 0, &evt);
    syn_sched_init(&sched, tasks, 1);

    /* Pass 1: task runs, hits PT_BLOCK_EVENT, goes BLOCKED */
    syn_sched_run(&sched);
    TEST_ASSERT_EQUAL_UINT8(SYN_TASK_BLOCKED, tasks[0].state);
    TEST_ASSERT_EQUAL_INT(0, block_counter);

    /* Pass 2: no event — task stays blocked */
    syn_sched_run(&sched);
    TEST_ASSERT_EQUAL_UINT8(SYN_TASK_BLOCKED, tasks[0].state);
    TEST_ASSERT_EQUAL_INT(0, block_counter);

    /* Set event flag */
    syn_event_set(&evt, EVT_DATA);

    /* Pass 3: scheduler sees event, wakes task, task increments counter */
    syn_sched_run(&sched);
    TEST_ASSERT_EQUAL_INT(1, block_counter);

    /* Flag should be auto-cleared */
    TEST_ASSERT_FALSE(syn_event_check_any(&evt, EVT_DATA));
}

/**
 * Blocked task is skipped — lower-priority task runs.
 */
static void test_block_event_skips_scan(void)
{
    mock_tick_ms = 0;
    log_reset();
    block_counter = 0;

    SYN_EventGroup evt;
    syn_event_init(&evt);

    SYN_Task tasks[2];
    SYN_Sched sched;
    static int id_b = 2;

    syn_task_create(&tasks[0], "blk", block_task_fn, 0, &evt);
    syn_task_create(&tasks[1], "low", yield_task, 1, &id_b);
    syn_sched_init(&sched, tasks, 2);

    /* Pass 1: high-pri task runs, blocks */
    syn_sched_run(&sched);
    TEST_ASSERT_EQUAL_UINT8(SYN_TASK_BLOCKED, tasks[0].state);

    /* Pass 2: high-pri blocked, low-pri runs */
    syn_sched_run(&sched);
    TEST_ASSERT_EQUAL_INT(2, run_log[0]);    /* Low-pri ran */
    TEST_ASSERT_EQUAL_INT(0, block_counter); /* High-pri didn't */
}

/**
 * Auto-clear: matched event flags are cleared after task resumes.
 */
static void test_block_event_auto_clear(void)
{
    mock_tick_ms = 0;
    block_counter = 0;

    SYN_EventGroup evt;
    syn_event_init(&evt);

    SYN_Task tasks[1];
    SYN_Sched sched;
    syn_task_create(&tasks[0], "blk", block_task_fn, 0, &evt);
    syn_sched_init(&sched, tasks, 1);

    /* Block the task */
    syn_sched_run(&sched);

    /* Set both DATA and DONE flags */
    syn_event_set(&evt, EVT_DATA | EVT_DONE);

    /* Wake — task waits on EVT_DATA only */
    syn_sched_run(&sched);

    /* EVT_DATA should be cleared (auto-clear), EVT_DONE should remain */
    TEST_ASSERT_FALSE(syn_event_check_any(&evt, EVT_DATA));
    TEST_ASSERT_TRUE(syn_event_check_any(&evt, EVT_DONE));
}

/**
 * Priority interaction: blocked high-pri task wakes and preempts.
 */
static void test_block_event_priority(void)
{
    mock_tick_ms = 0;
    log_reset();
    block_counter = 0;

    SYN_EventGroup evt;
    syn_event_init(&evt);

    SYN_Task tasks[2];
    SYN_Sched sched;
    static int id_b = 2;

    syn_task_create(&tasks[0], "blk", block_task_fn, 0, &evt);
    syn_task_create(&tasks[1], "low", yield_task, 1, &id_b);
    syn_sched_init(&sched, tasks, 2);

    /* Block high-pri */
    syn_sched_run(&sched);
    TEST_ASSERT_EQUAL_UINT8(SYN_TASK_BLOCKED, tasks[0].state);

    /* Low-pri runs while high-pri is blocked */
    syn_sched_run(&sched);
    TEST_ASSERT_EQUAL_INT(2, run_log[0]);

    /* Set event — high-pri should wake and run next */
    syn_event_set(&evt, EVT_DATA);
    log_reset();
    syn_sched_run(&sched);
    TEST_ASSERT_EQUAL_INT(1, block_counter); /* High-pri ran */
}

/**
 * Multiple tasks blocking on different events.
 */
static void test_block_event_multiple_tasks(void)
{
    mock_tick_ms = 0;
    SYN_EventGroup evt_a, evt_b;
    syn_event_init(&evt_a);
    syn_event_init(&evt_b);

    SYN_Task tasks[2];
    SYN_Sched sched;

    syn_task_create(&tasks[0], "a", block_task_fn, 0, &evt_a);
    syn_task_create(&tasks[1], "b", block_task_fn, 0, &evt_b);
    syn_sched_init(&sched, tasks, 2);

    block_counter = 0;

    /* Both tasks run once, then block */
    syn_sched_run(&sched); /* Task A blocks */
    syn_sched_run(&sched); /* Task B blocks */
    TEST_ASSERT_EQUAL_UINT8(SYN_TASK_BLOCKED, tasks[0].state);
    TEST_ASSERT_EQUAL_UINT8(SYN_TASK_BLOCKED, tasks[1].state);

    /* Only fire event A */
    syn_event_set(&evt_a, EVT_DATA);
    syn_sched_run(&sched);
    TEST_ASSERT_EQUAL_INT(1, block_counter);                   /* Only A woke */
    TEST_ASSERT_EQUAL_UINT8(SYN_TASK_BLOCKED, tasks[1].state); /* B still blocked */

    /* Fire event B */
    syn_event_set(&evt_b, EVT_DATA);
    syn_sched_run(&sched);
    TEST_ASSERT_EQUAL_INT(2, block_counter); /* Now B woke too */
}

/**
 * Suspend overrides block: suspending a blocked task changes state.
 */
static void test_block_event_suspend(void)
{
    mock_tick_ms = 0;
    block_counter = 0;

    SYN_EventGroup evt;
    syn_event_init(&evt);

    SYN_Task tasks[1];
    SYN_Sched sched;
    syn_task_create(&tasks[0], "blk", block_task_fn, 0, &evt);
    syn_sched_init(&sched, tasks, 1);

    /* Block the task */
    syn_sched_run(&sched);
    TEST_ASSERT_EQUAL_UINT8(SYN_TASK_BLOCKED, tasks[0].state);

    /* Suspend while blocked */
    syn_task_suspend(&tasks[0]);
    TEST_ASSERT_EQUAL_UINT8(SYN_TASK_SUSPENDED, tasks[0].state);

    /* Set event — task should NOT wake (it's suspended) */
    syn_event_set(&evt, EVT_DATA);
    syn_sched_run(&sched);
    TEST_ASSERT_EQUAL_INT(0, block_counter);

    /* Resume — task goes to READY, not BLOCKED */
    syn_task_resume(&tasks[0]);
    TEST_ASSERT_EQUAL_UINT8(SYN_TASK_READY, tasks[0].state);
}

/* ── PT_WAITING retry tests ──────────────────────────────────────────────── */

static bool wait_cond_a;

/* Task that waits on a condition, logs ID when condition is true */
static SYN_PT_Status wait_task(SYN_PT *pt, SYN_Task *task)
{
    int id = *(int *)task->user_data;
    PT_BEGIN(pt);
    for (;;) {
        PT_WAIT_UNTIL(pt, wait_cond_a);
        run_log[run_log_idx++] = id;
        wait_cond_a = false; /* consume the event */
        PT_YIELD(pt);
    }
    PT_END(pt);
}

/**
 * Waiting high-pri task doesn't starve lower-pri task.
 * A (pri 0) waits on false condition. B (pri 1) should run.
 */
static void test_waiting_doesnt_starve(void)
{
    mock_tick_ms = 0;
    log_reset();
    wait_cond_a = false;

    SYN_Task tasks[2];
    SYN_Sched sched;
    static int id_a = 1, id_b = 2;

    syn_task_create(&tasks[0], "w", wait_task, 0, &id_a);
    syn_task_create(&tasks[1], "b", yield_task, 1, &id_b);
    syn_sched_init(&sched, tasks, 2);

    /* Tick 1: A runs, hits PT_WAIT_UNTIL (false) → WAITING.
     * Scheduler retries → B runs. */
    syn_sched_run(&sched);
    TEST_ASSERT_EQUAL_INT(1, run_log_idx);
    TEST_ASSERT_EQUAL_INT(2, run_log[0]); /* B ran, not A */

    /* Tick 2: same pattern — B keeps running */
    syn_sched_run(&sched);
    TEST_ASSERT_EQUAL_INT(2, run_log_idx);
    TEST_ASSERT_EQUAL_INT(2, run_log[1]); /* B again */

    /* Tick 3: condition becomes true — A runs */
    wait_cond_a = true;
    syn_sched_run(&sched);
    TEST_ASSERT_EQUAL_INT(3, run_log_idx);
    TEST_ASSERT_EQUAL_INT(1, run_log[2]); /* A ran */
}

/**
 * PT_DEFER + PT_WAITING don't interfere.
 * A (pri 0, defers) → B (pri 1, waits) → C (pri 2, yields).
 * Expected: A, C, A, C, ...  (B never does useful work)
 */
static void test_waiting_with_defer_no_inversion(void)
{
    mock_tick_ms = 0;
    log_reset();
    wait_cond_a = false;

    SYN_Task tasks[3];
    SYN_Sched sched;
    static int id_a = 1, id_b = 2, id_c = 3;

    syn_task_create(&tasks[0], "a", defer_task, 0, &id_a);
    syn_task_create(&tasks[1], "w", wait_task, 1, &id_b);
    syn_task_create(&tasks[2], "c", yield_task, 2, &id_c);
    syn_sched_init(&sched, tasks, 3);

    /* Tick 1: A runs, defers */
    syn_sched_run(&sched);
    TEST_ASSERT_EQUAL_INT(1, run_log[0]); /* A */
    TEST_ASSERT_EQUAL_UINT8(SYN_TASK_DEFERRED, tasks[0].state);

    /* Tick 2: A deferred → skipped. B waits → WAITING. C runs.
     * A cleared to READY at end of tick. */
    syn_sched_run(&sched);
    TEST_ASSERT_EQUAL_INT(3, run_log[1]); /* C */
    TEST_ASSERT_EQUAL_UINT8(SYN_TASK_READY, tasks[0].state);

    /* Tick 3: A is back — runs and defers again */
    syn_sched_run(&sched);
    TEST_ASSERT_EQUAL_INT(1, run_log[2]); /* A */

    /* Tick 4: A deferred, B waits, C runs */
    syn_sched_run(&sched);
    TEST_ASSERT_EQUAL_INT(3, run_log[3]); /* C */
}

/**
 * All tasks waiting — no infinite loop, all cleared to READY.
 */
static void test_all_tasks_waiting(void)
{
    mock_tick_ms = 0;
    log_reset();
    wait_cond_a = false;

    SYN_Task tasks[3];
    SYN_Sched sched;
    static int id_a = 1, id_b = 2, id_c = 3;

    syn_task_create(&tasks[0], "a", wait_task, 0, &id_a);
    syn_task_create(&tasks[1], "b", wait_task, 1, &id_b);
    syn_task_create(&tasks[2], "c", wait_task, 2, &id_c);
    syn_sched_init(&sched, tasks, 3);

    /* All tasks wait — should not hang, no log entries */
    bool alive = syn_sched_run(&sched);
    TEST_ASSERT_TRUE(alive);
    TEST_ASSERT_EQUAL_INT(0, run_log_idx);

    /* All should be READY again for next tick */
    TEST_ASSERT_EQUAL_UINT8(SYN_TASK_READY, tasks[0].state);
    TEST_ASSERT_EQUAL_UINT8(SYN_TASK_READY, tasks[1].state);
    TEST_ASSERT_EQUAL_UINT8(SYN_TASK_READY, tasks[2].state);
}

/**
 * WAITING state lifecycle: set during tick, cleared at end.
 */
static void test_waiting_state_lifecycle(void)
{
    mock_tick_ms = 0;
    wait_cond_a = false;

    SYN_Task tasks[1];
    SYN_Sched sched;
    static int id_a = 1;

    syn_task_create(&tasks[0], "w", wait_task, 0, &id_a);
    syn_sched_init(&sched, tasks, 1);

    /* After tick: task was WAITING during tick but cleared to READY */
    syn_sched_run(&sched);
    TEST_ASSERT_EQUAL_UINT8(SYN_TASK_READY, tasks[0].state);
}

/**
 * Same-priority: one waits, the other works. They should alternate
 * fairly once the waiting condition clears.
 */
static void test_waiting_same_priority_fairness(void)
{
    mock_tick_ms = 0;
    log_reset();
    wait_cond_a = false;

    SYN_Task tasks[2];
    SYN_Sched sched;
    static int id_a = 1, id_b = 2;

    syn_task_create(&tasks[0], "w", wait_task, 0, &id_a);
    syn_task_create(&tasks[1], "b", yield_task, 0, &id_b);
    syn_sched_init(&sched, tasks, 2);

    /* While A waits, B should run every tick */
    syn_sched_run(&sched);
    syn_sched_run(&sched);
    TEST_ASSERT_EQUAL_INT(2, run_log[0]); /* B */
    TEST_ASSERT_EQUAL_INT(2, run_log[1]); /* B */

    /* Condition true: A runs (RR should give it a turn) */
    wait_cond_a = true;
    syn_sched_run(&sched);
    TEST_ASSERT_EQUAL_INT(1, run_log[2]); /* A */
}

/**
 * Delayed prio-0 + waiting prio-1: no priority inversion.
 * A (pri 0) is delayed. B (pri 1) waits. C (pri 2) should run.
 * When A's delay expires, A runs first.
 */
static void test_waiting_delayed_no_inversion(void)
{
    mock_tick_ms = 0;
    log_reset();
    wait_cond_a = false;

    SYN_Task tasks[3];
    SYN_Sched sched;
    static int id_a = 1, id_b = 2, id_c = 3;

    syn_task_create(&tasks[0], "a", yield_task, 0, &id_a);
    syn_task_create(&tasks[1], "w", wait_task, 1, &id_b);
    syn_task_create(&tasks[2], "c", yield_task, 2, &id_c);
    syn_sched_init(&sched, tasks, 3);

    /* Delay A for 100ms */
    tasks[0].delay_until = mock_tick_ms + 100;

    /* Tick: A delayed, B waits, C runs */
    syn_sched_run(&sched);
    TEST_ASSERT_EQUAL_INT(3, run_log[0]); /* C, not B */

    /* Advance past A's delay */
    mock_tick_advance(150);

    /* Tick: A is ready (pri 0) → runs first */
    syn_sched_run(&sched);
    TEST_ASSERT_EQUAL_INT(1, run_log[1]); /* A */
}

static void test_task_priority_boost_and_restore(void)
{
    SYN_Task tasks[2];
    SYN_Sched sched;
    static int id_a = 1, id_b = 2;

    syn_task_create(&tasks[0], "a", yield_task, 3, &id_a); /* Low priority 3 */
    syn_task_create(&tasks[1], "b", yield_task, 1, &id_b); /* High priority 1 */
    syn_sched_init(&sched, tasks, 2);

    TEST_ASSERT_EQUAL_UINT8(3, tasks[0].priority);
    TEST_ASSERT_EQUAL_UINT8(3, tasks[0].base_priority);
    TEST_ASSERT_EQUAL_UINT8(1, tasks[1].priority);

    log_reset();
    syn_sched_run(&sched);
    TEST_ASSERT_EQUAL_INT(2, run_log[0]); /* Task B (prio 1) runs first */

    /* Boost Task A from prio 3 to prio 0 (highest) */
    syn_task_boost_priority(&tasks[0], 0);
    TEST_ASSERT_EQUAL_UINT8(0, tasks[0].priority);
    TEST_ASSERT_EQUAL_UINT8(3, tasks[0].base_priority); /* Base priority unchanged */

    log_reset();
    syn_sched_run(&sched);
    TEST_ASSERT_EQUAL_INT(1, run_log[0]); /* Task A now preempts Task B */

    /* Attempt invalid boost (try to demote to prio 5) — should be rejected */
    syn_task_boost_priority(&tasks[0], 5);
    TEST_ASSERT_EQUAL_UINT8(0, tasks[0].priority); /* Stays at boosted 0 */

    /* Restore Task A back to base priority 3 */
    syn_task_restore_priority(&tasks[0]);
    TEST_ASSERT_EQUAL_UINT8(3, tasks[0].priority);

    log_reset();
    syn_sched_run(&sched);
    TEST_ASSERT_EQUAL_INT(2, run_log[0]); /* Task B runs again */

    /* Dynamically change base priority */
    syn_task_set_base_priority(&tasks[0], 0);
    TEST_ASSERT_EQUAL_UINT8(0, tasks[0].priority);
    TEST_ASSERT_EQUAL_UINT8(0, tasks[0].base_priority);
}

/**
 * Two high-priority tasks (A1, A2 at pri 0) both deferring.
 * Tracing the ping-pong deferral behavior.
 */
static void test_two_deferring_high_pri_tasks_starve_lower(void)
{
    mock_tick_ms = 0;
    log_reset();

    SYN_Task tasks[3];
    SYN_Sched sched;
    static int id_a1 = 1, id_a2 = 2, id_b = 3;

    syn_task_create(&tasks[0], "a1", defer_task, 0, &id_a1);
    syn_task_create(&tasks[1], "a2", defer_task, 0, &id_a2);
    syn_task_create(&tasks[2], "b", yield_task, 1, &id_b);
    syn_sched_init(&sched, tasks, 3);

    for (int i = 0; i < 6; i++) {
        syn_sched_run(&sched);
    }

    /* A1 runs (defers A1), A2 runs (clears A1, defers A2), A1 runs (clears A2, defers A1)... */
    /* Log: A1(1), A2(2), A1(1), A2(2), A1(1), A2(2) */
    /* Task B (3) never runs! */
    TEST_ASSERT_EQUAL_INT(1, run_log[0]);
    TEST_ASSERT_EQUAL_INT(2, run_log[1]);
    TEST_ASSERT_EQUAL_INT(1, run_log[2]);
    TEST_ASSERT_EQUAL_INT(2, run_log[3]);
}

static SYN_PT_Status us_delay_task_func(SYN_PT *pt, SYN_Task *task)
{
    (void)task;
    static uint32_t us_target;
    PT_BEGIN(pt);
    run_log[run_log_idx++] = 1;
    PT_DELAY_US(pt, &us_target, 50); /* 50 microsecond delay */
    run_log[run_log_idx++] = 2;
    PT_END(pt);
}

static void test_pt_delay_us(void)
{
    mock_tick_us = 0;
    log_reset();

    SYN_Task tasks[1];
    SYN_Sched sched;

    syn_task_create(&tasks[0], "us_task", us_delay_task_func, 0, NULL);
    syn_sched_init(&sched, tasks, 1);

    /* Step 1: Task runs, logs 1, sets us_target = 50us, returns PT_WAITING */
    syn_sched_run(&sched);
    TEST_ASSERT_EQUAL_INT(1, run_log_idx);
    TEST_ASSERT_EQUAL_INT(1, run_log[0]);

    /* Step 2: Advance by 20us (less than 50us) — task returns PT_WAITING */
    mock_tick_advance_us(20);
    syn_sched_run(&sched);
    TEST_ASSERT_EQUAL_INT(1, run_log_idx);

    /* Step 3: Advance by 35us more (total 55us >= 50us) — task completes and logs 2 */
    mock_tick_advance_us(35);
    syn_sched_run(&sched);
    TEST_ASSERT_EQUAL_INT(2, run_log_idx);
    TEST_ASSERT_EQUAL_INT(2, run_log[1]);
}

static bool block_cond_flag = false;
static int block_cond_runs = 0;

static SYN_PT_Status task_block_cond_entry(SYN_PT *pt, SYN_Task *task)
{
    PT_BEGIN(pt);

    block_cond_runs++;
    PT_BLOCK_CONDITION(pt, task, block_cond_flag);
    block_cond_runs++;

    PT_END(pt);
}

static void test_block_condition_and_primitives(void)
{
    SYN_Task task;
    SYN_Sched sched;

    block_cond_flag = false;
    block_cond_runs = 0;

    syn_task_create(&task, "block_cond", task_block_cond_entry, 0, NULL);
    syn_sched_init(&sched, &task, 1);

    /* Run 1: Task runs up to PT_BLOCK_CONDITION, increments to 1, sets state to BLOCKED */
    syn_sched_run(&sched);
    TEST_ASSERT_EQUAL_INT(1, block_cond_runs);
    TEST_ASSERT_EQUAL_INT((uint8_t)SYN_TASK_BLOCKED, task.state);

    /* Run 2: Task is BLOCKED, condition still false — scheduler skips task */
    syn_sched_run(&sched);
    TEST_ASSERT_EQUAL_INT(1, block_cond_runs);
    TEST_ASSERT_EQUAL_INT((uint8_t)SYN_TASK_BLOCKED, task.state);

    /* Resume task by setting condition true and unblocking */
    block_cond_flag = true;
    syn_task_resume(&task);
    TEST_ASSERT_EQUAL_INT((uint8_t)SYN_TASK_READY, task.state);

    /* Run 3: Task unblocks, evaluates condition true, and completes */
    syn_sched_run(&sched);
    TEST_ASSERT_EQUAL_INT(2, block_cond_runs);
    TEST_ASSERT_EQUAL_INT((uint8_t)SYN_TASK_DEAD, task.state);
}

/* ── PT_BLOCK_CONDITION lost-wakeup regression ──────────────────────────── */

static SYN_Task *g_lw_task;
static volatile bool g_lw_cond;
static bool g_lw_fire_isr_once;
static uint8_t g_lw_state_at_eval;
static int g_lw_done;

/**
 * Condition expression with an "ISR" side effect. Records the task state
 * observed at evaluation time, then — one-shot — simulates an interrupt
 * that produces the data and resumes the task, exactly in the window
 * between the condition check and the yield.
 */
static bool lw_cond_eval(void)
{
    bool result = g_lw_cond;

    g_lw_state_at_eval = g_lw_task->state;
    if (g_lw_fire_isr_once) {
        g_lw_fire_isr_once = false;
        g_lw_cond = true;           /* ISR produces the data...       */
        syn_task_resume(g_lw_task); /* ...and wakes the consumer task */
    }
    return result;
}

static SYN_PT_Status lw_task_fn(SYN_PT *pt, SYN_Task *task)
{
    PT_BEGIN(pt);
    PT_BLOCK_CONDITION(pt, task, lw_cond_eval());
    g_lw_done = 1;
    PT_END(pt);
}

/**
 * Regression: PT_BLOCK_CONDITION must set BLOCKED before evaluating the
 * condition. With the old order (evaluate, then block), an ISR firing in
 * between saw READY, its syn_task_resume() was a no-op, and the task then
 * blocked forever on a condition that was already true (lost wakeup).
 */
static void test_block_condition_isr_wakeup_not_lost(void)
{
    SYN_Task task;
    SYN_Sched sched;

    g_lw_task = &task;
    g_lw_cond = false;
    g_lw_fire_isr_once = true;
    g_lw_state_at_eval = 0xFF;
    g_lw_done = 0;

    syn_task_create(&task, "lost_wake", lw_task_fn, 0, NULL);
    syn_sched_init(&sched, &task, 1);

    /* Run 1: condition false, "ISR" fires at the evaluation boundary.
     * The task must already be BLOCKED so the resume takes effect. */
    syn_sched_run(&sched);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)SYN_TASK_BLOCKED, g_lw_state_at_eval);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)SYN_TASK_READY, task.state);
    TEST_ASSERT_EQUAL_INT(0, g_lw_done);

    /* Run 2: resumed task re-evaluates, condition now true, completes.
     * Pre-fix this deadlocked: BLOCKED with wait_event == NULL. */
    syn_sched_run(&sched);
    TEST_ASSERT_EQUAL_INT(1, g_lw_done);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)SYN_TASK_DEAD, task.state);
}

/**
 * Regression: PT_BLOCK_EVENT's auto-clear must go through the
 * critical-section-protected syn_event_flags_clear(), not a raw RMW that
 * an ISR flag-set could interleave with.
 */
static void test_block_event_auto_clear_is_atomic(void)
{
    mock_tick_ms = 0;
    block_counter = 0;

    SYN_EventGroup evt;
    syn_event_init(&evt);

    SYN_Task tasks[1];
    SYN_Sched sched;
    syn_task_create(&tasks[0], "blk", block_task_fn, 0, &evt);
    syn_sched_init(&sched, tasks, 1);

    /* Block the task, then fire the event plus an unrelated bit */
    syn_sched_run(&sched);
    syn_event_set(&evt, EVT_DATA | SYN_BIT(5));

    int enters_before = mock_critical_enter_count;

    /* Wake — the auto-clear inside PT_BLOCK_EVENT runs on this pass */
    syn_sched_run(&sched);
    TEST_ASSERT_EQUAL_INT(1, block_counter);

    /* Clear ran inside a critical section (raw RMW would not enter one) */
    TEST_ASSERT_GREATER_THAN_INT(enters_before, mock_critical_enter_count);
    TEST_ASSERT_EQUAL_INT(0, mock_critical_depth);

    /* Only the waited-on mask was cleared; the unrelated bit survives */
    TEST_ASSERT_FALSE(syn_event_check_any(&evt, EVT_DATA));
    TEST_ASSERT_TRUE(syn_event_check_any(&evt, SYN_BIT(5)));
}

/* ── PT_TASK_DELAY_MS deadline-lifecycle regressions ─────────────────────── */

static int stale_runs;

/* Task that delays once at startup, then yields forever (event-driven
 * long-lived task shape) */
static SYN_PT_Status delay_once_task(SYN_PT *pt, SYN_Task *task)
{
    PT_BEGIN(pt);
    PT_TASK_DELAY_MS(pt, task, 10);
    for (;;) {
        stale_runs++;
        PT_YIELD(pt);
    }
    PT_END(pt);
}

/**
 * Regression: a consumed delay deadline must be cleared. A stale
 * delay_until reads as "in the future" again once the tick advances
 * 2^31 ms (~24.8 days) past it, and the scheduler then freezes the task
 * for up to another 24.8 days.
 */
static void test_delay_deadline_consumed_no_wrap_freeze(void)
{
    mock_tick_ms = 1000;
    stale_runs = 0;

    SYN_Task tasks[1];
    SYN_Sched sched;
    syn_task_create(&tasks[0], "d", delay_once_task, 0, NULL);
    syn_sched_init(&sched, tasks, 1);

    /* Task starts its 10ms delay */
    syn_sched_run(&sched);
    TEST_ASSERT_EQUAL_INT(0, stale_runs);

    /* Delay expires — task runs and the deadline is consumed */
    mock_tick_advance(20);
    syn_sched_run(&sched);
    TEST_ASSERT_EQUAL_INT(1, stale_runs);
    TEST_ASSERT_EQUAL_UINT32(0, tasks[0].delay_until);

    /* ~25 days later: pre-fix the stale deadline (1010) looked "future"
     * and the task was silently skipped. It must still be scheduled. */
    mock_tick_advance(0x80000000u);
    syn_sched_run(&sched);
    TEST_ASSERT_EQUAL_INT(2, stale_runs);
}

/* Task that runs, delays 5ms, runs again — logs each phase */
static SYN_PT_Status wrap_delay_task(SYN_PT *pt, SYN_Task *task)
{
    PT_BEGIN(pt);
    run_log[run_log_idx++] = 1;
    PT_TASK_DELAY_MS(pt, task, 5);
    run_log[run_log_idx++] = 2;
    PT_END(pt);
}

/**
 * Regression: a delay whose computed deadline lands exactly on tick 0
 * (49.7-day counter wrap) must not alias the scheduler's "no deadline"
 * sentinel. Pre-fix, delay_until == 0 meant the scheduler polled the
 * task every pass instead of skipping it, and tickless mode saw it as
 * "ready now" (no sleep) for the whole delay window.
 */
static void test_delay_deadline_wrap_zero_alias(void)
{
    mock_tick_ms = UINT32_MAX - 4; /* deadline: (UINT32_MAX - 4) + 5 == 0 */
    log_reset();

    SYN_Task tasks[1];
    SYN_Sched sched;
    syn_task_create(&tasks[0], "z", wrap_delay_task, 0, NULL);
    syn_sched_init(&sched, tasks, 1);

    /* Task starts a 5ms delay spanning the wrap — the deadline must be
     * nudged off the sentinel (0 → 1), not disappear */
    syn_sched_run(&sched);
    TEST_ASSERT_EQUAL_INT(1, run_log[0]);
    TEST_ASSERT_EQUAL_UINT32(1, tasks[0].delay_until);

    /* Mid-delay: task is skipped, and the tickless wakeup is the real
     * deadline, not "ready now" */
    mock_tick_advance(2);
    syn_sched_run(&sched);
    TEST_ASSERT_EQUAL_INT(1, run_log_idx);
    TEST_ASSERT_EQUAL_UINT32(1, syn_sched_next_wakeup(&sched));

    /* Past the deadline (tick wrapped to 1): completes */
    mock_tick_advance(4);
    syn_sched_run(&sched);
    TEST_ASSERT_EQUAL_INT(2, run_log[1]);
}

static void test_sched_next_wakeup_blocked_event_fired(void)
{
    SYN_Task task;
    SYN_Sched sched;
    syn_task_create(&task, "evt_task", suspend_task_func, 0, NULL);
    syn_sched_init(&sched, &task, 1);

    SYN_EventFlags flags;
    syn_event_flags_init(&flags);

    /* Set task to BLOCKED state with wait_event and wait_mask */
    task.state = (uint8_t)SYN_TASK_BLOCKED;
    task.wait_event = &flags;
    task.wait_mask = 0x01;
    task.delay_until = syn_port_get_tick_ms() + 1000;

    /* Fire event flags */
    syn_event_flags_set(&flags, 0x01);

    uint32_t now = syn_port_get_tick_ms();
    uint32_t wakeup = syn_sched_next_wakeup(&sched);

    /* Since wait_event fired, next_wakeup returns 'now' (no sleep) */
    TEST_ASSERT_EQUAL_UINT32(now, wakeup);
}

static void test_sched_next_wakeup_uint32_max_and_prio_bounds(void)
{
    SYN_Task task;
    syn_task_create(&task, "dummy", sched_task_a, 0, NULL);
    SYN_Sched sched;
    mock_tick_ms = UINT32_MAX - 500;
    task.delay_until = UINT32_MAX;
    syn_sched_init(&sched, &task, 1);

    /* delay_until == UINT32_MAX target cap branch (line 276) */
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX - 1, syn_sched_next_wakeup(&sched));
    mock_tick_ms = 0;

    /* Round-robin start index clamp branch (line 165) */
    task.delay_until = 0;
    sched.rr_per_prio[0] = 10;
    syn_sched_run(&sched);
}

static SYN_EventFlags test_evt_flags;
static uint32_t test_evt_matched_flags = 0;

static SYN_PT_Status block_evt_timeout_task_func(SYN_PT *pt, SYN_Task *task)
{
    PT_BEGIN(pt);
    PT_BLOCK_EVENT_WITH_TIMEOUT(pt, task, &test_evt_flags, 0x01, 100, &test_evt_matched_flags);
    PT_END(pt);
}

static void test_block_event_with_timeout_event_fired(void)
{
    SYN_Task task;
    SYN_Sched sched;
    mock_tick_ms = 1000;
    syn_event_flags_init(&test_evt_flags);
    syn_task_create(&task, "evt_to_task", block_evt_timeout_task_func, 0, NULL);
    syn_sched_init(&sched, &task, 1);

    /* First pass: enters BLOCKED state with delay_until = 1100 */
    syn_sched_run(&sched);
    TEST_ASSERT_EQUAL_UINT8(SYN_TASK_BLOCKED, task.state);
    TEST_ASSERT_EQUAL_UINT32(1100, task.delay_until);

    /* Fire event at t = 1050 (before timeout 1100) */
    mock_tick_ms = 1050;
    syn_event_flags_set(&test_evt_flags, 0x01);

    /* Second pass: unblocks on event */
    syn_sched_run(&sched);
    TEST_ASSERT_EQUAL_UINT32(0, task.delay_until);
    TEST_ASSERT_NULL(task.wait_event);
    TEST_ASSERT_EQUAL_UINT32(0x01, test_evt_matched_flags);
    /* Matched flag auto-cleared */
    TEST_ASSERT_EQUAL_UINT32(0, syn_event_flags_get(&test_evt_flags));
}

static void test_block_event_with_timeout_expired(void)
{
    SYN_Task task;
    SYN_Sched sched;
    mock_tick_ms = 1000;
    syn_event_flags_init(&test_evt_flags);
    syn_task_create(&task, "evt_to_task", block_evt_timeout_task_func, 0, NULL);
    syn_sched_init(&sched, &task, 1);

    /* First pass: enters BLOCKED state with delay_until = 1100 */
    syn_sched_run(&sched);
    TEST_ASSERT_EQUAL_UINT8(SYN_TASK_BLOCKED, task.state);

    /* Advance time past timeout without firing event */
    mock_tick_ms = 1105;

    /* Second pass: unblocks on timeout */
    syn_sched_run(&sched);
    TEST_ASSERT_EQUAL_UINT32(0, task.delay_until);
    TEST_ASSERT_NULL(task.wait_event);
    TEST_ASSERT_EQUAL_UINT32(0, test_evt_matched_flags);
}

static void test_sched_next_wakeup_blocked_timeout(void)
{
    SYN_Task task;
    SYN_Sched sched;
    mock_tick_ms = 1000;
    syn_event_flags_init(&test_evt_flags);
    syn_task_create(&task, "evt_to_task", block_evt_timeout_task_func, 0, NULL);
    syn_sched_init(&sched, &task, 1);

    /* Enter BLOCKED state with delay_until = 1100 */
    syn_sched_run(&sched);

    /* next_wakeup returns earliest deadline = 1100 */
    uint32_t wake = syn_sched_next_wakeup(&sched);
    TEST_ASSERT_EQUAL_UINT32(1100, wake);

    /* Test UINT32_MAX cap branch for BLOCKED task */
    mock_tick_ms = UINT32_MAX - 500;
    task.delay_until = UINT32_MAX;
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX - 1, syn_sched_next_wakeup(&sched));

    /* Advance time to 1100: next_wakeup returns current tick (now) */
    task.delay_until = 1100;
    mock_tick_ms = 1100;
    wake = syn_sched_next_wakeup(&sched);
    TEST_ASSERT_EQUAL_UINT32(1100, wake);
    mock_tick_ms = 0;
}

static void test_block_event_with_timeout_isr_race_post_timeout(void)
{
    SYN_Task task;
    SYN_Sched sched;
    mock_tick_ms = 1000;
    syn_event_flags_init(&test_evt_flags);
    test_evt_matched_flags = 0xAA; /* sentinel */
    syn_task_create(&task, "evt_to_task", block_evt_timeout_task_func, 0, NULL);
    syn_sched_init(&sched, &task, 1);

    /* First pass: enters BLOCKED state with delay_until = 1100 */
    syn_sched_run(&sched);
    TEST_ASSERT_EQUAL_UINT8(SYN_TASK_BLOCKED, task.state);

    /* Advance time past timeout */
    mock_tick_ms = 1105;

    /* Second pass: unblocks on timeout */
    syn_sched_run(&sched);
    TEST_ASSERT_EQUAL_UINT32(0, test_evt_matched_flags); /* reported timeout (0) */

    /* Now set an event flag to simulate an ISR setting a flag after timeout */
    syn_event_flags_set(&test_evt_flags, 0x01);
    TEST_ASSERT_EQUAL_UINT32(0x01, syn_event_flags_get(&test_evt_flags)); /* flag remains intact */
}

static SYN_PT_Status block_task_two_flags_fn(SYN_PT *pt, SYN_Task *task)
{
    SYN_EventGroup *evt = (SYN_EventGroup *)task->user_data;
    PT_BEGIN(pt);
    PT_BLOCK_EVENT(pt, task, evt, EVT_DATA | EVT_DONE);
    block_counter++;
    PT_END(pt);
}

static void test_block_event_clears_only_matched_flags(void)
{
    mock_tick_ms = 0;
    block_counter = 0;

    SYN_EventGroup evt;
    syn_event_init(&evt);

    SYN_Task tasks[1];
    SYN_Sched sched;
    syn_task_create(&tasks[0], "blk_two", block_task_two_flags_fn, 0, &evt);
    syn_sched_init(&sched, tasks, 1);

    /* Pass 1: task blocks waiting on EVT_DATA | EVT_DONE */
    syn_sched_run(&sched);
    TEST_ASSERT_EQUAL_UINT8(SYN_TASK_BLOCKED, tasks[0].state);

    /* Set only EVT_DATA (0x01) */
    syn_event_set(&evt, EVT_DATA);

    /* Scheduler sees EVT_DATA, sets task->wait_mask = EVT_DATA and marks READY */
    syn_sched_run(&sched);
    TEST_ASSERT_EQUAL_INT(1, block_counter);

    /* EVT_DATA is cleared */
    TEST_ASSERT_FALSE(syn_event_check_any(&evt, EVT_DATA));

    /* Now set EVT_DONE (0x02) to verify it is preserved if set subsequently */
    syn_event_set(&evt, EVT_DONE);
    TEST_ASSERT_TRUE(syn_event_check_any(&evt, EVT_DONE));
}

static void test_sched_rr_uint16_rotation(void)
{
    SYN_Sched sched;
    syn_sched_init(&sched, NULL, 0);

    /* Verify rr_per_prio holds uint16_t without truncation */
    for (size_t i = 0; i < SYN_SCHED_PRIO_LEVELS; i++) {
        sched.rr_per_prio[i] = (uint16_t)(300 + i);
        TEST_ASSERT_EQUAL_UINT16((uint16_t)(300 + i), sched.rr_per_prio[i]);
    }
}

void run_sched_tests(void)
{
    RUN_TEST(test_scheduler);
    RUN_TEST(test_suspend_resume);
    RUN_TEST(test_sched_empty);
    RUN_TEST(test_sched_delayed_task);
    RUN_TEST(test_sched_alive_count);
    RUN_TEST(test_sched_run_forever);
    RUN_TEST(test_defer_basic);
    RUN_TEST(test_defer_rr_fairness);
    RUN_TEST(test_rr_same_priority);
    RUN_TEST(test_strict_priority_no_defer);
    RUN_TEST(test_defer_rr_three_lower);
    RUN_TEST(test_defer_state_lifecycle);
    RUN_TEST(test_defer_then_suspend);
    RUN_TEST(test_block_event_basic);
    RUN_TEST(test_block_event_skips_scan);
    RUN_TEST(test_block_event_auto_clear);
    RUN_TEST(test_block_event_priority);
    RUN_TEST(test_block_event_multiple_tasks);
    RUN_TEST(test_block_event_suspend);
    RUN_TEST(test_waiting_doesnt_starve);
    RUN_TEST(test_waiting_with_defer_no_inversion);
    RUN_TEST(test_all_tasks_waiting);
    RUN_TEST(test_waiting_state_lifecycle);
    RUN_TEST(test_waiting_same_priority_fairness);
    RUN_TEST(test_waiting_delayed_no_inversion);
    RUN_TEST(test_task_priority_boost_and_restore);
    RUN_TEST(test_two_deferring_high_pri_tasks_starve_lower);
    RUN_TEST(test_pt_delay_us);
    RUN_TEST(test_block_condition_and_primitives);
    RUN_TEST(test_block_condition_isr_wakeup_not_lost);
    RUN_TEST(test_block_event_auto_clear_is_atomic);
    RUN_TEST(test_block_event_clears_only_matched_flags);
    RUN_TEST(test_delay_deadline_consumed_no_wrap_freeze);
    RUN_TEST(test_delay_deadline_wrap_zero_alias);
    RUN_TEST(test_sched_next_wakeup_blocked_event_fired);
    RUN_TEST(test_sched_next_wakeup_uint32_max_and_prio_bounds);
    RUN_TEST(test_block_event_with_timeout_event_fired);
    RUN_TEST(test_block_event_with_timeout_expired);
    RUN_TEST(test_sched_next_wakeup_blocked_timeout);
    RUN_TEST(test_block_event_with_timeout_isr_race_post_timeout);
    RUN_TEST(test_sched_rr_uint16_rotation);
}
