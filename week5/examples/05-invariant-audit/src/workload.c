/*
 * COMP 410 — Week 5 — A5: the seeded workload generator (PROVIDED).
 *
 * Complete: you do not change it — see workload.h.
 *
 * Architecture: a dispatcher (the calling thread) releases WL_TASKS tasks,
 * each timestamped the instant it is ready. Workers pull from one or two
 * bounded queues, timestamp the instant they begin a task, stay occupied for the
 * task's service time (serve, below), and timestamp the end.
 *
 * Every task has a class (task_id % 4) and a service time proportional to
 * your derived size for that class, scaled so the mean over the four classes
 * is WL_MEAN_SERVICE_US. Class 3, the largest, is labeled shard 0 and the
 * other three shard 1, under every pathology.
 *
 * Exactly one of three behaviors is active, chosen by `seed % 3`
 * (select_pathology, below). Reading it tells you the label; it does not give
 * you the measurements the handout grades.
 *
 *   BLOCKED-HOLDER    — one shared queue, all workers. Every
 *                       WL_HOLDER_PERIODth task takes a gate every task must
 *                       pass before it starts, then sleeps WL_HOLD_US while
 *                       holding it. Workers that pop a task during the hold
 *                       block on the gate: tasks are waiting AND workers are
 *                       idle, because of a lock, not because of load.
 *   BURST-ARRIVAL     — one shared queue, all workers. The dispatcher paces
 *                       tasks evenly, except every WL_BURST_PERIODth task it
 *                       releases WL_BURST_SIZE tasks back to back. Tasks wait
 *                       because every worker is busy: plain queueing.
 *   UNEVEN-PARTITION  — two queues, partitioned workers and no stealing:
 *                       shard 0 (class 3, the heaviest) gets ONE worker,
 *                       shard 1 the other workers - 1. Shard 0 is offered 1.2x
 *                       what one worker can serve, so its queue backs up while
 *                       shard 1's workers sit idle.
 *
 * Return values: every call that can fail for a reason outside this file
 * (allocation, thread and synchronization-object creation, joins) is checked.
 * pthread_mutex_lock/unlock and pthread_cond_wait/signal/broadcast are not:
 * on the correctly initialized default mutexes used here they can fail only
 * through a programming error in this file (EINVAL), not at run time.
 */
#if defined(__APPLE__)
/* CLOCK_MONOTONIC on macOS ticks in whole microseconds; the pool's hand-off
 * latencies are a few microseconds, so they would all round to 1-4 us.
 * CLOCK_MONOTONIC_RAW ticks in ~42 ns, but macOS declares it only outside
 * strict POSIX mode, which is what _DARWIN_C_SOURCE turns on. */
#define _DARWIN_C_SOURCE
#endif
#define _POSIX_C_SOURCE 200809L

#include "workload.h"
#include "../config/params.h"

#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#if defined(__APPLE__) && defined(CLOCK_MONOTONIC_RAW)
#define WL_CLOCK CLOCK_MONOTONIC_RAW
#else
#define WL_CLOCK CLOCK_MONOTONIC
#endif

static void die(const char *what, int err) {
    fprintf(stderr, "wl_run: %s failed: %s\n", what, strerror(err));
    exit(1);
}

/* ---------------------------------------------------------------------- */
/* A small bounded queue of task ids.                                     */
/* ---------------------------------------------------------------------- */

typedef struct {
    unsigned       *slot;
    unsigned        capacity, head, tail, count;
    int             closed;
    pthread_mutex_t lock;
    pthread_cond_t  not_empty, not_full;
} taskq;

static void tq_init(taskq *q, unsigned capacity) {
    int rc;
    memset(q, 0, sizeof *q);
    q->slot = calloc(capacity, sizeof *q->slot);
    if (q->slot == NULL) {
        die("calloc (queue slots)", ENOMEM);
    }
    q->capacity = capacity;
    if ((rc = pthread_mutex_init(&q->lock, NULL)) != 0) {
        die("pthread_mutex_init", rc);
    }
    if ((rc = pthread_cond_init(&q->not_empty, NULL)) != 0) {
        die("pthread_cond_init", rc);
    }
    if ((rc = pthread_cond_init(&q->not_full, NULL)) != 0) {
        die("pthread_cond_init", rc);
    }
}

