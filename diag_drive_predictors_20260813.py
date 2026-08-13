"""Předpovídá některá z kontrol výsledek DRIVU? (13.08.)

Brána zlepšila K9a/K29/K34/K31 a chess se nehnul -- takže se ptáme, jestli
ty kontroly vůbec souvisí s tím, jak drive dopadne. Drive se dělí stejně
jako v diag_drive_failure: hranice = start hry, změna půle, log po TD.
"""
import sys, glob, math, gzip, json
sys.path.insert(0, '/home/jan/claude/bloodbowl')
from collections import defaultdict
from diag_rules_checks_20260812 import (load, players, threatens, adj, STANDING,
                                        key, TACKLE, dodge_cost, DODGE_COST_THRESHOLD)
from diag_exposure_scan_20260812 import Board, predictors

DATA = sys.argv[1] if len(sys.argv) > 1 else \
    '/home/jan/claude/bloodbowl/diag_replay_mine_20260811b_data'
rows = []
for path in sorted(glob.glob(DATA + '/*.json.gz')):
    r = load(path)
    nm = " ".join(p["name"] for p in r["turn_logs"][0]["home_players"][:3])
    ours = "home" if ("Longbeard" in nm or "Troll Slayer" in nm) else "away"
    fwd = 1 if ours == "home" else -1
    endzone = 25 if fwd == 1 else 0
    logs = r["turn_logs"]
    # hranice drivů
    starts = [0]
    for i in range(1, len(logs)):
        if logs[i]["half"] != logs[i-1]["half"] or logs[i-1].get("touchdown"):
            starts.append(i)
    starts.append(len(logs))
    for si in range(len(starts) - 1):
        a, b = starts[si], starts[si+1]
        seg = logs[a:b]
        ours_turns = [i for i in range(a, b) if logs[i]["active_team"] == ours]
        if len(ours_turns) < 7:   # jen PLNÉ drivy (uživatel 13.08.)
            continue
        scored = any(logs[i].get("touchdown") and logs[i]["active_team"] == ours
                     for i in range(a, b))
        # sesbírej kontroly přes naše kola v drivu
        acc = defaultdict(list)
        for i in ours_turns:
            if i + 1 >= len(logs) or logs[i+1]["half"] != logs[i]["half"]:
                continue
            E = logs[i+1]
            us = players(E, ours); them = players(E, "away" if ours=="home" else "home")
            car = next((p for p in us if p["has_ball"]), None)
            blocks = sum(1 for e in logs[i]["events"] if e["type"] == "BLOCK"
                         and any(p["id"] == e["player_id"] for p in us))
            acc["K33_blok"].append(1.0 if blocks else 0.0)
            acc["blokůdo"].append(float(blocks))
            if car is None:
                continue
            diag = [(car["x"]+dx, car["y"]+dy) for dx in (-1,1) for dy in (-1,1)]
            occ = {(p["x"],p["y"]): p for p in us}
            filled = [d for d in diag if d in occ]
            threat = [(p["x"],p["y"]) for p in them if threatens(p)]
            dirty = [d for d in filled if any(adj(d,t) for t in threat)]
            acc["rohů_všech"].append(float(len(filled)))
            # Uživatelovo pravidlo od 04.08.: roh sousedící s nepřítelem =
            # klec bez rohu. Do dneška se nikdy neměřilo. "Počet rohů" tedy
            # míchá dvě různé věci; správná veličina je počet ČISTÝCH rohů.
            acc["rohů_ČISTÝCH"].append(float(len(filled) - len(dirty)))
            # Špinavý roh není jen "chybějící roh": stojí tam NAŠE tělo, které
            # nebije a nekryje. Proto se měří zvlášť od počtu čistých.
            acc["rohů_ŠPINAVÝCH"].append(float(len(dirty)))
            if filled:
                acc["K29_čisté"].append(0.0 if dirty else 1.0)
            P = predictors(Board(E, ours))
            if "REACH0" in P:
                acc["K34_reach0"].append(1.0 if P["REACH0"] == 0 else 0.0)
                acc["REACH0_počet"].append(float(P["REACH0"]))
            acc["K35_fb2"].append(1.0 if P["FB2"] <= 1 else 0.0)
            turns_left = 9 - logs[i]["turn"]
            carS = next((p for p in players(logs[i], ours) if p["has_ball"]), None)
            if turns_left > 0 and carS and carS["id"] == car["id"]:
                need = math.ceil(abs(endzone - carS["x"]) / turns_left)
                got = (car["x"] - carS["x"]) * fwd
                acc["K9a_splněno"].append(1.0 if got >= need else 0.0)
                acc["Δx"].append(float(got))
        if acc:
            rows.append((scored, {k: sum(v)/len(v) for k, v in acc.items() if v}))

print(f"drivů: {len(rows)}, z toho se skórováním: {sum(1 for s,_ in rows if s)}\n")
keys = sorted({k for _, d in rows for k in d})
print(f"{'veličina':<16}{'TD drivy':>11}{'bez TD':>11}{'rozdíl':>10}{'σ':>8}{'n':>7}")
for k in keys:
    yes = [d[k] for s, d in rows if s and k in d]
    no  = [d[k] for s, d in rows if not s and k in d]
    if len(yes) < 5 or len(no) < 5:
        continue
    my, mn = sum(yes)/len(yes), sum(no)/len(no)
    vy = sum((x-my)**2 for x in yes)/max(1,len(yes)-1)
    vn = sum((x-mn)**2 for x in no)/max(1,len(no)-1)
    se = math.sqrt(vy/len(yes) + vn/len(no))
    sig = (my-mn)/se if se else 0
    print(f"{k:<16}{my:>11.3f}{mn:>11.3f}{my-mn:>+10.3f}{sig:>7.1f}σ{len(yes)+len(no):>7}")
