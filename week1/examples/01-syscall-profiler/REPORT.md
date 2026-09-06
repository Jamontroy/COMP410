## WHAT
In this test I measured the time it takes for a getppid and noop syscall to pass through the user kernel boundary.
The measurement was conducted by taking the median of the ns results from the getppid and subtracting it from the median of the noop tests
That gives the result of the median time (ns) it takes a syscall to cross the boundary

#### ENVIRONMENT
| Item | Value |
|---|---|
| CPU | AMD Ryzen 9 5900HS |
| Bare metal | Non-VM |
| OS | Ubuntu 24.04 |
| Compiler | gcc 13.3.0 |
| Flags | -O2 -Wall -Wextra -std=c11
| Iterations | 1000000 |
| Trials | 7 |
| Warm-up | 1 untimed warmup |

## RESULTS
- iterations per trial : 1000000
- trials               : 9
- control (noop)     :     1.12 ns/call   (spread 0.04)
- syscall (getppid)  :   293.66 ns/call   (spread 5.54)
- crossing cost      :   292.54 ns   (syscall - control)
- ratio              :    262.5x a user-mode call

## PAPER_CONNECTION / CRITIQUE

When I ran these trial on my own machine I came out with a median getppid time of 293 nanoseconds with a spread of 5.54ns. When we subtract this from the control noop of 1.12ns with a 0.04ns spread. We get a median crossing cost of 292.54. That crushes Ousterhout's fastest measurement being 38x faster (11000 ns / 292.54 ns). The reality is that I would hope my machine would perform faster 37 years later and it is. Breaking down the timing, there is clearly some lost to the 4 costs of save registers, switch privilege+stack, validate, and restore, but there is likely also some mitigation factors on my machine driving up the cost. This number is an isolated metric for crossover time and does not include specifics on portions of the system call process which are taking the most time, e.g. system mitigations. When I took a look back at the slides to compare my numbers with those of the Apple M3 chip, it blew my numbers out of the water with a median crossing time of 71ms. In effect my chip crosses 4 times slower. This slow down really intrigues me as I know I have a decently nice CPU but I am honestly don't have the low level knowledge of where to check to see if I can root out any specific mitigations which are causing the slowdown.

It is interesting to see how Ousterhout's numbers from 1989 compare to the numbers which I am pulling off of my own computer and modern day CPU. Obviously, we ran the same test which he performed labeled as #4 Kernel Entry-Exit in his paper. He timed the getpid kernel call to find the cost of entering and leaving the operating system. His results find that the DS5000 Ultra 3.1D had the fastest entry exit time with 11 microseconds. A critique I have of his paper is the use of average versus median in his trials, as discussed in the slides, using the average can throw off measurements considerably versus using the median as we did in our testing. Similarly, a threat to the credibility of the paper is that these are isolated tests on a small number of systems. To make the claim the Operating Systems in general is not keeping up with hardware advancements based on a small number of tests may be an over claim based on the tests and data.

When we take a step back and look at Ousterhout's claims, they still hold true today. His thesis was that the Operating Systems were not improving as fast as the subsequent hardware in the trials on a syscall level. The modern day CPU is leaps and bounds more powerful than those of 1989. Earlier I mentioned the DS5000 Ultra which he references in the paper as having the lowest crossover time. I did some research into those DECstation computers from the time and they ran on MIPS R4000 series CPU's. Their specs come in at around 50-250 MHz with a transistor count of 1.5 million. When we compare this to the specs of my AMD Ryzen 9 5900HS: 3.0 - 4.6 GHz with ~10.7 billion transistors. Comparing the metrics, my CPU has 7133x the transistors of the R4000 and ~22x the median clock speed. If crossover speeds were grew proportionally with the advancements in CPU, we would expect to see a jump much larger than only the 38x which my tests results. In my opinion, these metrics uphold Ousterhout's conclusions from 1990 as crossover speed and CPU advancements do not improve proportionally.

## CITATIONS
- Ousterhout 1990, *Why Aren't Operating Systems Getting Faster As Fast as Hardware?*
- Wikipedia, *R4000* https://en.wikipedia.org/wiki/R4000#
- TechPowerUp, *AMD Ryzen 9 5900HS Specs* https://www.techpowerup.com/cpu-specs/ryzen-9-5900hs.c3518 
