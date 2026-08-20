#!/usr/bin/env python3
"""Najdi odehranou situaci pro spor L vs sloupce (20.08.2026).

Hledá obranná kola, kde (a) držíme KONTAKT (≥2 těla v base), (b) soupeř
bloknul naše kontaktní tělo a nosič pak postoupil ≥4 pole (protitvrzení
webu v akci), NEBO (c) kontakt nosiče zamkl (postup ≤0 a nosič v naší TZ).
Vypíše hru/kolo a vykreslí desku.
"""
import sys, glob
from collections import Counter
sys.path.insert(0, "/home/jan/claude/bloodbowl")
import diag_rules_checks_20260812 as R

STANDING = 0


def adjc(ax, ay, bx, by):
    return max(abs(ax - bx), abs(ay - by)) == 1


def render(S, ours, theirs, car):
    grid = [["." for _ in range(26)] for _ in range(15)]
    for p in R.players(S, ours):
        c = "D" if p["state"] == STANDING else "d"
        grid[p["y"]][p["x"]] = c
    for p in R.players(S, theirs):
        c = "O" if p["state"] == STANDING else "o"
        grid[p["y"]][p["x"]] = c
    grid[car["y"]][car["x"]] = "*"
    out = ["    " + "".join(str(x % 10) for x in range(26))]
    for y in range(15):
        out.append("  %2d%s" % (y, "".join(grid[y])))
    return "\n".join(out)


def main():
    paths = sorted(glob.glob(sys.argv[1]))
    hits = []
    for path in paths:
        r = R.load(path)
        if r["home_race"] == "dwarf":
            ours, theirs = "home", "away"
        elif r["away_race"] == "dwarf":
            ours, theirs = "away", "home"
        else:
            continue
        oppfwd = -1 if ours == "home" else 1
        logs = r["turn_logs"]
        for i, S in enumerate(logs):
            if S["active_team"] != theirs or i + 1 >= len(logs):
                continue
            E = logs[i + 1]
            if E["half"] != S["half"] or S.get("touchdown"):
                continue
            their_all = R.players(S, theirs)
            car = next((p for p in their_all if p["has_ball"]), None)
            if car is None or car["state"] != STANDING:
                continue
            our_stand = [p for p in R.players(S, ours) if p["state"] == STANDING]
            their_stand = [p for p in their_all if p["state"] == STANDING]
            based = [q for q in our_stand
                     if any(adjc(q["x"], q["y"], t["x"], t["y"]) for t in their_stand)]
            if len(based) < 2:
                continue
            based_ids = {q["id"] for q in based}
            carE = next((p for p in R.players(E, theirs) if p["has_ball"]), None)
            if carE is None:
                continue
            dx = (E["ball_x"] - S["ball_x"]) * oppfwd
            blocked = [e for e in S["events"]
                       if e["type"] == "BLOCK" and e["target_id"] in based_ids]
            markers = [q for q in our_stand if adjc(q["x"], q["y"], car["x"], car["y"])]
            if blocked and dx >= 4 and markers:
                hits.append((path, i, dx, len(based), len(markers),
                             [e["target_id"] for e in blocked]))
    hits.sort(key=lambda h: -h[2])
    for h in hits[:12]:
        print(h)
    if hits and len(sys.argv) > 2:
        path, i, dx, nb, nm, tg = hits[int(sys.argv[2])]
        r = R.load(path)
        ours, theirs = (("home", "away") if r["home_race"] == "dwarf"
                        else ("away", "home"))
        logs = r["turn_logs"]
        S, E = logs[i], logs[i + 1]
        car = next(p for p in R.players(S, theirs) if p["has_ball"])
        print("\n%s  kolo idx %d  half %d  turn %s  rasa soupeře %s  my=%s (endzona x=%d)"
              % (path, i, S["half"], S["turn"], r[f"{theirs}_race"], ours,
                 0 if ours == "home" else 25))
        print("nosič id %d %s na (%d,%d), postup míče %d" % (car["id"], car["name"], car["x"], car["y"], dx))
        print("\nSNÍMEK PŘED soupeřovým kolem (D=náš stojící, d=náš ležící, O/o=soupeř, *=nosič):")
        print(render(S, ours, theirs, car))
        print("\nUDÁLOSTI soupeřova kola:")
        for e in S["events"]:
            print("  %(type)-12s hráč %(player_id)2d cíl %(target_id)2d (%(from_x)d,%(from_y)d)->(%(to_x)d,%(to_y)d) ok=%(success)s" % e)
        carE = next(p for p in R.players(E, theirs) if p["has_ball"])
        print("\nPO KOLE: nosič na (%d,%d)" % (carE["x"], carE["y"]))
        print(render(E, ours, theirs, carE))


if __name__ == "__main__":
    main()
