#!/usr/bin/env python3
"""Jeden průchod korpusem, dvě otázky (12.08.):

A) Četnost situací S0-S10 podle stromu ČÁSTI 1 spec
   (evidence/dwarf_turn_procedure_spec_20260811.md) pro KAŽDÉ trpasličí kolo.
   + kolik kol spadne MIMO strom / do víc situací zároveň.

B) Rozklad díry v S5 (volný míč, dosáhneme): co engine dělal místo sběru,
   byl sběr fyzicky dostupný (REKONSTRUKCE - množina legálních maker se
   neloguje, predikát X5 chybí), S5.3 (záloha u míče), S5.4 (krytí po sběru).

Rozhodnutí a aproximace (vše přiznané v reportu):
  * S0/S1 (SETUP) v korpusu NEJSOU - turn_logs nesou jen odehraná kola.
  * plan.achievable_pace je v logu vždy 0.0 -> paceAch se bere podle MA
    nosiče z měření R1 (Runner MA6=3.41, MA5=2.5 interpolace, MA4=1.50);
    citlivost: konstanty 1.73 / 2.5 / 3.41.
  * rezerva ≈ 0 čtu jako == 0 (je to celé číslo).
  * "poslední kolo půle" = turn 8; S8 "soupeři zbývají <=2 kola" = turn >= 7
    (aproximace, pořadí tahů v kole neřeším).
  * S5 klasifikuji jen podle DOSAHU (Chebyshev <= MA+2 stojícího hráče);
    "umíme zajistit" nelze na začátku kola spočítat -> překryv S5/S6 počítám.
  * konec kola = turnLogs[i+1] (ČÁST 4.1); kola před TD/koncem půle vyřazuji
    z end-of-turn metrik (S5.4, pohyb k míči), z klasifikace ne.
  * dosah po cestě = Dijkstra po 8-okolí, obsazená pole (kdokoliv na hřišti,
    i ležící) neprůchozí, dodge = opuštění pole v TZ STOJÍCÍHO soupeře,
    ležící hráč vstává za 3 MA (stunned ne), GFI +2 pole.
"""
import gzip, json, glob, heapq, sys
from collections import Counter, defaultdict

DIRS = ["diag_replay_mine_20260811_data",
        "diag_replay_mine_20260811b_data",
        "diag_replay_mine_20260811c_data"]

STAND, PRONE, STUN, OFF = 0, 1, 2, 3
PACE_BY_MA = {6: 3.41, 5: 2.5, 4: 1.50}   # měření R1 (pace_vs_contact)


def cheb(a, b):
    return max(abs(a[0] - b[0]), abs(a[1] - b[1]))


def ceil_div(a, b):
    import math
    return math.ceil(a / b)


def on_pitch(x, y):
    return 0 <= x <= 25 and 0 <= y <= 14


def attack_dir(game):
    """Za každou půli: home útočí na endzone dál od svého těžiště v 1. logu půle."""
    ez = {}
    for h in (1, 2):
        logs = [t for t in game["turn_logs"] if t["half"] == h]
        if not logs:
            continue
        t = logs[0]
        hx = [p["x"] for p in t["home_players"] if p["state"] != OFF]
        ax = [p["x"] for p in t["away_players"] if p["state"] != OFF]
        if not hx or not ax:
            ez[h] = (25, 0)
            continue
        home_ez = 25 if sum(hx) / len(hx) < sum(ax) / len(ax) else 0
        ez[h] = (home_ez, 25 - home_ez)   # (home útočí na, away útočí na)
    return ez


def reach_simple(players, ball):
    """Dosah bez cesty: stojící Chebyshev <= MA+2, ležící <= MA-3+2."""
    best = None
    for p in players:
        if p["state"] == STAND:
            allow = p["ma"] + 2
        elif p["state"] == PRONE:
            allow = max(0, p["ma"] - 3) + 2
        else:
            continue
        if cheb((p["x"], p["y"]), ball) <= allow:
            best = p if best is None else best
    return best is not None


