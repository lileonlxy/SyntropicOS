/**
 * @file test_mutex.c
 * @brief Unity tests for priority-inheriting mutex (syn_mutex).
 */

#include "mocks/mock_port.h"
#include "syntropic/sched/syn_mutex.h"
#include "syntropic/sched/syn_sched.h"
#include "syntropic/syntropic.h"
#include "unity/unity.h"

static SYN_Mutex g_test_mutex;
static int g_exec_order[8];
static int g_exec_count = 0;

static SYN_PT_Status dummy_pt_fn(SYN_PT *pt, SYN_Task *task)
{
    (void)task;
    PT_BEGIN(pt);
    PT_END(pt);
}

static void test_mutex_init_and_query(void)
{
    SYN_Mutex m;

    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_mutex_init(NULL));
    TEST_ASSERT_EQUAL(SYN_OK, syn_mutex_init(&m));
    TEST_ASSERT_FALSE(syn_mutex_is_locked(&m));
    TEST_ASSERT_NULL(syn_mutex_get_owner(&m));

    TEST_ASSERT_FALSE(syn_mutex_is_locked(NULL));
    TEST_ASSERT_NULL(syn_mutex_get_owner(NULL));
}

static void test_mutex_basic_trylock_and_unlock(void)
{
    SYN_Mutex m;
    SYN_Task t1, t2;

    syn_task_create(&t1, "t1", dummy_pt_fn, 1, NULL);
    syn_task_create(&t2, "t2", dummy_pt_fn, 2, NULL);
    TEST_ASSERT_EQUAL(SYN_OK, syn_mutex_init(&m));

    /* NULL parameters */
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_mutex_try_lock(NULL, &t1));
    TEST_ASSERT_EQUAL(SYN_INVALID_PARAM, syn_mutex_unlock(NULL, &t1));

    /* Acquire by t1 */
    TEST_ASSERT_EQUAL(SYN_OK, syn_mutex_try_lock(&m, &t1));
    TEST_ASSERT_TRUE(syn_mutex_is_locked(&m));
    TEST_ASSERT_EQUAL_PTR(&t1, syn_mutex_get_owner(&m));

    /* Attempt by t2 should fail (BUSY) */
    TEST_ASSERT_EQUAL(SYN_BUSY, syn_mutex_try_lock(&m, &t2));

    /* Unlock by wrong task should fail */
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_mutex_unlock(&m, &t2));
    TEST_ASSERT_TRUE(syn_mutex_is_locked(&m));

    /* Unlock by owner succeeds */
    TEST_ASSERT_EQUAL(SYN_OK, syn_mutex_unlock(&m, &t1));
    TEST_ASSERT_FALSE(syn_mutex_is_locked(&m));
    TEST_ASSERT_NULL(syn_mutex_get_owner(&m));

    /* Unlocking when already unlocked returns error */
    TEST_ASSERT_EQUAL(SYN_ERROR, syn_mutex_unlock(&m, &t1));
}

static void test_mutex_standalone_lock_unlock(void)
{
    SYN_Mutex m;
    TEST_ASSERT_EQUAL(SYN_OK, syn_mutex_init(&m));

    /* Standalone lock with task == NULL */
    TEST_ASSERT_EQUAL(SYN_OK, syn_mutex_try_lock(&m, NULL));
    TEST_ASSERT_TRUE(syn_mutex_is_locked(&m));
    TEST_ASSERT_NULL(syn_mutex_get_owner(&m));

    /* Standalone unlock */
    TEST_ASSERT_EQUAL(SYN_OK, syn_mutex_unlock(&m, NULL));
    TEST_ASSERT_FALSE(syn_mutex_is_locked(&m));
}

