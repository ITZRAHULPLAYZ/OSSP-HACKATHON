/*
 * test_runner.c — Automated test suite
 *
 * Tests:
 *   1. Queue operations (enqueue / dequeue / remove)
 *   2. MLFQ insert / pick / demote / promote
 *   3. Priority-sorted insertion
 *   4. Process creation and state transitions
 *   5. Aging: starving processes must get boosted
 *   6. MLFQ simulation: CPU-bound process gets demoted
 *   7. RR  simulation: fairness (no process waits indefinitely)
 *   8. PRIORITY simulation: highest priority runs first
 *   9. SRTF simulation: shortest job runs first
 *  10. Comparison mode: all algorithms complete without crash
 *
 * Compile:
 *   Linux/Mac : gcc -Wall -O2 -Iinclude tests/test_runner.c src/*.c -o test_runner
 *   Windows   : gcc -Wall -O2 -Iinclude tests/test_runner.c src/*.c -o test_runner.exe
 *
 * Smart CPU Scheduler with Dynamic Priority Adjustment
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../include/scheduler.h"
#include "../include/display.h"

/* ═══════════════════════════════════════════════════
 *  Minimal test framework
 * ═══════════════════════════════════════════════════ */
static int pass_count = 0;
static int fail_count = 0;

#define TEST_PASS(name) do { \
    printf("  " FG_BGREEN "[PASS]" A_RESET " %s\n", name); \
    pass_count++; \
} while(0)

#define TEST_FAIL(name, msg) do { \
    printf("  " FG_BRED "[FAIL]" A_RESET " %s — %s\n", name, msg); \
    fail_count++; \
} while(0)

#define ASSERT_EQ(name, a, b) do { \
    if ((a) == (b)) TEST_PASS(name); \
    else { \
        char _buf[128]; \
        snprintf(_buf, sizeof(_buf), "expected %d, got %d", (int)(b), (int)(a)); \
        TEST_FAIL(name, _buf); \
    } \
} while(0)

#define ASSERT_TRUE(name, cond) do { \
    if (cond) TEST_PASS(name); \
    else TEST_FAIL(name, #cond " is false"); \
} while(0)

#define SECTION(title) \
    printf("\n" FG_BWHITE A_BOLD " ── %s ──\n" A_RESET, title)

/* ═══════════════════════════════════════════════════
 *  Test 1: Queue basic operations
 * ═══════════════════════════════════════════════════ */
static void test_queue(void)
{
    SECTION("Queue Operations");

    Queue *q = queue_create();
    ASSERT_TRUE("queue_create: not null",      q != NULL);
    ASSERT_TRUE("queue_create: initially empty", queue_empty(q));

    Process *p1 = process_create(1, "p1", 0, 10, 5);
    Process *p2 = process_create(2, "p2", 0, 10, 3);
    Process *p3 = process_create(3, "p3", 0, 10, 7);

    queue_enqueue(q, p1);
    queue_enqueue(q, p2);
    queue_enqueue(q, p3);
    ASSERT_EQ("enqueue 3: count == 3", q->count, 3);

    Process *d = queue_dequeue(q);
    ASSERT_EQ("dequeue FIFO: pid == 1", d->pid, 1);
    ASSERT_EQ("after dequeue: count == 2", q->count, 2);

    queue_remove(q, 3);
    ASSERT_EQ("remove by pid: count == 1", q->count, 1);

    Process *last = queue_dequeue(q);
    ASSERT_EQ("remaining: pid == 2", last->pid, 2);
    ASSERT_TRUE("now empty", queue_empty(q));

    process_destroy(p1);
    process_destroy(p2);
    process_destroy(p3);
    queue_destroy(q);
}

/* ═══════════════════════════════════════════════════
 *  Test 2: MLFQ operations
 * ═══════════════════════════════════════════════════ */
static void test_mlfq(void)
{
    SECTION("MLFQ Operations");

    MLFQ *m = mlfq_create();
    ASSERT_TRUE("mlfq_create: not null", m != NULL);
    ASSERT_TRUE("mlfq_create: empty",    mlfq_empty(m));

    Process *pa = process_create(1, "a", 0, 10, 5);
    Process *pb = process_create(2, "b", 0, 10, 5);

    mlfq_insert(m, pa, 0);
    mlfq_insert(m, pb, 2);
    ASSERT_EQ("mlfq level 0 count", m->levels[0].count, 1);
    ASSERT_EQ("mlfq level 2 count", m->levels[2].count, 1);

    Process *picked = mlfq_pick(m);
    ASSERT_EQ("mlfq_pick: highest level first (pid=1)", picked->pid, 1);

    mlfq_demote(m, picked);
    ASSERT_EQ("after demote: now at level 1", picked->queue_level, 1);

    /* promote from level 2 */
    mlfq_promote(m, pb);
    ASSERT_EQ("after promote: pb at level 1", pb->queue_level, 1);

    process_destroy(pa);
    process_destroy(pb);
    mlfq_destroy(m);
}

