/*
 * main.c — Entry point, CLI argument parsing, process generation
 *
 * Usage:
 *   scheduler [OPTIONS]
 *
 * Options:
 *   --algo  <mlfq|rr|priority|srtf>   Scheduling algorithm (default: mlfq)
 *   --procs <N>                        Number of processes  (default: 8)
 *   --ticks <N>                        Simulation ticks     (default: 120)
 *   --quantum <N>                      Round-Robin quantum  (default: 5)
 *   --speed <ms>                       ms per tick          (default: 120)
 *   --seed  <N>                        RNG seed             (default: time)
 *   --compare                          Run all 4 algorithms and print table
 *   --no-anim                          Disable live dashboard
 *   --help                             Show this help
 *
 * Smart CPU Scheduler with Dynamic Priority Adjustment
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../include/scheduler.h"
#include "../include/display.h"

/* ── Argument parsing helpers ── */
static int arg_int(int argc, char **argv, const char *flag, int def)
{
    for (int i = 1; i < argc - 1; i++)
        if (strcmp(argv[i], flag) == 0) return atoi(argv[i + 1]);
    return def;
}

static int arg_flag(int argc, char **argv, const char *flag)
{
    for (int i = 1; i < argc; i++)
        if (strcmp(argv[i], flag) == 0) return 1;
    return 0;
}

static const char *arg_str(int argc, char **argv,
                            const char *flag, const char *def)
{
    for (int i = 1; i < argc - 1; i++)
        if (strcmp(argv[i], flag) == 0) return argv[i + 1];
    return def;
}

static void print_help(const char *prog)
{
    printf("\n  Smart CPU Scheduler with Dynamic Priority Adjustment\n\n");
    printf("  Usage: %s [OPTIONS]\n\n", prog);
    printf("  Options:\n");
    printf("    --algo  <mlfq|rr|priority|srtf>  Algorithm (default: mlfq)\n");
    printf("    --procs <N>                       Processes (default: 8)\n");
    printf("    --ticks <N>                       Ticks     (default: 120)\n");
    printf("    --quantum <N>                     RR quantum (default: 5)\n");
    printf("    --speed <ms>                      ms/tick   (default: 120)\n");
    printf("    --seed  <N>                       RNG seed  (default: time)\n");
    printf("    --compare                         Compare all 4 algorithms\n");
    printf("    --no-anim                         Disable live dashboard\n");
    printf("    --help                            Show this help\n\n");
    printf("  Examples:\n");
    printf("    %s --algo mlfq --procs 10 --ticks 150\n", prog);
    printf("    %s --algo rr --quantum 6 --speed 80\n", prog);
    printf("    %s --compare --procs 12 --ticks 200\n\n", prog);
}

/* ── Parse algorithm string ── */
static Algorithm parse_algo(const char *s)
{
    if (strcmp(s, "rr")       == 0) return ALGO_RR;
    if (strcmp(s, "priority") == 0) return ALGO_PRIORITY;
    if (strcmp(s, "srtf")     == 0) return ALGO_SRTF;
    return ALGO_MLFQ;   /* default */
}

