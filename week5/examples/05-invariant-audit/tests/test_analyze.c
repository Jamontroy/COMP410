/*
 * COMP 410 — Week 5 — the course tests for analyze.c.
 *
 * These are the course's tests; your own go in tests/test_mine.c.
 *
 * Two groups. The 17 base tests must all pass for the base assignment. The
 * two invariant tests (handout §3, the 3.0 part) are SKIPPED while
 * invariant_scan is still the stub, and must pass once you write it. They
 * deliberately do not test everything the contract says: what they leave
 * out is where your own invariant test goes (handout §3.3).
 *
 * Each test proves it reached the code it is about, including the boundary
 * of the latency rule itself: a classifier that flags everything, or
 * nothing, or gets the boundary backward, fails here.
 */
#define _POSIX_C_SOURCE 200809L

#include "analyze.h"
#include "workload.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int passed, failed;

static void check(int ok, const char *name, const char *why) {
    if (ok) {
        passed++;
        printf("  [ok]   %s\n", name);
    } else {
        failed++;
        printf("  [FAIL] %s\n         %s\n", name, why);
    }
}

static int close_enough(double a, double b) {
    return fabs(a - b) < 1e-6;
}

/* A record from microsecond timestamps. The pool's clock never reads 0, so
 * every time here is offset by 1 s: start_ns == 0 means "never started". */
#define T0_NS 1000000000ull
static wl_record rec(unsigned id, unsigned shard, double ready_us, double start_us,
                     double end_us) {
    wl_record r;
    memset(&r, 0, sizeof r);
    r.task_id  = id;
    r.shard    = shard;
    r.ready_ns = T0_NS + (uint64_t)(ready_us * 1000.0);
    r.start_ns = T0_NS + (uint64_t)(start_us * 1000.0);
    r.end_ns   = T0_NS + (uint64_t)(end_us * 1000.0);
    return r;
}

/* ---- median_of ------------------------------------------------------- */

static void test_median_odd(void) {
    double v[] = { 3, 1, 2 };
    check(close_enough(median_of(v, 3), 2.0),
          "median_of: odd-length array",
          "median_of({3,1,2}) must be 2 (the sorted middle value)");
}

static void test_median_even(void) {
    double v[] = { 1, 2, 3, 4 };
    check(close_enough(median_of(v, 4), 2.5),
          "median_of: even-length array averages the two middle values",
          "median_of({1,2,3,4}) must be 2.5, not 2 or 3");
}

static void test_median_single(void) {
    double v[] = { 42.0 };
    check(close_enough(median_of(v, 1), 42.0),
          "median_of: a single value is its own median",
          "median_of({42}) must be 42");
}

static void test_median_does_not_reorder_input(void) {
    double v[] = { 5, 1, 3 };
    double m = median_of(v, 3);
    check(close_enough(m, 3.0) && v[0] == 5 && v[1] == 1 && v[2] == 3,
          "median_of: returns 3 for {5,1,3} and leaves the array as it was",
          "the contract (analyze.h) says median_of does not modify v -- sort a copy");
}

/* ---- spread_of --------------------------------------------------------- */

static void test_spread_basic(void) {
    double v[] = { 5, 1, 9, 3 };
    check(close_enough(spread_of(v, 4), 8.0),
          "spread_of: largest minus smallest",
          "spread_of({5,1,9,3}) must be 9-1=8");
}

static void test_spread_single(void) {
    double v[] = { 7.0 };
    check(close_enough(spread_of(v, 1), 0.0),
          "spread_of: one value has zero spread",
          "spread_of({7}) must be 0");
}

/* ---- classify_latency -------------------------------------------------- */

static void test_classify_above_threshold(void) {
    check(classify_latency(101.0, 10.0) == 1,
          "classify_latency: strictly above BUG_MULTIPLIER x median is BUG-LENGTH",
          "101us against a 10us median (threshold 100us) must be 1");
}

