# COMP 410 — Week 2 — /proc inspector (STARTER)
#
#
#
# Targets:
#   make            build the two user-space models
#   make test       soundness checks (run-checks.sh)
#   make bench      the boundary and lifecycle output the slides quote
#   make analyze    the measurement the walkthrough is written from
#   make module     build the real .ko  (Linux + kernel headers ONLY)
#   make clean      remove binaries
#
# The two models build anywhere with a C compiler. The MODULE does not: it
# needs Linux kernel headers, so `make module` is expected to fail on macOS.
# That split is deliberate -- see README.md.

CC      ?= cc
CFLAGS  ?= -O2 -Wall -Wextra -std=c11

PROGS   := copyguard lifecycle

.PHONY: all test bench analyze module clean

all: $(PROGS)

copyguard: src/copyguard.c
	$(CC) $(CFLAGS) -o $@ $<

lifecycle: src/lifecycle.c
	$(CC) $(CFLAGS) -o $@ $<

test: all
	@sh ./tests/check.sh

proc-test:
	@sh ./tests/proc-check.sh

bench: all
	@echo "=== the user-copy boundary ==="
	@./copyguard
	@echo
	@echo "=== module lifecycle ==="
	@./lifecycle


module:
	@$(MAKE) -C module

clean:
	@rm -f $(PROGS)
	@echo "cleaned"
