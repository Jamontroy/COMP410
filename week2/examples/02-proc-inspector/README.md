# COMP 410 — Advanced Operating Systems
## Week 2 — Starter: `/proc` inspector

The starting point for **A2**. Six TODOs; `make test` goes **5 → 12**.

```bash
make          # build the two user-space models
make test     # 5 of 12 pass before you start
make bench    # see what the models print
```

**The module is built separately, in the guest:**

```bash
cd module && make          # needs Linux + kernel headers
sudo insmod procstat.ko
cat /proc/comp410_procstat
sudo rmmod procstat
cat /proc/comp410_procstat # should now be "No such file or directory"
```

> **Work in the QEMU guest, not on your host.** A module bug takes down the
> kernel it is loaded into. In the guest that is a reboot. See
> the **Week 1 systems setup guide** on Sakai.

## The six TODOs

| # | File | What |
|---|---|---|
| 1 | `src/copyguard.c` | **Rule 2 — Clamp.** Copy no more than the kernel holds. |
| 2 | `src/copyguard.c` | **Rule 3 — Bound.** Never write past `kbuf`. |
| 3 | `src/copyguard.c` | **Rule 4 — Terminate.** User data may arrive with no `NUL`. |
| 4 | `module/procstat.c` | Take the RCU read lock around the task walk. |
| 5 | `module/procstat.c` | Guard the read counter with the mutex. |
| 6 | `module/procstat.c` | Check `proc_create` and let `init` fail properly. |

## What the two models are for

`copyguard` and `lifecycle` are **user-space models**, not the kernel. They
reproduce the arithmetic of the user-copy boundary and the module lifecycle so
you can test your understanding on your own machine, with nothing at risk and no
guest needed.

They cannot model privilege — nothing here can oops a machine. What they model is
where the bugs actually are: lengths, bounds, and what is left behind.

## Your first run fails, on purpose

`./copyguard` reports three failing rules before you start, including:

```
[FAIL] Rule 3 Bound: oversized write   offered 256, wrote 256 -- RAN PAST kbuf
                                       into the guard (in a module this is
                                       corruption)
```

That guard region exists so this program can **tell** you it overflowed instead
of crashing. A real kernel buffer has no guard: the same bug silently corrupts
whatever came next, which is why rule 3 matters.

## Before you submit

Read section 3 of `handouts/a2-handout.pdf` — *How it is graded* — **before** you
start rather than after.
