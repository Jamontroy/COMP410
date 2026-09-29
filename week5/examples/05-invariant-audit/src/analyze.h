/*
 * COMP 410 — Week 5 — A5: the analysis (the functions you write).
 *
 * Base (handout §2): median_of, spread_of, classify_latency.
 *
 * Each has a fixed contract below and course tests that fail until it holds.
 * How you compute them is yours; the contract is not.
 */
#ifndef WL_ANALYZE_H
#define WL_ANALYZE_H

#include "workload.h"

/* The latency rule (handout 2.1): a task's hand-off latency is BUG-LENGTH
 * when it exceeds this many times the RUN'S OWN median latency. */
#define BUG_MULTIPLIER 10.0

/*
 * median_of - the median of n values.
 *
 * Contract: does not modify v. n >= 1. Odd n: the middle value once v is
 * sorted. Even n: the average of the two middle values once v is sorted.
 */
double median_of(const double *v, int n);

/*
 * spread_of - the spread of n values: the largest minus the smallest.
 *
 * Contract: does not modify v. n >= 1 (the spread of one value is 0).
 */
double spread_of(const double *v, int n);

/*
 * classify_latency - the latency rule, applied to one task.
 *
 * Contract: returns 1 (BUG-LENGTH) when latency_us is STRICTLY GREATER than
 * BUG_MULTIPLIER times baseline_median_us; returns 0 otherwise, including
 * exactly at the threshold. baseline_median_us is the run's own median.
 */
int classify_latency(double latency_us, double baseline_median_us);

/* ---- the invariant itself (handout §3) ------------------ */

/*
 * The pool's work-conserving invariant: no worker is idle while a task is
 * waiting. Over one run, for every instant t:
 *
 *   waiting(t) = number of records with  ready_ns <= t < start_ns
 *   busy(t)    = number of records with  start_ns <= t < end_ns
 *   the invariant is VIOLATED at t when  waiting(t) > 0  and  busy(t) < workers
 *
 * An EPISODE is a maximal interval over which it is violated. Evaluate the
 * state after applying every timestamp equal to t, so two intervals that
 * touch at t are one episode, not two.
 */
typedef struct {
    double   violation_us;   /* total length of the episodes LONGER than the window */
    unsigned episodes;       /* how many episodes are longer than the window */
    double   span_us;        /* earliest ready_ns to latest end_ns, in us */
} wl_invariant;

/*
 * invariant_scan - measure the invariant over one run.
 *
 * Contract: reads run->records[0 .. run->queued - 1] and skips any record
 * with start_ns == 0 (the task never started). workers is the pool size.
 * Counts only episodes strictly longer than min_episode_us. Does not modify
 * the run. With no started records, every field is 0.
 *
 * The starter's stub sets violation_us to -1.0, which the tests and
 * `make analyze` read as "not attempted".
 */
void invariant_scan(const wl_result *run, unsigned workers, double min_episode_us,
                    wl_invariant *out);

/*
 * episode_window_us - YOUR persistence window M, for one run.
 *
 * Lozi et al. flag a violation only once it has persisted for M (§4.1).
 * Return the window, in microseconds, below which an episode counts as a
 * normal hand-off rather than a violation, given the run's median hand-off
 * latency. It must be > 0. Your REPORT justifies the choice from your own
 * data. The starter's stub returns -1.0 ("not chosen").
 */
double episode_window_us(double median_latency_us);

/* ---- the summary (provided) -------------------------------------------- */

/* One run's summary. Every field is a bench/audit.csv column, in this order. */
typedef struct {
    unsigned queued, completed, started;
    unsigned shard0_count, shard1_count;
    double   median_latency_us, spread_latency_us, max_latency_us;
    unsigned bug_count;
    double   bug_fraction;                 /* bug_count / started */
    double   shard0_median_latency_us, shard1_median_latency_us;
    double   window_us;                    /* episode_window_us(median) */
    double   violation_us, span_us;        /* invariant_scan */
    unsigned episodes;
    double   violation_fraction;           /* violation_us / span_us; -1 if not attempted */
} wl_summary;

/*
 * summarize_run - PROVIDED. Summarizes one run: the latencies of the tasks
 * that started (records with start_ns == 0 are skipped), then the five
 * functions above.
 */
void summarize_run(const wl_result *run, wl_summary *out);

#endif /* WL_ANALYZE_H */