static void tq_destroy(taskq *q) {
    pthread_mutex_destroy(&q->lock);
    pthread_cond_destroy(&q->not_empty);
    pthread_cond_destroy(&q->not_full);
    free(q->slot);
}

static void tq_push(taskq *q, unsigned task_id) {
    pthread_mutex_lock(&q->lock);
    while (q->count == q->capacity) {
        pthread_cond_wait(&q->not_full, &q->lock);
    }
    q->slot[q->tail] = task_id;
    q->tail = (q->tail + 1) % q->capacity;
    q->count++;
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->lock);
}

/* 0 with *out set, or -1 once closed and drained. */
static int tq_pop(taskq *q, unsigned *out) {
    pthread_mutex_lock(&q->lock);
    while (q->count == 0 && !q->closed) {
        pthread_cond_wait(&q->not_empty, &q->lock);
    }
    if (q->count == 0) {
        pthread_mutex_unlock(&q->lock);
        return -1;
    }
    *out = q->slot[q->head];
    q->head = (q->head + 1) % q->capacity;
    q->count--;
    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->lock);
    return 0;
}

static void tq_close(taskq *q) {
    pthread_mutex_lock(&q->lock);
    q->closed = 1;
    pthread_cond_broadcast(&q->not_empty);
    pthread_cond_broadcast(&q->not_full);
    pthread_mutex_unlock(&q->lock);
}

/* ---------------------------------------------------------------------- */

