#!/usr/bin/env python3
"""P40 21.08. — Q-C: nese trpaslíkův zisk klec, nebo attrition?
Attrition = sum(home_attr) trpaslíka (home) v hrách, kde rameno běželo na něm
(cand_home=true). Test: kolik z ΔTD (+0,0465 P38 vs placebo) vysvětlí Δattr
přes within-arm sklon TD~attr. Jmenovatele vypsané."""
import json, glob, math

def load(pat):
    rows = []
    for p in sorted(glob.glob(pat)):
        with open(p) as f:
            rows.extend(json.loads(l) for l in f)
    return rows

p38 = load("/home/jan/claude/bloodbowl/cageadvance_20260819/dw-we_s*/diag_cageadvance_rows.jsonl")
p40 = load("/home/jan/claude/bloodbowl/placebo_20260820/dw-we_s*/diag_placebo_rows.jsonl")

def mean_se(xs):
    n = len(xs); m = sum(xs)/n
    v = sum((x-m)**2 for x in xs)/(n-1)
    return m, math.sqrt(v/n), n

def slope(xs, ys):
    n = len(xs)
    mx, my = sum(xs)/n, sum(ys)/n
    sxx = sum((x-mx)**2 for x in xs)
    sxy = sum((x-mx)*(y-my) for x, y in zip(xs, ys))
    b = sxy/sxx
    # SE sklonu
    res = [y - my - b*(x-mx) for x, y in zip(xs, ys)]
    s2 = sum(r*r for r in res)/(n-2)
    return b, math.sqrt(s2/sxx)

for side, sel, attr_key, td_key in (
        ("dwarf (rameno na něm)", lambda r: r["cand_home"], "home_attr", "cand"),
        ("wood-elf (rameno na něm)", lambda r: not r["cand_home"], "away_attr", "cand"),
        ("dwarf (základna)", lambda r: not r["cand_home"], "home_attr", "base"),
        ("wood-elf (základna)", lambda r: r["cand_home"], "away_attr", "base")):
    out = {}
    for tag, rows in (("P38", p38), ("P40", p40)):
        rr = [r for r in rows if sel(r)]
        at = [sum(r[attr_key]) for r in rr]
        td = [r[td_key] for r in rr]
        ma, sa, n = mean_se(at)
        mt, st_, _ = mean_se(td)
        b, sb = slope(at, td)
        out[tag] = (ma, sa, mt, b, sb, n)
        print(f"{side} {tag}: attr {ma:.4f} ± {sa:.4f}, TD {mt:.4f}, "
              f"sklon TD~attr {b:+.4f} ± {sb:.4f}  (n={n})")
    (ma1, sa1, mt1, b1, sb1, _), (ma0, sa0, mt0, b0, sb0, _) = out["P38"], out["P40"]
    dattr = ma1 - ma0; se_da = math.sqrt(sa1*sa1 + sa0*sa0)
    dtd = mt1 - mt0
    bmean = (b1 + b0) / 2
    print(f"  Δattr(P38-P40) = {dattr:+.4f} ± {se_da:.4f} ({dattr/se_da:+.2f}σ); "
          f"ΔTD = {dtd:+.4f}")
    print(f"  attrition vysvětlí: Δattr × sklon = {dattr*bmean:+.5f} "
          f"z ΔTD {dtd:+.4f}  ({100*dattr*bmean/dtd if dtd else 0:.1f} %)\n")
