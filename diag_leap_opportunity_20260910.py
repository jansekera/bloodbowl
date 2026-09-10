#!/usr/bin/env python3
"""
A1 LEAP — MĚŘENÍ PŘÍLEŽITOSTÍ (10.09.2026)

⚑ ZADÁNÍ UŽIVATELE (10.09.): „ať se měří jen přeskočení zdi — dostane se někam,
  kam jinak ne, a do klece na blitz nosiče."

⛔⛔ PROČ SE TO NEMĚŘÍ ZAPNUTÝM RAMENEM. `macro_actions.cpp:2494`: když je
  `leapWalkArm` ON, `movePlayerToward` se vrátí na STARÝ HLADOVÝ výběr
  (`findMoveToward`) místo nasazeného BFS (`nextStepToward`). Rameno tedy mění
  DVĚ věci naráz — leap jako kandidát + regrese pohybu — a jakýkoli běh s ním
  by byl nečitelný (táž vada, kvůli které se noc Q3 musela dělit na 16/17).
⇒ Tohle je proto měření PŘÍLEŽITOSTÍ, ne chování: ptá se, jak často situace,
  pro kterou ta dovednost je, na desce vůbec NASTANE. Na to rameno netřeba,
  a odpověď rozhoduje o nasazení: když příležitost není, nasazení nemůže nic
  změnit; když je (zvlášť průnik do klece), rameno se vyplatí dodělat.

⚠️ Dovednosti korpus NEEXPORTUJE (hráč nese jen id/x/y/state/has_ball/name/
  ma/st/ag/av) — Leap nositelé se proto poznávají PODLE JMÉNA: wood-elf
  `Wardancer *` (MA 8, AG 4), ověřeno v `roster.cpp:104` (Block+Dodge+Leap).

Definice leapu podle nabídky (`rules_engine.cpp:62-82`, pravidla ř. 8270-8283):
„jump to ANY EMPTY square" do Chebyshev vzdálenosti 2.

KLASIFIKACE každého legálního cíle leapu (a pak i celé aktivace):
  (A-tvrdé) cíl NENÍ dosažitelný chůzí vůbec — obklíčené pole, tj. „za zdí"
  (A-měkké) dosažitelný, ale JEN přes soupeřovy tacklezóny ⇒ chůze by stála
            dodge, leap stojí jeden hod na AG — u dvou a víc dodgí je to zisk
  (zdarma)  dosažitelný chůzí mimo TZ ⇒ leap by zaplatil hod ZA NIC
  (B)       cíl SOUSEDÍ se soupeřovým nosičem — „do klece na blitz nosiče"
⭐ ZBYTEK se tiskne vždy (feedback_take_counters_are_cumulative).

⭐ POZITIVNÍ KONTROLA je vevnitř: tiskne se průměrná velikost dosažitelné
  množiny. Kdyby BFS vracel prázdno, vypadalo by VŠECHNO jako „nedosažitelné"
  a nález by byl obrácený — nula i jednička se musí dát rozeznat.

Použití: python3 diag_leap_opportunity_20260910.py [korpus]
"""
import glob
import gzip
import json
import sys
from collections import Counter, deque

DATA = sys.argv[1] if len(sys.argv) > 1 else "corpus_m11_20260909_data"
W, H = 26, 15          # deska: x 1..26, y 1..15 (mirror diag_board_render)
DIRS = [(dx, dy) for dx in (-1, 0, 1) for dy in (-1, 0, 1) if dx or dy]


def cheb(a, b):
    return max(abs(a[0] - b[0]), abs(a[1] - b[1]))


def reachable(start, ma, blocked, forbidden=frozenset()):
    """Pole dosažitelná chůzí na `ma` kroků. `blocked` = obsazená pole,
    `forbidden` = pole, na která se nesmí vstoupit (u varianty 'bez TZ')."""
    seen = {start: 0}
    q = deque([start])
    while q:
        cur = q.popleft()
        d = seen[cur]
        if d >= ma:
            continue
        for dx, dy in DIRS:
            nxt = (cur[0] + dx, cur[1] + dy)
            if not (1 <= nxt[0] <= W and 1 <= nxt[1] <= H):
                continue
            if nxt in blocked or nxt in forbidden or nxt in seen:
                continue
            seen[nxt] = d + 1
            q.append(nxt)
    seen.pop(start, None)
    return set(seen)


