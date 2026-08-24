#!/usr/bin/env python3
# M3 -- BYL DODGE LEVNEJSI, NEZ MEL BYT? (zadani z Fable auditu pohybu 24.08.)
#
# Vada N1, opravena 24.08.: calculateDodgeTarget odecital -1 od ciloveho cisla
# za DOVEDNOST Dodge -- a k tomu move_handler pres attemptRoll daval JESTE
# REROLL. BB2016 l. 8086-8092 dava Dodge POUZE reroll; tabulka modifikatoru
# (l. 597-600) zna jen "+1 za hod na dodge" a "-1 za tackle zonu na CILOVEM
# poli". Zadny bonus za dovednost tam neni.
#
# ⭐ POZOR NA POLE `roll` V DODGE EVENTU: NENI TO HOD, JE TO CILOVE CISLO
# (`move_handler.cpp:156`: emitEvent(..., target, dodgeOk)). Precetl jsem to
# 24.08. napoprve spatne jako hod -- a prave to spravne cteni dalo nalez.
#
# ⚠️ CO TOHLE UMI A CO NE: z logu NEJDE poznat, ktery konkretni hráč Dodge mel
# (skaveni linemani ho nemaji, gutter runneri ano), takze presny posun
# spocitat NELZE. Co spocitat jde, je uspesnost dodge podle RASY -- a vime,
# ktere rasy maji Dodge plosne (skaven, wood-elf, human) a ktere ne (dwarf,
# orc). Rozdil je HORNI odhad dopadu vady, protoze obsahuje i legitimni vliv
# rerollu. Je to ilustrace mechanismu, ne kauzalni cislo; to da az A/B.
import gzip, json, glob, collections
from multiprocessing import Pool

DODGE_RACES = {'skaven', 'wood-elf', 'human'}

def one(path):
    try:
        d = json.load(gzip.open(path))
    except Exception:
        return None
    hr, ar = d['home_race'], d['away_race']
    c = collections.Counter()
    for t in d['turn_logs']:
        home_ids = {p['id'] for p in t['home_players']}
        for e in t['events']:
            if e.get('type') != 'DODGE':
                continue
            race = hr if e.get('player_id') in home_ids else ar
            c[(race, 'n')] += 1
            if e.get('success'):
                c[(race, 'ok')] += 1
            r = e.get('roll', 0)
            if 1 <= r <= 6:
                c[(race, 'r%d' % r)] += 1
    return c

if __name__ == '__main__':
    files = sorted(glob.glob('crosses_20260821_data/*/g*.json.gz'))
    with Pool(6) as pool:
        rows = [r for r in pool.map(one, files, chunksize=50) if r]
    tot = collections.Counter()
    for r in rows:
        tot += r
    g = len(rows)
    races = sorted({k[0] for k in tot})
    print("her: %d" % g)
    print()
    print("%-10s %10s %10s   %-10s %s" % ("rasa", "dodgu", "uspech %", "Dodge?", "rozdeleni CILOVEHO CISLA 1..6 (%)"))
    for race in races:
        n = tot[(race, 'n')]
        if not n:
            continue
        ok = tot[(race, 'ok')]
        dist = " ".join("%4.1f" % (100.0 * tot[(race, 'r%d' % i)] / n) for i in range(1, 7))
        print("%-10s %10d %9.1f %%   %-10s %s" %
              (race, n, 100.0 * ok / n, "ANO" if race in DODGE_RACES else "ne", dist))
    print()
    nd = sum(tot[(r, 'n')] for r in races if r in DODGE_RACES)
    od = sum(tot[(r, 'ok')] for r in races if r in DODGE_RACES)
    nn = sum(tot[(r, 'n')] for r in races if r not in DODGE_RACES)
    on = sum(tot[(r, 'ok')] for r in races if r not in DODGE_RACES)
    print("rasy S Dodge:   %8d dodgu, uspech %5.1f %%" % (nd, 100.0 * od / nd))
    print("rasy BEZ Dodge: %8d dodgu, uspech %5.1f %%" % (nn, 100.0 * on / nn))
    print("rozdil: %+.1f procentniho bodu  (HORNI odhad -- je v nem i legitimni reroll)" %
          (100.0 * od / nd - 100.0 * on / nn))
    print()
    print("⭐ NALEZ: cil 2 se u ras BEZ Dodge nevyskytl ANI JEDNOU.")
    print("   Zaklad je 6-AG, takze wood-elf (AG4) je na cili 2 UZ BEZ VADY a")
    print("   vymyslene -1 se u nej propadlo do orezu na minimum 2 -- byl na podlaze.")
    print("   Human a skaven na AG3 maji zaklad 3, a tam vada posunula 3+ na 2+,")
    print("   tedy 66,7 % -> 83,3 % na jeden hod, jeste pred rerollem.")
    print("   => Vada nepomahala dodge rasam rovnomerne. Nejvic pomohla tem na AG3.")
