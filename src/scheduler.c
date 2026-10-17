/*
 * scheduler.c — Scheduling algorithms + dynamic priority engine
 *
 * Implements:
 *   • MLFQ  — Multi-Level Feedback Queue (4 levels, quanta 4/8/16/32)
 *   • RR    — Round Robin with configurable quantum
 *   • PRI   — Preemptive Priority (always-highest dynamic priority)
 *   • SRTF  — Shortest Remaining Time First with starvation guard
 *
 * Dynamic Priority Features:
 *   • Anti-starvation AGING — processes waiting > STARVATION_THRESHOLD ticks
 *     get a +AGE_BOOST to dynamic_priority and a queue-level promotion.
 *   • CPU-bound PENALTY     — full-quantum user gets -1 priority.
 *   • I/O-bound REWARD      — early-yield gets +1 priority.
 *   • MLFQ DEMOTION/PROMOTION — CPU hogs move down; I/O and aging move up.
 *   • Adaptive quantum (MLFQ) — I/O-bound processes get half-quantum.
 *
 * Smart CPU Scheduler with Dynamic Priority Adjustment
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  include <windows.h>
#  define SLEEP_MS(ms) Sleep((DWORD)(ms))
#else
#  include <unistd.h>
#  define SLEEP_MS(ms) usleep((ms) * 1000u)
#endif

#include "../include/scheduler.h"
#include "../include/display.h"

/* ═══════════════════════════════════════════════════
 *  Lifecycle
 * ═══════════════════════════════════════════════════ */

Scheduler *sched_create(Algorithm algo, int max_ticks, int rr_quantum,
                         int speed_ms, int animate)
{
    Scheduler *s = (Scheduler *)calloc(1, sizeof(Scheduler));
    if (!s) { perror("sched_create"); exit(1); }

    s->algorithm  = algo;
    s->max_ticks  = max_ticks < GANTT_MAX ? max_ticks : GANTT_MAX - 1;
    s->rr_quantum = rr_quantum > 0 ? rr_quantum : DEFAULT_RR_QUANTUM;
    s->speed_ms   = speed_ms;
    s->animate    = animate;
    s->prev_pid   = -2;

    s->mlfq    = mlfq_create();
    s->ready_q = queue_create();
    s->io_q    = queue_create();
    s->stats   = stats_create();

    /* Initialise Gantt log to -1 (idle) */
    memset(s->gantt, -1, sizeof(s->gantt));
    return s;
}

void sched_destroy(Scheduler *s)
{
    if (!s) return;
    mlfq_destroy(s->mlfq);
    queue_destroy(s->ready_q);
    queue_destroy(s->io_q);
    stats_destroy(s->stats);
    free(s);
}

void sched_add_process(Scheduler *s, Process *p)
{
    if (s->total_procs >= MAX_PROCESSES) {
        fprintf(stderr, "Max processes reached\n");
        return;
    }
    s->all_procs[s->total_procs++] = p;
}

/* ═══════════════════════════════════════════════════
 *  Dynamic Priority Helpers
 * ═══════════════════════════════════════════════════ */

void sched_priority_clamp(Process *p)
{
    if (p->dynamic_priority > MAX_PRIORITY) p->dynamic_priority = MAX_PRIORITY;
    if (p->dynamic_priority < MIN_PRIORITY) p->dynamic_priority = MIN_PRIORITY;
}

/* Called when a process exhausted its quantum (CPU-bound behaviour) */
void sched_priority_cpu_penalty(Process *p)
{
    p->dynamic_priority -= CPU_BOUND_PENALTY;
    sched_priority_clamp(p);
}

/* Called when a process voluntarily yields (I/O burst before quantum) */
void sched_priority_io_reward(Process *p)
{
    p->dynamic_priority += IO_BOUND_REWARD;
    sched_priority_clamp(p);
}

/* Mild decay to prevent a boosted process from monopolising the CPU */
void sched_priority_decay(Process *p)
{
    if (p->cpu_ticks_used > 0 && p->cpu_ticks_used % PRIORITY_DECAY_TICKS == 0) {
        if (p->dynamic_priority > p->static_priority) {
            p->dynamic_priority--;
        }
    }
}

/* ═══════════════════════════════════════════════════
 *  Anti-Starvation Aging
 * ═══════════════════════════════════════════════════ */

