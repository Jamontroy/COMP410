## WHAT
	In this test I measured the time it takes for a getppid and noop syscall to pass through the user kernel boundary.
	The measurement was conducted by taking the median of the ns results from the getppid and subtracting it from the median of the noop tests
	That gives the result of the median time (ns) it takes a syscall to cross the boundary

	Environment
		CPU: AMD Ryzen 9 5900HS
		Bare metal
		OS: Ubuntu 24.04
		Compiler: gcc 13.3.0
		Flags: -O2 -Wall -Wextra -std=c11
		Iterations: 1000000
		Trials: 7
		Warm-up: 1 untimed warmup

## RESULTS
- iterations per trial : 1000000
- trials               : 9
- control (noop)     :     1.12 ns/call   (spread 0.04)
- syscall (getppid)  :   293.66 ns/call   (spread 5.54)
- crossing cost      :   292.54 ns   (syscall - control)
- ratio              :    262.5x a user-mode call

## PAPER_CONNECTION / CRITIQUE

It is interesting to see how Ousterhout's numbers from 1989 compare to the numbers which I am pulling off of my own computer and modern day CPU. Obviously, we ran the same test which he performed labeled as #4 Kernel Entry-Exit in his paper where he timed the getpid kernel call to find the cost of entering and leaving the operating system. His results find that the DS5000 Ultra 3.1D had the fastest entry exit time with 11 microseconds. A critique I have of his paper is the use of average versus median in his trials, as discussed in the slides using the average can throw off measurements considerably versus using the median as we did in our testing. His thesis was that the Operating Systems were not improving as fast as the subsequent hardware in the trials on a syscall level. When we compare this to the tests that I just ran, mine yeilded differing results. 
When I ran these trial on my own machine I came out with a median getppid time of 293 nanoseconds with a spread of 5.54ns. When we subtract this from the control noop of 1.12ns with a 0.04ns spread. We get a median crossing cost of 292.54. That crushes his fastest measurement being 38x faster (11000 ns / 292.54 ns). The reality is that I would hope my machine would perform faster 37 years later and it is. When put into the perspective though with the numbers from the slides from last week measuring a Apple M3 chip clocking in at 71ns, it brings up an interesting gap as to why mine is 4x slower. This slow down intrigued me but I am honestly not able to say specifically that I know what causes the slowdown other than speculating that it is simply an effect of various mitigations within my architecture.
When we take a step back and look at Ousterhout's claims, they still hold true today. The modern day CPU is leaps and bounds more powerful than those of 1989. When I compare my throughput numbers vs Ousterhout's it is likely that if CPU advancements and Kernel entry exit times had improved at the same rate as CPUs, then the numbers I would have received would have been far beyond only 38x. Showing that even today as CPU's continue to advance slowdowns such as mitigations and how the OS handles syscalls will be the ultimate factor when it comes to kernel entry exit timing.

I'm going to be honest I know this isn't great, but I am trying to catch up from joining the class late.

## CITATIONS
- Ousterhout 1990 
