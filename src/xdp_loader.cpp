// xdp_loader <ifname> <seconds>
//
// Attach the XDP_DROP program to <ifname>, let it drop the UDP flood for
// <seconds>, then print the achieved drop rate (packets/sec) to stdout.
// Tries native (driver) mode first, falls back to generic/SKB mode.
#include <bpf/libbpf.h>
#include <linux/if_link.h>   // XDP_FLAGS_*
#include <net/if.h>          // if_nametoindex
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "xdp_count.skel.h"

static double now_s()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec + ts.tv_nsec / 1e9;
}

int main(int argc, char **argv)
{
	if (argc < 3) {
		fprintf(stderr, "usage: %s <ifname> <seconds>\n", argv[0]);
		return 2;
	}
	const char *ifname = argv[1];
	double secs = atof(argv[2]);

	unsigned int ifindex = if_nametoindex(ifname);
	if (!ifindex) {
		fprintf(stderr, "if_nametoindex(%s): %s\n", ifname, strerror(errno));
		return 1;
	}

	struct xdp_count *skel = xdp_count__open_and_load();
	if (!skel) {
		fprintf(stderr, "xdp_count__open_and_load failed\n");
		return 1;
	}
	int prog_fd = bpf_program__fd(skel->progs.xdp_drop_prog);

	const char *mode = "native";
	int err = bpf_xdp_attach(ifindex, prog_fd, XDP_FLAGS_DRV_MODE, NULL);
	if (err) {
		mode = "generic";
		err = bpf_xdp_attach(ifindex, prog_fd, XDP_FLAGS_SKB_MODE, NULL);
	}
	if (err) {
		fprintf(stderr, "bpf_xdp_attach(%s): %s\n", ifname, strerror(-err));
		xdp_count__destroy(skel);
		return 1;
	}
	__u32 detach_flags =
		(strcmp(mode, "native") == 0) ? XDP_FLAGS_DRV_MODE : XDP_FLAGS_SKB_MODE;

	uint64_t p0 = skel->bss->packets;
	double t0 = now_s();
	struct timespec req = {(time_t)secs, (long)((secs - (long)secs) * 1e9)};
	nanosleep(&req, NULL);
	uint64_t p1 = skel->bss->packets;
	double t1 = now_s();

	bpf_xdp_detach(ifindex, detach_flags, NULL);
	xdp_count__destroy(skel);

	double dt = t1 - t0;
	double pps = dt > 0 ? (double)(p1 - p0) / dt : 0;
	fprintf(stderr, "[xdp] mode=%s dropped=%llu pps=%.0f\n", mode,
		(unsigned long long)(p1 - p0), pps);
	printf("%.0f\n", pps);
	return 0;
}
