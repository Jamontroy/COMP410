# COMP 410 — Advanced Operating Systems
## Week 4 — Report

<!-- Replace the italic prompt under each heading with your own words. Keep the headings: the specification names them, and `make test` checks that they are here. -->

## Answers

<!-- One per line: fill in the right-hand side. The layout makes each answer
     easy to find; it is not a mark in itself. Section 2 of the handout says
     what each line needs. -->

```bash
read_path_calls: rcu_read_lock, atomic_store_explicit, rcu_dereference, atomic_load_explicit, consistent, rcu_read_unlock, atomic_store_explicit, atomic_fetch_add_explicit
uaf_announce_release_tso: 3
uaf_announce_release_arm64: 0
uaf_deref_acquire_tso: 0
uaf_deref_acquire_arm64: 12
verdict_1_reader: not separated
verdict_8_readers: rcu
free_line: src/table.c:91
```

## What

In this assignment I implemented a read-copy-update table similar to the one described in the Mckenney-Slingwine paper on x86-64 architecture. Beginning in rcu.c TODO 1 is the simply creating the function which a reader can call to enter the read side area. This is done through a atomic_store using ORDER_ANNOUNCE. TODO 2 is similar. It is a simple unlock function which can be called by a reader to leave the critical section. The syntax is also similar where the local sequence is updated and then ORDER_EXIT is used to tell the table that the reader has exited the read side area. TODO 3 is where things get more interesting and complex. This is the implementation of the synchronize RCU where the RCU table is being fully synced and ensuring all of the Readers are out of the critical section. As the instructions stated, I implemented a lock while the program is reading through the sequence of each possible reader in the registry using ORDER_SCAN. If the sequence is odd it adds that reader to the list of readers it needs to wait on. This is followed by the unlock to allow registration changes while the updater waits. It then iterates through the readers waiting array and scans their sequence again to see if they are still in the critical area. If the sequence is even it doesn't mess with them as they have already left. If odd, the while loop explicitly waits for it to end. In the end these loops ensure that every reader successfully. To finish off the RCU table updating there is TODO 4 which implements the update on the table side. Thinking about how M&S RCU worked, no other thread could see changes made to an area until it was all consolidated and fully published. This code snippet does the updating to the new version which was changed. The pointer is reassigner to the new version (nv), synchronize_rcu is called to ensure all of the readers are out of the old area, and the old area is freed.

For TODO 5: The ordering follows as such:
| Site | Ordering | Pattern | Why it's the weakest safe on x86-64 and arm64 |
|---|---|---|---|
| STATS | relaxed | counter | is relaxed because it doesn't have any control over the RCU algorithm, if it sees an older statistic for a moment, it won't cause a reader to use free memory. It doesn't need a stronger ordering. |
| ANNOUNCE | seq_cst | handshake | I chose seq_cst because the reader needs to announce that its in the critical section before it loads the table pointer. This is a more critical need to have ordered memory as without it a writer could miss the reader and free the old table while the reader is in it. I tested this with Command A, trying ANNOUNCE with release and model returned unsafe on x86-64 TSO. Thus seq_cst is the weakest it can be. |
| DEREF | seq_cst | handshake | I also chose seq_cst because this is where the reader loads the published table pointer. The readers ANNOUNCE stored and its DEREF must not reorder with each other. Thus it would need a stricter memory order. I tested DEREF with Command B, trying DEREF with acquire ordering and model came back as unsafe on arm_64. Thus seq_cst is the weakest it can be. |
| EXIT | release | handshake | For EXIT I chose release, because when EXIT is called all of the readers need to be done with the old version. This means every read of the old version must happen before it. Nothing after exit needs to be ordered, thus release. I tested EXIT with relaxed and model reported safe. This one is an odd one since release also reports safe, but the handout says "relaxed for EXIT passes both machine models, and ThreadSanitizer on Linux still reports a race." So I'll keep it at release as the weakest |
| PUBLISH | seq_cst | handshake | I chose seq_cst because the writer must finish building the new version before making its pointer visible to readers. This makes it so that it needs ordering as it need to make sure the initialization of nv comes before the pointer publication. I tested PUBLISH with release and the model returned unsafe on x86-64 TSO. Thus seq_cst is the weakest which can be allowed |
| SCAN | seq_cst | handshake | I chose seq_cst because the writer uses it to search for which readers are in the critical area. This needs to have a more precise memory ordering as a scan with a weaker order could allow the writer to use an old observation to reorder the scan in a way which free the old memory prematurely. Finally I tested SCAN with release and it returned unsafe on ARM_64. Thus the weakest alloed must be seq_cst |

