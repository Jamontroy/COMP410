# COMP 410 — Advanced Operating Systems
## Week 4 — Method

## What was measured

For both the RCU table and the rwlock table, I measured the elapsed time in seconds from releasing the readers to the point when the last reader finished. I also recorded total reads, torn reads, and freed versions to check correctness. The reported throughput is `reads_total / median_s`.

I measured both the RCU table and rwlock table. These were measured using the time is seconds from releasing the readers to the time when the last reader finished. In tandem with this, I recorded total reads, torn reads, and freed versions. The reported throughput is output as read_total / median_s

## How

I tested 1, 2, 4, and 8 reader threads. Each of them went through 2,000,000 lookups while a writer went through 100 updates, with 20,000 spin iterations between every update. For the implementations and reader count, I got rid of one warm up run and then ran 5 timed trials. The table reports the median time and the spread between the slowest and fastest trial. There are 64 different table keys so ther is no random seeding.

## Machine

| Component | Info |
|--|--|
| CPU | AMD Ryzen 9 5900HS |
| Add | Bare metal |
| OS | Ubuntu 24.04 |
| Compiler | gcc 13.3.0 |
| Flags | -O2 -Wall -Wextra -std=c11 |

## What these numbers do NOT establish

These timings are specific to my machine, compiler, operating-system scheduling, and system load, so they do not prove that RCU will be faster in every environment.