#!/usr/bin/env python3
"""KDYŽ SE KLEC ROZPADNE — konkrétní kolo (20.08.2026)

Uživatel: „máme konkrétní kolo před a co se stalo a následek neúplná klec
v dalším kole? první a poslední kola vynech."

Hledá dvojici NAŠICH po sobě jdoucích kol, kde klec stála (>=2 rohy)
a v dalším našem kole má nosič rohů méně — a vypíše, co se mezi tím stalo.
"""
import sys, glob
sys.path.insert(0, "/home/jan/claude/bloodbowl")
import diag_rules_checks_20260812 as R
from collections import Counter

STANDING = 0
DIAG = [(-1, -1), (-1, 1), (1, -1), (1, 1)]


def corners(snap, side, car):
    occ = {(p["x"], p["y"]) for p in R.players(snap, side)
           if p["id"] != car["id"] and p["state"] == STANDING}
    return [(car["x"] + a, car["y"] + b) for a, b in DIAG
            if (car["x"] + a, car["y"] + b) in occ]


def main():
    st = Counter(); shown = 0
    for path in sorted(glob.glob(sys.argv[1])):
        r = R.load(path)
        ours = "home" if r.get("home_race") == "dwarf" else "away"
        theirs = "away" if ours == "home" else "home"
        logs = r["turn_logs"]
        idx = [i for i, S in enumerate(logs) if S["active_team"] == ours]
        for a, b in zip(idx, idx[1:]):
            A, B = logs[a], logs[b]
            if A["half"] != B["half"] or A.get("touchdown"): continue
            if A["turn"] <= 1 or B["turn"] >= 8: continue
            ca = next((p for p in R.players(A, ours) if p["has_ball"]), None)
            cb = next((p for p in R.players(B, ours) if p["has_ball"]), None)
            if ca is None or cb is None or ca["id"] != cb["id"]: continue
            na, nb = len(corners(A, ours, ca)), len(corners(B, ours, cb))
            if na < 2: continue
            st["dvojic, kde klec stála (≥2 rohy)"] += 1
            if nb >= na:
                st["  udržela se nebo zesílila"] += 1
                continue
            st["  ⛔ ROZPADLA SE"] += 1
            # ⭐ ROZDĚLENÍ PŘÍČINY (uživatel 20.08.): rozbil to soupeř, nebo
            # nosič odešel a klec za ním nedojela? Nosič se mezi našimi koly
            # hýbe JEN v našem kole A, takže posun A→B je NAŠE rozhodnutí.
            dxy = max(abs(cb["x"] - ca["x"]), abs(cb["y"] - ca["y"]))
            # padli/odsunuli soupeři někoho z rohů A?
            corner_ids = {p["id"] for p in R.players(A, ours)
                          if (p["x"], p["y"]) in corners(A, ours, ca)}
            hit = False
            for j in range(a + 1, b):
                M = logs[j]
                if M["active_team"] != theirs: continue
                for e in M["events"]:
                    if e["type"] in ("KNOCKED_DOWN", "PUSH") and e["player_id"] in corner_ids:
                        hit = True
            if dxy == 0 and not hit:
                st["    (1) nosič STÁL a rohy nikdo nesundal ⇒ rohy prostě odešly"] += 1
            elif dxy == 0 and hit:
                st["    (2) nosič STÁL, soupeř rohy sundal ⇒ SOUPEŘ"] += 1
            elif dxy > 0 and not hit:
                st["    (3) nosič ODEŠEL a rohy nešly s ním ⇒ MY"] += 1
                st["        ⌀ o kolik se nosič posunul ((3))"] += dxy
            else:
                st["    (4) nosič odešel I soupeř sundal ⇒ obojí"] += 1
            st[f"    ubylo rohů: {na}→{nb}"] += 1
            # co se stalo v soupeřově kole mezi A a B
            for j in range(a + 1, b):
                M = logs[j]
                if M["active_team"] != theirs: continue
                ids = {p["id"] for p in R.players(A, ours)}
                for e in M["events"]:
                    if e["type"] == "KNOCKED_DOWN" and e["player_id"] in ids:
                        st["      náš hráč SRAŽEN v jejich kole"] += 1
                    if e["type"] == "PUSH" and e["player_id"] in ids:
                        st["      náš hráč ODSUNUT"] += 1
            # a co dělal nosič
            moved = {e["player_id"] for e in A["events"]}
            if ca["id"] in moved: st["      nosič se v kole A hnul"] += 1
            if shown < 3 and nb == 0 and na >= 3:
                shown += 1
                print(f"\n--- PŘÍKLAD {shown}: {path.split('/')[-1]} "
                      f"půle {A['half']}, kolo {A['turn']} → {B['turn']} ---")
                print(f"  A: nosič {ca['name']} na ({ca['x']},{ca['y']}), rohů {na}")
                print(f"  B: nosič na ({cb['x']},{cb['y']}), rohů {nb}")
                for j in range(a + 1, b):
                    M = logs[j]
                    who = "MY" if M["active_team"] == ours else "SOUPEŘ"
                    ev = [f"{e['type']}#{e['player_id']}" for e in M["events"]
                          if e["type"] in ("BLOCK", "KNOCKED_DOWN", "PUSH", "BLITZ")]
                    print(f"    kolo {M['turn']} ({who}): {' '.join(ev[:12]) or '—'}")
    n = st["dvojic, kde klec stála (≥2 rohy)"]
    print(f"\n\ndvojic kol, kde klec stála: {n}")
    for k in sorted(st):
        if k.startswith("  "):
            print(f"  {k:48s} {st[k]:6d}  {100.0*st[k]/n:5.1f} %")


if __name__ == "__main__":
    main()
