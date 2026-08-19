#!/usr/bin/env python3
"""T0.1 KROK B — K9 JAKO ZPĚTNÝ ROZVRH PO FÁZÍCH (19.08.2026)

Uživatel 19.08.: „mám rozvrh a v každém z 8 kol mám svou část, co chci,
a dopočet do konce — zkus to počítat od osmého kola zpět."

Dnešní K9a: `need = ceil(vzdálenost / zbývající kola)` — rovnoměrné dělení.
Trestá klec, protože klec vydá 1,93 pole/kolo proti 2,85 ve výběhu (krok A).

Nová K9f: rozvrh se skládá **od 8. kola zpět** a dělí se **v poměru k tomu,
co která fáze vydá**:

    M(8) = p(VÝBĚH)                     # poslední kolo je doběh do endzony
    M(t) = p(fáze v kole t) + M(t+1)    # co se dá ujet od kola t do konce
    kvóta(t) = vzdálenost(t) * p(fáze v kole t) / M(t)

⇒ „svou část, co chci" = kvóta(t); „dopočet do konce" = M(t+1).
Když vzdálenost > M(t), rozvrh je NESPLNITELNÝ — a to je jiná informace než
„jel jsi pomalu": drive je ztracený rozvrhem, ne tempem.

⛔ ŽÁDNÁ KONSTANTA: p(fáze) se čte z KORPUSU, a fáze KLEC z tabulky podle
počtu volných těl (uživatelovo pravidlo „strop je funkce volných těl").

⚠️ KRUHOVOST: tempa se odvozují na PRVNÍ půlce korpusu a K9f se vyhodnocuje
na DRUHÉ. Jinak by reference vznikla z týchž kol, která se jí měří.
"""
import sys, glob, math
from collections import defaultdict
sys.path.insert(0, "/home/jan/claude/bloodbowl")
import diag_rules_checks_20260812 as R
from diag_drive_failure_20260811 import td_scorer_side

STANDING = 0
DIAG = [(-1, -1), (-1, 1), (1, -1), (1, 1)]


def turns_of(path, race="dwarf"):
    """Vydá (fáze, volných těl, vzdálenost na začátku kola, Δx, kolo, drive_id, skóroval)."""
    r = R.load(path)
    side_of = {}
    for s in ("home", "away"):
        nm = " ".join(p["name"] for p in r["turn_logs"][0][f"{s}_players"][:3])
        side_of[s] = "dwarf" if "Longbeard" in nm or "Troll Slayer" in nm else None
    ours = next((s for s, v in side_of.items() if v == race), None)
    if ours is None:
        return
    theirs = "away" if ours == "home" else "home"
    fwd = 1 if ours == "home" else -1
    endzone = 25 if fwd == 1 else 0
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
        carE = next((p for p in R.players(E, ours)
                     if p["has_ball"] and p["id"] == carS["id"]), None)
        if carE is None:
            continue
        them = [p for p in R.players(S, theirs) if p["state"] == STANDING]
        resist = S.get("corridor_resistance", -1)
        free = sum(1 for p in us_s if p["id"] != carS["id"] and p["state"] == STANDING)
        occ = {(p["x"], p["y"]) for p in us_s if p["id"] != carS["id"]}
        corners = sum(1 for a, b in DIAG if (carS["x"] + a, carS["y"] + b) in occ)
        reachable = any(max(abs(p["x"] - carS["x"]), abs(p["y"] - carS["y"]))
                        <= p["ma"] + 2 for p in them)
        if resist is not None and resist == 0 and not reachable:
            phase = "VÝBĚH"
        elif corners >= 2:
            phase = "KLEC"
        else:
            phase = "SÓLO"
        yield (phase, free, abs(endzone - carS["x"]),
               (carE["x"] - carS["x"]) * fwd, S["turn"])


def learn_paces(paths, pct):
    """Tempo fáze jako PERCENTIL, ne průměr.

    ⛔ Průměr je špatná kapacita rozvrhu: uživatel 19.08. řekl „cíl je maximum,
    ne ten strop". S průměrem vyjde 95,8 % kol jako „nesplnitelný rozvrh",
    ačkoli 15 % drivů TD dá — protože skórující drivy jedou RYCHLEJI než průměr.
    Kapacita se proto čte jako horní percentil toho, co fáze při daném počtu
    volných těl vydává."""
    agg = defaultdict(list)
    for p in paths:
        for phase, free, dist, dx, turn in turns_of(p):
            k = (phase, min(free, 9)) if phase == "KLEC" else (phase, None)
            agg[k].append(dx)
    out = {}
    for k, v in agg.items():
        if len(v) < 30:
            continue
        v.sort()
        out[k] = v[min(len(v) - 1, int(len(v) * pct))]
    return out


