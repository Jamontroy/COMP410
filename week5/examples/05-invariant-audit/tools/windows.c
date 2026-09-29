/*
 * COMP 410 — Week 5 — A5: `make windows` (PROVIDED).
 *
 * The evidence for choosing your persistence window (handout §3.2). Runs your
 * workload a few times and applies YOUR invariant_scan with a range of
 * windows, each a multiple of that run's median hand-off latency. It prints
 * how many episodes survive each window and what fraction of the run they
 * cover. Where the counts stop moving as the window grows is where normal
 * hand-offs end and something else begins. It recommends nothing.
 *
 * Usage: ./build/windows [runs]      (default 5)
 */
#define _POSIX_C_SOURCE 200809L

#include "analyze.h"
#include "workload.h"

#include <stdio.h>
#include <stdlib.h>

static const double MULTIPLES[] = { 1, 3, 10, 30, 100, 300, 1000 };
#define N_MULT (sizeof MULTIPLES / sizeof MULTIPLES[0])

int main(int argc, char **argv) {
    long runs = 5;
    if (argc > 1) {
        char *end = NULL;
        runs = strtol(argv[1], &end, 10);
        if (end == argv[1] || *end != '\0' || runs < 1 || runs > 100) {
            fprintf(stderr, "windows: '%s' is not a number of runs from 1 to 100\n", argv[1]);
            return 1;
        }
    }
    wl_config cfg;
    wl_default_config(&cfg);
    wl_record *records = calloc(WL_TASKS, sizeof *records);
    double *lat = calloc(WL_TASKS, sizeof *lat);
    double *eps = calloc((size_t)runs * N_MULT, sizeof *eps);
    double *frac = calloc((size_t)runs * N_MULT, sizeof *frac);
    double *med = calloc((size_t)runs, sizeof *med);
    if (records == NULL || lat == NULL || eps == NULL || frac == NULL || med == NULL) {
        fprintf(stderr, "windows: out of memory\n");
        return 1;
    }
    wl_result run = { .records = records };
    for (unsigned i = 0; i < cfg.warmup_runs; i++) {
        wl_run(&cfg, &run);
    }
    for (long r = 0; r < runs; r++) {
        wl_run(&cfg, &run);
        int n = 0;
        for (unsigned i = 0; i < run.queued; i++) {
            if (records[i].start_ns != 0) {
                lat[n++] = (double)(records[i].start_ns - records[i].ready_ns) / 1000.0;
            }
        }
        med[r] = median_of(lat, n);
        for (size_t m = 0; m < N_MULT; m++) {
            wl_invariant inv;
            invariant_scan(&run, run.workers, MULTIPLES[m] * med[r], &inv);
            if (inv.violation_us < 0.0) {
                printf("invariant_scan is still the stub: write it first (handout section 3.1).\n");
                return 1;
            }
            eps[(size_t)r * N_MULT + m] = inv.episodes;
            frac[(size_t)r * N_MULT + m] = inv.span_us > 0 ? inv.violation_us / inv.span_us : 0.0;
        }
    }
    printf("%ld run(s), pool of %u workers; median hand-off latency %.1f us (run 0)\n\n",
           runs, run.workers, med[0]);
    printf("%12s %14s %22s %26s\n", "window", "window_us", "episodes (per run)",
           "violation_fraction (per run)");
    for (size_t m = 0; m < N_MULT; m++) {
        double col_e[100], col_f[100];
        for (long r = 0; r < runs; r++) {
            col_e[r] = eps[(size_t)r * N_MULT + m];
            col_f[r] = frac[(size_t)r * N_MULT + m];
        }
        double me = median_of(col_e, (int)runs), se = spread_of(col_e, (int)runs);
        double mf = median_of(col_f, (int)runs), sf = spread_of(col_f, (int)runs);
        printf("%9.0fx med %14.1f %13.1f (+-%5.1f) %17.4f (+-%.4f)\n",
               MULTIPLES[m], MULTIPLES[m] * med[0], me, se / 2, mf, sf / 2);
    }
    printf("\nMedians across runs, with half the spread. Choose your window in\n"
           "src/analyze.c (episode_window_us) and say why in REPORT.md.\n");
    free(records);
    free(lat);
    free(eps);
    free(frac);
    free(med);
    return 0;
}
