#!/usr/bin/env python3
"""
NEROZBILO SE TO? — kontrola po pravidlové opravě dodge (11.09.2026)

⚑ ZADÁNÍ UŽIVATELE 11.09.: „pravidlová oprava se nedá měřit zlepšením - to je
  oprava, ne pokus o vylepšení" + „přeměřit asi sedí - ale jen jestli se to
  celé nerozbilo".

⇒ NEMĚŘÍ SE, jestli je klec hezčí. Měří se, jestli engine pořád HRAJE HRU:
  dohrává zápasy, dává TD, neztrácí míč nesmyslně častěji, nezasekává se.
  Estetika klece je jiná otázka a do téhle odpovědi nepatří.

⛔ JMENOVATEL: každý zápas i každé kolo padne právě do jednoho koše, tiskne se
  ZBYTEK, který musí být 0.
"""
import glob, gzip, json, math, sys

def load(d):
    out = []
    for f in sorted(glob.glob(d + "/*.json.gz")):
        try:
            out.append(json.load(gzip.open(f)))
        except Exception as e:
            print(f"  ⛔ nečitelné: {f}: {e}")
    return out

def stats(games, name):
    n = len(games)
    td = turns = turnovers = 0
    kratke = 0
    kola_celkem = 0
    skore = []
    for g in games:
        s = g["home_score"] + g["away_score"]
        td += s
        skore.append(s)
        tl = g["turn_logs"]
        kola_celkem += len(tl)
        if len(tl) < 32:
            kratke += 1
        for t in tl:
            if t.get("turnover"):
                turnovers += 1
    return dict(name=name, n=n, td=td, kola=kola_celkem, turnovers=turnovers,
                kratke=kratke, skore=skore)

def p(a, b):
    """rozdil dvou podilu + sigma"""
    pa, na = a
    pb, nb = b
    if na == 0 or nb == 0: return 0.0, 0.0, 0.0
    ra, rb = pa/na, pb/nb
    pooled = (pa+pb)/(na+nb)
    se = math.sqrt(pooled*(1-pooled)*(1/na+1/nb)) if 0 < pooled < 1 else 0.0
    return ra, rb, ((ra-rb)/se if se else 0.0)

A = sys.argv[1] if len(sys.argv) > 1 else "corpus_k7_20260910_data"
B = sys.argv[2] if len(sys.argv) > 2 else "corpus_m11_20260909_data"

print("⭐ POZITIVNÍ KONTROLA ČTEČKY (běží PŘED korpusem):")
ga = load(A)
gb = load(B)
for nm, gs in (("PO opravě  " + A, ga), ("PŘED opravou " + B, gb)):
    ok = len(gs) > 0 and all("turn_logs" in g for g in gs)
    print(f"    [{'OK ' if ok else 'VADA'}] {nm}: {len(gs)} her načteno, turn_logs u všech: {ok}")
if not ga or not gb:
    print("  ⛔ jeden z korpusů je prázdný — čísla níž nemají smysl."); sys.exit(1)
print("  ⇒ čtečka vidí obě strany.\n")

sa, sb = stats(ga, "PO opravě (496f5a03)"), stats(gb, "PŘED opravou (cf8634e8)")

print(f"{'':38s} {'PO opravě':>14s} {'PŘED opravou':>14s} {'sigma':>8s}")
print(f"{'':38s} {sa['n']:>10d} her {sb['n']:>10d} her")
print("-"*80)

ra, rb, sg = p((sa['td'], sa['n']), (sb['td'], sb['n']))
print(f"{'TD na zápas (obě strany)':38s} {ra:14.3f} {rb:14.3f} {sg:8.2f}")

ra, rb, sg = p((sa['turnovers'], sa['kola']), (sb['turnovers'], sb['kola']))
print(f"{'turnoverů na kolo':38s} {ra:14.3f} {rb:14.3f} {sg:8.2f}")

ra, rb, sg = p((sa['kratke'], sa['n']), (sb['kratke'], sb['n']))
print(f"{'zápasů kratších než 32 kol':38s} {ra:14.3f} {rb:14.3f} {sg:8.2f}")

ra, rb, sg = p((sum(1 for x in sa['skore'] if x == 0), sa['n']),
               (sum(1 for x in sb['skore'] if x == 0), sb['n']))
print(f"{'zápasů 0:0 (nikdo neskóroval)':38s} {ra:14.3f} {rb:14.3f} {sg:8.2f}")

print(f"\nkol celkem: PO {sa['kola']}  PŘED {sb['kola']}")
print(f"ZBYTEK zápasů (musí být 0): PO {sa['n']-len(sa['skore'])}  PŘED {sb['n']-len(sb['skore'])}")