/* ── Main ── */
int main(int argc, char **argv)
{
    if (arg_flag(argc, argv, "--help")) {
        print_help(argv[0]);
        return 0;
    }

    /* Parse CLI */
    const char *algo_str = arg_str(argc, argv, "--algo", "mlfq");
    int num_procs   = arg_int(argc, argv, "--procs",   8);
    int ticks       = arg_int(argc, argv, "--ticks",   DEFAULT_TICKS);
    int quantum     = arg_int(argc, argv, "--quantum",  DEFAULT_RR_QUANTUM);
    int speed       = arg_int(argc, argv, "--speed",   DEFAULT_SPEED_MS);
    int seed        = arg_int(argc, argv, "--seed",    (int)time(NULL));
    int compare     = arg_flag(argc, argv, "--compare");
    int no_anim     = arg_flag(argc, argv, "--no-anim");
    int json_out    = arg_flag(argc, argv, "--json");
    if (json_out) {
        no_anim = 1;
        speed   = 0;
    }

    /* Clamp values */
    if (num_procs < 1)  num_procs = 1;
    if (num_procs > MAX_PROCESSES) num_procs = MAX_PROCESSES;
    if (ticks < 10)     ticks = 10;
    if (ticks > GANTT_MAX - 1) ticks = GANTT_MAX - 1;

    srand((unsigned)seed);
    if (!json_out) display_enable_ansi();

    /* Generate process set */
    Process *procs[MAX_PROCESSES];
    for (int i = 0; i < num_procs; i++)
        procs[i] = process_random(i + 1, 0);

    /* ── Comparison mode ── */
    if (compare) {
        if (json_out) {
            Algorithm algos[]   = { ALGO_MLFQ, ALGO_RR, ALGO_PRIORITY, ALGO_SRTF };
            const char *names[] = { "MLFQ", "RR", "PRI", "SRTF" };
            printf("{\n  \"comparison\": [\n");
            for (int a = 0; a < 4; a++) {
                Scheduler *s = sched_create(algos[a], ticks, DEFAULT_RR_QUANTUM, 0, 0);
                for (int i = 0; i < num_procs; i++) {
                    Process *cp = (Process *)calloc(1, sizeof(Process));
                    *cp = *procs[i];
                    cp->state = PROC_NOT_ARRIVED;
                    cp->remaining_time = procs[i]->burst_time;
                    cp->dynamic_priority = procs[i]->static_priority;
                    sched_add_process(s, cp);
                }
                sched_run(s);
                stats_finalize(s->stats, s->tick);
                printf("    {\"algo\": \"%s\", \"avg_wait\": %.2f, \"avg_tat\": %.2f, \"avg_resp\": %.2f, \"cpu_util\": %.1f, \"starvations\": %d, \"promotions\": %d, \"completed\": \"%d/%d\"}%s\n",
                       names[a], s->stats->avg_waiting_time, s->stats->avg_turnaround_time, s->stats->avg_response_time,
                       s->stats->cpu_utilization, s->stats->starvation_events, s->stats->aging_promotions,
                       s->stats->processes_completed, s->total_procs, (a == 3) ? "" : ",");
                for (int i = 0; i < s->total_procs; i++) process_destroy(s->all_procs[i]);
                sched_destroy(s);
            }
            printf("  ]\n}\n");
            for (int i = 0; i < num_procs; i++) process_destroy(procs[i]);
            return 0;
        }

        printf(FG_BWHITE A_BOLD
               "\n  Smart CPU Scheduler — Algorithm Comparison\n" A_RESET);
        printf("  Seed: %d | Processes: %d | Ticks: %d\n",
               seed, num_procs, ticks);
        sched_compare(procs, num_procs, ticks);
        for (int i = 0; i < num_procs; i++) process_destroy(procs[i]);
        return 0;
    }

    /* ── Single-algorithm mode ── */
    Algorithm algo = parse_algo(algo_str);
    int animate    = !no_anim;

    if (!animate && !json_out) {
        printf(FG_BWHITE A_BOLD
               "\n  Smart CPU Scheduler — %s\n" A_RESET,
               algo_name_str(algo));
        printf("  Seed: %d | Processes: %d | Ticks: %d\n\n",
               seed, num_procs, ticks);
    }

    Scheduler *s = sched_create(algo, ticks, quantum, speed, animate);
    for (int i = 0; i < num_procs; i++)
        sched_add_process(s, procs[i]);

    sched_run(s);

    if (json_out) {
        stats_finalize(s->stats, s->tick);
        printf("{\n");
        printf("  \"algorithm\": \"%s\",\n", algo_name_str(algo));
        printf("  \"ticks\": %d,\n", s->tick);
        printf("  \"stats\": {\n");
        printf("    \"completed\": %d,\n", s->stats->processes_completed);
        printf("    \"avg_wait\": %.2f,\n", s->stats->avg_waiting_time);
        printf("    \"avg_tat\": %.2f,\n", s->stats->avg_turnaround_time);
        printf("    \"avg_resp\": %.2f,\n", s->stats->avg_response_time);
        printf("    \"cpu_util\": %.1f,\n", s->stats->cpu_utilization);
        printf("    \"starvations\": %d,\n", s->stats->starvation_events);
        printf("    \"promotions\": %d\n", s->stats->aging_promotions);
        printf("  },\n");
        printf("  \"processes\": [\n");
        for (int i = 0; i < s->total_procs; i++) {
            Process *p = s->all_procs[i];
            printf("    {\"pid\": %d, \"name\": \"%s\", \"wait\": %d, \"tat\": %d, \"resp\": %d, \"type\": \"%s\", \"starvations\": %d}%s\n",
                   p->pid, p->name, p->waiting_time, p->turnaround_time, p->response_time,
                   process_type_str(p->type), p->starvation_count, (i == s->total_procs - 1) ? "" : ",");
        }
        printf("  ]\n}\n");
        sched_destroy(s);
        return 0;
    }

    if (!animate) {
        /* Print final stats when animation is off */
        stats_finalize(s->stats, s->tick);
        stats_print(s->stats, algo_name_str(algo));
        printf("\n Per-Process Summary:\n");
        printf("  %-4s  %-14s  %-5s  %-5s  %-5s  TYPE  BOOSTS\n",
               "PID", "NAME", "WAIT", "TAT", "RESP");
        for (int i = 0; i < s->total_procs; i++) {
            Process *p = s->all_procs[i];
            printf("  %-4d  %-14s  %-5d  %-5d  %-5d  %-4s  %d\n",
                   p->pid, p->name, p->waiting_time,
                   p->turnaround_time, p->response_time,
                   process_type_str(p->type), p->starvation_count);
        }
        printf("\n");
    }

    sched_destroy(s);
    return 0;
}
