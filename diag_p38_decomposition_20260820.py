#!/usr/bin/env python3
"""P38 DEKOMPOZICE 20.08. — kolik z ramene je klec (B) a kolik boční volnost (A)
+ obejití záložní smyčky (C)?

Python replika ramene P38 (`expandAdvance` + `cageScoreForSquare`,
macro_actions.cpp, commit 74f153f2) a PLACEBA (totéž bez kritéria klece),
puštěná nad korpusem `corpus_baseline_20260819_data` (3 000 her).

Klíčová otázka (bod 3 zadání): v kolech, kde se nosič nehnul a neměl ŽÁDNOU
událost (P39, u trpaslíka 5 213 kol), existovalo by pole, které by
(1) našlo PLACEBO — prog>=1, prog>=maxProgress-1, TZ-free, neobsazené;
(2) našlo P38 — totéž A NAVÍC z pole vyjde plná čistá klec.
Rozdíl (1)-(2) = kolik z odblokování nosiče si NEzaslouží klec.

Doplnění uživatele 20.08. (rozpočet je rasový; oprava druhého doplnění:
ZEĎ patří do OBRANY, do útočného rozpočtu kola se nepočítá): klauzule
plnitelnosti rohů se počítá DVAKRÁT:
  (a) jako rameno dnes — každé stojící tělo s dosahem (Chebyshev <= ma);
  (b) navíc s vyloučením těl s jinou ÚTOČNOU povinností:
      (1) tělo mělo v tom kole VLASTNÍ událost (rozpočet kola je pryč);
      (2) tělo MARKUJE a jeho odchod je DRAHÝ dodge — max dodge_cost přes
          sousedící stojící soupeře > DODGE_COST_THRESHOLD (0,20).
          Levné markování je dle K30b povinnost bez ceny a roh nepřebíjí.
Rozdíl (a)-(b) = odhad nadhodnocení stropu. Stropy se tisknou ODDĚLENĚ pro
trpaslíka a wood-elfa (pravidlo je univerzální, rozpočet ne).

⚠️ Předpoklady repliky (přiznané, ne skryté):
  * movementRemaining = ma (nosič v idle kolech nejednal, takže pro NĚJ to
    sedí přesně; pro výplňová těla je to horní odhad — proto varianta (b));
  * pořadí průchodu kandidátů i greedy výplně kopíruje C++ (ox, oy od
    -budget, striktní >, roster pořadí), takže tie-break sedí;
  * S['turn'] = turnNumber týmu v poločase (1..8), ověřeno na korpusu.

Každý podíl se tiskne s vypsaným jmenovatelem (lekce 13.08./18.08.).
"""
import sys, glob
from collections import Counter
sys.path.insert(0, "/home/jan/claude/bloodbowl")
import diag_rules_checks_20260812 as R

STANDING = 0
DIAG = ((-1, -1), (-1, 1), (1, -1), (1, 1))   # DX/DY pořadí z C++
ORTH = ((-1, 0), (1, 0), (0, -1), (0, 1))


def on_pitch(x, y):
    return 0 <= x <= 25 and 0 <= y <= 14


def cheb(ax, ay, bx, by):
    return max(abs(ax - bx), abs(ay - by))