static void test_mutex_recursive_locking(void)
{
    SYN_Mutex m;
    SYN_Task t;

    syn_task_create(&t, "rec_task", dummy_pt_fn, 0, NULL);
    TEST_ASSERT_EQUAL(SYN_OK, syn_mutex_init(&m));

    /* Acquire 3 times */
    TEST_ASSERT_EQUAL(SYN_OK, syn_mutex_try_lock(&m, &t));
    TEST_ASSERT_EQUAL_UINT16(1, m.lock_count);

    TEST_ASSERT_EQUAL(SYN_OK, syn_mutex_try_lock(&m, &t));
    TEST_ASSERT_EQUAL_UINT16(2, m.lock_count);

    TEST_ASSERT_EQUAL(SYN_OK, syn_mutex_try_lock(&m, &t));
    TEST_ASSERT_EQUAL_UINT16(3, m.lock_count);

    /* Release depth 1 and 2: still locked */
    TEST_ASSERT_EQUAL(SYN_OK, syn_mutex_unlock(&m, &t));
    TEST_ASSERT_TRUE(syn_mutex_is_locked(&m));
    TEST_ASSERT_EQUAL_UINT16(2, m.lock_count);

    TEST_ASSERT_EQUAL(SYN_OK, syn_mutex_unlock(&m, &t));
    TEST_ASSERT_TRUE(syn_mutex_is_locked(&m));
    TEST_ASSERT_EQUAL_UINT16(1, m.lock_count);

    /* Release final depth: unlocked */
    TEST_ASSERT_EQUAL(SYN_OK, syn_mutex_unlock(&m, &t));
    TEST_ASSERT_FALSE(syn_mutex_is_locked(&m));
    TEST_ASSERT_EQUAL_UINT16(0, m.lock_count);
    TEST_ASSERT_NULL(syn_mutex_get_owner(&m));
}

static void test_mutex_priority_inheritance(void)
{
    SYN_Mutex m;
    SYN_Task task_l, task_m, task_h;

    syn_task_create(&task_l, "low", dummy_pt_fn, 3, NULL);
    syn_task_create(&task_m, "med", dummy_pt_fn, 1, NULL);
    syn_task_create(&task_h, "high", dummy_pt_fn, 0, NULL);

    TEST_ASSERT_EQUAL(SYN_OK, syn_mutex_init(&m));

    /* Task L acquires mutex */
    TEST_ASSERT_EQUAL(SYN_OK, syn_mutex_try_lock(&m, &task_l));
    TEST_ASSERT_EQUAL_UINT8(3, task_l.priority);

    /* Task M (prio 1) attempts acquisition — fails and does not boost Task L */
    TEST_ASSERT_EQUAL(SYN_BUSY, syn_mutex_try_lock(&m, &task_m));
    TEST_ASSERT_EQUAL_UINT8(1, task_l.priority); /* Boosted to 1 by Task M */

    /* Task H (prio 0) attempts acquisition — fails and boosts Task L to 0 */
    TEST_ASSERT_EQUAL(SYN_BUSY, syn_mutex_try_lock(&m, &task_h));
    TEST_ASSERT_EQUAL_UINT8(0, task_l.priority); /* Boosted to 0 by Task H */

    /* Task L unlocks mutex — priority restored to base priority 3 */
    TEST_ASSERT_EQUAL(SYN_OK, syn_mutex_unlock(&m, &task_l));
    TEST_ASSERT_EQUAL_UINT8(3, task_l.priority);
    TEST_ASSERT_FALSE(syn_mutex_is_locked(&m));

    /* Task H now successfully acquires */
    TEST_ASSERT_EQUAL(SYN_OK, syn_mutex_try_lock(&m, &task_h));
    TEST_ASSERT_EQUAL_PTR(&task_h, syn_mutex_get_owner(&m));
    TEST_ASSERT_EQUAL(SYN_OK, syn_mutex_unlock(&m, &task_h));
}

/* ── Cooperative Multitasking Scheduler Integration Tests ──────────────── */

static volatile int g_low_has_lock = 0;

static SYN_PT_Status pt_task_low_inversion(SYN_PT *pt, SYN_Task *task)
{
    PT_BEGIN(pt);

    /* Low acquires mutex immediately */
    PT_MUTEX_LOCK(pt, task, &g_test_mutex);
    g_exec_order[g_exec_count++] = 10;
    g_low_has_lock = 1;
    PT_YIELD(pt);

    /* After yield, High will have tried to lock and boosted Low to prio 0 */
    TEST_ASSERT_EQUAL_UINT8(0, task->priority);
    g_exec_order[g_exec_count++] = 11;

    PT_MUTEX_UNLOCK(&g_test_mutex, task);
    TEST_ASSERT_EQUAL_UINT8(3, task->priority); /* Restored */
    g_exec_order[g_exec_count++] = 12;

    PT_END(pt);
}

