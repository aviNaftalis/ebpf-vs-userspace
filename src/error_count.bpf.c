// src/error_count.bpf.c
//
// The eBPF side: a tiny program attached to the write() syscall tracepoint. For
// every write by a process named "logtarget", it peeks the first bytes of the
// buffer *in the kernel* and bumps a counter if the line starts with "ERROR".
// Nothing is copied to userspace per event — the userspace loader just reads the
// final counters from a shared (mmap'd) global.
//
// Note the verifier's constraints in action: no regex, no unbounded scan. We can
// only check a fixed-size prefix. That's the whole point of the comparison —
// eBPF observes almost for free, but it can't do rich parsing.

#include "vmlinux.h"
#include <bpf/bpf_helpers.h>

char LICENSE[] SEC("license") = "GPL";

// Globals live in .bss and are mmap'd, so userspace reads them live after the run.
__u64 write_events = 0;
__u64 error_lines = 0;

static __always_inline int is_target(void) {
    char comm[16];
    bpf_get_current_comm(&comm, sizeof(comm));
    const char want[] = "logtarget";
    for (int i = 0; i < 9; ++i)
        if (comm[i] != want[i]) return 0;
    return 1;
}

SEC("tracepoint/syscalls/sys_enter_write")
int on_write(struct trace_event_raw_sys_enter* ctx) {
    if (!is_target()) return 0;

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
