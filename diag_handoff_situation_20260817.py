#!/usr/bin/env python3
"""
P21 — JAK ČASTO VŮBEC NASTANE SITUACE, KTEROU HAND-OFF ŘEŠÍ

Ve 3 000 hrách korpusu 14.08. je `HAND_OFF` událostí **nula**, přestože
logování hand-offu bylo přidáno právě proto (3b11d33b) a Q1 sweep dává na
postavených pozicích 18,3 %. Dvě čtení:

  (a) situace „nosič je špatný a vedle stojí lepší ruce" v reálné hře prakticky
      nenastává  ⇒  P5 je správná, ale bezcenná oprava;
  (b) makro cesta hand-offu se resolvuje jinudy než `pass_handler.cpp`
      a log ji nepokrývá  ⇒  nevíme nic.

Tenhle skript rozhoduje (a)/(b) BEZ dalšího běhu: napočítá, jak často brána
nabídky v `macro_actions.cpp:673+` na snímku začátku našeho kola projde.

⚠️ MĚŘÍ SE PŘESNĚ TA BRÁNA, KTERÁ SE PROVÁDÍ — ne její přiblížení. Postupně:
      1. držíme míč a nosič může jednat
      2. cíl stojí, je soused (dist == 1)
      3. nosič má ŠPATNÉ RUCE  (AG <= 2 a nemá Sure Hands)
      4. cíl špatné ruce NEMÁ
      5. cíl není dál od endzony než nosič  (targetDist <= carrierDist)
      6. chytí to aspoň na 50 %  (7 - AG - 1 + TZ(cíl) <= 4)
   Trychtýř se tiskne po krocích, aby bylo vidět, KTERÁ podmínka to zabíjí.

⚠️ OMEZENÍ, KTERÉ SE NESMÍ ZAMLČET: snímek je ZAČÁTEK KOLA. Situace, která
   vznikne až během kola (nosič se pohne k Runnerovi), se tady nezapočítá,
   takže výsledek je PODLAHA výskytu, ne strop.

Použití:  python3 diag_handoff_situation_20260817.py [adresář_korpusu]
"""
import glob
import gzip
import json
import sys
from collections import Counter

DATA = sys.argv[1] if len(sys.argv) > 1 else "diag_replay_mine_20260814_dauntless_data"

# Sure Hands má z trpasličího rosteru jen Runner; ostatní AG2 těla ho nemají.
# Kdyby se roster změnil, tahle množina se musí změnit s ním.
SURE_HANDS = {"Runner"}


def endzone_x(side):
    return 25 if side == "home" else 0


def cheb(ax, ay, bx, by):
    return max(abs(ax - bx), abs(ay - by))


def tacklezones(px, py, enemies):
    """Kolik stojících soupeřů sousedí s polem — přesně jako countTacklezones."""
    return sum(1 for e in enemies
               if e["state"] == 0 and cheb(px, py, e["x"], e["y"]) == 1)


def poor_hands(p):
    return p["ag"] <= 2 and p["name"] not in SURE_HANDS