def path_reach(player, ball, occupied, opp_tz):
    """Dijkstra: min (dodges, gfi, kroky) do pole míče. None = nedosáhne."""
    if player["state"] == STAND:
        allow = player["ma"]
    elif player["state"] == PRONE:
        allow = player["ma"] - 3
        if allow < 0:
            return None
    else:
        return None
    start = (player["x"], player["y"])
    if start == ball:
        return (0, 0, 0)
    # (dodges, kroky) lexikograficky; gfi = kroky nad allow (max 2)
    dist = {start: (0, 0)}
    pq = [(0, 0, start)]
    while pq:
        dg, st, pos = heapq.heappop(pq)
        if dist.get(pos, (99, 99)) < (dg, st):
            continue
        if st >= allow + 2:
            continue
        leave_dodge = 1 if opp_tz.get(pos, 0) > 0 else 0
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                if not dx and not dy:
                    continue
                nx, ny = pos[0] + dx, pos[1] + dy
                if not on_pitch(nx, ny):
                    continue
                npos = (nx, ny)
                if npos != ball and npos in occupied:
                    continue
                if npos == ball and npos in occupied:
                    return None   # míč pod tělem - pole neprůchozí
                nd = (dg + leave_dodge, st + 1)
                if nd < dist.get(npos, (99, 99)):
                    dist[npos] = nd
                    if npos == ball:
                        continue
                    heapq.heappush(pq, (nd[0], nd[1], npos))
    if ball not in dist:
        return None
    dg, st = dist[ball]
    return (dg, max(0, st - allow), st)


def tz_of(players):
    tz = {}
    for p in players:
        if p["state"] != STAND:
            continue
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                if dx or dy:
                    k = (p["x"] + dx, p["y"] + dy)
                    tz[k] = tz.get(k, 0) + 1
    return tz


def classify(t, mine, theirs, ez, pace_mode):
    """-> (situace, rezerva|None, poznámky-set)"""
    notes = set()
    turns_left = 9 - t["turn"]
    ball = (t["ball_x"], t["ball_y"])
    carrier = next((p for p in mine if p["has_ball"]), None)
    their_carrier = next((p for p in theirs if p["has_ball"]), None)

    if carrier is not None:
        d = abs(carrier["x"] - ez)
        pace = pace_mode if isinstance(pace_mode, float) else PACE_BY_MA.get(carrier["ma"], 2.5)
        need = ceil_div(d, pace) if d > 0 else 0
        rez = turns_left - need
        if d <= carrier["ma"] + 2:
            return ("S10" if (t["turn"] == 8 or rez < 0) else "S9"), rez, notes
        if rez > 0:
            return "S3", rez, notes
        if rez == 0:
            return "S2", rez, notes
        return "S4", rez, notes

    if their_carrier is not None:
        return ("S8" if t["turn"] >= 7 else "S7"), None, notes

    # míč volný
    if not on_pitch(*ball):
        return "MIMO_STROM:míč mimo hřiště", None, notes
    ours = reach_simple(mine, ball)
    their = reach_simple(theirs, ball)
    if ours and their:
        notes.add("překryv S5+S6 (dosáhnou obě strany)")
    if ours:
        return "S5", None, notes
    if their:
        return "S6", None, notes
    return "S6'", None, notes


