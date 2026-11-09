"""Faza D: benchmark nəticələrindən qrafiklər və xülasə cədvəli.

İstifadə (layihənin kök qovluğundan):
    python scripts/plot_benchmarks.py
    python scripts/plot_benchmarks.py --data docs/benchmark/data --out docs/benchmark

Giriş:  bench/benchmarks.cpp-nin yazdığı CSV faylları
Çıxış:  q1.png ... q7.png və summary.md (əsas rəqəmlər)
"""

import argparse
import csv
from collections import defaultdict
from pathlib import Path

import matplotlib

matplotlib.use("Agg")  # ekran lazım deyil, yalnız fayl
import matplotlib.pyplot as plt

MINE = "#1f77b4"   # mystl: göy
STD = "#ff7f0e"    # std: narıncı
OTHER = ["#2ca02c", "#d62728", "#9467bd", "#8c564b"]


def read(data_dir, name):
    with open(data_dir / name, newline="", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))
    for row in rows:
        for key, value in row.items():
            try:
                row[key] = float(value) if "." in value or "e" in value else int(value)
            except ValueError:
                pass
    return rows


def series(rows, x, y, **where):
    """where şərtlərinə uyğun sətirlərdən (x, y) siyahıları."""
    picked = [r for r in rows if all(r[k] == v for k, v in where.items())]
    picked.sort(key=lambda r: r[x])
    return [r[x] for r in picked], [r[y] for r in picked]


def value_at(rows, column, n, **where):
    for r in rows:
        if r["n"] == n and all(r[k] == v for k, v in where.items()):
            return r[column]
    return None


def style(ax, title, xlabel, ylabel, logy=False):
    ax.set_title(title, fontsize=11)
    ax.set_xlabel(xlabel)
    ax.set_ylabel(ylabel)
    ax.set_xscale("log")
    if logy:
        ax.set_yscale("log")
    ax.grid(True, which="both", alpha=0.3)
    ax.legend(fontsize=8)


def save(fig, out_dir, name):
    fig.tight_layout()
    fig.savefig(out_dir / name, dpi=130)
    plt.close(fig)
    print("  ->", out_dir / name)


# ---------------------------------------------------------------------------


def q1(data, out, summary):
    time = read(data, "q1_push_back_time.csv")
    reloc = read(data, "q1_relocations.csv")
    fig, (a, b) = plt.subplots(1, 2, figsize=(11, 4))

    for impl, color in (("mystl", MINE), ("std", STD)):
        a.plot(*series(time, "n", "ns_per_op", impl=impl), "o-", color=color, label=impl)
    a.set_ylim(bottom=0)
    style(a, "push_back: vaxt / əməliyyat", "n (element sayı)", "ns / push_back")

    for impl, color in (("mystl", MINE), ("std", STD)):
        b.plot(*series(reloc, "n", "relocations_per_element", impl=impl), "o-", color=color,
               label=impl)
    b.axhline(2, color="gray", linestyle="--", label="nəzəri hədd: 2")
    b.axhline(1, color="gray", linestyle=":", label="n = 2^k olanda: < 1")
    b.set_ylim(0, 2.3)
    style(b, "Köçürmələr / element (2×)", "n", "köçürmə / n")
    save(fig, out, "q1.png")

    ns = [r["n"] for r in time if r["impl"] == "mystl"]
    summary.append("## Sual 1: push_back amortized O(1)-dirmi?\n")
    summary.append("| n | mystl ns/op | std ns/op | köçürmə / n (mystl) |")
    summary.append("|---|---|---|---|")
    for n in sorted(set(ns)):
        if n in (1_000, 10_000, 100_000, 1_000_000, 10_000_000):
            summary.append(
                f"| {n:,} | {value_at(time, 'ns_per_op', n, impl='mystl'):.2f} | "
                f"{value_at(time, 'ns_per_op', n, impl='std'):.2f} | "
                f"{value_at(reloc, 'relocations_per_element', n, impl='mystl'):.3f} |")
    summary.append("")


