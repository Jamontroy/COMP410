/*
 * COMP 410 — Week 5 — YOUR OWN tests.
 *
 * Two cases. Each must PASS on your finished analyze.c and FAIL on the broken
 * version you name in the comment above it. Keep the function names and the
 * check() calls in main(): `make test` reports them, and your tests are run
 * against broken versions of analyze.c you cannot see when your work is read.
 *
 * A test that cannot fail proves nothing. Before you submit, put each broken
 * version into analyze.c, run `make test`, watch the right test go red, then
 * put your code back.
 */
#include "analyze.h"
#include "workload.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int passed, failed, skipped;

/* result: 1 pass, 0 fail, -1 not written yet (reported, never a failure) */
static void check(int result, const char *name, const char *why) {
    if (result < 0) {
        skipped++;
        printf("  [skip] %s\n", name);
    } else if (result) {
        passed++;
        printf("  [ok]   %s\n", name);
    } else {
        failed++;
        printf("  [FAIL] %s\n         %s\n", name, why);
    }
}

__attribute__((unused))
static int close_enough(double a, double b) {
    return fabs(a - b) < 1e-6;
}

/*
 * BASE (handout 2.2): a broken median_of: since compare_double returns (int)(left - right),
 * which rounds close decimal values to the nearest integer towards 0, while all 17 course tests use
 * integer-separated values. The decimal input below exposes that bug. Return
 * 1 when the property holds.
 */
static int mine_base_case(void) {
    double values[] = { 2.0, 1.2, 1.8 };
    return close_enough(median_of(values, 3), 1.8);
}

/*
 * (handout 3.3): an invariant_scan that treats any waiting task as a
 * violation, even while every worker is busy. Build a run where that version
 * and a correct one disagree. Return -1 until you write it.
 */
static int mine_invariant_case(void) {
        const uint64_t base_ns = 1000000000ull; // initialize base_ns to 1 second in nanoseconds
        wl_record records[] = { // initialize an array of wl_record structs to represent the tasks in the run
                { .task_id = 0, .ready_ns = base_ns, .start_ns = base_ns,
                    .end_ns = base_ns + 100000 }, // task 0 is ready at base_ns, starts at base_ns, and ends at base_ns + 100000
                { .task_id = 1, .ready_ns = base_ns, .start_ns = base_ns,
                    .end_ns = base_ns + 100000 }, // task 1 is ready at base_ns, starts at base_ns, and ends at base_ns + 100000
                { .task_id = 2, .ready_ns = base_ns + 10000, // this is the kicker and will cause the invariant to be violated in the broken version of invariant_scan, since it will see that there is a waiting task while all workers are busy
                    .start_ns = base_ns + 100000, .end_ns = base_ns + 110000 } // its ready at base_ns + 10000, starts at base_ns + 100000, and ends at base_ns + 110000
        };
        wl_result run = { .queued = 3, .workers = 2, .records = records }; // initialize a wl_result struct to represent the run, with 3 queued tasks, 2 workers, and the records array
        wl_invariant inv; // initialize a wl_invariant struct to hold the results of the invariant_scan
        invariant_scan(&run, 2, 0.0, &inv); // call invariant_scan with the run, 2 workers, a minimum episode length of 0.0, and the inv struct to hold the results
        return inv.episodes == 0 && close_enough(inv.violation_us, 0.0); // return 1 if the invariant_scan reports 0 episodes and 0.0 violation time, otherwise return 0
}

int main(void) {
    printf("== your tests ==\n");
    check(mine_base_case(),
          "your base case",
          "not written yet: this stub fails on purpose (handout 2.2)");
    check(mine_invariant_case(),
          "your invariant case (for 3.0)",
          "fails: see the comment above mine_invariant_case");
    printf("  %d of %d of your tests pass", passed, passed + failed);
    if (skipped) {
        printf(" (%d not written yet)", skipped);
    }
    printf("\n");
    return failed ? 1 : 0;
}