static void test_classify_at_threshold_is_not_bug(void) {
    check(classify_latency(BUG_MULTIPLIER * 10.0, 10.0) == 0,
          "classify_latency: EXACTLY at the threshold is not bug-length",
          "the contract says STRICTLY greater -- exactly BUG_MULTIPLIER x median must return 0");
}

static void test_classify_below_threshold(void) {
    check(classify_latency(50.0, 10.0) == 0,
          "classify_latency: comfortably below the threshold is normal",
          "50us against a 10us median (threshold 100us) must be 0");
}

static void test_classify_uses_the_given_baseline_not_a_constant(void) {
    /* Same latency, two baselines: a hardcoded absolute threshold cannot get
     * both right, whatever constant it uses. */
    check(classify_latency(150.0, 20.0) == 0 && classify_latency(150.0, 5.0) == 1,
          "classify_latency: the verdict depends on the baseline it is given",
          "150us is NOT bug-length against a 20us median but IS against a 5us median");
}

/* ---- summarize_run (provided), with a hand-checked answer -------------- */

static void test_summarize_run_known_case(void) {
    /* 10 tasks, latencies (us) by id: 8,9,10,10,11,11,12,13,500,600. Shard 0
     * is class 3 (ids 3 and 7: latencies 10 and 13). Sorted, the median of
     * all ten is (11+11)/2 = 11, so the threshold is 110 and exactly two
     * (500, 600) are bug-length. Shard 0's median is (10+13)/2 = 11.5;
     * shard 1's eight values 8,9,10,11,11,12,500,600 have median 11. */
    static const double lat[10] = { 8, 9, 10, 10, 11, 11, 12, 13, 500, 600 };
    wl_record recs[10];
    for (unsigned i = 0; i < 10; i++) {
        recs[i] = rec(i, (i % 4 == 3) ? 0u : 1u, 0.0, lat[i], lat[i] + 1.0);
    }
    wl_result run = { .queued = 10, .completed = 10, .shard0_count = 2,
                      .shard1_count = 8, .workers = 4, .records = recs };
    wl_summary s;
    summarize_run(&run, &s);

    check(close_enough(s.median_latency_us, 11.0),
          "summarize_run: median over all ten tasks",
          "expected 11.0us (the average of the two middle sorted values)");
    check(close_enough(s.max_latency_us, 600.0),
          "summarize_run: max latency",
          "expected 600.0us, the largest of the ten");
    check(s.bug_count == 2 && close_enough(s.bug_fraction, 0.2),
          "summarize_run: exactly two of the ten cross the threshold",
          "expected bug_count 2 and bug_fraction 2/10 with an 11us median and a 10x rule");
    check(close_enough(s.shard0_median_latency_us, 11.5),
          "summarize_run: shard 0's median",
          "expected 11.5us, the median of shard 0's two latencies (10 and 13)");
    check(close_enough(s.shard1_median_latency_us, 11.0),
          "summarize_run: shard 1's median",
          "expected 11.0us, the median of shard 1's eight latencies");
}

/* A task that never started has start_ns == 0. Reading it as a latency would
 * be start - ready = an unsigned underflow of about 1.8e16 us; summarize_run
 * must skip it. */
static void test_summarize_run_skips_unstarted(void) {
    wl_record recs[4] = { rec(0, 1, 0, 10, 20), rec(1, 1, 0, 10, 20),
                          rec(2, 1, 0, 10, 20), rec(3, 0, 0, 10, 20) };
    recs[1].start_ns = 0;
    recs[1].end_ns   = 0;
    wl_result run = { .queued = 4, .completed = 3, .shard0_count = 1,
                      .shard1_count = 3, .workers = 3, .records = recs };
    wl_summary s;
    summarize_run(&run, &s);
    check(s.started == 3 && close_enough(s.max_latency_us, 10.0) && s.bug_count == 0,
          "summarize_run: a task that never started is skipped, not read as a latency",
          "expected started 3, max 10us, no bug-length tasks");
}

