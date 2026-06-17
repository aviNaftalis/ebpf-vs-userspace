# ebpf-vs-userspace

[![ci](https://github.com/aviNaftalis/ebpf-vs-userspace/actions/workflows/ci.yml/badge.svg)](https://github.com/aviNaftalis/ebpf-vs-userspace/actions/workflows/ci.yml)

You've got a process spitting out logs, and you want to count the `ERROR` lines —
without stopping it. What's the cheapest way to watch it?

We print 200,000 log lines (one `write()` each) and count the ERRORs four ways,
measuring how much each one slows the process down vs leaving it alone
(**baseline = nobody watching = 1×**):

- **eBPF** — a tiny kernel hook peeks at every `write()`. The process never notices.
- **pipe | grep** — the classic: pipe the logs to `grep ERROR` on another core.
- **strace** — `ptrace`: stop the process and inspect every `write()` from outside.

All four get the right answer (20,000 ERRORs). Here's the cost:

![results](docs/img/results.png)

**eBPF smokes `strace`** (~100×): strace freezes the process on *every* syscall;
eBPF doesn't. But **eBPF isn't free**, and here a humble `pipe | grep` actually
beats it — because `grep` gets its own core.

So take that core away — pin everything to one CPU:

![same core](docs/img/cores.png)

Now `grep` has to fight the process for the CPU and `pipe | grep` falls apart,
while eBPF (inline in the kernel, no extra process to schedule) barely moves.
**`pipe | grep`'s win was the spare core.**

And eBPF pulls further ahead with bigger log lines — it peeks 5 bytes while `grep`
copies and scans every byte:

![sizes](docs/img/sizes.png)

## So when do you use what?

- **eBPF** — watching a process you can't change, or firehose data you only want a
  summary of. Cheap and invisible. (It can't *parse*, though — the kernel verifier
  bans loops and regex, so it only checks a fixed prefix.)
- **pipe | grep** — you control the app and have a spare core. Simple, hard to beat.
- **strace** — debugging, not production. Brutal at scale.

<details><summary>exact numbers (auto-refreshed by CI)</summary>

<!-- RESULTS:START -->
_CI populates this on every push to `main`._
<!-- RESULTS:END -->

</details>

## Run it

Linux with BTF (`/sys/kernel/btf/vmlinux`) + root:

```bash
make && sudo ./scripts/bench.sh   # benchmark + write the CSVs
python3 scripts/plot.py           # render the charts
```

CI runs all of this on `ubuntu-latest` and commits the refreshed charts + numbers
back here on every push. Files: [`error_count.bpf.c`](src/error_count.bpf.c) (the
kernel hook) · [`ebpf_observer.cpp`](src/ebpf_observer.cpp) (loader) ·
[`logtarget.cpp`](src/logtarget.cpp) (the workload) · [`bench.sh`](scripts/bench.sh) ·
[`plot.py`](scripts/plot.py).

*One CI runner's numbers — the ratios and shapes are the point. Linux-only.*
