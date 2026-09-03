# COMP 410 — Advanced Operating Systems
## Week 1 — Bench output

`make bench` prints to stdout; redirect it here when you want a record:

```bash
make bench | tee bench/results.txt
```

Your A1 report should quote the **median and spread**, your **machine**, and the
**crossing cost** (syscall minus control) — not a single raw figure.
