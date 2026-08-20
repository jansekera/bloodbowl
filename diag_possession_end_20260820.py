#!/usr/bin/env python3
"""JAK KONČÍ NAŠE DRŽENÍ MÍČE (20.08.2026)

Uživatel 20.08.: *„takže v naší půli držíme míč 4 kola z 8 a neskórujeme —
umíme to popsat lépe? ztráta míče v důsledku blitz ballcarriera?"*

`diag_drive_failure_20260811.py` má kategorii C („ztratili jsme ho"), ale
**nerozpadá ji podle PŘÍČINY**. Tenhle skript dělá právě to.

Metoda: snímek `logs[i]` je ZAČÁTEK kola i. Držíme-li míč na snímku i a na
i+1 už ne, ztráta nastala BĚHEM kola i ⇒ příčina je v `logs[i]["events"]`
a `logs[i]["active_team"]` říká, čí to bylo kolo.

⚠️ **K32: v logu neexistuje událost BLITZ, jen BLOCK.** Rekonstrukce (18.08.):
blok, jehož útočník s cílem na ZAČÁTKU kola NESOUSEDIL, je blitz — blok
sousedství vyžaduje, blitz je jeden za kolo a tělo k cíli teprve dojde.
"""
import sys, glob
sys.path.insert(0, "/home/jan/claude/bloodbowl")
import diag_rules_checks_20260812 as R
from collections import Counter

STANDING = 0


def main():
    st = Counter()
    for path in sorted(glob.glob(sys.argv[1])):
        r = R.load(path)
        ours = "home" if r.get("home_race") == "dwarf" else "away"
        theirs = "away" if ours == "home" else "home"
        logs = r["turn_logs"]
        holding = False
        for i, S in enumerate(logs):
            car = next((p for p in R.players(S, ours) if p["has_ball"]), None)
            if car is None:
                holding = False
                continue
            if not holding:
                holding = True
                st["držení celkem"] += 1
            # konec půle / zápasu se drží míč => "došla kola"
            if i + 1 >= len(logs) or logs[i + 1]["half"] != S["half"]:
                st["  DOŠLA KOLA (drželi jsme na konci)"] += 1
                st["držení KONČÍ"] += 1
                holding = False
                continue
            if S.get("touchdown"):
                st["  TD"] += 1
                st["držení KONČÍ"] += 1
                holding = False
                continue
            nxt = next((p for p in R.players(logs[i + 1], ours) if p["has_ball"]), None)
            if nxt is not None:
                continue                      # držíme dál
            # --- ZTRÁTA: příčinu hledáme v událostech kola i ---
            st["držení KONČÍ"] += 1
            st["  ZTRÁTA MÍČE"] += 1
            holding = False
            ev = S["events"]
            cid = car["id"]
            mine = S["active_team"] == ours
            # srazili nosiče?
            kd = next((e for e in ev
                       if e["type"] == "KNOCKED_DOWN" and e["player_id"] == cid), None)
            if kd is not None:
                if mine:
                    # v NAŠEM kole: vlastní hod, nebo „both down" z našeho bloku
                    bad = next((e for e in ev
                                if e["player_id"] == cid and not e.get("success", True)
                                and e["type"] in ("DODGE", "GFI")), None)
                    st["    nosič SRAŽEN v NAŠEM kole — " +
                       (bad["type"] if bad else "blok / jiné")] += 1
                else:
                    # v JEJICH kole: kdo ho srazil a byl u něj už na začátku?
                    atk = next((e["player_id"] for e in ev
                                if e["type"] == "BLOCK" and e["target_id"] == cid), None)
                    if atk is None:
                        st["    nosič SRAŽEN v jejich kole — bez BLOCK události"] += 1
                    else:
                        a0 = next((p for p in R.players(S, theirs) if p["id"] == atk), None)
                        if a0 is None:
                            st["    nosič SRAŽEN v jejich kole — útočník neznámý"] += 1
                        else:
                            adj = max(abs(a0["x"] - car["x"]), abs(a0["y"] - car["y"])) <= 1
                            st["    nosič SRAŽEN v jejich kole — " +
                               ("BLOK (už sousedil)" if adj else "BLITZ (došel k němu)")] += 1
                continue
            # nosič nebyl sražen — jiné příčiny
            for t, name in (("DODGE", "neúspěšný DODGE nosiče"),
                            ("GFI", "neúspěšný GFI nosiče"),
                            ("CATCH", "neúspěšný CATCH"),
                            ("PASS", "neúspěšný PASS")):
                if any(e["type"] == t and e["player_id"] == cid
                       and not e.get("success", True) for e in ev):
                    st["    " + name] += 1
                    break
            else:
                st["    JINÉ (nosič nesražen, žádný neúspěšný hod)"] += 1

    n = st["držení KONČÍ"]
    print("držení míče celkem: %d   (konců: %d)\n" % (st["držení celkem"], n))
    for k in sorted(st):
        if k.startswith("  ") and not k.startswith("    "):
            print("%-46s %6d  %5.1f %%" % (k, st[k], 100.0 * st[k] / n))
    z = st["  ZTRÁTA MÍČE"]
    print("\n  rozpad ZTRÁT (jmenovatel = %d ztrát, ne konců):" % z)
    for k in sorted(st, key=lambda x: -st[x]):
        if k.startswith("    "):
            print("%-46s %6d  %5.1f %%" % (k, st[k], 100.0 * st[k] / z))


if __name__ == "__main__":
    main()
