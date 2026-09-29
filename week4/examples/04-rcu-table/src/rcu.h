/*
 * COMP 410 - Week 4 - a small user-space RCU.
 *
 * Readers take no locks. A reader's counter is odd while it is inside a
 * read-side critical section and even while it is outside. A writer that has
 * published a new version looks at every reader once, and waits for each one
 * that was inside to move on. That wait is the grace period.
 *
 * Restrictions, stated so the tests and the benchmark can respect them:
 *   - read-side critical sections do not nest;
 *   - synchronize_rcu() is never called from inside a read-side section;
 *   - a reader unregisters only while it is outside a section.
 *
 * The API in this file is the specification. Do not change it: the tests are
 * written against it.
 */
#ifndef RCU_H
#define RCU_H

#include <stdatomic.h>

#define RCU_MAX_READERS 64

typedef struct rcu_reader {
    _Atomic unsigned long seq;    /* odd while inside a section; only the owner stores it */
    _Atomic unsigned long reads;  /* sections completed - statistics only */
    unsigned long local_seq;      /* the owner's private copy of seq */
    int in_use;                   /* registry slot taken; guarded by the registry lock */
    char pad[36];                 /* 8 + 8 + 8 + 4 + 36 = one 64-byte cache line */
} rcu_reader;

/* Registration takes the registry lock. It is not part of the read path. */
rcu_reader   *rcu_register(void);          /* NULL when all slots are taken */
void          rcu_unregister(rcu_reader *r);

/* The read path: no locks, no allocation, no system calls. */
void          rcu_read_lock(rcu_reader *r);
void          rcu_read_unlock(rcu_reader *r);
void         *rcu_dereference(_Atomic(void *) *slot);

/* The update path. */
void          rcu_assign_pointer(_Atomic(void *) *slot, void *p);
void          synchronize_rcu(void);

/* How many times synchronize_rcu() found a reader inside and had to wait.
 * The tests use it to prove they reached the waiting path. */
unsigned long rcu_sync_waits(void);

#endif
