#!/bin/sh
# COMP 410 - Week 3 - submission specification check
#
# THIS FILE IS NOT YOURS TO EDIT. It came with the starter, and it is the same
# check your work is put through after you submit it. Change your own files
# until this passes; changing this file changes nothing that counts.
#
# This checks the SHAPE of your submission, not whether your code is right.
# `make test` will not pass until both are in order, which is deliberate: work
# that does not meet its stated interface is not finished.
#
# Read it if you want to -- it is plain `sh`, and it is the specification in
# the handout written out as commands. Nothing in it is hidden from you.
#
set -u
pass=0; fail=0
ok()  { pass=$((pass+1)); printf '  [ok]   %s
' "$1"; }
bad() { fail=$((fail+1)); printf '  [FAIL] %s
' "$1"; }

echo "== the files the assignment asks for =="
[ -e "src" ] && ok "src is there" || bad "src is missing"
[ -e "tests" ] && ok "tests is there" || bad "tests is missing"
[ -e "bench" ] && ok "bench is there" || bad "bench is missing"
[ -e "Makefile" ] && ok "Makefile is there" || bad "Makefile is missing"
[ -e "README.md" ] && ok "README.md is there" || bad "README.md is missing"
[ -e "REPORT.md" ] && ok "REPORT.md is there" || bad "REPORT.md is missing"
echo
echo "== the headings your report and readme need =="
if [ -f "README.md" ]; then
  grep -qi "^## *Build" "README.md" && ok "README.md has Build" || bad "README.md has no '## Build' heading"
  grep -qi "^## *Run" "README.md" && ok "README.md has Run" || bad "README.md has no '## Run' heading"
  grep -qi "^## *Environment" "README.md" && ok "README.md has Environment" || bad "README.md has no '## Environment' heading"
  grep -qi "^## *File map" "README.md" && ok "README.md has File map" || bad "README.md has no '## File map' heading"
  grep -qi "^## *Notes" "README.md" && ok "README.md has Notes" || bad "README.md has no '## Notes' heading"
else bad "README.md is missing"; fi
if [ -f "REPORT.md" ]; then
  grep -qi "^## *What" "REPORT.md" && ok "REPORT.md has What" || bad "REPORT.md has no '## What' heading"
  grep -qi "^## *Results" "REPORT.md" && ok "REPORT.md has Results" || bad "REPORT.md has no '## Results' heading"
  grep -qi "^## *Paper connection" "REPORT.md" && ok "REPORT.md has Paper connection" || bad "REPORT.md has no '## Paper connection' heading"
  grep -qi "^## *Citations" "REPORT.md" && ok "REPORT.md has Citations" || bad "REPORT.md has no '## Citations' heading"
else bad "REPORT.md is missing"; fi
echo
echo "== submission.json =="
if [ -f "submission.json" ]; then
  grep -q '"label"' "submission.json" && ok "submission.json declares label" || bad "submission.json has no label"
  grep -q '"student"' "submission.json" && ok "submission.json declares student" || bad "submission.json has no student"
  grep -q '"name"' "submission.json" && ok "submission.json declares name" || bad "submission.json has no name"
  grep -q '"what_i_built"' "submission.json" && ok "submission.json declares what_i_built" || bad "submission.json has no what_i_built"
  grep -q '"entrypoint"' "submission.json" && ok "submission.json declares entrypoint" || bad "submission.json has no entrypoint"
  grep -qi "yourlastname\|Your Full Name\|fill this in" "submission.json" && bad "submission.json still has the example text in it" || ok "submission.json has your own values"
else bad "submission.json is missing"; fi
echo
printf '  %s passed, %s failed
' "$pass" "$fail"
[ "$fail" -eq 0 ] || {
  echo
  echo "  Your submission does not yet match the specification in the handout."
  echo "  Each [FAIL] above names exactly what is missing."
  exit 1
}
