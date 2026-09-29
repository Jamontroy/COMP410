#!/usr/bin/env python3
"""
Pre-submission self-check. Run by `make test`; never blocks your build.

It looks for mechanical problems that would stop your work being read properly:
a renamed output column, a missing report heading, an unfilled manifest, a
build artifact in the archive.
"""
import csv
import glob
import json
import os
import sys

REQUIRED_COLUMNS = ['impl', 'readers', 'trials', 'reads_per_reader', 'reads_total', 'updates', 'torn_reads', 'freed_versions', 'median_s', 'spread_s', 'throughput_reads_s']
CSV_PATTERN = 'bench/*.csv'
ANSWER_KEYS = ['read_path_calls', 'uaf_announce_release_tso', 'uaf_announce_release_arm64', 'uaf_deref_acquire_tso', 'uaf_deref_acquire_arm64', 'verdict_1_reader', 'verdict_8_readers', 'free_line']
REPORT_HEADINGS = ['What', 'Results', 'Paper connection', 'Citations']
MANIFEST = 'submission.json'
MANIFEST_KEYS = ['label', 'student', 'name', 'what_i_built', 'entrypoint']
ARTIFACT_NAMES = ("a.out", "nullcall", "costscale")
ARTIFACT_SUFFIXES = (".o", ".so", ".ko", ".pyc")

#: Next to the report when there is no bench/ directory (shell assignments),
#: so the file still travels with the submission.
WARNINGS_FILE = (os.path.join("bench", "SUBMISSION-WARNINGS.txt")
                 if os.path.isdir("bench") else "SUBMISSION-WARNINGS.txt")


def check_answers_block():
    """Assignments whose REPORT carries fixed factual answers (141 A3/A4)."""
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
            "No benchmark CSV found at {}. Run `make bench` so your results "
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
                "REPORT.md has no `## {}` heading. The four headings are "
                "checked by name.".format(h))
    if "*replace" in lowered or "*what you" in lowered:
        out.append("REPORT.md still contains the starter's italic prompt text.")
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
        elif value.lower() in ("yourlastname", "your firstname lastname",
                               "your full name", "one sentence, in your own words",
                               "the exact command that runs your work"):
            out.append("{}: `{}` still holds the example value {!r}."
                       .format(MANIFEST, key, value))
    return out


def check_artifacts():
    out = []
    found = []
    for root, dirs, files in os.walk("."):
        dirs[:] = [d for d in dirs if d not in (".git", ".gradevenv", "__pycache__")]
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
               check_manifest, check_artifacts):
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
