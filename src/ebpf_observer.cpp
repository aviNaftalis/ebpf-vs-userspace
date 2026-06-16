// src/ebpf_observer.cpp
//
// Userspace loader. Order matters so we can filter by the target's PID:
//   open  -> fork the target (gated, not yet exec'd) -> set target_pid in the
//   program's read-only global -> load + attach -> release the target -> wait
//   -> read the counters straight out of the program's shared globals.
// The observer itself does almost no work; the kernel did the counting.

#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#include "error_count.skel.h"

int main(int argc, char** argv) {
    int sep = -1;
    for (int i = 1; i < argc; ++i)
        if (std::strcmp(argv[i], "--") == 0) { sep = i; break; }
    if (sep < 0 || sep + 1 >= argc) {
        std::fprintf(stderr, "usage: %s -- <command> [args...]\n", argv[0]);
        return 2;
    }
    char** cmd = &argv[sep + 1];

    struct error_count* skel = error_count__open();
    if (!skel) {
        std::fprintf(stderr, "ebpf_observer: failed to open BPF object\n");
        return 1;
    }

    // Gate the child so it doesn't write until the program is attached.
    int gate[2];
    if (pipe(gate) != 0) { perror("pipe"); return 1; }

    pid_t pid = fork();
    if (pid == 0) {
        close(gate[1]);
        char c;
        (void)!read(gate[0], &c, 1); // block until parent says go
        close(gate[0]);
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) dup2(devnull, STDOUT_FILENO);
        execvp(cmd[0], cmd);
        perror("execvp");
        _exit(127);
    }
    close(gate[0]);

    skel->rodata->target_pid = pid;          // filter to this PID
    if (error_count__load(skel)) {
        std::fprintf(stderr, "ebpf_observer: failed to load\n");
        return 1;
    }
    if (error_count__attach(skel)) {
        std::fprintf(stderr, "ebpf_observer: failed to attach\n");
        error_count__destroy(skel);
        return 1;
    }

    char go = 1;
    (void)!write(gate[1], &go, 1); // release the target
    close(gate[1]);

    int status = 0;
    waitpid(pid, &status, 0);

    const unsigned long long events = skel->bss->write_events;
    const unsigned long long errors = skel->bss->error_lines;
    std::fprintf(stderr, "[ebpf] write_events=%llu error_lines=%llu\n", events, errors);
    std::printf("%llu\n", errors); // ERROR count on stdout for the harness
    error_count__destroy(skel);
    return 0;
}
