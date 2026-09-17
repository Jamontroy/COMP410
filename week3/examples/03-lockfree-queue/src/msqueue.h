/*
 * COMP 410 - Week 3 - Michael & Scott lock-free queue (STARTER).
 *
 * A multi-producer, multi-consumer FIFO queue built from C11 atomics, following
 * Michael & Scott (PODC 1996). Two properties matter and are easy to confuse:
 *
 *   lock-free  - some thread always makes progress. A thread descheduled mid
 *                operation cannot block the others, which is what a mutex
 *                cannot promise.
 *   wait-free  - EVERY thread finishes in a bounded number of steps. This queue
 *                is NOT wait-free: a thread can retry its CAS indefinitely
 *                while others succeed.
 */
#ifndef MSQUEUE_H
#define MSQUEUE_H

#include <stdbool.h>
#include <stddef.h>

typedef struct msqueue msqueue;

/* Create an empty queue, or NULL on allocation failure. */
msqueue *msq_create(void);

/* Free the queue and any nodes still in it. Not safe against concurrent use. */
void msq_destroy(msqueue *q);

/* Append a value. Returns false only on allocation failure. */
bool msq_enqueue(msqueue *q, long value);

/* Remove the oldest value into *out. Returns false if the queue was empty. */
bool msq_dequeue(msqueue *q, long *out);

/* Total successful CAS retries, for the contention study. Not synchronized:
 * read it after the threads have joined. */
unsigned long msq_retries(const msqueue *q);

#endif /* MSQUEUE_H */
