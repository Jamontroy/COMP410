/*
 * COMP 410 - Week 4 - handshake model checker.
 *
 * A COURSE TOOL: it reads your src/orders.h. Change that file, not this one.
 *
 * It runs EVERY interleaving of one reader and one writer following the
 * protocol in src/rcu.c, on three memory models, and counts the executions in
 * which the reader uses a version the writer has already freed.
 *
 *   reader:  announce (store seq=1)   deref (load head)   use    exit (store seq=2)
 *   writer:  publish (store head=NEW) scan (load seq)     wait while seq unchanged, if odd
 *                                                         free OLD
 *
 * The three memory models:
 *
 *   sequential  every store is visible to every thread the instant it happens.
 *
 *   x86-64 TSO  each thread has a FIFO store buffer. A seq_cst store compiles
 *               to xchg, which waits for the buffer to drain and then writes
 *               memory directly. Every other store compiles to mov, into the
 *               buffer. Every load is a mov: newest entry in its own buffer,
 *               else memory. (Owens, Sarkar and Sewell 2009.)
 *
 *   arm64       each thread has a store buffer. A relaxed store is STR: it
 *               may commit ahead of earlier stores to other addresses. A
 *               release or seq_cst store is STLR: it commits only after every
 *               earlier store. A relaxed or acquire load (LDR, LDAPR) runs at
 *               once; a seq_cst load (LDAR) runs only once every earlier STLR
 *               of its own thread has committed. Stores to one address always
 *               commit in order.
 *
 * These model the instruction semantics the compilers emit - `make orderings`
 * shows them - not any one CPU. A model with no use-after-free is evidence for
 * your orderings on the small case it explores. It is not a proof about a
 * larger program.
 *
 *   ./build/model                        your orderings, from src/orders.h
 *   ./build/model --announce release     the same, with one site overridden
 *   ./build/model --trace                also print the first unsafe execution
 */
#include "orders.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { RELAXED, ACQUIRE, RELEASE, SEQCST };
static const char *ORDER_NAME[] = { "relaxed", "acquire", "release", "seq_cst" };

enum { S_ANNOUNCE, S_DEREF, S_EXIT, S_PUBLISH, S_SCAN, NSITES };
static const char *SITE_NAME[]     = { "announce", "deref", "exit", "publish", "scan" };
static const int   SITE_IS_STORE[] = { 1, 0, 1, 1, 0 };
static int order[NSITES];

enum { M_SC, M_TSO, M_ARM, NMODELS };
static const char *MODEL_NAME[] = { "sequential", "x86-64 TSO", "arm64" };
static int model;

enum { HEAD, SEQ };
enum { OLD = 1, NEW = 2 };
enum { R, W };

#define MAXBUF   4
#define MAXTRACE 24
#define TRACELEN 72

typedef struct { int var, val, stlr; } entry;

typedef struct {
    int   mem[2];
    entry buf[2][MAXBUF];
    int   nbuf[2];
    int   pc[2];
    int   p;          /* the version the reader loaded */
    int   snap;       /* the counter the writer saw */
    int   freed;      /* OLD has been freed */
    int   uaf;        /* the reader used OLD after the free */
    int   ntrace;
    char  trace[MAXTRACE][TRACELEN];
} state;

static long  n_exec, n_uaf, n_stuck;
static state first_uaf;

static int from_c(memory_order o)
{
    if (o == memory_order_seq_cst) return SEQCST;
    if (o == memory_order_release) return RELEASE;
    if (o == memory_order_acquire) return ACQUIRE;
    return RELAXED;
}

static const char *val_name(int var, int val)
{
    static char buf[8];
    if (var == HEAD)
        return val == OLD ? "OLD" : "NEW";
    snprintf(buf, sizeof buf, "%d", val);
    return buf;
}

static void note(state *s, const char *fmt, ...)
{
    if (s->ntrace >= MAXTRACE)
        return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(s->trace[s->ntrace++], TRACELEN, fmt, ap);
    va_end(ap);
}

/* What thread t sees when it loads var: its own newest buffered store, else memory. */
static int read_var(const state *s, int t, int var)
{
    if (model != M_SC)
        for (int i = s->nbuf[t] - 1; i >= 0; i--)
            if (s->buf[t][i].var == var)
                return s->buf[t][i].val;
    return s->mem[var];
}

