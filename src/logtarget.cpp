// src/logtarget.cpp
//
// The workload being observed: a process that emits N log lines, a fixed
// percentage prefixed "ERROR", the rest "INFO". One write() syscall per line
// (no buffering) so the syscall rate is high — that's what makes the cost of
// *observing* each write visible. Writes go to stdout (the harness redirects it).

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

int main(int argc, char** argv) {
    long lines = 200000;
    int error_pct = 10;
    for (int i = 1; i + 1 < argc; i += 2) {
        std::string k = argv[i], v = argv[i + 1];
        if (k == "--lines") lines = std::atol(v.c_str());
        else if (k == "--error-pct") error_pct = std::atoi(v.c_str());
    }

    char buf[64];
    for (long i = 0; i < lines; ++i) {
        const bool err = (i % 100) < error_pct;
        int n = std::snprintf(buf, sizeof(buf), "%-6s%08ld request handled ok\n",
                              err ? "ERROR" : "INFO", i);
        if (n < 0) return 1;
        if (n > static_cast<int>(sizeof(buf))) n = sizeof(buf);
        ssize_t w = ::write(STDOUT_FILENO, buf, static_cast<size_t>(n));
        (void)w;
    }
    return 0;
}