def main():
    sit = Counter()
    sit_by_pace = {m: Counter() for m in ("MA", 1.73, 2.5, 3.41)}
    overlap = Counter()
    n_turns = n_excluded_end = 0
    rez_hist = Counter()

    # --- B) S5 ---
    s5 = dict(n=0, attempt=0, attempt_ok=0, ball_under_body=0,
              backup_start=0, backup_start_attempt=0, backup_end=0,
              covered_after=0, covered_n=0, runner_best=0)
    s5_tier = Counter()               # dostupnost (rekonstrukce) všech S5 kol
    noatt_tier = Counter()            # dostupnost v kolech BEZ pokusu
    noatt_action = Counter()          # co dělal místo toho
    noatt_cross = Counter()           # (tier, akce)
    noatt_turnover = 0
    noatt_n = 0
    att_by_tier = Counter()

    games = []
    for d in DIRS:
        games += sorted(glob.glob(d + "/g*.json.gz"))
    for fn in games:
        g = json.load(gzip.open(fn))
        my_side = "home" if g["home_race"] == "dwarf" else "away"
        if g[my_side + "_race"] != "dwarf":
            continue
        ezmap = attack_dir(g)
        tl = g["turn_logs"]
        for i, t in enumerate(tl):
            if t["active_team"] != my_side:
                continue
            n_turns += 1
            mine = [p for p in t[my_side + "_players"] if p["state"] != OFF]
            theirs = [p for p in t[("away" if my_side == "home" else "home") + "_players"]
                      if p["state"] != OFF]
            ez = ezmap[t["half"]][0 if my_side == "home" else 1]

            s, rez, notes = classify(t, mine, theirs, ez, "MA")
            sit[s] += 1
            if rez is not None:
                rez_hist[max(-6, min(6, rez))] += 1
            for nt in notes:
                overlap[nt] += 1
            for m in sit_by_pace:
                sm, _, _ = classify(t, mine, theirs, ez, m if m != "MA" else "MA")
                sit_by_pace[m][sm] += 1

            # end-of-turn snímek platný?
            end_ok = (i + 1 < len(tl) and tl[i + 1]["half"] == t["half"]
                      and not t["touchdown"])
            if not end_ok:
                n_excluded_end += 1

            if s != "S5":
                continue

            # ---------------- B) rozklad S5 ----------------
            s5["n"] += 1
            ball = (t["ball_x"], t["ball_y"])
            my_ids = {p["id"] for p in mine}
            ev = t["events"]
            picks = [e for e in ev if e["type"] == "PICKUP" and e["player_id"] in my_ids]
            attempt = bool(picks)
            occupied = {(p["x"], p["y"]) for p in mine + theirs}
            otz = tz_of(theirs)

            # rekonstrukce dostupnosti (X5 chybí -> dopočet z pozice)
            best = None
            best_runner = False
            for p in mine:
                r = path_reach(p, ball, occupied, otz)
                if r is not None and (best is None or r < best):
                    best = r
                    best_runner = p["name"].startswith("Runner")
            if ball in occupied:
                s5["ball_under_body"] += 1
            if best is None:
                tier = "D nedosažitelný po cestě"
            elif best[0] == 0 and best[1] == 0:
                tier = "A čistý (0 dodge, 0 GFI)"
            elif best[0] == 0:
                tier = "B jen GFI"
            else:
                tier = "C nutný dodge"
            s5_tier[tier] += 1
            if best_runner and tier.startswith("A"):
                s5["runner_best"] += 1

            # S5.3 záloha u míče před hodem (aproximace: startovní snímek)
            backup = any(cheb((p["x"], p["y"]), ball) == 1 and p["state"] == STAND
                         for p in mine)
            if backup:
                s5["backup_start"] += 1

            if attempt:
                s5["attempt"] += 1
                att_by_tier[tier] += 1
                if backup:
                    s5["backup_start_attempt"] += 1
                if any(e.get("success") for e in picks):
                    s5["attempt_ok"] += 1
                    # S5.4 krytí po sběru (jen platný koncový snímek)
                    if end_ok:
                        E = tl[i + 1]
                        emine = E[my_side + "_players"]
                        eth = [p for p in E[("away" if my_side == "home" else "home")
                                            + "_players"] if p["state"] == STAND]
                        ecar = next((p for p in emine if p["has_ball"]), None)
                        if ecar is not None:
                            s5["covered_n"] += 1
                            reach_blitz = any(
                                cheb((q["x"], q["y"]), (ecar["x"], ecar["y"])) - 1
                                <= q["ma"] + 2 for q in eth)
                            if not reach_blitz:
                                s5["covered_after"] += 1
            else:
                noatt_n += 1
                if any(e["type"] == "TURNOVER" for e in ev):
                    noatt_turnover += 1
                noatt_tier[tier] += 1
                # co dělal místo sběru
                blocks = any(e["type"] == "BLOCK" and e["player_id"] in my_ids for e in ev)
                moved = any(e["type"] == "MOVE" and e["player_id"] in my_ids for e in ev)
                fouled = any(e["type"] == "FOUL" and e["player_id"] in my_ids for e in ev)
                closer = None
                if end_ok:
                    E = tl[i + 1]
                    em = [p for p in E[my_side + "_players"] if p["state"] == STAND]
                    d0 = min((cheb((p["x"], p["y"]), ball) for p in mine
                              if p["state"] == STAND), default=99)
                    eb = (E["ball_x"], E["ball_y"])
                    d1 = min((cheb((p["x"], p["y"]), eb) for p in em), default=99)
                    closer = d1 < d0
                if not blocks and not moved and not fouled:
                    act = "NIC (žádná naše akce)"
                elif closer:
                    act = "POHYB K MÍČI (nedošel/nesebral)" + (" +blok" if blocks else "")
                elif blocks and not moved:
                    act = "JEN BLOKY"
                elif blocks:
                    act = "BLOKY+POHYB jinam"
                elif fouled and not moved:
                    act = "FAUL"
                else:
                    act = "POHYB JINAM"
                noatt_action[act] += 1
                noatt_cross[(tier.split()[0], act)] += 1

    # ---------------- výstup ----------------
    print(f"her: {len(games)} (trpaslík v každé) · trpasličích kol: {n_turns}")
    print(f"kol s neplatným koncovým snímkem (TD/půle/konec logu): {n_excluded_end}")
    print("\n=== A) ROZLOŽENÍ SITUACÍ (klasifikace na ZAČÁTKU kola, pace=MA nosiče) ===")
    for k in sorted(sit, key=lambda k: -sit[k]):
        print(f"  {k:<28} {sit[k]:>5}  {sit[k]/n_turns:>6.1%}")
    print(f"  překryvy: {dict(overlap)}")
    print("\n  citlivost S2/S3/S4/S9/S10 na paceAch:")
    hdr = ["S2", "S3", "S4", "S9", "S10"]
    print("   pace      " + "".join(f"{h:>7}" for h in hdr))
    for m in ("MA", 1.73, 2.5, 3.41):
        c = sit_by_pace[m]
        print(f"   {str(m):<9}" + "".join(f"{c.get(h,0):>7}" for h in hdr))
    print("\n  histogram rezervy (jen kola s naším nosičem, pace=MA):")
    for k in sorted(rez_hist):
        print(f"   rez {k:>3}: {rez_hist[k]:>4} {'#'*(rez_hist[k]//10)}")

    print("\n=== B) S5 — VOLNÝ MÍČ, DOSÁHNEME ===")
    n5 = s5["n"]
    print(f"kol S5: {n5} · pokus o sběr: {s5['attempt']} ({s5['attempt']/n5:.1%})"
          f" · úspěšný: {s5['attempt_ok']}")
    print(f"míč pod tělem hráče: {s5['ball_under_body']}")
    print("\nREKONSTRUOVANÁ dostupnost sběru (X5 se neloguje — dopočet z pozice, "
          "Dijkstra, obsazená pole neprůchozí):")
    for k in sorted(s5_tier):
        print(f"  {k:<30} {s5_tier[k]:>4}  {s5_tier[k]/n5:>6.1%}"
              f"   (z toho pokus: {att_by_tier[k]})")
    print(f"\nkola BEZ pokusu: {noatt_n} · z toho kolo ukončil TURNOVER: {noatt_turnover}")
    print("  dostupnost v kolech bez pokusu:")
    for k in sorted(noatt_tier):
        print(f"    {k:<30} {noatt_tier[k]:>4}  {noatt_tier[k]/noatt_n:>6.1%}")
    print("  co engine dělal místo sběru:")
    for k, v in noatt_action.most_common():
        print(f"    {k:<34} {v:>4}  {v/noatt_n:>6.1%}")
    print("  křížem (dostupnost × akce):")
    for (tr, ac), v in sorted(noatt_cross.items(), key=lambda kv: -kv[1]):
        print(f"    {tr} × {ac:<34} {v:>4}")

    print(f"\nS5.3 záloha (stojící soused míče na začátku kola): "
          f"{s5['backup_start']}/{n5} = {s5['backup_start']/n5:.1%}"
          f" · v kolech s pokusem: {s5['backup_start_attempt']}/{s5['attempt']}")
    if s5["covered_n"]:
        print(f"S5.4 nosič po úspěšném sběru KRYTÝ (nedosažitelný blitzem, "
              f"Chebyshev aproximace): {s5['covered_after']}/{s5['covered_n']}"
              f" = {s5['covered_after']/s5['covered_n']:.1%}")
    print(f"\nčistý dosah (tier A) drží nejlépe Runner: {s5['runner_best']}/{s5_tier.get('A čistý (0 dodge, 0 GFI)',0)} kol")


if __name__ == "__main__":
    sys.exit(main())
