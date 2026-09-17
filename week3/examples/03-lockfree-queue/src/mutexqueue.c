/* COMP 410 - Week 3 - mutex-protected queue (STARTER). The baseline. */
#define _POSIX_C_SOURCE 200809L

#include "mutexqueue.h"

#include <pthread.h>
#include <stdlib.h>

typedef struct mnode {
    long value;
    struct mnode *next;
} mnode;

struct mqueue {
    mnode *head, *tail;
    pthread_mutex_t lock;
};

mqueue *mq_create(void)
{
    mqueue *q = malloc(sizeof *q);
    if (!q)
        return NULL;
    q->head = q->tail = NULL;
    if (pthread_mutex_init(&q->lock, NULL) != 0) {
        free(q);
        return NULL;
    }
    return q;
}

void mq_destroy(mqueue *q)
{
    if (!q)
        return;
    mnode *n = q->head;
    while (n) {
        mnode *next = n->next;
        free(n);
        n = next;
    }
    pthread_mutex_destroy(&q->lock);
    free(q);
}

bool mq_enqueue(mqueue *q, long value)
{
    mnode *n = malloc(sizeof *n);
    if (!n)
        return false;
    n->value = value;
    n->next = NULL;
    pthread_mutex_lock(&q->lock);
    if (q->tail)
        q->tail->next = n;
    else
        q->head = n;
    q->tail = n;
    pthread_mutex_unlock(&q->lock);
    return true;
}

bool mq_dequeue(mqueue *q, long *out)
{
    pthread_mutex_lock(&q->lock);
    mnode *n = q->head;
    if (!n) {
        pthread_mutex_unlock(&q->lock);
        return false;
    }
    q->head = n->next;
    if (!q->head)
        q->tail = NULL;
    pthread_mutex_unlock(&q->lock);
    *out = n->value;
    free(n);
    return true;
}