For the safety arguement, starting from free(old) in table.c, its safe because the updater first waits until every reader that could still be using old has left its critical section. Remember the reader is always going to declare that it is in the critical section with its counter, if a writer sees it as odd, the writer will wait to change. When the reader eventually makes its counter even  it calls EXIT with release ordering. When the writer's SCAN with seq_cst ordering loads and sees that exit or a later counter from the same reader, its ordering makes the reader's use of old happen-before the writer continues to free(old). If instead the reader announces after the writer's scan, the seq_cst order goes PUBLISH, SCAN, ANNOUNCE, then DEREF. Since PUBLISH happens before the reader loads the pointer, that load gets nv, not old. So the writer either waits for a reader that could have the old version, or a reader starts late enough to get the new version.

Moving on to TODO 6 & 7 these were the real beasts of this assignment. 

For 6, I had to set up the test where there are 2 readers still inside the critical section. This is testing the methods which we implemented in TODO 1-4 and making sure that the table can properly wait for both readers to leave before updating the table. I created a struct overlap_reader to hold the reader pointer and flags as to whether the reader is inside or left. within the mine_two_readers_both_waited_for I initialized the RCU table and creates the readers. It sends both of the readers into the critical section then runs the tests. It runs the update thread which runs rt_update. This if we rememeber from TODO 4 updates the table and runs synchonize_rcu() to make sure the readers leave the critical section. It then gives the updater 20ms for synchronize_rcu to run. Now since the test controls both of the readers flags and we need to pass them through, this first check both readers should still be in the critical section and the RCU_table shouldn't be released. We check it against the baseline "base" we set earlier of amount of released tables. Then for Reader[0] we change its flag to leave the area then checks rt_freed again. This time the table still shouldn't be released because Reader[1] is still inside. Then we change Reader[1]'s leave flag so that it leave the critical section. Now when we run the test base should have incremented by 1 as the updater/synchronize_rcu should have freed the table incrementing the number of freed tables by 1.

For 7, I am implementing a scenario where there is a busy reader who repeatedly enters and leaves the critical section every 1ms. This is testing that the updater does not get stuck waiting for one reader to become even, because the reader can leave one section and quickly enter another one. I created a struct busy_arg to hold the table pointer and the stop and done flags. Within the mine_busy_reader_does_not_stall function I initialized the RCU table and saved the starting number of freed versions. I then created one thread for the busy reader and another thread for the updater, which runs 50 updates on its own. The test waits up to 1.5 seconds for the updater to set the done flag. After this time limit, it sets the stop flag so the reader can finish and then joins the updater, which also means that a broken synchronize_rcu can be stopped by the test alarm instead of making the test wait forever. To pass, the updater must finish within the time limit, synchronize_rcu must have recorded at least one wait, and exactly 50 old versions must have been freed. This confirms that the reader can keep re-entering the critical section without stalling the updates.

## Results
From make test
== course tests ==
  [ok]   a published pointer is visible to a later dereference
  [ok]   a registered reader outside any section does not make synchronize_rcu() wait
  [ok]   synchronize_rcu() waits for a reader that is inside its section
  [ok]   rt_update() publishes the next generation
  [ok]   the old version is freed after the reader leaves, and not before
  [ok]   50 updates free exactly 50 old versions
  [ok]   4 readers and 200 updates: no torn read, and every old version freed
  7 of 7 course tests pass

