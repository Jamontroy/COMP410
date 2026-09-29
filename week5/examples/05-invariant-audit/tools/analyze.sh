#!/bin/sh
# COMP 410 - Week 5 - A5 - `make analyze` (PROVIDED: the evidence, not the reading).
#
# Turns bench/audit.csv into per-column statistics across your runs: the
# MEDIAN, the smallest and largest run, and the SPREAD (largest minus
# smallest). Every figure your REPORT gives is a median with its spread, and
# `## Answers` asks for them by name, so they can be read straight off.
#
# It states NO verdict about which pathology you have: that is what the
# assignment asks you to work out, in REPORT.md, from this data.
CSV=${1:-bench/audit.csv}
if [ ! -f "$CSV" ]; then
    echo "No $CSV yet. Run  make bench  first; it writes that file."
    exit 1
fi

awk -F, '
NR == 1 { for (i = 1; i <= NF; i++) col[$i] = i; next }
{
    n++
    if ($col["completed"] + 0 != $col["queued"] + 0) lost++
    s1 = $col["shard1_median_latency_us"] + 0
    v["median_latency_us", n]        = $col["median_latency_us"] + 0
    v["max_latency_us", n]           = $col["max_latency_us"] + 0
    v["bug_fraction", n]             = $col["bug_fraction"] + 0
    v["shard0_median_latency_us", n] = $col["shard0_median_latency_us"] + 0
    v["shard1_median_latency_us", n] = s1
    v["shard_ratio", n]              = (s1 > 0) ? ($col["shard0_median_latency_us"] + 0) / s1 : 0
    v["window_us", n]                = $col["window_us"] + 0
    v["violation_fraction", n]       = $col["violation_fraction"] + 0
    v["episodes", n]                 = $col["episodes"] + 0
    v["pace_late_us", n]             = $col["pace_late_us"] + 0
    if ($col["violation_fraction"] + 0 < 0) not_attempted++
    workers = $col["workers"]
    if (n > 1 && $col["seed"] != seed) seeds++
    seed = $col["seed"]
}
# median / min / max of v[name, 1..n]; a median cannot be accumulated like a
# sum, so each value is kept and sorted.
function stats(name,   i, j, t, s) {
    for (i = 1; i <= n; i++) s[i] = v[name, i]
    for (i = 2; i <= n; i++) { t = s[i]; j = i - 1
        while (j >= 1 && s[j] > t) { s[j + 1] = s[j]; j-- }
        s[j + 1] = t }
    MED = (n % 2) ? s[(n + 1) / 2] : (s[n / 2] + s[n / 2 + 1]) / 2
    MIN = s[1]; MAX = s[n]
}
function row(label, name, fmt) {
    stats(name)
    printf "%-26s " fmt " " fmt " " fmt " " fmt "\n", label, MED, MIN, MAX, MAX - MIN
}
END {
    if (n == 0) { print "No runs in " FILENAME; exit 1 }
    print "A5 audit: " n " run(s) from " FILENAME ", seed " seed ", pool of " workers " workers"
    if (seeds + 0 > 0) print "  WARNING: these rows come from more than one seed -- run make rebench"
    if (lost + 0 > 0) printf "  %d of %d runs LOST WORK (completed != queued): a workload.c defect, not yours\n", lost, n
    print ""
    printf "%-26s %12s %12s %12s %12s\n", "", "MEDIAN", "min", "max", "SPREAD"
    row("median_latency_us",        "median_latency_us",        "%12.3f")
    row("max_latency_us",           "max_latency_us",           "%12.3f")
    row("bug_fraction",             "bug_fraction",             "%12.6f")
    row("shard0_median_latency_us", "shard0_median_latency_us", "%12.3f")
    row("shard1_median_latency_us", "shard1_median_latency_us", "%12.3f")
    row("shard_ratio (s0 / s1)",    "shard_ratio",              "%12.3f")
    print ""
    if (not_attempted + 0 > 0) {
        print "The invariant (handout section 3): not attempted yet -- invariant_scan is still the stub."
    } else {
        print "The invariant (handout section 3), with YOUR window:"
        row("window_us",            "window_us",                "%12.3f")
        row("violation_fraction",   "violation_fraction",       "%12.6f")
        row("episodes",             "episodes",                 "%12.1f")
    }
    print ""
    stats("pace_late_us")
    printf "Dispatcher lateness: median %.0f us behind its schedule", MED
    if (MED > 200) print " -- the load is LOWER than designed on this machine; say so in ## Limits."
    else print " (the load asked for is the load delivered)."
    print ""
    print "Every figure above is a median across runs, with its spread. No verdict:"
    print "which pathology this shape matches, and why, is your REPORT."
}
' "$CSV"
