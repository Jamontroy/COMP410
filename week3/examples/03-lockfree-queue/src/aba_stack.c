/*
 * COMP 410 - Week 3 - the ABA exercise (STARTER).
 *
 * A Treiber stack with a real bug, and a two-thread demonstration that shows it
 * on demand rather than once in a thousand runs.
 *
 * Build and run:   make aba
 *
 */
#define _POSIX_C_SOURCE 200809L

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct anode {
    long value;
    struct anode *next;
} anode;

/* THE BUG IS HERE: the CAS target is a bare pointer.
 *
 * Compare `tagged` in msqueue.c, which pairs the pointer with a counter. A
 * node popped and pushed again at the SAME ADDRESS compares equal here, so a
 * CAS succeeds against a pointer that is no longer the one it read. */
static _Atomic(anode *) top;

/* Three real nodes, reused. Reuse is what creates ABA: a fresh allocation
 * every time would usually land at a new address and hide the bug. */
static anode A, B, Cn;

/* Two gates, so the interleaving below is exact rather than hoped for. */
static atomic_int t2_may_run, t2_done;

static void push(anode *n)
{
    do {
        n->next = atomic_load(&top);
    } while (!atomic_compare_exchange_weak(&top, &n->next, n));
}

/* The victim pop: reads the head, pauses, then commits. The pause stands in
 * for a context switch, an interrupt, or a cache miss. */
static anode *slow_pop(void)
{
    anode *t = atomic_load(&top);
    if (!t)
        return NULL;
    anode *next = t->next;              /* stale the moment thread 2 runs */

    atomic_store(&t2_may_run, 1);       /* let thread 2 do its work */
    while (!atomic_load(&t2_done))
        ;

    /* `top` is A again -- but the stack underneath it is no longer what this
     * thread read. The CAS cannot tell. */
    if (atomic_compare_exchange_strong(&top, &t, next))
        return t;
    return NULL;
}

static void *thread2(void *arg)
{
    (void)arg;
    while (!atomic_load(&t2_may_run))
        ;
    /* Pop A, pop B, push A back: top is A again, and A->next is now C. */
    anode *a = atomic_load(&top);
    atomic_store(&top, a->next);        /* pop A */
    anode *b = atomic_load(&top);
    atomic_store(&top, b->next);        /* pop B */
    push(a);                            /* push A back */
    atomic_store(&t2_done, 1);
    return NULL;
}

int main(void)
{
    A.value = 1; B.value = 2; Cn.value = 3;

    /* Stack: A -> B -> C */
    Cn.next = NULL;
    B.next = &Cn;
    A.next = &B;
    atomic_store(&top, &A);

    printf("start:  top -> A -> B -> C\n");
    printf("thread 1 reads top = A, next = B, then pauses.\n");
    printf("thread 2 pops A, pops B, pushes A back:  top -> A -> C\n");
    printf("thread 1 wakes and CASes top from A to B.\n\n");

    pthread_t t;
    pthread_create(&t, NULL, thread2, NULL);
    anode *got = slow_pop();
    pthread_join(t, NULL);

    printf("result: thread 1 popped %s\n", got ? (got == &A ? "A" : "?") : "nothing");
    printf("        top is now %s\n",
           atomic_load(&top) == &B ? "B  <-- B WAS ALREADY POPPED" : "something else");

    int broken = (atomic_load(&top) == &B);
    printf("\n%s\n", broken
        ? "  The CAS succeeded because top was A again -- the same ADDRESS thread 1\n"
          "  read. It could not see that the stack beneath it had changed. B is back\n"
          "  on a stack it was already removed from, and C has been lost.\n\n"
          "  No crash and no hang: just a wrong answer, which is what makes this\n"
          "  class of bug expensive to find."
        : "  The stack is intact this run.");
    return broken ? 1 : 0;
}
