# ebpf-vs-userspace

[![ci](https://github.com/aviNaftalis/ebpf-vs-userspace/actions/workflows/ci.yml/badge.svg)](https://github.com/aviNaftalis/ebpf-vs-userspace/actions/workflows/ci.yml)

**What does eBPF actually do better, and than whom?** Not log *parsing* — a native
binary beats it there, and the eBPF verifier won't even let you write a general
parser. eBPF's superpower is **observing another process with almost no overhead**,
because it runs *in the kernel* and never copies each event out to userspace.

This repo proves it with one workload observed four ways. A target process emits
log lines (one `write()` syscall each); the task is to **count the `ERROR` lines
while it runs**:

| method | what it is |
|---|---|
| **baseline** | the target alone, unobserved — the speed ceiling |
| **eBPF** | hook `write()` in-kernel, check the line prefix, count — *target unmodified* |
| **pipe \| grep** | the normal `app \| grep ERROR` pipeline (cooperative userspace) |
| **strace / ptrace** | trace every `write()` from userspace — the thing eBPF replaces |

## Results

<!-- RESULTS:START -->

<!-- RESULTS:END -->

**Expected story:** `eBPF ≈ baseline` (it observes for almost free) **≪ `strace`**
(ptrace pays ~2 context switches *per syscall*, so the target crawls), with
`pipe | grep` in between (an extra process + a copy, but no per-syscall trap).
All three observers report the same correct `ERROR` count — eBPF just gets there
without slowing the target down.

## The catch (why eBPF isn't a parser)

Look at [`src/error_count.bpf.c`](src/error_count.bpf.c): it only checks a
**fixed 5-byte prefix** (`"ERROR"`). That's not a stylistic choice — the kernel
**verifier** forbids what real parsing needs: unbounded loops, a big stack, heap,
regex. eBPF trades generality for safety and locality. So:

- ✅ **eBPF wins** at *low-overhead observation/filtering* of events (syscalls,
  packets, function calls) — where the alternative is shipping every event to
  userspace. It beats `strace` by ~1–2 orders of magnitude here, and beats a
  userspace agent on the overhead it imposes.
- ❌ **eBPF loses** at *general computation* — rich parsing, transformation,
  anything unbounded. That belongs in a native binary (and Docker/VM barely
  change that; they're isolation layers, not a different execution model).

The honest one-liner: **eBPF is the cheapest way to *watch*, not the fastest way
to *compute*.**

## How it works

- [`src/logtarget.cpp`](src/logtarget.cpp) — the observed workload (N lines, one `write()` each).
- [`src/error_count.bpf.c`](src/error_count.bpf.c) — the in-kernel program on the `sys_enter_write` tracepoint; counts in a shared global, no per-event userspace copy.
- [`src/ebpf_observer.cpp`](src/ebpf_observer.cpp) — libbpf loader: load → attach → run the target → read the counters.
- [`scripts/bench.sh`](scripts/bench.sh) — runs all four methods, emits the results table.
- CI (`.github/workflows/ci.yml`) builds it, runs the benchmark **as root on `ubuntu-latest`**, and commits the refreshed table back into this README.

## Run it yourself

Needs Linux with **BTF** (`/sys/kernel/btf/vmlinux`) and **root**:

```bash
sudo apt-get install -y clang llvm libbpf-dev libelf-dev bpftool strace make
make
sudo ./scripts/bench.sh        # prints the Markdown results table
```

## Requirements & caveats

- **Linux only**, recent kernel **with BTF** (CO-RE); needs **root** (`CAP_BPF`/`CAP_SYS_ADMIN`).
- On **WSL2**: works if `/sys/kernel/btf/vmlinux` exists (recent 6.x kernels have it); otherwise use an Ubuntu VM/cloud box — which is what CI does anyway.
- Numbers are one runner's; the *ratios* (eBPF vs strace) are the point, not absolute values.
- A teaching benchmark, not a production tracer — the eBPF program favors clarity.