static void test_summarize_run_survives_nothing_started(void) {
    wl_record recs[2] = { rec(0, 1, 0, 10, 20), rec(1, 0, 0, 10, 20) };
    recs[0].start_ns = 0;
    recs[1].start_ns = 0;
    wl_result run = { .queued = 2, .completed = 0, .shard0_count = 1,
                      .shard1_count = 1, .workers = 3, .records = recs };
    wl_summary s;
    memset(&s, 0xA5, sizeof s);          /* poison: the empty path must overwrite it */
    summarize_run(&run, &s);
    check(s.started == 0 && s.bug_count == 0 && close_enough(s.bug_fraction, 0.0),
          "summarize_run: a run where nothing started is summarized, not divided by zero",
          "expected started 0, bug_count 0, bug_fraction 0.0");
}

/* ---- invariant_scan: the 3.0 part (handout section 3) ------------------ */

/*
 * Two workers.   task A: ready 0,    start 0,    end 1000   (busy throughout)
 *                task B: ready 100,  start 400,  end 500
 *                task C: ready 600,  start 700,  end 800
 * [100,400): B waits, only A busy (1 < 2)   -> violated, 300 us
 * [400,500): A and B busy, nothing waits    -> not violated
 * [500,600): only A busy, nothing waits     -> not violated
 * [600,700): C waits, only A busy           -> violated, 100 us
 * The span is 0 .. 1000 us.
 */
static void three_task_run(wl_record recs[3], wl_result *run) {
    recs[0] = rec(0, 1, 0, 0, 1000);
    recs[1] = rec(1, 1, 100, 400, 500);
    recs[2] = rec(2, 1, 600, 700, 800);
    *run = (wl_result){ .queued = 3, .completed = 3, .shard1_count = 3,
                        .workers = 2, .records = recs };
}

static int invariant_attempted(void) {
    wl_record recs[3];
    wl_result run;
    wl_invariant inv;
    three_task_run(recs, &run);
    invariant_scan(&run, 2, 0.0, &inv);
    return inv.violation_us >= 0.0;
}

static void test_invariant_two_episodes(void) {
    wl_record recs[3];
    wl_result run;
    wl_invariant inv;
    three_task_run(recs, &run);
    invariant_scan(&run, 2, 50.0, &inv);
    check(inv.episodes == 2 && close_enough(inv.violation_us, 400.0)
          && close_enough(inv.span_us, 1000.0),
          "invariant_scan: two episodes, 300us and 100us, over a 1000us span",
          "expected episodes 2, violation_us 400, span_us 1000 (see the diagram above three_task_run)");
}

static void test_invariant_window_is_strict(void) {
    wl_record recs[3];
    wl_result run;
    wl_invariant inv;
    three_task_run(recs, &run);
    invariant_scan(&run, 2, 300.0, &inv);
    check(inv.episodes == 0 && close_enough(inv.violation_us, 0.0),
          "invariant_scan: an episode exactly as long as the window does not count",
          "with a 300us window, the 300us and 100us episodes are both excluded (strictly longer)");
}

int main(void) {
    printf("== course tests: the base ==\n");
    test_median_odd();
    test_median_even();
    test_median_single();
    test_median_does_not_reorder_input();
    test_spread_basic();
    test_spread_single();
    test_classify_above_threshold();
    test_classify_at_threshold_is_not_bug();
    test_classify_below_threshold();
    test_classify_uses_the_given_baseline_not_a_constant();
    test_summarize_run_known_case();
    test_summarize_run_skips_unstarted();
    test_summarize_run_survives_nothing_started();
    printf("  %d of %d base tests pass\n", passed, passed + failed);
    int base_failed = failed;

    printf("\n== course tests: the invariant (handout section 3, for 3.0) ==\n");
    if (!invariant_attempted()) {
        printf("  [skip] invariant_scan is still the stub -- 2 tests not run\n");
    } else {
        passed = failed = 0;
        test_invariant_two_episodes();
        test_invariant_window_is_strict();
        printf("  %d of %d invariant tests pass\n", passed, passed + failed);
    }
    return (base_failed || failed) ? 1 : 0;
}
