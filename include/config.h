/*
 * config.h — Simulation configuration constants
 * Smart CPU Scheduler with Dynamic Priority Adjustment
 */
#ifndef CONFIG_H
#define CONFIG_H

/* ── Process limits ── */
#define MAX_PROCESSES           50
#define MAX_NAME_LEN            24

/* ── Simulation timing ── */
#define DEFAULT_TICKS           120
#define DEFAULT_SPEED_MS        120     /* ms per tick */
#define GANTT_DISPLAY_SIZE      52      /* ticks shown in Gantt bar */

/* ── MLFQ configuration ── */
#define MLFQ_LEVELS             4
/* Time quanta for each MLFQ level (must match MLFQ_LEVELS) */
#define MLFQ_Q0                 4
#define MLFQ_Q1                 8
#define MLFQ_Q2                 16
#define MLFQ_Q3                 32

/* ── Priority bounds ── */
#define MAX_PRIORITY            10
#define MIN_PRIORITY            1
#define DEFAULT_RR_QUANTUM      5

/* ── Anti-starvation / aging ── */
#define STARVATION_THRESHOLD    18      /* ticks before aging kicks in */
#define AGING_CHECK_INTERVAL    4       /* run aging check every N ticks */
#define AGE_BOOST               2       /* priority bump per aging event */
#define MAX_QUEUE_BOOST         2       /* max queue-level promotions via aging */

/* ── Dynamic priority weights ── */
#define CPU_BOUND_PENALTY       1       /* full-quantum user → -1 priority */
#define IO_BOUND_REWARD         1       /* early-yield user → +1 priority */
#define PRIORITY_DECAY_TICKS    8       /* decay 1 point every N running ticks */

/* ── Process generation ── */
#define IO_BURST_PROBABILITY    40      /* % chance a process has an I/O phase */
#define MIN_IO_BURST            2
#define MAX_IO_BURST            7
#define MIN_CPU_BURST           6
#define MAX_CPU_BURST           28
#define MAX_ARRIVAL_SPREAD      6       /* max ticks after sim start for arrivals */

/* ── Display ── */
#define DISPLAY_WIDTH           76      /* terminal columns for boxes */

#endif /* CONFIG_H */
