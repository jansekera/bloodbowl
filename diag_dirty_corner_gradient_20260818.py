"""Je JEDEN špinavý roh už selhání, nebo se to stupňuje? (18.08.)

Uživatel 18.08.: „k těm rohům — rohy musí být čisté."
σ-tabulka téhož dne: počet ŠPINAVÝCH rohů −6,8σ, počet ČISTÝCH −0,2σ.
Zbývá tvar: zákaz („ani jeden"), nebo rozpočet („čím míň, tím líp")?

Bere se DRIVE (jen plné, >=7 našich kol -- stejný filtr jako σ-tabulka),
koš = průměrný počet špinavých rohů na kolo, hodnota = podíl drivů s TD.
"""
import sys, glob, math
from collections import defaultdict
sys.path.insert(0, '/home/jan/claude/bloodbowl')
from diag_rules_checks_20260812 import load, players, threatens, adj, STANDING
from diag_drive_failure_20260811 import td_scorer_side, build_id_map

DATA = sys.argv[1] if len(sys.argv) > 1 else \
    '/home/jan/claude/bloodbowl/corpus_baseline_20260817_data'

buckets = defaultdict(lambda: [0, 0])   # koš -> [drivů, s TD]
exact   = defaultdict(lambda: [0, 0])   # kol: přesný počet špinavých -> [kol, TD drivů]

for path in sorted(glob.glob(DATA + '/*.json.gz')):
    r = load(path)
    ours = "home" if r.get("home_race") == "dwarf" else "away"
    them = "away" if ours == "home" else "home"
    logs = r["turn_logs"]; id_map = build_id_map(r)
    starts = [0]
    for i in range(1, len(logs)):
        if logs[i]["half"] != logs[i-1]["half"] or logs[i-1].get("touchdown"):
            starts.append(i)
    starts.append(len(logs))
    for si in range(len(starts) - 1):
        a, b = starts[si], starts[si+1]
        turns = [i for i in range(a, b) if logs[i]["active_team"] == ours]
        if len(turns) < 7:
            continue
        scored = any(td_scorer_side(logs[i], id_map) == ours for i in range(a, b))
        dirty_per_turn = []
        for i in turns:
            if i + 1 >= len(logs) or logs[i+1]["half"] != logs[i]["half"]:
                continue
            E = logs[i+1]
            us = players(E, ours); th = players(E, them)
            car = next((p for p in us if p["has_ball"]), None)
            if car is None:
                continue
            c = (car["x"], car["y"])
            corners = [(c[0]+dx, c[1]+dy) for dx in (-1, 1) for dy in (-1, 1)]
            d = 0
            for sq in corners:
                occ = next((p for p in us if (p["x"], p["y"]) == sq
                            and p["state"] == STANDING), None)
                if occ is None:
                    continue
                if any(threatens(o) and adj((o["x"], o["y"]), sq) for o in th):
                    d += 1
            dirty_per_turn.append(d)
            exact[min(d, 3)][0] += 1
            exact[min(d, 3)][1] += 1 if scored else 0
        if not dirty_per_turn:
            continue
        avg = sum(dirty_per_turn) / len(dirty_per_turn)
        k = 0 if avg == 0 else 1 if avg <= 0.25 else 2 if avg <= 0.5 else 3 if avg <= 1.0 else 4
        buckets[k][0] += 1
        buckets[k][1] += 1 if scored else 0

LBL = {0: "0 (ani jeden, celý drive)", 1: "0 < prům. <= 0,25",
       2: "0,25 < prům. <= 0,50", 3: "0,50 < prům. <= 1,00", 4: "prům. > 1,00"}
print(f"korpus: {DATA.rsplit('/',1)[-1]}   jen PLNÉ drivy (>=7 našich kol)\n")
print("A) DRIVE podle PRŮMĚRNÉHO počtu špinavých rohů na kolo")
print(f"{'koš':<28}{'drivů':>8}{'s TD':>8}{'podíl TD':>11}")
base = None
for k in sorted(buckets):
    n, td = buckets[k]
    p = td / n if n else 0
    if base is None: base = p
    print(f"{LBL[k]:<28}{n:>8}{td:>8}{100*p:>10.1f} %")
print("\nB) KOLO podle PŘESNÉHO počtu špinavých rohů (podíl kol v drivu, co skončil TD)")
print(f"{'špinavých rohů':<28}{'kol':>8}{'v TD drivu':>12}{'podíl':>10}")
for k in sorted(exact):
    n, td = exact[k]
    lbl = f"{k}" if k < 3 else "3+"
    print(f"{lbl:<28}{n:>8}{td:>12}{100*td/max(1,n):>9.1f} %")