uint64_t wl_now_ns(void) {
    struct timespec ts;
    if (clock_gettime(WL_CLOCK, &ts) != 0) {
        die("clock_gettime", errno);
    }
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

/* Sleep for `ns`, resuming after a signal interrupts it. nanosleep, not
 * usleep: usleep was removed from POSIX.1-2008, and glibc does not declare it
 * under _POSIX_C_SOURCE 200809L. */
static void sleep_ns(uint64_t ns) {
    struct timespec ts = { .tv_sec = (time_t)(ns / 1000000000ull),
                           .tv_nsec = (long)(ns % 1000000000ull) };
    while (nanosleep(&ts, &ts) != 0 && errno == EINTR) {
    }
}

/* Stay off the CPU for `ns`, accurately. A single nanosleep of a few
 * milliseconds overshoots by 30-50% on both the macOS host and the course
 * guest (measured: 8 ms asked, 10.4-11.3 ms slept), so the hold sleeps half
 * of what remains, again and again, and spins only the last 0.4 ms. */
static void hold_for(uint64_t ns) {
    uint64_t end = wl_now_ns() + ns;
    for (;;) {
        uint64_t now = wl_now_ns();
        if (now >= end) {
            break;
        }
        if (end - now > 400000u) {
            sleep_ns((end - now) / 2);
        }
    }
}

/* Keeps the optimizer from deleting the busy loop below. */
static _Atomic unsigned long g_sink;

/* Keep this worker busy for `us` of wall-clock time, on the CPU. Timed by the
 * clock, not by an iteration count: an iteration count takes a different time
 * on every machine, and the pacing below depends on service times being what
 * they say. Sleeping instead would overshoot by 50-100 us per task and raise
 * the load by a different amount on every machine. */
static void serve(double us) {
    uint64_t end = wl_now_ns() + (uint64_t)(us * 1000.0);
    unsigned long acc = 1;
    while (wl_now_ns() < end) {
        acc = acc * 1664525u + 1013904223u;
    }
    g_sink += acc;
}

static wl_pathology select_pathology(uint64_t seed) {
    return (wl_pathology)(seed % 3u);
}

/*
 * Pacing. The dispatcher releases tasks on a fixed schedule (absolute
 * deadlines, so a late wake-up is made up on the next one rather than
 * accumulating). The gap between releases stays far above the platform's
 * shortest sleep (about 70 us on the course guest): below it, the dispatcher
 * falls behind and delivers less load than it asks for. With a 300 us mean
 * service time the gaps below are 136-270 us, and pace_workers keeps them
 * there.
 */
#define WL_TARGET_RHO        0.55  /* shared-queue pathologies: pool utilization */
#define WL_SHARD0_OVERLOAD   1.2   /* uneven-partition: shard 0's one worker */
#define WL_PACE_MAX_WORKERS  4u    /* pace as if at most 4 workers serve */

/* How many workers the pool actually runs: your derived count, but never more
 * than this machine's CPUs minus one (the dispatcher needs one). With more
 * busy workers than CPUs, workers wait for a real CPU and the KERNEL's
 * run-queue latency leaks into the pool's numbers (2-4 ms per burst on a
 * 4-vCPU guest). The CSV's `workers` column is the count the pool used. */
unsigned wl_pool_workers(const wl_config *cfg) {
    unsigned n = cfg->workers;
    long online = sysconf(_SC_NPROCESSORS_ONLN);
    if (online > 1 && (unsigned long)(online - 1) < n) {
        n = (unsigned)(online - 1);
    }
    return n < WL_MIN_WORKERS ? WL_MIN_WORKERS : n;
}

/* Pacing by the worker count alone would make the gap 68 us at 8 workers,
 * below the course guest's ~70 us shortest sleep: the dispatcher would fall
 * behind again. So the offered load stops growing at 4 workers; a larger
 * pool is simply less busy. */
static unsigned pace_workers(unsigned pool) {
    return pool < WL_PACE_MAX_WORKERS ? pool : WL_PACE_MAX_WORKERS;
}

static double dispatch_gap_us(unsigned pool, wl_pathology path,
                              const double service_us[WL_NUM_SIZES]) {
    if (path == WL_PATH_UNEVEN_PARTITION) {
        /* shard 0 receives one task in four; hold its one worker at 1.2x */
        return service_us[3] / (4.0 * WL_SHARD0_OVERLOAD);
    }
    return WL_MEAN_SERVICE_US / ((double)pace_workers(pool) * WL_TARGET_RHO);
}

typedef struct {
    taskq              *q;
    wl_record          *records;
    pthread_mutex_t    *holder_gate;
    _Atomic unsigned   *completed;
} worker_args;

static void *worker_main(void *arg) {
    worker_args *wa = arg;
    unsigned task_id;
    while (tq_pop(wa->q, &task_id) == 0) {
        wl_record *r = &wa->records[task_id];
        /* Every task passes the gate before it is timed as started. Only a
         * holder task ever holds it longer than a lock/unlock. */
        pthread_mutex_lock(wa->holder_gate);
        pthread_mutex_unlock(wa->holder_gate);
        r->start_ns = wl_now_ns();
        if (r->is_holder) {
            pthread_mutex_lock(wa->holder_gate);
            hold_for((uint64_t)WL_HOLD_US * 1000u);
            pthread_mutex_unlock(wa->holder_gate);
        } else {
            serve(r->service_us);
        }
        r->end_ns = wl_now_ns();
        atomic_fetch_add(wa->completed, 1u);
    }
    return NULL;
}

static void dispatch_one(wl_pathology path, unsigned task_id, const double service_us[],
                         wl_record *records, taskq qs[2]) {
    wl_record *r = &records[task_id];
    unsigned cls = task_id % WL_NUM_SIZES;
    r->task_id    = task_id;
    r->shard      = (cls == WL_NUM_SIZES - 1) ? 0u : 1u;
    r->service_us = service_us[cls];
    r->is_holder  = (path == WL_PATH_BLOCKED_HOLDER) && (task_id % WL_HOLDER_PERIOD == 0);
    r->start_ns   = 0;
    r->end_ns     = 0;
    /* Recorded BEFORE the push, which may block on a full queue: waiting for a
     * slot is as much "not yet running" as waiting inside the queue. */
    r->ready_ns   = wl_now_ns();
    /* Only uneven-partition has two real queues; the other two run every
     * task through queue 0 and `shard` is a label. */
    tq_push(&qs[path == WL_PATH_UNEVEN_PARTITION ? r->shard : 0u], task_id);
}

static int cmp_double(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

void wl_default_config(wl_config *cfg) {
    cfg->seed    = WL_SEED;
    cfg->workers = (WL_WORKERS < WL_MIN_WORKERS) ? WL_MIN_WORKERS : WL_WORKERS;
    cfg->sizes[0] = WL_SIZE_0;
    cfg->sizes[1] = WL_SIZE_1;
    cfg->sizes[2] = WL_SIZE_2;
    cfg->sizes[3] = WL_SIZE_3;
    cfg->warmup_runs = WL_REPEATS;
}

void wl_run(const wl_config *cfg, wl_result *out) {
    wl_pathology path = select_pathology(cfg->seed);
    int rc;

    double size_sum = 0.0, service_us[WL_NUM_SIZES];
    for (unsigned c = 0; c < WL_NUM_SIZES; c++) {
        size_sum += (double)cfg->sizes[c];
    }
    for (unsigned c = 0; c < WL_NUM_SIZES; c++) {
        service_us[c] = WL_MEAN_SERVICE_US * WL_NUM_SIZES * (double)cfg->sizes[c] / size_sum;
    }

    unsigned pool = wl_pool_workers(cfg);
    unsigned shard_workers[2];
    if (path == WL_PATH_UNEVEN_PARTITION) {
        shard_workers[0] = 1;
        shard_workers[1] = pool - 1;
    } else {
        shard_workers[0] = pool;           /* one shared queue */
        shard_workers[1] = 0;
    }

    taskq qs[2];
    tq_init(&qs[0], WL_CAPACITY);
    tq_init(&qs[1], WL_CAPACITY);

    pthread_mutex_t holder_gate;
    if ((rc = pthread_mutex_init(&holder_gate, NULL)) != 0) {
        die("pthread_mutex_init", rc);
    }
    _Atomic unsigned completed = 0;

    unsigned total_workers = shard_workers[0] + shard_workers[1];
    pthread_t    *th   = calloc(total_workers, sizeof *th);
    worker_args  *wa   = calloc(total_workers, sizeof *wa);
    double       *late = calloc(WL_TASKS, sizeof *late);
    if (th == NULL || wa == NULL || late == NULL) {
        die("calloc (workers)", ENOMEM);
    }

    unsigned wi = 0;
    for (unsigned s = 0; s < 2; s++) {
        for (unsigned k = 0; k < shard_workers[s]; k++) {
            wa[wi].q           = &qs[s];
            wa[wi].records     = out->records;
            wa[wi].holder_gate = &holder_gate;
            wa[wi].completed   = &completed;
            if ((rc = pthread_create(&th[wi], NULL, worker_main, &wa[wi])) != 0) {
                die("pthread_create", rc);
            }
            wi++;
        }
    }

    uint64_t gap_ns = (uint64_t)(dispatch_gap_us(pool, path, service_us) * 1000.0);
    uint64_t deadline = wl_now_ns();
    unsigned n_late = 0;
    unsigned i = 0;
    while (i < WL_TASKS) {
        if (path == WL_PATH_BURST_ARRIVAL && i % WL_BURST_PERIOD == 0) {
            unsigned end = i + WL_BURST_SIZE;
            if (end > WL_TASKS) {
                end = WL_TASKS;
            }
            for (; i < end; i++) {
                dispatch_one(path, i, service_us, out->records, qs);   /* no gap: the burst */
            }
            deadline = wl_now_ns();
            continue;
        }
        deadline += gap_ns;
        uint64_t now = wl_now_ns();
        if (now < deadline) {
            sleep_ns(deadline - now);
            now = wl_now_ns();
        }
        late[n_late++] = (double)(now - deadline) / 1000.0;
        /* A dispatcher that fell far behind (blocked on a full queue during a
         * hold) starts a fresh schedule instead of releasing its whole debt
         * at once, which would be a burst this pathology does not have. */
        if (now > deadline + 2 * gap_ns) {
            deadline = now;
        }
        dispatch_one(path, i, service_us, out->records, qs);
        i++;
    }

    tq_close(&qs[0]);
    tq_close(&qs[1]);
    for (unsigned k = 0; k < total_workers; k++) {
        if ((rc = pthread_join(th[k], NULL)) != 0) {
            die("pthread_join", rc);
        }
    }

    qsort(late, n_late, sizeof *late, cmp_double);
    out->pace_late_us = n_late ? late[n_late / 2] : 0.0;
    out->queued       = WL_TASKS;
    out->completed    = completed;
    out->workers      = pool;
    out->shard0_count = 0;
    out->shard1_count = 0;
    for (unsigned k = 0; k < WL_TASKS; k++) {
        if (out->records[k].shard == 0) {
            out->shard0_count++;
        } else {
            out->shard1_count++;
        }
    }

    free(th);
    free(wa);
    free(late);
    pthread_mutex_destroy(&holder_gate);
    tq_destroy(&qs[0]);
    tq_destroy(&qs[1]);
}
