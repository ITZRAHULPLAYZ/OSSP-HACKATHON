/*
 * scheduler.h — Core scheduler context and public API
 * Smart CPU Scheduler with Dynamic Priority Adjustment
 */
#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "queue.h"
#include "stats.h"

#define GANTT_MAX 512   /* max ticks we log in Gantt array */

/* ── Scheduler context ── */

typedef struct {
    Algorithm algorithm;

    /* Process registry */
    Process *all_procs[MAX_PROCESSES];
    int      total_procs;
    int      completed_procs;

    /* Queues */
    MLFQ    *mlfq;          /* used by ALGO_MLFQ */
    Queue   *ready_q;       /* used by ALGO_RR / ALGO_PRIORITY / ALGO_SRTF */
    Queue   *io_q;          /* processes doing I/O (all algorithms) */

    /* Currently executing process */
    Process *running;
    int      prev_pid;      /* for context-switch detection */

    /* Clock */
    int      tick;          /* current simulation tick */
    int      max_ticks;
    int      rr_quantum;    /* Round-Robin quantum */

    /* Gantt log: gantt[t] = PID of process running at tick t, -1 = idle */
    int      gantt[GANTT_MAX];

    /* Statistics */
    Stats   *stats;

    /* Config flags */
    int      speed_ms;      /* ms to sleep per tick (0 = no delay) */
    int      animate;       /* 1 = live dashboard, 0 = silent */
} Scheduler;

/* ── Lifecycle ── */
Scheduler *sched_create(Algorithm algo, int max_ticks, int rr_quantum, int speed_ms, int animate);
void       sched_destroy(Scheduler *s);
void       sched_add_process(Scheduler *s, Process *p);

/* ── Main entry point ── */
void sched_run(Scheduler *s);

/* ── Comparison mode: run all 4 algorithms on cloned process set ── */
void sched_compare(Process **procs, int count, int ticks);

/* ── Internal tick operations (exposed for testing) ── */
void sched_tick(Scheduler *s);
void sched_handle_arrivals(Scheduler *s);
void sched_handle_io(Scheduler *s);
void sched_apply_aging(Scheduler *s);
void sched_handle_completion(Scheduler *s, Process *p);
void sched_handle_io_trigger(Scheduler *s, Process *p);
void sched_handle_preemption(Scheduler *s);

/* ── Algorithm-specific selectors ── */
Process *sched_select_mlfq(Scheduler *s);
Process *sched_select_rr(Scheduler *s);
Process *sched_select_priority(Scheduler *s);
Process *sched_select_srtf(Scheduler *s);

/* ── Dynamic priority helpers ── */
void sched_priority_clamp(Process *p);
void sched_priority_cpu_penalty(Process *p);
void sched_priority_io_reward(Process *p);
void sched_priority_decay(Process *p);

#endif /* SCHEDULER_H */
