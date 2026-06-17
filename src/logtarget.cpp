// src/logtarget.cpp
//
// The workload being observed: a process that emits N log lines, a fixed
// percentage prefixed "ERROR", the rest "INFO". One write() syscall per line
// (no buffering) so the syscall rate is high.
//
// --bytes B pads each line to exactly B bytes. That knob drives the use-case
// sweep: tiny lines = lots of syscall overhead per byte of real work; big lines
// = lots of data to move (which punishes anything that copies it to userspace).

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <unistd.h>

int main(int argc, char** argv) {
    long lines = 200000;
    int error_pct = 10;
    int bytes = 0; // 0 = natural short line; >0 = pad to this many bytes
    for (int i = 1; i + 1 < argc; i += 2) {
        std::string k = argv[i], v = argv[i + 1];
        if (k == "--lines") lines = std::atol(v.c_str());
        else if (k == "--error-pct") error_pct = std::atoi(v.c_str());
        else if (k == "--bytes") bytes = std::atoi(v.c_str());
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
        }
        return 0;
    }

    if (bytes < 16) bytes = 16;
    if (bytes > 65536) bytes = 65536;
    std::vector<char> buf(static_cast<size_t>(bytes), 'x'); // padding
    char head[32];
    for (long i = 0; i < lines; ++i) {
        const bool err = (i % 100) < error_pct;
        int h = std::snprintf(head, sizeof(head), "%-6s%08ld ", err ? "ERROR" : "INFO", i);
        if (h > 0) std::memcpy(buf.data(), head, std::min<size_t>(h, bytes - 1)); // keep prefix; rest stays 'x'
        buf[bytes - 1] = '\n';
        (void)!::write(STDOUT_FILENO, buf.data(), static_cast<size_t>(bytes));
    }
    return 0;
}
