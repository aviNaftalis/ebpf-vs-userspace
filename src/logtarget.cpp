// src/logtarget.cpp
//
// The workload being observed: a process that emits N log lines, a fixed
// percentage prefixed "ERROR", the rest "INFO". One write() syscall per line.
//
//   --bytes B   pad each line to B bytes (tiny vs huge writes)
//   --work W    do W iterations of CPU work per line — i.e. a process that
//               actually does something, not one that only writes. This is what
//               turns the scary "2.5x" ratios into the <1% they are in reality.

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <unistd.h>

static volatile std::uint64_t g_sink = 0;

static inline void busy(long work) {
    std::uint64_t a = g_sink;
    for (long w = 0; w < work; ++w) a = a * 6364136223846793005ULL + 1442695040888963407ULL;
    g_sink = a;
}

int main(int argc, char** argv) {
    long lines = 200000, work = 0;
    int error_pct = 10, bytes = 0;
    for (int i = 1; i + 1 < argc; i += 2) {
        std::string k = argv[i], v = argv[i + 1];
        if (k == "--lines") lines = std::atol(v.c_str());
        else if (k == "--error-pct") error_pct = std::atoi(v.c_str());
        else if (k == "--bytes") bytes = std::atoi(v.c_str());
        else if (k == "--work") work = std::atol(v.c_str());
    }

    if (bytes <= 0) {
        char buf[64];
        for (long i = 0; i < lines; ++i) {
            const bool err = (i % 100) < error_pct;
            int n = std::snprintf(buf, sizeof(buf), "%-6s%08ld request handled ok\n",
                                  err ? "ERROR" : "INFO", i);
            if (n < 0) return 1;
            if (n > static_cast<int>(sizeof(buf))) n = sizeof(buf);
            (void)!::write(STDOUT_FILENO, buf, static_cast<size_t>(n));
            if (work) busy(work);
        }
        return 0;
    }

    if (bytes < 16) bytes = 16;
    if (bytes > 65536) bytes = 65536;
    std::vector<char> buf(static_cast<size_t>(bytes), 'x');
    char head[32];
    for (long i = 0; i < lines; ++i) {
        const bool err = (i % 100) < error_pct;
        int h = std::snprintf(head, sizeof(head), "%-6s%08ld ", err ? "ERROR" : "INFO", i);
        if (h > 0) std::memcpy(buf.data(), head, std::min<size_t>(h, bytes - 1));
        buf[bytes - 1] = '\n';
        (void)!::write(STDOUT_FILENO, buf.data(), static_cast<size_t>(bytes));
        if (work) busy(work);
    }
    return 0;
}
