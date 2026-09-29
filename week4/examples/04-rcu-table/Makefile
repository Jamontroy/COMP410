# COMP 410 - Week 4 - RCU-style read-mostly table (STARTER)
#
# TOOLCHAIN: a C11 compiler with pthreads (CC=), make, sh and awk. That is all
#   `make`, `make test` and `make bench` need, on macOS or Linux.
#   `make tsan` needs ThreadSanitizer (clang, or gcc with libtsan) and is worth
#   running on Linux as well as your own machine: a data race it reports on one
#   platform may not show on the other.
#   `make orderings` passes ORDERINGS_FLAGS= to the compiler - e.g. a clang
#   --target - and `make model` passes MODEL_ARGS= to the model checker.
CC      ?= cc
CFLAGS  ?= -std=c11 -Wall -Wextra -O2 -pthread -Isrc
LDFLAGS ?= -pthread

LIB  = src/rcu.c src/table.c
DEPS = $(LIB) src/rcu.h src/table.h src/orders.h

.PHONY: all test tsan bench model orderings analyze clean help submit

all: build/test_rcu build/test_mine build/bench build/model

build:
	@mkdir -p build

build/test_rcu: tests/test_rcu.c $(DEPS) | build
	$(CC) $(CFLAGS) -o $@ tests/test_rcu.c $(LIB) $(LDFLAGS)

build/test_mine: tests/test_mine.c $(DEPS) | build
	$(CC) $(CFLAGS) -o $@ tests/test_mine.c $(LIB) $(LDFLAGS)

build/bench: src/bench.c $(DEPS) | build
	$(CC) $(CFLAGS) -o $@ src/bench.c $(LIB) $(LDFLAGS)

build/model: model/handshake.c src/orders.h | build
	$(CC) $(CFLAGS) -o $@ model/handshake.c

# The course tests, your tests, then the shape of the submission. Every part
# runs even when an earlier one fails, so you see everything at once.
test: build/test_rcu build/test_mine
	@st=0; \
	echo "== course tests =="; ./build/test_rcu || st=1; \
	echo; echo "== your tests =="; ./build/test_mine || st=1; \
	echo; echo "== the submission specification =="; sh tests/check-submission-spec.sh || st=1; \
	if command -v python3 >/dev/null 2>&1; then python3 tests/check_submission_ready.py || true; fi; \
	exit $$st

# ThreadSanitizer finds data races. It treats a release or seq_cst store, and an
# acquire or seq_cst load, as synchronization; a relaxed one it does not.
tsan: | build
	$(CC) -std=c11 -Wall -Wextra -g -O1 -fsanitize=thread -pthread -Isrc \
		-o build/test_rcu_tsan tests/test_rcu.c $(LIB) $(LDFLAGS)
	@# ThreadSanitizer aborts with "unexpected memory mapping" on aarch64 Linux
	@# unless ASLR is off; `setarch -R` turns it off for one command.
	@if command -v setarch >/dev/null 2>&1; then \
		setarch $$(uname -m) -R ./build/test_rcu_tsan; \
	else \
		./build/test_rcu_tsan; \
	fi

bench: build/bench
	@mkdir -p bench
	@./build/bench 5 | tee bench/results.csv

model: build/model
	@./build/model $(MODEL_ARGS)

orderings:
	@echo "== the instruction $(CC) $(ORDERINGS_FLAGS) uses for each ordering =="
	@$(CC) -std=c11 -O2 -S $(ORDERINGS_FLAGS) -o - tools/orderings.c | awk '\
		/^_?(store|load)_[a-z]+:/ { sub(/:.*/, ""); sub(/^_/, ""); printf "\n  %-14s", $$0; next } \
		/^[ \t]+[a-z]/ && !/adrp|add[ \t].*lo12|GOT|rbp|ret|endbr|^[ \t]+\./ { $$1 = $$1; printf " %s", $$0 }'
	@echo

analyze:
	@sh src/analyze.sh bench/results.csv

help:
	@echo "make test       - the course tests, your tests, and the submission check"
	@echo "make tsan       - the course tests under ThreadSanitizer"
	@echo "make bench      - rcu vs rwlock, 1/2/4/8 readers -> bench/results.csv"
	@echo "make analyze    - the bench table as gaps and ratios (no verdicts)"
	@echo "make model      - every interleaving of the handshake, three memory models"
	@echo "make orderings  - the instruction your compiler emits for each ordering"
	@echo "make submit     - build the zip you hand in"
	@echo "make clean      - remove build output"

clean:
	@rm -rf build

# `make submit` builds the zip you upload. It names the file, lays it out under
# submission/, and keeps your scripts executable. It ALWAYS builds - unfinished
# work is reported as a warning, not a refusal.
submit:
	@sh tests/make-submission.sh
