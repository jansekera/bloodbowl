#!/usr/bin/env python3
"""Analýza řetězu špinavých rohů nad rows.jsonl.gz (14.08.2026).

Metodika:
* dvouvýběrové srovnání = Welch (týž vzorec jako scratchpad/drive_corr.py);
* spojitá×spojitá = Pearson r, σ přes Fisherovu z-transformaci
  (SE = 1/sqrt(n-3));
* parciální korelace = korelace reziduí po OLS na kontroly (intercept
  + OPP3 + REACH0 + turn), řešeno lstsq;
* žádná binarizace prediktorů — počty zůstávají počty; koše se používají
  jen pro ČTENÍ (tabulky), σ se počítá ze spojité verze.
Každá tabulka tiskne n; koš pod 30 vzorků se značí ⚠ (Bucket.MIN_N).
"""
import gzip, json, math, sys
import numpy as np
from collections import defaultdict

SRC = sys.argv[1] if len(sys.argv) > 1 else \
    "/home/jan/claude/bloodbowl/scratchpad/dirty_corner_chain_20260814/rows.jsonl.gz"

rows = [json.loads(l) for l in gzip.open(SRC, "rt")]
print(f"řádků (našich kol): {len(rows)}")

RACES = ["human", "orc", "skaven", "wood-elf"]


def welch(a, b):
    a, b = np.asarray(a, float), np.asarray(b, float)
    if len(a) < 5 or len(b) < 5:
        return None
    d = a.mean() - b.mean()
    se = math.sqrt(a.var(ddof=1) / len(a) + b.var(ddof=1) / len(b))
    return d, (d / se if se else 0.0), len(a), len(b)


def pearson(x, y):
    x, y = np.asarray(x, float), np.asarray(y, float)
    n = len(x)
    if n < 10 or x.std() == 0 or y.std() == 0:
        return None
    r = float(np.corrcoef(x, y)[0, 1])
    z = 0.5 * math.log((1 + r) / (1 - r)) if abs(r) < 1 else 0
    return r, z * math.sqrt(n - 3), n


def partial(x, y, Z):
    """Pearson mezi rezidui x a y po OLS na Z (s interceptem)."""
    x, y = np.asarray(x, float), np.asarray(y, float)
    Z = np.column_stack([np.ones(len(x))] + [np.asarray(z, float) for z in Z])
    rx = x - Z @ np.linalg.lstsq(Z, x, rcond=None)[0]
    ry = y - Z @ np.linalg.lstsq(Z, y, rcond=None)[0]
    return pearson(rx, ry)


def fmt_r(t, label):
    if t is None:
        return f"  {label:<52} N/A (málo vzorků)"
    r, sig, n = t
    return f"  {label:<52} r={r:+.3f}  {sig:+6.1f}σ  n={n}"


def sel(rs, *keys):
    """Řádky, kde jsou všechny klíče přítomné (ne None)."""
    return [r for r in rs if all(r.get(k) is not None for k in keys)]


def cols(rs, *keys):
    return [np.array([r[k] for r in rs], float) for k in keys]


# ────────────────────────────────────────────────────────────────────────
print("\n" + "=" * 72)
print("ČÁST A — P0.1: blok → čistota rohů")
print("=" * 72)

# A1: adresný test na rozhodovacím bodě (začátek N+1).
# Vzorek: kola s aspoň jedním polluterem na začátku N+1 a platným koncem N+1.
A1 = sel(rows, "poll_ids_S2", "dirty_N1", "poll_hit", "blocks_N1")
A1 = [r for r in A1 if len(r["poll_ids_S2"]) > 0]
hit = [r for r in A1 if r["poll_hit"] > 0]
noblock = [r for r in A1 if r["blocks_N1"] == 0]
otherblock = [r for r in A1 if r["blocks_N1"] > 0 and r["poll_hit"] == 0]
print(f"\nA1 — kola s ≥1 polluterem na začátku N+1: n={len(A1)}")
print(f"  z toho: polluter blokován {len(hit)}, blok jen jinam "
      f"{len(otherblock)}, žádný blok {len(noblock)}")
