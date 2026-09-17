/*
 * COMP 410 - Week 3 - Michael & Scott lock-free queue (STARTER).
 *
 * The algorithm in one paragraph. The queue is a singly-linked list with a
 * permanent DUMMY node at the head, so head and tail never alias even when the
 * queue is empty -- that is what lets a producer and a consumer work at the two
 * ends without a lock between them. Enqueue links a node after the last one and
 * then swings tail; dequeue reads the value out of head->next and swings head.
 *
 * Every reader also HELPS: if it finds tail lagging behind the real last node,
 * it advances tail before retrying. Without that helping step a thread
 * descheduled between "link the node" and "swing tail" would stall everyone,
 * and the queue would not be lock-free.
 */
#define _POSIX_C_SOURCE 200809L

#include "msqueue.h"

#include <stdatomic.h>
#include <stdlib.h>

typedef struct node node;

/* A tagged pointer. The COUNT is the ABA defense: a node freed and reallocated
 * at the same ADDRESS still compares unequal, because the count moved on.
 * Without it, a CAS can succeed against a pointer that only looks unchanged. */
typedef struct {
    node *ptr;
    unsigned long count;
} tagged;

struct node {
    long value;
    _Atomic tagged next;
};

struct msqueue {
    _Atomic tagged head;
    _Atomic tagged tail;
    atomic_ulong retries;
};

static node *node_new(long value)
{
    node *n = malloc(sizeof *n);
    if (!n)
        return NULL;
    n->value = value;
    /* NOTE the named local. glibc's atomic_init is a MACRO, so a braced
     * initializer passed directly splits on its comma and looks like three
     * arguments -- it compiles on macOS and fails on Linux, which is where
     * this course is graded. (§1j) */
    tagged empty = { NULL, 0 };
    atomic_init(&n->next, empty);
    return n;
}

msqueue *msq_create(void)
{
    msqueue *q = malloc(sizeof *q);     //allocates the memory for the queue
    if (!q)                             // checks if the vaule of q is NULL, if it is then it returns NULL
        return NULL;
    node *dummy = node_new(0);          //sets the dummy node to the first node of the queue 0.
    if (!dummy) {                       //If there is no dummy node then it frees the allcated memory. Its a check.
        free(q);
        return NULL;
    }
    tagged start = { dummy, 0 };        // a tagged has the pointer of the start of the quese and the count of the number of nodes in the queue
    atomic_init(&q->head, start);       //initializes the head of the queue to the start of the queue
    atomic_init(&q->tail, start);       //initializes the tail of the queue to the start of the queue
    atomic_init(&q->retries, 0);        
    return q;
}

void msq_destroy(msqueue *q)
{
    if (!q)                             // does the same check as before to check if the queue is allocated
        return;

    tagged h = atomic_load_explicit(&q->head, memory_order_acquire); //Added the memory order to the atomic load to make sure that the value is loaded correctly and not optimized out by the compiler.
    node *n = h.ptr;
    while (n) {
        node *next = atomic_load_explicit(&n->next, memory_order_acquire).ptr; // same here
        free(n);
        n = next;
    }
    free(q);
}