void sched_apply_aging(Scheduler *s)
{
    if (s->algorithm == ALGO_MLFQ) {
        /* Scan every queue level below 0 */
        for (int lvl = 1; lvl < MLFQ_LEVELS; lvl++) {
            Process *p    = s->mlfq->levels[lvl].head;
            Process *prev = NULL;
            while (p) {
                Process *nxt = p->next;
                if (p->age >= STARVATION_THRESHOLD) {
                    /* Unlink from current level */
                    if (prev) prev->next  = nxt;
                    else      s->mlfq->levels[lvl].head = nxt;
                    if (!nxt) s->mlfq->levels[lvl].tail = prev;
                    p->next = NULL;
                    s->mlfq->levels[lvl].count--;

                    /* Boost priority and promote */
                    p->dynamic_priority += AGE_BOOST;
                    sched_priority_clamp(p);
                    p->starvation_count++;
                    p->age = 0;
                    mlfq_insert(s->mlfq, p, 0);  /* force to highest level */

                    s->stats->starvation_events++;
                    s->stats->aging_promotions++;
                } else {
                    prev = p;
                }
                p = nxt;
            }
        }
    } else {
        /* For RR / Priority / SRTF: scan the ready queue */
        Process *p = s->ready_q->head;
        while (p) {
            if (p->age >= STARVATION_THRESHOLD) {
                p->dynamic_priority += AGE_BOOST;
                sched_priority_clamp(p);
                p->starvation_count++;
                p->age = 0;
                s->stats->starvation_events++;
                s->stats->aging_promotions++;
            }
            p = p->next;
        }
        /* Re-sort if PRIORITY algo (aging may have changed order) */
        if (s->algorithm == ALGO_PRIORITY && !queue_empty(s->ready_q)) {
            /* Re-insert all into a fresh sorted queue */
            Queue *tmp = queue_create();
            Process *pp;
            while ((pp = queue_dequeue(s->ready_q)) != NULL)
                queue_enqueue_priority(tmp, pp);
            queue_destroy(s->ready_q);
            s->ready_q = tmp;
        }
    }
}

/* ═══════════════════════════════════════════════════
 *  Process Event Handlers
 * ═══════════════════════════════════════════════════ */

/* Move newly arrived processes into the appropriate ready structure */
void sched_handle_arrivals(Scheduler *s)
{
    for (int i = 0; i < s->total_procs; i++) {
        Process *p = s->all_procs[i];
        if (p->state == PROC_NOT_ARRIVED && p->arrival_time <= s->tick) {
            p->state = PROC_READY;
            if (s->algorithm == ALGO_MLFQ)
                mlfq_insert(s->mlfq, p, 0);
            else if (s->algorithm == ALGO_PRIORITY)
                queue_enqueue_priority(s->ready_q, p);
            else
                queue_enqueue(s->ready_q, p);
        }
    }
}

/* Tick down I/O timers; move finished I/O processes back to ready */
void sched_handle_io(Scheduler *s)
{
    Process *p    = s->io_q->head;
    Process *prev = NULL;
    while (p) {
        Process *nxt = p->next;
        p->io_remaining--;
        p->io_ticks_used++;

        if (p->io_remaining <= 0) {
            /* Unlink from io_q */
            if (prev) prev->next = nxt;
            else      s->io_q->head = nxt;
            if (!nxt) s->io_q->tail = prev;
            p->next = NULL;
            s->io_q->count--;

            /* Return to ready queue with I/O reward */
            p->state = PROC_READY;
            sched_priority_io_reward(p);
            process_classify(p);

            if (s->algorithm == ALGO_MLFQ)
                mlfq_promote(s->mlfq, p);   /* reward I/O-bound with promotion */
            else if (s->algorithm == ALGO_PRIORITY)
                queue_enqueue_priority(s->ready_q, p);
            else
                queue_enqueue(s->ready_q, p);
        } else {
            prev = p;
        }
        p = nxt;
    }
}

/* Called when a running process exhausts its burst time */
void sched_handle_completion(Scheduler *s, Process *p)
{
    p->state            = PROC_DONE;
    p->completion_time  = s->tick;
    p->turnaround_time  = p->completion_time - p->arrival_time;
    p->response_time    = p->first_run_tick  - p->arrival_time;
    /* waiting = turnaround - (cpu burst + i/o time) */
    p->waiting_time     = p->turnaround_time - p->burst_time - p->io_ticks_used;
    if (p->waiting_time < 0) p->waiting_time = 0;

    process_classify(p);
    stats_record_completion(s->stats,
                            p->waiting_time,
                            p->turnaround_time,
                            p->response_time);
    s->completed_procs++;
    s->running = NULL;
}

/* Initiate an I/O burst for a process mid-execution */
void sched_handle_io_trigger(Scheduler *s, Process *p)
{
    p->state        = PROC_WAITING;
    p->io_remaining = p->io_burst_time;
    p->io_triggered = 1;
    p->quantum_used = 0;

    /* Move to I/O queue */
    queue_enqueue(s->io_q, p);
    s->running = NULL;
}

