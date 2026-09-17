/*
 * COMP 410 - Week 3 - contention benchmark (STARTER).
 *
 * Runs the same producer/consumer workload against both queues at a range of
 * thread counts and prints one CSV row per (queue, threads, trial).
 *
 * Reports the MEDIAN of several trials, and the spread, because a single timing
 * on a shared machine says very little: scheduling, frequency scaling and cache
 * state all move the number.
 */
#define _POSIX_C_SOURCE 200809L

#include "msqueue.h"
#include "mutexqueue.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define OPS_PER_THREAD 20000

/* "Other work" between queue operations, in spin iterations.
 *
 * Michael & Scott put ~6us of spinning between every enqueue and dequeue
 * (§4, PDF p. 6), deliberately: back-to-back queue operations give
 * "overly-optimistic performance due to an unrealistically low cache miss
 * rate". Their words. Zero here reproduces the naive benchmark; a non-zero
 * value reproduces theirs, and the two give OPPOSITE answers.
 *
 *     ./bench 5 0      back-to-back  -> the mutex wins
 *     ./bench 5 2000   with work     -> the lock-free queue wins at 8 threads
 */
static int other_work = 0;

static void do_other_work(void)
{
    for (volatile int s = 0; s < other_work; s++)
        ;
}

typedef struct {
    void *queue;
    int   lockfree;
    long  ops;
} work;

static double now_s(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static void *worker(void *arg)
{
    work *w = arg;
    long out;
    for (long i = 0; i < w->ops; i++) {
        if (w->lockfree) {
            msq_enqueue(w->queue, i);
            do_other_work();
            msq_dequeue(w->queue, &out);
            do_other_work();
        } else {
            mq_enqueue(w->queue, i);
            do_other_work();
            mq_dequeue(w->queue, &out);
            do_other_work();
        }
    }
    return NULL;
}

/* One timed run. Returns seconds. */
static double run_once(int lockfree, int nthreads)
{
    void *q = lockfree ? (void *)msq_create() : (void *)mq_create();
    if (!q)
        return -1.0;

    pthread_t *th = calloc(nthreads, sizeof *th);
    work *w = calloc(nthreads, sizeof *w);
    if (!th || !w) {
        free(th); free(w);
        return -1.0;
    }

    double t0 = now_s();
    for (int i = 0; i < nthreads; i++) {
        w[i] = (work){ q, lockfree, OPS_PER_THREAD };
        pthread_create(&th[i], NULL, worker, &w[i]);
    }
    for (int i = 0; i < nthreads; i++)
        pthread_join(th[i], NULL);
    double t1 = now_s();

    if (lockfree) msq_destroy(q); else mq_destroy(q);
    free(th); free(w);
    return t1 - t0;
}

static int cmp_double(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

int main(int argc, char **argv)
{
    int trials = 5;
    if (argc > 1)
        trials = atoi(argv[1]);
    if (trials < 1)
        trials = 1;
    if (argc > 2)
        other_work = atoi(argv[2]);
    if (other_work < 0)
        other_work = 0;
    fprintf(stderr, "trials=%d other_work=%d spins\n", trials, other_work);

    const int threads[] = { 1, 2, 4, 8 };
    const int nsteps = (int)(sizeof threads / sizeof threads[0]);

    printf("queue,threads,other_work,ops_total,median_s,spread_s,throughput_ops_s\n");
    for (int lf = 1; lf >= 0; lf--) {
        for (int s = 0; s < nsteps; s++) {
            int n = threads[s];
            double *t = calloc(trials, sizeof *t);
            for (int k = 0; k < trials; k++)
                t[k] = run_once(lf, n);
            qsort(t, trials, sizeof *t, cmp_double);
            double med = t[trials / 2];
            double spread = t[trials - 1] - t[0];
            /* Each op is an enqueue AND a dequeue. */
            double total = (double)n * OPS_PER_THREAD * 2.0;
            printf("%s,%d,%d,%.0f,%.6f,%.6f,%.0f\n",
                   lf ? "lockfree" : "mutex", n, other_work, total, med, spread,
                   med > 0 ? total / med : 0);
            free(t);
        }
    }
    return 0;
}
