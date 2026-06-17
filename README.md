# ebpf-vs-userspace

[![ci](https://github.com/aviNaftalis/ebpf-vs-userspace/actions/workflows/ci.yml/badge.svg)](https://github.com/aviNaftalis/ebpf-vs-userspace/actions/workflows/ci.yml)

Count a running process's `ERROR` log lines **without stopping it** — four ways,
measuring how much each one slows the process down vs unwatched (**baseline = 1×**).

**TL;DR**
- **eBPF beats `strace` ~100×** — no per-syscall context switch.
- **`pipe | grep` beats eBPF** — but *only* because `grep` gets its own core.
- **Pin to one core → eBPF wins** (`grep` can't run in parallel anymore).
- **Bigger log lines → eBPF wins** anyway — it peeks 5 bytes; `grep` scans every byte.

The workload: 200,000 lines, one `write()` each. eBPF hooks `write()` in the kernel;
`pipe | grep` is the classic pipeline; `strace` uses `ptrace`. All return the right count.

![cost of each method](docs/img/results.png)
![same workload pinned to one core](docs/img/cores.png)
![cost vs log-line size](docs/img/sizes.png)

## When to use what
- **eBPF** — watch a process you can't change, or firehose data you only want a
  summary of. Cheap and invisible; can't *parse* (the verifier bans loops/regex).
- **pipe | grep** — you own the app and have a spare core.
- **strace** — debugging only; brutal at scale.

<details><summary>exact numbers (auto-refreshed by CI)</summary>

<!-- RESULTS:START -->
_CI populates this on every push to `main`._
<!-- RESULTS:END -->

</details>

## Run it

Linux with BTF + root: `make && sudo ./scripts/bench.sh && python3 scripts/plot.py`.
CI does this on `ubuntu-latest` and commits the refreshed charts here on every push.
Code: [`error_count.bpf.c`](src/error_count.bpf.c) ·
[`ebpf_observer.cpp`](src/ebpf_observer.cpp) · [`logtarget.cpp`](src/logtarget.cpp) ·
[`bench.sh`](scripts/bench.sh) · [`plot.py`](scripts/plot.py).
