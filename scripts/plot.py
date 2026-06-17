#!/usr/bin/env python3
"""Render the XDP packet-drop chart (hand-drawn / xkcd style) into docs/img/xdp.png.

Reads packets.csv (bytes,method,pps): packets/sec each method drained from the
same UDP flood, at small vs large payloads.

Usage: python3 scripts/plot.py [packets.csv]   (needs matplotlib + numpy)
"""
import csv
import os
import sys

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.ticker import FuncFormatter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATA = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "packets.csv")
IMG = os.path.join(ROOT, "docs", "img")
os.makedirs(IMG, exist_ok=True)

COLOR = {"XDP": "#2ca02c", "iptables": "#1f77b4", "userspace": "#d62728"}
METHODS = ["XDP", "iptables", "userspace"]
LABELS = {"XDP": "XDP\n(driver)", "iptables": "iptables\n(netfilter)",
          "userspace": "userspace\n(recv)"}


def human(v):
    if v >= 1e6:
        return f"{v / 1e6:.1f}M"
    if v >= 1e3:
        return f"{v / 1e3:.0f}K"
    return f"{v:.0f}"


def main():
    if not os.path.exists(DATA):
        print(f"no {DATA} — skipping xdp.png")
        return
    d, sizes = {}, []
    with open(DATA, newline="") as f:
        for r in csv.DictReader(f):
            d[(r["bytes"], r["method"])] = float(r["pps"])
            if r["bytes"] not in sizes:
                sizes.append(r["bytes"])
    sizes = sorted(sizes, key=int)

    with plt.xkcd():
        fig, axes = plt.subplots(1, len(sizes), figsize=(5.2 * len(sizes), 6.0),
                                 squeeze=False)
        for ci, size in enumerate(sizes):
            ax = axes[0][ci]
            ys = [d.get((size, m), 0) for m in METHODS]
            ax.bar(range(len(METHODS)), ys, color=[COLOR[m] for m in METHODS])
            for i, v in enumerate(ys):
                ax.text(i, v, human(v) + "\npps", ha="center", va="bottom", fontsize=9)
            ax.set_xticks(range(len(METHODS)))
            ax.set_xticklabels([LABELS[m] for m in METHODS])
            ax.set_title(f"{size} B packets")
            ax.set_ylim(0, max(ys) * 1.25 if max(ys) else 1)
            ax.yaxis.set_major_formatter(FuncFormatter(lambda v, _: human(v)))
            if ci == 0:
                ax.set_ylabel("packets/sec dropped (higher = keeps up)")
        fig.suptitle("Dropping a UDP flood: kernel (XDP / iptables) keeps up, "
                     "userspace can't\n(veth in CI — software senders, not a real NIC)")
        fig.tight_layout()
        fig.savefig(os.path.join(IMG, "xdp.png"), dpi=130)
        plt.close(fig)
    print("wrote xdp.png")


if __name__ == "__main__":
    main()
