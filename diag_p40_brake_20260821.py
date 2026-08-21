#!/usr/bin/env python3
"""P40 21.08. — Je kritérium klece BRZDA a je brzda rasová? (Q-A, Q-B, screen, X3)

Nad corpus_baseline_20260819_data (3 000 her). Replika ramene P38/placeba
převzata z diag_p38_decomposition_20260820.py (validovaná replika CR).

BLOKOVANÉ kolo := placebo pole najde, P38 ne (v kole s naším stojícím nosičem).

Q-A: u blokovaného kola změřit, JAKÉ pole brzda zrušila:
  - odpor koridoru placebo picku vs SKUTEČNÉHO náhradního konce (base_final
    = aritmetický fallback s pull-backem; když steps klesne na 0, nosič stojí);
  - vzdálenost picku od lajny (min(y, 14-y)) vs nosičova;
  - obětovaný postup prog(pick).
Rozklad DŮVODŮ bloku na placebo picku (nezávisle, ne short-circuit):
  <4 rohy (lajna/EZ) · soupeř (lehlý) na rohu · špinavý roh (stojící soupeř
  u rohu) · ortogonála obsazena NAŠÍM stojícím / naším lehlým / lehlým soupeřem
  · nedost výplní. + RELAXACE kotvení: naše stojící tělo na ortogonále kandidáta
  se ignoruje, pokud samo dosáhne na některý roh kandidáta (vada smíšeného
  časového kotvení: klauzule 3 se čte z DNEŠNÍ desky, klauzule 1 z BUDOUCÍ).

SCREEN test (definice PŘED měřením, nezměněná po ní):
  na desce NÁSLEDUJÍCÍHO logu E; hrozby = stojící soupeři s cheb<=5 od nosiče;
  screen-tělo = naše stojící tělo != nosič, cheb(t,nosič)>=2, a existuje hrozba
  o: cheb(t,o) < cheb(nosič,o) a t leží v bounding boxu (nosič,o) +1;
  SCREEN := >=3 screen-těla po dvou cheb>=2 (rozestup>1, nesousedí s nosičem).
  Jinak ROZSYP. Kola bez hrozby do 5 se počítají zvlášť (nelze klasifikovat).

X3 (nepřímý odhad kotvení CAGE): v kolech, kde se nosič pohnul a spoluhráčův
MOVE končí diagonálně u STARÉHO vs NOVÉHO pole nosiče; + pořadí událostí
(výplň PŘED prvním MOVE nosiče = stavěla se kolem starého pole).

Každý podíl s vypsaným jmenovatelem.
"""
import sys, glob
from collections import Counter
sys.path.insert(0, "/home/jan/claude/bloodbowl")
import diag_rules_checks_20260812 as R

STANDING = 0
DIAG = ((-1, -1), (-1, 1), (1, -1), (1, 1))
ORTH = ((-1, 0), (1, 0), (0, -1), (0, 1))


def on_pitch(x, y):
    return 0 <= x <= 25 and 0 <= y <= 14


def cheb(ax, ay, bx, by):
    return max(abs(ax - bx), abs(ay - by))