for out, lbl in (("dirty_N1", "špinavé rohy konec N+1"),
                 ("clean_N1", "čisté rohy konec N+1"),
                 ("dx_N1", "Δx nosiče v N+1"),
                 ("td_N1", "TD v N+1")):
    groups = {}
    for name, g in (("hit", hit), ("other", otherblock), ("none", noblock)):
        v = [r[out] for r in g if r.get(out) is not None]
        groups[name] = v
    line = f"  {lbl:<28}"
    for name in ("hit", "other", "none"):
        v = groups[name]
        line += f" {name}={np.mean(v):+.3f}(n={len(v)})" if v else f" {name}=N/A"
    w1 = welch(groups["hit"], groups["other"])
    w2 = welch(groups["hit"], groups["none"])
    if w1: line += f" | hit−other {w1[0]:+.3f} ({w1[1]:+.1f}σ)"
    if w2: line += f" | hit−none {w2[0]:+.3f} ({w2[1]:+.1f}σ)"
    print(line)

# osud pollutera: srazili jsme ho → přestal špinit?
A1b = sel(A1, "poll_still", "poll_down")
if A1b:
    h = [r for r in A1b if r["poll_hit"] > 0]
    o = [r for r in A1b if r["poll_hit"] == 0]
    for g, lbl in ((h, "polluter blokován"), (o, "neblokován")):
        still = np.mean([r["poll_still"] / len(r["poll_ids_S2"]) for r in g])
        down = np.mean([r["poll_down"] / len(r["poll_ids_S2"]) for r in g])
        print(f"  {lbl:<22} podíl polluterů stále špiní {still:.1%}, "
              f"na zemi {down:.1%}  (n={len(g)})")

# A1 kontrola hustoty: stratifikace podle OPP3 na začátku N+1
print("\nA1 stratifikováno podle hustoty OPP3(S2) — špinavé rohy konec N+1:")
for lo, hi in ((0, 2), (3, 4), (5, 99)):
    st = [r for r in A1 if r.get("opp3_S2") is not None
          and lo <= r["opp3_S2"] <= hi]
    hh = [r["dirty_N1"] for r in st if r["poll_hit"] > 0]
    nn = [r["dirty_N1"] for r in st if r["poll_hit"] == 0]
    w = welch(hh, nn)
    if w:
        print(f"  OPP3 {lo}–{hi if hi < 99 else '+'}: hit {np.mean(hh):.3f} "
              f"(n={len(hh)}) vs ne {np.mean(nn):.3f} (n={len(nn)}) "
              f"→ {w[0]:+.3f} ({w[1]:+.1f}σ)")

# A2: bloky v N → rohy v N+1, surově a parciálně
print("\nA2 — bloky v kole N → stav rohů na konci N+1 (jen kola s klecí "
      "na konci N, filled_N ≥ 1):")
A2 = [r for r in sel(rows, "blocks_N", "dirty_N1", "clean_N1", "opp3_N",
                     "reach0_N", "turn", "filled_N") if r["filled_N"] >= 1]
x, dN1, cN1, opp3, re0, turn = cols(A2, "blocks_N", "dirty_N1", "clean_N1",
                                    "opp3_N", "reach0_N", "turn")
print(fmt_r(pearson(x, dN1), "bloky_N → špinavé_N+1 (surové)"))
print(fmt_r(partial(x, dN1, [opp3, re0, turn]),
            "bloky_N → špinavé_N+1 | OPP3,REACH0,kolo"))
print(fmt_r(pearson(x, cN1), "bloky_N → čisté_N+1 (surové)"))
print(fmt_r(partial(x, cN1, [opp3, re0, turn]),
            "bloky_N → čisté_N+1 | OPP3,REACH0,kolo"))
A2x = sel(A2, "dx_N1")
xx, dxx, o3, r0, tt = cols(A2x, "blocks_N", "dx_N1", "opp3_N", "reach0_N", "turn")
print(fmt_r(pearson(xx, dxx), "bloky_N → Δx_N+1 (surové)"))
print(fmt_r(partial(xx, dxx, [o3, r0, tt]),
            "bloky_N → Δx_N+1 | OPP3,REACH0,kolo"))

# ────────────────────────────────────────────────────────────────────────
print("\n" + "=" * 72)
print("ČÁST B — P0.5: špinavý roh N → zámek → roh/tempo N+1")
print("=" * 72)

B = [r for r in sel(rows, "dirty_N", "filled_N", "opp3_N", "reach0_N", "turn")
     if r["filled_N"] >= 1]
print(f"\nkola s klecí (filled_N ≥ 1) a stojícím nosičem: n={len(B)}")

# B0: tautologická část zámků
Bl = sel(B, "locked_N", "locked_corner_N")
d, lN, lc = cols(Bl, "dirty_N", "locked_N", "locked_corner_N")
print(f"  zámky na konci N: průměr {lN.mean():.2f}, z toho na špinavých "
      f"rozích {lc.mean():.2f} (tautologická část)")
