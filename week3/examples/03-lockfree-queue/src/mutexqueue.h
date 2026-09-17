/*
 * COMP 410 - Week 3 - mutex-protected queue (STARTER).
 *
 * The BASELINE. A claim that a lock-free structure is faster means nothing
 * without the thing it is faster than, measured on the same machine, in the
 * same harness, on the same workload.
 */
#ifndef MUTEXQUEUE_H
#define MUTEXQUEUE_H

#include <stdbool.h>

typedef struct mqueue mqueue;

mqueue *mq_create(void);
void    mq_destroy(mqueue *q);
bool    mq_enqueue(mqueue *q, long value);
bool    mq_dequeue(mqueue *q, long *out);

#endif /* MUTEXQUEUE_H */
