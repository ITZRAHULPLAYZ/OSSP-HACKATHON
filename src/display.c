/*
 * display.c — Real-time ANSI terminal dashboard
 *
 * Renders a full-screen dashboard with:
 *   • Header bar  (algorithm, clock, CPU%, context-switches)
 *   • Gantt chart (last GANTT_DISPLAY_SIZE ticks, colour-coded per PID)
 *   • Queue inspector (MLFQ levels or ready-queue count)
 *   • Process table  (all processes with live metrics)
 *   • Stats bar      (averages, throughput)
 *
 * Smart CPU Scheduler with Dynamic Priority Adjustment
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/display.h"

#ifdef _WIN32
#  include <windows.h>
static void win_enable_ansi(void)
{
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD  mode = 0;
    GetConsoleMode(h, &mode);
    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(h, mode);
}
#endif

/* ═══════════════════════════════════════════════════
 *  Colour palette — one per PID slot
 * ═══════════════════════════════════════════════════ */
static const char *PALETTE[] = {
    "\033[91m", "\033[92m", "\033[93m", "\033[94m",
    "\033[95m", "\033[96m", "\033[33m", "\033[36m",
    "\033[32m", "\033[35m", "\033[34m", "\033[31m",
};
#define PALETTE_SZ ((int)(sizeof(PALETTE)/sizeof(PALETTE[0])))

const char *display_proc_color(int pid)
{
    if (pid < 0) return A_DIM;
    return PALETTE[pid % PALETTE_SZ];
}

/* ── Enable ANSI on Windows ── */
void display_enable_ansi(void)
{
#ifdef _WIN32
    win_enable_ansi();
#endif
    /* Enable UTF-8 on Windows console */
#ifdef _WIN32
    SetConsoleOutputCP(65001);
#endif
}

/* ═══════════════════════════════════════════════════
 *  Drawing primitives
 * ═══════════════════════════════════════════════════ */

static void repeat(const char *s, int n)
{
    for (int i = 0; i < n; i++) fputs(s, stdout);
}

static void hline(char left, char right, int inner_width)
{
    putchar(left);
    repeat("\xe2\x94\x80", inner_width);   /* ─ */
    putchar(right);
    putchar('\n');
}

static void box_top(int w)   { hline('\xe2\x94\x8c', '\xe2\x94\x90', w); }
static void box_sep(int w)   { hline('\xe2\x94\x9c', '\xe2\x94\xa4', w); }
static void box_bot(int w)   { hline('\xe2\x94\x94', '\xe2\x94\x98', w); }

static void box_row(const char *content, int width)
{
    int len = (int)strlen(content);
    printf("\xe2\x94\x82 %s", content);             /* │ content */
    /* Pad to width */
    for (int i = len + 1; i < width - 1; i++) putchar(' ');
    printf(" \xe2\x94\x82\n");                      /* │ */
}

/* Horizontal fill bar: filled/total cells rendered in [color] + dim */
void display_hbar(const char *color, int filled, int total, int width)
{
    int fill_chars = (total > 0) ? (filled * width / total) : 0;
    printf("%s", color);
    for (int i = 0; i < fill_chars; i++) putchar('#');
    printf(A_RESET A_DIM);
    for (int i = fill_chars; i < width; i++) putchar('-');
    printf(A_RESET);
}

/* ═══════════════════════════════════════════════════
 *  Header
 * ═══════════════════════════════════════════════════ */
static void draw_header(const Scheduler *s)
{
    char buf[DISPLAY_WIDTH + 4];
    float util = (s->tick > 0)
        ? (float)s->stats->cpu_busy_ticks / s->tick * 100.0f : 0.0f;

    box_top(DISPLAY_WIDTH);

    snprintf(buf, sizeof(buf),
        A_BOLD FG_BWHITE
        " CPU SCHEDULER " A_RESET
        "  Algo: " FG_BCYAN A_BOLD "%-20s" A_RESET
        "  Tick: " FG_BYELLOW A_BOLD "%04d" A_RESET
        "  CPU: " FG_BGREEN A_BOLD "%.1f%%" A_RESET
        "  CtxSw: " FG_BMAGENTA A_BOLD "%d" A_RESET,
        algo_name_str(s->algorithm), s->tick,
        util, s->stats->context_switches);
    box_row(buf, DISPLAY_WIDTH + 60);  /* +60 for escape sequences */
    box_sep(DISPLAY_WIDTH);
}

/* ═══════════════════════════════════════════════════
 *  Gantt Chart
 * ═══════════════════════════════════════════════════ */
