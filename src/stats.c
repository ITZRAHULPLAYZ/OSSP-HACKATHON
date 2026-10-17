/*
 * stats.c — Statistics collection and reporting
 * Smart CPU Scheduler with Dynamic Priority Adjustment
 */
#include <stdio.h>
#include <stdlib.h>
#include "../include/stats.h"

Stats *stats_create(void)
{
    Stats *s = (Stats *)calloc(1, sizeof(Stats));
    if (!s) { perror("stats_create"); exit(1); }
    return s;
}

void stats_destroy(Stats *s)
{
    free(s);
}

void stats_record_completion(Stats *s, int wait, int tat, int resp)
{
    s->total_waiting    += wait;
    s->total_turnaround += tat;
    s->total_response   += resp;
    s->procs_counted++;
    s->processes_completed++;
}

void stats_finalize(Stats *s, int total_ticks)
{
    if (s->procs_counted > 0) {
        s->avg_waiting_time    = (float)s->total_waiting    / s->procs_counted;
        s->avg_turnaround_time = (float)s->total_turnaround / s->procs_counted;
        s->avg_response_time   = (float)s->total_response   / s->procs_counted;
    }
    if (total_ticks > 0) {
        s->cpu_utilization = (float)s->cpu_busy_ticks / total_ticks * 100.0f;
        s->throughput      = (float)s->processes_completed / total_ticks;
    }
}

void stats_print(const Stats *s, const char *algo_name)
{
    printf("\n");
    printf("  %-24s  %s\n", "Algorithm:", algo_name);
    printf("  %-24s  %d\n",   "Processes Completed:",  s->processes_completed);
    printf("  %-24s  %.2f ticks\n", "Avg Waiting Time:",    s->avg_waiting_time);
    printf("  %-24s  %.2f ticks\n", "Avg Turnaround Time:", s->avg_turnaround_time);
    printf("  %-24s  %.2f ticks\n", "Avg Response Time:",   s->avg_response_time);
    printf("  %-24s  %.1f%%\n",     "CPU Utilization:",     s->cpu_utilization);
    printf("  %-24s  %.4f proc/tick\n", "Throughput:",      s->throughput);
    printf("  %-24s  %d\n",   "Context Switches:",     s->context_switches);
    printf("  %-24s  %d\n",   "Starvation Events:",    s->starvation_events);
    printf("  %-24s  %d\n",   "Aging Promotions:",     s->aging_promotions);
    printf("  %-24s  %d\n",   "Preemptions:",          s->preemptions);
}
