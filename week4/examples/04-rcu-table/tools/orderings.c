/*
 * COMP 410 - Week 4 - what does the compiler do with a memory ordering?
 *
 * `make orderings` compiles this file to assembly and prints the instruction
 * each function uses to touch `gp`. Nothing here runs.
 *
 * A store takes relaxed, release or seq_cst; a load takes relaxed, acquire or
 * seq_cst. The compiler warns on the other combinations.
 */
#include <stdatomic.h>

_Atomic(void *) gp;

void store_relaxed(void *p) { atomic_store_explicit(&gp, p, memory_order_relaxed); }
void store_release(void *p) { atomic_store_explicit(&gp, p, memory_order_release); }
void store_seqcst(void *p)  { atomic_store_explicit(&gp, p, memory_order_seq_cst); }

void *load_relaxed(void) { return atomic_load_explicit(&gp, memory_order_relaxed); }
void *load_acquire(void) { return atomic_load_explicit(&gp, memory_order_acquire); }
void *load_seqcst(void)  { return atomic_load_explicit(&gp, memory_order_seq_cst); }
