/*
 * COMP 410 - Week 3 - correctness tests (STARTER).
 *
 * A lock-free structure that is merely FAST is worthless. These check the
 * properties that make it a queue at all, including under contention, where
 * the interesting failures live.
 */
#define _POSIX_C_SOURCE 200809L

#include "msqueue.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int pass = 0, fail = 0;

static void ok(const char *what)  { pass++; printf("  [ok]   %s\n", what); }
static void bad(const char *what) { fail++; printf("  [FAIL] %s\n", what); }
static void check(int cond, const char *what) { cond ? ok(what) : bad(what); }

#define NTHREADS 4
#define PER_THREAD 5000

static msqueue *shared;

static void *producer(void *arg)
{
    long base = (long)(intptr_t)arg * PER_THREAD;
    for (long i = 0; i < PER_THREAD; i++)
        msq_enqueue(shared, base + i);
    return NULL;
}

int main(void)
{
    printf("== a queue is FIFO ==\n");
    msqueue *q = msq_create();
    check(q != NULL, "create returns a queue");

    long out = -1;
    check(!msq_dequeue(q, &out), "dequeue on an empty queue returns false");

    for (long i = 1; i <= 3; i++)
        msq_enqueue(q, i);
    int order_ok = 1;
    for (long i = 1; i <= 3; i++) {
        if (!msq_dequeue(q, &out) || out != i)
            order_ok = 0;
    }
    check(order_ok, "three values come back in the order they went in");
    check(!msq_dequeue(q, &out), "the queue is empty again afterwards");
    msq_destroy(q);

    printf("\n== it survives contention ==\n");
    shared = msq_create();
    pthread_t th[NTHREADS];
    for (long i = 0; i < NTHREADS; i++)
        pthread_create(&th[i], NULL, producer, (void *)(intptr_t)i);
    for (int i = 0; i < NTHREADS; i++)
        pthread_join(th[i], NULL);

    /* Every value enqueued must come back exactly once: none lost, none
     * duplicated. A duplicate is the classic symptom of a CAS that succeeded
     * when it should not have. */
    const long total = (long)NTHREADS * PER_THREAD;
    char *seen = calloc(total, 1);
    long got = 0, dup = 0, oob = 0;
    while (msq_dequeue(shared, &out)) {
        got++;
        if (out < 0 || out >= total) { oob++; continue; }
        if (seen[out]) dup++;
        seen[out] = 1;
    }
    check(got == total, "every enqueued value is dequeued exactly once (count)");
    check(dup == 0, "no value is dequeued twice");
    check(oob == 0, "no value appears that was never enqueued");
    if (got != total)
        printf("         expected %ld, got %ld\n", total, got);

    printf("\n  retries under contention: %lu\n", msq_retries(shared));
    free(seen);
    msq_destroy(shared);

    printf("\n  %d passed, %d failed\n", pass, fail);
    return fail ? 1 : 0;
}