== your tests ==
  [ok]   two readers inside at once: the update waits for both
  [ok]   a reader re-entering every 1 ms does not stall 50 updates
  2 of 2 of your tests pass

From make tsan
```bash
  [ok]   a published pointer is visible to a later dereference
  [ok]   a registered reader outside any section does not make synchronize_rcu() wait
  [ok]   synchronize_rcu() waits for a reader that is inside its section
  [ok]   rt_update() publishes the next generation
  [ok]   the old version is freed after the reader leaves, and not before
  [ok]   50 updates free exactly 50 old versions
  [ok]   4 readers and 200 updates: no torn read, and every old version freed
  7 of 7 course tests pass

```
### Testing the memory orderings

  From make model
  ```bash
  orderings: announce=seq_cst deref=seq_cst exit=release publish=seq_cst scan=seq_cst

  model         executions  use-after-free
  sequential            19               0
  x86-64 TSO            26               0
  arm64                105               0

  SAFE: no execution on any of the three models uses a freed version.
  ```

  From make model MODEL_ARGS="--announce release --deref seq_cst --exit seq_cst --publish seq_cst --scan seq_cst" 
  Command A ANNOUNCE release
  ```bash
  orderings: announce=release deref=seq_cst exit=seq_cst publish=seq_cst scan=seq_cst

  model         executions  use-after-free
  sequential            19               0
  x86-64 TSO           128               3
  arm64                105               0

  UNSAFE on: x86-64 TSO; run with --trace to see the first such execution.
  ```

  From make model MODEL_ARGS="--announce seq_cst --deref acquire --exit seq_cst --publish seq_cst --scan seq_cst"

  Command B DEREF acquire
  ```bash
  orderings: announce=seq_cst deref=acquire exit=seq_cst publish=seq_cst scan=seq_cst

  model         executions  use-after-free
  sequential            19               0
  x86-64 TSO            19               0
  arm64                582              12

  UNSAFE on: arm64; run with --trace to see the first such execution.
  ```

  From make model MODEL_ARGS="--publish release"
  - PUBLISH release
  - Testing publish with release memory ordering is unsafe

  ```bash

  orderings: announce=seq_cst deref=seq_cst exit=release publish=release scan=seq_cst

  model         executions  use-after-free
  sequential            19               0
  x86-64 TSO           187              13
  arm64                105               0

  UNSAFE on: x86-64 TSO; run with --trace to see the first such execution.

  ```

  From make model MODEL_ARGS="--scan acquire"
  - SCAN acquire
  - Testing SCAN with acquire memory ordering is unsafe

  ```bash
  orderings: announce=seq_cst deref=seq_cst exit=release publish=seq_cst scan=acquire

  model         executions  use-after-free
  sequential            19               0
  x86-64 TSO            26               0
  arm64                353              43

  UNSAFE on: arm64; run with --trace to see the first such execution.
  ```

  From make model MODEL_ARGS="--exit relaxed"
  - EXIT relaxed
  - Testing EXIT with relaxed memory ordering is safe

  ```bash
  Handshake model checker - one reader, one writer, every interleaving.

  orderings: announce=seq_cst deref=seq_cst exit=relaxed publish=seq_cst scan=seq_cst

  model         executions  use-after-free
  sequential            19               0
  x86-64 TSO            26               0
  arm64                105               0

  SAFE: no execution on any of the three models uses a freed version.

  ```

From make orderings
```bash
== the instruction cc  uses for each ordering ==

  store_relaxed  movq %rdi, gp(%rip)
  store_release  movq %rdi, gp(%rip)
  store_seqcst   xchgq gp(%rip), %rdi
  load_relaxed   movq gp(%rip), %rax
  load_acquire   movq gp(%rip), %rax
  load_seqcst    movq gp(%rip), %rax

```

