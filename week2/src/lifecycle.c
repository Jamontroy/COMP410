// COMP 410 - Week 2 - module load/unload, modeled in user space.
//

//
// The rule this exists to make measurable: EVERY resource acquired in init
// must be released in exit, AND a FAILING init must release the ones it
// already took before it returns an error. The second half is the one people
// get wrong, because the happy path tests fine.
//
// Here a "resource" is a counted allocation. The kernel leaks silently until
// the machine dies; this program just prints the count, which is the same
// information available at a glance.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int live;          // resources currently held
static int peak;

static void *acquire(const char *what)
{
        void *p = malloc(16);
        if (!p)
                return NULL;
        live++;
        if (live > peak)
                peak = live;
        printf("      acquire %-12s live=%d\n", what, live);
        return p;
}

static void release(void *p, const char *what)
{
        if (!p)
                return;
        free(p);
        live--;
        printf("      release %-12s live=%d\n", what, live);
}

// A module init that takes three resources. `fail_at` says which acquisition
// fails (0 = none), so the same code shows the happy path and every partial
// failure -- which is exactly what an unload-path bug needs to be visible.
static int init_module_model(int fail_at, void **a, void **b, void **c)
{
        *a = *b = *c = NULL;

        *a = (fail_at == 1) ? NULL : acquire("proc entry");
        if (!*a)
                return -1;                       // nothing held yet

        *b = (fail_at == 2) ? NULL : acquire("workqueue");
        if (!*b) {
                release(*a, "proc entry");       // unwind what we DID take
                *a = NULL;
                return -1;
        }

        *c = (fail_at == 3) ? NULL : acquire("timer");
        if (!*c) {
                release(*b, "workqueue");        // unwind in REVERSE order
                release(*a, "proc entry");
                *b = *a = NULL;
                return -1;
        }
        return 0;
}

static void exit_module_model(void *a, void *b, void *c)
{
        release(c, "timer");                     // reverse of acquisition
        release(b, "workqueue");
        release(a, "proc entry");
}

int main(void)
{
        void *a, *b, *c;
        int rc, bad = 0;

        printf("Module lifecycle: what must be zero after every path\n\n");

        printf("  load then unload (the happy path)\n");
        rc = init_module_model(0, &a, &b, &c);
        printf("      init returned %d\n", rc);
        exit_module_model(a, b, c);
        printf("      after unload: live=%d  %s\n\n", live,
               live == 0 ? "clean" : "LEAK");
        if (live != 0)
                bad++;

        // The three partial failures. exit() is NEVER called for a failed
        // init -- the kernel does not call module_exit if module_init returns
        // an error -- so init must clean up after itself or the leak is
        // permanent until reboot.
        for (int f = 1; f <= 3; f++) {
                printf("  load fails at acquisition %d (exit is NOT called)\n", f);
                rc = init_module_model(f, &a, &b, &c);
                printf("      init returned %d\n", rc);
                printf("      after failed load: live=%d  %s\n\n", live,
                       live == 0 ? "clean" : "LEAK");
                if (rc == 0 || live != 0)
                        bad++;
        }

        printf("peak resources held: %d\n", peak);
        printf("%s\n", bad ? "SOME PATHS LEAKED" : "All four paths end at zero");
        return bad ? 1 : 0;
}
