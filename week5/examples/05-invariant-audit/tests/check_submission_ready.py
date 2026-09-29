#!/usr/bin/env python3
"""
Pre-submission self-check. Run by `make test`; never blocks your build.

It looks for mechanical problems that would stop your work being read properly:
a renamed output column, a missing report heading, an unfilled manifest, a
build artifact in the archive.

Anything it prints is something you can fix in a minute. Anything it prints
that you do NOT fix travels with your submission in
bench/SUBMISSION-WARNINGS.txt, and may be referred to when your work is marked.

Part of the assignment's tests: do not edit it.
"""
import csv
import glob
import json
import os
import re
import sys

REQUIRED_COLUMNS = ['run', 'seed', 'workers', 'tasks', 'queued', 'completed', 'started', 'shard0_count', 'shard1_count', 'median_latency_us', 'spread_latency_us', 'max_latency_us', 'bug_count', 'bug_fraction', 'shard0_median_latency_us', 'shard1_median_latency_us', 'window_us', 'violation_us', 'episodes', 'span_us', 'violation_fraction', 'pace_late_us']
CSV_PATTERN = 'bench/*.csv'
ANSWER_KEYS = ['derived_seed', 'runs_completed', 'bug_fraction_median', 'bug_fraction_spread', 'max_latency_median_us', 'shard_ratio_median', 'identified_pathology', 'predicted_pathology', 'prediction_assessment', 'window_us_median', 'violation_fraction_median', 'violation_fraction_spread', 'episodes_median', 'invariant_violated']
REPORT_HEADINGS = ['Prediction', 'Answers', 'What', 'Results', 'Paper connection', 'Limits', 'Citations']
MANIFEST = 'submission.json'
MANIFEST_KEYS = ['label', 'student', 'name', 'what_i_built', 'entrypoint']
ARTIFACT_NAMES = ("a.out", "audit")
ARTIFACT_SUFFIXES = (".o", ".so", ".ko", ".pyc")

#: Written next to REPORT.md when there is no bench/ directory, so it still
#: travels with your submission.
WARNINGS_FILE = (os.path.join("bench", "SUBMISSION-WARNINGS.txt")
                 if os.path.isdir("bench") else "SUBMISSION-WARNINGS.txt")


def check_answers_block():
    """The `## Answers` block: every key present and filled in."""
    out = []
    if not ANSWER_KEYS or not os.path.isfile("REPORT.md"):
        return out
    text = open("REPORT.md", errors="replace").read()
    missing = [k for k in ANSWER_KEYS if k not in text]
    if missing:
        out.append(
            "REPORT.md has no line for {}. The `## Answers` block ships in the "
            "starter with the labels already there - fill in the right-hand "
            "side, one per line.".format(", ".join("`" + m + ":`" for m in missing)))
        return out
    blank = []
    for key in ANSWER_KEYS:
        for line in text.splitlines():
            if line.strip().startswith(key + ":"):
                rest = line.split(":", 1)[1]
                value = rest.split("command", 1)[0].strip()
                if not value or value.startswith("<"):
                    blank.append(key)
                break
    if blank:
        out.append(
            "These answers are still blank or hold the placeholder: {}."
            .format(", ".join(blank)))
    return out


def check_csv_schema():
    out = []
    if not REQUIRED_COLUMNS:
        return out          # this assignment produces no CSV study
    files = sorted(glob.glob(CSV_PATTERN))
    if not files:
        out.append(
            "No audit CSV found at {}. Run `make bench` so your results "
            "are written to a file - a report without its data cannot be "
            "checked.".format(CSV_PATTERN))
        return out
    matched = False
    for path in files:
        with open(path, newline="") as fh:
            header = next(csv.reader(fh), [])
        present = {h.strip().lower() for h in header if h}
        if not present:
            out.append("{} has no header row.".format(os.path.basename(path)))
            continue
        if not [c for c in REQUIRED_COLUMNS if c.lower() in present]:
            continue
        matched = True
        missing = [c for c in REQUIRED_COLUMNS if c.lower() not in present]
        if missing:
            out.append(
                "{} is missing the column(s) {}. Keep the starter's column "
                "names - the analysis reads them by name. Extra columns are "
                "fine.".format(os.path.basename(path), ", ".join(missing)))
    if files and not matched:
        out.append(
            "None of your CSVs use the expected columns ({}). If you rewrote "
            "the output format, your numbers cannot be read back and checked."
            .format(", ".join(REQUIRED_COLUMNS)))
    return out


def check_report():
    out = []
    if not os.path.isfile("REPORT.md"):
        out.append("REPORT.md is missing.")
        return out
    text = open("REPORT.md", errors="replace").read()
    lowered = text.lower()
    for h in REPORT_HEADINGS:
        if ("## " + h).lower() not in lowered:
            out.append(
                "REPORT.md has no `## {}` heading. These headings are "
                "checked by name.".format(h))
    # template prompt lines are reported by check_finished, below
    return out


