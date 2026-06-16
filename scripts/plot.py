#!/usr/bin/env python3
"""Render the eBPF-vs-userspace results from results.csv into docs/img/results.png.

Two panels (log scale — strace dwarfs everything): throughput and slowdown.
Usage: python3 scripts/plot.py [results.csv]   (needs matplotlib + numpy)
"""
import csv
import os
import sys

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CSV = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "results.csv")
IMG = os.path.join(ROOT, "docs", "img")
os.makedirs(IMG, exist_ok=True)

COLOR = {"baseline": "#7f7f7f", "eBPF": "#2ca02c", "pipe|grep": "#ff7f0e",
         "strace": "#d62728"}


def load():
    with open(CSV, newline="") as f:
        return list(csv.DictReader(f))


def main():
    rows = load()
    methods = [r["method"] for r in rows]
    tput = [float(r["throughput_klps"]) for r in rows]
    slow = [float(r["slowdown"]) for r in rows]
    colors = [COLOR.get(m, "#1f77b4") for m in methods]

    fig, (a_tp, a_sl) = plt.subplots(1, 2, figsize=(12, 5.2))

    a_tp.bar(methods, tput, color=colors)
    a_tp.set_yscale("log")
    a_tp.set_ylabel("throughput (K lines/s, log) — higher is better")
    a_tp.set_title("Throughput while observing")
    for i, v in enumerate(tput):
        a_tp.text(i, v, f"{v:,.0f}", ha="center", va="bottom", fontsize=9)

    a_sl.bar(methods, slow, color=colors)
    a_sl.set_yscale("log")
    a_sl.set_ylim(bottom=0.8)
    a_sl.axhline(1.0, color="gray", ls="--", lw=1)
    a_sl.set_ylabel("slowdown vs baseline (×, log) — lower is better")
    a_sl.set_title("Overhead imposed on the observed process")
    for i, v in enumerate(slow):
        a_sl.text(i, v, f"{v:g}×", ha="center", va="bottom", fontsize=9)

    for a in (a_tp, a_sl):
        a.tick_params(axis="x", rotation=12)

    fig.suptitle("Counting a process's ERROR lines: eBPF vs userspace — eBPF crushes "
                 "strace (~50×) but isn't free; pipe|grep wins this high-rate case",
                 fontsize=12)
    fig.tight_layout()
    fig.savefig(os.path.join(IMG, "results.png"), dpi=130)
    plt.close(fig)
    print(f"Wrote {IMG}/results.png")


if __name__ == "__main__":
    main()