static void draw_gantt(const Scheduler *s)
{
    /* Show last GANTT_DISPLAY_SIZE ticks */
    int start = s->tick - GANTT_DISPLAY_SIZE;
    if (start < 0) start = 0;

    /* First row: label */
    char lbl[64];
    snprintf(lbl, sizeof(lbl), FG_BWHITE A_BOLD " GANTT  " A_RESET
             A_DIM "[tick %3d → %3d]" A_RESET, start, s->tick - 1);
    box_row(lbl, DISPLAY_WIDTH + 30);

    /* Second row: blocks */
    printf("\xe2\x94\x82 ");
    for (int t = start; t < s->tick && t < GANTT_MAX; t++) {
        int pid = s->gantt[t];
        if (pid < 0) {
            printf(A_DIM "·" A_RESET);
        } else {
            printf("%s█" A_RESET, display_proc_color(pid));
        }
    }
    /* Pad remainder */
    int drawn = s->tick - start;
    for (int i = drawn; i < GANTT_DISPLAY_SIZE; i++) putchar(' ');
    printf(" \xe2\x94\x82\n");

    /* Legend row: last 10 unique PIDs seen */
    char leg[DISPLAY_WIDTH * 2] = "";
    char entry[24];
    int shown[20];
    int scount = 0;
    for (int t = s->tick - 1; t >= start && scount < 8; t--) {
        int pid = s->gantt[t];
        if (pid < 0) continue;
        int dup = 0;
        for (int x = 0; x < scount; x++) if (shown[x] == pid) { dup = 1; break; }
        if (!dup) {
            shown[scount++] = pid;
            /* find process name */
            const char *nm = "???";
            for (int i = 0; i < s->total_procs; i++)
                if (s->all_procs[i]->pid == pid) { nm = s->all_procs[i]->name; break; }
            snprintf(entry, sizeof(entry), "%s█ P%d:%-8s" A_RESET " ",
                     display_proc_color(pid), pid, nm);
            strncat(leg, entry, sizeof(leg) - strlen(leg) - 1);
        }
    }
    if (scount == 0) strncat(leg, A_DIM "(no activity yet)" A_RESET,
                              sizeof(leg) - strlen(leg) - 1);
    box_row(leg, DISPLAY_WIDTH + 60);
    box_sep(DISPLAY_WIDTH);
}

/* ═══════════════════════════════════════════════════
 *  Queue Inspector
 * ═══════════════════════════════════════════════════ */
static void draw_queues(const Scheduler *s)
{
    char buf[DISPLAY_WIDTH * 3];

    snprintf(buf, sizeof(buf), FG_BWHITE A_BOLD " QUEUES" A_RESET);
    box_row(buf, DISPLAY_WIDTH + 20);

    if (s->algorithm == ALGO_MLFQ) {
        int quanta[MLFQ_LEVELS] = { MLFQ_Q0, MLFQ_Q1, MLFQ_Q2, MLFQ_Q3 };
        for (int lvl = 0; lvl < MLFQ_LEVELS; lvl++) {
            char row[DISPLAY_WIDTH * 3] = "";
            char tmp[64];
            snprintf(tmp, sizeof(tmp),
                     FG_YELLOW " Level %d" A_RESET A_DIM "[Q=%-2d]" A_RESET " : ",
                     lvl, quanta[lvl]);
            strcat(row, tmp);

            Process *p = s->mlfq->levels[lvl].head;
            if (!p) {
                strcat(row, A_DIM "(empty)" A_RESET);
            } else {
                while (p) {
                    snprintf(tmp, sizeof(tmp), "%sP%d" A_RESET,
                             display_proc_color(p->pid), p->pid);
                    strcat(row, tmp);
                    if (p->next) strcat(row, A_DIM " → " A_RESET);
                    p = p->next;
                }
            }
            box_row(row, DISPLAY_WIDTH + 80);
        }
    } else {
        char row[DISPLAY_WIDTH * 3] = "";
        char tmp[64];
        snprintf(tmp, sizeof(tmp),
                 FG_YELLOW " Ready Queue" A_RESET " (%d) : ",
                 s->ready_q->count);
        strcat(row, tmp);
        Process *p = s->ready_q->head;
        int shown = 0;
        while (p && shown < 12) {
            snprintf(tmp, sizeof(tmp), "%sP%d" A_RESET,
                     display_proc_color(p->pid), p->pid);
            strcat(row, tmp);
            if (p->next && shown < 11) strcat(row, A_DIM " → " A_RESET);
            p = p->next; shown++;
        }
        if (!s->ready_q->head) strcat(row, A_DIM "(empty)" A_RESET);
        box_row(row, DISPLAY_WIDTH + 80);
    }

    /* I/O wait queue */
    {
        char row[DISPLAY_WIDTH * 3] = "";
        char tmp[64];
        snprintf(tmp, sizeof(tmp), FG_BRED " I/O Wait " A_RESET "         : ");
        strcat(row, tmp);
        Process *p = s->io_q->head;
        if (!p) {
            strcat(row, A_DIM "(empty)" A_RESET);
        } else {
            while (p) {
                snprintf(tmp, sizeof(tmp), "%sP%d" A_RESET
                         A_DIM "[%dt]" A_RESET,
                         display_proc_color(p->pid), p->pid, p->io_remaining);
                strcat(row, tmp);
                if (p->next) strcat(row, A_DIM " → " A_RESET);
                p = p->next;
            }
        }
        box_row(row, DISPLAY_WIDTH + 80);
    }
    box_sep(DISPLAY_WIDTH);
}