def main():
    files = sorted(glob.glob(f"{DATA}/g*.json.gz"))
    if not files:
        sys.exit(f"žádná data v {DATA}")

    games = we_turns = acts = 0
    c = Counter()
    reach_sizes = []
    tgt_total = 0

    for f in files:
        g = json.load(gzip.open(f))
        if g["home_race"] == "wood-elf":
            us, them = "home", "away"
        elif g["away_race"] == "wood-elf":
            us, them = "away", "home"
        else:
            continue
        games += 1

        for t in g["turn_logs"]:
            if t["active_team"] != us:
                continue
            we_turns += 1
            mine = t[f"{us}_players"]
            theirs = t[f"{them}_players"]
            occ = {(p["x"], p["y"]) for p in mine + theirs if p["state"] != 1}
            stand_theirs = [p for p in theirs if p["state"] == 0]
            tz = {(x, y)
                  for e in stand_theirs
                  for dx, dy in DIRS
                  for x, y in [(e["x"] + dx, e["y"] + dy)]}
            carrier = next((p for p in theirs if p["has_ball"]), None)
            cpos = (carrier["x"], carrier["y"]) if carrier else None

            for p in mine:
                if not p["name"].startswith("Wardancer") or p["state"] != 0:
                    continue
                acts += 1
                me = (p["x"], p["y"])
                r_any = reachable(me, p["ma"], occ - {me})
                r_free = reachable(me, p["ma"], occ - {me}, forbidden=tz)
                reach_sizes.append(len(r_any))

                # legální cíle leapu: prázdná pole do vzdálenosti 2
                a_hard = a_soft = free = b_any = ab = 0
                for dx in range(-2, 3):
                    for dy in range(-2, 3):
                        d = (me[0] + dx, me[1] + dy)
                        if d == me or not (1 <= d[0] <= W and 1 <= d[1] <= H):
                            continue
                        if cheb(me, d) > 2 or d in occ:
                            continue
                        tgt_total += 1
                        isB = cpos is not None and cheb(d, cpos) == 1
                        if d not in r_any:
                            a_hard += 1
                            if isB:
                                ab += 1
                        elif d not in r_free:
                            a_soft += 1
                        else:
                            free += 1
                        if isB:
                            b_any += 1

                c["akt. s A-tvrdým cílem"] += bool(a_hard)
                c["akt. s A-měkkým cílem"] += bool(a_soft)
                c["akt. s cílem u NOSIČE (B)"] += bool(b_any)
                c["akt. s A-tvrdým U NOSIČE (A&B)"] += bool(ab)
                if not a_hard and not a_soft and not b_any:
                    c["akt. BEZ jakékoli příležitosti (ZBYTEK)"] += 1

    if not acts:
        sys.exit("⛔ žádná aktivace Wardancera — měřidlo nemá co číst")

    print(f"korpus {DATA}: {games} her s wood-elfem, {we_turns} jejich tahů")
    print(f"aktivací stojícího Wardancera: {acts} "
          f"({acts/games:.2f} na hru, {acts/we_turns:.2f} na tah)")
    print(f"legálních cílů leapu celkem: {tgt_total} ({tgt_total/acts:.1f} na aktivaci)")
    avg = sum(reach_sizes) / len(reach_sizes)
    print(f"\n⭐ POZITIVNÍ KONTROLA dosažitelnosti: průměrně {avg:.1f} polí "
          f"dosažitelných chůzí (min {min(reach_sizes)}, max {max(reach_sizes)})")
    print("   => kdyby tu byla nula, vypadalo by vsechno jako 'za zdi' a nalez by byl obraceny\n")

    print("PŘÍLEŽITOST NA AKTIVACI (podíl aktivací, kde takový cíl EXISTUJE)")
    print("-" * 66)
    order = ["akt. s A-tvrdým cílem", "akt. s A-měkkým cílem",
             "akt. s cílem u NOSIČE (B)", "akt. s A-tvrdým U NOSIČE (A&B)",
             "akt. BEZ jakékoli příležitosti (ZBYTEK)"]
    for k in order:
        print(f"  {k:42s} {c[k]:6d}  {100*c[k]/acts:5.1f} %")
    kryto = c[order[0]] + c[order[1]] + c[order[2]]
    print(f"\n  (A a B se PREKRYVAJI, soucet {kryto} neni 100 % - proto je zbytek")
    print("   veden zvlast jako 'ani jedno')")


if __name__ == "__main__":
    main()


