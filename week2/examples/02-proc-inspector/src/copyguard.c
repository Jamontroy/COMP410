// COMP 410 - Week 2 - STARTER: the user-copy boundary, modeled in user space.
//
// A kernel module cannot be built or loaded on a machine with no Linux kernel
// headers, so the rules that govern copy_to_user()/copy_from_user() would be
// untestable here.
//
// This program models the boundary instead. `kread`/`kwrite` stand in for the
// real copy helpers: they enforce the same four checks, and return the same
// thing the kernel returns (the number of bytes NOT copied, 0 on success).
// The failures below are the failures a module has, in the order they bite.
//
// What this does NOT model: privilege. Nothing here can oops a machine. It
// models the ARITHMETIC of the boundary, which is where the bugs actually are.

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define KBUF_SZ 64
#define min(a,b) ((a) < (b) ? (a) : (b))

// The "kernel-side" buffer. In a module this is kernel memory: a user pointer
// may never be dereferenced directly, and this buffer may never be handed out.
// kbuf and its guard live in ONE struct so the guard is genuinely adjacent.
// Two separate arrays would let the compiler lay them out in any order, and
// the overflow would miss the guard and corrupt something else instead.
//
// The real kernel has no such guard: an overflow there simply damages
// whatever came next. The guard exists so this program can REPORT the bug
// rather than dying with no output.
#define GUARD_SZ 512
#define GUARD_BYTE 0x7e
static struct {
        char buf[KBUF_SZ];
        char guard[GUARD_SZ];
} kmem;
#define kbuf (kmem.buf)
static size_t kbuf_len;

static void guard_arm(void) { memset(kmem.guard, GUARD_BYTE, sizeof kmem.guard); }
static int  guard_intact(void)
{
        for (size_t i = 0; i < sizeof kmem.guard; i++)
                if ((unsigned char)kmem.guard[i] != GUARD_BYTE)
                        return 0;
        return 1;
}


// A stand-in for the user address space. The only thing that matters for the
// model is that it is a DIFFERENT region: a pointer into one is meaningless
// in the other, which is exactly why the copy helpers exist.
#define UBUF_SZ 64
static char ubuf[UBUF_SZ];

// Model of copy_to_user(). Returns bytes NOT copied, like the kernel's.
//
// Rule 1 (Validate): a null destination is rejected, never dereferenced.
// Rule 2 (Clamp):    copy min(requested, available) -- never the requested
//                    length on faith. Trusting it is the classic info leak:
//                    the extra bytes come from whatever sat next to kbuf.
static unsigned long kwrite(char *udst, size_t ulen, const char *ksrc, size_t klen)
{

        if (!udst)                 // Rule 1
                return ulen;
        // TODO 1 (Rule 2 -- Clamp). Right now this copies whatever length the
        // caller asked for, which hands out `ulen - klen` bytes of memory
        // sitting next to kbuf. Copy no more than the kernel actually holds.

        memcpy(udst, ksrc, min(ulen, klen)); //This is the clamp which we learned in class. You need to use min(ulen,klen) to make ensure it clamps to prevent a memory leak.
        return ulen - min(ulen, klen);
}

// Model of copy_from_user().
//
// Rule 3 (Bound):     never write past the kernel buffer, whatever the user
//                     asked for. This is the overflow that corrupts the kernel.
// Rule 4 (Terminate): the kernel must not assume user data is NUL-terminated.
static unsigned long kread(const char *usrc, size_t ulen)
{
	size_t n;

        if (!usrc)
                return ulen;
        // TODO 2 (Rule 3 -- Bound). kbuf holds KBUF_SZ bytes. This writes as
        // many as the caller offers. Bound it, leaving room for TODO 3.
	n = min(ulen, KBUF_SZ - 1);
        memcpy(kbuf, usrc, n);   // may run into kguard if n is not bounded
        // TODO 3 (Rule 4 -- Terminate). User data may arrive with no NUL.
        // strlen() on kbuf then runs off the end. Terminate it.
	kbuf[n] = '\0'; //terminates the data
        kbuf_len = n;
        return ulen - n;
}

// ---- the four checks, each printed as evidence -------------------------

static int failures;