print(fmt_r(pearson(d, lN), "špinavé_N → zamčení_N (surové, ~tautologie)"))
print(fmt_r(pearson(d, lN - lc), "špinavé_N → zamčení_N MIMO rohy"))

# B1: perzistence — osud těla ze špinavého rohu na začátku N+1
fates = defaultdict(int)
for r in B:
    for f in r.get("dirty_fates") or []:
        fates[f] += 1
tot = sum(fates.values()) or 1
print(f"\nB1 — osud těla ze špinavého rohu (konec N) na začátku N+1 "
      f"(n={tot} těl):")
for k in ("locked", "down", "out", "free"):
    print(f"  {k:<8} {fates[k]:>6}  {fates[k] / tot:.1%}")
d2c = sel(B, "dirty_to_clean")
if d2c:
    num = sum(r["dirty_to_clean"] for r in d2c)
    den = sum(len(r["dirty_ids"]) for r in d2c)
    print(f"  … a jako ČISTÝ roh na konci N+1 slouží {num}/{den} "
          f"= {num / den:.1%}")

# B2: řetěz do N+1
print("\nB2 — špinavé rohy N → výsledky N+1 (kontrola OPP3, REACH0, kolo, "
      "filled_N):")
B2 = sel(B, "locked_S2", "free_S2")
d, l2, fr, o3, r0, tt, fN = cols(B2, "dirty_N", "locked_S2", "free_S2",
                                 "opp3_N", "reach0_N", "turn", "filled_N")
print(fmt_r(pearson(d, l2), "špinavé_N → zamčení na začátku N+1 (surové)"))
print(fmt_r(partial(d, l2, [o3, r0, tt, fN]),
            "špinavé_N → zamčení_S2 | kontroly"))
print(fmt_r(pearson(d, fr), "špinavé_N → volná těla na začátku N+1 (surové)"))
print(fmt_r(partial(d, fr, [o3, r0, tt, fN]),
            "špinavé_N → volná_S2 | kontroly"))
for out, lbl in (("clean_N1", "čisté rohy N+1"),
                 ("dirty_N1", "špinavé rohy N+1"),
                 ("dx_N1", "Δx nosiče N+1"),
                 ("downed_opp_turn", "naši sražení v soupeřově kole")):
    Bo = sel(B, out)
    d, y, o3, r0, tt, fN = cols(Bo, "dirty_N", out, "opp3_N", "reach0_N",
                                "turn", "filled_N")
    print(fmt_r(pearson(d, y), f"špinavé_N → {lbl} (surové)"))
    print(fmt_r(partial(d, y, [o3, r0, tt, fN]),
                f"špinavé_N → {lbl} | kontroly"))

# mezičlánek: zámky S2 → rohy/tempo N+1
print("\nmezičlánek — zamčená těla na začátku N+1 → výsledek N+1:")
for out, lbl in (("clean_N1", "čisté rohy N+1"), ("dx_N1", "Δx nosiče N+1")):
    Bm = sel(B, "locked_S2", out, "opp3_S2")
    l2, y, o3 = cols(Bm, "locked_S2", out, "opp3_S2")
    print(fmt_r(pearson(l2, y), f"zamčení_S2 → {lbl} (surové)"))
    print(fmt_r(partial(l2, y, [o3]), f"zamčení_S2 → {lbl} | OPP3(S2)"))

# tabulka pro čtení: Δx_N+1 podle počtu špinavých rohů
print("\nΔx nosiče v N+1 podle špinavých rohů na konci N (jen čtení):")
for dv in (0, 1, 2):
    g = [r["dx_N1"] for r in B if r.get("dx_N1") is not None
         and (r["dirty_N"] == dv if dv < 2 else r["dirty_N"] >= 2)]
    lbl = str(dv) if dv < 2 else "2+"
    mark = " ⚠ málo vzorků" if len(g) < 30 else ""
    if g:
        print(f"  špinavých {lbl:<3} n={len(g):<6} Δx = {np.mean(g):+.2f}{mark}")

# B3: splatnost po soupeřích
print("\n" + "-" * 72)
print("B3 — rozpad po soupeřích: okamžitá vs odložená splatnost")
print("-" * 72)
print(f"\n{'soupeř':<10}{'P(ztráta|dirty=0)':>19}{'P(ztráta|≥1)':>15}"
      f"{'Δpp':>7}{'σ':>7}{'n0/n1':>13}")
