#!/usr/bin/env python3
"""Render the eBPF-vs-userspace charts (hand-drawn / xkcd style) into docs/img/.

  results.png  one bar chart: how much each way of watching slows the process down
  sizes.png    line chart: how that cost changes as the log lines get bigger

Everything is measured against "baseline" = the process running with nobody
watching it (= 1x). Usage: python3 scripts/plot.py [results.csv]
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
    slow = [float(r["slowdown"]) for r in data]
    colors = [COLOR.get(m, "#1f77b4") for m in methods]

    with plt.xkcd():
        fig, ax = plt.subplots(figsize=(9, 5.6))
        ax.bar(methods, slow, color=colors)
        ax.set_yscale("log")
        ax.set_ylim(bottom=0.85)
        ax.axhline(1.0, color="gray", ls="--")
        ax.set_ylabel("times slower than UNWATCHED (log)")
        ax.set_title("How much does each way of counting a process's\n"
                     "ERROR lines slow that process down?\n"
                     "(baseline = nobody watching = 1x)")
        for i, v in enumerate(slow):
            ax.text(i, v, f"{v:g}x", ha="center", va="bottom")
        if "strace" in methods:
            i = methods.index("strace")
            ax.annotate("strace stops the\nprocess on EVERY\nsyscall",
                        xy=(i, slow[i]), xytext=(i - 1.5, slow[i] * 0.35),
                        arrowprops=dict(arrowstyle="->"))
        ax.tick_params(axis="x", rotation=10)
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
        fig, ax = plt.subplots(figsize=(9.5, 6))
        for m in ("eBPF", "pipe|grep"):
            if m in series:
                pts = sorted(series[m])
                ax.plot([p[0] for p in pts], [p[1] for p in pts], "-o",
                        color=COLOR[m], label=m, lw=2.5)
        ax.axhline(1.0, color="gray", ls="--", label="unwatched (1x)")
        ax.set_xscale("log", base=2)
        ax.set_yscale("log")
        ax.set_xlabel("log line size (bytes)")
        ax.set_ylabel("times slower than UNWATCHED (log)")
        ax.set_title("Same task, bigger log lines: who slows the process more?\n"
                     "eBPF reads 5 bytes/line; pipe|grep copies + scans every byte")
        ax.legend()
        ax.annotate("small lines:\npipe|grep wins", xy=(0.03, 0.18),
                    xycoords="axes fraction")
        ax.annotate("big lines:\neBPF wins", xy=(0.7, 0.62),
                    xycoords="axes fraction")
        fig.tight_layout()
        fig.savefig(os.path.join(IMG, "sizes.png"), dpi=130)
        plt.close(fig)
    print("wrote sizes.png")


def plot_cores():
    path = os.path.join(os.path.dirname(RESULTS), "cores.csv")
    if not os.path.exists(path):
        print("no cores.csv — skipping cores.png")
        return
    data = rows(path)
    methods = ["baseline", "eBPF", "pipe|grep", "strace"]
    configs = ["all cores", "1 core"]
    val = {(r["method"], r["cpus"]): float(r["slowdown"]) for r in data}
    with plt.xkcd():
        fig, ax = plt.subplots(figsize=(9, 5.6))
        w = 0.38
        for j, cfg in enumerate(configs):
            xs = [i + (j - 0.5) * w for i in range(len(methods))]
            ys = [val.get((m, cfg), 0) for m in methods]
            ax.bar(xs, ys, w, label=cfg,
                   color=[COLOR[m] for m in methods],
                   hatch=("" if j == 0 else "////"), edgecolor="black")
            for x, y in zip(xs, ys):
                ax.text(x, y, f"{y:g}x", ha="center", va="bottom")
        ax.set_yscale("log")
        ax.set_ylim(bottom=0.85)
        ax.set_xticks(range(len(methods)))
        ax.set_xticklabels(methods)
        ax.set_ylabel("times slower than UNWATCHED (log)")
        ax.set_title("Give grep its own core, and it wins.\n"
                     "Pin everything to ONE core (hatched), and eBPF wins.")
        ax.legend()
        fig.tight_layout()
        fig.savefig(os.path.join(IMG, "cores.png"), dpi=130)
        plt.close(fig)
    print("wrote cores.png")


def main():
    plot_results()
    plot_cores()
    plot_sizes()


if __name__ == "__main__":
    main()
