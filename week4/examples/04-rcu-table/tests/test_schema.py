"""
Schema check for your benchmark output.

This does not judge your results. It checks that the CSV your code writes still
has the columns the assignment analyses, so that your numbers can be read back
and verified -- by you, and during review.

If it fails, you have renamed or removed a column.
"""
import csv
import glob
import os

REQUIRED = ['impl', 'readers', 'trials', 'reads_per_reader', 'reads_total', 'updates', 'torn_reads', 'freed_versions', 'median_s', 'spread_s', 'throughput_reads_s']
PATTERN = 'bench/*.csv'


def test_bench_csv_exists():
    files = glob.glob(PATTERN)
    assert files, (
        "No benchmark CSV found at {}. Run `make bench` before `make test`, "
        "and make sure it writes its results there.".format(PATTERN)
    )


def test_bench_csv_has_required_columns():
    files = glob.glob(PATTERN)
    if not files:
        return  # reported by the test above
    problems = []
    matched_any = False
    for path in files:
        with open(path, newline="") as fh:
            header = next(csv.reader(fh), [])
        present = {h.strip().lower() for h in header if h}
        if not present:
            problems.append("{} has no header row".format(os.path.basename(path)))
            continue
        # A file sharing none of the required columns is a different artifact
        # (a summary, a log); only flag files that look like the study output.
        overlap = [c for c in REQUIRED if c.lower() in present]
        if not overlap:
            continue
        matched_any = True
        missing = [c for c in REQUIRED if c.lower() not in present]
        if missing:
            problems.append(
                "{} is missing {} (has: {})".format(
                    os.path.basename(path), ", ".join(missing),
                    ", ".join(sorted(present)))
            )
    # A submission where NO file matches the schema has replaced the layout
    # wholesale rather than extended it. That is the silent case: one real
    # submission passed every check by being unrecognisable.
    assert matched_any, (
        "None of your CSVs use the expected columns ({}).\n"
        "Keep the starter's output format: the analysis reads these by name, "
        "and a renamed or restructured file cannot be checked at all."
        .format(", ".join(REQUIRED))
    )
    assert not problems, (
        "Your benchmark output no longer matches the expected schema:\n  "
        + "\n  ".join(problems)
        + "\n\nKeep these columns: " + ", ".join(REQUIRED)
        + "\nExtra columns are fine; renamed ones are not, because the "
          "analysis reads them by name."
    )


def test_rows_are_numeric_where_expected():
    """A column that should hold numbers must hold numbers."""
    numeric = [c for c in REQUIRED if c.lower() not in ("algo", "algorithm",
                                                        "backend", "method",
                                                        "dist", "distribution")]
    bad = []
    for path in glob.glob(PATTERN):
        with open(path, newline="") as fh:
            rows = list(csv.DictReader(fh))
        if not rows:
            continue
        cols = {(c or "").strip().lower(): c for c in rows[0]}
        if not any(c.lower() in cols for c in REQUIRED):
            continue
        for i, row in enumerate(rows[:50], start=2):
            for want in numeric:
                real = cols.get(want.lower())
                if real is None:
                    continue
                value = (row.get(real) or "").strip()
                if value == "":
                    continue
                try:
                    float(value)
                except ValueError:
                    bad.append("{} line {}: {}={!r} is not a number".format(
                        os.path.basename(path), i, want, value))
    assert not bad, "Non-numeric values in numeric columns:\n  " + "\n  ".join(bad[:10])
