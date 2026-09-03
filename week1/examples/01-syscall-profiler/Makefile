# COMP 410 — Week 1 — syscall-boundary profiler (STARTER)
#
# Targets:
#   make            build
#   make test       check your measurement is sound (not that it is fast)
#   make bench      run the measurement and print the numbers for your report
#   make clean      remove the binary
#
# TOOLCHAIN — a C compiler and a POSIX shell. Nothing else.
#   `make test` runs tests/check.sh, which uses only sh/awk/grep.
#   Different compiler? Override it, no need to edit this file:
#     make CC=gcc-14
#   See notes/systems-setup-guide.pdf if the build does not run.

CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra -std=c11

.PHONY: all test bench clean

all: nullcall

nullcall: src/nullcall.c
	$(CC) $(CFLAGS) -o $@ $<

test: all
	@./tests/check.sh

bench: all
	@echo "=== null syscall vs user-mode call ==="
	@./nullcall 10000000 9

clean:
	@rm -f nullcall
	@echo "cleaned"
