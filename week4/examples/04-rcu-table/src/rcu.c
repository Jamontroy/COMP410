/*
 * COMP 410 - Week 4 - a small user-space RCU (STARTER).
 *
 * The protocol, in four operations:
 *
 *   reader enters     store own counter (now odd)        ORDER_ANNOUNCE
 *   reader reads      load the version pointer           ORDER_DEREF
 *   writer publishes  store the version pointer          ORDER_PUBLISH
 *   writer waits      load each reader's counter         ORDER_SCAN
 *
 * and one on the way out: the reader leaves by storing its counter again (now
 * even), ORDER_EXIT.
 *
 * Registration, publish and dereference are written. TODOs 1-3 are yours; the
 * orderings are TODO 5, in orders.h.
 */
#define _POSIX_C_SOURCE 200809L

#include "rcu.h"
#include "orders.h"

#include <pthread.h>
#include <sched.h>
#include <stddef.h>

static rcu_reader readers[RCU_MAX_READERS];
static pthread_mutex_t registry = PTHREAD_MUTEX_INITIALIZER;
static _Atomic unsigned long sync_waits;

rcu_reader *rcu_register(void)
{
    rcu_reader *r = NULL;
    pthread_mutex_lock(&registry);
    for (int i = 0; i < RCU_MAX_READERS; i++) {
        if (!readers[i].in_use) {
            r = &readers[i];
            r->in_use = 1;
            r->local_seq = 0;
            atomic_store_explicit(&r->seq, 0, ORDER_EXIT);
            atomic_store_explicit(&r->reads, 0, ORDER_STATS);
            break;
        }
    }
    pthread_mutex_unlock(&registry);
    return r;
}

void rcu_unregister(rcu_reader *r)
{
    pthread_mutex_lock(&registry);
    r->in_use = 0;
    pthread_mutex_unlock(&registry);
}

void rcu_read_lock(rcu_reader *r)
{
    /*
     * TODO 1 - enter a read-side critical section.
     *
     * Contract: when this returns, r->seq holds an odd value that no earlier
     * section of this reader used, stored with ORDER_ANNOUNCE. Only this
     * thread ever stores r->seq, and r->local_seq is its private copy.
     *
     * Must not: take a lock, allocate memory, or make a system call.
     *
     * Course test: "synchronize_rcu() waits for a reader that is inside its
     * section".
     */
    unsigned long next = r->local_seq + 1; // updates the readers local sequence + 1 to make it an odd number
    r->local_seq = next; // records the sequence locally so the next unlock can advance it again
    atomic_store_explicit(&r->seq, next, ORDER_ANNOUNCE); // publishes that this reader is inside its critical section
}

void rcu_read_unlock(rcu_reader *r)
{
    /*
     * TODO 2 - leave the section.
     *
     * Contract: when this returns, r->seq holds the next even value, stored
     * with ORDER_EXIT, and the reader's statistics counter r->reads is one
     * larger, with ORDER_STATS.
     *
     * Must not: take a lock, allocate memory, or make a system call.
     */
    unsigned long next = r->local_seq + 1; // advances this reader's private sequence to the next even value
    r->local_seq = next; // records the sequence for the next read-side entry
    atomic_store_explicit(&r->seq, next, ORDER_EXIT); // publishes that this reader has left its critical section
    atomic_fetch_add_explicit(&r->reads, 1, ORDER_STATS); // records one completed read-side section
}

void *rcu_dereference(_Atomic(void *) *slot)
{
    return atomic_load_explicit(slot, ORDER_DEREF); // loads the currently published version for a reader
}

void rcu_assign_pointer(_Atomic(void *) *slot, void *p)
{
    atomic_store_explicit(slot, p, ORDER_PUBLISH); // publishes the fully initialized version to future readers
}

void synchronize_rcu(void)
{
    /*
     * TODO 3 - wait for a grace period.
     *
     * Contract: return only when every registered reader that was inside a
     * section at the moment you looked at it has since left that section. A
     * reader whose counter is even is outside and needs no wait. Read each
     * counter with ORDER_SCAN. Each time you find a reader inside and have to
     * wait for it, add one to sync_waits with ORDER_STATS: the course tests
     * read it to confirm the waiting path ran.
     *
     * Hold the registry lock while you look at the readers, so none is
     * registered or unregistered under you. While waiting, give up the CPU
     * rather than spinning on it.
     *
     * Two ways to get this wrong. Your own tests in tests/test_mine.c must
     * catch both: returning while a reader is still inside, and waiting for a
     * counter to become even - a reader that re-enters at once is almost never
     * seen even.
     *
     * Course test: "synchronize_rcu() waits for a reader that is inside its
     * section".
     */
    struct {rcu_reader *r; unsigned long seq;} // stores a reader and the exact sequence observed while it was inside
    waiting[RCU_MAX_READERS]; // holds every reader that must be waited for during this grace period
    size_t n_wait = 0; // counts the readers recorded in the waiting array

    pthread_mutex_lock(&registry); // prevents registration changes while the reader set is being sampled
    for (int i = 0; i < RCU_MAX_READERS; i++) { // examines each possible registry slot
        rcu_reader *r = &readers[i];
        if (!r->in_use) // ignores registry slots that have no registered reader
            continue; // moves to the next slot
        unsigned long seq = atomic_load_explicit(&r->seq, ORDER_SCAN); // samples this reader's published sequence
        if ((seq % 2) == 1) { // an odd sequence means this reader is inside a section
            waiting[n_wait].r = r; // records the reader that may still hold an old version
            waiting[n_wait].seq = seq; // records the exact section that must finish
            n_wait++; // adds this reader to the grace-period wait set
        }
    }

    pthread_mutex_unlock(&registry); // allows registration changes while the updater waits

    for (size_t i = 0; i < n_wait; i++) { // waits for each reader that was inside when the snapshot was taken
        rcu_reader *r = waiting[i].r; // retrieves the sampled reader
        unsigned long seq = waiting[i].seq; // retrieves the reader's sampled sequence
        if ((seq % 2) == 0) // skips a defensive case where the recorded sequence was already even
            continue;

        atomic_fetch_add_explicit(&sync_waits, 1, ORDER_STATS); // records that this grace period found an active reader
        while (atomic_load_explicit(&r->seq, ORDER_SCAN) == seq) // waits until this exact read-side section has ended
            sched_yield(); // yields the CPU while the reader completes
    }
}

unsigned long rcu_sync_waits(void)
{
    return atomic_load_explicit(&sync_waits, ORDER_STATS);
}
