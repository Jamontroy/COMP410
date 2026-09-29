/*
 * COMP 410 - Week 4 - a read-mostly lookup table, two ways.
 *
 * Every version of the table satisfies one rule:
 *
 *     val[k] == gen * TABLE_KEYS + k        for every key k
 *
 * A lookup checks it. A value that breaks the rule was read from a version
 * that was half-written, or freed and reused - a "torn" read. A correct RCU
 * table and a correct rwlock table both report zero.
 *
 * The API in this file is the specification. Do not change it.
 */
#ifndef TABLE_H
#define TABLE_H

#include "rcu.h"

#include <pthread.h>

#define TABLE_KEYS 64

typedef struct version {
    long gen;
    long val[TABLE_KEYS];
} version;

/* ---- the RCU table ------------------------------------------------------ */

typedef struct rcu_table {
    _Atomic(void *) cur;          /* the published version: readers load this */
    version *latest;              /* the updater's own copy, guarded by `update` */
    pthread_mutex_t update;       /* updaters exclude each other; readers never take it */
} rcu_table;

int  rt_init(rcu_table *t);                                       /* 0 ok, -1 no memory */
int  rt_lookup(rcu_table *t, rcu_reader *r, int key, long *value); /* 1 consistent, 0 torn */
int  rt_update(rcu_table *t);          /* next generation: 0 ok, -1 no memory */
void rt_destroy(rcu_table *t);         /* no readers may be inside */
long rt_freed(void);                   /* versions freed by rt_update, all tables */

/* ---- the baseline: one version, updated in place under a rwlock --------- */

typedef struct rw_table {
    pthread_rwlock_t lock;
    version v;
} rw_table;

int  rw_init(rw_table *t);
int  rw_lookup(rw_table *t, int key, long *value);                 /* 1 consistent, 0 torn */
void rw_update(rw_table *t);
void rw_destroy(rw_table *t);

#endif
