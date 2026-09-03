# COMP 410 — Advanced Operating Systems
## Week 1 — Example: syscall-boundary profiler (starter)

Measure what a system call costs, and how much of that is the boundary rather
than the work.

This is the starter you extend for **A1**. The scaffolding — argument parsing,
warm-up, trials, output — is written. **Three TODOs** in `src/nullcall.c` are
yours: the syscall loop, the control loop, and the median.

## Run

```bash
make          # build
make test     # check your measurement is sound
make bench    # run it and print the numbers for your report
```

> **Toolchain.** A C compiler and a shell — that is the whole list. There is
> nothing to install and no environment to activate. `make test` runs
> `tests/check.sh`, which uses only `sh`, `awk` and `grep`.
>
> Using a different compiler? Override it without editing the `Makefile`:
> `make CC=gcc-14`. If the build does not run at all, the full walkthrough is in
> `notes/systems-setup-guide.pdf`.
>
> **Never commit or submit build artefacts** — the `nullcall` binary and any
> `*.o` files. `make clean` removes them.

## What you are building

Two loops, timed identically, differing only in what they call:

| loop | calls | crosses the boundary? |
|---|---|---|
| syscall | `getppid()` | **yes** |
| control | `noop()` | no — an ordinary function call |

Subtracting the control removes the loop counter, the branch and the call
sequence, leaving the part attributable to the crossing. **Without a control you
are quoting the cost of a loop as if it were the cost of a system call** — which
is one of the flaws in this week's Final Challenge.

`getppid()` is chosen because it does almost nothing: it returns a number the
kernel already holds. Ousterhout used `getpid()` for the same reason in 1990, and
his paper is this week's reading.

## The trap to know about

An optimising compiler will happily delete a loop whose results nobody uses. If
your control reports **0.00 ns/call**, that is what happened — the program says
so and exits non-zero rather than letting you publish the number.

Three things prevent it, and all three are already in the code:

- `noop()` is marked `noinline`, so there is a real call;
- it increments a `volatile` global, so its result escapes;
- your loops must accumulate the return values into the `sink`.

## What to report for A1

- your **machine**: CPU, OS and version — syscall costs differ by more than 2×
  across kernels and mitigation settings;
- **median and spread** across trials, not a single run;
- the **crossing cost** (syscall minus control), not the raw syscall figure;
- one sentence on **what the number does not include**.

## Files

```
src/nullcall.c      the measurement — three TODOs
tests/check.sh      soundness checks (shell only, no dependencies)
Makefile            build / test / bench
```
