/*
 * nullcall.c — measure the cost of a system-call boundary crossing.
 *
 * This is the starter.
 * Look for the TODO markers.
 *
 * The technique is Ousterhout's (USENIX Summer 1990, §4 "Kernel Entry-Exit"):
 * time a kernel call that does as little as possible, so what you measure is
 * the crossing rather than the work. He used getpid(); we use getppid() for the
 * same reason — it returns a number the kernel already holds.
 *
 * You time TWO loops, identical except for what they call:
 *
 *   syscall loop : getppid()  — crosses the user/kernel boundary
 *   control loop : noop()     — an ordinary function call, stays in user mode
 *
 * Subtracting the control removes the loop counter, the branch and the call
 * sequence, leaving the part attributable to the boundary. Without it you would
 * be quoting the cost of the loop as if it were the cost of the crossing.
 *
 * Build:  make
 * Run:    ./nullcall [iterations] [trials]
 * Check:  make test
 */

/* clock_gettime() and CLOCK_MONOTONIC are POSIX, not ISO C. We build with
 * -std=c11 (strict ISO), which on glibc hides everything outside the standard
 * unless a feature-test macro asks for it -- so without this line the build
 * fails on Linux with "CLOCK_MONOTONIC undeclared". macOS headers expose it
 * regardless, which is exactly why this is easy to miss on a Mac.
 * This must come before any #include. */
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define DEFAULT_ITERS 1000000
#define DEFAULT_TRIALS 7

/* The control call.
 *
 * Three defences, all needed, or an optimising compiler will delete the loop
 * and your control will measure 0.00 ns:
 *   - noinline       : forces a real call/return rather than inlining
 *   - a global sink  : the return value escapes, so the call is not dead code
 *   - opaque counter : the compiler cannot prove what the function returns
 *
 * If you ever see the control report 0.00, the compiler has won. Do not report
 * the number — fix the barrier. (This is flaw 1 of the week's Final Challenge.)
 */
volatile long noop_sink = 0;

/* `unused` keeps the starter warning-clean before the TODOs are done;
 * it has no effect once your loops actually call these. */
__attribute__((noinline, unused)) static long noop(void)
{
    return ++noop_sink;
}

/* Monotonic clock only. CLOCK_REALTIME can jump (NTP, a manual change) and a
 * jump backward yields a negative duration. */
__attribute__((unused)) static double now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}

/*
 * TODO 1 — time `iters` calls to getppid() and return nanoseconds per call.
 */

static double time_syscall_loop(long iters, volatile long *sink)
{
    long total = 0;  //total is here to make sure the compiler doesn;t delete the code in compilation
    double t0 = now_ns();
    for (long i = 0; i < iters; i++)
        total += getppid(); //getppid()
    double t1 = now_ns();
    *sink += total;
    return (t1 - t0) / (double)iters;
}

/*
 * TODO 2 — the same, but calling noop() instead of getppid().
 */
static double time_control_loop(long iters, volatile long *sink)
{
    long total = 0;  //same archtecture but with noop for control now
    double t0 = now_ns();
    for (long i = 0; i < iters; i++)
        total += noop(); //now it is noop
    double t1 = now_ns();
    *sink += total;
    return (t1 - t0) / (double) iters;
}

/*Noop is important because it measures how long it take to perform a function call in isolation
without needing to cross over the boundary. Thus when we subtract the median getppid time from
noop time we cross out the overhead of the function call to be left solely with the crossover time. */

static int cmp_double(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

/*
 * TODO 3 — return the median of v[0..n-1].
 */
static double median(double *v, int n)
{
    qsort(v, (size_t)n, sizeof(double), cmp_double); //I had to remind myself how qsort works from documentation: qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));
    if (n % 2 == 1)                                  //cmp_double acts as our comparison function
        return v[n / 2];                             // This is just pretty standard median calculation
    return (v[n / 2 -1] + v[n / 2]) / 2.0;
}

int main(int argc, char **argv)
{
    long iters = (argc > 1) ? atol(argv[1]) : DEFAULT_ITERS;
    int trials = (argc > 2) ? atoi(argv[2]) : DEFAULT_TRIALS;

    if (iters <= 0 || trials <= 0) {
        fprintf(stderr, "usage: %s [iterations] [trials]\n", argv[0]);
        return 2;
    }

    double *sys = malloc(sizeof(double) * (size_t)trials);
    double *ctl = malloc(sizeof(double) * (size_t)trials);
    if (!sys || !ctl) { fprintf(stderr, "out of memory\n"); return 1; }

    volatile long sink = 0;

    /* One untimed warm-up of each: the first pass pays for cold caches and
     * page faults that no later pass repeats. */
    time_syscall_loop(iters / 10 + 1, &sink);
    time_control_loop(iters / 10 + 1, &sink);

    for (int t = 0; t < trials; t++) {
        sys[t] = time_syscall_loop(iters, &sink);
        ctl[t] = time_control_loop(iters, &sink);
    }

    double sys_med = median(sys, trials);
    double ctl_med = median(ctl, trials);
    double sys_spread = sys[trials - 1] - sys[0];   /* sorted by median() */
    double ctl_spread = ctl[trials - 1] - ctl[0];
    double crossing = sys_med - ctl_med;

    printf("iterations per trial : %ld\n", iters);
    printf("trials               : %d\n\n", trials);
    printf("  control (noop)     : %8.2f ns/call   (spread %.2f)\n",
           ctl_med, ctl_spread);
    printf("  syscall (getppid)  : %8.2f ns/call   (spread %.2f)\n",
           sys_med, sys_spread);
    printf("  ---------------------------------------------\n");
    printf("  crossing cost      : %8.2f ns   (syscall - control)\n", crossing);
    if (ctl_med > 0.0)
        printf("  ratio              : %8.1fx a user-mode call\n",
               sys_med / ctl_med);

    printf("\nNote: absolute figures are machine-specific (CPU, kernel version,\n");
    printf("Meltdown/Spectre mitigations). The RATIO is the stable part.\n");

    /* Guard against measuring nothing. A control of 0.00 ns means the compiler
     * removed the loop, and every number above it is then worthless. */
    if (ctl_med <= 0.0) {
        fprintf(stderr,
                "\nERROR: the control loop measured 0 ns/call.\n"
                "Either the TODOs are not finished, or the compiler has\n"
                "optimized the loop away. Do not report this figure.\n");
        free(sys); free(ctl);
        return 3;
    }

    if (sink == 0x7fffffff)
        fprintf(stderr, "unreachable %ld\n", (long)sink);

    free(sys);
    free(ctl);
    return 0;
}
