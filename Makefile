# Build the eBPF object (CO-RE), generate its skeleton, and the userspace tools.
# Requires: clang, bpftool, libbpf-dev, and /sys/kernel/btf/vmlinux (BTF).

CLANG   ?= clang
BPFTOOL ?= bpftool
CXX     ?= g++
ARCH    := $(shell uname -m | sed 's/x86_64/x86/; s/aarch64/arm64/')

BPF_CFLAGS := -O2 -g -target bpf -D__TARGET_ARCH_$(ARCH) -I.
CXXFLAGS   := -O2 -std=c++17 -Wall

.PHONY: all clean
all: logtarget ebpf_observer

vmlinux.h:
	$(BPFTOOL) btf dump file /sys/kernel/btf/vmlinux format c > $@

src/error_count.bpf.o: src/error_count.bpf.c vmlinux.h
	$(CLANG) $(BPF_CFLAGS) -c $< -o $@

src/error_count.skel.h: src/error_count.bpf.o
	$(BPFTOOL) gen skeleton $< name error_count > $@

ebpf_observer: src/ebpf_observer.cpp src/error_count.skel.h
	$(CXX) $(CXXFLAGS) -Isrc src/ebpf_observer.cpp -lbpf -lelf -lz -o $@

logtarget: src/logtarget.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

clean:
	rm -f logtarget ebpf_observer vmlinux.h src/*.bpf.o src/*.skel.h
