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
    return -1;  /* not written yet: skipped */
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
