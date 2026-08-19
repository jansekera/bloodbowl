#!/usr/bin/env python3
"""P34 — druhá pasáž (Fable, 19.08.2026): co PŘESNĚ je k mání z kategorie (1a).

Pasáž 1 (diag_corner_gap_20260819.py) našla, že 50,2 % těl na ortogonálním
poli tam DOŠLO vlastním pohybem v tomtéž kole a 73,6 % z nich mělo čistý
prázdný roh hned vedle zvoleného cíle.  To je per-BODY číslo bez párování:
dvě těla si mohla nárokovat týž roh.  Tady se počítá:

  A) párování (1a)-redirect těl na čisté rohy (roh sousedí s cílem pohybu),
  B) SPOLEČNÉ párování (5č) + (1a) těl — celkový počet rohů, které jde
     získat POUHOU ZMĚNOU VOLBY (krok navíc u nehravšího těla, jiné cílové
     pole u táhnuvšího), rohů/kolo i rohů/zápas,
  C) MA-rezerva (1a) těl: kroků použito (počet MOVE událostí) vs MA —
     kolik z nich mělo rezervu ≥1 pole (postačující podmínka, že redirect
     nestál ani GFI),
  D) σ kontrola vyhladovění: PRAVIDLO jako „aspoň jednou v drivu" (ano/ne)
     metodikou P30 — jestli 0,0σ z pasáže 1 není jen nulový rozptyl
     per-drive průměru vzácné události.

⛔ Jen čtení korpusu.  nice -n 19.
"""
import sys, glob, math
from collections import Counter, defaultdict

sys.path.insert(0, "/home/jan/claude/bloodbowl")
from diag_rules_checks_20260812 import (load, players, threatens, adj, STANDING)
from diag_drive_failure_20260811 import td_scorer_side, build_id_map
from diag_corner_gap_20260819 import (ORTH, DIAG, on_pitch, cheb, max_matching)


