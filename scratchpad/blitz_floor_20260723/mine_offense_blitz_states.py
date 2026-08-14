"""Mine the 24-game 21.07 replay corpus for REAL offensive turn-start states
where a BLITZ is plausibly the objectively best macro, to feed the C++
diag harness (diag_blitz_offense_floor_harness.cpp).

Selection criteria (turn-start snapshot):
  - active team holds the ball (on offense), carrier STANDING
  - carrier is MARKED: >=1 standing enemy adjacent to carrier
  - a FREE friendly blitzer exists: standing, not the carrier, not itself
    adjacent to any standing enemy (so a plain BLOCK is unavailable to it),
    within blitz range (chebyshev dist to the marker <= 6, MA proxy)
  - blitzer would get at least parity dice vs the marker (can't check ST from
    the log; require >=1 friendly assist adjacent to the marker OR accept and
    let the C++ one-ply Q arbitrate)
  - turnsRemaining >= 3 (turn <= 6) so SCORE-family urgency floors are quiet
  - score diff in [-1, +1] (avoid trailing2plus SCORE floor / big-lead caps)

Output: C++-pasteable snapshot blocks, most-promising first (ranked by
marker count on carrier + assist availability), written to
candidate_states.txt.
"""
import gzip, json, glob, os

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = "/home/jan/claude/bloodbowl"

def cheb(a, b):
    return max(abs(a[0]-b[0]), abs(a[1]-b[1]))

cands = []
for fp in sorted(glob.glob(os.path.join(REPO, "diag_replay_mine_20260721_data/g*.json.gz"))):
    rec = json.load(gzip.open(fp, "rt"))
    gname = os.path.basename(fp)
    for ti, t in enumerate(rec["turn_logs"]):
        if not t["ball_held"] or t["ball_carrier_id"] <= 0:
            continue
        active = t["active_team"]
        home, away = t["home_players"], t["away_players"]
        act = home if active == "home" else away
        opp = away if active == "home" else home
        car = next((p for p in act if p["id"] == t["ball_carrier_id"]), None)
        if car is None or car["state"] != 0:
            continue  # not on offense / carrier down
        my_score = t["home_score"] if active == "home" else t["away_score"]
        opp_score = t["away_score"] if active == "home" else t["home_score"]
        sd = my_score - opp_score
        if not (-1 <= sd <= 1):
            continue
        if t["turn"] > 6:
            continue
        cpos = (car["x"], car["y"])
        markers = [p for p in opp if p["state"] == 0 and cheb((p["x"], p["y"]), cpos) == 1]
        if not markers:
            continue
        standing_enemies = [p for p in opp if p["state"] == 0]
        # free blitzers: standing, not carrier, adjacent to NO standing enemy
        free_blitzers = []
        for p in act:
            if p["id"] == car["id"] or p["state"] != 0:
                continue
            if any(cheb((p["x"], p["y"]), (e["x"], e["y"])) == 1 for e in standing_enemies):
                continue
            free_blitzers.append(p)
        # blitzer in range of a marker, with >=1 other friendly assisting the marker
        best = None
        for m in markers:
            mpos = (m["x"], m["y"])
            assists = [p for p in act if p["id"] != car["id"] and p["state"] == 0
                       and cheb((p["x"], p["y"]), mpos) == 1]
            reach = [b for b in free_blitzers if cheb((b["x"], b["y"]), mpos) <= 6]
            if reach:
                score = len(assists) * 10 + len(reach) + len(markers)  # rank
                if best is None or score > best[0]:
                    best = (score, m, reach, assists)
        if best is None:
            continue
        cands.append({
            "rank": best[0], "game": gname, "turn_idx": ti,
            "races": (rec["home_race"], rec["away_race"]),
            "half": t["half"], "turn": t["turn"], "active": active,
            "scores": (t["home_score"], t["away_score"]),
            "carrier": car["id"], "cpos": cpos,
            "marker": best[1]["id"], "n_markers": len(markers),
            "blitzers": [b["id"] for b in best[2]],
            "assists": [a["id"] for a in best[3]],
            "home": home, "away": away,
            "ball": (t["ball_x"], t["ball_y"]),
        })

cands.sort(key=lambda c: -c["rank"])
print(f"total candidates: {len(cands)}")
out = []
for c in cands[:8]:
    hdr = (f"// {c['game']} turns[{c['turn_idx']}]  H{c['half']} T{c['turn']} "
           f"active={c['active']} score {c['scores'][0]}-{c['scores'][1]} "
           f"races={c['races'][0]}/{c['races'][1]}\n"
           f"// carrier id{c['carrier']}@{c['cpos']} marked by id{c['marker']} "
           f"(n_markers={c['n_markers']}) assists={c['assists']} free_blitzers={c['blitzers']} "
           f"ball@{c['ball']} rank={c['rank']}")
    rows = []
    for p in c["home"] + c["away"]:
        rows.append(f"{{{p['id']},{p['x']},{p['y']},{p['state']}}}")
    present = {p["id"] for p in c["home"] + c["away"]}
    missing = sorted(set(range(1, 23)) - present)
    out.append(hdr + "\n// missing (KO/CAS/off): " + str(missing) + "\n"
               + "{" + ",".join(rows) + "}\n")
    print(hdr)

with open(os.path.join(HERE, "candidate_states.txt"), "w") as f:
    f.write("\n".join(out))
print("\nwrote", os.path.join(HERE, "candidate_states.txt"))
