#!/usr/bin/env python3
"""Render the eBPF-vs-userspace charts (hand-drawn / xkcd style) into docs/img/.

  results.png  bar charts: throughput + slowdown for the headline workload
  sizes.png    line chart: slowdown vs message size — the good-vs-bad use case
               (tiny messages -> pipe|grep wins; big messages -> eBPF wins)

Usage: python3 scripts/plot.py [results.csv]   (needs matplotlib + numpy)
"""
import csv
import os
import sys

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RESULTS = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "results.csv")
SIZES = os.path.join(os.path.dirname(RESULTS), "sizes.csv")
IMG = os.path.join(ROOT, "docs", "img")
os.makedirs(IMG, exist_ok=True)

COLOR = {"baseline": "#7f7f7f", "eBPF": "#2ca02c", "pipe|grep": "#ff7f0e",
         "strace": "#d62728"}


def rows(path):
    with open(path, newline="") as f:
        return list(csv.DictReader(f))


def plot_results():
    data = rows(RESULTS)
    methods = [r["method"] for r in data]
    tput = [float(r["throughput_klps"]) for r in data]
    slow = [float(r["slowdown"]) for r in data]
    colors = [COLOR.get(m, "#1f77b4") for m in methods]

    with plt.xkcd():
        fig, (a_tp, a_sl) = plt.subplots(1, 2, figsize=(12, 5.4))

        a_tp.bar(methods, tput, color=colors)
        a_tp.set_yscale("log")
        a_tp.set_ylabel("throughput (K lines/s) — higher better")
        a_tp.set_title("how fast the watched process runs")
        for i, v in enumerate(tput):
            a_tp.text(i, v, f"{v:,.0f}", ha="center", va="bottom")

        a_sl.bar(methods, slow, color=colors)
        a_sl.set_yscale("log")
        a_sl.set_ylim(bottom=0.8)
        a_sl.axhline(1.0, color="gray", ls="--")
        a_sl.set_ylabel("slowdown vs baseline (x) — lower better")
        a_sl.set_title("the tax for watching")
        for i, v in enumerate(slow):
            a_sl.text(i, v, f"{v:g}x", ha="center", va="bottom")
        # a little bazzaz
        if "strace" in methods:
            i = methods.index("strace")
            a_sl.annotate("ptrace: a context\nswitch PER syscall!",
                          xy=(i, slow[i]), xytext=(i - 1.4, slow[i] * 0.5),
                          arrowprops=dict(arrowstyle="->"))
        if "eBPF" in methods:
            i = methods.index("eBPF")
            a_sl.annotate("in-kernel, but\nstill not free",
                          xy=(i, slow[i]), xytext=(i - 0.6, slow[i] * 4),
                          arrowprops=dict(arrowstyle="->"))

        for a in (a_tp, a_sl):
            a.tick_params(axis="x", rotation=12)
        fig.suptitle("eBPF vs userspace: it CRUSHES strace, but a humble pipe|grep\n"
                     "can still win when you have a spare core")
        fig.tight_layout()
        fig.savefig(os.path.join(IMG, "results.png"), dpi=130)
        plt.close(fig)
    print("wrote results.png")


def plot_sizes():
    if not os.path.exists(SIZES):
        print("no sizes.csv — skipping sizes.png")
        return
    series = {}
    for r in rows(SIZES):
        series.setdefault(r["method"], []).append(
            (int(r["bytes"]), float(r["slowdown"])))
    with plt.xkcd():
        fig, ax = plt.subplots(figsize=(10, 6))
        for m in ("baseline", "pipe|grep", "eBPF"):
            if m not in series:
                continue
            pts = sorted(series[m])
            ax.plot([p[0] for p in pts], [p[1] for p in pts], "-o",
                    color=COLOR.get(m), label=m, lw=2.5)
        ax.set_xscale("log", base=2)
        ax.set_yscale("log")
        ax.set_xlabel("message size (bytes per write)")
        ax.set_ylabel("slowdown vs baseline (x) — lower better")
        ax.set_title("good case vs bad case for eBPF\n"
                     "eBPF cost is PER EVENT; pipe|grep pays PER BYTE")
        ax.legend()
        ax.annotate("BAD for eBPF:\ntiny msgs, lots of\nsyscall overhead\n-> pipe|grep wins",
                    xy=(0.04, 0.04), xycoords="axes fraction", va="bottom")
        ax.annotate("GOOD for eBPF:\nbig msgs -> pipe|grep must\nshovel every byte;\neBPF just peeks 5",
                    xy=(0.62, 0.7), xycoords="axes fraction", va="top")
        fig.tight_layout()
        fig.savefig(os.path.join(IMG, "sizes.png"), dpi=130)
        plt.close(fig)
    print("wrote sizes.png")


def main():
    plot_results()
    plot_sizes()


if __name__ == "__main__":
    main()
