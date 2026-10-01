# COMP 410 — Advanced Operating Systems
## Week 5 — Method

<!-- Replace each italic prompt with your own words, a line or two each. This
     file is what lets someone else re-run your audit and get the same shape. -->

## What was measured

*The quantity, named precisely: hand-off latency from `ready_ns` to `start_ns`, and (for 3.0) the invariant's violated time.*

## How

*Your seed and pool size (both are in `bench/audit.csv`), how many runs, and that every figure is a median across runs with its spread.*

## Machine

| Component | Info |
|--|--|
| CPU | ARM11 32-bit CPU |
| Architecture | ARM |
| Interface | Bare Metal |
| OS | Raspbian GNU/Linux 13.6 (trixie)|
| Compiler | gcc 14.2.0 |
| Flags | std=c11 -Wall -Wextra -O2 -pthread -Isrc |

## What these numbers do NOT establish

*One or two honest limits: for example, what a single-node VM cannot show.*
