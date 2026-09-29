#!/bin/sh
# COMP 410 - Week 4 - `make analyze`: your benchmark as ratios and gaps.
#
# This does the bookkeeping and STOPS SHORT OF THE ANSWER ON PURPOSE. It prints
# the numbers the decision rule needs, but it does not apply the rule, name a
# winner, or explain a curve. Those are what A4 asks you to do.
CSV=${1:-bench/results.csv}
if [ ! -f "$CSV" ]; then
    echo "No $CSV yet. Run  make bench  first; it writes that file."
    exit 1
fi

awk -F, '
NR == 1 { for (i = 1; i <= NF; i++) col[$i] = i; next }
{
    impl = $col["impl"]; n = $col["readers"] + 0
    med[impl, n] = $col["median_s"] + 0;  spr[impl, n] = $col["spread_s"] + 0
    thr[impl, n] = $col["throughput_reads_s"] + 0
    torn[impl, n] = $col["torn_reads"] + 0; freed[impl, n] = $col["freed_versions"] + 0
    upd[impl, n] = $col["updates"] + 0;   tot[impl, n] = $col["reads_total"] + 0
    rpr[impl, n] = $col["reads_per_reader"] + 0
    have[impl, n] = 1
    if (!(n in seen)) { seen[n] = 1; lv[++nl] = n }
}
function max(a, b) { if (a > b) return a; return b }
function abs(a)    { if (a < 0) return -a; return a }
END {
    print "Read-side scaling: rcu against rwlock, from " FILENAME
    print ""
    print "How to read this:"
    print "  gap    = |median(rcu) - median(rwlock)|"
    print "  larger = the larger of the two spreads (spread = slowest - fastest trial)"
    print "  The handout states the decision rule that uses these two columns."
    print ""
    printf "%7s  %22s  %22s  %9s  %9s\n", "readers", "rcu median (spread) s", "rwlock median (spread) s", "gap s", "larger"
    for (i = 1; i <= nl; i++) {
        n = lv[i]
        if (!have["rcu", n] || !have["rwlock", n]) continue
        printf "%7d  %11.6f (%8.6f)  %11.6f (%8.6f)  %9.6f  %9.6f\n", n, med["rcu", n], spr["rcu", n], med["rwlock", n], spr["rwlock", n], abs(med["rcu", n] - med["rwlock", n]), max(spr["rcu", n], spr["rwlock", n])
    }

    print ""
    print "Throughput relative to one reader (reads/s at N readers / reads/s at 1):"
    printf "%7s  %10s  %10s\n", "readers", "rcu", "rwlock"
    for (i = 1; i <= nl; i++) {
        n = lv[i]
        printf "%7d  %9.2fx  %9.2fx\n", n, thr["rcu", n] / thr["rcu", lv[1]], thr["rwlock", n] / thr["rwlock", lv[1]]
    }

    print ""
    print "Exact counts (these hold on every machine when the code is correct):"
    bad = 0
    for (k in have) {
        split(k, p, SUBSEP); impl = p[1]; n = p[2]
        if (tot[impl, n] != n * rpr[impl, n]) { printf "  %s %d readers: reads_total is not readers x reads_per_reader\n", impl, n; bad++ }
        if (torn[impl, n] != 0)               { printf "  %s %d readers: %d torn reads\n", impl, n, torn[impl, n]; bad++ }
        if (impl == "rcu" && freed[impl, n] != upd[impl, n]) { printf "  rcu %d readers: freed %d of %d old versions\n", n, freed[impl, n], upd[impl, n]; bad++ }
    }
    if (bad == 0) print "  all hold: reads_total = readers x reads_per_reader, torn_reads = 0, rcu freed_versions = updates"

    print ""
    print "Your job - none of this is printed above:"
    print "  1. Apply the decision rule at every reader count, and say which, if either, is faster."
    print "  2. Explain the rwlock column from the mechanism, not from the numbers."
    print "  3. Explain what the rcu read side still costs, using your one-reader row."
    print "  4. Say what your machine (its core count) does to the largest reader count."
}
' "$CSV"
