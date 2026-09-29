#!/bin/sh
# COMP 410 - Week 5 - A5 - build the zip you hand in.
#
# THIS FILE IS NOT YOURS TO EDIT. It came with the starter, and it builds the
# archive exactly the way it has to be submitted.
#
# Run it with:   make submit
#
# It does four things you would otherwise have to get right by hand:
#   1. names the zip            comp410-A5-<netid>.zip
#   2. puts everything under    submission/
#   3. keeps your scripts executable
#   4. leaves out editor and system clutter
#
# IT ALWAYS BUILDS. If your work is unfinished it says so and builds anyway -
# submit what you have. Partial work in the right shape is marked; a missing
# submission is not.
#
# Read it if you want to -- it is plain `sh`. Nothing in it is hidden from you.
set -u

LABEL="A5"
COURSE="comp410"

# ---------------------------------------------------------------- who it is for
# Taken from submission.json so there is ONE place to get it right. For A5,
# "student" IS your netid, not your surname -- it is the same value you gave
# `make params NETID=...`, because that is what the configuration is
# re-derived from when your work is read. If it is missing or still the
# example text, ask - never refuse to build.
NETID=""
if [ -f submission.json ]; then
  NETID=$(sed -n 's/.*"student"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' \
            submission.json | head -1)
fi

case "$NETID" in
  ""|yourlastname|YOURLASTNAME|yournetid|YOURNETID|"Your Full Name"|example|"<yournetid>")
    echo
    echo "  Your netid is not set in submission.json yet, and the zip has to"
    echo "  be named after it so your work can be matched to you, AND so the"
    echo "  same config/params.h you built against is re-derived from it."
    echo
    TYPED=""
    # Only prompt if there is a terminal to prompt at. Piped or scripted runs
    # fall through to the warning rather than hanging or silently skipping.
    if [ -t 0 ]; then
      printf "  Type your netid and press Enter: "
      read -r TYPED 2>/dev/null || TYPED=""
    fi
    # Keep only characters that are safe in a filename.
    NETID=$(printf '%s' "$TYPED" | tr 'A-Z' 'a-z' | tr -cd 'a-z0-9-')

    if [ -z "$NETID" ]; then
      NETID="unnamed"
      echo
      echo "  ****************************************************************"
      echo "  *  NO NETID GIVEN - the zip is named 'unnamed'.               *"
      echo "  *                                                              *"
      echo "  *  Before you upload it, do ONE of these:                      *"
      echo "  *    - rename the file to comp410-A5-<netid>.zip               *"
      echo "  *    - put your netid in submission.json and run               *"
      echo "  *      make submit  again (it takes a second).                 *"
      echo "  *                                                              *"
      echo "  *  A zip called 'unnamed' still contains your work and can     *"
      echo "  *  still be marked, but it has to be matched to you by hand.   *"
      echo "  ****************************************************************"
    else
      echo "  Using '$NETID'."
      echo "  Put the same value in submission.json so it is recorded with"
      echo "  your work; then you will not be asked again."
    fi;;
esac

# ------------------------------------------------------- how far did you get?
# This ALWAYS builds the zip. Partial work submitted in the right shape is far
# better than finished work submitted in the wrong one - and an unfinished
# assignment is still worth marks for the parts that are done.
CHECKS_PASS=1
sh tests/check-submission-spec.sh >/dev/null 2>&1 || CHECKS_PASS=0

if [ "$CHECKS_PASS" -eq 1 ]; then
  echo "  all checks pass"
else
  echo "  NOTE: some checks do not pass yet."
  echo "        Building the zip anyway - submit what you have."
  echo "        Run  make test  to see what is unfinished. If you have time,"
  echo "        fix it and run  make submit  again; the zip is rebuilt each"
  echo "        time. If you are out of time, hand this in as it is and say"
  echo "        in REPORT.md what is unfinished. Partial work scores; a"
  echo "        missing submission does not."
fi

# ------------------------------------------------------------------ build it
ZIP="$COURSE-$LABEL-$NETID.zip"
STAGE=".submit-staging"

rm -rf "$STAGE" "$ZIP"
mkdir -p "$STAGE/submission"

# Everything the handout asks for, and nothing else.
for item in src tests bench config tools Makefile README.md REPORT.md submission.json; do
  if [ -e "$item" ]; then
    cp -R "$item" "$STAGE/submission/"
  else
    echo "  WARNING: $item is missing - the zip will not contain it."
  fi
done

# Keep the executable bit on EVERY shipped script, not just the ones in bin/.
# A zip made on Windows records no Unix permissions at all, and a script that
# arrives non-executable looks broken through no fault of yours.
find "$STAGE/submission" -name '*.sh' -exec chmod +x {} \; 2>/dev/null

# Drop editor, OS, build and cache clutter that otherwise rides along.
find "$STAGE" \( -name '.DS_Store' -o -name '*~' -o -name '._*' \
     -o -name '__pycache__' -o -name '*.pyc' -o -name '.pytest_cache' \
     -o -name 'build' \) \
     -exec rm -rf {} + 2>/dev/null

# Three ways to make the archive, in order of preference. macOS ships `zip`;
# Ubuntu 24.04 (so WSL2 too) does not, but every supported setup has python3.
# Nobody should have to install anything to hand work in.
if command -v zip >/dev/null 2>&1; then
  ( cd "$STAGE" && zip -q -r "../$ZIP" submission ) 2>/dev/null
elif command -v python3 >/dev/null 2>&1; then
  echo "  ('zip' is not installed - using python3 instead, same result)"
  ( cd "$STAGE" && python3 - "$ZIP" <<'PYZIP'
import os, sys, zipfile
out = os.path.join("..", sys.argv[1])
with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
    for root, _dirs, files in os.walk("submission"):
        for name in sorted(files):
            path = os.path.join(root, name)
            info = zipfile.ZipInfo.from_file(path, path)
            # Carry the executable bit across; a script that arrives
            # non-executable looks broken through no fault of yours.
            info.external_attr = (os.stat(path).st_mode & 0xFFFF) << 16
            info.compress_type = zipfile.ZIP_DEFLATED
            with open(path, "rb") as fh:
                z.writestr(info, fh.read())
PYZIP
  ) 2>/dev/null
fi

if [ ! -f "$ZIP" ]; then
  echo
  echo "  Could not build the archive automatically: this machine has neither"
  echo "  'zip' nor 'python3'. That is unusual - check the setup guide."
  echo
  echo "  Option 1 - install zip, then run  make submit  again:"
  echo "    Ubuntu / Debian / WSL:   sudo apt install zip"
  echo "    macOS (Homebrew):        brew install zip"
  echo "    Fedora:                  sudo dnf install zip"
  echo
  echo "  Option 2 - compress it yourself. The folder is ready in"
  echo "    $STAGE/submission"
  echo "  The archive must contain ONE folder called submission/, and the"
  echo "  file must be named $ZIP"
  exit 1
fi

rm -rf "$STAGE"

echo
echo "  Built $ZIP"
echo
echo "  Inside it:"
unzip -l "$ZIP" 2>/dev/null | sed -n '4,24p' | sed 's/^/    /'
echo
echo "  Upload that file to Sakai. Nothing else needs renaming."