def pace(paces, phase, free):
    if phase == "KLEC":
        k = ("KLEC", min(free, 9))
        if k in paces:
            return paces[k]
        return paces.get(("KLEC", 9), 1.9)
    return paces.get((phase, None), 2.3)


def main():
    paths = sorted(glob.glob(sys.argv[1]))
    half = len(paths) // 2
    pct = float(sys.argv[2]) if len(sys.argv) > 2 else 0.75
    paces = learn_paces(paths[:half], pct)
    print("kapacita fází = %d. percentil, odvozeno na PRVNÍ půlce korpusu (%d her):"
          % (int(pct * 100), half))
    for k in sorted(paces, key=lambda x: (x[0], x[1] if x[1] is not None else -1)):
        print("   %-8s %-4s  %.2f pole/kolo" % (k[0], k[1] if k[1] is not None else "", paces[k]))

    st = defaultdict(int)
    for p in paths[half:]:
        rows = list(turns_of(p))
        for phase, free, dist, dx, turn in rows:
            n_left = 9 - turn
            if n_left <= 0:
                continue
            # --- zpětný rozvrh od 8. kola ---
            M = 0.0
            for t in range(8, turn - 1, -1):
                if t == 8:
                    M = pace(paces, "VÝBĚH", free)
                else:
                    M += pace(paces, phase if t == turn else phase, free)
            quota = dist * pace(paces, phase, free) / M if M > 0 else dist
            st["kol"] += 1
            st[f"{phase}|kol"] += 1
            # ⛔ stará K9a se počítá na VŠECH kolech, i nesplnitelných -- jinak
            # se srovnávají dva různé jmenovatele (vada první verze 19.08.).
            need_old = math.ceil(dist / n_left)
            if dx >= need_old:
                st["K9a (stará) splněno"] += 1
                st[f"{phase}|stará splněno"] += 1
            if dist > M:
                st["ROZVRH NESPLNITELNÝ (vzdálenost > co se dá ujet)"] += 1
                st[f"{phase}|nesplnitelný"] += 1
                continue
            st["rozvrh splnitelný"] += 1
            if dx >= quota:
                st["K9f splněno"] += 1
                st[f"{phase}|splněno"] += 1

    n = st["kol"]
    ok = st["rozvrh splnitelný"]
    print("\nvyhodnoceno na DRUHÉ půlce: %d kol" % n)
    print("  rozvrh NESPLNITELNÝ: %d (%.1f %%) — drive je ztracený rozvrhem, ne tempem"
          % (st["ROZVRH NESPLNITELNÝ (vzdálenost > co se dá ujet)"],
             100.0 * st["ROZVRH NESPLNITELNÝ (vzdálenost > co se dá ujet)"] / n))
    print("  K9f splněno:      %6d / %6d = %5.1f %%  (jen splnitelná kola)"
          % (st["K9f splněno"], ok, 100.0 * st["K9f splněno"] / ok if ok else 0))
    print("  K9a stará splněno:%6d / %6d = %5.1f %%  (týchž kol, rovnoměrné dělení)"
          % (st["K9a (stará) splněno"], n, 100.0 * st["K9a (stará) splněno"] / n))
    print("\n  po fázích (podíl splnění, K9f vs stará K9a):")
    for ph in ["SÓLO", "KLEC", "VÝBĚH"]:
        c = st[f"{ph}|kol"]
        if not c:
            continue
        feas = c - st[f"{ph}|nesplnitelný"]
        print("   %-8s kol %6d · nesplnitelný %5.1f %% · K9f %5.1f %% · K9a %5.1f %%"
              % (ph, c, 100.0 * st[f"{ph}|nesplnitelný"] / c,
                 100.0 * st[f"{ph}|splněno"] / feas if feas else 0,
                 100.0 * st[f"{ph}|stará splněno"] / c))


if __name__ == "__main__":
    main()