for race in RACES:
    Rr = [r for r in B if r["opp"] == race and r.get("ball_lost") is not None]
    a = [r["ball_lost"] for r in Rr if r["dirty_N"] == 0]
    b = [r["ball_lost"] for r in Rr if r["dirty_N"] >= 1]
    w = welch(b, a)
    if w:
        print(f"{race:<10}{np.mean(a):>18.1%}{np.mean(b):>15.1%}"
              f"{100 * w[0]:>+7.1f}{w[1]:>+6.1f}σ{len(a):>7}/{len(b)}")

print("\nokamžitá splatnost s kontrolou hustoty (parciální, "
      "| OPP3,REACH0,kolo,filled):")
for race in RACES:
    Rr = sel([r for r in B if r["opp"] == race], "ball_lost")
    d, y, o3, r0, tt, fN = cols(Rr, "dirty_N", "ball_lost", "opp3_N",
                                "reach0_N", "turn", "filled_N")
    print(fmt_r(partial(d, y, [o3, r0, tt, fN]),
                f"{race}: špinavé_N → ztráta míče"))

print("\nodložená splatnost po soupeřích (parciální | OPP3,REACH0,kolo,"
      "filled):")
for out, lbl in (("dx_N1", "Δx_N+1"), ("locked_S2", "zamčení_S2"),
                 ("downed_opp_turn", "sražení naši"),
                 ("clean_N1", "čisté_N+1")):
    for race in RACES:
        Rr = sel([r for r in B if r["opp"] == race], out)
        if len(Rr) < 50:
            continue
        d, y, o3, r0, tt, fN = cols(Rr, "dirty_N", out, "opp3_N", "reach0_N",
                                    "turn", "filled_N")
        print(fmt_r(partial(d, y, [o3, r0, tt, fN]),
                    f"{race}: špinavé_N → {lbl}"))
    print()

# absolutní úrovně pro čtení: Δx podle rasy a dirty
print("Δx_N+1 podle rasy × špinavé rohy (čtecí tabulka):")
print(f"{'soupeř':<10}{'dirty=0':>14}{'dirty=1':>14}{'dirty≥2':>14}")
for race in RACES:
    line = f"{race:<10}"
    for dv in (0, 1, 2):
        g = [r["dx_N1"] for r in B
             if r["opp"] == race and r.get("dx_N1") is not None
             and (r["dirty_N"] == dv if dv < 2 else r["dirty_N"] >= 2)]
        mark = "⚠" if len(g) < 30 else ""
        line += f"{np.mean(g):+8.2f}(n={len(g)}){mark}" if g else "     N/A"
    print(line)

# ────────────────────────────────────────────────────────────────────────
print("\n" + "=" * 72)
print("ČÁST C — rozpočet: co očištění rohu stojí")
print("=" * 72)

# C1: kolik špinavých rohů jde očistit BEZ blitzu (blok zdarma)
C1 = [r for r in rows if r.get("pollS_n", 0) > 0]
np_tot = sum(r["pollS_n"] for r in C1)
np_hit = sum(r["pollS_hitter"] for r in C1)
np_hit2 = sum(r["pollS_hitter2d"] for r in C1)
print(f"\nC1 — polluteři na ZAČÁTKU kola (rozhodovací bod): "
      f"{np_tot} polluterů v {len(C1)} kolech")
print(f"  má volného stojícího souseda (blok ZDARMA möglich): "
      f"{np_hit}/{np_tot} = {np_hit/np_tot:.1%}")
print(f"  … z toho s ≥2 kostkami:                            "
      f"{np_hit2}/{np_tot} = {np_hit2/np_tot:.1%}")
per_turn1 = np.mean([1.0 if r["pollS_hitter"] > 0 else 0.0 for r in C1])
print(f"  kol s ≥1 polluterem, kde jde aspoň jeden očistit zdarma: "
      f"{per_turn1:.1%} (n={len(C1)})")
fb = sum(r.get("pollS_freeblocked", 0) for r in C1)
bz = sum(r.get("pollS_blitzed", 0) for r in C1)
print(f"  SKUTEČNĚ blokováno zdarma týž kolo: {fb}/{np_tot} = "
      f"{fb/np_tot:.1%}; blitzem: {bz}/{len(C1)} kol = {bz/len(C1):.1%}")

