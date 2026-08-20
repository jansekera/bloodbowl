#!/usr/bin/env python3
"""ŘETĚZ „NOSIČ KONČÍ KOLO V KONTAKTU" (20.08.2026)

Uživatel 20.08.: *„mi přijde jako větší průšvih block na nosiče — ale ten je
méně častý — ale já mám za to, že se nesmí stávat vůbec."*

⭐ **Proč je blok horší než blitz, ačkoli je vzácnější.** Blok vyžaduje, aby
útočník u nosiče UŽ STÁL na začátku svého kola. To znamená, že jsme SVOJE
kolo ukončili s nepřítelem vedle míče — je to **naše chyba, ne jeho zásluha**.
Blitz stojí soupeře jeho jedinou blitz akci za kolo; blok je **zadarmo**.

Řetěz se proto měří celý:
    (1) naše kolo skončí s nosičem v kontaktu   = EXPOZICE (náš zákaz)
    (2) soupeř na něj v příštím kole udeří BLOKEM
    (3) přijdeme o míč
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
        for i, S in enumerate(logs):
            if S["active_team"] != ours or i + 1 >= len(logs):
                continue
            E = logs[i + 1]                 # začátek soupeřova kola = konec našeho
            if E["half"] != S["half"] or S.get("touchdown"):
                continue
            car = next((p for p in R.players(E, ours) if p["has_ball"]), None)
            if car is None or car["state"] != STANDING:
                continue
            st["naše kola končící s nosičem"] += 1
            nb = [p for p in R.players(E, theirs)
                  if p["state"] == STANDING
                  and max(abs(p["x"] - car["x"]), abs(p["y"] - car["y"])) <= 1]
            if not nb:
                st["  nosič ČISTÝ (zákaz dodržen)"] += 1
                continue
            st["  ⛔ EXPOZICE — nosič končí v kontaktu"] += 1
            st["  ⛔ sousedů celkem"] += len(nb)
            if E["active_team"] != theirs:
                continue
            nbid = {p["id"] for p in nb}
            hit = any(e["type"] == "BLOCK" and e["target_id"] == car["id"]
                      and e["player_id"] in nbid for e in E["events"])
            if not hit:
                st["    soupeř NEUDEŘIL"] += 1
                continue
            st["    soupeř UDEŘIL blokem"] += 1
            down = any(e["type"] == "KNOCKED_DOWN" and e["player_id"] == car["id"]
                       for e in E["events"])
            if down:
                st["      nosič SRAŽEN"] += 1
            if i + 2 < len(logs) and logs[i + 2]["half"] == E["half"]:
                still = next((p for p in R.players(logs[i + 2], ours)
                              if p["has_ball"] and p["id"] == car["id"]), None)
                if still is None:
                    st["      ⇒ PŘIŠLI JSME O MÍČ"] += 1

    n = st["naše kola končící s nosičem"]
    e = st["  ⛔ EXPOZICE — nosič končí v kontaktu"]
    u = st["    soupeř UDEŘIL blokem"]
    print("naše kola končící s naším stojícím nosičem: %d\n" % n)
    print("  %-44s %6d  %5.1f %% z %d"
          % ("nosič ČISTÝ (zákaz dodržen)", st["  nosič ČISTÝ (zákaz dodržen)"],
             100.0 * st["  nosič ČISTÝ (zákaz dodržen)"] / n, n))
    print("  %-44s %6d  %5.1f %% z %d"
          % ("⛔ EXPOZICE — končí v kontaktu", e, 100.0 * e / n, n))
    print("     ⌀ soupeřů u nosiče při expozici: %.2f" % (st["  ⛔ sousedů celkem"] / e))
    print("\n  řetěz od expozice (jmenovatel = %d expozic):" % e)
    print("    %-42s %6d  %5.1f %%" % ("soupeř UDEŘIL blokem", u, 100.0 * u / e))
    print("    %-42s %6d  %5.1f %%" % ("soupeř NEUDEŘIL", st["    soupeř NEUDEŘIL"],
                                       100.0 * st["    soupeř NEUDEŘIL"] / e))
    if u:
        print("\n    z %d bloků (jmenovatel = bloky, ne expozice):" % u)
        for k in ("      nosič SRAŽEN", "      ⇒ PŘIŠLI JSME O MÍČ"):
            print("    %-42s %6d  %5.1f %%" % (k.strip(), st[k], 100.0 * st[k] / u))
    print("\n  ⇒ EXPOZICE → ZTRÁTA MÍČE celkem: %.1f %% z expozic, %.1f %% z našich kol"
          % (100.0 * st["      ⇒ PŘIŠLI JSME O MÍČ"] / e,
             100.0 * st["      ⇒ PŘIŠLI JSME O MÍČ"] / n))


if __name__ == "__main__":
    main()
