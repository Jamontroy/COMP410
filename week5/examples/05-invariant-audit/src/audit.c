/*
 * COMP 410 — Week 5 — A5: the audit harness (PROVIDED).
 *
 * Runs your seeded workload WL_REPEATS times as a discarded warm-up, then
 * N_RUNS times for real, and writes one row per real run to
 * bench/audit.csv: the columns are wl_summary's fields (src/analyze.h),
 * plus the run number, the task count and the dispatcher's own lateness.
 *
 * Usage: ./build/audit [n_runs]      (default N_RUNS below)
 */
#define _POSIX_C_SOURCE 200809L

#include "analyze.h"
#include "workload.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#define N_RUNS 25

int main(int argc, char **argv) {
    long n_runs = N_RUNS;
    if (argc > 1) {
        char *end = NULL;
        n_runs = strtol(argv[1], &end, 10);
        if (end == argv[1] || *end != '\0') {
            fprintf(stderr, "audit: '%s' is not a number of runs\n", argv[1]);
            return 1;
        }
    }
    if (n_runs < 20 || n_runs > 1000) {
        fprintf(stderr,
                "audit: refusing to run %ld time(s) -- the handout asks for at "
                "least 20. `make bench` runs the default (%d) unless you override it, "
                "and you should have a reason to.\n", n_runs, N_RUNS);
        return 1;
    }

    wl_config cfg;
    wl_default_config(&cfg);
    fprintf(stderr,
            "seed %" PRIu64 "  workers %u (the pool runs %u here)  sizes %u,%u,%u,%u"
            "  warmup %u  runs %ld  (about a second per run)\n",
            cfg.seed, cfg.workers, wl_pool_workers(&cfg), cfg.sizes[0], cfg.sizes[1], cfg.sizes[2],
            cfg.sizes[3], cfg.warmup_runs, n_runs);

    wl_record *records = calloc(WL_TASKS, sizeof *records);
    if (records == NULL) {
        fprintf(stderr, "audit: out of memory\n");
        return 1;
    }
    wl_result run = { .records = records };

    for (unsigned i = 0; i < cfg.warmup_runs; i++) {
        wl_run(&cfg, &run);
    }

    printf("run,seed,workers,tasks,queued,completed,started,shard0_count,shard1_count,"
           "median_latency_us,spread_latency_us,max_latency_us,bug_count,bug_fraction,"
           "shard0_median_latency_us,shard1_median_latency_us,"
           "window_us,violation_us,episodes,span_us,violation_fraction,pace_late_us\n");

    for (long i = 0; i < n_runs; i++) {
        wl_run(&cfg, &run);
        if (run.completed != run.queued) {
            fprintf(stderr,
                    "audit: run %ld completed %u of %u queued tasks -- the substrate "
                    "lost work. That is a defect in workload.c, not in your "
                    "analyze.c; it should not happen with the file as shipped.\n",
                    i, run.completed, run.queued);
        }
        wl_summary s;
        summarize_run(&run, &s);
        printf("%ld,%" PRIu64 ",%u,%u,%u,%u,%u,%u,%u,%.3f,%.3f,%.3f,%u,%.6f,%.3f,%.3f,"
               "%.3f,%.3f,%u,%.3f,%.6f,%.3f\n",
               i, cfg.seed, run.workers, WL_TASKS, s.queued, s.completed, s.started,
               s.shard0_count, s.shard1_count,
               s.median_latency_us, s.spread_latency_us, s.max_latency_us,
               s.bug_count, s.bug_fraction,
               s.shard0_median_latency_us, s.shard1_median_latency_us,
               s.window_us, s.violation_us, s.episodes, s.span_us,
               s.violation_fraction, run.pace_late_us);
    }

    free(records);
    return 0;
}
