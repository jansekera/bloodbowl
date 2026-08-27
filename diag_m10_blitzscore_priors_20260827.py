#!/usr/bin/env python3
"""
M10 (27.08.2026) — „ODBLITZOVÁNÍ MARKERA": kolik z nabídek BLITZ_AND_SCORE
padne do větve, kde je nízký prior ZÁMĚR (stall), a kolik do větve, kde je
podlaha a makro se stejně nevzalo.

⚑ PROČ. P27 (Fable 17.08.) našel: BLITZ_AND_SCORE nabídnuto v 1 123 kolech,
nosič blitzoval 38x, TD 2x -- „vada ve VOLBĚ". Brzda k tomu ale hned zapsala
podmínku: `macro_mcts.cpp:494` dává SCORE-rodině STROP 0,02 při vedení a více
než dvou zbývajících kolech, a to je doktrína stall, ne chyba. Dokud se ta
kola neodečtou, není známo, na jak velkém vzorku se ta „vada" vůbec měří.

⭐ CO SE POČÍTÁ. Každé naše kolo, kde brána nabídne BLITZ_AND_SCORE, se zařadí
do TÉŽE větve, jakou by vzal `macro_mcts.cpp:472-500` (ladder je uspořádaný,
platí PRVNÍ splněná podmínka):

  (1) turnsRemaining <= 1        -> PODLAHA 0,70 / 0,90   (poslední kolo)
  (2) trailing2plus              -> PODLAHA 0,50
  (3) isFirstTurn                -> STROP   0,05
  (4) leading && tR > 2          -> STROP   0,02          <= ZÁMĚR (stall)
  (5) turnsRemaining <= 2        -> PODLAHA 0,35
  (6) turnsRemaining <= 4        -> PODLAHA 0,20
  (7) jinak                      -> PODLAHA 0,08

Strop = makro se smí nabídnout, ale prior mu je stlačen; podlaha = naopak.
⇒ „vada ve VOLBĚ" se smí tvrdit JEN o kolech z větví s podlahou.

⚠️ OMEZENÍ (dědí se z rekonstrukce brány, viz diag_fable_offered_played_20260817):
  - snímek je ZAČÁTEK kola, takže „nabídnuto" je PODLAHA (makra vzniklá až
    během kola nevidíme),
  - v dw-dw se čte jen strana `home` (skript volí naši stranu jako u Fableho),
  - `turnsRemaining = 9 - turn` podle `macro_mcts.cpp:442`; `turn` v korpusu
    je 1..8 a v každé půli se počítá znovu -- ověřeno na datech.

Použití: nice -n 19 python3 diag_m10_blitzscore_priors_20260827.py [korpus]
"""
import glob
import gzip
import json
import sys
from collections import Counter

sys.path.insert(0, ".")
_m = __import__("diag_fable_offered_played_20260817")
analyze_turn_offers = _m.analyze_turn_offers

DATA = sys.argv[1] if len(sys.argv) > 1 else "blitzlanding_replic_20260825_corpus_data"


def branch(turn, score_diff):
    """Vrať (klíč, druh, hodnota) té větve ladderu, která by platila."""
    tr = max(0, 9 - turn)
    if tr <= 1:
        return "(1) poslední kolo", "PODLAHA", 0.70
    if score_diff <= -2:
        return "(2) prohráváme o 2+", "PODLAHA", 0.50
    if turn == 1:
        return "(3) první kolo", "STROP", 0.05
    if score_diff >= 1 and tr > 2:
        return "(4) VEDEME (stall)", "STROP", 0.02
    if tr <= 2:
        return "(5) zbývají <= 2 kola", "PODLAHA", 0.35
    if tr <= 4:
        return "(6) zbývají <= 4 kola", "PODLAHA", 0.20
    return "(7) zbytek", "PODLAHA", 0.08


