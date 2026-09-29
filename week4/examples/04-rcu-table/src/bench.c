/*
 * COMP 410 - Week 4 - read-side scaling: the RCU table against a rwlock table.
 *
 * For each implementation and each reader count, every reader thread does
 * READS lookups while one writer does UPDATES updates. A run is timed from the
 * moment the readers are released until the last reader finishes; the writer's
 * own time is not in it.
 *
 * Each (impl, readers) cell does one warm-up run, discarded, then TRIALS timed
 * runs, and prints one CSV row: the median and the spread (slowest - fastest).
 * The warm-up is there because the first runs on a laptop land on a CPU that has
 * not yet raised its clock speed, and would otherwise look like a slow reader.
 *
 * There is no randomness - reader lookups cycle through the keys in order - so
 * every count in the output is exact on any machine. Only the timings are
 * machine-specific.
 *
 *   ./build/bench [trials] [updates] [reads_per_reader]
 */
#define _POSIX_C_SOURCE 200809L

#include "table.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static long reads_per_reader = 2000000;
static int  updates          = 100;

/* Other work between the writer's updates, in spin iterations, so updates are
 * spread across the run rather than packed at its start. */
#define UPDATE_SPIN 20000

typedef struct { pthread_mutex_t m; pthread_cond_t c; int go; } gate;

static void gate_wait(gate *g)
{
    pthread_mutex_lock(&g->m);
    while (!g->go)
        pthread_cond_wait(&g->c, &g->m);
    pthread_mutex_unlock(&g->m);
}

typedef struct {
    int rcu;
    rcu_table *rt;
    rw_table *wt;
    gate *g;
    long torn;
} worker;

static void *reader(void *p)
{
    worker *w = p;
    rcu_reader *me = w->rcu ? rcu_register() : NULL;
    gate_wait(w->g);
    long torn = 0, v;
    for (long j = 0; j < reads_per_reader; j++) {
        int key = (int)(j % TABLE_KEYS);
        int ok = w->rcu ? rt_lookup(w->rt, me, key, &v) : rw_lookup(w->wt, key, &v);
        torn += !ok;
    }
    w->torn = torn;
    if (me)
        rcu_unregister(me);
    return NULL;
}

static void *writer(void *p)
{
    worker *w = p;
    gate_wait(w->g);
    for (int u = 0; u < updates; u++) {
        if (w->rcu)
            rt_update(w->rt);
        else
            rw_update(w->wt);
        for (volatile int s = 0; s < UPDATE_SPIN; s++)
            ;
    }
    return NULL;
}

static double now_s(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

/* One timed run. Returns seconds; adds to *torn and sets *freed. */
static double run_once(int rcu, int nreaders, long *torn, long *freed)
{
    rcu_table rt;
    rw_table wt;
    if (rcu ? rt_init(&rt) : rw_init(&wt)) {
        fprintf(stderr, "bench: out of memory\n");
        exit(1);
    }
    gate g = { PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, 0 };
    worker w[9];
    pthread_t th[9];
    long base = rt_freed();

    for (int i = 0; i <= nreaders; i++) {
        w[i] = (worker){ rcu, &rt, &wt, &g, 0 };
        pthread_create(&th[i], NULL, i < nreaders ? reader : writer, &w[i]);
    }
    /* Let every reader register and block on the gate before timing starts. */
    struct timespec pause = { 0, 20 * 1000000L };
    nanosleep(&pause, NULL);

    pthread_mutex_lock(&g.m);
    g.go = 1;
    double t0 = now_s();
    pthread_cond_broadcast(&g.c);
    pthread_mutex_unlock(&g.m);

    for (int i = 0; i < nreaders; i++) {
        pthread_join(th[i], NULL);
        *torn += w[i].torn;
    }
    double t1 = now_s();
    pthread_join(th[nreaders], NULL);

    *freed = rcu ? rt_freed() - base : 0;
    if (rcu)
        rt_destroy(&rt);
    else
        rw_destroy(&wt);
    return t1 - t0;
}

static int cmp_double(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

int main(int argc, char **argv)
{
    int trials = argc > 1 ? atoi(argv[1]) : 5;
    if (argc > 2)
        updates = atoi(argv[2]);
    if (argc > 3)
        reads_per_reader = atol(argv[3]);
    if (trials < 1 || trials > 99 || updates < 0 || reads_per_reader < 1) {
        fprintf(stderr, "usage: %s [trials 1-99] [updates] [reads_per_reader]\n", argv[0]);
        return 2;
    }

    static const int levels[] = { 1, 2, 4, 8 };
    static const char *impl[] = { "rcu", "rwlock" };

    printf("impl,readers,trials,reads_per_reader,reads_total,updates,torn_reads,"
           "freed_versions,median_s,spread_s,throughput_reads_s\n");
    for (int m = 0; m < 2; m++) {
        int rcu = m == 0;
        for (size_t l = 0; l < sizeof levels / sizeof levels[0]; l++) {
            int n = levels[l];
            double t[99];
            long torn = 0, freed = 0, freed_min = -1, warm_torn = 0, warm_freed = 0;

            run_once(rcu, n, &warm_torn, &warm_freed);          /* warm-up, discarded */
            torn += warm_torn;                                  /* ...but a torn read still counts */

            for (int i = 0; i < trials; i++) {
                t[i] = run_once(rcu, n, &torn, &freed);
                if (freed_min < 0 || freed < freed_min)
                    freed_min = freed;
            }
            qsort(t, (size_t)trials, sizeof t[0], cmp_double);
            double median = trials % 2 ? t[trials / 2] : (t[trials / 2 - 1] + t[trials / 2]) / 2;
            double spread = t[trials - 1] - t[0];
            long total = (long)n * reads_per_reader;
            printf("%s,%d,%d,%ld,%ld,%d,%ld,%ld,%.6f,%.6f,%.0f\n", impl[m], n, trials,
                   reads_per_reader, total, updates, torn, freed_min, median, spread,
                   total / median);
            fflush(stdout);
        }
    }
    return 0;
}
