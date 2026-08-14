#!/usr/bin/env python3
"""KOLIK KOL POTŘEBUJE TRPASLÍK NA TD? (11.08.2026)

Otázka uživatele: *„kolik kol potřebují trpaslíci na TD — a 10 za poločas
neberu jako odpověď."*

Odpověď „8,7 kola" (20,9 pole / 2,40 měřeného tempa) je k ničemu, protože
půle má 8 kol. Otázka tedy zní: **je 2,40 vlastnost trpaslíka, nebo jen
průměr přes kola, ve kterých se nemá kam hnout?**

⭐ ROZHODUJÍCÍ ROZLIŠENÍ, KTERÉ JSME NIKDY NEUDĚLALI:
Doktrinální tempo „2-3 pole za tah" je tempo MLETÍ — platí, když je před
klecí zeď. Na začátku drivu žádná zeď není: soupeř stojí u své lajny
(x=13) a naše klec vyráží z x≈4. První dvě tři kola jsou VOLNÁ a měla by
běžet rychlostí klece (nejpomalejší roh), ne rychlostí mletí.

⇒ Kdyby se ukázalo, že i NEKONTAKTNÍ kola postupují 2,4 pole, není to
doktrína, je to VADA — klec se plazí i tam, kde jí nic nebrání.

=========================  PŘEDREGISTRACE  =========================
Zapsáno PŘED během.

MĚŘÍM (trpasličí kola s naším míčem, cíl ADVANCE):
  odpor = počet STOJÍCÍCH soupeřů v koridoru před nosičem
          (blíž k naší endzone než nosič, Chebyshev <= 4)
  mark  = počet soupeřů v Chebyshev-1 od nosiče
  postup = Δx nosiče mezi začátkem tohoto a začátkem příštího kola
           (příští záznam téhož týmu = stav po konci mého kola;
            nález 11.08. -- konec kola JE v datech jako turnLogs[i+1])

TABULKA: průměrný postup podle odporu (0 / 1 / 2 / 3+) a podle kola.

PREDIKCE (aby to šlo vyvrátit):
  Jestli je 2,40 doktrína, MUSÍ postup při odporu 0 být výrazně vyšší
  (>= 4 pole, tj. rychlost roh
  ů MA5). Jestli vyjde i při odporu 0 kolem
  2-3, je to vada rozhodování, ne vlastnost rasy.

KONTROLA: totéž pro ork/skaven/wood-elf/člověk týmž rozhodčím.
====================================================================
"""
import gzip
import json
from collections import defaultdict
from pathlib import Path

CORPUS = "diag_replay_mine_20260730_data"


def cheb(a, b):
    return max(abs(a[0] - b[0]), abs(a[1] - b[1]))


def rows_for_game(rec):
    """Vrátí měřicí řádky pro jednu hru."""
    turns = rec["turn_logs"]
    out = []
    for i, t in enumerate(turns):
        side = t["active_team"]
        race = rec["home_race"] if side == "home" else rec["away_race"]
        if not t["ball_held"]:
            continue
        mine = t["home_players"] if side == "home" else t["away_players"]
        theirs = t["away_players"] if side == "home" else t["home_players"]
        my_ids = {p["id"] for p in mine}
        cid = t["ball_carrier_id"]
        if cid not in my_ids:
            continue
        ez = 25 if side == "home" else 0
        carrier = next(p for p in mine if p["id"] == cid)
        cpos = (carrier["x"], carrier["y"])
        dist = abs(cpos[0] - ez)
        if dist <= carrier["ma"] + 2:
            continue                       # SCORE, ne ADVANCE

        # odpor: stojící soupeři v koridoru před nosičem
        resistance = 0
        marks = 0
        for p in theirs:
            if p["state"] != 0:            # 0 = standing
                continue
            pp = (p["x"], p["y"])
            if cheb(pp, cpos) <= 1:
                marks += 1
            if abs(pp[0] - ez) <= dist and cheb(pp, cpos) <= 4:
                resistance += 1

        # konec kola = začátek dalšího záznamu TÉHOŽ týmu ve stejné půli
        nxt = None
        for j in range(i + 1, len(turns)):
            if turns[j]["active_team"] == side:
                nxt = turns[j]
                break
        if nxt is None or nxt["half"] != t["half"]:
            continue                       # TD/poločas mezi tím -> nesrovnatelné
        nmine = nxt["home_players"] if side == "home" else nxt["away_players"]
        ncar = next((p for p in nmine if p["id"] == cid), None)
        if ncar is None:
            continue
        # postup měřím na TOM SAMÉM hráči; když se míč předal, je to jiná věc
        still_ours = nxt["ball_held"] and nxt["ball_carrier_id"] in my_ids
        advance = abs(cpos[0] - ez) - abs(ncar["x"] - ez)

        out.append({
            "race": race,
            "turn": t["turn"],
            "dist": dist,
            "resistance": resistance,
            "marks": marks,
            "advance": advance,
            "kept": still_ours,
            "carrier_ma": carrier["ma"],
            "carrier": carrier.get("name", "?"),
        })
    return out


