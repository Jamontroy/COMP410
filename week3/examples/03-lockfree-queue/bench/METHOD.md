# COMP 410 — Advanced Operating Systems
## Week 3 — Method

## What was measured

Running make bench median_time, spread_time, and throughput_ops_s were measured between mutex a single lock algorithm and my rereation of M&S lock-free algorithm

## How
```bash
== back-to-back (no other work) ==
trials=5 other_work=0 spins
queue,threads,other_work,ops_total,median_s,spread_s,throughput_ops_s
lockfree,1,0,40000,0.001276,0.000409,31349338
lockfree,2,0,80000,0.004937,0.000577,16205637
lockfree,4,0,160000,0.012698,0.001014,12600243
lockfree,8,0,320000,0.038948,0.001581,8216075
mutex,1,0,40000,0.000486,0.000133,82263395
mutex,2,0,80000,0.002770,0.000314,28881096
mutex,4,0,160000,0.005607,0.000349,28535698
mutex,8,0,320000,0.014876,0.001246,21510890

== with Michael and Scotts 'other work' between operations ==
trials=5 other_work=2000 spins
queue,threads,other_work,ops_total,median_s,spread_s,throughput_ops_s
lockfree,1,2000,40000,0.031892,0.001243,1254222
lockfree,2,2000,80000,0.032610,0.001803,2453243
lockfree,4,2000,160000,0.036094,0.002929,4432844
lockfree,8,2000,320000,0.038221,0.006824,8372355
mutex,1,2000,40000,0.023987,0.002340,1667540
mutex,2,2000,80000,0.027485,0.000248,2910698
mutex,4,2000,160000,0.029591,0.005617,5407125
mutex,8,2000,320000,0.052063,0.009490,6146344
```

## Machine
| Component | Info |
|--|--|
| CPU | AMD Ryzen 9 5900HS |
| Add | Bare metal |
| OS | Ubuntu 24.04 |
| Compiler | gcc 13.3.0 |
| Flags | -O2 -Wall -Wextra -std=c11 |


## What these numbers do NOT establish

These numbers do not establish how this operation would perform on other machines. This is a single test on a single machine ran once and does not speak for the wide array or architectures and machines where a lockfree algorithm may run faster.