static int load_enabled(const state *s, int t, int site)
{
    if (model == M_ARM && order[site] == SEQCST)      /* LDAR */
        for (int i = 0; i < s->nbuf[t]; i++)
            if (s->buf[t][i].stlr)
                return 0;
    return 1;
}

static int store_enabled(const state *s, int t, int site)
{
    if (model == M_TSO && order[site] == SEQCST)      /* xchg */
        return s->nbuf[t] == 0;
    return 1;
}

/* Returns 1 when the store went straight to memory. */
static int do_store(state *s, int t, int site, int var, int val)
{
    if (model == M_SC || (model == M_TSO && order[site] == SEQCST)) {
        s->mem[var] = val;
        return 1;
    }
    entry e = { var, val, model == M_ARM && order[site] != RELAXED };
    s->buf[t][s->nbuf[t]++] = e;
    return 0;
}

static int flush_enabled(const state *s, int t, int i)
{
    if (model == M_TSO)
        return i == 0;
    for (int j = 0; j < i; j++) {
        if (s->buf[t][j].var == s->buf[t][i].var)
            return 0;                                 /* one address keeps its order */
        if (s->buf[t][i].stlr)
            return 0;                                 /* STLR waits for every earlier store */
    }
    return 1;
}

static void explore(const state *s);

static void step_store(const state *s, int t, int site, int var, int val, int next_pc)
{
    if (!store_enabled(s, t, site))
        return;
    state c = *s;
    int direct = do_store(&c, t, site, var, val);
    note(&c, "%c %-8s store %s=%s %s", t == R ? 'R' : 'W', SITE_NAME[site],
         var == HEAD ? "head" : "seq", val_name(var, val),
         direct ? "(to memory)" : "(into its store buffer)");
    c.pc[t] = next_pc;
    explore(&c);
}

static void explore(const state *s)
{
    int moved = 0;
    long before = n_exec + n_stuck;

    /* ---- reader ---- */
    switch (s->pc[R]) {
    case 0:
        step_store(s, R, S_ANNOUNCE, SEQ, 1, 1);
        break;
    case 1:
        if (load_enabled(s, R, S_DEREF)) {
            state c = *s;
            c.p = read_var(&c, R, HEAD);
            note(&c, "R deref    load head -> %s", val_name(HEAD, c.p));
            c.pc[R] = 2;
            explore(&c);
        }
        break;
    case 2: {
        state c = *s;
        if (c.p == OLD && c.freed) {
            c.uaf = 1;
            note(&c, "R use      reads OLD, which W freed: USE-AFTER-FREE");
        } else {
            note(&c, "R use      reads %s", val_name(HEAD, c.p));
        }
        c.pc[R] = 3;
        explore(&c);
        break;
    }
    case 3:
        step_store(s, R, S_EXIT, SEQ, 2, 4);
        break;
    }

    /* ---- writer ---- */
    switch (s->pc[W]) {
    case 0:
        step_store(s, W, S_PUBLISH, HEAD, NEW, 1);
        break;
    case 1:
        if (load_enabled(s, W, S_SCAN)) {
            state c = *s;
            c.snap = read_var(&c, W, SEQ);
            note(&c, "W scan     load seq -> %d%s", c.snap,
                 c.snap % 2 ? " (inside: wait)" : " (outside: free now)");
            c.pc[W] = c.snap % 2 ? 2 : 3;
            explore(&c);
        }
        break;
    case 2:
        if (load_enabled(s, W, S_SCAN) && read_var(s, W, SEQ) != s->snap) {
            state c = *s;
            note(&c, "W wait     load seq -> %d (changed)", read_var(&c, W, SEQ));
            c.pc[W] = 3;
            explore(&c);
        }
        break;
    case 3: {
        state c = *s;
        c.freed = 1;
        note(&c, "W free     frees OLD");
        c.pc[W] = 4;
        explore(&c);
        break;
    }
    }

    /* ---- store buffers commit ---- */
    for (int t = R; t <= W; t++)
        for (int i = 0; i < s->nbuf[t]; i++) {
            if (!flush_enabled(s, t, i))
                continue;
            state c = *s;
            entry e = c.buf[t][i];
            c.mem[e.var] = e.val;
            memmove(&c.buf[t][i], &c.buf[t][i + 1], (size_t)(c.nbuf[t] - i - 1) * sizeof e);
            c.nbuf[t]--;
            note(&c, "%c buffer   commits %s=%s to memory", t == R ? 'R' : 'W',
                 e.var == HEAD ? "head" : "seq", val_name(e.var, e.val));
            explore(&c);
        }

    moved = (n_exec + n_stuck) != before;
    if (moved)
        return;

    /* Nothing could move: an execution has ended. */
    if (s->pc[R] == 4 && s->pc[W] == 4 && s->nbuf[R] == 0 && s->nbuf[W] == 0) {
        n_exec++;
        if (s->uaf && n_uaf++ == 0)
            first_uaf = *s;
    } else {
        n_stuck++;
    }
}

