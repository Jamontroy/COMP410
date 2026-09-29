# COMP 410 - Week 5 - A5: the invariant audit (INDIVIDUAL)
#
# TOOLCHAIN: a C11 compiler with pthreads (CC=), make, sh, awk and python3
#   (stdlib only). Nothing else is needed for `make`, `make test`,
#   `make bench`, `make analyze` or `make windows`, on macOS or Linux.
#   `make tsan` needs ThreadSanitizer (clang, or gcc with libtsan); 
CC      ?= cc
CFLAGS  ?= -std=c11 -Wall -Wextra -O2 -pthread -Isrc
LDFLAGS ?= -pthread -lm

LIB  = src/workload.c src/analyze.c
DEPS = $(LIB) src/workload.h src/analyze.h config/params.h

.PHONY: all test tsan bench rebench analyze windows params clean help submit

all: build/audit build/test_analyze build/test_mine build/windows

build:
	@mkdir -p build

# Auto-generated ONCE, for a placeholder netid, so a fresh checkout still
# builds. `make params NETID=yournetid` replaces it with your own -- do that
# before you measure anything.
config/params.h:
	@echo "No config/params.h yet -- writing one for the placeholder netid 'example-student'."
	@echo "Run 'make params NETID=yournetid' with YOUR OWN netid before you measure anything."
	@python3 tools/gen-params.py --netid example-student

params:
	@if [ -z "$(NETID)" ]; then echo "usage: make params NETID=yournetid"; exit 1; fi
	python3 tools/gen-params.py --netid "$(NETID)"

build/audit: src/audit.c $(DEPS) | build
	$(CC) $(CFLAGS) -o $@ src/audit.c $(LIB) $(LDFLAGS)

build/windows: tools/windows.c $(DEPS) | build
	$(CC) $(CFLAGS) -o $@ tools/windows.c $(LIB) $(LDFLAGS)

build/test_analyze: tests/test_analyze.c $(DEPS) | build
	$(CC) $(CFLAGS) -o $@ tests/test_analyze.c $(LIB) $(LDFLAGS)

build/test_mine: tests/test_mine.c $(DEPS) | build
	$(CC) $(CFLAGS) -o $@ tests/test_mine.c $(LIB) $(LDFLAGS)

# The course tests, your tests, the shape of the submission, and the columns
# of bench/audit.csv. Every part runs even when an earlier one fails, so you
# see everything at once; any failure fails `make test`. The last line is
# advice only and never fails the build.
test: build/test_analyze build/test_mine
	@st=0; \
	./build/test_analyze || st=1; \
	echo; ./build/test_mine || st=1; \
	echo; echo "== the submission specification =="; \
	sh tests/check-submission-spec.sh || st=1; \
	echo; echo "== the columns of bench/audit.csv =="; \
	python3 tests/test_schema.py || st=1; \
	python3 tests/check_submission_ready.py || true; \
	exit $$st

# ThreadSanitizer over the THREADED code: 20 runs of the real workload.
# (The course tests call no threads, so running them under TSan would say
# nothing.) ThreadSanitizer aborts with "unexpected memory mapping" on
# aarch64 Linux unless ASLR is off; `setarch -R` turns it off for one command.
tsan: $(DEPS) | build
	$(CC) -std=c11 -Wall -Wextra -g -O1 -fsanitize=thread -pthread -Isrc \
		-o build/audit_tsan src/audit.c $(LIB) $(LDFLAGS)
	@if command -v setarch >/dev/null 2>&1; then \
		setarch $$(uname -m) -R ./build/audit_tsan 20 > /dev/null; \
	else \
		./build/audit_tsan 20 > /dev/null; \
	fi && echo "tsan: 20 runs, no data race reported"

# bench/audit.csv is your evidence: the numbers in your REPORT are checked
# against it. `make bench` therefore never replaces it, so the evidence you
# submit is the evidence you measured. Use `make rebench` to replace it on
# purpose (after changing analyze.c, for one).
bench: build/audit
	@mkdir -p bench
	@if [ -f bench/audit.csv ]; then \
		echo "bench/audit.csv already exists -- kept. 'make rebench' replaces it."; \
		csv_seed=$$(awk -F, 'NR == 2 { print $$2 }' bench/audit.csv); \
		my_seed=$$(sed -n 's/^#define WL_SEED *\([0-9]*\).*/\1/p' config/params.h); \
		if [ "$$csv_seed" != "$$my_seed" ]; then \
			echo "WARNING: it was measured with seed $$csv_seed, not your $$my_seed -- run 'make rebench'."; \
		fi; \
	else \
		./build/audit > bench/audit.csv.tmp && mv bench/audit.csv.tmp bench/audit.csv; \
	fi

rebench: build/audit
	@mkdir -p bench
	@./build/audit > bench/audit.csv.tmp && mv bench/audit.csv.tmp bench/audit.csv

analyze:
	@sh tools/analyze.sh bench/audit.csv

windows: build/windows
	@./build/windows

help:
	@echo "make params NETID=..  - derive YOUR config/params.h (do this first)"
	@echo "make test             - the course tests, your tests, the submission check"
	@echo "make bench            - 25 runs of your workload -> bench/audit.csv (about 30 s),"
	@echo "                        only if it does not exist yet"
	@echo "make rebench          - the same, replacing bench/audit.csv"
	@echo "make analyze          - the CSV as medians with spreads; no verdict"
	@echo "make windows          - episodes under a range of windows (handout section 3.2)"
	@echo "make tsan             - the threaded workload under ThreadSanitizer"
	@echo "make submit           - build the zip you hand in"
	@echo "make clean            - remove build output"

clean:
	@rm -rf build

submit:
	@sh tests/make-submission.sh
