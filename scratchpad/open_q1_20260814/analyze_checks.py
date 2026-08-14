#!/usr/bin/env python3
"""OQ1: (H5) selekce/tautologie u prediktorů TD + (H3) korelace kontrol.

Populace: přijímací PLNÉ drivy (>=7 našich kol) — táž populace jako
drives.txt „plné drivy". Kontrolně i všechny plné drivy (jako
diag_drive_predictors_20260813, který nefiltroval na přijímací).

H5 test: prediktor spočtený z CELÉHO drivu (mechanicky svázaný s TD —
kdo skóroval, urazil vzdálenost) vs z PRVNÍCH 3 kol s míčem vs bez
posledního kola. Pokud σ spadne, je původní síla z velké části selekce.

H3: Pearson korelace per-drive agregátů + per-turn korelace uvnitř kol.
Bootstrap po HRÁCH (kola v téže hře nejsou nezávislá).
"""
import json, math, random, sys
from collections import defaultdict

random.seed(7)
DRIVES = [json.loads(l) for l in open(sys.argv[1])]

def agg(d, sel=None, drop_last_if_scored=False):
    """Per-drive agregáty přes kola; sel = indexy kol s nosičem, která brát."""
    turns = d["turns"]
    ball = [t for t in turns if "k9a_got" in t]
    if drop_last_if_scored and d["scored"] and ball:
        ball = ball[:-1]
    if sel is not None:
        ball = ball[:sel]
    if not ball:
        return None
    out = {}
    out["tempo"] = sum(t["k9a_got"] for t in ball) / len(ball)
    out["k9a_marg"] = sum(t["k9a_got"] - t["k9a_need"] for t in ball) / len(ball)
    out["k9a_ok"] = sum(1.0 for t in ball if t["k9a_got"] >= t["k9a_need"]) / len(ball)
    out["blocks"] = sum(t["blocks"] for t in ball) / len(ball)
    out["clean"] = sum(t.get("clean", 0) for t in ball) / len(ball)
    out["dirty"] = sum(t.get("dirty", 0) for t in ball) / len(ball)
    r0 = [t["reach0"] for t in ball if t.get("reach0") is not None]
    out["reach0"] = sum(r0) / len(r0) if r0 else None
    out["fb2"] = sum(t["fb2"] for t in ball) / len(ball)
    out["idle"] = sum(t["idle"] for t in ball) / len(ball)
    out["n_ball"] = len(ball)
    return out

def welch(yes, no):
    if len(yes) < 5 or len(no) < 5:
        return None
    my, mn = sum(yes) / len(yes), sum(no) / len(no)
    vy = sum((x - my) ** 2 for x in yes) / (len(yes) - 1)
    vn = sum((x - mn) ** 2 for x in no) / (len(no) - 1)
    se = math.sqrt(vy / len(yes) + vn / len(no))
    return my, mn, (my - mn) / se if se else 0.0

KEYS = ["tempo", "k9a_marg", "k9a_ok", "blocks", "clean", "dirty",
        "reach0", "fb2", "idle"]

def table(pop, label, **kw):
    rows = []
    for d in pop:
        a = agg(d, **kw)
        if a:
            rows.append((d["scored"], a, d["game"]))
    print(f"\n=== {label}: drivů {len(rows)}, TD {sum(1 for s,_,_ in rows if s)} ===")
    print(f"{'veličina':<10}{'TD':>9}{'bez TD':>9}{'rozdíl':>9}{'σ':>8}")
    for k in KEYS:
        yes = [a[k] for s, a, _ in rows if s and a[k] is not None]
        no = [a[k] for s, a, _ in rows if not s and a[k] is not None]
        w = welch(yes, no)
        if w:
            my, mn, sig = w
            print(f"{k:<10}{my:>9.3f}{mn:>9.3f}{my-mn:>+9.3f}{sig:>7.1f}σ")
    return rows

full_recv = [d for d in DRIVES if d["receiving"] and d["n_our_turns"] >= 7]
full_all = [d for d in DRIVES if d["n_our_turns"] >= 7]

rows_all = table(full_recv, "PŘIJÍMACÍ plné drivy, agregát přes CELÝ drive")
table(full_recv, "totéž BEZ posledního kola TD drivů", drop_last_if_scored=True)
table(full_recv, "jen PRVNÍ 3 kola s míčem", sel=3)
table(full_recv, "první 3 kola s míčem, bez posl. kola TD", sel=3, drop_last_if_scored=True)
table(full_all, "kontrola: VŠECHNY plné drivy (jako 13.08.), celý drive")

