/*
 * stats.h — Statistics tracking and reporting
 * Smart CPU Scheduler with Dynamic Priority Adjustment
 */
#ifndef STATS_H
#define STATS_H

typedef struct {
    /* CPU usage */
    int   cpu_busy_ticks;
    int   cpu_idle_ticks;

    /* Process counts */
    int   processes_completed;
    int   context_switches;
    int   preemptions;

    /* Anti-starvation events */
    int   starvation_events;    /* times a process triggered aging */
    int   aging_promotions;     /* total queue-level promotions via aging */

    /* Aggregated (running totals, normalized at end) */
    long  total_waiting;
    long  total_turnaround;
    long  total_response;
    int   procs_counted;

    /* Final computed metrics (call stats_finalize) */
    float avg_waiting_time;
    float avg_turnaround_time;
    float avg_response_time;
    float cpu_utilization;      /* 0–100 % */
    float throughput;           /* procs / tick */
} Stats;

Stats *stats_create(void);
void   stats_destroy(Stats *s);
void   stats_record_completion(Stats *s, int wait, int tat, int resp);
void   stats_finalize(Stats *s, int total_ticks);
void   stats_print(const Stats *s, const char *algo_name);

#endif /* STATS_H */
