// src/error_count.bpf.c
//
// The eBPF side: a tiny program on the write() syscall tracepoint. For every
// write by the *target* PID it peeks the first bytes of the buffer in the kernel
// and bumps a counter if the line starts with "ERROR". Nothing is copied to
// userspace per event — the loader reads the final counters from a shared
// (mmap'd) global.
//
// We filter by PID (cheap: one read of the current pid) rather than by comm
// (which copies 16 bytes per event) — the tracepoint still fires for every
// write on the box, so the per-event filter wants to be as cheap as possible.
//
// Note the verifier's limits: no regex, no unbounded scan — only a fixed-size
// prefix check. That's the point: eBPF observes cheaply but can't parse richly.

#include "vmlinux.h"
#include <bpf/bpf_helpers.h>

char LICENSE[] SEC("license") = "GPL";

const volatile int target_pid = 0; // set by the loader before load()

__u64 write_events = 0;
__u64 error_lines = 0;

SEC("tracepoint/syscalls/sys_enter_write")
int on_write(struct trace_event_raw_sys_enter* ctx) {
    int pid = (int)(bpf_get_current_pid_tgid() >> 32);
    if (pid != target_pid) return 0;

    const char* user_buf = (const char*)ctx->args[1];
    __u64 len = (__u64)ctx->args[2];
    __sync_fetch_and_add(&write_events, 1);

    if (len < 5) return 0;
    char head[5];
    if (bpf_probe_read_user(head, sizeof(head), user_buf) != 0) return 0;
    if (head[0] == 'E' && head[1] == 'R' && head[2] == 'R' &&
        head[3] == 'O' && head[4] == 'R')
        __sync_fetch_and_add(&error_lines, 1);
    return 0;
}
