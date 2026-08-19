#!/usr/bin/env python3
"""T0.1 KROK A — TEMPO PO FÁZÍCH (19.08.2026)

K9 dnes měří rovnoměrnou podlahu `need = ceil(vzdálenost / zbývající kola)`,
což je čistá geometrie: trestá klec za to, že jede pomalu, ačkoli klec rychleji
jet NEMŮŽE. Uživatel 18.08.: přepsat na FÁZE, každou měřit zvlášť, a
⛔ ŽÁDNÁ KONSTANTA — podlaha fáze „klec" je funkce volných těl, ne 2 pole/kolo.

⇒ Než se podlaha napíše, musí se změřit, co která fáze VYDÁ. Tenhle skript
dělá právě to: klasifikuje každé naše kolo s míčem do fáze a měří dosažené
tempo, odpor koridoru a počet volných těl.

Fáze *(pořadí rozhodování, první shoda vyhrává)*:
  VÝBĚH  — před nosičem nikdo nestojí: odpor koridoru 0 a žádný stojící
           soupeř na nosiče nedosáhne (Chebyshev > MA+2, tedy ani přes GFI)
  KLEC   — aspoň dvě naše těla na diagonálách nosiče
  SÓLO   — všechno ostatní (nosič jede sám, klec ještě nestojí)
"""
import sys, glob
from collections import defaultdict
sys.path.insert(0, "/home/jan/claude/bloodbowl")
import diag_rules_checks_20260812 as R

STANDING = 0
DIAG = [(-1, -1), (-1, 1), (1, -1), (1, 1)]


class Agg:
    def __init__(self):
        self.n = 0
        self.dx = 0.0
        self.resist = 0.0
        self.resist_n = 0
        self.free = 0.0
        self.corners = 0.0
        self.td = 0

    def add(self, dx, resist, free, corners):
        self.n += 1
        self.dx += dx
        self.free += free
        self.corners += corners
        if resist is not None and resist >= 0:
            self.resist += resist
            self.resist_n += 1


def run(paths, race="dwarf"):
    ph = defaultdict(Agg)
    bybodies = defaultdict(Agg)
    for path in paths:
        r = R.load(path)
        side_of = {}
        for s in ("home", "away"):
            nm = " ".join(p["name"] for p in r["turn_logs"][0][f"{s}_players"][:3])
            side_of[s] = "dwarf" if "Longbeard" in nm or "Troll Slayer" in nm else None
        ours = next((s for s, v in side_of.items() if v == race), None)
        if ours is None:
            continue
        theirs = "away" if ours == "home" else "home"
        fwd = 1 if ours == "home" else -1
        logs = r["turn_logs"]
        for i, S in enumerate(logs):
            if S["active_team"] != ours or i + 1 >= len(logs):
                continue
            E = logs[i + 1]
            if S.get("touchdown") or E["half"] != S["half"]:
                continue
            us_s = R.players(S, ours)
            carS = next((p for p in us_s if p["has_ball"]), None)
            if carS is None:
                continue
            us_e = R.players(E, ours)
            carE = next((p for p in us_e if p["has_ball"] and p["id"] == carS["id"]),
                        None)
            if carE is None:
                continue          # nosič se během kola změnil nebo míč ztracen
            them = [p for p in R.players(S, theirs) if p["state"] == STANDING]

            dx = (carE["x"] - carS["x"]) * fwd
            resist = S.get("corridor_resistance", -1)
            free = sum(1 for p in us_s
                       if p["id"] != carS["id"] and p["state"] == STANDING)
            occ = {(p["x"], p["y"]) for p in us_s if p["id"] != carS["id"]}
            corners = sum(1 for a, b in DIAG
                          if (carS["x"] + a, carS["y"] + b) in occ)

            reachable = any(max(abs(p["x"] - carS["x"]),
                                abs(p["y"] - carS["y"])) <= p["ma"] + 2
                            for p in them)
            if (resist is not None and resist == 0) and not reachable:
                phase = "VÝBĚH"
            elif corners >= 2:
                phase = "KLEC"
            else:
                phase = "SÓLO"

            ph[phase].add(dx, resist, free, corners)
            if phase == "KLEC":
                bybodies[min(free, 9)].add(dx, resist, free, corners)
    return ph, bybodies


def main():
    ph, bybodies = run(sorted(glob.glob(sys.argv[1])))
    tot = sum(a.n for a in ph.values())
    print("jmenovatel = %d našich kol, kde nosič drží míč na začátku i konci kola\n" % tot)
    print("%-8s %8s %7s %10s %10s %9s" % ("fáze", "kol", "podíl", "Δx/kolo",
                                          "odpor", "volných těl"))
    for k in ["SÓLO", "KLEC", "VÝBĚH"]:
        a = ph[k]
        if not a.n:
            continue
        print("%-8s %8d %6.1f %% %9.2f %10.2f %9.2f"
              % (k, a.n, 100.0 * a.n / tot, a.dx / a.n,
                 a.resist / a.resist_n if a.resist_n else float("nan"),
                 a.free / a.n))
    print("\n⭐ FÁZE KLEC — tempo jako FUNKCE volných těl (ne konstanta):")
    print("%-14s %8s %9s %10s %9s" % ("volných těl", "kol", "Δx/kolo", "odpor", "rohů"))
    for k in sorted(bybodies):
        a = bybodies[k]
        if a.n < 30:
            continue
        print("%-14d %8d %9.2f %10.2f %9.2f"
              % (k, a.n, a.dx / a.n,
                 a.resist / a.resist_n if a.resist_n else float("nan"),
                 a.corners / a.n))


if __name__ == "__main__":
    main()
