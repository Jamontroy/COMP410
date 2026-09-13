# COMP 410 — Advanced Operating Systems
## Build

```bash
make          # build the two user-space models
make test     
make proc-test # validate the live /proc interface in the guest
make bench    
```
## Run

```bash
cd module && make          # needs Linux + kernel headers
sudo insmod procstat.ko
cat /proc/comp410_procstat
sudo rmmod procstat
cat /proc/comp410_procstat # should now be "No such file or directory"
```
## File map

    module/procstat.c      kernel module: /proc entry, task-state counts, RCU + mutex
    module/Makefile        kbuild makefile; builds procstat.ko against kernel headers
    src/copyguard.c        user-space model of the four user-copy boundary rules
    src/lifecycle.c        user-space model of module load/unload and failing init
    tests/check.sh         boundary-rule, lifecycle, and module source checks
    tests/proc-check.sh    user-space format and invariant checks for /proc output
    tests/results.txt      captured output: 12 of 12 passing
    bench/proc-read.txt    captured read of /proc/comp410_procstat
    bench/dmesg-load-unload.txt   kernel log either side of insmod and rmmod
    bench/dmesg-failing-init.txt  kernel log for the forced init failure
    Makefile               builds copyguard and lifecycle; make test, make bench
    README.md              this file
    REPORT.md              what was built, results, paper connection, citations
    submission.json        assignment metadata

## Environment

    Hypervisor: VirtualBox 7.2.16
    Bare metal
    Kernel Version: 6.12.107+deb13-amd64
    Compiler: gcc 13.3.0
    Flags: -O2 -Wall -Wextra -std=c11

## Notes