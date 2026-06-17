# Build the XDP object (CO-RE), generate its skeleton, and the userspace tools.
# Requires: clang, bpftool, libbpf-dev, and /sys/kernel/btf/vmlinux (BTF).

CLANG   ?= clang
BPFTOOL ?= bpftool
CXX     ?= g++
ARCH    := $(shell uname -m | sed 's/x86_64/x86/; s/aarch64/arm64/')

BPF_CFLAGS := -O2 -g -target bpf -D__TARGET_ARCH_$(ARCH) -I.
CXXFLAGS   := -O2 -std=c++17 -Wall

.PHONY: all clean
all: xdp_loader udp_flood udp_sink

vmlinux.h:
	$(BPFTOOL) btf dump file /sys/kernel/btf/vmlinux format c > $@

src/xdp_count.bpf.o: src/xdp_count.bpf.c vmlinux.h
	$(CLANG) $(BPF_CFLAGS) -c $< -o $@

src/xdp_count.skel.h: src/xdp_count.bpf.o
	$(BPFTOOL) gen skeleton $< name xdp_count > $@

xdp_loader: src/xdp_loader.cpp src/xdp_count.skel.h
	$(CXX) $(CXXFLAGS) -Isrc src/xdp_loader.cpp -lbpf -lelf -lz -o $@

udp_flood: src/udp_flood.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

udp_sink: src/udp_sink.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

clean:
	rm -f xdp_loader udp_flood udp_sink vmlinux.h src/*.bpf.o src/*.skel.h
