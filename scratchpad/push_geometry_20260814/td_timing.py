#!/usr/bin/env python3
"""Kdy dáváme TD a co s tím soupeř udělá — podklad pro P11 (14.08.2026).

Otázka uživatele: „kolik jsme dali TD, vyčti napřed z výsledků."

Atribuce TD podle STŘELCE, ne podle `active_team` — viz oprava z 14.08.
(15 z 2183 TD padne v kole druhého týmu, odsunem do endzony).
"""
import glob, gzip, json, sys
from collections import Counter, defaultdict

sys.path.insert(0, "/home/jan/claude/bloodbowl")
from diag_drive_failure_20260811 import build_id_map

DATA = sys.argv[1] if len(sys.argv) > 1 else \
    "/home/jan/claude/bloodbowl/diag_replay_mine_20260813_big_data"


def scorer(tl, id_map):
    for e in tl.get("events", []):
        if str(e.get("type", "")).upper() == "TOUCHDOWN":
            ent = id_map.get(e["player_id"])
            if ent:
                return ent[0]
    return tl["active_team"] if tl.get("touchdown") else None


res = Counter()
our_turn = Counter()
answered = Counter()          # (kola zbylá soupeři) -> [odpovědí, celkem]
answer_by_left = defaultdict(lambda: [0, 0])
per_opp = defaultdict(Counter)
n_games = 0

for path in sorted(glob.glob(DATA + "/*.json.gz")):
    g = json.load(gzip.open(path, "rt"))
    if "dwarf" not in (g["home_race"], g["away_race"]):
        continue
    n_games += 1
    ours = "home" if g["home_race"] == "dwarf" else "away"
    opp = g["away_race"] if ours == "home" else g["home_race"]
    im = build_id_map(g)
    logs = g["turn_logs"]

    hs, aws = g["home_score"], g["away_score"]
    ourScore, theirScore = (hs, aws) if ours == "home" else (aws, hs)
    res["naše TD"] += ourScore
    res["jejich TD"] += theirScore
    per_opp[opp]["naše"] += ourScore
    per_opp[opp]["jejich"] += theirScore
    if ourScore > theirScore:
        res["výhry"] += 1
    elif ourScore == theirScore:
        res["remízy"] += 1
    else:
        res["prohry"] += 1
    if ourScore == 0:
        res["zápasů bez našeho TD"] += 1

    # kdy padl náš TD a kolik kol pak zbylo soupeři
    for i, tl in enumerate(logs):
        if not tl["touchdown"]:
            continue
        if scorer(tl, im) != ours:
            continue
        t, half = tl["turn"], tl["half"]
        our_turn[(half, t)] += 1
        left = 8 - t                       # kolik kol soupeři v půli zbývá
        answer_by_left[left][1] += 1
        # odpověděl soupeř do konce téže půle?
        for tl2 in logs[i + 1:]:
            if tl2["half"] != half:
                break
            if tl2["touchdown"] and scorer(tl2, im) != ours:
                answer_by_left[left][0] += 1
                break

print("her: %d\n" % n_games)
print("=== SKÓRE ===")
for k in ("naše TD", "jejich TD", "výhry", "remízy", "prohry",
          "zápasů bez našeho TD"):
    print("  %-24s %6d   (%.3f na hru)" % (k, res[k], res[k] / n_games))
print()
print("=== KDY DÁVÁME TD (kolo půle) ===")
tot = sum(our_turn.values())
by_turn = Counter()
for (half, t), v in our_turn.items():
    by_turn[t] += v
for t in sorted(by_turn):
    bar = "#" * int(40.0 * by_turn[t] / max(tot, 1))
    print("  kolo %d  %5d  %5.1f %%  %s" % (t, by_turn[t],
                                            100.0 * by_turn[t] / tot, bar))
print()
print("=== ODPOVĚDĚL SOUPEŘ DO KONCE PŮLE? (podle zbylých kol) ===")
print("  %-16s %8s %10s %10s" % ("zbylo soupeři", "našich TD", "odpovědí", "podíl"))
for left in sorted(answer_by_left):
    a, n = answer_by_left[left]
    print("  %-16d %8d %10d %9.1f %%" % (left, n, a, 100.0 * a / max(n, 1)))
print()
print("=== PO SOUPEŘÍCH ===")
for o in sorted(per_opp):
    c = per_opp[o]
    print("  %-10s naše %4d | jejich %4d" % (o, c["naše"], c["jejich"]))