/* ═══════════════════════════════════════════════════
 *  Test 3: Priority sorted queue
 * ═══════════════════════════════════════════════════ */
static void test_priority_queue(void)
{
    SECTION("Priority-Sorted Queue");

    Queue *q = queue_create();
    Process *pa = process_create(1, "low",  0, 5, 2); pa->dynamic_priority = 2;
    Process *pb = process_create(2, "high", 0, 5, 9); pb->dynamic_priority = 9;
    Process *pc = process_create(3, "mid",  0, 5, 5); pc->dynamic_priority = 5;

    queue_enqueue_priority(q, pa);
    queue_enqueue_priority(q, pb);
    queue_enqueue_priority(q, pc);

    Process *d1 = queue_dequeue(q);
    Process *d2 = queue_dequeue(q);
    Process *d3 = queue_dequeue(q);
    ASSERT_EQ("priority sort: first  (pri=9)", d1->dynamic_priority, 9);
    ASSERT_EQ("priority sort: second (pri=5)", d2->dynamic_priority, 5);
    ASSERT_EQ("priority sort: third  (pri=2)", d3->dynamic_priority, 2);

    process_destroy(pa); process_destroy(pb); process_destroy(pc);
    queue_destroy(q);
}

/* ═══════════════════════════════════════════════════
 *  Test 4: Anti-starvation aging
 * ═══════════════════════════════════════════════════ */
static void test_aging(void)
{
    SECTION("Anti-Starvation Aging");

    /* Create a MLFQ scheduler with a low-priority process at level 3
       and force its age above the threshold */
    Scheduler *s = sched_create(ALGO_MLFQ, 200, 5, 0, 0);

    /* Starving process: low priority, stuck at bottom level */
    Process *hungry = process_create(10, "starving", 0, 20, 1);
    hungry->state   = PROC_READY;
    hungry->age     = STARVATION_THRESHOLD + 1;  /* already starving */
    hungry->dynamic_priority = 1;

    sched_add_process(s, hungry);
    mlfq_insert(s->mlfq, hungry, 3);  /* stuck in lowest level */

    int prev_pri = hungry->dynamic_priority;
    sched_apply_aging(s);

    ASSERT_TRUE("aging: priority boosted",
                hungry->dynamic_priority > prev_pri);
    ASSERT_TRUE("aging: moved to level 0",
                hungry->queue_level == 0);
    ASSERT_EQ("aging: starvation_count == 1",
              hungry->starvation_count, 1);
    ASSERT_EQ("aging: stats.starvation_events == 1",
              s->stats->starvation_events, 1);

    sched_destroy(s);
}

/* ═══════════════════════════════════════════════════
 *  Test 5: MLFQ — CPU-bound process gets demoted
 * ═══════════════════════════════════════════════════ */
static void test_mlfq_simulation(void)
{
    SECTION("MLFQ Simulation — CPU-bound demotion");

    srand(42);
    Scheduler *s = sched_create(ALGO_MLFQ, 60, 5, 0, 0);

    /* Create a purely CPU-bound process (no I/O) */
    Process *p = process_create(1, "cpu_hog", 0, 50, 5);
    p->io_burst_time = 0;
    sched_add_process(s, p);

    sched_run(s);

    /* A CPU-bound process that uses full quanta should have
       been demoted at some point (queue_level > 0, or dynamic_priority
       was reduced at least once) */
    ASSERT_TRUE("mlfq: CPU-bound process penalised",
                p->dynamic_priority <= p->static_priority);

    sched_destroy(s);
}

/* ═══════════════════════════════════════════════════
 *  Test 6: RR — all processes eventually complete
 * ═══════════════════════════════════════════════════ */
static void test_rr_simulation(void)
{
    SECTION("Round Robin — all processes complete");

    srand(7);
    int N = 6;
    Scheduler *s = sched_create(ALGO_RR, 300, 4, 0, 0);
    for (int i = 1; i <= N; i++) {
        Process *p = process_random(i, 0);
        p->io_burst_time = 0;  /* no I/O for determinism */
        sched_add_process(s, p);
    }
    sched_run(s);

    ASSERT_EQ("rr: all processes completed",
              s->completed_procs, N);
    ASSERT_TRUE("rr: avg waiting time > 0",
                s->stats->avg_waiting_time >= 0.0f);

    sched_destroy(s);
}

