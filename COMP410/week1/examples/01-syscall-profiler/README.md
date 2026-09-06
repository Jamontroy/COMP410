# COMP 410 — Advanced Operating Systems
## Week 1 — Example: syscall-boundary profiler (starter)

## Build

```bash
make          # build
```

Requires only a C compiler and a POSIX shell. Built with gcc 13.3.0 using
`-O2 -Wall -Wextra -std=c11`; builds warning-free. To use a different compiler,
override CC without editing the Makefile: `make CC=gcc-14`.

## Run

```bash
make test     
make bench    
./nullcall [iterations] [trials]   
```

## Environment
  CPU: AMD Ryzen 9 5900HS
	Bare metal
	OS: Ubuntu 24.04
	Compiler: gcc 13.3.0
	Flags: -O2 -Wall -Wextra -std=c11
	Iterations: 1000000
	Trials: 7
	Warm-up: 1 untimed warmup

## File map

```
src/nullcall.c      the measurement — the three timing/median functions
tests/check.sh      soundness checks (shell only — sh, awk, grep)
bench/results.txt   raw output from make bench, for the report
Makefile            build / test / bench / clean
submission.json     declares what was built and how to run it
README.md           this file
REPORT.md           What / Results / Paper connection / Citations
```

# Notes
