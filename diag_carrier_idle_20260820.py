#!/usr/bin/env python3
"""P39 — NOSIČ SE V KOLE VŮBEC NEAKTIVUJE (přepočet na CELÉM korpusu 20.08.)

19.08. se rozpad kol s Δx = 0 počítal na vzorku 800 her a v dokladu
`cage_ma_cap_20260819.md` má **rozbité jmenovatele**: 957 + 684 = 1 641,
tedy podíly "volný / v TZ" byly z VŠECH kol s Δx = 0, ne z těch, kde nosič
nejednal, ačkoli tabulka říká "z toho". Tady se každý podíl tiskne se svým
jmenovatelem vypsaným.

⚠️ Δx = 0 se počítá ze VŠECH našich kol s nosičem, ne jen z kol s klecí --
P39 je o nosiči, ne o kleci.
"""
import sys, glob
from collections import Counter
sys.path.insert(0, "/home/jan/claude/bloodbowl")
import diag_rules_checks_20260812 as R

STANDING = 0


def main():
    paths = sorted(glob.glob(sys.argv[1]))
    st = Counter()
    for path in paths:
        r = R.load(path)
        side_of = {}
        for s in ("home", "away"):
            nm = " ".join(p["name"] for p in r["turn_logs"][0][f"{s}_players"][:3])
            side_of[s] = "dwarf" if ("Longbeard" in nm or "Troll Slayer" in nm) else None
        ours = next((s for s, v in side_of.items() if v == "dwarf"), None)
        if ours is None:
            continue
        fwd = 1 if ours == "home" else -1
        theirs = "away" if ours == "home" else "home"
        logs = r["turn_logs"]
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
            st["kol s naším stojícím nosičem"] += 1
            dx = (carE["x"] - car["x"]) * fwd
            if dx != 0:
                continue
            st["Δx = 0"] += 1
            acted = any(e["player_id"] == car["id"] for e in S["events"])
            # volný = žádná stojící soupeřova TZ na poli nosiče
            tz = sum(1 for p in R.players(S, theirs)
                     if p["state"] == STANDING
                     and max(abs(p["x"] - car["x"]), abs(p["y"] - car["y"])) <= 1)
            # ⚠️ VLASTNÍ klíč pro každou větev. První verze tohohle skriptu
            # (20.08.) zvyšovala "byl volný" v OBOU větvích, takže volný+TZ
            # dalo 5 497 proti jmenovateli 5 213 -- táž rodina vady jako
            # rozbité jmenovatele v cage_ma_cap_20260819.md, kvůli kterým
            # se přepočet dělal.
            if acted:
                st["  nosič JEDNAL (a přesto Δx=0)"] += 1
            else:
                st["  nosič NEJEDNAL VŮBEC"] += 1
                if tz == 0:
                    st["    …a byl přitom volný"] += 1
                else:
                    st["    …stál v soupeřově TZ"] += 1

    n = st["kol s naším stojícím nosičem"]
    z = st["Δx = 0"]
    ne = st["  nosič NEJEDNAL VŮBEC"]
    print("kol s naším stojícím nosičem: %d" % n)
    print("  Δx = 0                                 %6d  %5.1f %% z %d"
          % (z, 100.0 * z / n, n))
    print("  ├─ nosič NEJEDNAL VŮBEC                %6d  %5.1f %% z %d"
          % (ne, 100.0 * ne / z, z))
    print("  │   ├─ byl přitom VOLNÝ (0 TZ)         %6d  %5.1f %% z %d"
          % (st["    …a byl přitom volný"], 100.0 * st["    …a byl přitom volný"] / ne, ne))
    print("  │   └─ stál v soupeřově TZ             %6d  %5.1f %% z %d"
          % (st["    …stál v soupeřově TZ"], 100.0 * st["    …stál v soupeřově TZ"] / ne, ne))
    print("  └─ nosič JEDNAL, ale Δx=0              %6d  %5.1f %% z %d"
          % (st["  nosič JEDNAL (a přesto Δx=0)"],
             100.0 * st["  nosič JEDNAL (a přesto Δx=0)"] / z, z))


if __name__ == "__main__":
    main()
