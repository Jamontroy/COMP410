/*
 * COMP 410 — Week 5 — A5: the analysis (STARTER).
 *
 * Five functions are yours. Each has a fixed contract in analyze.h and course
 * tests in tests/test_analyze.c that fail until it holds. How you compute
 * each one is yours: nothing below states the steps, only what must be true
 * when you return.
 *
 * Base (handout section 2): median_of, spread_of, classify_latency.
 */
#include "analyze.h"

#include <stdio.h>
#include <stdlib.h>

/*
 * This is needed so that qsort can iterate through and sort the array smoothly.
 * It will tell it with a -1,0,1 if the "left_value" or the value with the lower index in the list
 * e.g. [0] is bigger, equal to, or smaller than the "right value" which has a larger index e.g. [1]
 * This is pretty much qsort 101 and I found it from a quick Google Search
 */
static int compare_double(const void *left, const void *right) {
    const double left_value = *(const double *)left; //takes the first value 
    const double right_value = *(const double *)right; //takes the second value

    if (left_value < right_value) {
        return -1; // if left is less return -1
    }
    if (left_value > right_value) {
        return 1; // if left is greater return 1
    }
    return 0; // if equal return zero
}

double median_of(const double *v, int n) {
    /* To get the median we first need to sort the list of v, but since the tests check whether
     * the value of v is changed it works best if we create a new array sorted[] to sort v
     */

    double *sorted = malloc((size_t)n * sizeof *sorted); // Initialized and allocated memory for the sorted array
    if (sorted == NULL) {
        fprintf(stderr, "median_of: out of memory (n=%d)\n", n);
        exit(1);
    } // standard check if memory was allocated
    for (int i = 0; i < n; i++) { // for loop which moves every value of v[] into sorted[]
        sorted[i] = v[i];
    }
    qsort(sorted, (size_t)n, sizeof *sorted, compare_double); //The qsort which sorts all of the values in numberical order with the help of compare_double

    double median; // initialize median to be returned
    // Basic median grabbing code
    if (n % 2 == 1) { //If the size is odd
        median = sorted[n / 2]; //grab the middle value
    } else { //If the size is even
        median = (sorted[n / 2 - 1] + sorted[n / 2]) / 2.0; //Add the two middle values and divide them by 2.0 giving you the middle/average to the two.
    }
    free(sorted); //Release the memory held by sorted
    return median; //return the median value
}

double spread_of(const double *v, int n) {
    /* The spread of the values is just the maximum value within v minus the minimum*/
    double minimum = v[0]; // set maximum to the first value of v[]
    double maximum = v[0]; // set minimum to the first value of v[]
    for (int i = 1; i < n; i++) { // for every value in v[]
        if (v[i] < minimum) { // if v[i] is less than minimum, change minimum to that value
            minimum = v[i];
        }
        if (v[i] > maximum) { // if v[i] is greater than maximum, change maximum to that value
            maximum = v[i];
        }
    }
    return maximum - minimum;
}

int classify_latency(double latency_us, double baseline_median_us) {
    /* returns 1 (BUG-LENGTH) when latency_us is STRICTLY GREATER than
     * BUG_MULTIPLIER times baseline_median_us; returns 0 otherwise, including
     * exactly at the threshold. */
    return latency_us > BUG_MULTIPLIER * baseline_median_us;
}

/* ---- (handout section 3) --------------------------------- */

void invariant_scan(const wl_result *run, unsigned workers, double min_episode_us,
                    wl_invariant *out) {
    /* YOUR JOB (handout 3.1), for 3.0. Contract: analyze.h. Until you write
     * it, violation_us = -1 tells the tests and `make analyze` that section 3
     * has not been attempted. */
    (void)run;
    (void)workers;
    (void)min_episode_us;
    *out = (wl_invariant){ .violation_us = -1.0 };
}

double episode_window_us(double median_latency_us) {
    /* YOUR DECISION (handout 3.2), for 3.0: the persistence window, from
     * `make windows` on your own workload. -1 means "not chosen yet". */
    (void)median_latency_us;
    return -1.0;
}

/* ---- the summary (provided) ------------------------------------------- */

void summarize_run(const wl_result *run, wl_summary *out) {
    *out = (wl_summary){ .queued = run->queued, .completed = run->completed,
                         .shard0_count = run->shard0_count,
                         .shard1_count = run->shard1_count };
    double *lat = malloc(((size_t)run->queued + 1) * 3 * sizeof *lat);
    if (lat == NULL) {
        fprintf(stderr, "summarize_run: out of memory (queued=%u)\n", run->queued);
        exit(1);
    }
    double *lat0 = lat + run->queued + 1, *lat1 = lat0 + run->queued + 1;
    unsigned n = 0, n0 = 0, n1 = 0;
    for (unsigned i = 0; i < run->queued; i++) {
        const wl_record *r = &run->records[i];
        if (r->start_ns == 0) {
            continue;                       /* never started: no latency to report */
        }
        double us = (double)(r->start_ns - r->ready_ns) / 1000.0;
        lat[n++] = us;
        if (r->shard == 0) {
            lat0[n0++] = us;
        } else {
            lat1[n1++] = us;
        }
    }
    out->started = n;
    out->violation_fraction = -1.0;
    if (n == 0) {
        free(lat);
        return;
    }

    out->median_latency_us = median_of(lat, (int)n);
    out->spread_latency_us = spread_of(lat, (int)n);
    out->shard0_median_latency_us = n0 ? median_of(lat0, (int)n0) : 0.0;
    out->shard1_median_latency_us = n1 ? median_of(lat1, (int)n1) : 0.0;
    double max_us = lat[0];
    unsigned bug = 0;
    for (unsigned i = 0; i < n; i++) {
        if (lat[i] > max_us) {
            max_us = lat[i];
        }
        bug += (unsigned)(classify_latency(lat[i], out->median_latency_us) != 0);
    }
    out->max_latency_us = max_us;
    out->bug_count      = bug;
    out->bug_fraction   = (double)bug / (double)n;

    out->window_us = episode_window_us(out->median_latency_us);
    wl_invariant inv;
    invariant_scan(run, run->workers, out->window_us, &inv);
    out->violation_us = inv.violation_us;
    out->episodes     = inv.episodes;
    out->span_us      = inv.span_us;
    if (inv.violation_us >= 0.0 && inv.span_us > 0.0) {
        out->violation_fraction = inv.violation_us / inv.span_us;
    }
    free(lat);
}
