"""Kolik posil DOJDE k poli nosiče, když ho srazíme? (18.08., verze 2)

⛔ VERZE 1 MĚŘILA ŠPATNOU VĚC. Počítala těla, která u nosiče už STOJÍ.
Uživatel 18.08.: „ne nutně přivést posilu hned — když jsou v dosahu, můžou
naopak až po shození nosiče přijít, jeden vzít míč, ostatní dělat screen,
když už ne klec."
⇒ Pořadí je: BLITZ na nosiče → míč se rozptýlí → teprve pak přijdou ostatní.
Takže se neptáme na vzdálenost, ale na DOSAH: kolik našich volných těl se
dostane k poli nosiče v TOMTÉŽ kole, po úderu.

Dosah = Chebyshev vzdálenost k nejbližšímu poli sousedícímu s polem nosiče
<= MA (bez GFI). Druhý sloupec s +2 GFI jako horní mez.
Nezapočítává dodge z TZ ani obsazená pole -- je to HORNÍ ODHAD dosahu
a je tak označený.
"""
import sys, glob
from collections import Counter
sys.path.insert(0, '/home/jan/claude/bloodbowl')
from diag_rules_checks_20260812 import load, players, threatens, adj, STANDING

DATA = sys.argv[1] if len(sys.argv) > 1 else \
    '/home/jan/claude/bloodbowl/corpus_baseline_20260817_data'

def cheb(a, b): return max(abs(a[0]-b[0]), abs(a[1]-b[1]))

st = Counter(); reach = Counter(); hist = Counter(); histG = Counter(); edge = Counter()
tot = Counter()

for path in sorted(glob.glob(DATA + '/*.json.gz')):
    r = load(path)
    ours = "home" if r.get("home_race") == "dwarf" else "away"
    them = "away" if ours == "home" else "home"
    for S in r["turn_logs"]:
        if S["active_team"] != ours:
            continue
        us = players(S, ours); th = players(S, them)
        car = next((p for p in th if p["has_ball"]), None)
        if car is None:
            continue
        st["kol se soupeřovým míčem"] += 1
        c = (car["x"], car["y"])
        us_st = [p for p in us if p["state"] == STANDING]
        th_st = [p for p in th if p["state"] == STANDING and p["id"] != car["id"]]
        blockers = [p for p in us_st if cheb((p["x"], p["y"]), c) == 1]
        if not blockers:
            continue
        st["blok na nosiče k dispozici"] += 1
        bid = blockers[0]["id"]
        # volný = nesousedí se stojícím soupeřem (může jednat bez dodge)
        def free(p, opp):
            return not any(cheb((p["x"], p["y"]), (o["x"], o["y"])) == 1 for o in opp)
        def steps_to_ring(p):
            # nejbližší pole sousedící s polem nosiče
            return max(0, cheb((p["x"], p["y"]), c) - 1)
        n_ma = n_gfi = 0
        for p in us_st:
            if p["id"] == bid or not free(p, th_st):
                continue
            d = steps_to_ring(p)
            if d <= p["ma"]:
                n_ma += 1
            if d <= p["ma"] + 2:
                n_gfi += 1
        t_ma = sum(1 for p in th_st if free(p, us_st) and steps_to_ring(p) <= p["ma"])
        tot["naše MA"] += n_ma; tot["naše +GFI"] += n_gfi; tot["soupeř MA"] += t_ma
        hist[min(n_ma, 4)] += 1; histG[min(n_gfi, 4)] += 1
        edge["my >" if n_ma > t_ma else "shoda" if n_ma == t_ma else "soupeř >"] += 1

n = st["blok na nosiče k dispozici"]
print(f"korpus: {DATA.rsplit('/',1)[-1]}   příležitostí: {n}")
print("DOSAH = dojde k poli sousedícímu s nosičem v témž kole (horní odhad:\n"
      "        nepočítá dodge z TZ ani obsazená pole)\n")
print(f"  našich volných těl, co DOJDE (bez GFI)   {tot['naše MA']/n:5.2f}")
print(f"  totéž s +2 GFI (horní mez)               {tot['naše +GFI']/n:5.2f}")
print(f"  soupeřových volných, co dojde            {tot['soupeř MA']/n:5.2f}")
print("\nROZLOŽENÍ našich posil v dosahu (bez GFI)")
for k in sorted(hist):
    lbl = f"{k}" if k < 4 else "4+"
    print(f"  {lbl:>3}   {hist[k]:6d}   {100*hist[k]/n:5.1f} %")
print("\nKDO MÁ V DOSAHU VÍC (bez GFI)")
for k in ("my >", "shoda", "soupeř >"):
    print(f"  {k:>9}   {edge[k]:6d}   {100*edge[k]/n:5.1f} %")