# ── feasibility kontrola: tempo_first3 vs TD po koších počátečního need ──
print("\n=== tempo (první 3 kola, bez posl. kola TD) vs TD, po koších need0 ===")
print("need0 = vzdálenost při 1. držení / zbývající kola (proveditelnost od startu)")
buckets = defaultdict(lambda: ([], []))
deg = 0
for d in full_recv:
    fh = d.get("first_hold")
    if not fh or not fh["turns_left"] or fh["turns_left"] <= 0:
        deg += 1
        continue
    need0 = fh["dist"] / fh["turns_left"]
    a = agg(d, sel=3, drop_last_if_scored=True)
    if not a:
        deg += 1
        continue
    b = "≤2.61 (stihnutelné)" if need0 <= 2.61 else "2.61–3.5" if need0 <= 3.5 else ">3.5 (nestihnutelné)"
    buckets[b][d["scored"]].append(a["tempo"])
for b in ["≤2.61 (stihnutelné)", "2.61–3.5", ">3.5 (nestihnutelné)"]:
    no, yes = buckets[b]
    w = welch(yes, no)
    if w:
        my, mn, sig = w
        print(f"  need0 {b:<22} nTD={len(yes):<5} nBez={len(no):<5} "
              f"tempo TD={my:.2f} bez={mn:.2f} Δ={my-mn:+.2f} {sig:+.1f}σ")
    else:
        print(f"  need0 {b:<22} nTD={len(yes):<5} nBez={len(no):<5} málo vzorků")
print(f"  degenerovaných (bez 1. držení / bez kol): {deg}")

# ── H3: korelace kontrol per-drive (celý drive, bez posl. kola TD) ──
def pearson(xs, ys):
    n = len(xs)
    mx, my = sum(xs) / n, sum(ys) / n
    sx = math.sqrt(sum((x - mx) ** 2 for x in xs))
    sy = math.sqrt(sum((y - my) ** 2 for y in ys))
    if sx == 0 or sy == 0:
        return 0.0
    return sum((x - mx) * (y - my) for x, y in zip(xs, ys)) / (sx * sy)

rows = []
for d in full_recv:
    a = agg(d, drop_last_if_scored=True)
    if a and a["reach0"] is not None:
        rows.append((a, d["game"]))
print(f"\n=== H3: korelační matice per-drive agregátů (n={len(rows)} drivů) ===")
print("      " + "".join(f"{k[:7]:>9}" for k in KEYS))
mat = {}
for i, k1 in enumerate(KEYS):
    line = f"{k1[:6]:<6}"
    for j, k2 in enumerate(KEYS):
        if j < i:
            line += f"{'':>9}"
            continue
        xs = [a[k1] for a, _ in rows]
        ys = [a[k2] for a, _ in rows]
        r = pearson(xs, ys)
        mat[(k1, k2)] = r
        line += f"{r:>+9.2f}"
    print(line)

# bootstrap po hrách pro vybrané páry (hraniční / doktrinálně důležité)
by_game = defaultdict(list)
for a, g in rows:
    by_game[g].append(a)
games = list(by_game)
PAIRS = [("tempo", "blocks"), ("tempo", "clean"), ("tempo", "reach0"),
         ("tempo", "dirty"), ("clean", "blocks"), ("dirty", "blocks"),
         ("idle", "blocks"), ("tempo", "fb2"), ("clean", "reach0")]
print("\nbootstrap po hrách (2000×), 95% CI:")
for k1, k2 in PAIRS:
    bs = []
    for _ in range(2000):
        sample = [a for _ in range(len(games))
                  for a in by_game[games[random.randrange(len(games))]]]
        bs.append(pearson([a[k1] for a in sample], [a[k2] for a in sample]))
    bs.sort()
    lo, hi = bs[int(0.025 * len(bs))], bs[int(0.975 * len(bs))]
    star = " *" if lo > 0 or hi < 0 else ""
    print(f"  r({k1},{k2}) = {mat.get((k1,k2), mat.get((k2,k1))):+.3f}  [{lo:+.3f},{hi:+.3f}]{star}")

# ── per-turn korelace UVNITŘ kola (tentýž tah: postup vs bití/rohy) ──
print("\n=== per-turn (tentýž tah, kola s nosičem, bez TD kola) ===")
tt = defaultdict(list)
for d in full_recv:
    ball = [t for t in d["turns"] if "k9a_got" in t]
    if d["scored"] and ball:
        ball = ball[:-1]
    for t in ball:
        tt["dx"].append(t["k9a_got"])
        tt["blocks"].append(t["blocks"])
        tt["clean"].append(t.get("clean", 0))
        tt["dirty"].append(t.get("dirty", 0))
        tt["idle"].append(t["idle"])
        tt["reach0"].append(t["reach0"] if t.get("reach0") is not None else float("nan"))
n = len(tt["dx"])
print(f"kol: {n}")
for k in ("blocks", "clean", "dirty", "idle"):
    print(f"  r(dx, {k}) = {pearson(tt['dx'], tt[k]):+.3f}")
ok = [i for i in range(n) if not math.isnan(tt["reach0"][i])]
print(f"  r(dx, reach0) = "
      f"{pearson([tt['dx'][i] for i in ok], [tt['reach0'][i] for i in ok]):+.3f}  (n={len(ok)})")
