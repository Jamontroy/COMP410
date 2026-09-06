#!/usr/bin/env bash
# Week 1 starter checks — POSIX shell only, no extra tools needed.
#
# These check that your measurement is SOUND, not that it is fast. Absolute
# figures differ on every machine and are never checked.

set -uo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$HERE" || exit 1

pass=0; fail=0
ok()  { printf "  PASS %s\n" "$1"; pass=$((pass+1)); }
bad() { printf "  FAIL %s\n" "$1"; fail=$((fail+1)); }

echo "Week 1 starter checks"
echo

make -s all >/dev/null 2>&1 && ok "builds clean" || { bad "build failed"; exit 1; }

OUT="$(./nullcall 200000 3 2>&1)"; RC=$?

if [ $RC -eq 3 ]; then
    echo
    echo "  Your control loop measured 0 ns/call."
    echo "  Either the TODOs are not finished yet, or the compiler removed the"
    echo "  loop. Both are expected at the start — finish TODO 1-3 and re-run."
    exit 1
fi

[ $RC -eq 0 ] && ok "runs and exits 0" || bad "exited $RC"

CTL="$(echo "$OUT" | awk '/control \(noop\)/ {print $4}')"
SYS="$(echo "$OUT" | awk '/syscall \(getppid\)/ {print $4}')"

if [ -n "$CTL" ] && awk "BEGIN{exit !($CTL > 0)}"; then
    ok "control loop is real, not optimized away (${CTL} ns/call)"
else
    bad "control measured ${CTL:-?} — the compiler deleted your loop (TODO 2)"
fi

if [ -n "$SYS" ] && awk "BEGIN{exit !($SYS > 0)}"; then
    ok "syscall loop produced a figure (${SYS} ns/call)"
else
    bad "syscall loop measured ${SYS:-?} (TODO 1)"
fi

if [ -n "$SYS" ] && [ -n "$CTL" ] && awk "BEGIN{exit !($SYS > $CTL)}"; then
    ok "the syscall costs more than the control — as it must"
else
    bad "syscall (${SYS:-?}) is not dearer than the control (${CTL:-?})"
fi

# The median must actually sort: with 3 trials a working median is never the
# smallest value unless all three are equal.
echo "$OUT" | grep -q "spread" && ok "reports a spread" || bad "no spread (TODO 3)"

./nullcall 0 3 >/dev/null 2>&1 && bad "accepted 0 iterations" || ok "rejects bad arguments"

echo
echo "  $pass passed, $fail failed"
[ "$fail" -eq 0 ] || exit 1
