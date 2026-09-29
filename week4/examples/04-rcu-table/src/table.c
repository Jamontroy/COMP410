/*
 * COMP 410 - Week 4 - the RCU table and the rwlock baseline (STARTER).
 *
 * The rwlock baseline is complete. The RCU table is complete except for the end
 * of rt_update(), which is TODO 4.
 */
#define _POSIX_C_SOURCE 200809L

#include "table.h"
#include "orders.h"

#include <stdlib.h>
#include <string.h>

static _Atomic long freed;

static void fill(version *v, long gen)
{
    v->gen = gen;
    for (int k = 0; k < TABLE_KEYS; k++)
        v->val[k] = gen * TABLE_KEYS + k;
}

static int consistent(const version *v, int key, long x)
{
    return x == v->gen * TABLE_KEYS + key;
}

/* ---- RCU ---------------------------------------------------------------- */

int rt_init(rcu_table *t)
{
    version *v = malloc(sizeof *v);
    if (!v)
        return -1;
    fill(v, 0);
    pthread_mutex_init(&t->update, NULL);
    t->latest = v;
    atomic_init(&t->cur, v);
    return 0;
}

int rt_lookup(rcu_table *t, rcu_reader *r, int key, long *value)
{
    rcu_read_lock(r);
    version *v = rcu_dereference(&t->cur);
    long x = v->val[key];
    int ok = consistent(v, key, x);
    rcu_read_unlock(r);
    *value = x;
    return ok;
}

/*
 * Read-copy update: copy the current version, change the copy, publish it,
 * wait for every reader that might hold the old one, then free the old one.
 */
int rt_update(rcu_table *t)
{
    version *nv = malloc(sizeof *nv);
    if (!nv)
        return -1;

    pthread_mutex_lock(&t->update);
    version *old = t->latest;
    memcpy(nv, old, sizeof *nv);
    fill(nv, old->gen + 1);

    /*
     * TODO 4 - finish the update.
     *
     * nv is a complete copy of the next generation, and no reader can see it
     * yet. The contract, in this order:
     *   - make nv the version that readers starting from now on will find;
     *   - make nv the updater's own latest copy;
     *   - wait until no reader can still hold old;
     *   - free old, and add one to `freed` with ORDER_STATS.
     *
     * Must not: free old while any reader might hold it, or leave it unfreed.
     *
     * Course tests: "rt_update() publishes the next generation", "the old
     * version is freed after the reader leaves, and not before", "50 updates
     * free exactly 50 old versions".
     *
     * The two lines after this comment are a placeholder so the starter builds
     * and does not leak. Replace them.
     */
    rcu_assign_pointer(&t->cur, nv); // publishes the initialized new version to readers
    t->latest = nv; // makes the new version the updater's current source for the next update
    synchronize_rcu(); // waits for readers that could still hold the old version
    free(old); // reclaims the old version after the grace period ends
    atomic_fetch_add_explicit(&freed, 1, ORDER_STATS); // records that one old version was reclaimed

    pthread_mutex_unlock(&t->update);
    return 0;
}

void rt_destroy(rcu_table *t)
{
    free(t->latest);
    pthread_mutex_destroy(&t->update);
}

long rt_freed(void)
{
    return atomic_load_explicit(&freed, ORDER_STATS);
}

/* ---- rwlock baseline ------------------------------------------------------ */

int rw_init(rw_table *t)
{
    fill(&t->v, 0);
    return pthread_rwlock_init(&t->lock, NULL) == 0 ? 0 : -1;
}

int rw_lookup(rw_table *t, int key, long *value)
{
    pthread_rwlock_rdlock(&t->lock);
    long x = t->v.val[key];
    int ok = consistent(&t->v, key, x);
    pthread_rwlock_unlock(&t->lock);
    *value = x;
    return ok;
}

void rw_update(rw_table *t)
{
    pthread_rwlock_wrlock(&t->lock);
    fill(&t->v, t->v.gen + 1);
    pthread_rwlock_unlock(&t->lock);
}

void rw_destroy(rw_table *t)
{
    pthread_rwlock_destroy(&t->lock);
}