def stall_aware_steps(car, opp_standing, turn, ez):
    dist = abs(car["x"] - ez)
    tr = max(1, 9 - turn)
    ideal = max(1, (dist + tr - 1) // tr)
    mv = car["ma"]                      # nosič nejednal -> plná MA
    blitzable = any(cheb(o["x"], o["y"], car["x"], car["y"]) <= o["ma"]
                    for o in opp_standing)
    if tr <= 2 or blitzable:
        return min(ideal, mv)
    return min(ideal, max(1, mv // 2))


def cage_ok(cand, car, occupied_wo_car, opp_all_pos, opp_standing, fillers):
    """Replika cageScoreForSquare. fillers = [(x, y, ma, id)] v roster pořadí."""
    cx, cy = cand
    # (3) žádný jiný soused nosiče na ortogonálách
    for ox, oy in ORTH:
        nx, ny = cx + ox, cy + oy
        if on_pitch(nx, ny) and (nx, ny) in occupied_wo_car:
            return False
    corners = []
    for dx, dy in DIAG:
        kx, ky = cx + dx, cy + dy
        if not on_pitch(kx, ky):
            continue                    # lajna: roh neexistuje
        if (kx, ky) in opp_all_pos:     # roh drží soupeř (i lehlý)
            return False
        # (2) čistý: žádný STOJÍCÍ soupeř do vzdálenosti 1
        for o in opp_standing:
            if cheb(o["x"], o["y"], kx, ky) <= 1:
                return False
        corners.append((kx, ky))
    if len(corners) < 4:
        return False
    # (1) greedy: 4 těla, co na rohy dosáhnou (Chebyshev <= mv), roster pořadí
    used = set()
    for kx, ky in corners:
        best_id, best_d = -1, 999
        for fx, fy, fma, fid in fillers:
            if fid in used:
                continue
            d = cheb(fx, fy, kx, ky)
            if d > fma or d >= best_d:
                continue
            best_d, best_id = d, fid
        if best_id < 0:
            return False
        used.add(best_id)
    return True


def scan(car, budget, ez_dir, occupied, occupied_wo_car,
         opp_all_pos, opp_standing, fillers_a, fillers_b):
    """Vrátí (maxProgress, pick_placebo, pick_p38a, pick_p38b) — pick je
    (x, y) nebo None. Pořadí průchodu a tie-break přesně jako C++."""
    cx0, cy0 = car["x"], car["y"]
    max_prog = 0
    cands = []
    for ox in range(-budget, budget + 1):
        for oy in range(-budget, budget + 1):
            x, y = cx0 + ox, cy0 + oy
            if not on_pitch(x, y) or (x, y) in occupied:
                continue
            prog = ez_dir * (x - cx0)
            if prog > max_prog:
                max_prog = prog
            cands.append((x, y, prog))
    picks = [None, None, None]          # placebo, p38a, p38b
    bests = [0, 0, 0]
    for x, y, prog in cands:            # týž průchod jako výše (ox vnější)
        if prog < 1 or prog < max_prog - 1:
            continue
        # TZ filtr: stojící soupeř do vzdálenosti 1
        if any(cheb(o["x"], o["y"], x, y) <= 1 for o in opp_standing):
            continue
        variants_ok = [True, None, None]
        for vi, fillers in ((1, fillers_a), (2, fillers_b)):
            variants_ok[vi] = cage_ok((x, y), car, occupied_wo_car,
                                      opp_all_pos, opp_standing, fillers)
        for vi in range(3):
            if variants_ok[vi] and prog > bests[vi]:
                bests[vi] = prog
                picks[vi] = (x, y)
    return max_prog, picks[0], picks[1], picks[2]


def main():
    paths = sorted(glob.glob(sys.argv[1] if len(sys.argv) > 1
                             else "corpus_baseline_20260819_data/*.json.gz"))
    st = {"dwarf": Counter(), "wood-elf": Counter()}
    for gi, path in enumerate(paths):
        r = R.load(path)
        names0 = {s: " ".join(p["name"] for p in r["turn_logs"][0][f"{s}_players"][:3])
                  for s in ("home", "away")}
        dwarf_side = next((s for s in ("home", "away")
                           if "Longbeard" in names0[s] or "Troll Slayer" in names0[s]),
                          None)
        if dwarf_side is None:
            continue
        logs = r["turn_logs"]
        for ours, race in ((dwarf_side, "dwarf"),
                           ("away" if dwarf_side == "home" else "home", "wood-elf")):
            theirs = "away" if ours == "home" else "home"
            ez = 25 if ours == "home" else 0
            ez_dir = 1 if ours == "home" else -1
            c = st[race]
            for i, S in enumerate(logs):
                if S["active_team"] != ours or i + 1 >= len(logs):
                    continue
                E = logs[i + 1]
                if S.get("touchdown") or E["half"] != S["half"]:
                    continue
                car = next((p for p in R.players(S, ours) if p["has_ball"]), None)
                if car is None or car["state"] != STANDING:
                    continue
                carE = next((p for p in R.players(E, ours)
                             if p["has_ball"] and p["id"] == car["id"]), None)
                if carE is None:
                    continue
                c["kol s naším stojícím nosičem"] += 1

                my = R.players(S, ours)
                opp = R.players(S, theirs)
                opp_standing = [p for p in opp if p["state"] == STANDING]
                opp_all_pos = {(p["x"], p["y"]) for p in opp}
                occupied = {(p["x"], p["y"]) for p in my} | opp_all_pos
                occupied_wo_car = occupied - {(car["x"], car["y"])}
                acted_ids = {e["player_id"] for e in S["events"]}
                my_standing = [p for p in my
                               if p["id"] != car["id"] and p["state"] == STANDING]
                fillers_a = [(p["x"], p["y"], p["ma"], p["id"])
                             for p in my_standing]
                # (b) bez těl s jinou ÚTOČNOU povinností (zeď NE — obrana):
                #     mělo událost, nebo markuje s DRAHÝM odchodem (>0,20)
                fillers_b = []
                for p in my_standing:
                    if p["id"] in acted_ids:
                        continue
                    markers = [o for o in opp_standing
                               if cheb(o["x"], o["y"], p["x"], p["y"]) <= 1]
                    if markers and max(R.dodge_cost(p, o) for o in markers) \
                            > R.DODGE_COST_THRESHOLD:
                        continue
                    fillers_b.append((p["x"], p["y"], p["ma"], p["id"]))

                budget = stall_aware_steps(car, opp_standing, S["turn"], ez)
                max_prog, pk_pl, pk_a, pk_b = scan(
                    car, budget, ez_dir, occupied, occupied_wo_car,
                    opp_all_pos, opp_standing, fillers_a, fillers_b)

                # aritmetický cíl základního expandAdvance (pro kontext)
                tx = min(24, max(1, car["x"] + ez_dir * budget))
                ty = car["y"] + (1 if car["y"] < 5 else (-1 if car["y"] > 9 else 0))

                # --- odpor koridoru (replika corridorResistance, K9b) -----
                def resistance(x, y):
                    return sum(1 for o in opp_standing
                               if 1 <= (o["x"] - x) * ez_dir <= 4
                               and abs(o["y"] - y) <= 2)

                # validace repliky proti exportu (z AKTUÁLNÍHO pole nosiče)
                cr = S.get("corridor_resistance", -1)
                if cr >= 0:
                    c["CR: porovnáno s exportem"] += 1
                    if cr != resistance(car["x"], car["y"]):
                        c["CR: replika NESOUHLASÍ s exportem"] += 1

                # skutečné koncové pole základu: pull-back smyčka (C++ ř. 1425)
                bsteps, btx = budget, tx
                while bsteps > 0 and (
                        (btx, ty) in occupied
                        or any(cheb(o["x"], o["y"], btx, ty) <= 1
                               for o in opp_standing)):
                    bsteps -= 1
                    btx = min(24, max(1, car["x"] + ez_dir * bsteps))
                base_final = (btx, ty) if bsteps > 0 else (car["x"], car["y"])

                # rozpad OBĚHNUTÍ/PROLOMENÍ: pick ramene vs pole základu
                res_base = resistance(tx, ty)
                res_bfin = resistance(*base_final)
                for tag, pk in (("P38(a)", pk_a), ("placebo", pk_pl)):
                    if not pk or pk == (tx, ty):
                        continue
                    dres = resistance(pk[0], pk[1]) - res_base
                    lateral = pk[1] != ty
                    c[f"ZED {tag}: pick != základ"] += 1
                    if lateral and dres < 0:
                        c[f"ZED {tag}: OBĚHNUTÍ (strana, odpor klesá)"] += 1
                    elif not lateral and dres >= 0:
                        c[f"ZED {tag}: PROLOMENÍ (vpřed, odpor neklesá)"] += 1
                    elif lateral and dres == 0:
                        c[f"ZED {tag}: strana, odpor stejný"] += 1
                    elif lateral:
                        c[f"ZED {tag}: strana, odpor ROSTE"] += 1
                    else:
                        c[f"ZED {tag}: vpřed, odpor klesá"] += 1
                    if dres > 0:
                        c[f"ZED {tag}: pick má VYŠŠÍ odpor než základ"] += 1
                    # a totéž proti SKUTEČNÉMU konci základu (po pull-backu;
                    # když se základ nehne, je to pole nosiče)
                    dres2 = resistance(pk[0], pk[1]) - res_bfin
                    if dres2 < 0:
                        c[f"ZED {tag}: odpor KLESÁ vs skutečný konec základu"] += 1
                    elif dres2 > 0:
                        c[f"ZED {tag}: odpor ROSTE vs skutečný konec základu"] += 1

                if pk_pl:
                    c["VŠE: placebo najde pole"] += 1
                if pk_a:
                    c["VŠE: P38(a) najde pole"] += 1
                    if pk_a != (tx, ty):
                        c["VŠE: P38(a) změní cíl proti aritmetice"] += 1
                    if pk_pl and pk_a == pk_pl:
                        c["VŠE: P38(a) i placebo VYBEROU TOTÉŽ pole"] += 1
                if pk_b:
                    c["VŠE: P38(b) najde pole"] += 1

                dx = (carE["x"] - car["x"]) * ez_dir
                if dx != 0:
                    continue
                acted = car["id"] in acted_ids
                if acted:
                    continue
                c["IDLE: Δx=0 a nosič NEJEDNAL"] += 1
                tz_car = any(cheb(o["x"], o["y"], car["x"], car["y"]) <= 1
                             for o in opp_standing)
                key_free = "volný" if not tz_car else "v TZ"
                c[f"IDLE {key_free}"] += 1
                if pk_pl:
                    c["IDLE: placebo najde pole"] += 1
                    c[f"IDLE {key_free}: placebo najde"] += 1
                if pk_a:
                    c["IDLE: P38(a) najde pole"] += 1
                    c[f"IDLE {key_free}: P38(a) najde"] += 1
                    if pk_b:
                        c["IDLE: P38(a) najde a P38(b) taky"] += 1
                if pk_b:
                    c["IDLE: P38(b) najde pole"] += 1
        if (gi + 1) % 500 == 0:
            print("  ... %d/%d her" % (gi + 1, len(paths)), file=sys.stderr)

    for race in ("dwarf", "wood-elf"):
        c = st[race]
        n = c["kol s naším stojícím nosičem"]
        ni = c["IDLE: Δx=0 a nosič NEJEDNAL"]
        print("\n===== %s (nosič) =====" % race)
        print("kol se stojícím nosičem: %d" % n)

        def pr(label, num, den, dlabel):
            d = max(1, den)
            print("  %-46s %6d  %5.1f %% z %d (%s)"
                  % (label, num, 100.0 * num / d, den, dlabel))

        pr("VŠE: placebo najde pole", c["VŠE: placebo najde pole"], n, "všech kol")
        pr("VŠE: P38(a) najde pole", c["VŠE: P38(a) najde pole"], n, "všech kol")
        pr("VŠE: P38(b) najde pole", c["VŠE: P38(b) najde pole"], n, "všech kol")
        pr("VŠE: P38(a) změní cíl proti aritmetice",
           c["VŠE: P38(a) změní cíl proti aritmetice"],
           c["VŠE: P38(a) najde pole"], "kol kde P38(a) najde")
        pr("VŠE: P38(a) a placebo vyberou TOTÉŽ pole",
           c["VŠE: P38(a) i placebo VYBEROU TOTÉŽ pole"],
           c["VŠE: P38(a) najde pole"], "kol kde P38(a) najde")
        print("IDLE kola (Δx=0, nosič bez události): %d  (%s volný %d, v TZ %d)"
              % (ni, race, c["IDLE volný"], c["IDLE v TZ"]))
        pr("IDLE: placebo najde pole", c["IDLE: placebo najde pole"], ni, "idle kol")
        pr("IDLE: P38(a) najde pole", c["IDLE: P38(a) najde pole"], ni, "idle kol")
        pr("IDLE: P38(b) najde pole", c["IDLE: P38(b) najde pole"], ni, "idle kol")
        pr("IDLE volný: placebo najde", c["IDLE volný: placebo najde"],
           c["IDLE volný"], "idle volných")
        pr("IDLE volný: P38(a) najde", c["IDLE volný: P38(a) najde"],
           c["IDLE volný"], "idle volných")
        pr("IDLE v TZ: placebo najde", c["IDLE v TZ: placebo najde"],
           c["IDLE v TZ"], "idle v TZ")
        pr("IDLE v TZ: P38(a) najde", c["IDLE v TZ: P38(a) najde"],
           c["IDLE v TZ"], "idle v TZ")
        print("validace CR repliky: %d neshod z %d porovnaných"
              % (c["CR: replika NESOUHLASÍ s exportem"], c["CR: porovnáno s exportem"]))
        for tag in ("P38(a)", "placebo"):
            nz = c[f"ZED {tag}: pick != základ"]
            print("ZEĎ %s (kola, kde pick != pole základu): %d" % (tag, nz))
            for lab in ("OBĚHNUTÍ (strana, odpor klesá)",
                        "PROLOMENÍ (vpřed, odpor neklesá)",
                        "strana, odpor stejný", "strana, odpor ROSTE",
                        "vpřed, odpor klesá",
                        "pick má VYŠŠÍ odpor než základ",
                        "odpor KLESÁ vs skutečný konec základu",
                        "odpor ROSTE vs skutečný konec základu"):
                pr("  " + lab, c[f"ZED {tag}: {lab}"], nz, "picků != základ")


if __name__ == "__main__":
    main()