def main():
    rows = []
    for f in sorted(Path(CORPUS).glob("*.json.gz")):
        with gzip.open(f, "rt") as fh:
            rows += rows_for_game(json.load(fh))

    print("=" * 74)
    print("KOLIK KOL NA TD — postup nosiče podle ODPORU v koridoru")
    print(f"korpus {CORPUS}, {len(rows)} kol s naším míčem v režimu ADVANCE")
    print("=" * 74)

    def bucket(r):
        return "0" if r == 0 else ("1" if r == 1 else ("2" if r == 2 else "3+"))

    for race in ("dwarf", "orc", "skaven", "woodelf", "human"):
        rr = [r for r in rows if r["race"] == race]
        if not rr:
            continue
        print(f"\n### {race}  ({len(rr)} kol)")
        print(f"  {'odpor':<8}{'kol':>6}{'postup':>9}{'držel':>8}{'markován':>10}")
        for b in ("0", "1", "2", "3+"):
            sel = [r for r in rr if bucket(r["resistance"]) == b]
            if not sel:
                continue
            adv = sum(r["advance"] for r in sel) / len(sel)
            kept = sum(1 for r in sel if r["kept"]) / len(sel)
            mk = sum(r["marks"] for r in sel) / len(sel)
            print(f"  {b:<8}{len(sel):>6}{adv:>9.2f}{kept:>8.0%}{mk:>10.2f}")
        allv = sum(r["advance"] for r in rr) / len(rr)
        print(f"  {'CELKEM':<8}{len(rr):>6}{allv:>9.2f}")

    # kdo nese a jak rychle
    print("\n### trpaslík: postup podle NOSIČE (kdo drží míč)")
    dw = [r for r in rows if r["race"] == "dwarf"]
    byc = defaultdict(list)
    for r in dw:
        byc[r["carrier"].split(" ")[0]].append(r)
    for name, sel in sorted(byc.items(), key=lambda kv: -len(kv[1])):
        adv = sum(r["advance"] for r in sel) / len(sel)
        ma = sel[0]["carrier_ma"]
        print(f"  {name:<12} MA{ma}  kol {len(sel):>4}  postup {adv:>5.2f}")

    # postup podle kola -- kde se to láme
    print("\n### trpaslík: postup podle ČÍSLA KOLA (a odpor v tom kole)")
    byt = defaultdict(list)
    for r in dw:
        byt[r["turn"]].append(r)
    for tn in sorted(byt):
        sel = byt[tn]
        adv = sum(r["advance"] for r in sel) / len(sel)
        res = sum(r["resistance"] for r in sel) / len(sel)
        print(f"  kolo {tn}: kol {len(sel):>3}  postup {adv:>5.2f}  odpor {res:>4.1f}")


if __name__ == "__main__":
    main()
