#!/usr/bin/env bash
# bench.sh — the same job (drop a UDP flood) done three ways, measured as
# "packets/sec each method actually handled". Run as root:  sudo ./scripts/bench.sh
#
#   XDP        eBPF in the driver — drop the packet before an sk_buff exists
#   iptables   netfilter DROP — packet gets an sk_buff + walks the IP stack first
#   userspace  bind a UDP socket and recvmmsg() the firehose (full stack + copy)
#
# A pack of senders floods the veth flat-out; whichever method is active is the
# only thing draining it, so each gets the whole offered load to itself. We sweep
# small (64 B) vs large (1400 B) payloads. Output: packets.csv (bytes,method,pps).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
# shellcheck source=scripts/netns.sh
source "$ROOT/scripts/netns.sh"

DUR=${DUR:-4}            # seconds each method is measured
SENDERS=${SENDERS:-3}    # parallel flood processes (out-run the userspace sink)
SIZES=${SIZES:-"64 1400"}

trap net_down EXIT
net_up

echo "bytes,method,pps" > "$ROOT/packets.csv"

ipt_count() {  # packets matched by our DROP rule so far
	ip netns exec "$RXNS" iptables -nvx -L INPUT 2>/dev/null \
		| awk '/DROP/ && /udp/ {print $1; exit}'
}

for size in $SIZES; do
	echo "=== payload ${size} B ===" >&2

	# start the flood (covers all three measurement windows + margin)
	window=$(( DUR * 3 + 8 ))
	pids=()
	for _ in $(seq "$SENDERS"); do
		./udp_flood "$RXIP" "$PORT" "$size" "$window" 2>/dev/null &
		pids+=($!)
	done
	sleep 0.5

	# 1) XDP — attach, drop for DUR, read the in-kernel counter
	xdp=$(ip netns exec "$RXNS" ./xdp_loader "$VETH1" "$DUR" 2>>/tmp/xdp.log)

	# 2) iptables — DROP rule, zero counters, measure the delta over DUR
	ip netns exec "$RXNS" iptables -A INPUT -p udp --dport "$PORT" -j DROP
	ip netns exec "$RXNS" iptables -Z INPUT
	sleep "$DUR"
	c=$(ipt_count); c=${c:-0}
	ipt=$(awk -v c="$c" -v d="$DUR" 'BEGIN{ printf "%.0f", c / d }')
	ip netns exec "$RXNS" iptables -D INPUT -p udp --dport "$PORT" -j DROP

	# 3) userspace — recvmmsg as fast as we can for DUR
	us=$(ip netns exec "$RXNS" ./udp_sink 0.0.0.0 "$PORT" "$DUR" 2>>/tmp/sink.log)

	kill "${pids[@]}" 2>/dev/null || true
	wait 2>/dev/null || true

	echo "${size},XDP,${xdp:-0}"       >> "$ROOT/packets.csv"
	echo "${size},iptables,${ipt:-0}"  >> "$ROOT/packets.csv"
	echo "${size},userspace,${us:-0}"  >> "$ROOT/packets.csv"
done

echo "--- packets.csv ---" >&2
cat "$ROOT/packets.csv" >&2

# Markdown summary on stdout (spliced into the README by update_readme.py).
nproc_n=$(nproc)
kern=$(uname -r)
fmt() { awk -v v="$1" 'BEGIN{ if (v>=1e6) printf "%.1f M", v/1e6; else if (v>=1e3) printf "%.0f K", v/1e3; else printf "%.0f", v }'; }
val() { awk -F, -v b="$1" -v m="$2" '$1==b && $2==m {print $3}' "$ROOT/packets.csv"; }

{
echo "_Measured by CI on $(date -u +%Y-%m-%d) — kernel \`${kern}\`, ${nproc_n} CPUs, over a veth pair (software, not a real NIC; see the caveat above)._"
echo
echo "| payload | XDP (driver) | iptables (netfilter) | userspace (recv) |"
echo "|---|--:|--:|--:|"
for size in $SIZES; do
	printf "| %s B | %s pps | %s pps | %s pps |\n" \
		"$size" "$(fmt "$(val "$size" XDP)")" "$(fmt "$(val "$size" iptables)")" "$(fmt "$(val "$size" userspace)")"
done
echo
echo "Packets/sec each method drained from the same flood. XDP and iptables drop in the kernel and keep up with whatever the (software) senders offer; **userspace can't** — the gap is everything it pays per packet that the kernel paths skip."
}
