"""Item7 recap podklad (03.08.): atribuce DODGE eventu k BLITZ approachum.

Rekonstrukce blitzu z replay eventu: retez MOVE/DODGE/GFI hrace X zakonceny
jeho BLOCK eventem v temze tahu (plain BLOCK nema pred sebou pohyb - cil uz
je adjacent). Pro kazdy DODGE v retezu: TZ z/do + existence stejne blizke
volne alternativy s mene TZ (= co ma tiebreak fixu 3953393 chytat).
Vystup viz evidence/item7_recap_podklad_20260803.md.
"""
import gzip, json, glob

files = sorted(glob.glob("diag_replay_mine_20260730_data/g*.json.gz"))

def tzc(x, y, opp):
    return sum(1 for p in opp if p["state"] == 0 and max(abs(p["x"]-x), abs(p["y"]-y)) == 1)

def occupied(x, y, allp):
    return any(p["state"] == 0 and p["x"] == x and p["y"] == y for p in allp)

blitz_dodges, n_blitz = [], 0
for fp in files:
    rec = json.load(gzip.open(fp, "rt"))
    for ti, t in enumerate(rec["turn_logs"]):
        opp = t["away_players"] if t["active_team"] == "home" else t["home_players"]
        allp = t["home_players"] + t["away_players"]
        evs = t.get("events", [])
        for i, ev in enumerate(evs):
            if ev["type"] != "BLOCK":
                continue
            pid = ev["player_id"]
            chain, j = [], i - 1
            while j >= 0 and evs[j]["type"] in ("MOVE", "DODGE", "GFI") and evs[j]["player_id"] == pid:
                chain.append(evs[j]); j -= 1
            if not chain:
                continue
            n_blitz += 1
            chain.reverse()
            bx, by = ev["to_x"], ev["to_y"]
            for ev2 in chain:
                if ev2["type"] != "DODGE":
                    continue
                fx, fy, tx, ty = ev2["from_x"], ev2["from_y"], ev2["to_x"], ev2["to_y"]
                d_taken = max(abs(tx-bx), abs(ty-by))
                alts = []
                for dx in (-1, 0, 1):
                    for dy in (-1, 0, 1):
                        if dx == dy == 0:
                            continue
                        ax, ay = fx+dx, fy+dy
                        if not (0 <= ax <= 25 and 0 <= ay <= 14) or (ax, ay) == (tx, ty):
                            continue
                        if occupied(ax, ay, allp):
                            continue
                        if max(abs(ax-bx), abs(ay-by)) == d_taken:
                            alts.append(tzc(ax, ay, opp))
                blitz_dodges.append(dict(
                    g=fp.split("/")[-1][:5], turn=ti, pid=pid, frm=(fx, fy), to=(tx, ty),
                    tzf=tzc(fx, fy, opp), tzt=tzc(tx, ty, opp), fail=not ev2["success"],
                    d_taken=d_taken, min_alt_tz=min(alts, default=None), n_alts=len(alts)))

print(f"blitz approaches: {n_blitz}, dodges within: {len(blitz_dodges)}")
worse = [d for d in blitz_dodges if d["tzt"] > d["tzf"]]
print(f"dodge into MORE TZ: {len(worse)}")
missed = [d for d in blitz_dodges if d["min_alt_tz"] is not None and d["min_alt_tz"] < d["tzt"]]
print(f"equally-close safer square existed: {len(missed)}")
for d in blitz_dodges:
    print(d)