def stall_aware_steps(car, opp_standing, turn, ez):
    dist = abs(car["x"] - ez)
    tr = max(1, 9 - turn)
    ideal = max(1, (dist + tr - 1) // tr)
    mv = car["ma"]
    blitzable = any(cheb(o["x"], o["y"], car["x"], car["y"]) <= o["ma"]
                    for o in opp_standing)
    if tr <= 2 or blitzable:
        return min(ideal, mv)
    return min(ideal, max(1, mv // 2))


def cage_ok(cand, occupied_wo_car, opp_all_pos, opp_standing, fillers,
            relax_own_ortho=False, my_standing_pos=None):
    cx, cy = cand
    for ox, oy in ORTH:
        nx, ny = cx + ox, cy + oy
        if not on_pitch(nx, ny) or (nx, ny) not in occupied_wo_car:
            continue
        if relax_own_ortho and (nx, ny) in my_standing_pos:
            # relaxace: naše stojící tělo, které samo dosáhne na roh kandidáta
            ma = my_standing_pos[(nx, ny)]
            if any(on_pitch(cx+dx, cy+dy) and cheb(nx, ny, cx+dx, cy+dy) <= ma
                   for dx, dy in DIAG):
                continue
        return False
    corners = []
    for dx, dy in DIAG:
        kx, ky = cx + dx, cy + dy
        if not on_pitch(kx, ky):
            continue
        if (kx, ky) in opp_all_pos:
            return False
        for o in opp_standing:
            if cheb(o["x"], o["y"], kx, ky) <= 1:
                return False
        corners.append((kx, ky))
    if len(corners) < 4:
        return False
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


def fail_reasons(cand, occupied_wo_car, opp_all_pos, opp_prone_pos,
                 opp_standing, my_standing_pos, my_prone_pos, fillers):
    """Nezávislý rozklad důvodů, proč cage_ok na cand padá."""
    rs = set()
    cx, cy = cand
    for ox, oy in ORTH:
        nx, ny = cx + ox, cy + oy
        if not on_pitch(nx, ny) or (nx, ny) not in occupied_wo_car:
            continue
        if (nx, ny) in my_standing_pos:
            rs.add("ortho: NAŠE stojící tělo")
        elif (nx, ny) in my_prone_pos:
            rs.add("ortho: naše lehlé tělo")
        elif (nx, ny) in opp_prone_pos:
            rs.add("ortho: LEHLÝ soupeř")
        else:
            rs.add("ortho: stojící soupeř")
    corners = []
    n_corners = 0
    for dx, dy in DIAG:
        kx, ky = cx + dx, cy + dy
        if not on_pitch(kx, ky):
            continue
        n_corners += 1
        if (kx, ky) in opp_prone_pos:
            rs.add("roh: drží LEHLÝ soupeř")
        elif (kx, ky) in opp_all_pos:
            rs.add("roh: drží stojící soupeř")
        if any(cheb(o["x"], o["y"], kx, ky) <= 1 for o in opp_standing):
            rs.add("roh: ŠPINAVÝ (stojící soupeř u rohu)")
        corners.append((kx, ky))
    if n_corners < 4:
        rs.add("<4 rohy (lajna/EZ)")
    else:
        used = set()
        filled = 0
        for kx, ky in corners:
            best_id, best_d = -1, 999
            for fx, fy, fma, fid in fillers:
                if fid in used:
                    continue
                d = cheb(fx, fy, kx, ky)
                if d > fma or d >= best_d:
                    continue
                best_d, best_id = d, fid
            if best_id >= 0:
                used.add(best_id)
                filled += 1
        if filled < 4:
            rs.add("nedost VÝPLNÍ (dosah)")
    return rs


def scan(car, budget, ez_dir, occupied, occupied_wo_car, opp_all_pos,
         opp_standing, fillers, my_standing_pos, relax=False):
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
    pk_pl = pk_38 = pk_rx = None
    b_pl = b_38 = b_rx = 0
    for x, y, prog in cands:
        if prog < 1 or prog < max_prog - 1:
            continue
        if any(cheb(o["x"], o["y"], x, y) <= 1 for o in opp_standing):
            continue
        if prog > b_pl:
            b_pl, pk_pl = prog, (x, y)
        if cage_ok((x, y), occupied_wo_car, opp_all_pos, opp_standing, fillers):
            if prog > b_38:
                b_38, pk_38 = prog, (x, y)
        elif relax and cage_ok((x, y), occupied_wo_car, opp_all_pos,
                               opp_standing, fillers, True, my_standing_pos):
            if prog > b_rx:
                b_rx, pk_rx = prog, (x, y)
    return max_prog, pk_pl, pk_38, pk_rx


def main():
    paths = sorted(glob.glob("/home/jan/claude/bloodbowl/corpus_baseline_20260819_data/*.json.gz"))
    st = {"dwarf": Counter(), "wood-elf": Counter()}
    acc = {"dwarf": Counter(), "wood-elf": Counter()}   # souhrny (sumy)
    per_game = {"dwarf": [], "wood-elf": []}
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
            a = acc[race]
            g_blocked = g_differ = 0
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
                c["kol se stojícím nosičem"] += 1

                my = R.players(S, ours)
                opp = R.players(S, theirs)
                opp_standing = [p for p in opp if p["state"] == STANDING]
                opp_all_pos = {(p["x"], p["y"]) for p in opp}
                opp_prone_pos = {(p["x"], p["y"]) for p in opp
                                 if p["state"] != STANDING}
                occupied = {(p["x"], p["y"]) for p in my} | opp_all_pos
                occupied_wo_car = occupied - {(car["x"], car["y"])}
                my_standing = [p for p in my
                               if p["id"] != car["id"] and p["state"] == STANDING]
                my_standing_pos = {(p["x"], p["y"]): p["ma"] for p in my_standing}
                my_prone_pos = {(p["x"], p["y"]) for p in my
                                if p["id"] != car["id"] and p["state"] != STANDING}
                fillers = [(p["x"], p["y"], p["ma"], p["id"]) for p in my_standing]

                budget = stall_aware_steps(car, opp_standing, S["turn"], ez)
                max_prog, pk_pl, pk_38, pk_rx = scan(
                    car, budget, ez_dir, occupied, occupied_wo_car,
                    opp_all_pos, opp_standing, fillers, my_standing_pos,
                    relax=True)

                def resistance(x, y):
                    return sum(1 for o in opp_standing
                               if 1 <= (o["x"] - x) * ez_dir <= 4
                               and abs(o["y"] - y) <= 2)

                # skutečný náhradní konec: aritmetický fallback s pull-backem
                tx = min(24, max(1, car["x"] + ez_dir * budget))
                ty = car["y"] + (1 if car["y"] < 5 else (-1 if car["y"] > 9 else 0))
                bsteps, btx = budget, tx
                while bsteps > 0 and (
                        (btx, ty) in occupied
                        or any(cheb(o["x"], o["y"], btx, ty) <= 1
                               for o in opp_standing)):
                    bsteps -= 1
                    btx = min(24, max(1, car["x"] + ez_dir * bsteps))
                base_final = (btx, ty) if bsteps > 0 else (car["x"], car["y"])
                base_stuck = bsteps <= 0

                if pk_pl:
                    c["placebo najde pole"] += 1
                if pk_38:
                    c["P38 najde pole"] += 1
                if pk_pl and pk_38 and pk_pl != pk_38:
                    c["oba najdou, RŮZNÁ pole"] += 1
                    g_differ += 1
                    a["differ: dRES(P38 - placebo)"] += (
                        resistance(*pk_38) - resistance(*pk_pl))
                if pk_pl and not pk_38:
                    c["BLOKOVÁNO (placebo ano, P38 ne)"] += 1
                    g_blocked += 1
                    g_differ += 1
                    for reason in fail_reasons(
                            pk_pl, occupied_wo_car, opp_all_pos, opp_prone_pos,
                            opp_standing, my_standing_pos, my_prone_pos, fillers):
                        c[f"BLK důvod na picku: {reason}"] += 1
                    if pk_rx:
                        c["BLK: RELAXACE kotvení blok RUŠÍ"] += 1
                    # Q-A metriky
                    dres = resistance(*pk_pl) - resistance(*base_final)
                    a["BLK: sum dRES(pick - náhrada)"] += dres
                    c["BLK: dRES>0 (pick horší odpor)"] += (dres > 0)
                    c["BLK: dRES=0"] += (dres == 0)
                    c["BLK: dRES<0 (pick lepší odpor)"] += (dres < 0)
                    sd_pick = min(pk_pl[1], 14 - pk_pl[1])
                    sd_car = min(car["y"], 14 - car["y"])
                    a["BLK: sum lajna_dist(pick)"] += sd_pick
                    a["BLK: sum lajna_dist(nosič)"] += sd_car
                    c["BLK: pick BLÍŽ lajně než nosič"] += (sd_pick < sd_car)
                    a["BLK: sum prog(pick)"] += ez_dir * (pk_pl[0] - car["x"])
                    prog_nahr = ez_dir * (base_final[0] - car["x"])
                    a["BLK: sum prog(náhrada)"] += prog_nahr
                    c["BLK: náhrada = nosič stojí (fallback 0)"] += base_stuck

                    # idle podmnožina
                    acted_ids = {e["player_id"] for e in S["events"]}
                    dxc = (carE["x"] - car["x"]) * ez_dir
                    if dxc == 0 and car["id"] not in acted_ids:
                        c["BLK a IDLE"] += 1

                    # SCREEN klasifikace na desce E
                    myE = [p for p in R.players(E, ours)
                           if p["id"] != car["id"] and p["state"] == STANDING]
                    oppE = [p for p in R.players(E, theirs)
                            if p["state"] == STANDING]
                    cx, cy = carE["x"], carE["y"]
                    threats = [o for o in oppE
                               if cheb(o["x"], o["y"], cx, cy) <= 5]
                    if not threats:
                        c["BLK screen: bez hrozby do 5 (neklasifikováno)"] += 1
                    else:
                        sb = []
                        for b in myE:
                            if cheb(b["x"], b["y"], cx, cy) < 2:
                                continue
                            for o in threats:
                                if cheb(b["x"], b["y"], o["x"], o["y"]) \
                                        < cheb(cx, cy, o["x"], o["y"]) \
                                        and min(cx, o["x"])-1 <= b["x"] <= max(cx, o["x"])+1 \
                                        and min(cy, o["y"])-1 <= b["y"] <= max(cy, o["y"])+1:
                                    sb.append(b)
                                    break
                        # najdi max podmnožinu s pairwise cheb>=2 (greedy)
                        picked = []
                        for b in sb:
                            if all(cheb(b["x"], b["y"], q["x"], q["y"]) >= 2
                                   for q in picked):
                                picked.append(b)
                        if len(picked) >= 3:
                            c["BLK screen: SCREEN"] += 1
                            a["BLK screen: sum lajna_dist nosiče (SCREEN)"] += \
                                min(cy, 14 - cy)
                            c["BLK screen: SCREEN n"] += 1
                        else:
                            c["BLK screen: ROZSYP"] += 1
                            a["BLK screen: sum lajna_dist nosiče (ROZSYP)"] += \
                                min(cy, 14 - cy)
                            c["BLK screen: ROZSYP n"] += 1

                # X3: kotvení výplní, jen kola s pohybem nosiče
                if (carE["x"], carE["y"]) != (car["x"], car["y"]):
                    evs = S["events"]
                    car_mv_idx = next((k for k, e in enumerate(evs)
                                       if e["type"] == "MOVE"
                                       and e["player_id"] == car["id"]), None)
                    old = (car["x"], car["y"])
                    new = (carE["x"], carE["y"])
                    my_ids = {p["id"] for p in my}
                    for k, e in enumerate(evs):
                        if e["type"] != "MOVE" or e["player_id"] == car["id"] \
                                or e["player_id"] not in my_ids:
                            continue
                        to = (e["to_x"], e["to_y"])
                        d_old = cheb(to[0], to[1], old[0], old[1])
                        d_new = cheb(to[0], to[1], new[0], new[1])
                        diag_old = d_old == 1 and to[0] != old[0] and to[1] != old[1]
                        diag_new = d_new == 1 and to[0] != new[0] and to[1] != new[1]
                        if diag_old and not diag_new:
                            c["X3 výplň: diag u STARÉHO pole"] += 1
                            if car_mv_idx is not None and k < car_mv_idx:
                                c["X3 výplň u starého: PŘED pohybem nosiče"] += 1
                        elif diag_new and not diag_old:
                            c["X3 výplň: diag u NOVÉHO pole"] += 1
                            if car_mv_idx is not None and k < car_mv_idx:
                                c["X3 výplň u nového: PŘED pohybem nosiče"] += 1
            per_game[race].append((g_blocked, g_differ))
        if (gi + 1) % 500 == 0:
            print("  ... %d/%d her" % (gi + 1, len(paths)), file=sys.stderr)

    for race in ("dwarf", "wood-elf"):
        c, a = st[race], acc[race]
        n = c["kol se stojícím nosičem"]
        nb = c["BLOKOVÁNO (placebo ano, P38 ne)"]
        print("\n===== %s (nosič) =====" % race)
        print("kol se stojícím nosičem: %d" % n)

        def pr(label, num, den, dl):
            print("  %-52s %6d  %5.1f %% z %d (%s)"
                  % (label, num, 100.0 * num / max(1, den), den, dl))

        pr("placebo najde pole", c["placebo najde pole"], n, "kol")
        pr("P38 najde pole", c["P38 najde pole"], n, "kol")
        pr("BLOKOVÁNO (placebo ano, P38 ne)", nb, n, "kol")
        pr("BLK a IDLE (Δx=0, nosič bez události)", c["BLK a IDLE"], nb, "blokovaných")
        pr("oba najdou, RŮZNÁ pole", c["oba najdou, RŮZNÁ pole"], n, "kol")
        print("  --- důvody bloku na placebo picku (nezávisle, může jich být víc):")
        for k in sorted(c):
            if k.startswith("BLK důvod"):
                pr("  " + k[16:], c[k], nb, "blokovaných")
        pr("RELAXACE kotvení (naše ortho tělo dosáhne rohu) blok RUŠÍ",
           c["BLK: RELAXACE kotvení blok RUŠÍ"], nb, "blokovaných")
        print("  --- Q-A metriky blokovaného picku vs skutečné náhrady:")
        pr("dRES>0: zrušený pick měl VYŠŠÍ odpor než náhrada",
           c["BLK: dRES>0 (pick horší odpor)"], nb, "blokovaných")
        pr("dRES=0", c["BLK: dRES=0"], nb, "blokovaných")
        pr("dRES<0: zrušený pick měl NIŽŠÍ odpor",
           c["BLK: dRES<0 (pick lepší odpor)"], nb, "blokovaných")
        print("    mean dRES(pick-náhrada) = %+.3f  (n=%d)"
              % (a["BLK: sum dRES(pick - náhrada)"] / max(1, nb), nb))
        print("    mean lajna_dist: pick %.2f vs nosič %.2f  (n=%d)"
              % (a["BLK: sum lajna_dist(pick)"] / max(1, nb),
                 a["BLK: sum lajna_dist(nosič)"] / max(1, nb), nb))
        pr("pick BLÍŽ lajně než nosič", c["BLK: pick BLÍŽ lajně než nosič"],
           nb, "blokovaných")
        print("    mean prog: pick %.2f vs náhrada %.2f  (n=%d)"
              % (a["BLK: sum prog(pick)"] / max(1, nb),
                 a["BLK: sum prog(náhrada)"] / max(1, nb), nb))
        pr("náhrada = nosič stojí (fallback stáhl na 0)",
           c["BLK: náhrada = nosič stojí (fallback 0)"], nb, "blokovaných")
        nd = c["oba najdou, RŮZNÁ pole"]
        print("    mean dRES(P38-placebo) kde oba najdou různě = %+.3f (n=%d)"
              % (a["differ: dRES(P38 - placebo)"] / max(1, nd), nd))
        print("  --- SCREEN klasifikace (blokovaná kola, deska E):")
        ncls = c["BLK screen: SCREEN"] + c["BLK screen: ROZSYP"]
        pr("SCREEN", c["BLK screen: SCREEN"], ncls, "klasifikovaných")
        pr("ROZSYP", c["BLK screen: ROZSYP"], ncls, "klasifikovaných")
        pr("bez hrozby do 5 (neklasifikováno)",
           c["BLK screen: bez hrozby do 5 (neklasifikováno)"], nb, "blokovaných")
        if c["BLK screen: SCREEN n"]:
            print("    lajna_dist nosiče: SCREEN %.2f (n=%d) vs ROZSYP %.2f (n=%d)"
                  % (a["BLK screen: sum lajna_dist nosiče (SCREEN)"]
                     / c["BLK screen: SCREEN n"], c["BLK screen: SCREEN n"],
                     a["BLK screen: sum lajna_dist nosiče (ROZSYP)"]
                     / max(1, c["BLK screen: ROZSYP n"]),
                     c["BLK screen: ROZSYP n"]))
        print("  --- X3 kotvení výplní (kola s pohybem nosiče):")
        no = c["X3 výplň: diag u STARÉHO pole"]
        nn = c["X3 výplň: diag u NOVÉHO pole"]
        print("    výplně diag u STARÉHO pole: %d (z toho PŘED pohybem nosiče %d)"
              % (no, c["X3 výplň u starého: PŘED pohybem nosiče"]))
        print("    výplně diag u NOVÉHO pole:  %d (z toho PŘED pohybem nosiče %d)"
              % (nn, c["X3 výplň u nového: PŘED pohybem nosiče"]))
        # Q-B: na hru
        games = per_game[race]
        ng = len(games)
        print("  --- Q-B (na hru, n=%d her s touto rasou):" % ng)
        print("    blokovaných kol/hru: %.3f   kol s odlišnou volbou/hru: %.3f"
              % (sum(g[0] for g in games) / max(1, ng),
                 sum(g[1] for g in games) / max(1, ng)))


if __name__ == "__main__":
    main()