static void check(const char *name, int ok, const char *detail)
{
        printf("  [%s] %-34s %s\n", ok ? "ok" : "FAIL", name, detail);
        if (!ok)
                failures++;
}

// Sweep mode: for each requested length, report how many bytes the clamp
// actually let through. This is the measurement analyze.sh reports -- the
// numbers come from running kwrite(), not from restating its rule.
static int sweep(void)
{
        static const size_t reqs[] = { 8, 16, 32, 64, 128, 256 };
        char big[512];

        printf("requested,copied,untouched_tail\n");
        for (size_t i = 0; i < sizeof reqs / sizeof reqs[0]; i++) {
                size_t req = reqs[i], copied, tail;
                unsigned long left;

                memset(big, 'X', sizeof big);
                strcpy(kbuf, "hello");
                kbuf_len = 5;

                left = kwrite(big, req, kbuf, kbuf_len);
                // Count what the copy actually changed, rather than trusting
                // the return value -- that is the leak we are looking for.
                copied = 0;
                while (copied < req && big[copied] != 'X')
                        copied++;
                tail = left;
                printf("%zu,%zu,%zu\n", req, copied, tail);
        }
        return 0;
}

int main(int argc, char **argv)
{
        if (argc > 1 && strcmp(argv[1], "--sweep") == 0)
                return sweep();

        char detail[160];
        unsigned long left;

        setvbuf(stdout, NULL, _IONBF, 0);   // report progress even if we die
        guard_arm();

        printf("The user-copy boundary: four rules, four failures they stop\n\n");

        // Rule 1 -- a null user pointer is refused, not dereferenced.
        left = kwrite(NULL, 16, "hello", 5);
        snprintf(detail, sizeof detail,
                 "refused, %lu bytes uncopied (no crash)", left);
        check("Rule 1 Validate: NULL dest", left == 16, detail);

        // Rule 2 -- an over-long read is clamped to what the kernel holds.
        // Without the clamp this returns 64 bytes, 59 of them whatever memory
        // happened to follow the string: a kernel info leak.
        memset(ubuf, 'X', sizeof ubuf);
        strcpy(kbuf, "hello");
        kbuf_len = 5;
        left = kwrite(ubuf, sizeof ubuf, kbuf, kbuf_len);
        snprintf(detail, sizeof detail,
                 "asked 64, copied %zu, %lu left unfilled", kbuf_len, left);
        check("Rule 2 Clamp: over-long read", left == sizeof ubuf - kbuf_len
              && ubuf[5] == 'X', detail);

        // Rule 3 -- a user write longer than the kernel buffer is truncated,
        // not allowed to run off the end.
        {
                char big[256];
                memset(big, 'A', sizeof big);
                left = kread(big, sizeof big);
                snprintf(detail, sizeof detail,
                         "offered 256, stored %zu (buffer holds %d)",
                         kbuf_len, KBUF_SZ - 1);
                if (!guard_intact())
                        snprintf(detail, sizeof detail,
                                 "offered 256, wrote %zu -- RAN PAST kbuf into "
                                 "the guard (in a module this is corruption)",
                                 kbuf_len);
                check("Rule 3 Bound: oversized write",
                      guard_intact() && kbuf_len == KBUF_SZ - 1
                      && left == 256 - (KBUF_SZ - 1), detail);
        }

        // Rule 4 -- unterminated user data is terminated by the kernel before
        // any string function touches it.
        {
                char raw[8];
                memset(raw, 'B', sizeof raw);   // deliberately no NUL
                kread(raw, sizeof raw);
                snprintf(detail, sizeof detail,
                         "stored %zu bytes, strlen reads %zu%s",
                         kbuf_len, strlen(kbuf),
                         strlen(kbuf) == sizeof raw
                                 ? " (stops where the data stops)"
                                 : " -- RAN PAST the data");
                check("Rule 4 Terminate: no NUL sent",
                      strlen(kbuf) == sizeof raw, detail);
        }

        printf("\n%s (%d failure%s)\n", failures ? "SOME CHECKS FAILED" : "All four rules hold",
               failures, failures == 1 ? "" : "s");
        return failures ? 1 : 0;
}