From make bench
```bash
impl,readers,trials,reads_per_reader,reads_total,updates,torn_reads,freed_versions,median_s,spread_s,throughput_reads_s
rcu,1,5,2000000,2000000,100,0,100,0.018108,0.001304,110447530
rcu,2,5,2000000,4000000,100,0,100,0.017881,0.001756,223700885
rcu,4,5,2000000,8000000,100,0,100,0.017035,0.007632,469618666
rcu,8,5,2000000,16000000,100,0,100,0.020066,0.001144,797367929
rwlock,1,5,2000000,2000000,100,0,0,0.021512,0.007360,92972778
rwlock,2,5,2000000,4000000,100,0,0,0.161782,0.003690,24724648
rwlock,4,5,2000000,8000000,100,0,0,0.430565,0.005834,18580217
rwlock,8,5,2000000,16000000,100,0,0,1.144832,0.013180,13975848
```

```bash
How to read this:
  gap    = |median(rcu) - median(rwlock)|
  larger = the larger of the two spreads (spread = slowest - fastest trial)
  The handout states the decision rule that uses these two columns.

readers   rcu median (spread) s  rwlock median (spread) s      gap s     larger
      1     0.018108 (0.001304)     0.021512 (0.007360)   0.003404   0.007360
      2     0.017881 (0.001756)     0.161782 (0.003690)   0.143901   0.003690
      4     0.017035 (0.007632)     0.430565 (0.005834)   0.413530   0.007632
      8     0.020066 (0.001144)     1.144832 (0.013180)   1.124766   0.013180

Throughput relative to one reader (reads/s at N readers / reads/s at 1):
readers         rcu      rwlock
      1       1.00x       1.00x
      2       2.03x       0.27x
      4       4.25x       0.20x
      8       7.22x       0.15x

Exact counts (these hold on every machine when the code is correct):
  all hold: reads_total = readers x reads_per_reader, torn_reads = 0, rcu freed_versions = updates
```
Your job - none of this is printed above:
1. I applied the handout's rule at every reader count, the medians a different only when the gap between them is greater than the larger spread; otherwise, they aren't separated. With 1 reader, they are not separated because the 0.003404-second gap is less than the 0.007360-second larger spread. With 2 readers, RCU is faster because the 0.143901-second gap is greater than the 0.003690-second spread. With 4 readers, RCU is faster because the 0.413530-second gap is greater than the 0.007632-second spread. With 8 readers, RCU is faster because the 1.124766-second gap is greater than the 0.013180-second spread.
2. The rwlock makes every reader update the same shared reader counter when it takes and releases the read lock. With readers running on different cores, that counter's cache line has to bounce between cores, so the readers contend over shared lock state even though they are only reading the table.
3. The RCU read side still has a cost. Each lookup pays for an xchg for the seq_cst ANNOUNCE and a fetch add to the reader's statistics counter. With only 1 reader, the overhead is still there as the RCU's median is a little lower. But the results are not separated by the spread.
4. My machine has 8 physical cores and 16 logical threads. With 8 readers plus the writer, there are 9 worker threads for 8 physical cores, so they oversubscribe the physical cores. That helps explain why the RCU throughput scales to 7.22x at 8 readers instead of a full 8x.


## Paper connection

In their paper, McKenney and Slingwine look to address the syncronization overhead issue which was plaguing systems at the time. Their RCU model is exactly what we built in this assignment. We setup all of the functions to properly read, copy, and update. When we run through our testing we can see that it matches one of their claims in Section 5, “Measured Performance,”. They explain that when updates are a small fraction of data-structure accesses, RCU can provide large speedups over locking, and that lock overhead increases under contention. In my 8-reader benchmark, RCU took 0.020066 seconds while the rwlock took 1.144832 seconds for the same workload, making RCU about 57 times faster. This result supports their claim because the readers perform many more lookups than the writer performs updates, while the rwlock becomes much more expensive as more reader threads compete for it.

## Citations

- McKenney, P., Slingwine, J *Read Copy Update: Using Execution History To Solver Concurrency Problems*, 1998