def main():
    files = sorted(glob.glob(f"{DATA}/g*.json.gz"))
    if not files:
        sys.exit(f"žádná data v {DATA}")

    step = Counter()          # trychtýř
    carriers = Counter()      # kdo nese, když nese
    bad_carriers = Counter()  # kdo nese, když nese ŠPATNĚ
    killed_by = Counter()     # co bránu zabilo, když nosič byl špatný
    offer_turns = 0
    games_with_offer = 0
    handoff_events = 0
    our_turns = 0

    for f in files:
        g = json.load(gzip.open(f))
        # „naše" strana je trpaslík; korpus je 3000 trpasličích her
        if g["home_race"] == "dwarf":
            us, them = "home", "away"
        elif g["away_race"] == "dwarf":
            us, them = "away", "home"
        else:
            continue
        ez = endzone_x(us)
        seen_here = False

        for t in g["turn_logs"]:
            for e in t.get("events", []):
                if e.get("type") == "HAND_OFF":
                    handoff_events += 1
            if t["active_team"] != us:
                continue
            our_turns += 1

            mine = t[f"{us}_players"]
            theirs = t[f"{them}_players"]

            # 1. držíme míč, nosič je na hřišti a může jednat
            carrier = next((p for p in mine if p["has_ball"]), None)
            if carrier is None or carrier["state"] != 0:
                continue
            step["1. držíme míč, nosič může jednat"] += 1
            carriers[carrier["name"]] += 1

            # 3. nosič má špatné ruce  (bez toho se hand-off nenabídne vůbec)
            if not poor_hands(carrier):
                continue
            step["3. nosič má ŠPATNÉ RUCE (AG<=2, bez Sure Hands)"] += 1
            bad_carriers[carrier["name"]] += 1

            c_dist = abs(carrier["x"] - ez)

            # 2. + 4. + 5. + 6. postupně, ať je vidět, kdo to zabíjí
            neighbours = [p for p in mine
                          if p["id"] != carrier["id"] and p["state"] == 0
                          and cheb(carrier["x"], carrier["y"], p["x"], p["y"]) == 1]
            if not neighbours:
                killed_by["nikdo nestojí vedle nosiče"] += 1
                continue
            step["2. stojí vedle něj spoluhráč"] += 1

            good = [p for p in neighbours if not poor_hands(p)]
            if not good:
                killed_by["soused má taky špatné ruce"] += 1
                continue
            step["4. soused má LEPŠÍ ruce"] += 1

            fwd = [p for p in good if abs(p["x"] - ez) <= c_dist]
            if not fwd:
                killed_by["lepší ruce jsou jen DOZADU"] += 1
                continue
            step["5. a není dál od endzony"] += 1

            ok = [p for p in fwd
                  if (7 - p["ag"] - 1 + tacklezones(p["x"], p["y"], theirs)) <= 4]
            if not ok:
                killed_by["cíl je v tackle zónách, chycení pod 50 %"] += 1
                continue
            step["6. chytí to aspoň na 50 %  ⇒  NABÍDKA PROJDE"] += 1

            offer_turns += 1
            if not seen_here:
                games_with_offer += 1
                seen_here = True

    print(f"korpus: {len(files)} her · našich kol {our_turns}")
    print(f"\nHAND_OFF událostí v celém korpusu: {handoff_events}")

    print("\n=== TRYCHTÝŘ NABÍDKY HAND-OFFU (snímek začátku našeho kola) ===")
    base = step["1. držíme míč, nosič může jednat"]
    for k in ["1. držíme míč, nosič může jednat",
              "3. nosič má ŠPATNÉ RUCE (AG<=2, bez Sure Hands)",
              "2. stojí vedle něj spoluhráč",
              "4. soused má LEPŠÍ ruce",
              "5. a není dál od endzony",
              "6. chytí to aspoň na 50 %  ⇒  NABÍDKA PROJDE"]:
        n = step[k]
        print(f"  {k:52s} {n:7d}  {100.0 * n / base if base else 0:5.2f} % kol s míčem")

    print(f"\n  kol, kde nabídka projde: {offer_turns}"
          f"  ({100.0 * offer_turns / our_turns if our_turns else 0:.3f} % všech našich kol)")
    print(f"  her, kde nastane aspoň jednou: {games_with_offer} z {len(files)}"
          f"  ({100.0 * games_with_offer / len(files):.1f} %)")

    print("\n=== CO BRÁNU ZABÍJÍ (jen kola, kde nosič má špatné ruce) ===")
    tot = sum(killed_by.values()) + offer_turns
    for k, v in killed_by.most_common():
        print(f"  {k:44s} {v:7d}  {100.0 * v / tot if tot else 0:5.1f} %")

    print("\n=== KDO NESE MÍČ (kola s míčem) ===")
    for k, v in carriers.most_common():
        print(f"  {k:20s} {v:7d}  {100.0 * v / base if base else 0:5.1f} %")

    print("\n=== KDO NESE, KDYŽ NESE ŠPATNĚ ===")
    bad = sum(bad_carriers.values())
    for k, v in bad_carriers.most_common():
        print(f"  {k:20s} {v:7d}  {100.0 * v / bad if bad else 0:5.1f} %")


if __name__ == "__main__":
    main()
