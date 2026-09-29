/*
 * COMP 410 - Week 4 - one memory ordering per atomic operation site (STARTER).
 *
 * Every atomic operation in rcu.c and table.c takes its ordering from here, and
 * the model checker (`make model`) reads this same file. This file is the whole
 * of the ordering decision.
 *
 * TODO 5 - choose each ordering. Every site below starts as relaxed, the
 * weakest there is. For each one, choose the weakest ordering that is still
 * correct on BOTH x86-64 and arm64, and argue the choice in REPORT.md.
 * A store takes relaxed, release or seq_cst; a load takes relaxed, acquire or
 * seq_cst. The compiler warns on any other combination.
 *
 *   STATS     the statistics counters: reads, sync_waits, freed   (fetch-add)
 *   ANNOUNCE  a reader entering a section stores its counter       (store)
 *   DEREF     a reader loads the published version pointer         (load)
 *   EXIT      a reader leaving a section stores its counter        (store)
 *   PUBLISH   the writer stores the new version pointer            (store)
 *   SCAN      the writer loads a reader's counter                  (load)
 */
#ifndef ORDERS_H
#define ORDERS_H

#include <stdatomic.h>

#define ORDER_STATS     memory_order_relaxed
#define ORDER_ANNOUNCE  memory_order_seq_cst
#define ORDER_DEREF     memory_order_seq_cst
#define ORDER_EXIT      memory_order_release
#define ORDER_PUBLISH   memory_order_seq_cst
#define ORDER_SCAN      memory_order_seq_cst

#endif
