# COMP 410 - Week 3 - lock-free queue (STARTER)
CC      ?= cc
CFLAGS  ?= -std=c11 -Wall -Wextra -O2 -pthread -Isrc
# A 16-byte atomic (the tagged pointer) is inlined by clang on macOS but needs
# libatomic on Linux/aarch64, where the compiler emits a call to
# __atomic_compare_exchange_16. Linking it unconditionally is harmless where it
# is not needed.
LDFLAGS ?= -pthread
LDLIBS  ?= $(shell printf 'int main(void){return 0;}' > /tmp/.at.c 2>/dev/null; \
             $(CC) /tmp/.at.c -latomic -o /dev/null 2>/dev/null && echo -latomic)

SRC  = src/msqueue.c src/mutexqueue.c
BINS = build/test_queue build/bench

.PHONY: all test bench tsan clean help submit

all: $(BINS)

build:
	@mkdir -p build

build/test_queue: tests/test_queue.c $(SRC) | build
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

build/bench: src/bench.c $(SRC) | build
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

test: build/test_queue
	@./build/test_queue

# ThreadSanitizer. A lock-free structure that passes the correctness tests can
# still carry a data race that only shows up on another machine or another day;
# tsan finds the race itself rather than waiting for it to bite.
tsan: tests/test_queue.c $(SRC) | build
	$(CC) -std=c11 -Wall -Wextra -g -O1 -fsanitize=thread -pthread -Isrc \
		-o build/test_tsan $^ $(LDFLAGS) $(LDLIBS)
	@# ThreadSanitizer aborts with "unexpected memory mapping" on aarch64
	@# Linux unless ASLR is off. `setarch -R` disables it for one command;
	@# where setarch is absent (macOS) the binary runs directly.
	@if command -v setarch >/dev/null 2>&1; then \
		setarch $$(uname -m) -R ./build/test_tsan; \
	else \
		./build/test_tsan; \
	fi

# Two workloads, because they give opposite answers -- see bench/METHOD.md.
bench: build/bench
	@mkdir -p bench
	@echo "== back-to-back (no other work) =="
	@./build/bench 5 0    | tee bench/results.csv
	@echo
	@echo "== with Michael & Scott's 'other work' between operations =="
	@./build/bench 5 2000 | tee bench/results-otherwork.csv

help:
	@echo "make test   - correctness, including under contention"
	@echo "make tsan   - the same tests under ThreadSanitizer"
	@echo "make bench  - contention study, BOTH workloads -> bench/*.csv"
	@echo "make clean  - remove build output"

clean:
	@rm -rf build

# The in-class ABA exercise. Deliberately buggy; see src/aba_stack.c.
# Exits 1 when the bug shows, which is the POINT of the exercise -- `-` stops
# make reporting that as a build failure.
aba: | build
	$(CC) $(CFLAGS) -o build/aba src/aba_stack.c $(LDFLAGS) $(LDLIBS)
	-@./build/aba

# `make submit` builds the zip you upload. It names the file, lays it out under
# submission/, and keeps your scripts executable. It ALWAYS builds - unfinished
# work is reported as a warning, not a refusal.
submit:
	@sh tests/make-submission.sh