/* ═══════════════════════════════════════════════════
 *  Process Table
 * ═══════════════════════════════════════════════════ */
static void draw_process_table(const Scheduler *s)
{
    /* Header row */
    char hdr[256];
    snprintf(hdr, sizeof(hdr),
        FG_BWHITE A_BOLD
        " PID  %-12s  PRI DPR  %-9s  WAIT REM  AGE TYPE BOOST" A_RESET,
        "NAME", "STATE");
    box_row(hdr, DISPLAY_WIDTH + 60);

    for (int i = 0; i < s->total_procs; i++) {
        Process *p = s->all_procs[i];
        const char *col;
        switch (p->state) {
            case PROC_RUNNING:  col = FG_BGREEN;   break;
            case PROC_READY:    col = FG_BYELLOW;  break;
            case PROC_WAITING:  col = FG_BRED;     break;
            case PROC_DONE:     col = A_DIM;       break;
            default:            col = FG_WHITE;    break;
        }

        /* Starvation indicator: stars for each boost received */
        char boost_str[16] = "";
        for (int b = 0; b < p->starvation_count && b < 3; b++)
            strcat(boost_str, "\xe2\x98\x85");   /* ★ */

        /* Age bar: fill 1 # per 3 ticks of age, max 5 */
        char age_bar[12] = "";
        int bar_fill = p->age / 3;
        if (bar_fill > 5) bar_fill = 5;
        const char *age_col = (p->age >= STARVATION_THRESHOLD) ? FG_BRED : FG_YELLOW;
        char bar_tmp[32] = "";
        if (p->state == PROC_READY) {
            for (int b = 0; b < bar_fill; b++) strcat(bar_tmp, "|");
        }

        char row[512];
        snprintf(row, sizeof(row),
            " %s%3d%s  %-12s  %2d  %2d   %s%-9s%s  %3d  %3d  %s%3d%s %s%-3s  %s%s%s",
            display_proc_color(p->pid), p->pid, A_RESET,
            p->name,
            p->static_priority, p->dynamic_priority,
            col, process_state_str(p->state), A_RESET,
            p->waiting_time, p->remaining_time,
            age_col, p->age, A_RESET,
            p->type != PTYPE_UNKNOWN ? "" : A_DIM,
            process_type_str(p->type),
            FG_BYELLOW, boost_str, A_RESET);
        box_row(row, DISPLAY_WIDTH + 180);
    }
    box_sep(DISPLAY_WIDTH);
}

/* ═══════════════════════════════════════════════════
 *  Stats Bar
 * ═══════════════════════════════════════════════════ */
static void draw_stats_bar(const Scheduler *s)
{
    float util = (s->tick > 0)
        ? (float)s->stats->cpu_busy_ticks / s->tick * 100.0f : 0.0f;

    char buf[512];
    snprintf(buf, sizeof(buf),
        " " FG_BGREEN "AvgWait" A_RESET "=%.1ft  "
        FG_BBLUE "AvgTAT" A_RESET "=%.1ft  "
        FG_BCYAN "AvgResp" A_RESET "=%.1ft  "
        FG_BYELLOW "Util" A_RESET "=%.0f%%  "
        FG_BMAGENTA "Aging" A_RESET "=%d  "
        FG_BRED "Stars" A_RESET "=%d",
        s->stats->procs_counted > 0
            ? (float)s->stats->total_waiting / s->stats->procs_counted : 0,
        s->stats->procs_counted > 0
            ? (float)s->stats->total_turnaround / s->stats->procs_counted : 0,
        s->stats->procs_counted > 0
            ? (float)s->stats->total_response / s->stats->procs_counted : 0,
        util,
        s->stats->aging_promotions,
        s->stats->starvation_events);

    box_row(buf, DISPLAY_WIDTH + 120);
    box_bot(DISPLAY_WIDTH);
}

