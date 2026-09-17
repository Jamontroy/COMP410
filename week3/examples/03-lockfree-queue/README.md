# COMP 410 — Advanced Operating Systems
## Week 3 — Readme

## Build

```bash
make
```

This builds the two project binaries in build


## Run

```bash
make test
make tsan
make bench
```
test, runs the initial checks for FIFO and contention
tsan runs the same tests but adds contention
bench runs the tests for the algorithm against mutex with both no "other work" and with it.

## Environment

| Component | Info |
|--|--|
| CPU | AMD Ryzen 9 5900HS |
| Add | Bare metal |
| OS | Ubuntu 24.04 |
| Compiler | gcc 13.3.0 |
| Flags | -O2 -Wall -Wextra -std=c11 |

## File map

- `Makefile` — builds the queue binaries, runs tests, runs the benchmark, and supports ThreadSanitizer and submission packaging
- `README.md` — project overview, build/run commands, environment, and notes
- `REPORT.md` — assignment summary, measurements, and paper connection
- `src/msqueue.c` — lock-free Michael & Scott queue implementation
- `src/mutexqueue.c` — mutex-based reference queue used for comparison
- `src/bench.c` — benchmark harness used to measure throughput under contention
- `src/aba_stack.c` — deliberately buggy ABA example used as a teaching demonstration
- `tests/test_queue.c` — correctness tests for the queue implementation
- `tests/check-submission-spec.sh` — submission-shape validation script
- `tests/make-submission.sh` — creates the final zip submission package
- `bench/METHOD.md` — explanation of the two benchmark workloads
- `bench/results.csv` — benchmark output for the back-to-back workload
- `bench/results-otherwork.csv` — benchmark output for the Michael & Scott “other work” workload

## Notes

I was surprised that after running this on my system that the single lock mutex was faster than my lock-free M&S algorithm.
