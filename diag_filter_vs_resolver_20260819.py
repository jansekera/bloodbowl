#!/usr/bin/env python3
"""AUDIT FILTR vs RESOLVER, druhé kolo (19.08.2026).

Vzorec z 14.08.: **rozhodovací vrstva oceňuje jinou akci, než jakou resolver
provede.** Tehdy 5 nálezů za den. Tenhle skript počítá STROP dvou nových:

  N1  BLITZ SE OCEŇUJE Z VÝCHOZÍHO POLE.
      `getBlockDiceCount` (macro_actions.cpp:182) počítá OBRANNÉ asistence
      jako `countAssists(state, att.position, ...)` -- tedy kolem pole, kde
      blitzující STOJÍ. Resolver (block_handler.cpp:491) je počítá kolem pole,
      kde blitzující STOJÍ AŽ PO PŘESUNU, protože blitz je pohyb a teprve pak
      blok. Kdo blitzuje do hloučku, má na startu 0 obranných asistencí
      a u cíle jich může mít několik.
      ⇒ Kolik našich blitzů se vybralo s optimistickými kostkami?

  N2  `carrierIsBlitzable` (macro_actions.cpp:1162) NEZNÁ GFI.
      Testuje `distance <= opp.stats.movement`. CRP dovolí 2 pole navíc přes
      GFI (každé 1/6 na pád), takže dosah blitzu je MA+2, ne MA. Tenhle test
      rozhoduje, jestli si nosič NECHÁ pohyb v záloze (`carrierStallAwareSteps`)
      -- tedy jestli se schová, nebo se spolehne, že na něj soupeř nedosáhne.
      ⇒ V kolika kolech si nosič nechá zálohu, ačkoli soupeř na něj přes GFI dosáhne?

⚠️ Obojí je STROP: počítá se z korpusu, ne z běhu, a bez znalosti dovedností
(korpus neveze Guard). Guard by obranné asistence jen PŘIDAL, takže N1 je
konzervativní -- skutečné číslo může být jen vyšší.
"""
import sys, glob
from collections import Counter
sys.path.insert(0, "/home/jan/claude/bloodbowl")
import diag_rules_checks_20260812 as R

STANDING = 0


def dice_bracket(att, dfn):
    """Vrátí (počet kostek, vybírá útočník?) -- táž tabulka jako getBlockDiceInfo."""
    if att > 2 * dfn: return (3, True)
    if att > dfn:     return (2, True)
    if att == dfn:    return (1, True)
    if dfn > 2 * att: return (3, False)
    return (2, False)