def main():
    files = sorted(glob.glob(f"{DATA}/g*.json.gz"))
    if not files:
        sys.exit(f"žádná data v {DATA}")

    offered = Counter()          # větev -> kol s nabídkou
    carrier_blitzed = Counter()  # větev -> z toho nosič JISTĚ blitzoval (blok po pohybu)
    ambiguous = Counter()        # větev -> nosič blokoval BEZ pohybu (BLOCK nebo blitz zblízka)
    tds = Counter()              # větev -> z toho TD v témž kole
    kind = {}
    our_turns = games = 0
    offered_total = 0
    also_score = 0

    for f in files:
        g = json.load(gzip.open(f))
        if g["home_race"] == "dwarf":
            us, them = "home", "away"
        elif g["away_race"] == "dwarf":
            us, them = "away", "home"
        else:
            continue
        games += 1
        ez = 25 if us == "home" else 0
        opp_race = g[f"{them}_race"]

        for t in g["turn_logs"]:
            if t["active_team"] != us:
                continue
            our_turns += 1
            mine = t[f"{us}_players"]
            theirs = t[f"{them}_players"]
            ball = {"x": t["ball_x"], "y": t["ball_y"], "held": t["ball_held"]}
            off = analyze_turn_offers(mine, theirs, "dwarf", opp_race, ez,
                                      t["turn"], t["weather"], ball)
            if off.get("BLITZ_AND_SCORE", 0) <= 0:
                continue
            offered_total += 1
            if off.get("SCORE", 0) > 0:
                also_score += 1

            sd = t[f"{us}_score"] - t[f"{them}_score"]
            key, k, val = branch(t["turn"], sd)
            kind[key] = (k, val)
            offered[key] += 1

            # ⚠️ „nosič blitzoval" NEJDE z logu určit jednoznačně: blitz ze
            # SOUSEDNÍ pozice nepotřebuje krok, takže je k nerozeznání od
            # makra BLOCK (táž výhrada jako u Fableho 17.08.). Tisknou se
            # proto DVA sloupce: jistý blitz (blok PO pohybu nosiče) a blok
            # BEZ pohybu, který je horní mez toho zbytku.
            carrier_id = next((p["id"] for p in mine if p["has_ball"]), -1)
            moved = False
            sure = amb = False
            for e in t["events"]:
                pid = e.get("player_id", -1)
                if pid != carrier_id:
                    continue
                if e["type"] in ("MOVE", "GFI", "DODGE"):
                    moved = True
                elif e["type"] == "BLOCK":
                    if moved:
                        sure = True
                    else:
                        amb = True
            if sure:
                carrier_blitzed[key] += 1
            elif amb:
                ambiguous[key] += 1
            if t.get("touchdown"):
                tds[key] += 1

    print(f"korpus {DATA}: {games} her, {our_turns} našich kol")
    print(f"BLITZ_AND_SCORE nabídnuto v {offered_total} kolech "
          f"({offered_total / max(1, games):.3f} na hru), "
          f"z toho {also_score} i se současnou nabídkou SCORE\n")
    hdr = (f"{'větev ladderu':<24}{'druh':<9}{'hodn.':>7}{'kol':>8}{'podíl':>8}"
           f"{'blitz':>7}{'blok?':>7}{'TD':>5}")
    print(hdr); print("-" * len(hdr))
    order = sorted(offered, key=lambda k: -offered[k])
    cap = flo = capb = flob = 0
    for key in order:
        k, val = kind[key]
        n = offered[key]
        print(f"{key:<24}{k:<9}{val:>7.2f}{n:>8}{n / offered_total:>7.1%}"
              f"{carrier_blitzed[key]:>7}{ambiguous[key]:>7}{tds[key]:>5}")
        if k == "STROP":
            cap += n; capb += carrier_blitzed[key]
        else:
            flo += n; flob += carrier_blitzed[key]
    print("-" * len(hdr))
    print(f"{'STROP (prior stlačen)':<24}{'':<9}{'':>7}{cap:>8}{cap / offered_total:>7.1%}{capb:>7}")
    print(f"{'PODLAHA (prior zvednut)':<24}{'':<9}{'':>7}{flo:>8}{flo / offered_total:>7.1%}{flob:>7}")
    print("\n  blitz = blok nosiče PO jeho pohybu (jistý blitz)")
    print("  blok? = blok nosiče BEZ pohybu -- BLOCK makro NEBO blitz ze sousední"
          " pozice, z logu nerozlišitelné (horní mez)")
    print(f"\n⇒ „vada ve VOLBĚ\" se smí tvrdit o {flo} kolech, ne o {offered_total}.")
    print(f"  konverze v podlahových větvích: {flob}/{flo} = {flob / max(1, flo):.2%}")


if __name__ == "__main__":
    main()