static SYN_PT_Status pt_task_med(SYN_PT *pt, SYN_Task *task)
{
    PT_BEGIN(pt);

    /* Defer on initial pass to allow Low to acquire mutex */
    PT_DEFER(pt, task);

    /* Medium-priority task doing work */
    g_exec_order[g_exec_count++] = 20;
    PT_YIELD(pt);
    g_exec_order[g_exec_count++] = 21;

    PT_END(pt);
}

static SYN_PT_Status pt_task_high_inversion(SYN_PT *pt, SYN_Task *task)
{
    PT_BEGIN(pt);

    /* High waits for Low to acquire first */
    PT_WAIT_UNTIL(pt, g_low_has_lock == 1);

    /* Now High attempts to lock — gets blocked and boosts Low */
    PT_MUTEX_LOCK(pt, task, &g_test_mutex);
    g_exec_order[g_exec_count++] = 30;

    PT_MUTEX_UNLOCK(&g_test_mutex, task);
    g_exec_order[g_exec_count++] = 31;

    PT_END(pt);
}

static void test_mutex_priority_inversion_prevention(void)
{
    SYN_Sched sched;
    SYN_Task tasks[3];

    g_exec_count = 0;
    g_low_has_lock = 0;
    PT_MUTEX_INIT(&g_test_mutex);

    syn_task_create(&tasks[0], "high", pt_task_high_inversion, 0, NULL);
    syn_task_create(&tasks[1], "med", pt_task_med, 1, NULL);
    syn_task_create(&tasks[2], "low", pt_task_low_inversion, 3, NULL);

    syn_sched_init(&sched, tasks, 3);

    /* Run scheduler until all tasks finish */
    int safety = 0;
    while (syn_sched_run(&sched) && safety++ < 20)
        ;

    /* Verify order:
     * 1. Low locks mutex (10).
     * 2. High attempts to lock, gets blocked (WAITING) and boosts Low to prio 0.
     * 3. Scheduler re-scans: Low is prio 0, Med is prio 1. Low RUNS before Med (11, 12)!
     * 4. High acquires mutex, runs (30, 31).
     * 5. Med runs (20, 21).
     */
    TEST_ASSERT_EQUAL_INT(7, g_exec_count);
    TEST_ASSERT_EQUAL_INT(10, g_exec_order[0]); /* Low lock */
    TEST_ASSERT_EQUAL_INT(11, g_exec_order[1]); /* Low boosted run before Med */
    TEST_ASSERT_EQUAL_INT(12, g_exec_order[2]); /* Low unlock & restore */
    TEST_ASSERT_EQUAL_INT(30, g_exec_order[3]); /* High acquire */
    TEST_ASSERT_EQUAL_INT(31, g_exec_order[4]); /* High unlock */
    TEST_ASSERT_EQUAL_INT(20, g_exec_order[5]); /* Med runs part 1 */
    TEST_ASSERT_EQUAL_INT(21, g_exec_order[6]); /* Med runs part 2 */
}

static void test_mutex_trylock_macro(void)
{
    SYN_Mutex m;
    SYN_Task t;

    syn_task_create(&t, "test", dummy_pt_fn, 0, NULL);
    PT_MUTEX_INIT(&m);

    TEST_ASSERT_TRUE(PT_MUTEX_TRYLOCK(&m, &t));
    TEST_ASSERT_TRUE(syn_mutex_is_locked(&m));
    TEST_ASSERT_EQUAL(SYN_OK, PT_MUTEX_UNLOCK(&m, &t));
    TEST_ASSERT_FALSE(syn_mutex_is_locked(&m));
}

void run_mutex_tests(void)
{
    RUN_TEST(test_mutex_init_and_query);
    RUN_TEST(test_mutex_basic_trylock_and_unlock);
    RUN_TEST(test_mutex_standalone_lock_unlock);
    RUN_TEST(test_mutex_recursive_locking);
    RUN_TEST(test_mutex_priority_inheritance);
    RUN_TEST(test_mutex_priority_inversion_prevention);
    RUN_TEST(test_mutex_trylock_macro);
}
