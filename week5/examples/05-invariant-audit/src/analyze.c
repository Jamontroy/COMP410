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

typedef struct {uint64_t time_ns; int waiting_delta; int busy_delta;} invariant_event; // Create a new struct to hold the time_ns, waiting_delta, and busy_delta values for each event. This will be used to sort the events in order of time_ns and then process them to determine if the invariant is violated.

static int compare_event_time(const void *left, const void *right) { // This function is nearly identical to the one I used in compare_double, but this time it is comparing invariant_event structs and the time_ns values for qsort.
    const invariant_event *left_event = left;
    const invariant_event *right_event = right;
    if (left_event->time_ns < right_event->time_ns) {
        return -1;
    }
    if (left_event->time_ns > right_event->time_ns) {
        return 1;
    }
    return 0;
}

void invariant_scan(const wl_result *run, unsigned workers, double min_episode_us,
                    wl_invariant *out) {
    *out = (wl_invariant){0};

    size_t started = 0; // Count the number of started records in run->records
    uint64_t earliest_ready = 0, latest_end = 0;
    for (unsigned i = 0; i < run->queued; i++) { // for every record in run->records
        const wl_record *record = &run->records[i];  // set record to the current record[i] in run->records
        if (record->start_ns == 0) { // if the record has not started, skip it
            continue;
        }
        if (started == 0 || record->ready_ns < earliest_ready) { // if this is the first started record or its ready_ns is lower than the earliest_ready set it to earliest ready
            earliest_ready = record->ready_ns;
        }
        if (started == 0 || record->end_ns > latest_end) { // the same thing but with latest_end, if it has the longest end_ns set it to latest_end
            latest_end = record->end_ns;
        }
        started++; // increment the number of started records
    }
    if (started == 0) { // checks if there are any started records after the for loop, if not it returns out of the function with all values in out set to 0, as per the contract.
        return;
    }

    invariant_event *events = malloc(started * 3 * sizeof *events); // allocate memory for the new events array, which will hold all of the events for each started record. Each record has 3 events: ready, start, and end.
    if (events == NULL) { // check to make sure the memory was actually allocated
        fprintf(stderr, "invariant_scan: out of memory (started=%zu)\n", started);
        exit(1);
    }

    size_t event_count = 0; // initialize the event_count to 0, this will be used to keep track of how many events have been added to the events array
    for (unsigned i = 0; i < run->queued; i++) { // same as above for every record in run->records
        const wl_record *record = &run->records[i]; // same as above, set record to the current record[i] in run->records
        if (record->start_ns == 0) { // if the record has not started, skip it
            continue;
        }
        events[event_count++] = (invariant_event){ record->ready_ns, 1, 0 }; // add the ready event to the events array, with a waiting_delta of 1 and a busy_delta of 0
        events[event_count++] = (invariant_event){ record->start_ns, -1, 1 }; // add the start event to the events array, with a waiting_delta of -1 and a busy_delta of 1
        events[event_count++] = (invariant_event){ record->end_ns, 0, -1 }; // add the end event to the events array, with a waiting_delta of 0 and a busy_delta of -1
    }
    qsort(events, event_count, sizeof *events, compare_event_time); // use qsort to sort the events by time_ns, this will be used so we can process the events in order and determine if the invariant is violated at any point in time. Also uses the compare_event_time function I made earlier.

    int64_t waiting = 0, busy = 0; // these will be used to track the number of waiting and busy workers
    int in_episode = 0; // this will be used to track if we are currently in an episode of invariant violation
    uint64_t episode_start = 0; // this will be used to track the start time of an episode
    size_t i = 0; // this will be used to iterate through the events array
    while (i < event_count) { // while there are still events to process
        uint64_t time_ns = events[i].time_ns; // set time_ns to the current event's time_ns
        int64_t waiting_delta = 0, busy_delta = 0; // these will be used to track the changes in waiting and busy workers at this time_ns
        do { // processes all events with the same time_ns, adding their waiting_delta and busy_delta to the totals
            waiting_delta += events[i].waiting_delta;
            busy_delta += events[i].busy_delta;
            i++;
        } while (i < event_count && events[i].time_ns == time_ns); // while there are still events to process and the next event has the same time_ns, keep processing

        waiting += waiting_delta; // update number of waiting and busy workers
        busy += busy_delta; // update number of waiting and busy workers
        int now_violated = waiting > 0 && busy < (int64_t)workers; // check if the invariant is violated at this time_ns
        if (in_episode && !now_violated) { // if we are in a episode and the invariant is no longer violated, we need to end the episode and record its length
            double duration_us = (double)(time_ns - episode_start) / 1000.0;
            if (duration_us > min_episode_us) {
                out->episodes++;
                out->violation_us += duration_us;
            }
        } else if (!in_episode && now_violated) { // if we are not in an episode and the invariant is now violated, we need to start a new episode
            episode_start = time_ns;
        }
        in_episode = now_violated; // update the in_episode flag to reflect the current state of the invariant
    }

    out->span_us = (double)(latest_end - earliest_ready) / 1000.0; // set the span_us to the total time from the earliest ready_ns to the latest end_ns
    free(events); // free the memory allocated for the events array
}

double episode_window_us(double median_latency_us) {
    return median_latency_us > 0.0 ? 100.0 * median_latency_us : 1.0; // if the median latency is greater than 0, return 100 times the median latency, otherwise return 1.0
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