def q2(data, out, summary):
    growth = read(data, "q2_growth.csv")
    curve = read(data, "q2_waste_curve.csv")
    waste = read(data, "q2_waste_summary.csv")
    fig, (a, b, c) = plt.subplots(1, 3, figsize=(15, 4))
    colors = {"2x": MINE, "1.5x": OTHER[0]}

    for policy, color in colors.items():
        a.plot(*series(growth, "n", "ns_per_op", policy=policy), "o-", color=color, label=policy)
        b.plot(*series(growth, "n", "reallocations", policy=policy), "o-", color=color,
               label=policy)
        xs, ys = series(curve, "size", "waste_ratio", policy=policy)
        c.plot(xs, [100 * y for y in ys], "-", color=color, label=policy, linewidth=1)
    a.set_ylim(bottom=0)
    style(a, "push_back vaxtı", "n", "ns / push_back")
    style(b, "Yenidən ayırma sayı", "n", "yenidən ayırma")
    style(c, "Boş yer: (capacity − size) / capacity", "size", "%")
    save(fig, out, "q2.png")

    summary.append("## Sual 2: growth factor 2× və 1.5×\n")
    summary.append("| siyasət | n | ns/op | yenidən ayırma | köçürmə / n | boş yer |")
    summary.append("|---|---|---|---|---|---|")
    for policy in colors:
        for n in (1_000_000, 10_000_000):
            row = next((r for r in growth if r["policy"] == policy and r["n"] == n), None)
            if row:
                summary.append(
                    f"| {policy} | {n:,} | {row['ns_per_op']:.2f} | {row['reallocations']} | "
                    f"{row['relocations'] / n:.2f} | {100 * row['waste_ratio']:.1f}% |")
    summary.append("")
    summary.append("| siyasət | ölçülər | orta boş yer | maksimum boş yer |")
    summary.append("|---|---|---|---|")
    for r in waste:
        summary.append(f"| {r['policy']} | 1 … {r['up_to_size']:,} | "
                       f"{100 * r['mean_waste_ratio']:.1f}% | {100 * r['max_waste_ratio']:.1f}% |")
    summary.append("")


def q3(data, out, summary):
    rows = read(data, "q3_reserve.csv")
    fig, ax = plt.subplots(figsize=(7, 4))
    for impl, color in (("mystl", MINE), ("std", STD)):
        ax.plot(*series(rows, "n", "ns_per_op", impl=impl, variant="no_reserve"), "o-",
                color=color, label=f"{impl}: reserve-siz")
        ax.plot(*series(rows, "n", "ns_per_op", impl=impl, variant="reserve"), "s--",
                color=color, label=f"{impl}: reserve(n)")
    ax.set_ylim(bottom=0)
    style(ax, "reserve(n) ilə və onsuz n push_back", "n", "ns / push_back")
    save(fig, out, "q3.png")

    summary.append("## Sual 3: reserve nə qədər qazandırır?\n")
    summary.append("| n | mystl reserve-siz | mystl reserve | sürətlənmə | std reserve-siz | std reserve | sürətlənmə |")
    summary.append("|---|---|---|---|---|---|---|")
    for n in (10_000, 100_000, 1_000_000, 10_000_000):
        m0 = value_at(rows, "ns_per_op", n, impl="mystl", variant="no_reserve")
        m1 = value_at(rows, "ns_per_op", n, impl="mystl", variant="reserve")
        s0 = value_at(rows, "ns_per_op", n, impl="std", variant="no_reserve")
        s1 = value_at(rows, "ns_per_op", n, impl="std", variant="reserve")
        if m0:
            summary.append(f"| {n:,} | {m0:.2f} | {m1:.2f} | {m0 / m1:.2f}× | "
                           f"{s0:.2f} | {s1:.2f} | {s0 / s1:.2f}× |")
    summary.append("")


def q4(data, out, summary):
    growth = read(data, "q4_growth_noexcept.csv")
    push = read(data, "q4_push_copy_vs_move.csv")
    fig, (a, b) = plt.subplots(1, 2, figsize=(11, 4))
    for impl, color in (("mystl", MINE), ("std", STD)):
        a.plot(*series(growth, "n", "ns_per_op", impl=impl, element="noexcept_move"), "o-",
               color=color, label=f"{impl}: move noexcept")
        a.plot(*series(growth, "n", "ns_per_op", impl=impl, element="throwing_move"), "s--",
               color=color, label=f"{impl}: noexcept YOX")
        b.plot(*series(push, "n", "ns_per_op", impl=impl, mode="push_back(copy)"), "s--",
               color=color, label=f"{impl}: push_back(s)")
        b.plot(*series(push, "n", "ns_per_op", impl=impl, mode="push_back(move)"), "o-",
               color=color, label=f"{impl}: push_back(std::move(s))")
    a.set_ylim(bottom=0)
    b.set_ylim(bottom=0)
    style(a, "Böyümə: string elementləri, noexcept ilə/onsuz", "n", "ns / emplace_back")
    style(b, "Kopya və köçürmə (reserve edilmiş vektor)", "n", "ns / push_back")
    save(fig, out, "q4.png")

    summary.append("## Sual 4: copy ilə move; noexcept unudulanda\n")
    summary.append("| n | impl | move noexcept | noexcept YOX | fərq | push_back(copy) | push_back(move) | fərq |")
    summary.append("|---|---|---|---|---|---|---|---|")
    for n in (10_000, 100_000, 1_000_000):
        for impl in ("mystl", "std"):
            ok = value_at(growth, "ns_per_op", n, impl=impl, element="noexcept_move")
            bad = value_at(growth, "ns_per_op", n, impl=impl, element="throwing_move")
            cp = value_at(push, "ns_per_op", n, impl=impl, mode="push_back(copy)")
            mv = value_at(push, "ns_per_op", n, impl=impl, mode="push_back(move)")
            if ok:
                summary.append(f"| {n:,} | {impl} | {ok:.1f} | {bad:.1f} | {bad / ok:.2f}× | "
                               f"{cp:.1f} | {mv:.1f} | {cp / mv:.2f}× |")
    summary.append("")