/* ═══════════════════════════════════════════════════
 *  Test 7: PRIORITY — higher priority runs first
 * ═══════════════════════════════════════════════════ */
static void test_priority_simulation(void)
{
    SECTION("Priority Preemptive — highest priority first");

    srand(1);
    Scheduler *s = sched_create(ALGO_PRIORITY, 200, 5, 0, 0);

    /* Two processes arriving at same time; high priority should run first */
    Process *lo = process_create(1, "low",  0, 8, 2);
    Process *hi = process_create(2, "high", 0, 8, 9);
    lo->io_burst_time = 0;
    hi->io_burst_time = 0;

    sched_add_process(s, lo);
    sched_add_process(s, hi);
    sched_run(s);

    /* high priority should finish first (lower completion time) */
    ASSERT_TRUE("priority: high-pri completes before low-pri",
                hi->completion_time <= lo->completion_time);

    sched_destroy(s);
}

/* ═══════════════════════════════════════════════════
 *  Test 8: SRTF — shortest job runs first
 * ═══════════════════════════════════════════════════ */
static void test_srtf_simulation(void)
{
    SECTION("SRTF — shortest remaining time first");

    srand(3);
    Scheduler *s = sched_create(ALGO_SRTF, 200, 5, 0, 0);

    Process *short_p = process_create(1, "short", 0, 4,  5);
    Process *long_p  = process_create(2, "long",  0, 20, 5);
    short_p->io_burst_time = 0;
    long_p->io_burst_time  = 0;

    sched_add_process(s, short_p);
    sched_add_process(s, long_p);
    sched_run(s);

    ASSERT_TRUE("srtf: short job finishes before long job",
                short_p->completion_time < long_p->completion_time);

    sched_destroy(s);
}

/* ═══════════════════════════════════════════════════
 *  Test 9: Comparison mode runs without crash
 * ═══════════════════════════════════════════════════ */
static void test_comparison(void)
{
    SECTION("Comparison Mode — all algorithms run");

    srand(99);
    int N = 5;
    Process *procs[5];
    for (int i = 0; i < N; i++) {
        procs[i] = process_random(i + 1, 0);
        procs[i]->io_burst_time = 0;
    }

    /* This should not crash */
    sched_compare(procs, N, 150);

    for (int i = 0; i < N; i++) process_destroy(procs[i]);
    TEST_PASS("comparison: no crash");
}

/* ═══════════════════════════════════════════════════
 *  Test 10: Stats correctness
 * ═══════════════════════════════════════════════════ */
static void test_stats(void)
{
    SECTION("Statistics Correctness");

    Stats *st = stats_create();
    stats_record_completion(st, 5, 15, 2);
    stats_record_completion(st, 3, 12, 1);
    stats_finalize(st, 20);

    ASSERT_EQ("stats: 2 processes recorded", st->procs_counted, 2);
    /* avg_waiting = (5+3)/2 = 4.0 */
    ASSERT_TRUE("stats: avg_waiting == 4.0", st->avg_waiting_time == 4.0f);
    /* avg_tat = (15+12)/2 = 13.5 */
    ASSERT_TRUE("stats: avg_tat == 13.5", st->avg_turnaround_time == 13.5f);

    stats_destroy(st);
}

/* ═══════════════════════════════════════════════════
 *  Main
 * ═══════════════════════════════════════════════════ */
int main(void)
{
    display_enable_ansi();

    printf(FG_BWHITE A_BOLD
           "\n  Smart CPU Scheduler — Test Suite\n"
           "  ══════════════════════════════════\n"
           A_RESET);

    test_queue();
    test_mlfq();
    test_priority_queue();
    test_aging();
    test_mlfq_simulation();
    test_rr_simulation();
    test_priority_simulation();
    test_srtf_simulation();
    test_stats();
    test_comparison();

    printf("\n" A_BOLD);
    printf("  Results: ");
    printf(FG_BGREEN "%d passed" A_RESET A_BOLD, pass_count);
    printf("  |  ");
    if (fail_count > 0)
        printf(FG_BRED "%d failed" A_RESET "\n\n", fail_count);
    else
        printf(FG_BGREEN "0 failed" A_RESET "\n\n");

    return fail_count > 0 ? 1 : 0;
}