/* ═══════════════════════════════════════════════════
 *  Full Dashboard
 * ═══════════════════════════════════════════════════ */
void display_dashboard(const Scheduler *s)
{
    printf(CUR_HOME);
    draw_header(s);
    draw_gantt(s);
    draw_queues(s);
    draw_process_table(s);
    draw_stats_bar(s);
    fflush(stdout);
}

/* ═══════════════════════════════════════════════════
 *  Final Report (after simulation ends)
 * ═══════════════════════════════════════════════════ */
void display_final_report(const Scheduler *s)
{
    printf("\n\n");
    printf(FG_BWHITE A_BOLD
           "  ╔══════════════════════════════════════════════╗\n"
           "  ║         SIMULATION COMPLETE — REPORT         ║\n"
           "  ╚══════════════════════════════════════════════╝\n"
           A_RESET);

    stats_print(s->stats, algo_name_str(s->algorithm));

    printf("\n" A_BOLD " Per-Process Summary:\n" A_RESET);
    printf("  %-4s  %-14s  %-5s  %-5s  %-5s  %-4s  %-4s\n",
           "PID", "NAME", "WAIT", "TAT", "RESP", "TYPE", "★");
    printf("  %-4s  %-14s  %-5s  %-5s  %-5s  %-4s  %-4s\n",
           "---", "--------------", "-----", "-----", "-----", "----", "--");

    for (int i = 0; i < s->total_procs; i++) {
        Process *p = s->all_procs[i];
        if (p->state != PROC_DONE) continue;
        printf("  %s%-3d%s   %-14s  %-5d  %-5d  %-5d  %-4s  %d\n",
               display_proc_color(p->pid), p->pid, A_RESET,
               p->name, p->waiting_time, p->turnaround_time,
               p->response_time, process_type_str(p->type),
               p->starvation_count);
    }
    printf("\n");
}

/* ═══════════════════════════════════════════════════
 *  Algorithm Comparison Table
 * ═══════════════════════════════════════════════════ */
void display_comparison_table(Stats **stats_arr,
                               const char *const *names, int count)
{
    printf(FG_BWHITE A_BOLD
           "\n  ╔══════════════════════════════════════════════════════════════╗\n"
           "  ║              ALGORITHM COMPARISON REPORT                     ║\n"
           "  ╠══════════════╦══════════╦══════════╦══════════╦══════════════╣\n"
           "  ║ Algorithm    ║ AvgWait  ║ AvgTAT   ║ AvgResp  ║ CPU Util%%   ║\n"
           "  ╠══════════════╬══════════╬══════════╬══════════╬══════════════╣\n"
           A_RESET);

    /* Find best values for highlighting */
    float best_wait = 1e9f, best_tat = 1e9f, best_resp = 1e9f, best_util = 0;
    for (int i = 0; i < count; i++) {
        if (stats_arr[i]->avg_waiting_time    < best_wait) best_wait = stats_arr[i]->avg_waiting_time;
        if (stats_arr[i]->avg_turnaround_time < best_tat)  best_tat  = stats_arr[i]->avg_turnaround_time;
        if (stats_arr[i]->avg_response_time   < best_resp) best_resp = stats_arr[i]->avg_response_time;
        if (stats_arr[i]->cpu_utilization     > best_util) best_util = stats_arr[i]->cpu_utilization;
    }

    for (int i = 0; i < count; i++) {
        Stats *st = stats_arr[i];
        const char *wc = (st->avg_waiting_time    <= best_wait + 0.01f) ? FG_BGREEN : "";
        const char *tc = (st->avg_turnaround_time <= best_tat  + 0.01f) ? FG_BGREEN : "";
        const char *rc = (st->avg_response_time   <= best_resp + 0.01f) ? FG_BGREEN : "";
        const char *uc = (st->cpu_utilization     >= best_util - 0.01f) ? FG_BGREEN : "";

        printf("  \xe2\x94\x82 %-12s \xe2\x94\x82 %s%8.2f%s \xe2\x94\x82 %s%8.2f%s \xe2\x94\x82 %s%8.2f%s \xe2\x94\x82 %s%10.1f%%%s \xe2\x94\x82\n",
               names[i],
               wc, st->avg_waiting_time,    A_RESET,
               tc, st->avg_turnaround_time, A_RESET,
               rc, st->avg_response_time,   A_RESET,
               uc, st->cpu_utilization,     A_RESET);
    }

    printf(A_BOLD
           "  ╚══════════════╩══════════╩══════════╩══════════╩══════════════╝\n"
           A_RESET);
    printf("  " FG_BGREEN "★ Green" A_RESET " = best value in that column\n\n");
}