def main():
    paths = sorted(glob.glob(sys.argv[1]))
    st = Counter()
    drive_rows = []
    ma_slack = Counter()

    for gi, path in enumerate(paths):
        r = load(path)
        ours = "home" if r.get("home_race") == "dwarf" else "away"
        theirs = "away" if ours == "home" else "home"
        logs = r["turn_logs"]
        id_map = build_id_map(r)

        starts = [0]
        for i in range(1, len(logs)):
            if logs[i]["half"] != logs[i - 1]["half"] or logs[i - 1].get("touchdown"):
                starts.append(i)
        starts.append(len(logs))
        for si in range(len(starts) - 1):
            a, b = starts[si], starts[si + 1]
            ours_turns = [i for i in range(a, b) if logs[i]["active_team"] == ours]
            if len(ours_turns) < 7:
                continue
            scored = any(td_scorer_side(logs[i], id_map) == ours for i in range(a, b))
            vals = []
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
                extra_any = ([o for o in orth if o in occ_us]
                             + [o for o in orth if o in occ_them]
                             + [d for d in diag if d in occ_them])
                vals.append(1.0 if (len(filled) == 4 and not dirty
                                    and not extra_any) else 0.0)
            if vals:
                drive_rows.append((gi, scored, sum(vals) / len(vals),
                                   1.0 if any(vals) else 0.0))

        # ── párování (1a) + (5č) ────────────────────────────────────
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
            st["n kol s míčem"] += 1
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
            played = {e["player_id"] for e in S.get("events", [])}
            threat = [(p["x"], p["y"]) for p in them if threatens(p)]
            standing_them = [p for p in them if p["state"] == STANDING]
            avail = [d for d in empty_diag if on_pitch(d)]
            avail_clean = [d for d in avail if not any(adj(d, t) for t in threat)]
            occ_all = set(occ_us) | set(occ_them) | {(cx, cy)}

            def free_reach(bpos, corner):
                d = cheb(bpos, corner)
                if d == 1:
                    return True
                if d == 2:
                    for sx in range(min(bpos[0], corner[0]), max(bpos[0], corner[0]) + 1):
                        for sy in range(min(bpos[1], corner[1]), max(bpos[1], corner[1]) + 1):
                            s = (sx, sy)
                            if s in (bpos, corner) or not on_pitch(s):
                                continue
                            if cheb(s, bpos) == 1 and cheb(s, corner) == 1 \
                               and s not in occ_all \
                               and not any(adj(s, t) for t in threat):
                                return True
                return False

            bodies = []   # (druh, [rohy])
            for p in extra_ours:
                bpos = (p["x"], p["y"])
                if p["id"] in played:
                    last_pos_ev = None
                    for e in S.get("events", []):
                        if e["player_id"] == p["id"] and "to_x" in e:
                            last_pos_ev = e
                    if (last_pos_ev and last_pos_ev["type"] == "MOVE"
                            and (last_pos_ev["to_x"], last_pos_ev["to_y"]) == bpos):
                        cs = [c for c in avail_clean if cheb(c, bpos) == 1]
                        if cs:
                            bodies.append(("1a", cs))
                            steps = sum(1 for e in S["events"]
                                        if e["player_id"] == p["id"]
                                        and e["type"] == "MOVE")
                            ma_slack[min(3, max(-1, p["ma"] - steps))] += 1
                    continue
                if p["state"] != STANDING:
                    continue
                if any(adj(bpos, (o["x"], o["y"])) for o in standing_them):
                    continue
                if not avail:
                    continue
                cs = [c for c in avail_clean if free_reach(bpos, c)]
                if cs:
                    bodies.append(("5c", cs))

            for kinds, label in ((("1a",), "(1a) redirect samostatně"),
                                 (("5c",), "(5č) samostatně"),
                                 (("1a", "5c"), "SPOLEČNĚ (1a)+(5č)")):
                sel = [(k, cs) for k, cs in bodies if k in kinds]
                if not sel:
                    continue
                corners = sorted({c for _, cs in sel for c in cs})
                cidx = {c: j for j, c in enumerate(corners)}
                edges = {bi: [cidx[c] for c in cs] for bi, (_, cs) in enumerate(sel)}
                m, _ = max_matching(edges, len(sel), len(corners))
                st[f"rohů: {label}"] += m
                st[f"kol s ≥1: {label}"] += 1 if m else 0

    n = st["n kol s míčem"]
    G = len(paths)
    print(f"korpus: {G} her · n = {n} kol s míčem\n")
    for label in ("(1a) redirect samostatně", "(5č) samostatně",
                  "SPOLEČNĚ (1a)+(5č)"):
        m = st[f"rohů: {label}"]
        print(f"  {label:<28} {m:6d} rohů = {m/n:.3f}/kolo = {m/G:.2f}/zápas"
              f"   (kol s ≥1: {st[f'kol s ≥1: {label}']})")

    print(f"\n  MA-rezerva (1a)-redirect těl (MA − použité kroky):")
    tot = sum(ma_slack.values())
    for k in sorted(ma_slack):
        lbl = f"{k}+" if k == 3 else str(k)
        print(f"    rezerva {lbl:>3}: {ma_slack[k]:6d}  {100*ma_slack[k]/tot:5.1f} %")
    print(f"    (n = {tot} těl; rezerva ≥1 = redirect jistě nestál ani GFI)")

    def sigma(vals_yes, vals_no):
        my, mn = sum(vals_yes) / len(vals_yes), sum(vals_no) / len(vals_no)
        vy = sum((x - my) ** 2 for x in vals_yes) / max(1, len(vals_yes) - 1)
        vn = sum((x - mn) ** 2 for x in vals_no) / max(1, len(vals_no) - 1)
        se = math.sqrt(vy / len(vals_yes) + vn / len(vals_no))
        return my, mn, (my - mn) / se if se else 0.0

    print(f"\nσ kontrola vyhladovění (drivů: {len(drive_rows)}, "
          f"TD: {sum(1 for _, s, _, _ in drive_rows if s)}):")
    for idx, name in ((2, "PRAVIDLO per-drive průměr (replika pasáže 1)"),
                      (3, "PRAVIDLO aspoň jednou v drivu (ano/ne)")):
        yes = [r[idx] for r in drive_rows if r[1]]
        no = [r[idx] for r in drive_rows if not r[1]]
        my, mn, sg = sigma(yes, no)
        print(f"  {name:<48} {my:.3f} vs {mn:.3f} → {sg:+.1f}σ")
        for half, hn in ((0, "A"), (1, "B")):
            yh = [r[idx] for r in drive_rows if r[1] and r[0] % 2 == half]
            nh = [r[idx] for r in drive_rows if not r[1] and r[0] % 2 == half]
            my, mn, sg = sigma(yh, nh)
            print(f"      půlka {hn}: {my:.3f} vs {mn:.3f} → {sg:+.1f}σ")


if __name__ == "__main__":
    main()
