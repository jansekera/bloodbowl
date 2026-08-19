#!/usr/bin/env python3
"""P34 — CO BRÁNÍ ČTVRTÉMU ROHU (Fable, 19.08.2026).

Vstup: 52,4 % našich kol s míčem má zároveň (a) chybějící roh klece a
(b) naše tělo ORTOGONÁLNĚ u nosiče — na poli, které pravidlo K29⭐⭐
zakazuje, jedno pole od prázdného rohu.  Strop 0,73 rohu/kolo.

Tenhle skript strop ROZEBÍRÁ NA PŘÍČINY.  Každé ortogonální tělo v každém
kvalifikujícím kole padne do PRÁVĚ JEDNÉ kategorie (kaskáda (1)→(5)):

  (1)  už hrálo            — má v kole vlastní událost ⇒ rozpočet kola pryč
  (0)  leží/omráčeno       — nehrálo, ale nestojí (brief ji nevyjmenovává;
                            bez ní by kaskáda ležící tělo tiše pustila do (5))
  (2)  markuje DRAZE       — sousedí se stojícím soupeřem, jehož únik něco
                            stojí (dodge_cost ≥ 0,20) ⇒ R3 drží roh přebije
  (3)  zamčené (levné mark.) — sousedí se stojícím soupeřem, ale VŠECHNY úniky
                            jsou levné ⇒ R3 podle K30b roh NEpřebije, jenže
                            tělo samo stojí v TZ a krok do rohu stojí NÁŠ dodge
       ⚠️ geometrická poznámka: „markuje" a „je v TZ" je u stojícího soupeře
       TENTÝŽ predikát (sousedství).  Čisté (3) bez (2) je proto prázdná množina
       KONSTRUKCÍ, ne měřením — kaskáda dělí podle toho, ČÍ dodge je drahý:
       (2) = soupeřův (povinnost), (3) = jen náš (cena kroku).
  (4)  roh není k mání      — žádný prázdný roh na hřišti (obsazené soupeřem /
                            mimo hřiště; 15.5 lajna)
  (4b) roh je, ale krok stojí hod / cesta blokovaná — volné stojící tělo,
                            prázdný roh existuje, ale 2-krokový přesun vede
                            jen přes TZ nebo obsazená pole
  (5)  NIC TOMU NEBRÁNILO   — stojící, nehrálo, nemarkuje, není v TZ, prázdný
                            roh dosažitelný BEZ HODU; dělí se na
                            (5)č (dosažitelný ČISTÝ roh — jediné, co pravidlu
                            pomáhá) a (5)š (jen ŠPINAVÝ roh — krok je zdarma,
                            ale vyrobí roh −6,8σ ⇒ NENÍ zadarmo věcně).

Turn-level: max. párování (5)č-těl na čisté prázdné rohy ⇒ rohů/kolo a
rohů/zápas SKUTEČNĚ zadarmo (to je číslo pro rozhodnutí o implementaci).

Cena kroku: pro každé kolo s párováním se těla PŘESUNOU do rohů v kopii
snímku a přepočítá se REACH0/FB2 (E1/E2, importovaná jediná definice).

σ: K29rule jako prediktor skórujícího drivu, metodika P30 doslova
(diag_drive_predictors_20260813) vč. filtru plných drivů; kotvy K9a a
rohů_ŠPINAVÝCH musí reprodukovat tabulku 18.08., jinak σ nečíst.

⛔ Jen čtení korpusu; žádný engine.  Pouštět s nice -n 19.
"""
import sys, glob, math
from collections import Counter, defaultdict

sys.path.insert(0, "/home/jan/claude/bloodbowl")
from diag_rules_checks_20260812 import (load, players, threatens, adj, STANDING,
                                        key, TACKLE, dodge_cost,
                                        DODGE_COST_THRESHOLD)
from diag_exposure_scan_20260812 import Board, predictors
from diag_drive_failure_20260811 import td_scorer_side, build_id_map

ORTH = [(-1, 0), (1, 0), (0, -1), (0, 1)]
DIAG = [(-1, -1), (-1, 1), (1, -1), (1, 1)]
PITCH_W, PITCH_H = 26, 15   # engine/include/bb/position.h


def on_pitch(s):
    return 0 <= s[0] < PITCH_W and 0 <= s[1] < PITCH_H


def cheb(a, b):
    return max(abs(a[0] - b[0]), abs(a[1] - b[1]))