static int parse_order(const char *s)
{
    for (int o = RELAXED; o <= SEQCST; o++)
        if (strcmp(s, ORDER_NAME[o]) == 0)
            return o;
    return -1;
}

int main(int argc, char **argv)
{
    int trace = 0;
    order[S_ANNOUNCE] = from_c(ORDER_ANNOUNCE);
    order[S_DEREF]    = from_c(ORDER_DEREF);
    order[S_EXIT]     = from_c(ORDER_EXIT);
    order[S_PUBLISH]  = from_c(ORDER_PUBLISH);
    order[S_SCAN]     = from_c(ORDER_SCAN);

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--trace") == 0) {
            trace = 1;
            continue;
        }
        int site = -1;
        for (int k = 0; k < NSITES; k++)
            if (strncmp(argv[i], "--", 2) == 0 && strcmp(argv[i] + 2, SITE_NAME[k]) == 0)
                site = k;
        int o = (site >= 0 && i + 1 < argc) ? parse_order(argv[i + 1]) : -1;
        if (site < 0 || o < 0) {
            fprintf(stderr, "usage: %s [--trace] [--announce|--deref|--exit|--publish|--scan "
                            "relaxed|acquire|release|seq_cst] ...\n", argv[0]);
            return 2;
        }
        order[site] = o;
        i++;
    }

    for (int k = 0; k < NSITES; k++) {
        if (SITE_IS_STORE[k] && order[k] == ACQUIRE) {
            fprintf(stderr, "%s is a store: acquire is an ordering for loads (the compiler warns "
                            "-Winvalid-memory-model)\n", SITE_NAME[k]);
            return 2;
        }
        if (!SITE_IS_STORE[k] && order[k] == RELEASE) {
            fprintf(stderr, "%s is a load: release is an ordering for stores (the compiler warns "
                            "-Winvalid-memory-model)\n", SITE_NAME[k]);
            return 2;
        }
    }

    printf("Handshake model checker - one reader, one writer, every interleaving.\n\n");
    printf("orderings:");
    for (int k = 0; k < NSITES; k++)
        printf(" %s=%s", SITE_NAME[k], ORDER_NAME[order[k]]);
    printf("\n\n%-12s %11s %15s\n", "model", "executions", "use-after-free");

    int unsafe[NMODELS] = { 0 };
    state first[NMODELS];
    for (model = M_SC; model < NMODELS; model++) {
        n_exec = n_uaf = n_stuck = 0;
        state s;
        memset(&s, 0, sizeof s);
        s.mem[HEAD] = OLD;
        s.mem[SEQ]  = 0;
        explore(&s);
        printf("%-12s %11ld %15ld\n", MODEL_NAME[model], n_exec, n_uaf);
        if (n_stuck)
            printf("  (%ld executions could not finish - the model deadlocked)\n", n_stuck);
        unsafe[model] = n_uaf > 0;
        first[model] = first_uaf;
    }

    int any = unsafe[M_SC] || unsafe[M_TSO] || unsafe[M_ARM];
    printf("\n");
    if (!any) {
        printf("SAFE: no execution on any of the three models uses a freed version.\n");
    } else {
        printf("UNSAFE on:");
        for (int m = 0; m < NMODELS; m++)
            if (unsafe[m])
                printf(" %s;", MODEL_NAME[m]);
        printf(" run with --trace to see the first such execution.\n");
    }

    if (trace)
        for (int m = 0; m < NMODELS; m++) {
            if (!unsafe[m])
                continue;
            printf("\nfirst use-after-free on %s:\n", MODEL_NAME[m]);
            for (int i = 0; i < first[m].ntrace; i++)
                printf("  %2d. %s\n", i + 1, first[m].trace[i]);
        }
    return 0;
}
