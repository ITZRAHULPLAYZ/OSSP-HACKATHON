/*
 * process.h — Process Control Block (PCB) and related types
 * Smart CPU Scheduler with Dynamic Priority Adjustment
 */
#ifndef PROCESS_H
#define PROCESS_H

#include "config.h"

/* ── Enumerations ── */

typedef enum {
    PROC_NOT_ARRIVED = 0,
    PROC_READY       = 1,
    PROC_RUNNING     = 2,
    PROC_WAITING     = 3,   /* blocked on I/O */
    PROC_DONE        = 4
} ProcessState;

typedef enum {
    PTYPE_UNKNOWN = 0,
    PTYPE_CPU     = 1,      /* CPU-bound (uses full quanta) */
    PTYPE_IO      = 2,      /* I/O-bound (yields early) */
    PTYPE_MIXED   = 3
} ProcessType;

typedef enum {
    ALGO_MLFQ     = 0,
    ALGO_RR       = 1,
    ALGO_PRIORITY = 2,      /* preemptive priority */
    ALGO_SRTF     = 3       /* shortest remaining time first */
} Algorithm;

/* ── Process Control Block ── */

typedef struct Process {
    /* Identity */
    int   pid;
    char  name[MAX_NAME_LEN];

    /* Timing */
    int   arrival_time;
    int   burst_time;           /* total CPU time needed */
    int   remaining_time;       /* CPU time still needed */
    int   io_burst_time;        /* duration of I/O phase (0 = none) */
    int   io_remaining;         /* remaining I/O ticks */
    int   io_triggered;         /* 1 once I/O phase has been initiated */
    int   quantum_used;         /* ticks used in current scheduling slot */

    /* Priority */
    int   static_priority;      /* original (1–10, higher = more urgent) */
    int   dynamic_priority;     /* adjusted at runtime */
    int   age;                  /* ticks since last scheduled */
    int   starvation_count;     /* times aging was applied */

    /* Statistics */
    int   waiting_time;         /* cumulative READY-state ticks */
    int   turnaround_time;      /* completion_time - arrival_time */
    int   response_time;        /* first_run_tick  - arrival_time */
    int   completion_time;
    int   first_run_tick;
    int   cpu_ticks_used;       /* total CPU ticks consumed */
    int   io_ticks_used;        /* total I/O ticks consumed */

    /* State */
    ProcessState state;
    ProcessType  type;
    int          queue_level;   /* MLFQ level (0 = highest priority) */
    int          first_run;     /* flag: 0 = never ran yet */

    struct Process *next;       /* intrusive linked-list pointer */
} Process;

/* ── Function declarations ── */

Process *process_create(int pid, const char *name,
                        int arrival, int burst, int priority);
Process *process_random(int pid, int current_tick);
void     process_destroy(Process *p);
void     process_classify(Process *p);   /* detect CPU/IO type from usage */
void     process_print(const Process *p);

const char *process_state_str(ProcessState s);
const char *process_type_str(ProcessType  t);
const char *algo_name_str(Algorithm a);

#endif /* PROCESS_H */
