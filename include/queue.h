/*
 * queue.h — Ready queue, I/O queue, and MLFQ structures
 * Smart CPU Scheduler with Dynamic Priority Adjustment
 */
#ifndef QUEUE_H
#define QUEUE_H

#include "process.h"

/* ── Simple FIFO queue (intrusive linked list) ── */

typedef struct {
    Process *head;
    Process *tail;
    int      count;
} Queue;

/* ── Multi-Level Feedback Queue ── */

typedef struct {
    Queue levels[MLFQ_LEVELS];
    int   quanta[MLFQ_LEVELS];   /* time quantum per level */
} MLFQ;

/* ── Queue operations ── */

Queue *queue_create(void);
void   queue_destroy(Queue *q);
void   queue_enqueue(Queue *q, Process *p);
Process *queue_dequeue(Queue *q);
Process *queue_peek(const Queue *q);
void   queue_remove(Queue *q, int pid);     /* remove by PID (O(n)) */
int    queue_empty(const Queue *q);

/* ── MLFQ operations ── */

MLFQ *mlfq_create(void);
void  mlfq_destroy(MLFQ *m);
void  mlfq_insert(MLFQ *m, Process *p, int level);
Process *mlfq_pick(MLFQ *m);               /* dequeue from highest non-empty level */
int   mlfq_empty(const MLFQ *m);
void  mlfq_demote(MLFQ *m, Process *p);    /* move down one level */
void  mlfq_promote(MLFQ *m, Process *p);   /* move up one level (aging) */

/* ── Priority-sorted insertion (for PRIORITY algo) ── */
void queue_enqueue_priority(Queue *q, Process *p); /* sorted by dynamic_priority desc */

#endif /* QUEUE_H */