def max_matching(edges, n_bodies, n_corners):
    """Max. bipartitní párování (≤4×4) — Kuhn."""
    match_c = {}

    def try_body(b, seen):
        for c in edges.get(b, ()):
            if c in seen:
                continue
            seen.add(c)
            if c not in match_c or try_body(match_c[c], seen):
                match_c[c] = b
                return True
        return False

    cnt = 0
    for b in range(n_bodies):
        if try_body(b, set()):
            cnt += 1
    return cnt, {b: c for c, b in match_c.items()}


def main():
    paths = sorted(glob.glob(sys.argv[1]))
    st = Counter()
    dodge_costs_step = []          # náš dodge v (3) (cena kroku, kdyby se šel)
    drive_rows = []                # (game_idx, scored, {metrika: mean})
    reach_delta = Counter()
    fb2_delta = Counter()
    reach_pairs = []               # (REACH0 před, po)
    marked_after = 0               # (5č) tělo, které by v rohu NOVĚ markovalo
    screen_proxy = 0               # (5č) tělo se stojícím soupeřem do 2 polí
    matched_total = 0

    for gi, path in enumerate(paths):
        r = load(path)
        ours = "home" if r.get("home_race") == "dwarf" else "away"
        theirs = "away" if ours == "home" else "home"
        fwd = 1 if ours == "home" else -1
        endzone = 25 if fwd == 1 else 0
        logs = r["turn_logs"]
        id_map = build_id_map(r)

        # ── hranice drivů (doslova P30) ─────────────────────────────
        starts = [0]
        for i in range(1, len(logs)):
            if logs[i]["half"] != logs[i - 1]["half"] or logs[i - 1].get("touchdown"):
                starts.append(i)
        starts.append(len(logs))

        for si in range(len(starts) - 1):
            a, b = starts[si], starts[si + 1]
            ours_turns = [i for i in range(a, b) if logs[i]["active_team"] == ours]
            if len(ours_turns) < 7:      # jen PLNÉ drivy (výběr P30, vědomě)
                continue
            scored = any(td_scorer_side(logs[i], id_map) == ours for i in range(a, b))
            acc = defaultdict(list)
            for i in ours_turns:
                if i + 1 >= len(logs) or logs[i + 1]["half"] != logs[i]["half"]:
                    continue
                E = logs[i + 1]
                us = players(E, ours)
                them = players(E, theirs)
                car = next((p for p in us if p["has_ball"]), None)
                if car is None:
                    continue
                cx, cy = car["x"], car["y"]
                diag = [(cx + dx, cy + dy) for dx, dy in DIAG]
                orth = [(cx + dx, cy + dy) for dx, dy in ORTH]
                occ_us = {(p["x"], p["y"]): p for p in us if p["id"] != car["id"]}
                occ_them = {(p["x"], p["y"]): p for p in them}
                filled = [d for d in diag if d in occ_us]
                threat = [(p["x"], p["y"]) for p in them if threatens(p)]
                dirty = [d for d in filled if any(adj(d, t) for t in threat)]
                acc["rohů_všech"].append(float(len(filled)))
                acc["rohů_ČISTÝCH"].append(float(len(filled) - len(dirty)))
                acc["rohů_ŠPINAVÝCH"].append(float(len(dirty)))
                extra_any = ([o for o in orth if o in occ_us]
                             + [o for o in orth if o in occ_them]
                             + [d for d in diag if d in occ_them])
                rule = (len(filled) == 4) and not dirty and not extra_any
                acc["PRAVIDLO_K29rule"].append(1.0 if rule else 0.0)
                turns_left = 9 - logs[i]["turn"]
                carS = next((p for p in players(logs[i], ours) if p["has_ball"]), None)
                if turns_left > 0 and carS and carS["id"] == car["id"]:
                    need = math.ceil(abs(endzone - carS["x"]) / turns_left)
                    got = (car["x"] - carS["x"]) * fwd
                    acc["K9a_splněno"].append(1.0 if got >= need else 0.0)
            if acc:
                drive_rows.append((gi, scored,
                                   {k: sum(v) / len(v) for k, v in acc.items() if v}))

        # ── rozklad kategorií (podmínky doslova diag_cage_rule) ─────
        for i, S in enumerate(logs):
            if S["active_team"] != ours or i + 1 >= len(logs):
                continue
            E = logs[i + 1]
            if S.get("touchdown") or E["half"] != S["half"]:
                continue
            us = players(E, ours)
            them = players(E, theirs)
            car = next((p for p in us if p["has_ball"]), None)
            if car is None:
                continue
            st["kol s míčem (jmenovatel n)"] += 1
            cx, cy = car["x"], car["y"]
            diag = [(cx + dx, cy + dy) for dx, dy in DIAG]
            orth = [(cx + dx, cy + dy) for dx, dy in ORTH]
            occ_us = {(p["x"], p["y"]): p for p in us if p["id"] != car["id"]}
            occ_them = {(p["x"], p["y"]): p for p in them}
            filled = [d for d in diag if d in occ_us]
            empty_diag = [d for d in diag if d not in occ_us and d not in occ_them]
            extra_ours = [occ_us[o] for o in orth if o in occ_us]
            if not (len(filled) < 4 and empty_diag and extra_ours):
                continue
            st["kvalifikující kola (52,4 %)"] += 1
            st["strop (min(prázdné rohy, těla)) — replika 0,73"] += \
                min(len(empty_diag), len(extra_ours))

            played = {e["player_id"] for e in S.get("events", [])}
            threat = [(p["x"], p["y"]) for p in them if threatens(p)]
            standing_them = [p for p in them if p["state"] == STANDING]
            avail = [d for d in empty_diag if on_pitch(d)]
            avail_clean = [d for d in avail
                           if not any(adj(d, t) for t in threat)]
            occ_all = set(occ_us) | set(occ_them) | {(cx, cy)}

            def free_reach(bpos, corner):
                """Dojde tělo (stojící, mimo TZ) do rohu BEZ HODU?"""
                d = cheb(bpos, corner)
                if d == 1:
                    return True
                if d == 2:
                    for sx in range(min(bpos[0], corner[0]), max(bpos[0], corner[0]) + 1):
                        for sy in range(min(bpos[1], corner[1]), max(bpos[1], corner[1]) + 1):
                            s = (sx, sy)
                            if s in ((bpos), (corner)) or not on_pitch(s):
                                continue
                            if cheb(s, bpos) == 1 and cheb(s, corner) == 1 \
                               and s not in occ_all \
                               and not any(adj(s, t) for t in threat):
                                return True
                return False

            cat5_bodies = []     # (player, [dosažitelné čisté rohy])
            for p in extra_ours:
                bpos = (p["x"], p["y"])
                st["těl celkem (jmenovatel B)"] += 1
                if p["id"] in played:
                    # (1)a: DOŠLO na ortogonální pole vlastním pohybem v TOMHLE
                    # kole — poslední poziční událost hráče je MOVE končící
                    # na bpos.  To není „rozpočet pryč": táž akce mohla za
                    # stejnou cenu skončit V ROHU.  Vada volby CÍLOVÉHO POLE.
                    last_pos_ev = None
                    for e in S.get("events", []):
                        if e["player_id"] == p["id"] and "to_x" in e:
                            last_pos_ev = e
                    if (last_pos_ev and last_pos_ev["type"] == "MOVE"
                            and (last_pos_ev["to_x"], last_pos_ev["to_y"]) == bpos):
                        st["(1a) hrálo: DOŠLO SEM pohybem (vada cílového pole)"] += 1
                        if any(c for c in avail_clean if cheb(c, bpos) == 1):
                            st["(1a)… a ČISTÝ prázdný roh je hned vedle cíle"] += 1
                    else:
                        st["(1b) hrálo jinak (blok/blitz/stálo a jednalo)"] += 1
                    continue
                if p["state"] != STANDING:
                    st["(0) leží/omráčeno (a nehrálo)"] += 1
                    continue
                markers = [o for o in standing_them
                           if adj(bpos, (o["x"], o["y"]))]
                if markers:
                    if any(dodge_cost(o, p) >= DODGE_COST_THRESHOLD for o in markers):
                        st["(2) markuje DRAZE (R3 platí)"] += 1
                    else:
                        st["(3) zamčené — markuje jen LEVNĚ (K30b: roh nemá přebít)"] += 1
                        dodge_costs_step.append(dodge_cost(p, markers[0]))
                    continue
                if not avail:
                    st["(4) roh není k mání (obsazen soupeřem / mimo hřiště)"] += 1
                    continue
                clean_r = [c for c in avail_clean if free_reach(bpos, c)]
                dirty_r = [c for c in avail if c not in avail_clean
                           and free_reach(bpos, c)]
                if clean_r:
                    st["(5č) NIC NEBRÁNILO — dosažitelný ČISTÝ roh"] += 1
                    cat5_bodies.append((p, clean_r))
                    if any(cheb(bpos, (o["x"], o["y"])) <= 2 for o in standing_them):
                        screen_proxy += 1
                elif dirty_r:
                    st["(5š) krok zdarma, ale jen do ŠPINAVÉHO rohu (−6,8σ)"] += 1
                else:
                    st["(4b) roh je, ale krok stojí hod / cesta blokovaná"] += 1

            # ── turn-level: kolik ČISTÝCH rohů se v TOMHLE kole složí ──
            if cat5_bodies:
                corners = sorted({c for _, cs in cat5_bodies for c in cs})
                cidx = {c: j for j, c in enumerate(corners)}
                edges = {bi: [cidx[c] for c in cs]
                         for bi, (_, cs) in enumerate(cat5_bodies)}
                m, assign = max_matching(edges, len(cat5_bodies), len(corners))
                st["kol s ≥1 rohem zadarmo"] += 1
                st["(5) rohů ZADARMO (párování)"] += m
                matched_total += m

                # cena kroku: přesuň spárovaná těla, přepočti REACH0/FB2
                new_pl = []
                moved_ids = {}
                for bi, ci in assign.items():
                    moved_ids[cat5_bodies[bi][0]["id"]] = corners[ci]
                for p in E[f"{ours}_players"]:
                    if p["id"] in moved_ids:
                        q = dict(p)
                        q["x"], q["y"] = moved_ids[p["id"]]
                        new_pl.append(q)
                    else:
                        new_pl.append(p)
                E2 = dict(E)
                E2[f"{ours}_players"] = new_pl
                P0 = predictors(Board(E, ours))
                P1 = predictors(Board(E2, ours))
                if "REACH0" in P0 and "REACH0" in P1:
                    dr = P1["REACH0"] - P0["REACH0"]
                    reach_delta[max(-3, min(3, dr))] += 1
                    reach_pairs.append((P0["REACH0"], P1["REACH0"]))
                df = P1["FB2"] - P0["FB2"]
                fb2_delta[max(-3, min(3, df))] += 1
                # markoval by nově? (u čistého rohu z definice ne — kontrola)
                for pid, c in moved_ids.items():
                    if any(adj(c, t) for t in threat):
                        marked_after += 1

    # ═════════ výstup ═════════
    n = st["kol s míčem (jmenovatel n)"]
    Q = st["kvalifikující kola (52,4 %)"]
    B = st["těl celkem (jmenovatel B)"]
    G = len(paths)
    print(f"korpus: {G} her · n = {n} našich kol s míčem · "
          f"Q = {Q} kvalifikujících kol ({100*Q/n:.1f} %) · B = {B} těl")
    print(f"replika stropu: {st['strop (min(prázdné rohy, těla)) — replika 0,73']}"
          f" rohů = {st['strop (min(prázdné rohy, těla)) — replika 0,73']/n:.2f}/kolo\n")

    cats = ["(1a) hrálo: DOŠLO SEM pohybem (vada cílového pole)",
            "(1b) hrálo jinak (blok/blitz/stálo a jednalo)",
            "(0) leží/omráčeno (a nehrálo)",
            "(2) markuje DRAZE (R3 platí)",
            "(3) zamčené — markuje jen LEVNĚ (K30b: roh nemá přebít)",
            "(4) roh není k mání (obsazen soupeřem / mimo hřiště)",
            "(4b) roh je, ale krok stojí hod / cesta blokovaná",
            "(5č) NIC NEBRÁNILO — dosažitelný ČISTÝ roh",
            "(5š) krok zdarma, ale jen do ŠPINAVÉHO rohu (−6,8σ)"]
    tot = 0
    print(f"ROZKLAD TĚL (jmenovatel B = {B}):")
    for c in cats:
        v = st[c]
        tot += v
        print(f"  {c:<58} {v:6d}  {100*v/B:5.1f} %")
    assert tot == B, f"kaskáda nepokrývá všechna těla: {tot} != {B}"
    print(f"  {'SOUČET (kontrola úplnosti kaskády)':<58} {tot:6d}  100.0 %")
    v1a = st["(1a) hrálo: DOŠLO SEM pohybem (vada cílového pole)"]
    v1ac = st["(1a)… a ČISTÝ prázdný roh je hned vedle cíle"]
    print(f"  pod-množina (1a): ČISTÝ prázdný roh hned vedle zvoleného cíle: "
          f"{v1ac} z {v1a} ({100*v1ac/max(1,v1a):.1f} %)")
    if dodge_costs_step:
        print(f"\n  (3): ⌀ P(selhání NAŠEHO dodge při kroku do rohu) "
              f"= {sum(dodge_costs_step)/len(dodge_costs_step):.2f} "
              f"(n={len(dodge_costs_step)})")

    m5 = st["(5) rohů ZADARMO (párování)"]
    print(f"\n(5) TURN-LEVEL (párování (5č)-těl na čisté prázdné rohy):")
    print(f"  kol s ≥1 rohem zadarmo: {st['kol s ≥1 rohem zadarmo']}"
          f"  ({100*st['kol s ≥1 rohem zadarmo']/n:.1f} % z n, "
          f"{100*st['kol s ≥1 rohem zadarmo']/Q:.1f} % z Q)")
    print(f"  (5) rohů zadarmo: {m5}  =  {m5/n:.3f} rohu/kolo  =  {m5/G:.2f} rohu/zápas")

    print(f"\nCENA KROKU (přepočet E1/E2 po přesunu spárovaných těl; "
          f"kol s přesunem n={matched_total and st['kol s ≥1 rohem zadarmo']}):")
    nn = sum(reach_delta.values())
    print(f"  ΔREACH0 (soupeřů s dodge-free cestou k nosiči), n={nn}:")
    for d in sorted(reach_delta):
        print(f"    {d:+d}: {reach_delta[d]:6d}  {100*reach_delta[d]/nn:5.1f} %")
    nf = sum(fb2_delta.values())
    print(f"  ΔFB2, n={nf}:")
    for d in sorted(fb2_delta):
        print(f"    {d:+d}: {fb2_delta[d]:6d}  {100*fb2_delta[d]/nf:5.1f} %")
    print(f"  (5č) tělo, které by v rohu NOVĚ markovalo: {marked_after} "
          f"(z {matched_total}; u čistého rohu musí být 0 — kontrola definice)")
    print(f"  (5č) tělo se stojícím soupeřem do 2 polí (screen-proxy): "
          f"{screen_proxy} z {st['(5č) NIC NEBRÁNILO — dosažitelný ČISTÝ roh']}")

    # ═════════ σ (metodika P30) ═════════
    print(f"\nσ-METODIKA P30: plné drivy (≥7 našich kol): {len(drive_rows)}, "
          f"z toho se skórováním {sum(1 for _, s, _ in drive_rows if s)}")

    def sigma(rows, k):
        yes = [d[k] for _, s, d in rows if s and k in d]
        no = [d[k] for _, s, d in rows if not s and k in d]
        if len(yes) < 5 or len(no) < 5:
            return None
        my, mn = sum(yes) / len(yes), sum(no) / len(no)
        vy = sum((x - my) ** 2 for x in yes) / max(1, len(yes) - 1)
        vn = sum((x - mn) ** 2 for x in no) / max(1, len(no) - 1)
        se = math.sqrt(vy / len(yes) + vn / len(no))
        return my, mn, (my - mn) / se if se else 0.0, len(yes) + len(no)

    keys = ["PRAVIDLO_K29rule", "K9a_splněno", "rohů_ŠPINAVÝCH",
            "rohů_všech", "rohů_ČISTÝCH"]
    print(f"  {'veličina':<20}{'TD drivy':>10}{'bez TD':>10}{'σ':>8}{'n drivů':>9}")
    for k in keys:
        r_ = sigma(drive_rows, k)
        if r_:
            my, mn, sg, nd = r_
            print(f"  {k:<20}{my:>10.3f}{mn:>10.3f}{sg:>7.1f}σ{nd:>9}")
    print("  (kotvy K9a 20,7σ · špinavé −6,8σ · všech −2,1σ · čistých −0,2σ "
          "musí sedět s tabulkou 18.08.)")
    print("\n  test stability — půlky korpusu (sudé/liché hry):")
    for half, name in ((0, "A (sudé)"), (1, "B (liché)")):
        rows_h = [r_ for r_ in drive_rows if r_[0] % 2 == half]
        r_ = sigma(rows_h, "PRAVIDLO_K29rule")
        if r_:
            my, mn, sg, nd = r_
            print(f"    půlka {name}: PRAVIDLO {my:.3f} vs {mn:.3f} → {sg:+.1f}σ (n={nd})")


if __name__ == "__main__":
    main()