bool msq_enqueue(msqueue *q, long value)
{
    node *n = node_new(value);  // allocates a new node with whatever the long is, should allocate 8 bytes.
    if (!n)
        return false;   //If it fails to allocate the node, it returns false.

    for (;;) {      // infinite loop, will only break when the enqueue is successful
        tagged tail = atomic_load_explicit(&q->tail, memory_order_acquire);     // loads the tail, then memory order acquires to make sure it loaded right
        tagged next = atomic_load_explicit(&tail.ptr->next, memory_order_acquire); // loads the value of the next node into next, then memory order acquires to make sure it loaded right

        /* Re-read tail: if it moved, our view is stale and we start again. */
        if (!(tail.ptr == atomic_load_explicit(&q->tail, memory_order_acquire).ptr))    // checks if the tail is still the same or has been changed by ABA
            continue;

        if (next.ptr == NULL) { // if the node we loaded into next is NULL

            /* TODO 1: tail really is last, so link your new node after it.
             *
             * A simple compare and swap which moves the tail to the new node if
             * the value after the tail is NULL. The first IF statement checks if
             * the CAS can successfully swap to the new tail, and then it does it. 
             * The focus is on count of each tagged to make sure that if an ABA pops up,
             * the CAS will fail and the loop will retry.
             */

            tagged newnext = { n, next.count + 1 };     // creates a new tagged newnext at the pointer of n (which was created at new_node) and then increments the count by 1 from the previous count
            if (atomic_compare_exchange_weak_explicit(&tail.ptr->next, &next, newnext, memory_order_release, memory_order_relaxed)) {      // If the CAS can successfully move the tail to the new node then
                tagged newtail = { n, tail.count + 1 };     // Similar to that of newnext, the pointer remains n, but the count is incremented by 1 from the previous count of tail
                atomic_compare_exchange_strong_explicit(&q->tail, &tail, newtail, memory_order_release, memory_order_relaxed);  //// With that newtail, we can now CAS the tail to the new node
                return true; //returns true to indicate that the enqueue was successful
            }

        } else {    //So this scenario is when the next pointer is not NULL meaning the tail is lagging behind.
            /* TODO 2: tail is lagging behind the real last node.
             *
             * Advance it -- CAS q->tail from `tail` to { next.ptr,
             * tail.count + 1 } -- and then let the loop retry.
             *
             * This step is what makes the queue LOCK-FREE rather than merely
             * correct: without it, a thread descheduled between TODO 1's two
             * CASes stalls every other thread. Deleting this branch still
             * passes the single-threaded tests.
             * 
             * 
             */
            tagged laggingtail = { next.ptr, tail.count + 1 }; // creates a new tag laggingtail at the "next" pointer and increments the count by 1 from the previous count of tail
            atomic_compare_exchange_strong_explicit(&q->tail, &tail, laggingtail, memory_order_release, memory_order_relaxed); // CAS the tail to the laggingtail if the expected value of tail is the same as the current value of tail, if not then it will retry.
            continue;
        }
        atomic_fetch_add_explicit(&q->retries, 1, memory_order_relaxed);
    }
}

bool msq_dequeue(msqueue *q, long *out)
{
    for (;;) {  // Same thing as before, infinite loop until the dequeue is successful
        tagged head = atomic_load_explicit(&q->head, memory_order_acquire); //tagged value for head
        tagged tail = atomic_load_explicit(&q->tail, memory_order_acquire); //tagged value for tail
        tagged next = atomic_load_explicit(&head.ptr->next, memory_order_acquire);  //tagged value for the value after head, next.

        if (!(head.ptr == atomic_load_explicit(&q->head, memory_order_acquire).ptr)) //IF the head has changed since we loaded it, then we retry the loop
            continue;

        if (head.ptr == tail.ptr) { //If the head and tail are equal, aka a single node queue.
            if (next.ptr == NULL) //If the node after head (next) is NULL
                return false;                  /* genuinely empty */ 
            /* Empty-looking only because tail lags. HELP, then retry. */
            tagged newtail = { next.ptr, tail.count + 1 }; //initialize new tail for CAS
            atomic_compare_exchange_strong_explicit(
                &q->tail, &tail, newtail,
                memory_order_release, memory_order_relaxed); //CAS the tail to next, making the queue have 2 nodes, one for head and one for tail.
        } else {
            // TODO 3: take the value and advance head.
            /* With dequeue we are looking to return all of the values out similarly to a stack where they come out of the head first.
             * So we decrement the head down and release the values out the top. So we start by storing the value of the next pointer
             * in val. We then set up a new tagged newhead to move head to the next pointer, which we then run compare and swap to move
             * head to the next pointer down the queue, which we then output the value that was stored there and was being held by val
             * and put that to *out to return. following that we free the space taken by the old head.
             */
            
            long val = next.ptr->value; // read the value of next and set it to val, this is done before the CAS to avoid ABA

            tagged newhead = { next.ptr, head.count + 1 };  // create a new tagged newhead to move the head forward in the queue. 
            if (atomic_compare_exchange_strong_explicit(&q->head, &head, newhead, memory_order_release, memory_order_relaxed)) {    // CAS the head to the newhead if the expected value of head is the same as the current value of head.
                *out = val; // set the value of out to val, so that the value of the dequeued node is returned
                free(head.ptr); // free the old head node
                return true;    // return true to indicate that the dequeue was successful
            }
        }
        atomic_fetch_add_explicit(&q->retries, 1, memory_order_relaxed);
    }
}

unsigned long msq_retries(const msqueue *q)
{
    return atomic_load_explicit(&q->retries, memory_order_relaxed);
}
