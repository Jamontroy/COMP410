/*
 * COMP 410 — Week 5 — A5: the seeded workload generator (PROVIDED).
 *
 * This file, and workload.c, are complete: you do not change them, and they
 * are not the assessed part. A5 asks you to
 * MEASURE and DIAGNOSE a thread pool that is handed to you working, the way
 * you would a service you did not write yourself.
 *
 * The pool is a user-space model of a scheduler. A worker thread plays the
 * part of a core; a task waiting in a queue plays the part of a runnable
 * thread on a run queue. wl_run() executes ONE pass of your own seeded
 * workload: WL_TASKS tasks, each with a service time, handed to the pool's
 * workers (wl_pool_workers). Exactly one of three pathologies is active, chosen by your derived
 * seed (config/params.h, written by `make params`). Which one is never
 * printed by this program: finding out is the assignment.
 *
 * What wl_run() gives you, per task, is three timestamps: when the task
 * became ready (like the kernel's sched:sched_wakeup), when a worker began
 * it (like sched:sched_switch picking it) and when it finished. The gap
 * between the first two is the task's hand-off latency in THIS pool: the
 * pool's analog of run-queue latency, not the kernel's. `bpftrace` on the
 * real sched: tracepoints measures the kernel's (systems mini-reference 5.6),
 * and the two need not agree.
 */
#ifndef WL_WORKLOAD_H
#define WL_WORKLOAD_H

#include <stddef.h>
#include <stdint.h>

/* Fixed for every student, so a run is comparable across the whole class. */
#define WL_TASKS            4000u
#define WL_NUM_SIZES        4       /* task classes; class = task_id % 4 */
#define WL_CAPACITY         64u     /* capacity of each queue */
#define WL_MEAN_SERVICE_US  300.0   /* mean service time over the four classes */
#define WL_HOLDER_PERIOD    400u    /* blocked-holder: every 400th task holds the gate */
#define WL_HOLD_US          10000u  /* ... for this long, off the CPU */
#define WL_BURST_PERIOD     150u    /* burst-arrival: a burst starts every 150th task */
#define WL_BURST_SIZE       30u     /* ... and releases this many tasks with no gap */
#define WL_MIN_WORKERS      3u      /* config/params.h is clamped to at least this */

typedef enum {
    WL_PATH_BLOCKED_HOLDER   = 0,
    WL_PATH_BURST_ARRIVAL    = 1,
    WL_PATH_UNEVEN_PARTITION = 2
} wl_pathology;

typedef struct {
    uint64_t seed;                  /* derive(netid, "A5") -- see tools/gen-params.py.
                                      * Up to 48 bits: too wide for `unsigned`, and a
                                      * truncated seed no longer matches your netid's. */
    unsigned workers;               /* worker threads in the pool, >= 3 */
    unsigned sizes[WL_NUM_SIZES];   /* relative task sizes, your own mix (sorted) */
    unsigned warmup_runs;           /* discarded runs before the measured ones */
} wl_config;

typedef struct {
    unsigned task_id;
    unsigned shard;              /* 0 for the largest class (task_id % 4 == 3),
                                  * 1 for the other three -- the same rule under
                                  * every pathology, so shard0_count is always
                                  * WL_TASKS / 4 */
    double   service_us;         /* how long this task keeps its worker busy */
    int      is_holder;          /* 1 for a gate-holding task (blocked-holder only) */
    uint64_t ready_ns;           /* the task became ready (queued, or waiting to be) */
    uint64_t start_ns;           /* a worker began it; 0 if it never started */
    uint64_t end_ns;             /* the worker finished it; 0 if it never finished */
} wl_record;

typedef struct {
    unsigned   queued;
    unsigned   completed;
    unsigned   shard0_count, shard1_count;
    unsigned   workers;          /* the pool size this run used: wl_pool_workers */
    double     pace_late_us;     /* median lateness of the paced dispatches: how
                                  * far behind its own schedule the dispatcher
                                  * ran. Near 0 means the load you asked for is
                                  * the load you got. */
    wl_record *records;          /* caller-owned, at least WL_TASKS entries */
} wl_result;

/* Fills *cfg from config/params.h (WL_SEED / WL_WORKERS / WL_SIZE_0..3 /
 * WL_REPEATS), clamping workers to WL_MIN_WORKERS. */
void wl_default_config(wl_config *cfg);

/* The number of workers the pool runs: cfg->workers, capped at this
 * machine's CPUs minus one (see workload.c for why), never below 3. */
unsigned wl_pool_workers(const wl_config *cfg);

/* Runs one full pass of the workload. out->records must point at a caller-
 * allocated array of at least WL_TASKS entries. On return, records[i] is the
 * record of task i (indexed by task_id, NOT by completion order), and a task
 * that never started has start_ns == 0. */
void wl_run(const wl_config *cfg, wl_result *out);

/* Monotonic time in nanoseconds, from the finest clock this platform offers
 * (see workload.c). The pool's timestamps all come from here. */
uint64_t wl_now_ns(void);

#endif /* WL_WORKLOAD_H */
