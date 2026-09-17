# COMP 410 — Advanced Operating Systems
## Week 3 — Report

## What

In this assignment, I built the main functions of the enqueue and dequeue utilizing compare-and-swap while avoiding and preventing issues with ABA. I build this on x64 architecture. For TODO 1: I built a simple compare and swap which moves the tail to the new node if the value after the tail is NULL. The first IF statement checks if the CAS can successfully swap to the new tail, and then it does it.  The focus is on count of each tagged to make sure that if an ABA pops up, the CAS will fail and the loop will retry. For TODO 2: I built out the else of the IF next.ptr == NULL meaning I built what the enqueue does if the tail is lagging behind. I created a new tagging laggingtail which with CAS increments through the queue by checking if the expected value is equal to &tail. If it is not equal to &tail the loop retries with a new incremented value. If it succeeds it moves the tail to the new node. TODO 3: works with Dequeue and specifically the else statement if the head and tail pointer are not on the same node. With dequeue we are looking to return all of the values out similarly to a stack where they come out of the head first. So we decrement the head down and release the values out the top. So we start by storing the value of the next pointer in val. We then set up a new tagged newhead to move head to the next pointer, which we then run compare and swap to move head to the next pointer down the queue, which we then output the value that was stored there and was being held by val and put that to *out to return. following that we free the space taken by the old head. A critical component with all of these TODO's is implementing them with counter. The whole point of tagged is to have a struct that can keep track of both pointer and count for a node. If an ABA occurs from another thread before the CAS can take place, the counter will mismatch and cause the loop to retry.

To specifically handle the memory ordering I used the three simple atomic operations of memory_order_acquire, memory_order_release, and memory_order_relaxed. memory_order_acquire is for loading the order of the queue to test if it has changed at all. It's important to do this because they are accessing memory which is shared and could be overwritten by another thread. Second, memory_order_release is used after the next node is is linked into the queue. memory_order_release makes this new node visible in memory to other threads so that it doesn't get overwritten. The opposite is for memory_order_relaxed. This is when the CAS does not add a new node onto the end of the list and nothing is taken or republished into the shared state. This is the memory ordering I chose for the lock-free mechanism as it maintains specific data on each head, tail, etc. while also keeping the memory visible in the shared thread space.

## Results

From the simple test of *make test*, I passed all of the requirements.

```bash
== a queue is FIFO ==
  [ok]   create returns a queue
  [ok]   dequeue on an empty queue returns false
  [ok]   three values come back in the order they went in
  [ok]   the queue is empty again afterwards

== it survives contention ==
  [ok]   every enqueued value is dequeued exactly once (count)
  [ok]   no value is dequeued twice
  [ok]   no value appears that was never enqueued
```

Similarly with *make tsan* my code passes the test with the race condition
```bash
== a queue is FIFO ==
  [ok]   create returns a queue
  [ok]   dequeue on an empty queue returns false
  [ok]   three values come back in the order they went in
  [ok]   the queue is empty again afterwards

== it survives contention ==
  [ok]   every enqueued value is dequeued exactly once (count)
  [ok]   no value is dequeued twice
  [ok]   no value appears that was never enqueued

  retries under contention: 22592
```

*make bench* returns the two workloads similar to those ran by Michael and Scott. These results are explored deeper in METHOD.md
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

## Paper connection

Michael and Scott in their paper attack the problem of finding a suitable non-blocking lock-free algorithm. Prior to their paper, many researchers had tried non-blocking algorithms but to little success with major caveats. M&S implemented a non-blocking algorithm with atomic reads, special structs to hold both a pointer and a counter, and two CAS operations for the enqueue and dequeue. Their "new non-blocking" algorithm outperforms the single-lock, two-lock, and all other algorithms they tested in their multiprocessor Silicon Graphics Challenge.

After recreating their algorithm and experiment on my modern Linux x64 machine with an 8 core processor. When compared against a standard mutex single locking algorithm, the mutex outperformed the lockfree algorithm across both no other work and other work trials. In my head this seems counter intuitive, how is a blocking system out performing a block-free system. I have done a little digging and I have a theory as to why. A single lock mechanism will have much less to write and will be a much quicker operation than M&S algorithm because it simply does not have as many operations. M&S is performing several atomic loads, checking and re-checking available space, and performs two CAS. A single lock is simply locking, writing to two pointers, then unlocking. This is a much faster operation which has no risk of ABA since it locks the queue during operation. I am not entirely sure but that is my theory as to why the single-lock mutex is outperforming the lock-free algorithm presented by M&S

## Citations

Michael M., Scott M., 1996 *Simple, Fast, and Practical Non-Blocking and Blocking Concurrent Queue Algorithms*