def q5(data, out, summary):
    rows = read(data, "q5_insert_erase.csv")
    fig, axes = plt.subplots(1, 2, figsize=(11, 4))
    places = {"front": OTHER[1], "middle": OTHER[0], "back": OTHER[2]}
    for ax, op in zip(axes, ("insert", "erase")):
        for position, color in places.items():
            ax.plot(*series(rows, "n", "ns_per_op", impl="mystl", op=op, position=position),
                    "o-", color=color, label=f"mystl: {position}")
            ax.plot(*series(rows, "n", "ns_per_op", impl="std", op=op, position=position),
                    "x:", color=color, label=f"std: {position}")
        style(ax, f"{op}: mövqeyə görə", "n", "ns / əməliyyat", logy=True)
    save(fig, out, "q5.png")

    summary.append("## Sual 5: ortaya insert/erase nə qədər bahadır?\n")
    summary.append("| n | əməliyyat | əvvəl (mystl / std) | orta (mystl / std) | son (mystl / std) |")
    summary.append("|---|---|---|---|---|")
    for n in (1_000, 100_000, 10_000_000):
        for op in ("insert", "erase"):
            cells = []
            for position in places:
                m = value_at(rows, "ns_per_op", n, impl="mystl", op=op, position=position)
                s = value_at(rows, "ns_per_op", n, impl="std", op=op, position=position)
                cells.append(f"{m:,.0f} / {s:,.0f} ns" if m is not None else "–")
            summary.append(f"| {n:,} | {op} | " + " | ".join(cells) + " |")
    summary.append("")


