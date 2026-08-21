#!/usr/bin/env python3
"""P40 21.08. — ověření dekódování řádků obou nocí (§1 zadání).
Každý podíl s vypsaným jmenovatelem."""
import json, glob, math
from collections import defaultdict

def load(tag, pat):
    rows = []
    for p in sorted(glob.glob(pat)):
        with open(p) as f:
            for line in f:
                rows.append(json.loads(line))
    print(f"{tag}: {len(rows)} řádků z {len(sorted(glob.glob(pat)))} shardů")
    return rows

def mean_se(xs):
    n = len(xs)
    m = sum(xs)/n
    var = sum((x-m)**2 for x in xs)/(n-1)
    return m, math.sqrt(var/n), n

def per_race(rows, tag):
    # dwarf s ramenem = cand @ cand_home=True; bez = base @ cand_home=False
    dw_arm  = [r["cand"] for r in rows if r["cand_home"]]
    dw_base = [r["base"] for r in rows if not r["cand_home"]]
    we_arm  = [r["cand"] for r in rows if not r["cand_home"]]
    we_base = [r["base"] for r in rows if r["cand_home"]]
    out = {}
    for name, xs in (("dwarf arm", dw_arm), ("dwarf base", dw_base),
                     ("we arm", we_arm), ("we base", we_base)):
        m, se, n = mean_se(xs)
        out[name] = (m, se, n)
        print(f"  {tag} {name:10s}: {m:.4f} ± {se:.4f}  (n={n})")
    return out

p38 = load("P38 (mode6)", "/home/jan/claude/bloodbowl/cageadvance_20260819/dw-we_s*/diag_cageadvance_rows.jsonl")
p40 = load("P40 (mode7)", "/home/jan/claude/bloodbowl/placebo_20260820/dw-we_s*/diag_placebo_rows.jsonl")
a = per_race(p38, "P38")
b = per_race(p40, "P40")

def delta(t, k1, k0, A):
    m1, s1, _ = A[k1]; m0, s0, _ = A[k0]
    d = m1-m0; se = math.sqrt(s1*s1+s0*s0)
    print(f"  {t}: {d:+.4f} ± {se:.4f} ({d/se:+.2f}σ)")
    return d, se

print("\nDelta rasy (arm - base) uvnitř noci:")
d1 = delta("P38 dwarf", "dwarf arm", "dwarf base", a)
d2 = delta("P38 we   ", "we arm", "we base", a)
d3 = delta("P40 dwarf", "dwarf arm", "dwarf base", b)
d4 = delta("P40 we   ", "we arm", "we base", b)
print("\nCena kritéria klece (delta P38 - delta P40), nepárově:")
for nm, (x, sx), (y, sy) in (("dwarf", d1, d3), ("we", d2, d4)):
    d = x-y; se = math.sqrt(sx*sx+sy*sy)
    print(f"  {nm}: {d:+.4f} ± {se:.4f} ({d/se:+.2f}σ)")
print("\nPřímé srovnání ramen (arm P38 - arm P40):")
for nm, k in (("dwarf", "dwarf arm"), ("we", "we arm")):
    m1, s1, _ = a[k]; m0, s0, _ = b[k]
    d = m1-m0; se = math.sqrt(s1*s1+s0*s0)
    print(f"  {nm}: {d:+.4f} ± {se:.4f} ({d/se:+.2f}σ)")
print("\nPosun základen mezi nocemi (P38 base - P40 base):")
for nm, k in (("dwarf", "dwarf base"), ("we", "we base")):
    m1, s1, _ = a[k]; m0, s0, _ = b[k]
    print(f"  {nm}: {m1-m0:+.4f}")

# shoda základen pár po páru
idx40 = {(r["matchup"], r["seed_idx"], r["cand_home"]): r for r in p40}
matched = same = 0
for r in p38:
    k = (r["matchup"], r["seed_idx"], r["cand_home"])
    if k in idx40:
        matched += 1
        if idx40[k]["base"] == r["base"]:
            same += 1
print(f"\nShoda base pár po páru: {same} z {matched} ({100*same/max(1,matched):.1f} %)")

# attrition — zjistit, která kombinace dá 1,043/0,989 a 2,914/2,899
print("\nAttrition interpretace (jen řádky cand_home=True pro dwarf=home):")
for tag, rows in (("P38", p38), ("P40", p40)):
    for side, key in (("dwarf(home)", "home_attr"), ("we(away)", "away_attr")):
        arrs = [r[key] for r in rows]
        n = len(arrs)
        sums = [sum(x) for x in arrs]
        print(f"  {tag} {side}: mean per-elem "
              + " ".join(f"[{i}]={sum(x[i] for x in arrs)/n:.3f}" for i in range(4))
              + f"  sum={sum(sums)/n:.3f}  (n={n})")
