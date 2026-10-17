/*
 * process.c — Process creation, random generation, classification
 * Smart CPU Scheduler with Dynamic Priority Adjustment
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/process.h"

/* Realistic process names for the simulation */
static const char *proc_name_pool[] = {
    "nginx", "vim",    "gcc",    "python",  "bash",
    "node",  "redis",  "mysql",  "firefox", "java",
    "htop",  "ssh",    "curl",   "rsync",   "git",
    "docker","kafka",  "go",     "rustc",   "llvm"
};
#define NAME_POOL_SZ ((int)(sizeof(proc_name_pool)/sizeof(proc_name_pool[0])))

/* ── Public API ── */

Process *process_create(int pid, const char *name,
                        int arrival, int burst, int priority)
{
    Process *p = (Process *)calloc(1, sizeof(Process));
    if (!p) { perror("process_create: calloc"); exit(1); }

    p->pid            = pid;
    strncpy(p->name, name, MAX_NAME_LEN - 1);
    p->arrival_time   = arrival;
    p->burst_time     = burst;
    p->remaining_time = burst;
    p->static_priority  = priority;
    p->dynamic_priority = priority;
    p->state          = PROC_NOT_ARRIVED;
    p->type           = PTYPE_UNKNOWN;
    p->queue_level    = 0;   /* start at highest MLFQ level */
    p->first_run      = 0;

    /* Randomly assign an I/O burst */
    if ((rand() % 100) < IO_BURST_PROBABILITY) {
        p->io_burst_time = MIN_IO_BURST + rand() % (MAX_IO_BURST - MIN_IO_BURST + 1);
    }
    return p;
}

Process *process_random(int pid, int current_tick)
{
    char name[MAX_NAME_LEN];
    snprintf(name, sizeof(name), "%s-%d",
             proc_name_pool[pid % NAME_POOL_SZ], pid);

    int burst    = MIN_CPU_BURST + rand() % (MAX_CPU_BURST - MIN_CPU_BURST + 1);
    int priority = 1 + rand() % MAX_PRIORITY;
    int arrival  = current_tick + rand() % (MAX_ARRIVAL_SPREAD + 1);

    return process_create(pid, name, arrival, burst, priority);
}

void process_destroy(Process *p)
{
    free(p);
}

void process_classify(Process *p)
{
    int total = p->cpu_ticks_used + p->io_ticks_used;
    if (total == 0) { p->type = PTYPE_UNKNOWN; return; }

    float cpu_ratio = (float)p->cpu_ticks_used / (float)total;
    if      (cpu_ratio > 0.72f) p->type = PTYPE_CPU;
    else if (cpu_ratio < 0.38f) p->type = PTYPE_IO;
    else                         p->type = PTYPE_MIXED;
}

void process_print(const Process *p)
{
    printf("PID=%-3d %-14s PRI=%-2d DPR=%-2d REM=%-3d STATE=%-9s\n",
           p->pid, p->name,
           p->static_priority, p->dynamic_priority,
           p->remaining_time, process_state_str(p->state));
}

/* ── String converters ── */

const char *process_state_str(ProcessState s)
{
    switch (s) {
        case PROC_NOT_ARRIVED: return "PENDING";
        case PROC_READY:       return "READY";
        case PROC_RUNNING:     return "RUNNING";
        case PROC_WAITING:     return "I/O WAIT";
        case PROC_DONE:        return "DONE";
        default:               return "???";
    }
}

const char *process_type_str(ProcessType t)
{
    switch (t) {
        case PTYPE_CPU:   return "CPU";
        case PTYPE_IO:    return "I/O";
        case PTYPE_MIXED: return "MIX";
        default:          return "???";
    }
}

const char *algo_name_str(Algorithm a)
{
    switch (a) {
        case ALGO_MLFQ:     return "MLFQ (Dynamic)";
        case ALGO_RR:       return "Round Robin";
        case ALGO_PRIORITY: return "Priority Preemptive";
        case ALGO_SRTF:     return "SRTF";
        default:            return "Unknown";
    }
}
