/*
 * display.h — ANSI terminal dashboard for the scheduler
 * Smart CPU Scheduler with Dynamic Priority Adjustment
 */
#ifndef DISPLAY_H
#define DISPLAY_H

#include "scheduler.h"

/* ── ANSI escape codes ── */
#define A_RESET     "\033[0m"
#define A_BOLD      "\033[1m"
#define A_DIM       "\033[2m"

/* Standard foreground colors */
#define FG_BLACK    "\033[30m"
#define FG_RED      "\033[31m"
#define FG_GREEN    "\033[32m"
#define FG_YELLOW   "\033[33m"
#define FG_BLUE     "\033[34m"
#define FG_MAGENTA  "\033[35m"
#define FG_CYAN     "\033[36m"
#define FG_WHITE    "\033[37m"

/* Bright foreground colors */
#define FG_BRED     "\033[91m"
#define FG_BGREEN   "\033[92m"
#define FG_BYELLOW  "\033[93m"
#define FG_BBLUE    "\033[94m"
#define FG_BMAGENTA "\033[95m"
#define FG_BCYAN    "\033[96m"
#define FG_BWHITE   "\033[97m"

/* Background colors */
#define BG_BLACK    "\033[40m"
#define BG_RED      "\033[41m"
#define BG_GREEN    "\033[42m"
#define BG_YELLOW   "\033[43m"
#define BG_BLUE     "\033[44m"
#define BG_MAGENTA  "\033[45m"
#define BG_CYAN     "\033[46m"
#define BG_WHITE    "\033[47m"
#define BG_BBLACK   "\033[100m"

/* Cursor control */
#define CUR_HOME    "\033[H"
#define CUR_CLEAR   "\033[2J\033[H"
#define CUR_HIDE    "\033[?25l"
#define CUR_SHOW    "\033[?25h"

/* Box-drawing (UTF-8) */
#define BX_TL  "\xe2\x94\x8c"  /* ┌ */
#define BX_TR  "\xe2\x94\x90"  /* ┐ */
#define BX_BL  "\xe2\x94\x94"  /* └ */
#define BX_BR  "\xe2\x94\x98"  /* ┘ */
#define BX_H   "\xe2\x94\x80"  /* ─ */
#define BX_V   "\xe2\x94\x82"  /* │ */
#define BX_ML  "\xe2\x94\x9c"  /* ├ */
#define BX_MR  "\xe2\x94\xa4"  /* ┤ */
#define BX_TM  "\xe2\x94\xac"  /* ┬ */
#define BX_BM  "\xe2\x94\xb4"  /* ┴ */
#define BX_MM  "\xe2\x94\xbc"  /* ┼ */

/* ── Public display API ── */
void display_enable_ansi(void);
void display_dashboard(const Scheduler *s);
void display_final_report(const Scheduler *s);
void display_comparison_table(Stats **stats_arr, const char *const *names, int count);

/* ── Helpers ── */
const char *display_proc_color(int pid);
void        display_hbar(const char *color, int filled, int total, int width);

#endif /* DISPLAY_H */
