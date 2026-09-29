# COMP 410 — Advanced Operating Systems
## Week 4 — Readme

## Build

Run `make`

## Run

- Run `make test` to run the course tests, my TODO 6 and TODO 7 tests
- Run `make bench` to compare RCU with the rwlock baseline and write results to `bench/results.csv`
- Run `make model` to check the selected memory orderings on the sequential, x86-64 TSO, and arm64 models.
- Run `make orderings` to show the compiler instructions for each ordering
- Run `make tsan` to run the course test under ThreadSanitizer
- Run `make analyze` to summarize the benchmark CSV.

## Environment

| Component | Info |
|--|--|
| CPU | AMD Ryzen 9 5900HS |
| Add | Bare metal |
| OS | Ubuntu 24.04 |
| Compiler | gcc 13.3.0 |
| Flags | -O2 -Wall -Wextra -std=c11 |

## File map

`Makefile`: builds and runs the tests, model, benchmark, ordering report, and submission checks.
`src/rcu.c`: implements reader registration, read-side entry and exit, pointer publication, dereferencing, and grace-period synchronization.
`src/rcu.h`: defines the RCU reader API and reader state.
`src/table.c`: implements the RCU table, safe version updates, reclamation, and the rwlock comparison table.
`src/table.h`: defines the table and version data structures and APIs.
`src/orders.h`: selects the C11 memory ordering for each atomic operation site.
`src/bench.c`: compares RCU and rwlock read-side scaling and writes CSV results.
`src/analyze.sh`: summarizes the benchmark results and applies no final verdict by itself.
`tests/test_rcu.c`: contains the provided course tests.
`tests/test_mine.c`: contains the TODO 6 and TODO 7 tests for overlapping and repeatedly re-entering readers.
`tests/check_submission_ready.py`: checks the report, manifest, benchmark schema, and submission layout.
`tests/check-submission-spec.sh`: checks the required submission structure.
`tests/test_schema.py`: validates the expected submission schema.
`tests/make-submission.sh`: creates the source-only submission archive.
`model/handshake.c`: explores all interleavings of one reader and one writer under three memory models.
`REPORT.md`: records the implementation explanation, ordering answers, test results, and benchmark results.
`submission.json`: stores the submission metadata and entrypoint.

## Notes

None

