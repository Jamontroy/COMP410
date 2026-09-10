# COMP 410 — Advanced Operating Systems
## Week 2 — bench/

Put your captured output here for A2: the `/proc` reads, your test's output, and
the `dmesg` either side of a load and unload.

The assignment asks for evidence, not assertions. "It unloads cleanly" is a
claim; the `dmesg` output around `rmmod` is evidence. Include the **failing-init**
path too — that is the one the rubric marks and the one nobody captures.

```bash
sudo dmesg -C                        # start clean
sudo insmod module/procstat.ko
cat /proc/comp410_procstat  | tee bench/proc-read.txt
sudo rmmod procstat
dmesg | tee bench/dmesg-load-unload.txt
```