def signed(att, dfn):
    n, mine = dice_bracket(att, dfn)
    return n if mine else -n


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
        theirs = "away" if ours == "home" else "home"

        for S in r["turn_logs"]:
            if S["active_team"] != ours:
                continue
            us = R.players(S, ours)
            them = R.players(S, theirs)
            by_id = {p["id"]: p for p in us + them}
            occ = {(p["x"], p["y"]) for p in us + them}
            us_std = [p for p in us if p["state"] == STANDING]
            them_std = [p for p in them if p["state"] == STANDING]

            def our_tz(pos):
                return sum(1 for p in us_std if R.adj((p["x"], p["y"]), pos))

            def their_tz(pos):
                return sum(1 for p in them_std if R.adj((p["x"], p["y"]), pos))

            # ---------- N1: blitz oceněný z výchozího pole ----------
            for e in S["events"]:
                if e["type"] != "BLOCK":
                    continue
                a = by_id.get(e["player_id"])
                d = by_id.get(e.get("target_id"))
                if a is None or d is None or a["id"] not in {p["id"] for p in us}:
                    continue
                apos, dpos = (a["x"], a["y"]), (d["x"], d["y"])
                if R.adj(apos, dpos):
                    continue          # sousedili na začátku kola => byl to BLOK, ne blitz
                st["našich rekonstruovaných blitzů"] += 1

                # útočné asistence: naše stojící těla u CÍLE, mimo soupeřovu TZ
                # (kromě samotného cíle) -- na pozici útočníka nezávisí
                off = sum(1 for p in us_std
                          if p["id"] != a["id"] and R.adj((p["x"], p["y"]), dpos)
                          and their_tz((p["x"], p["y"])) - 1 <= 0)
                # obranné asistence TAK, JAK JE POČÍTÁ FILTR: kolem VÝCHOZÍHO pole
                dfn_start = sum(1 for p in them_std
                                if p["id"] != d["id"] and R.adj((p["x"], p["y"]), apos)
                                and our_tz((p["x"], p["y"])) - 1 <= 0)
                # ... a tak, jak je spočítá RESOLVER: kolem pole, kam blitzující
                # dojde. Bereme pro nás NEJPŘÍZNIVĚJŠÍ dostupné pole u cíle
                # => strop je konzervativní.
                best = None
                for dx in (-1, 0, 1):
                    for dy in (-1, 0, 1):
                        if dx == dy == 0:
                            continue
                        land = (dpos[0] + dx, dpos[1] + dy)
                        if not (0 <= land[0] <= 25 and 0 <= land[1] <= 14):
                            continue
                        if land in occ and land != apos:
                            continue
                        v = sum(1 for p in them_std
                                if p["id"] != d["id"] and R.adj((p["x"], p["y"]), land)
                                and our_tz((p["x"], p["y"])) - 1 <= 0)
                        best = v if best is None else min(best, v)
                if best is None:
                    st["blitz bez volného pole u cíle (nepočítá se)"] += 1
                    continue

                if best > dfn_start:
                    st["N1 filtr byl OPTIMISTICKÝ (u cíle víc obranných asistencí)"] += 1
                    st["N1 podhodnocených asistencí celkem"] += best - dfn_start

                dice_f = signed(a["st"] + off, d["st"] + dfn_start)
                dice_r = signed(a["st"] + off, d["st"] + best)
                if dice_f != dice_r:
                    st["N1 ZMĚNÍ SE POČET KOSTEK"] += 1
                    if dice_f > 0 and dice_r <= 0:
                        st["N1 ⛔ z 'vybíráme my' na 'vybírá soupeř'"] += 1
                    st[f"N1 kostky {dice_f:+d} → {dice_r:+d}"] += 1

            # ---------- N2: carrierIsBlitzable nezná GFI ----------
            car = next((p for p in us if p["has_ball"]), None)
            if car is None:
                continue
            st["našich kol s nosičem"] += 1
            cpos = (car["x"], car["y"])
            reach_ma = any(max(abs(p["x"] - cpos[0]), abs(p["y"] - cpos[1])) <= p["ma"]
                           for p in them_std)
            reach_gfi = any(max(abs(p["x"] - cpos[0]), abs(p["y"] - cpos[1])) <= p["ma"] + 2
                            for p in them_std)
            if reach_ma:
                st["N2 kód VÍ, že je nosič v dosahu (plný sprint)"] += 1
            elif reach_gfi:
                st["N2 ⛔ kód říká BEZPEČNO, ale soupeř dosáhne přes GFI"] += 1
            else:
                st["N2 nosič je mimo dosah i s GFI"] += 1
    return st


def main():
    st = run(sorted(glob.glob(sys.argv[1])))
    nb = st["našich rekonstruovaných blitzů"]
    nt = st["našich kol s nosičem"]
    print("N1 — BLITZ OCEŇOVANÝ Z VÝCHOZÍHO POLE   (jmenovatel: %d blitzů)" % nb)
    for k in ["N1 filtr byl OPTIMISTICKÝ (u cíle víc obranných asistencí)",
              "N1 ZMĚNÍ SE POČET KOSTEK",
              "N1 ⛔ z 'vybíráme my' na 'vybírá soupeř'"]:
        print("   %-58s %6d  %5.1f %%" % (k, st[k], 100.0 * st[k] / nb if nb else 0))
    print("   podhodnocených asistencí celkem %d = %.2f na blitz"
          % (st["N1 podhodnocených asistencí celkem"],
             st["N1 podhodnocených asistencí celkem"] / nb if nb else 0))
    for k in sorted(st):
        if k.startswith("N1 kostky"):
            print("     %-56s %6d" % (k, st[k]))
    print("\nN2 — carrierIsBlitzable NEZNÁ GFI      (jmenovatel: %d kol s nosičem)" % nt)
    for k in ["N2 kód VÍ, že je nosič v dosahu (plný sprint)",
              "N2 ⛔ kód říká BEZPEČNO, ale soupeř dosáhne přes GFI",
              "N2 nosič je mimo dosah i s GFI"]:
        print("   %-58s %6d  %5.1f %%" % (k, st[k], 100.0 * st[k] / nt if nt else 0))


if __name__ == "__main__":
    main()
