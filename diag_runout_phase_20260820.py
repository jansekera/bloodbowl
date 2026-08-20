#!/usr/bin/env python3
"""P41 — FÁZE VÝBĚH ROZPADEM PODLE ZBÝVAJÍCÍCH KOL (20.08.2026)

T0.1 změřilo, že ve fázi VÝBĚH (endzona na dosah) splníme rozvrhovou podlahu
ve 2,9 % kol a chybí nám 3,38 pole. ⚠️ Jenže podlaha je tam `min(need, MA)`,
což v POSLEDNÍM kole půle znamená „sprintuj naplno" -- takže část toho čísla
je dána konstrukcí, ne chybou.

⭐ A přibyl druhý důvod, proč to nelze číst jako selhání (uživatel 20.08.):
STALL JE SPRÁVNÁ HRA. Skórovat brzy vrací soupeři míč, takže „nedošel" a
„schválně nedošel" jsou dvě různé věci -- a rozlišit je umí jedině počet
zbývajících kol: v POSLEDNÍM kole stall nedává smysl, tam je nedojití vada.

Proto se to rozpadá podle `turns_left`.
"""
import sys, glob
sys.path.insert(0, "/home/jan/claude/bloodbowl")
import diag_rules_checks_20260812 as R
from collections import defaultdict

STANDING = 0


def main():
    rows = defaultdict(lambda: [0, 0, 0.0, 0])   # tl -> [n, splněno, součet rezervy, TD]
    for path in sorted(glob.glob(sys.argv[1])):
        r = R.load(path)
        ours = "home" if r.get("home_race") == "dwarf" else "away"
        fwd = 1 if ours == "home" else -1
        endzone = 25 if fwd == 1 else 0
        logs = r["turn_logs"]
        for i, S in enumerate(logs):
            if S["active_team"] != ours or i + 1 >= len(logs):
                continue
            E = logs[i + 1]
            if E["half"] != S["half"]:
                continue
            carS = next((p for p in R.players(S, ours) if p["has_ball"]), None)
            if carS is None or carS["state"] != STANDING:
                continue
            ma = carS.get("ma", 6)
            dist = abs(endzone - carS["x"])
            if dist > ma + 2:            # není fáze VÝBĚH
                continue
            tl = 9 - S["turn"]
            if tl <= 0:
                continue
            carE = next((p for p in R.players(E, ours)
                         if p["has_ball"] and p["id"] == carS["id"]), None)
            got = (carE["x"] - carS["x"]) * fwd if carE else dist  # TD => došel
            need = min(-(-dist // tl), ma)
            rows[tl][0] += 1
            rows[tl][1] += 1 if got >= need else 0
            rows[tl][2] += got - need
            rows[tl][3] += 1 if S.get("touchdown") else 0

    print("FÁZE VÝBĚH (endzona na dosah: dist <= MA+2), rozpad podle zbývajících kol\n")
    print(f"  {'zbývá kol':>10} {'kol':>7} {'splněno':>9} {'⌀ rezerva':>11} {'TD v kole':>11}")
    tot = [0, 0, 0.0, 0]
    for tl in sorted(rows, reverse=True):
        n, ok, res, td = rows[tl]
        for k in range(4): tot[k] += rows[tl][k]
        print(f"  {tl:>10} {n:>7} {100.0*ok/n:>8.1f}% {res/n:>11.2f} {100.0*td/n:>10.1f}%")
    n, ok, res, td = tot
    print(f"  {'CELKEM':>10} {n:>7} {100.0*ok/n:>8.1f}% {res/n:>11.2f} {100.0*td/n:>10.1f}%")
    print("\n⭐ Jak číst: v POSLEDNÍM kole (zbývá 1) stall nedává smysl —")
    print("   tam je nesplnění VADA. Ve vyšších kolech může být záměr.")


if __name__ == "__main__":
    main()