def q6(data, out, summary):
    rows = read(data, "q6_traversal.csv")
    fig, ax = plt.subplots(figsize=(7.5, 4.5))
    styles = {
        "mystl::vector": (MINE, "o-"),
        "std::vector": (STD, "x--"),
        "list (sequential nodes)": (OTHER[0], "s-"),
        "list (shuffled nodes)": (OTHER[1], "^-"),
    }
    for name, (color, fmt) in styles.items():
        ax.plot(*series(rows, "n", "ns_per_element", container=name), fmt, color=color, label=name)
    # int = 4 bayt; Ryzen 7 5850U: L2 = 512 KB / nüvə, L3 = 16 MB
    ax.axvline(512 * 1024 // 4, color="gray", linestyle=":", linewidth=1)
    ax.axvline(16 * 1024 * 1024 // 4, color="gray", linestyle=":", linewidth=1)
    ax.text(512 * 1024 // 4, ax.get_ylim()[1] if False else 0.05, " L2 (vector)", fontsize=7,
            color="gray")
    ax.text(16 * 1024 * 1024 // 4, 0.05, " L3 (vector)", fontsize=7, color="gray")
    style(ax, "Traversal: vector və linked list (ikisi də O(n))", "n", "ns / element", logy=True)
    save(fig, out, "q6.png")

    summary.append("## Sual 6: cache-in təsiri\n")
    summary.append("| n | mystl::vector | std::vector | list (ardıcıl düyünlər) | list (qarışıq düyünlər) | qarışıq list / vector |")
    summary.append("|---|---|---|---|---|---|")
    for n in (1_000, 100_000, 1_000_000, 10_000_000):
        vals = [value_at(rows, "ns_per_element", n, container=c) for c in styles]
        if vals[0]:
            summary.append(f"| {n:,} | " + " | ".join(f"{v:.3f} ns" for v in vals) +
                           f" | {vals[3] / vals[0]:.0f}× |")
    summary.append("")


def q7(data, out, summary):
    """std::vector ilə müqayisə: mystl / std vaxt nisbəti.

    Tək bir n nöqtəsi səs-küyə həssasdır (CPU tezliyi, OS). Ona görə hər əməliyyat
    üçün n >= 10 000 olan BÜTÜN nöqtələrdəki nisbətlərin həndəsi ortası götürülür,
    min və max isə səpələnmə kimi göstərilir.
    """
    q1t = read(data, "q1_push_back_time.csv")
    q3r = read(data, "q3_reserve.csv")
    q4g = read(data, "q4_growth_noexcept.csv")
    q4p = read(data, "q4_push_copy_vs_move.csv")
    q5r = read(data, "q5_insert_erase.csv")
    q6t = read(data, "q6_traversal.csv")

    def ratio(rows, column, mine, std):
        ratios = []
        for n in sorted({r["n"] for r in rows if r["n"] >= 10_000}):
            m = value_at(rows, column, n, **mine)
            s = value_at(rows, column, n, **std)
            if m and s:
                ratios.append(m / s)
        if not ratios:
            return None
        product = 1.0
        for r in ratios:
            product *= r
        return (product ** (1 / len(ratios)), min(ratios), max(ratios), len(ratios))

    items = [
        ("push_back (int)", ratio(q1t, "ns_per_op", {"impl": "mystl"}, {"impl": "std"})),
        ("reserve + push_back", ratio(q3r, "ns_per_op", {"impl": "mystl", "variant": "reserve"},
                                      {"impl": "std", "variant": "reserve"})),
        ("emplace_back (string)", ratio(q4g, "ns_per_op",
                                        {"impl": "mystl", "element": "noexcept_move"},
                                        {"impl": "std", "element": "noexcept_move"})),
        ("push_back(copy) string", ratio(q4p, "ns_per_op",
                                         {"impl": "mystl", "mode": "push_back(copy)"},
                                         {"impl": "std", "mode": "push_back(copy)"})),
        ("push_back(move) string", ratio(q4p, "ns_per_op",
                                         {"impl": "mystl", "mode": "push_back(move)"},
                                         {"impl": "std", "mode": "push_back(move)"})),
        ("insert (orta)", ratio(q5r, "ns_per_op",
                                {"impl": "mystl", "op": "insert", "position": "middle"},
                                {"impl": "std", "op": "insert", "position": "middle"})),
        ("erase (orta)", ratio(q5r, "ns_per_op",
                               {"impl": "mystl", "op": "erase", "position": "middle"},
                               {"impl": "std", "op": "erase", "position": "middle"})),
        ("traversal", ratio(q6t, "ns_per_element", {"container": "mystl::vector"},
                            {"container": "std::vector"})),
    ]
    items = [(name, r) for name, r in items if r is not None]

    fig, ax = plt.subplots(figsize=(8, 4))
    names = [name for name, _ in items]
    means = [r[0] for _, r in items]
    lower = [r[0] - r[1] for _, r in items]
    upper = [r[2] - r[0] for _, r in items]
    bars = ax.barh(names, means, xerr=[lower, upper], capsize=3,
                   color=[MINE if v <= 1.05 else OTHER[1] for v in means])
    ax.axvline(1.0, color="black", linewidth=1)
    for bar, (_, r) in zip(bars, items):
        ax.text(r[2] + 0.02, bar.get_y() + bar.get_height() / 2, f"{r[0]:.2f}", va="center",
                fontsize=8)
    ax.set_xlabel("mystl / std vaxt nisbəti (1.0 = eyni; < 1 = mystl sürətlidir)")
    ax.set_title("mystl::vector və std::vector (n ≥ 10 000, həndəsi orta; xətt: min–max)",
                 fontsize=10)
    ax.invert_yaxis()
    ax.grid(True, axis="x", alpha=0.3)
    save(fig, out, "q7.png")

    summary.append("## Sual 7: mystl::vector std::vector-a nə qədər yaxındır?\n")
    summary.append("Nisbət = mystl vaxtı / std vaxtı (1.00 = eyni). Hər əməliyyat üçün "
                   "n ≥ 10 000 olan bütün ölçmələrin həndəsi ortası.\n")
    summary.append("| əməliyyat | həndəsi orta | min | max | ölçmə sayı |")
    summary.append("|---|---|---|---|---|")
    for name, r in items:
        summary.append(f"| {name} | {r[0]:.2f} | {r[1]:.2f} | {r[2]:.2f} | {r[3]} |")
    summary.append("")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--data", default="docs/benchmark/data")
    parser.add_argument("--out", default="docs/benchmark")
    args = parser.parse_args()
    data, out = Path(args.data), Path(args.out)
    out.mkdir(parents=True, exist_ok=True)

    summary = ["# Benchmark nəticələri (avtomatik yaradılıb: scripts/plot_benchmarks.py)\n"]
    for step in (q1, q2, q3, q4, q5, q6, q7):
        step(data, out, summary)
    (out / "summary.md").write_text("\n".join(summary), encoding="utf-8")
    print("  ->", out / "summary.md")


if __name__ == "__main__":
    main()