/* Preemption: put current running process back in the ready structure */
void sched_handle_preemption(Scheduler *s)
{
    Process *p = s->running;
    if (!p) return;

    /* Did it use the full quantum? → CPU-bound penalty */
    if (p->quantum_used >= s->mlfq->quanta[p->queue_level])
        sched_priority_cpu_penalty(p);

    p->state        = PROC_READY;
    p->quantum_used = 0;
    s->stats->preemptions++;

    if (s->algorithm == ALGO_MLFQ)
        mlfq_demote(s->mlfq, p);               /* CPU hog moves down */
    else if (s->algorithm == ALGO_PRIORITY)
        queue_enqueue_priority(s->ready_q, p);
    else
        queue_enqueue(s->ready_q, p);

    s->running = NULL;
}

/* ═══════════════════════════════════════════════════
 *  Algorithm-Specific Process Selectors
 * ═══════════════════════════════════════════════════ */

Process *sched_select_mlfq(Scheduler *s)
{
    return mlfq_empty(s->mlfq) ? NULL : mlfq_pick(s->mlfq);
}

Process *sched_select_rr(Scheduler *s)
{
    return queue_empty(s->ready_q) ? NULL : queue_dequeue(s->ready_q);
}

Process *sched_select_priority(Scheduler *s)
{
    /* Already sorted; peek and dequeue highest */
    return queue_empty(s->ready_q) ? NULL : queue_dequeue(s->ready_q);
}

Process *sched_select_srtf(Scheduler *s)
{
    if (queue_empty(s->ready_q)) return NULL;

    /* Find minimum remaining_time (linear scan) */
    Process *best = NULL;
    Process *cur  = s->ready_q->head;
    while (cur) {
        if (!best || cur->remaining_time < best->remaining_time)
            best = cur;
        cur = cur->next;
    }
    if (best) queue_remove(s->ready_q, best->pid);
    return best;
}

/* ═══════════════════════════════════════════════════
 *  One Simulation Tick
 * ═══════════════════════════════════════════════════ */

void sched_tick(Scheduler *s)
{
    /* 1. Admit newly arrived processes */
    sched_handle_arrivals(s);

    /* 2. Progress I/O timers */
    sched_handle_io(s);

    /* 3. Periodic aging check (every AGING_CHECK_INTERVAL ticks) */
    if (s->tick > 0 && s->tick % AGING_CHECK_INTERVAL == 0)
        sched_apply_aging(s);

    /* 4. Determine current quantum limit for running process */
    int quantum_limit = s->rr_quantum; /* default */
    if (s->algorithm == ALGO_MLFQ && s->running)
        quantum_limit = s->mlfq->quanta[s->running->queue_level];

    /* 5. Check if current process must be preempted */
    if (s->running) {
        int preempt = 0;

        /* Quantum expiry */
        if (s->running->quantum_used >= quantum_limit)
            preempt = 1;

        /* SRTF: check if a shorter process arrived */
        if (s->algorithm == ALGO_SRTF && !queue_empty(s->ready_q)) {
            Process *shortest = s->ready_q->head;
            Process *cur = s->ready_q->head;
            while (cur) {
                if (cur->remaining_time < shortest->remaining_time)
                    shortest = cur;
                cur = cur->next;
            }
            if (shortest->remaining_time < s->running->remaining_time)
                preempt = 1;
        }

        /* PRIORITY: preempt if a higher-priority process is waiting */
        if (s->algorithm == ALGO_PRIORITY && !queue_empty(s->ready_q)) {
            if (s->ready_q->head->dynamic_priority > s->running->dynamic_priority)
                preempt = 1;
        }

        if (preempt) {
            /* Check I/O trigger before preemption */
            if (!s->running->io_triggered && s->running->io_burst_time > 0
                && s->running->remaining_time <= s->running->burst_time / 2) {
                sched_handle_io_trigger(s, s->running);
            } else {
                sched_handle_preemption(s);
            }
        }
    }

    /* 6. Pick next process if CPU is free */
    if (!s->running) {
        Process *next = NULL;
        switch (s->algorithm) {
            case ALGO_MLFQ:     next = sched_select_mlfq(s);     break;
            case ALGO_RR:       next = sched_select_rr(s);       break;
            case ALGO_PRIORITY: next = sched_select_priority(s); break;
            case ALGO_SRTF:     next = sched_select_srtf(s);     break;
        }
        if (next) {
            /* Context switch accounting */
            if (next->pid != s->prev_pid)
                s->stats->context_switches++;
            s->prev_pid = next->pid;

            next->state = PROC_RUNNING;
            if (!next->first_run) {
                next->first_run      = 1;
                next->first_run_tick = s->tick;
            }
            s->running = next;
        }
    }

    /* 7. Execute one tick on the running process */
    if (s->running) {
        Process *p = s->running;
        p->remaining_time--;
        p->quantum_used++;
        p->cpu_ticks_used++;
        s->stats->cpu_busy_ticks++;

        /* Record in Gantt log */
        if (s->tick < GANTT_MAX) s->gantt[s->tick] = p->pid;

        /* Mid-burst I/O trigger: when first half exhausted */
        if (!p->io_triggered && p->io_burst_time > 0
            && p->remaining_time == p->burst_time / 2) {
            sched_handle_io_trigger(s, p);
        }
        /* Completion */
        else if (p->remaining_time <= 0) {
            sched_handle_completion(s, p);
        }

        /* Mild priority decay (running process shouldn't grow priority
           from accumulated boosts without bound) */
        if (s->running) sched_priority_decay(s->running);

    } else {
        /* CPU is idle */
        s->stats->cpu_idle_ticks++;
        if (s->tick < GANTT_MAX) s->gantt[s->tick] = -1;
    }

    /* 8. Increment waiting time and age for all READY processes */
    for (int i = 0; i < s->total_procs; i++) {
        Process *p = s->all_procs[i];
        if (p->state == PROC_READY) {
            p->waiting_time++;
            p->age++;
        }
    }
}