# ============================================================================
# ⭐⭐⭐ DOPLNĚK (10.09.2026): KOLIK DODGÍ BY CHŮZE STÁLA
#
# ⛔ PROČ NESTAČÍ PRVNÍ MĚŘENÍ. „A-měkké" (dosažitelné, ale jen přes TZ) vyšlo
#   na 81 %, což nerozhoduje NIC: slévá „stálo by to JEDEN dodge" (leap je pak
#   skoro nula, sám je hod na AG) s „stálo by to PĚT dodgí" (leap je čistý zisk).
#   A `A&B` = 0 znamená, že pole u nosiče byla VŽDY nějak dochozí ⇒ otázka není
#   „dostane se tam?", ale „ZA KOLIK se tam dostane jinak".
# ⇒ Pro cíle U NOSIČE (B) se proto počítá MINIMÁLNÍ POČET DODGÍ přes všechny
#   chodící cesty do MA kroků. Dodge se platí za OPUŠTĚNÍ pole v cizí TZ
#   (ne za vstup), takže cena hrany u->v je 1, když `u` leží v TZ.
#   Lexikografický Dijkstra: nejdřív min dodgí, při rovnosti min kroků.
# ============================================================================
import heapq


def min_dodges_to(start, targets, ma, blocked, tz):
    """{cíl: (min_dodgí, kroky)} pro cíle dosažitelné do `ma` kroků."""
    best = {start: (0, 0)}
    pq = [(0, 0, start)]
    out = {}
    want = set(targets)
    while pq:
        dg, st, cur = heapq.heappop(pq)
        if best.get(cur, (99, 99)) < (dg, st):
            continue
        if cur in want:
            out[cur] = (dg, st)
            if len(out) == len(want):
                break
        if st >= ma:
            continue
        step_cost = 1 if cur in tz else 0
        for dx, dy in DIRS:
            nxt = (cur[0] + dx, cur[1] + dy)
            if not (1 <= nxt[0] <= W and 1 <= nxt[1] <= H) or nxt in blocked:
                continue
            cand = (dg + step_cost, st + 1)
            if cand < best.get(nxt, (99, 99)):
                best[nxt] = cand
                heapq.heappush(pq, (cand[0], cand[1], nxt))
    return out


def dodge_cost_report():
    files = sorted(glob.glob(f"{DATA}/g*.json.gz"))
    hist = Counter()
    acts_with_b = 0
    for f in files:
        g = json.load(gzip.open(f))
        if g["home_race"] == "wood-elf":
            us, them = "home", "away"
        elif g["away_race"] == "wood-elf":
            us, them = "away", "home"
        else:
            continue
        for t in g["turn_logs"]:
            if t["active_team"] != us:
                continue
            mine, theirs = t[f"{us}_players"], t[f"{them}_players"]
            carrier = next((p for p in theirs if p["has_ball"]), None)
            if carrier is None:
                continue
            cpos = (carrier["x"], carrier["y"])
            occ = {(p["x"], p["y"]) for p in mine + theirs if p["state"] != 1}
            tz = {(x, y) for e in theirs if e["state"] == 0
                  for dx, dy in DIRS for x, y in [(e["x"] + dx, e["y"] + dy)]}
            for p in mine:
                if not p["name"].startswith("Wardancer") or p["state"] != 0:
                    continue
                me = (p["x"], p["y"])
                bt = [(me[0] + dx, me[1] + dy)
                      for dx in range(-2, 3) for dy in range(-2, 3)
                      if (me[0] + dx, me[1] + dy) != me
                      and 1 <= me[0] + dx <= W and 1 <= me[1] + dy <= H
                      and (me[0] + dx, me[1] + dy) not in occ
                      and cheb(me, (me[0] + dx, me[1] + dy)) <= 2
                      and cheb((me[0] + dx, me[1] + dy), cpos) == 1]
                if not bt:
                    continue
                acts_with_b += 1
                res = min_dodges_to(me, bt, p["ma"], occ - {me}, tz)
                # nejlevnější z cílů u nosiče: to je ta skutečná alternativa
                if not res:
                    hist["nedosažitelné chůzí"] += 1
                else:
                    hist[min(d for d, _ in res.values())] += 1
    print("\n" + "=" * 66)
    print("CENA CHŮZE K POLI U NOSIČE (min. počet dodgí, nejlevnější takový cíl)")
    print(f"aktivací Wardancera, kde pole u nosiče v dosahu leapu EXISTUJE: {acts_with_b}")
    print("-" * 66)
    tot = sum(hist.values())
    for k in sorted(hist, key=lambda x: (isinstance(x, str), x)):
        lbl = f"{k} dodgí" if isinstance(k, int) else k
        verd = ("  <- leap NEMA smysl (C)" if k == 0 else
                "  <- prakticky vyrovnane" if k == 1 else
                "  <- LEAP JE ZISK" if isinstance(k, int) else "  <- leap je jedina cesta")
        print(f"  {lbl:24s} {hist[k]:6d}  {100*hist[k]/tot:5.1f} %{verd}")
    print(f"  {'ZBYTEK (musi byt 0)':24s} {tot - sum(hist.values()):6d}")


if __name__ == "__main__":
    dodge_cost_report()
