/*
 * COMP 410 - Week 4 - the course tests for the RCU table.
 *
 * These are the course's tests; yours go in tests/test_mine.c.
 *
 * Each test proves it reached the code it is about. A test of waiting checks
 * that synchronize_rcu() really found a reader inside (rcu_sync_waits), and a
 * test of freeing checks that the free really happened (rt_freed), so code
 * that does nothing cannot pass by doing nothing.
 */
#define _POSIX_C_SOURCE 200809L

#include "rcu.h"
#include "table.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static void sleep_ms(long ms)
{
    struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

/* A one-shot signal between threads, from a mutex and a condition variable. */
typedef struct { pthread_mutex_t m; pthread_cond_t c; int set; } latch;

static void latch_init(latch *l)
{
    pthread_mutex_init(&l->m, NULL);
    pthread_cond_init(&l->c, NULL);
    l->set = 0;
}

static void latch_set(latch *l)
{
    pthread_mutex_lock(&l->m);
    l->set = 1;
    pthread_cond_broadcast(&l->c);
    pthread_mutex_unlock(&l->m);
}

static void latch_wait(latch *l)
{
    pthread_mutex_lock(&l->m);
    while (!l->set)
        pthread_cond_wait(&l->c, &l->m);
    pthread_mutex_unlock(&l->m);
}

static int latch_is_set(latch *l)
{
    pthread_mutex_lock(&l->m);
    int v = l->set;
    pthread_mutex_unlock(&l->m);
    return v;
}

static int passed, failed;

static void report(int ok, const char *name, const char *why)
{
    if (ok) {
        passed++;
        printf("  [ok]   %s\n", name);
    } else {
        failed++;
        printf("  [FAIL] %s\n         %s\n", name, why);
    }
}

/* ---- 1 ------------------------------------------------------------------ */

static void t_publish_visible(void)
{
    static int a, b;
    _Atomic(void *) slot;
    atomic_init(&slot, (void *)&a);
    rcu_assign_pointer(&slot, &b);
    report(rcu_dereference(&slot) == &b, "a published pointer is visible to a later dereference",
           "rcu_dereference() did not return what rcu_assign_pointer() stored");
}

/* ---- 2 ------------------------------------------------------------------ */

typedef struct { latch registered, release; } idle_arg;

static void *idle_reader(void *p)
{
    idle_arg *a = p;
    rcu_reader *r = rcu_register();
    latch_set(&a->registered);
    latch_wait(&a->release);
    rcu_unregister(r);
    return NULL;
}

static void t_sync_ignores_idle_reader(void)
{
    idle_arg a;
    latch_init(&a.registered);
    latch_init(&a.release);
    pthread_t th;
    pthread_create(&th, NULL, idle_reader, &a);
    latch_wait(&a.registered);
    unsigned long before = rcu_sync_waits();
    synchronize_rcu();
    unsigned long after = rcu_sync_waits();
    latch_set(&a.release);
    pthread_join(th, NULL);
    report(after == before, "a registered reader outside any section does not make synchronize_rcu() wait",
           "synchronize_rcu() waited for a reader whose counter was even");
}

/* ---- 3 and 5: a reader that holds a section open ------------------------ */

typedef struct {
    latch inside, leaving;
    rcu_table *t;                 /* NULL: hold a section without reading a table */
    long hold_ms;
    long gen_seen, freed_at_unlock;
    int consistent;
} hold_arg;

static void *holding_reader(void *p)
{
    hold_arg *a = p;
    rcu_reader *r = rcu_register();
    rcu_read_lock(r);
    version *v = a->t ? rcu_dereference(&a->t->cur) : NULL;
    latch_set(&a->inside);
    sleep_ms(a->hold_ms);
    if (v) {
        a->gen_seen = v->gen;
        a->consistent = 1;
        for (int k = 0; k < TABLE_KEYS; k++)
            if (v->val[k] != v->gen * TABLE_KEYS + k)
                a->consistent = 0;
        a->freed_at_unlock = rt_freed();
    }
    latch_set(&a->leaving);
    rcu_read_unlock(r);
    rcu_unregister(r);
    return NULL;
}

static void hold_init(hold_arg *a, rcu_table *t, long ms)
{
    latch_init(&a->inside);
    latch_init(&a->leaving);
    a->t = t;
    a->hold_ms = ms;
    a->gen_seen = -1;
    a->freed_at_unlock = -1;
    a->consistent = 0;
}

static void t_sync_waits_for_reader(void)
{
    hold_arg a;
    hold_init(&a, NULL, 200);
    pthread_t th;
    pthread_create(&th, NULL, holding_reader, &a);
    latch_wait(&a.inside);
    unsigned long before = rcu_sync_waits();
    synchronize_rcu();
    int reader_had_left = latch_is_set(&a.leaving);
    unsigned long after = rcu_sync_waits();
    pthread_join(th, NULL);

    const char *why = !reader_had_left
        ? "synchronize_rcu() returned while a reader was still inside its section"
        : "rcu_sync_waits() did not increase, so synchronize_rcu() never saw the reader inside";
    report(reader_had_left && after > before,
           "synchronize_rcu() waits for a reader that is inside its section", why);
}

/* ---- 4 ------------------------------------------------------------------ */

static void t_update_visible(void)
{
    rcu_table t;
    rt_init(&t);
    rt_update(&t);
    rcu_reader *r = rcu_register();
    rcu_read_lock(r);
    version *v = rcu_dereference(&t.cur);
    long gen = v->gen;
    rcu_read_unlock(r);
    rcu_unregister(r);
    rt_destroy(&t);
    report(gen == 1, "rt_update() publishes the next generation",
           "after one rt_update() a new reader still sees generation 0");
}

/* ---- 5 ------------------------------------------------------------------ */

static void t_no_early_free(void)
{
    rcu_table t;
    rt_init(&t);
    long base = rt_freed();
    hold_arg a;
    hold_init(&a, &t, 200);
    pthread_t th;
    pthread_create(&th, NULL, holding_reader, &a);
    latch_wait(&a.inside);
    rt_update(&t);
    long after = rt_freed();
    pthread_join(th, NULL);
    rt_destroy(&t);

    const char *why = "";
    if (a.freed_at_unlock != base)
        why = "the old version was freed while a reader still held it";
    else if (after != base + 1)
        why = "rt_update() did not free the old version once the reader had left";
    else if (!a.consistent || a.gen_seen != 0)
        why = "the reader's version changed underneath it";
    report(a.freed_at_unlock == base && after == base + 1 && a.consistent && a.gen_seen == 0,
           "the old version is freed after the reader leaves, and not before", why);
}

/* ---- 6 ------------------------------------------------------------------ */

static void t_every_retired_version_freed(void)
{
    rcu_table t;
    rt_init(&t);
    long base = rt_freed();
    for (int i = 0; i < 50; i++)
        rt_update(&t);
    long freed = rt_freed() - base;
    long gen = t.latest->gen;
    rt_destroy(&t);
    report(freed == 50 && gen == 50, "50 updates free exactly 50 old versions",
           "the number of versions freed does not match the number of updates");
}

/* ---- 7 ------------------------------------------------------------------ */

typedef struct { rcu_table *t; long lookups, torn; } stress_arg;

static void *stress_reader(void *p)
{
    stress_arg *a = p;
    rcu_reader *r = rcu_register();
    long v;
    for (long j = 0; j < a->lookups; j++)
        if (!rt_lookup(a->t, r, (int)(j % TABLE_KEYS), &v))
            a->torn++;
    rcu_unregister(r);
    return NULL;
}

static void t_stress(void)
{
    enum { READERS = 4, UPDATES = 200 };
    rcu_table t;
    rt_init(&t);
    long base = rt_freed();
    stress_arg a[READERS];
    pthread_t th[READERS];
    for (int i = 0; i < READERS; i++) {
        a[i] = (stress_arg){ &t, 200000, 0 };
        pthread_create(&th[i], NULL, stress_reader, &a[i]);
    }
    for (int u = 0; u < UPDATES; u++)
        rt_update(&t);
    long torn = 0;
    for (int i = 0; i < READERS; i++) {
        pthread_join(th[i], NULL);
        torn += a[i].torn;
    }
    long freed = rt_freed() - base;
    rt_destroy(&t);
    report(torn == 0 && freed == UPDATES,
           "4 readers and 200 updates: no torn read, and every old version freed",
           torn ? "a reader saw a torn or freed version" : "not every old version was freed");
}

int main(void)
{
    alarm(60);   /* a hung grace period ends the run instead of the terminal */
    t_publish_visible();
    t_sync_ignores_idle_reader();
    t_sync_waits_for_reader();
    t_update_visible();
    t_no_early_free();
    t_every_retired_version_freed();
    t_stress();
    printf("  %d of %d course tests pass\n", passed, passed + failed);
    return failed ? 1 : 0;
}
