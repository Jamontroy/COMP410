"""
Schema check for your audit output.

This does not judge your results. It checks that the CSV your code writes
still has the columns the assignment analyses, so that your numbers can be
read back and verified -- by you, and by whoever marks this.

If it fails, you have renamed or removed a column. Add whatever extra
columns you like; just keep these.

Part of the assignment's tests: do not edit it.
"""
import csv
import glob
import os

REQUIRED = ['run', 'seed', 'workers', 'tasks', 'queued', 'completed', 'started', 'shard0_count', 'shard1_count', 'median_latency_us', 'spread_latency_us', 'max_latency_us', 'bug_count', 'bug_fraction', 'shard0_median_latency_us', 'shard1_median_latency_us', 'window_us', 'violation_us', 'episodes', 'span_us', 'violation_fraction', 'pace_late_us']
TEXT_COLUMNS = []
PATTERN = 'bench/*.csv'


def test_bench_csv_exists():
    files = glob.glob(PATTERN)
    assert files, (
        "No audit CSV found at {}. Run `make bench` before `make test`, "
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
    # If NO file matches the schema, the output format was replaced rather
    # than extended, and your numbers can no longer be read back.
    assert matched_any, (
        "None of your CSVs use the expected columns ({}).\n"
        "Keep the starter's output format: the analysis reads these by name, "
        "and a renamed or restructured file cannot be checked at all."
        .format(", ".join(REQUIRED))
    )
    assert not problems, (
        "Your audit output no longer matches the expected schema:\n  "
        + "\n  ".join(problems)
        + "\n\nKeep these columns: " + ", ".join(REQUIRED)
        + "\nExtra columns are fine; renamed ones are not, because the "
          "analysis reads them by name."
    )


def test_rows_are_numeric_where_expected():
    """A column that should hold numbers must hold numbers."""
    # Which columns hold numbers is a property of THIS study, so it comes from
    # the assignment: TEXT_COLUMNS lists them, and when it is empty they are
    # inferred from the data.
    numeric = [c for c in REQUIRED if c.lower() not in TEXT_COLUMNS]
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

# Runs standalone (`python3 tests/test_schema.py`) as well as under pytest, so
# the check works on a machine without pytest.
if __name__ == "__main__":
    import sys as _sys
    _failures = []
    for _name in sorted(n for n in dict(globals()) if n.startswith("test_")):
        _fn = globals()[_name]
        if not callable(_fn):
            continue
        try:
            _fn()
        except AssertionError as _exc:
            _failures.append(_name + ": " + str(_exc))
    for _f in _failures:
        print("FAIL " + _f)
    if _failures:
        _sys.exit(1)
    print("schema ok: " + ", ".join(REQUIRED))
