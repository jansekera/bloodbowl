#!/usr/bin/env python3
"""P5 — HAND-OFF: NABÍDKA vs VOLBA, na korpusu, který má opravu exportu (19.08.)

⛔ Do dneška se hand-off nedal měřit: `corpus_baseline_20260817_data` se sbíral
17.08. v 10:15, oprava exportu `HAND_OFF` (`c943e8b8`) je z 11:59 téhož dne,
takže hand-offy v něm leží jako `UNKNOWN`. `corpus_baseline_20260819_data` je
první korpus, který tu opravu má.

Otázka P5: nosič, který NENÍ Runner, a vedle stojí volný Runner -- předáme mu
míč? Runner nese 3,41 pole/kolo, Longbeard 1,50 (11.08.), takže je to výměna
nosiče, ne postup.
"""
import sys, glob
from collections import Counter
sys.path.insert(0, "/home/jan/claude/bloodbowl")
import diag_rules_checks_20260812 as R

STANDING = 0


def is_runner(p):
    return p["name"].startswith("Runner")


def run(paths, race="dwarf"):
    st = Counter()
    for path in paths:
        r = R.load(path)
        side_of = {}
        for s in ("home", "away"):
            nm = " ".join(p["name"] for p in r["turn_logs"][0][f"{s}_players"][:3])
            side_of[s] = "dwarf" if "Longbeard" in nm or "Troll Slayer" in nm else None
        ours = next((s for s, v in side_of.items() if v == race), None)
        if ours is None:
            continue
        st["her"] += 1
        logs = r["turn_logs"]
        for i, S in enumerate(logs):
            if S["active_team"] != ours:
                continue
            st["našich kol"] += 1
            us = R.players(S, ours)
            ids = {p["id"] for p in us}
            for e in S["events"]:
                if e["type"] == "HAND_OFF" and e["player_id"] in ids:
                    st["HAND_OFF zahraných (naše strana)"] += 1
                if e["type"] == "PASS" and e["player_id"] in ids:
                    st["PASS zahraných"] += 1

            car = next((p for p in us if p["has_ball"]), None)
            if car is None:
                continue
            st["kol s nosičem"] += 1
            if is_runner(car):
                st["nosič JE Runner"] += 1
                continue
            st["nosič NENÍ Runner"] += 1
            # volný stojící Runner v sousedství = situace, na kterou míří P5
            adj_runners = [p for p in us
                           if p["id"] != car["id"] and p["state"] == STANDING
                           and is_runner(p)
                           and R.adj((p["x"], p["y"]), (car["x"], car["y"]))]
            near_runners = [p for p in us
                            if p["id"] != car["id"] and p["state"] == STANDING
                            and is_runner(p)
                            and max(abs(p["x"] - car["x"]),
                                    abs(p["y"] - car["y"])) <= p["ma"]]
            if adj_runners:
                st["⭐ P5 SITUACE: nosič není Runner a Runner STOJÍ VEDLE"] += 1
                played = any(e["type"] == "HAND_OFF" and e["player_id"] == car["id"]
                             for e in S["events"])
                if played:
                    st["   z toho jsme předali"] += 1
            elif near_runners:
                st["Runner není vedle, ale DOJDE (do MA)"] += 1
            else:
                st["žádný volný Runner na dosah"] += 1
    return st


def main():
    st = run(sorted(glob.glob(sys.argv[1])))
    g = st["her"]
    print("korpus: %s\nher: %d, našich kol: %d, kol s nosičem: %d\n"
          % (sys.argv[1], g, st["našich kol"], st["kol s nosičem"]))
    print("  %-56s %7d   %6.3f/hru" % ("HAND_OFF zahraných (naše strana)",
          st["HAND_OFF zahraných (naše strana)"], st["HAND_OFF zahraných (naše strana)"] / g))
    print("  %-56s %7d   %6.3f/hru" % ("PASS zahraných", st["PASS zahraných"],
          st["PASS zahraných"] / g))
    n = st["kol s nosičem"]
    print("\n  jmenovatel = %d kol s nosičem" % n)
    for k in ["nosič JE Runner", "nosič NENÍ Runner"]:
        print("  %-56s %7d  %5.1f %%" % (k, st[k], 100.0 * st[k] / n))
    nn = st["nosič NENÍ Runner"]
    print("\n  jmenovatel = %d kol, kdy nosič NENÍ Runner" % nn)
    for k in ["⭐ P5 SITUACE: nosič není Runner a Runner STOJÍ VEDLE",
              "Runner není vedle, ale DOJDE (do MA)",
              "žádný volný Runner na dosah"]:
        print("  %-56s %7d  %5.1f %%" % (k, st[k], 100.0 * st[k] / nn if nn else 0))
    s5 = st["⭐ P5 SITUACE: nosič není Runner a Runner STOJÍ VEDLE"]
    print("\n  ⇒ v té situaci jsme předali %d z %d = %.1f %%  (%.3f/hru)"
          % (st["   z toho jsme předali"], s5,
             100.0 * st["   z toho jsme předali"] / s5 if s5 else 0, s5 / g))


if __name__ == "__main__":
    main()