def check_finished():
    """Finished work counts in the code-quality row (see the handout's *How each
    row is scored*): template prompt lines, TODO/DONE markers left in your own
    source, and CRLF line endings are reported here before you submit."""
    out = []
    for doc in ("README.md", "REPORT.md"):
        if not os.path.isfile(doc):
            continue
        lines = open(doc, errors="replace").read().splitlines()
        left = [i for i, ln in enumerate(lines, 1)
                if re.match(r"^\s*\*[^*\s][^*]*\*\s*$", ln)
                or re.match(r"^\s*_[^_\s][^_]*_\s*$", ln)]
        if left:
            out.append("{} still has template prompt line(s) at line {} - replace "
                       "each with your own words.".format(doc, ", ".join(map(str, left[:5]))))
    marked, crlf = [], []
    for top in ("src", "bin"):
        for root, _dirs, files in os.walk(top):
            for name in files:
                if not name.endswith((".c", ".h", ".py", ".sh")):
                    continue
                path = os.path.join(root, name)
                raw = open(path, "rb").read()
                if b"\r\n" in raw:
                    crlf.append(path)
                text = raw.decode("utf-8", "replace")
                for i, ln in enumerate(text.splitlines(), 1):
                    if re.search(r"\bTODO\b|\bDONE\b", ln):
                        marked.append("{}:{}".format(path, i))
    if marked:
        out.append("TODO/DONE instruction markers are still in your source ({}) - "
                   "remove them once the work is done.".format(", ".join(marked[:4])))
    if crlf:
        out.append("{} use Windows (CRLF) line endings - save them with LF."
                   .format(", ".join(crlf[:4])))
    return out


def check_manifest():
    out = []
    if not MANIFEST:
        return out
    if not os.path.isfile(MANIFEST):
        out.append("{} is missing.".format(MANIFEST))
        return out
    try:
        data = json.load(open(MANIFEST))
    except ValueError as exc:
        out.append("{} is not valid JSON ({}).".format(MANIFEST, exc))
        return out
    for key in MANIFEST_KEYS:
        value = str(data.get(key, "")).strip()
        if not value:
            out.append("{}: `{}` is empty.".format(MANIFEST, key))
        elif value.lower() in ("yourlastname", "yourgroupname", "yournetid",
                               "your firstname lastname",
                               "your full name", "one sentence, in your own words",
                               "the exact command that runs your work"):
            out.append("{}: `{}` still holds the example value {!r}."
                       .format(MANIFEST, key, value))
    return out


def check_artifacts():
    out = []
    found = []
    for root, dirs, files in os.walk("."):
        # build/ is where `make` puts its output, and `make submit` leaves it
        # out of the zip, so it is not flagged.
        dirs[:] = [d for d in dirs if d not in (".git", ".gradevenv", "__pycache__", "build")]
        for name in files:
            if name in ARTIFACT_NAMES or name.endswith(ARTIFACT_SUFFIXES):
                found.append(os.path.join(root, name).lstrip("./"))
    if found:
        out.append(
            "Build artifacts in your folder: {}. The handout asks for sources "
            "only; run `make clean` before you package."
            .format(", ".join(sorted(found)[:4])))
    return out


def main():
    warnings = []
    for fn in (check_csv_schema, check_answers_block, check_report,
               check_finished, check_manifest, check_artifacts):
        try:
            warnings.extend(fn())
        except Exception as exc:  # a self-check must never break the build
            warnings.append("({} could not run: {})".format(fn.__name__, exc))

    try:
        if os.path.dirname(WARNINGS_FILE):
            os.makedirs(os.path.dirname(WARNINGS_FILE), exist_ok=True)
        if warnings:
            with open(WARNINGS_FILE, "w") as fh:
                fh.write("Pre-submission warnings, shown when `make test` ran.\n")
                fh.write("These were displayed before submission and not resolved.\n\n")
                for w in warnings:
                    fh.write("- " + w + "\n")
        elif os.path.exists(WARNINGS_FILE):
            os.remove(WARNINGS_FILE)
    except OSError:
        pass

    if not warnings:
        print("  submission self-check: nothing to fix")
        return 0

    print("")
    print("  " + "!" * 68)
    print("  !  SUBMISSION WARNINGS - {} thing(s) to fix before you hand in".format(len(warnings)))
    print("  " + "!" * 68)
    for w in warnings:
        print("  !  - " + w)
    print("  !")
    print("  !  None of this blocks your build, and none of it is about whether")
    print("  !  your answers are right. It is about whether your work can be")
    print("  !  read and checked. Unresolved items are recorded in")
    print("  !  {} and travel with your submission.".format(WARNINGS_FILE))
    print("  " + "!" * 68)
    print("")
    return 0


if __name__ == "__main__":
    sys.exit(main())