# C1 po rasách
print("\n  po soupeřích (podíl polluterů s volným sousedem / s ≥2k):")
for race in RACES:
    Rr = [r for r in C1 if r["opp"] == race]
    t = sum(r["pollS_n"] for r in Rr)
    h = sum(r["pollS_hitter"] for r in Rr)
    h2 = sum(r["pollS_hitter2d"] for r in Rr)
    if t:
        print(f"    {race:<10} {h/t:.1%} / {h2/t:.1%}   (n={t} polluterů)")

# C2: na co padl blitz a co to stálo/vyneslo
print("\nC2 — použití blitzu v kolech s klecí a ≥1 polluterem na začátku:")
C2 = [r for r in rows if r.get("pollS_n", 0) > 0]
from collections import Counter
cnt = Counter(r["blitz_cls"] for r in C2)
print(f"  rozdělení blitzu: {dict(cnt)}")
print(f"\n  {'blitz na':<14}{'n':>6}{'Δx týž kolo':>13}{'Δx N+1':>9}"
      f"{'špinavé N+1':>13}{'čisté N+1':>11}{'ztráta míče':>13}")
for cls in ("corner", "carrier_mark", "wall_fwd", "other", "none"):
    g = [r for r in C2 if r["blitz_cls"] == cls]
    if len(g) < 30:
        continue
    def m(k):
        v = [r[k] for r in g if r.get(k) is not None]
        return f"{np.mean(v):+.2f}" if v else "N/A"
    def mp(k):
        v = [r[k] for r in g if r.get(k) is not None]
        return f"{np.mean(v):.1%}" if v else "N/A"
    print(f"  {cls:<14}{len(g):>6}{m('dx_N'):>13}{m('dx_N1'):>9}"
          f"{m('dirty_N1'):>13}{m('clean_N1'):>11}{mp('ball_lost'):>13}")

# C2 s kontrolou: parciální efekt blitz==corner vs blitz==wall_fwd
print("\n  kontrolované srovnání corner vs wall_fwd "
      "(| opp3_S, pollS_n, kolo):")
for out, lbl in (("dx_N", "Δx týž kolo"), ("dx_N1", "Δx N+1"),
                 ("dirty_N1", "špinavé N+1"), ("ball_lost", "ztráta míče")):
    g = sel([r for r in C2 if r["blitz_cls"] in ("corner", "wall_fwd")],
            out, "opp3_S", "pollS_n", "turn")
    if len(g) < 100:
        print(f"    {lbl}: nedost. n ({len(g)})")
        continue
    xz = np.array([1.0 if r["blitz_cls"] == "corner" else 0.0 for r in g])
    y, o3, pn, tt = cols(g, out, "opp3_S", "pollS_n", "turn")
    print(fmt_r(partial(xz, y, [o3, pn, tt]),
                f"  blitz=corner (vs wall) → {lbl}"))

# C3: idle těla a jejich dosah na pollutery
C3 = [r for r in rows if r.get("idle_reach") is not None]
tot_idle = sum(r["idle_n"] for r in C3)
tot_reach = sum(r["idle_reach"] for r in C3)
print(f"\nC3 — kola s polluterem a ≥1 idle tělem (K31): n={len(C3)}")
allpoll = [r for r in rows if r.get("pollS_n", 0) > 0]
print(f"  podíl kol s polluterem, kde vůbec je idle tělo: "
      f"{len(C3)}/{len(allpoll)} = {len(C3)/len(allpoll):.1%}")
if tot_idle:
    print(f"  idle těl v těch kolech: {tot_idle} "
          f"({tot_idle/len(C3):.2f}/kolo), z toho DOSÁHNE na pollutera "
          f"(MA+2): {tot_reach} = {tot_reach/tot_idle:.1%}")
    r1 = np.mean([1.0 if r["idle_reach"] > 0 else 0.0 for r in C3])
    print(f"  kol, kde aspoň jedno idle tělo dosáhne: {r1:.1%}")

# C4: kdy roh, kdy zeď — heuristika podle vzdálenosti od endzone?
print("\nC4 — Δx N+1 po blitzi corner/wall podle fáze (kolo v půli):")
for cls in ("corner", "wall_fwd"):
    for tlo, thi, lbl in ((1, 3, "kola 1–3"), (4, 6, "kola 4–6"),
                          (7, 8, "kola 7–8")):
        g = [r["dx_N1"] for r in C2 if r["blitz_cls"] == cls
             and tlo <= r["turn"] <= thi and r.get("dx_N1") is not None]
        mark = " ⚠ málo vzorků" if len(g) < 30 else ""
        if g:
            print(f"  {cls:<12} {lbl}: Δx_N+1 = {np.mean(g):+.2f} "
                  f"(n={len(g)}){mark}")
