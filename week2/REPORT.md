## What
In this assignment, my module reports the state of each process in which the kernel is tracking. It sorts the state into three buckets which were either running, sleeping, or zombie. It also includes a total for all of the processes. It tests for zombie by checking exit_state & EXIT_ZOMBIE, then running with if task_is_running(t), then else sleeping. To expose this, the user begins by loading the module then running cat. The cat returns 5 lines of output being tasks total, running, sleeping, zombies, and the reads. This is exposed through proc_create as it registers /proc/comp410_procstat. procstat_ops reads the function taking each cat which is ran and returns the answer. For the implemetation of my code, I needed to add the 4 rules to ensure there we no information leaks. To ensure rule 2 in copyguard.c I took the examples which we performed in class by limiting the space claimed by the buffer to not exceed the space needed. For rule 3, I wanted to ensure that the program never wrote past the buffer size and I used min() and KBUFF_SIZE to prevent that. In interesting thing about writing this code min() isn't available natively since it is a kernel macro not a libc so I had to define it using #define min(a,b) ((a) < (b) ? (a) : (b)). For Rule 4, I had to ensure that I terminated any left over or unfinished data, so I used kbuf[n] = '\0' with n representing the end of the buffer with n = min(ulen, KBUF_SZ - 1). That ensured the 4 rules were followed. In the Procstat.c I had to add the read_locks in the right place to prevent the process from being accidentally exited as a part of another task in the tasklist. This is followed by the stat_locks through MUTEX which allow for the smooth reading of how many times the module has been read without any hiccups. Finally to test the failures during the load. A failed init is not going to exit the program, but it could end up leaving bytes in memory. I need to make sure my code releases those bytes even if the init fails. What I did was wait until proc_create succeeded then forced a branch adding a block called err_proc into memory after proc_create succeeded. Then I tested after to see if the block was still there and it was not. This means that the code is cleaning out the memory after it runs to ensure nothing is left behind. 

## Results
proc read when you cat /proc/comp410_procstat when the module is loaded
```bash
tasks_total    188
tasks_running  2
tasks_sleeping 186
tasks_zombie   0
reads          1
```

test output from make test
```bash
== copyguard: the four boundary rules ==
  [ok]   Rule 1 -- NULL destination refused
  [ok]   Rule 2 -- over-long read is clamped
  [ok]   Rule 3 -- oversized write is bounded
  [ok]   Rule 4 -- data is terminated
  [ok]   nothing ran past its buffer

== lifecycle: nothing is left behind ==
  [ok]   the happy path unloads clean
  [ok]   every path ends at zero
  [ok]   no load path leaked

== the module you will build in the guest ==
  [ok]   task walk takes the RCU lock
  [ok]   shared counter is locked
  [ok]   failed init returns an error
  [ok]   exit removes the /proc entry

12 passed, 0 failed
```
The results of proc-check.sh which I created for the user-space test. It must be run while the module is inserted.
```bash
proc-check: reads incremented 1 -> 2
proc-check: valid output (189 tasks, read 1)
```


The successful load and unload of the module
```bash
[ 2526.139542] comp410: /proc/comp410_procstat created
[ 2526.216913] comp410: /proc/comp410_procstat removed
```

This is the output of the failing load from dmesg-failing-init
```bash
[ 2233.067256] comp410: init failed, /proc entry removed
```

## Paper Connection

I ran all of my tests within a virtualBox VM which I configured to run Debian. Debian is linux-based and has a monolithic kernel. The paper by Jochen Liedtke discusses the benefits of using a microkernel instead of a monolithic kernel. The microkernel is built with the core kernel having the absolute bare minimum it needs to run with everything else being built out as modules. This allows for multiple contained small crosses over the user boundary instead of a single large one with a monolithic kernel. At the time of the paper, microkernels were ctiticized for these numberous small crosses as they were seen to take up too many resources. Liedtke was able to show that much of the resource contraint came from poor hardware configuration instead of it being the microkernels issue. When it comes to the differences between the kernel I like to think of it almost as the modern day issues with a bloatware operating system like windows vs something more barebones like debian. Windows comes preinstalled with all of these tools you may or may not use, but debian comes very plain. 

With this in mind, I can relate it to the module work I have done this week with the kernel. My module is built to do one simple thing, check the state of each process in the kernel. Although added to my monolithic kernel, my module would be a lightweight user-side server within a microkernel implementation. It is coded to follow the four rules to ensure a lightweight and smooth kernel load and exit without taking too many resources. It is interesting today that seemingly the monolithic kernel has won the battle when it comes to unix-based distributions. I think this is simply that the average consumer or linux user doesn't want to think too hard about loading a handful of modules into the kernel or doesn't care about its possibiity for hyperoptomization. It sheds an interesting light in retrospect on Liedtke's paper. He can go through and breakdown every aspect of why monolithic kernels are worse, and make true optomizations to improve the microkernel, but most people will still choose the monolithic because it is simpler and the tradeoffs are minimal.

## Citations
- Liedtke *On micro-Kernel Construction* SOSP 1995