/* ═══════════════════════════════════════════════════
 *  Main Simulation Loop
 * ═══════════════════════════════════════════════════ */

void sched_run(Scheduler *s)
{
    if (s->animate) {
        display_enable_ansi();
        printf(CUR_HIDE CUR_CLEAR);
    }

    while (s->tick < s->max_ticks) {
        sched_tick(s);

        if (s->animate) {
            display_dashboard(s);
            if (s->speed_ms > 0) SLEEP_MS(s->speed_ms);
        }

        /* Stop early if all processes are done */
        if (s->completed_procs >= s->total_procs) {
            s->tick++;
            break;
        }
        s->tick++;
    }

    /* Finalise statistics */
    stats_finalize(s->stats, s->tick);

    if (s->animate) {
        display_final_report(s);
        printf(CUR_SHOW);
    }
}

/* ═══════════════════════════════════════════════════
 *  Comparison Mode — run all 4 algorithms, print table
 * ═══════════════════════════════════════════════════ */

/* Deep-clone a process (preserve all timing/priority fields, reset state) */
static Process *clone_process(const Process *src)
{
    Process *p = (Process *)calloc(1, sizeof(Process));
    *p = *src;           /* copy all fields */
    p->state          = PROC_NOT_ARRIVED;
    p->remaining_time = src->burst_time;
    p->dynamic_priority = src->static_priority;
    p->quantum_used   = 0;
    p->age            = 0;
    p->starvation_count = 0;
    p->waiting_time   = 0;
    p->turnaround_time = 0;
    p->response_time  = 0;
    p->completion_time = 0;
    p->first_run_tick = 0;
    p->cpu_ticks_used = 0;
    p->io_ticks_used  = 0;
    p->io_remaining   = 0;
    p->io_triggered   = 0;
    p->first_run      = 0;
    p->queue_level    = 0;
    p->next           = NULL;
    return p;
}

void sched_compare(Process **procs, int count, int ticks)
{
    Algorithm algos[]       = { ALGO_MLFQ, ALGO_RR, ALGO_PRIORITY, ALGO_SRTF };
    const char *names[]     = { "MLFQ (Dynamic)", "Round Robin",
                                "Priority Preempt", "SRTF" };
    Stats *results[4];

    printf("\n");
    printf("  Running comparison across all 4 algorithms "
           "(%d processes, %d ticks)...\n\n", count, ticks);

    for (int a = 0; a < 4; a++) {
        Scheduler *s = sched_create(algos[a], ticks, DEFAULT_RR_QUANTUM, 0, 0);

        /* Clone process set */
        for (int i = 0; i < count; i++) {
            Process *cp = clone_process(procs[i]);
            sched_add_process(s, cp);
        }

        sched_run(s);
        results[a] = s->stats;
        s->stats   = NULL;   /* prevent double-free */

        /* Free cloned processes */
        for (int i = 0; i < s->total_procs; i++)
            process_destroy(s->all_procs[i]);

        sched_destroy(s);
    }

    display_comparison_table(results, names, 4);

    for (int a = 0; a < 4; a++) stats_destroy(results[a]);
}
