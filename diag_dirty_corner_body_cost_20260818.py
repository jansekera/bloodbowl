"""Co stojí tělo, které stálo ve ŠPINAVÉM rohu? (18.08., přeměření)

Uživatel 18.08.: „to máš měřitelné, že?"
Fable 14.08. dal 94,3 % nedostupných / 13,8 % znovu čistým rohem -- ale na
korpusu o TŘI commity enginu starším (P13, hand-off, darované TD).
Tady se to opakuje na `corpus_baseline_20260817` a přidává se KONTROLNÍ
SKUPINA: táž otázka pro těla z ČISTÝCH rohů. Bez ní se nedá odlišit
„je to vlastnost špinavého rohu" od „je to vlastnost rohu vůbec".

Nedostupné = leží/omráčený, NEBO stojí, ale sousedí se stojícím soupeřem
(zamčené, K36). „Volné" v tomhle projektu znamená, že tělo může jednat.
"""
import sys, glob
from collections import Counter
sys.path.insert(0, '/home/jan/claude/bloodbowl')
from diag_rules_checks_20260812 import load, players, threatens, adj, STANDING

DATA = sys.argv[1] if len(sys.argv) > 1 else \
    '/home/jan/claude/bloodbowl/corpus_baseline_20260817_data'

R = {"špinavý": Counter(), "čistý": Counter()}

def corners_of(car):
    c = (car["x"], car["y"])
    return [(c[0]+dx, c[1]+dy) for dx in (-1, 1) for dy in (-1, 1)]

for path in sorted(glob.glob(DATA + '/*.json.gz')):
    r = load(path)
    ours = "home" if r.get("home_race") == "dwarf" else "away"
    them = "away" if ours == "home" else "home"
    logs = r["turn_logs"]
    our_idx = [i for i in range(len(logs)) if logs[i]["active_team"] == ours]
    for a, b in zip(our_idx, our_idx[1:]):
        if logs[a]["half"] != logs[b]["half"]:
            continue
        if a + 1 >= len(logs) or logs[a+1]["half"] != logs[a]["half"]:
            continue
        E = logs[a+1]                       # konec našeho kola N
        us = players(E, ours); th = players(E, them)
        car = next((p for p in us if p["has_ball"]), None)
        if car is None:
            continue
        for sq in corners_of(car):
            occ = next((p for p in us if (p["x"], p["y"]) == sq
                        and p["state"] == STANDING), None)
            if occ is None:
                continue
            dirty = any(threatens(o) and adj((o["x"], o["y"]), sq) for o in th)
            kind = "špinavý" if dirty else "čistý"
            C = R[kind]; C["těl"] += 1
            # --- kolo N+1: co se s tím tělem stalo (konec našeho příštího kola)
            if b + 1 >= len(logs) or logs[b+1]["half"] != logs[b]["half"]:
                C["nelze dohledat"] += 1; continue
            E2 = logs[b+1]
            us2 = players(E2, ours); th2 = players(E2, them)
            p2 = next((p for p in us2 if p["id"] == occ["id"]), None)
            if p2 is None:
                C["mimo hřiště"] += 1; C["NEDOSTUPNÉ"] += 1; continue
            if p2["state"] != STANDING:
                C["na zemi"] += 1; C["NEDOSTUPNÉ"] += 1; continue
            locked = any(threatens(o) and adj((o["x"], o["y"]), (p2["x"], p2["y"]))
                         for o in th2)
            if locked:
                C["zamčené (v TZ)"] += 1; C["NEDOSTUPNÉ"] += 1
            else:
                C["VOLNÉ"] += 1
            # slouží znovu jako ČISTÝ roh?
            car2 = next((p for p in us2 if p["has_ball"]), None)
            if car2 is not None and (p2["x"], p2["y"]) in corners_of(car2):
                d2 = any(threatens(o) and adj((o["x"], o["y"]), (p2["x"], p2["y"]))
                         for o in th2)
                C["znovu rohem"] += 1
                if not d2:
                    C["znovu ČISTÝM rohem"] += 1

print(f"korpus: {DATA.rsplit('/',1)[-1]}   her: {len(glob.glob(DATA+'/*.json.gz'))}")
print("tělo stojící v rohu klece na konci našeho kola N → jeho stav na konci kola N+1\n")
print(f"{'':24}{'ŠPINAVÝ roh':>14}{'ČISTÝ roh':>14}")
def row(lbl, k, denom="těl"):
    a, b = R["špinavý"], R["čistý"]
    fa = 100*a[k]/max(1, a[denom]); fb = 100*b[k]/max(1, b[denom])
    print(f"{lbl:24}{a[k]:7d} {fa:5.1f}%{b[k]:7d} {fb:5.1f}%")
print(f"{'těl celkem':24}{R['špinavý']['těl']:13d}{R['čistý']['těl']:14d}")
for k in ("NEDOSTUPNÉ", "na zemi", "zamčené (v TZ)", "mimo hřiště", "VOLNÉ",
          "znovu rohem", "znovu ČISTÝM rohem", "nelze dohledat"):
    row(k, k)
