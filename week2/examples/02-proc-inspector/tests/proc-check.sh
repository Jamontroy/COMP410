#!/bin/sh
# Read and validate the live /proc interface from user space.
set -u

proc=${1:-/proc/comp410_procstat}

if [ ! -r "$proc" ]; then
    printf 'proc-check: cannot read %s (is the module loaded?)\n' "$proc" >&2
    exit 1
fi

first=$(cat "$proc")
second=$(cat "$proc")

r1=$(printf '%s\n' "$first"  | awk '$1=="reads"{print $2}')
r2=$(printf '%s\n' "$second" | awk '$1=="reads"{print $2}')

if [ "$r2" -eq $((r1 + 1)) ]; then
    printf 'proc-check: reads incremented %d -> %d\n' "$r1" "$r2"
else
    printf 'proc-check: reads did not increment (%d -> %d)\n' "$r1" "$r2" >&2
    exit 1
fi

printf '%s\n' "$first" | awk '
BEGIN {
    expected[1] = "tasks_total"
    expected[2] = "tasks_running"
    expected[3] = "tasks_sleeping"
    expected[4] = "tasks_zombie"
    expected[5] = "reads"
    valid = 1
}
{
    if (NF != 2 || $1 != expected[NR] || $2 !~ /^[0-9]+$/) {
        valid = 0
    }
    value[NR] = $2
}
END {
    if (NR != 5 || !valid || value[1] != value[2] + value[3] + value[4] || value[5] < 1) {
        print "proc-check: malformed /proc output" > "/dev/stderr"
        exit 1
    }
    printf "proc-check: valid output (%d tasks, read %d)\n", value[1], value[5]
}
'