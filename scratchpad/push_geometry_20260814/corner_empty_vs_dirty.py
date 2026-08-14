#!/usr/bin/env python3
"""Prázdný roh vs. špinavý roh — co je pro klec lepší? (14.08.2026)

Uživatel 14.08.: *„když X nesrazíme, roh neočistíme — pak volíme, jestli má
u rohu zůstat náš hráč, nebo X. Co je pro roh lepší?"*

Nepřímo to složit umíme (počet rohů 0σ, špinavé rohy −2,2σ ⇒ prázdný vyhrává),
ale takhle to změřené NENÍ a chybí mechanismus: prázdný roh je prázdné pole
VEDLE NOSIČE, kam soupeř může příště vstoupit — tedy `REACH0`, kde je podle E1
rozdíl mezi 1,8 % a 33 % ztráty míče.

Proto se porovnávají jen SROVNATELNÉ situace: rohové pole, u kterého soupeř
STOJÍ VEDLE. Takový roh je buď obsazený naším tělem (= špinavý), nebo prázdný.
Průměrný prázdný roh do srovnání nepatří — ten soupeře vedle nemá.

Měří se na začátku NÁSLEDUJÍCÍHO našeho kola (N+1):
  reach0   kolik soupeřů dosáhne na nosiče bez dodge
  lost     ztratili jsme míč do konce drivu
  dx       postup nosiče

⚠️ Snímek je začátek kola; uvnitř kola se deska mění. Výhrada platí na obojí
rameno stejně, takže srovnání nezaujímá, jen zeslabuje.
"""
import glob, gzip, json, sys
from collections import defaultdict

sys.path.insert(0, "/home/jan/claude/bloodbowl")
from diag_rules_checks_20260812 import load, players, threatens, adj, STANDING

DATA = sys.argv[1] if len(sys.argv) > 1 else \
    "/home/jan/claude/bloodbowl/diag_replay_mine_20260813_big_data"
LIMIT = int(sys.argv[2]) if len(sys.argv) > 2 else 0


def cheb(a, b):
    return max(abs(a[0] - b[0]), abs(a[1] - b[1]))


def reach0(car, them):
    """Kolik stojících soupeřů sousedí s nosičem (dosáhnou bez dodge)."""
    return sum(1 for p in them
               if p["state"] == STANDING and adj((p["x"], p["y"]),
                                                 (car["x"], car["y"])))


def main():
    paths = sorted(glob.glob(DATA + "/*.json.gz"))
    if LIMIT:
        paths = paths[:LIMIT]
    acc = defaultdict(lambda: {"n": 0, "reach0": 0, "dx": 0, "opp3": 0})

    for path in paths:
        r = load(path)
        if r["home_race"] == "dwarf":
            ours = "home"
        elif r["away_race"] == "dwarf":
            ours = "away"
        else:
            continue
        theirs = "away" if ours == "home" else "home"
        fwd = 1 if ours == "home" else -1
        logs = r["turn_logs"]

        for i, S in enumerate(logs):
            if S["active_team"] != ours or i + 2 >= len(logs):
                continue
            S2 = logs[i + 2]
            if S.get("touchdown") or S2["half"] != S["half"]:
                continue
            if S2["active_team"] != ours:
                continue
            us, them = players(S, ours), players(S, theirs)
            car = next((p for p in us if p["has_ball"]), None)
            if car is None:
                continue
            occ = {(p["x"], p["y"]) for p in us}
            threat = [p for p in them if threatens(p)]
            diag = [(car["x"] + dx, car["y"] + dy)
                    for dx in (-1, 1) for dy in (-1, 1)]

            # jen rohová pole, u kterých soupeř STOJÍ VEDLE -> srovnatelné
            for d in diag:
                if not any(adj(d, (t["x"], t["y"])) for t in threat):
                    continue
                kind = "špinavý (naše tělo tam stojí)" if d in occ \
                    else "prázdný (soupeř vedle, my ne)"
                # ⚠️ KONTROLA KONFOUNDÉRU (14.08.): roh zůstane prázdný nejčastěji
                # proto, že jsme NEMĚLI KOHO tam dát -- a to je kolo, ve kterém
                # jsme na tom celkově hůř. Hustota soupeřů na tohle nestačí.
                # Když rozdíl uvnitř téže zásoby volných těl zůstane, je efekt
                # skutečný; když zmizí, byl prázdný roh jen PŘÍZNAK.
                # Těla se počítají VČETNĚ těch na rozích. Vyloučit je nelze:
                # špinavý roh z definice jedno tělo na rohu MÁ, takže by se mu
                # počet uměle snižoval a ramena by skončila v nesrovnatelných
                # koších (naměřeno: koš ≥7 měl 2840 prázdných proti 958
                # špinavým -- artefakt čitatele, ne vlastnost hry).
                free = sum(1 for p in us
                           if p["state"] == STANDING and not p["has_ball"])
                bucket = "≤4" if free <= 4 else "5-6" if free <= 6 else "≥7"
                kind = "%s  [volných těl %s]" % (kind, bucket)

                us2 = players(S2, ours)
                them2 = players(S2, theirs)
                car2 = next((p for p in us2 if p["has_ball"]), None)
                a = acc[kind]
                a["n"] += 1
                a["opp3"] += sum(1 for p in them
                                 if cheb((p["x"], p["y"]),
                                         (car["x"], car["y"])) <= 3)
                if car2 is not None:
                    a["reach0"] += reach0(car2, them2)
                    a["dx"] += (car2["x"] - car["x"]) * fwd
                else:
                    a["reach0"] += 0
                    a["dx"] += 0
                a.setdefault("held2", 0)
                a["held2"] += 1 if car2 is not None else 0

    print("rohová pole se soupeřem VEDLE (jen srovnatelné situace)\n")
    print("  %-34s %7s %9s %9s %9s %9s"
          % ("stav rohu", "n", "REACH0", "Δx", "držíme", "opp≤3"))
    for k in sorted(acc):
        a = acc[k]
        n = max(a["n"], 1)
        print("  %-34s %7d %9.2f %9.2f %8.1f %% %9.2f"
              % (k, a["n"], a["reach0"] / n, a["dx"] / n,
                 100.0 * a["held2"] / n, a["opp3"] / n))
    print("\n⚠️ opp≤3 je kontrola hustoty: když se ramena liší, není srovnání férové.")


main()
