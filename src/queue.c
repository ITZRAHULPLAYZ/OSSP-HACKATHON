/*
 * queue.c — FIFO queue, priority queue, and MLFQ implementation
 * Smart CPU Scheduler with Dynamic Priority Adjustment
 */
#include <stdio.h>
#include <stdlib.h>
#include "../include/queue.h"

/* ═══════════════════════════════════════════════════
 *  Simple FIFO Queue
 * ═══════════════════════════════════════════════════ */

Queue *queue_create(void)
{
    Queue *q = (Queue *)calloc(1, sizeof(Queue));
    if (!q) { perror("queue_create"); exit(1); }
    return q;
}

void queue_destroy(Queue *q)
{
    free(q);
}

void queue_enqueue(Queue *q, Process *p)
{
    if (!q || !p) return;
    p->next = NULL;
    if (q->tail) q->tail->next = p;
    else         q->head = p;
    q->tail = p;
    q->count++;
}

Process *queue_dequeue(Queue *q)
{
    if (!q || !q->head) return NULL;
    Process *p = q->head;
    q->head = p->next;
    if (!q->head) q->tail = NULL;
    p->next = NULL;
    q->count--;
    return p;
}

Process *queue_peek(const Queue *q)
{
    return q ? q->head : NULL;
}

/* Remove a process by PID without dequeuing everything */
void queue_remove(Queue *q, int pid)
{
    if (!q || !q->head) return;
    Process *prev = NULL;
    Process *curr = q->head;
    while (curr) {
        if (curr->pid == pid) {
            if (prev) prev->next = curr->next;
            else      q->head   = curr->next;
            if (curr == q->tail) q->tail = prev;
            curr->next = NULL;
            q->count--;
            return;
        }
        prev = curr;
        curr = curr->next;
    }
}

int queue_empty(const Queue *q)
{
    return (!q || q->count == 0);
}

/* ── Priority-sorted insertion (descending dynamic_priority) ── */
void queue_enqueue_priority(Queue *q, Process *p)
{
    if (!q || !p) return;
    p->next = NULL;

    /* Find insertion point: before the first process with lower priority */
    if (!q->head || p->dynamic_priority > q->head->dynamic_priority) {
        p->next = q->head;
        q->head = p;
        if (!q->tail) q->tail = p;
        q->count++;
        return;
    }
    Process *curr = q->head;
    while (curr->next && curr->next->dynamic_priority >= p->dynamic_priority)
        curr = curr->next;
    p->next    = curr->next;
    curr->next = p;
    if (!p->next) q->tail = p;
    q->count++;
}

/* ═══════════════════════════════════════════════════
 *  Multi-Level Feedback Queue (MLFQ)
 * ═══════════════════════════════════════════════════ */

MLFQ *mlfq_create(void)
{
    MLFQ *m = (MLFQ *)calloc(1, sizeof(MLFQ));
    if (!m) { perror("mlfq_create"); exit(1); }

    int quanta[MLFQ_LEVELS] = { MLFQ_Q0, MLFQ_Q1, MLFQ_Q2, MLFQ_Q3 };
    for (int i = 0; i < MLFQ_LEVELS; i++)
        m->quanta[i] = quanta[i];
    return m;
}

void mlfq_destroy(MLFQ *m)
{
    free(m);
}

void mlfq_insert(MLFQ *m, Process *p, int level)
{
    if (!m || !p) return;
    if (level < 0)            level = 0;
    if (level >= MLFQ_LEVELS) level = MLFQ_LEVELS - 1;
    p->queue_level = level;
    queue_enqueue(&m->levels[level], p);
}

/* Dequeue from the highest-priority (lowest index) non-empty level */
Process *mlfq_pick(MLFQ *m)
{
    if (!m) return NULL;
    for (int i = 0; i < MLFQ_LEVELS; i++) {
        if (!queue_empty(&m->levels[i]))
            return queue_dequeue(&m->levels[i]);
    }
    return NULL;
}

int mlfq_empty(const MLFQ *m)
{
    if (!m) return 1;
    for (int i = 0; i < MLFQ_LEVELS; i++)
        if (!queue_empty(&m->levels[i])) return 0;
    return 1;
}

/* CPU-bound: used full quantum → demote */
void mlfq_demote(MLFQ *m, Process *p)
{
    int next_level = p->queue_level + 1;
    if (next_level >= MLFQ_LEVELS) next_level = MLFQ_LEVELS - 1;
    mlfq_insert(m, p, next_level);
}

/* Aging / I/O-bound reward → promote */
void mlfq_promote(MLFQ *m, Process *p)
{
    /* Remove from current level first */
    queue_remove(&m->levels[p->queue_level], p->pid);
    int next_level = p->queue_level - 1;
    if (next_level < 0) next_level = 0;
    mlfq_insert(m, p, next_level);
}
