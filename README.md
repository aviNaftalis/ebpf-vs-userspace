# ebpf-vs-userspace

[![ci](https://github.com/aviNaftalis/ebpf-vs-userspace/actions/workflows/ci.yml/badge.svg)](https://github.com/aviNaftalis/ebpf-vs-userspace/actions/workflows/ci.yml)

Count a running process's `ERROR` log lines **without stopping it** — three ways:
**eBPF** (a kernel hook on `write()`), the classic **`pipe | grep`**, or **`strace`**
(`ptrace`). How much does each slow the process down? It depends on three knobs:
**write size, spare cores, and whether the process actually does work.**

**TL;DR**
- **`strace`: always ~100× — never in production.** It context-switches to a userspace
  tracer on *every* syscall.
- **eBPF vs `pipe | grep`: it depends** (see the matrix). `pipe | grep` wins *only* for
  small writes *with a spare core*; eBPF wins on big writes or one core.
- **On a process that does real work, every overhead is tiny (~1.1×).** The scary
  "2.5×" only happens when the process does nothing but write to `/dev/null`.

## Why `strace` is hopeless: the context switch

![per-event cost — how often each leaves the kernel](docs/img/contextswitch.png)

All three watch every `write()`; the difference is **how often each leaves the
kernel**. `strace` switches per syscall (~100×); `pipe | grep` *batches* (the pipe
buffers ~64 KB, so ~1 switch per few thousand writes); eBPF *never* switches.
(eBPF's ~600 ns is inline on the process's own thread; `pipe | grep`'s ~235 ns is
just the pipe-write — `grep` scans on another core.)

## The full picture: every knob

eBPF vs `pipe | grep` across **write size × cores × process load** (`strace` omitted —
always ~100×):

![eBPF vs pipe|grep across all parameters](docs/img/matrix.png)

- **Small writes + a spare core →** `pipe | grep` wins: it batches its switches and
  offloads the scan to another core.
- **Big writes (≈ the pipe's 64 KB buffer) →** batching collapses (it fills every
  write) and it must move every byte → **eBPF wins**.
- **One core →** nothing to offload to → eBPF wins or ties.
- **Busy process (real work per line) →** every overhead shrinks toward ~1.1× — eBPF
  is essentially free; the headline ratios were an artifact of a do-nothing baseline.

## When to use what
- **eBPF** — watch a process you can't change, a firehose you only want summarized, or
  when there's no spare core. Cheap and invisible; can't *parse* (the verifier bans loops/regex).
- **pipe | grep** — you own the app, output is small/streamed, and you have a spare core.
- **strace** — debugging only; brutal at scale.

<details><summary>exact numbers for the headline config (auto-refreshed by CI)</summary>

<!-- RESULTS:START -->
_CI populates this on every push to `main`._
<!-- RESULTS:END -->

</details>

## Run it

Linux with BTF (`/sys/kernel/btf/vmlinux`) + root:

```bash
make && sudo ./scripts/bench.sh && python3 scripts/plot.py
```

CI runs it on `ubuntu-latest` and commits the refreshed charts + numbers here on every
push. Code: [`error_count.bpf.c`](src/error_count.bpf.c) ·
[`ebpf_observer.cpp`](src/ebpf_observer.cpp) · [`logtarget.cpp`](src/logtarget.cpp) ·
[`bench.sh`](scripts/bench.sh) · [`plot.py`](scripts/plot.py).
