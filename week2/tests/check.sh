#!/bin/sh
# COMP 410 - Week 2 - STARTER checks.
#
# These tell you whether your boundary rules and lifecycle hold. They do not
# check the kernel module -- that has to be loaded in the guest, and only the
# guest can tell you if it oopses.
set -u
pass=0; fail=0
ok()  { pass=$((pass+1)); printf '  [ok]   %s\n' "$1"; }
bad() { fail=$((fail+1)); printf '  [FAIL] %s\n' "$1"; }
want(){ case "$3" in *"$2"*) ok "$1" ;; *) bad "$1" ;; esac }

echo "== copyguard: the four boundary rules =="
CG=$(./copyguard 2>&1)
want "Rule 1 -- NULL destination refused"   "[ok] Rule 1" "$CG"
want "Rule 2 -- over-long read is clamped"  "[ok] Rule 2" "$CG"
want "Rule 3 -- oversized write is bounded" "[ok] Rule 3" "$CG"
want "Rule 4 -- data is terminated"         "[ok] Rule 4" "$CG"
case "$CG" in *"RAN PAST"*) bad "something ran past its buffer" ;;
              *) ok "nothing ran past its buffer" ;; esac

echo
echo "== lifecycle: nothing is left behind =="
LC=$(./lifecycle 2>&1)
want "the happy path unloads clean"   "after unload: live=0" "$LC"
want "every path ends at zero"        "All four paths end at zero" "$LC"
case "$LC" in *LEAK*) bad "a load path leaked" ;; *) ok "no load path leaked" ;; esac

echo
echo "== the module you will build in the guest =="
M=module/procstat.c
grep -q 'rcu_read_lock'  "$M" && ok "task walk takes the RCU lock" || bad "TODO 4: task walk is unlocked"
grep -q 'mutex_lock'     "$M" && ok "shared counter is locked"     || bad "TODO 5: counter is unguarded"
grep -q 'ENOMEM'         "$M" && ok "failed init returns an error" || bad "TODO 6: init cannot fail"
grep -q 'proc_remove'    "$M" && ok "exit removes the /proc entry" || bad "exit leaks the /proc entry"

echo
echo "$pass passed, $fail failed"
[ $fail -eq 0 ] || exit 1
