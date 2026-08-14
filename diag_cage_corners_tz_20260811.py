#!/usr/bin/env python3
"""[1] ROH KLECE V SOUPEŘOVĚ TACKLE ZÓNĚ — kontrola K7 (11.08.2026).

Stálé pravidlo uživatele, opakovaně sdělené od 04.08. („uvolňování rohů",
„release markovaných rohů"). Web to potvrzuje (bbtactics Cage Basics:
*„None of your five players end the turn in the tacklezone of an opposing
player"*) a dodává důvod: obklíčený roh soupeř odblokuje a klec se otevře,
takže **markovaný roh je horší než užitečný — je aktivně škodlivý.**

DOSUD JSME MĚŘILI JEN OBSAZENOST rohů (0 rohů ve 39 % kol, 4 rohy v 8 %).
Jestli je část těch obsazených rohů markovaná, je skutečné číslo klece
horší než 8 %.

=========================  PŘEDREGISTRACE  =========================
Zapsáno PŘED během.

Pro každé kolo, kde NÁŠ hráč drží míč a nejsme v dosahu endzone (ADVANCE):
  KONEC KOLA = turn_logs[i+1] (nález 11.08.; výjimka kola před TD/poločasem)
  rohy       = 4 diagonály nosiče
  obsazený   = stojí na něm náš hráč
  ČISTÝ      = obsazený A ZÁROVEŇ na tom poli není žádná soupeřova
               tackle zóna (soupeř stojící v Chebyshev-1 od toho pole)
  nosič      = táž kontrola pro pole nosiče (povinnost S2.2)

HLAVNÍ ČÍSLA:
  1. rozdělení počtu OBSAZENÝCH rohů (0-4)   <- srovnatelné s 10.08.
  2. rozdělení počtu ČISTÝCH rohů (0-4)      <- NOVÉ
  3. podíl obsazených rohů, které jsou markované
  4. podíl kol, kde je markovaný i NOSIČ
  5. „plná čistá klec" = 4 čisté rohy A nemarkovaný nosič

PREDIKCE (aby to šlo vyvrátit): tvrdím, že podíl markovaných rohů bude
VYŠŠÍ než podíl markovaných nosičů — nosiče plánovač chrání, rohy ne
(cage_advance.cpp o tackle zónách vůbec neví). Kdyby to vyšlo obráceně,
je moje čtení kódu špatně.

KONTROLA: totéž pro ostatní rasy, týmž rozhodčím.
====================================================================
"""
import gzip
import json
import sys
from collections import Counter
from pathlib import Path

CORPUS = sys.argv[1] if len(sys.argv) > 1 else "diag_replay_mine_20260730_data"


def cheb(a, b):
    return max(abs(a[0] - b[0]), abs(a[1] - b[1]))


def marked(pos, foes):
    """Je pole POS v tackle zóně některého stojícího soupeře?"""
    return any(cheb(pos, f) <= 1 for f in foes)


def rows_for_game(rec):
    turns = rec["turn_logs"]
    out = []
    for i, t in enumerate(turns):
        side = t["active_team"]
        race = rec["home_race"] if side == "home" else rec["away_race"]
        if not t["ball_held"]:
            continue
        mine = t["home_players"] if side == "home" else t["away_players"]
        my_ids = {p["id"] for p in mine}
        if t["ball_carrier_id"] not in my_ids:
            continue

        # konec kola = další záznam téhož týmu ve stejné půli
        nxt = None
        for j in range(i + 1, len(turns)):
            if turns[j]["active_team"] == side:
                nxt = turns[j]
                break
        if nxt is None or nxt["half"] != t["half"]:
            continue
        nmine = nxt["home_players"] if side == "home" else nxt["away_players"]
        nfoes = nxt["away_players"] if side == "home" else nxt["home_players"]
        cid = nxt["ball_carrier_id"]
        if not nxt["ball_held"] or cid not in my_ids:
            continue
        carrier = next((p for p in nmine if p["id"] == cid), None)
        if carrier is None or carrier["state"] != 0:
            continue

        ez = 25 if side == "home" else 0
        cpos = (carrier["x"], carrier["y"])
        if abs(cpos[0] - ez) <= carrier["ma"] + 2:
            continue                                    # SCORE, ne ADVANCE

        foes = [(p["x"], p["y"]) for p in nfoes if p["state"] == 0]
        mates = {(p["x"], p["y"]) for p in nmine
                 if p["state"] == 0 and p["id"] != cid}

        occupied = clean = 0
        for ddx, ddy in ((1, 1), (1, -1), (-1, 1), (-1, -1)):
            slot = (cpos[0] + ddx, cpos[1] + ddy)
            if slot in mates:
                occupied += 1
                if not marked(slot, foes):
                    clean += 1

        out.append({
            "race": race,
            "occupied": occupied,
            "clean": clean,
            "carrier_marked": marked(cpos, foes),
        })
    return out


def main():
    rows = []
    for f in sorted(Path(CORPUS).glob("*.json.gz")):
        with gzip.open(f, "rt") as fh:
            rows += rows_for_game(json.load(fh))

    print("=" * 72)
    print("[1] ROHY KLECE V TACKLE ZÓNĚ — kontrola K7")
    print(f"korpus {CORPUS}, {len(rows)} kol s naším míčem v režimu ADVANCE")
    print("=" * 72)

    for race in ("dwarf", "orc", "skaven", "wood-elf", "human"):
        rr = [r for r in rows if r["race"] == race]
        if not rr:
            continue
        n = len(rr)
        occ = Counter(r["occupied"] for r in rr)
        cln = Counter(r["clean"] for r in rr)
        tot_occ = sum(r["occupied"] for r in rr)
        tot_cln = sum(r["clean"] for r in rr)
        full = sum(1 for r in rr if r["clean"] == 4 and not r["carrier_marked"])
        cm = sum(1 for r in rr if r["carrier_marked"]) / n

        print(f"\n### {race}  ({n} kol)")
        print(f"  {'rohů':<6}{'OBSAZENÝCH':>13}{'ČISTÝCH':>11}")
        for k in range(5):
            print(f"  {k:<6}{occ.get(k,0)/n:>12.0%}{cln.get(k,0)/n:>11.0%}")
        if tot_occ:
            print(f"  ⇒ z {tot_occ} obsazených rohů je MARKOVANÝCH "
                  f"{tot_occ-tot_cln} = {(tot_occ-tot_cln)/tot_occ:.0%}")
        print(f"  ⇒ NOSIČ markován v {cm:.0%} kol")
        print(f"  ⇒ PLNÁ ČISTÁ KLEC (4 čisté rohy + volný nosič): {full/n:.0%}")


if __name__ == "__main__":
    main()
