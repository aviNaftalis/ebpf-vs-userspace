# Sourced by bench.sh. Builds a veth pair with one end (the "rx" side) in its own
# network namespace, so traffic actually crosses the virtual wire instead of
# short-circuiting through loopback. The flood runs in the root ns and sends to
# RXIP; the drop method under test sits on VETH1 inside RXNS.
RXNS=xdp_rx
VETH0=xveth0      # root ns (sender side)
VETH1=xveth1      # RXNS (receiver side)
TXIP=10.123.0.1
RXIP=10.123.0.2
PORT=9999

net_down() {
	ip netns del "$RXNS" 2>/dev/null || true
	ip link del "$VETH0" 2>/dev/null || true
}

net_up() {
	net_down
	ip netns add "$RXNS"
	ip link add "$VETH0" type veth peer name "$VETH1"
	ip link set "$VETH1" netns "$RXNS"

	ip addr add "$TXIP/24" dev "$VETH0"
	ip link set "$VETH0" up
	ip netns exec "$RXNS" ip addr add "$RXIP/24" dev "$VETH1"
	ip netns exec "$RXNS" ip link set "$VETH1" up
	ip netns exec "$RXNS" ip link set lo up

	# generic XDP needs the rx queue to keep up; bump backlog a bit
	ip netns exec "$RXNS" sysctl -qw net.core.netdev_max_backlog=5000 || true
}
