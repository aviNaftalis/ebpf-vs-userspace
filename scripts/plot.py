#!/usr/bin/env python3
"""Render the eBPF-vs-userspace charts (hand-drawn / xkcd style) into docs/img/.

  contextswitch.png  per-event cost: how often each method leaves the kernel
  matrix.png         eBPF vs pipe|grep across write size x cores x process load

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
DATADIR = os.path.dirname(RESULTS)
IMG = os.path.join(ROOT, "docs", "img")
os.makedirs(IMG, exist_ok=True)
MCOL = {"eBPF": "#2ca02c", "pipe": "#ff7f0e", "strace": "#d62728"}


def rows(path):
    with open(path, newline="") as f:
        return list(csv.DictReader(f))


def color_for(label):
    return next((c for k, c in MCOL.items() if k in label), "#1f77b4")


def plot_perevent():
    path = os.path.join(DATADIR, "perevent.csv")
    if not os.path.exists(path):
        print("no perevent.csv — skipping contextswitch.png")
        return
    data = rows(path)
    labels = [r["method"] for r in data]
    ns = [float(r["ns_per_event"]) for r in data]
    with plt.xkcd():
        fig, ax = plt.subplots(figsize=(11, 5.8))
        ax.bar(labels, ns, color=[color_for(m) for m in labels])
        ax.set_yscale("log")
        ax.set_ylabel("ns added per write() (log)")
        ax.set_title("What leaving the kernel costs, per write()\n"
                     "strace switches EVERY syscall · pipe|grep batches (~1 per 64 KB) · "
                     "eBPF never switches")
        for i, v in enumerate(ns):
            ax.text(i, v, f"{v:,.0f} ns", ha="center", va="bottom")
        si = next((i for i, m in enumerate(labels) if "strace" in m), None)
        if si is not None:
            ax.annotate("a context switch\nEVERY syscall", xy=(si, ns[si]),
                        xytext=(si - 1.4, ns[si] * 0.4), arrowprops=dict(arrowstyle="->"))
        fig.tight_layout()
        fig.savefig(os.path.join(IMG, "contextswitch.png"), dpi=130)
        plt.close(fig)
    print("wrote contextswitch.png")


def plot_matrix():
    path = os.path.join(DATADIR, "matrix.csv")
    if not os.path.exists(path):
        print("no matrix.csv — skipping matrix.png")
        return
    d = {}
    sizes = []
    for r in rows(path):
        d[(r["size"], r["cores"], r["load"], r["method"])] = float(r["slowdown"])
        if r["size"] not in sizes:
            sizes.append(r["size"])
    sizes = sorted(sizes, key=int)
    size_label = {"64": "small\n(64 B)", "65536": "big\n(64 KB)"}
    cores_list, loads, methods = ["1", "2"], ["idle", "busy"], ["eBPF", "pipe|grep"]
    with plt.xkcd():
        fig, axes = plt.subplots(2, 2, figsize=(12, 8.5), squeeze=False)
        for ri, c in enumerate(cores_list):
            for ci, ld in enumerate(loads):
                ax = axes[ri][ci]
                w = 0.38
                for mi, m in enumerate(methods):
                    xs = [xi + (mi - 0.5) * w for xi in range(len(sizes))]
                    ys = [d.get((s, c, ld, m), 0) for s in sizes]
                    ax.bar(xs, ys, w, color=color_for(m), label=m)
                    for xx, yy in zip(xs, ys):
                        ax.text(xx, yy, f"{yy:g}x", ha="center", va="bottom", fontsize=8)
                ax.set_yscale("log")
                ax.set_ylim(bottom=0.8)
                ax.axhline(1.0, color="gray", ls="--")
                ax.set_xticks(range(len(sizes)))
                ax.set_xticklabels([size_label.get(s, s) for s in sizes])
                ax.set_title(f"{c} core{'' if c == '1' else 's'}, {ld}")
                if ri == 0 and ci == 0:
                    ax.legend(fontsize=9)
                if ci == 0:
                    ax.set_ylabel("slowdown vs unwatched (log)")
        fig.suptitle("eBPF vs pipe|grep across every knob: write size x cores x process load\n"
                     "(slowdown of the watched process; strace omitted — always ~100x)")
        fig.tight_layout()
        fig.savefig(os.path.join(IMG, "matrix.png"), dpi=130)
        plt.close(fig)
    print("wrote matrix.png")


def main():
    plot_perevent()
    plot_matrix()


if __name__ == "__main__":
    main()
