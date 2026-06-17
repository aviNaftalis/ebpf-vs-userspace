// XDP program: drop every IPv4/UDP packet at ingress (in the driver, before the
// kernel allocates an sk_buff) and count it. Everything else (ARP, etc.) passes,
// so the veth link still resolves while we flood it with UDP.
#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_endian.h>

char LICENSE[] SEC("license") = "GPL";

// vmlinux.h gives us the structs (ethhdr, iphdr) and the xdp_action enum, but not
// these constants — they live in uapi headers we can't include alongside vmlinux.h.
#define ETH_P_IP 0x0800
#define IPPROTO_UDP 17

__u64 packets = 0;   // .bss — read back from userspace via the skeleton
__u64 bytes = 0;

SEC("xdp")
int xdp_drop_prog(struct xdp_md *ctx)
{
	void *data = (void *)(long)ctx->data;
	void *data_end = (void *)(long)ctx->data_end;

	struct ethhdr *eth = data;
	if ((void *)(eth + 1) > data_end)
		return XDP_PASS;
	if (eth->h_proto != bpf_htons(ETH_P_IP))
		return XDP_PASS;

	struct iphdr *ip = (void *)(eth + 1);
	if ((void *)(ip + 1) > data_end)
		return XDP_PASS;
	if (ip->protocol != IPPROTO_UDP)
		return XDP_PASS;

	__sync_fetch_and_add(&packets, 1);
	__sync_fetch_and_add(&bytes, (__u64)(data_end - data));
	return XDP_DROP;
}
