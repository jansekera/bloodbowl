#!/usr/bin/env python3
"""OQ1: strop účinku útočné změny na chess + rozklad marží.

chess hry = 1 výhra / 0,5 remíza / 0 prohra (týž převod jako harness).
Scénáře (vždy Δchess = nový − současný, po hrách, s bootstrapem po hrách):

  S+1TD   : +1 náš TD v každé hře (hrubý strop čehokoli útočného)
  S_D2    : každý D2 drive (pomalá klec) se změní v náš TD
  S_D1    : každý D1 drive v náš TD (vyžadovalo by opravit kickoff-return,
            u 99,1 % krátkých od výkopu ani to nestačí — horní mez)
  S_C     : každý C drive v náš TD; soupeřův TD z TÉHOŽ drivu se ruší
            (švih o 2) — strop „nikdy neztratíme míč"
  S_Cnoswing: C bez rušení soupeřova TD (konzervativní: skóroval by jindy)
  S_full  : všechny plné přijímací drivy (>=7 kol) bez našeho TD skórují
  S_def   : obranné drivy „soupeř TD" se změní na „bez TD" (obranný protějšek)
Citlivost: Δchess na 1 konvertovaný drive v každém scénáři.
"""
import json, random, sys
from collections import Counter, defaultdict

GAMES = [json.loads(l) for l in open(sys.argv[1])]
random.seed(42)

def chess(our, their):
    return 1.0 if our > their else 0.5 if our == their else 0.0

def summarize(label, deltas, converted):
    n = len(deltas)
    mu = sum(deltas) / n
    # bootstrap po hrách
    bs = []
    for _ in range(2000):
        s = [deltas[random.randrange(n)] for _ in range(n)]
        bs.append(sum(s) / n)
    bs.sort()
    lo, hi = bs[int(0.025 * len(bs))], bs[int(0.975 * len(bs))]
    conv = sum(converted)
    per = mu * n / conv if conv else float("nan")
    print(f"{label:12} Δchess={mu:+.4f} [{lo:+.4f},{hi:+.4f}]  "
          f"konvertováno {conv} drivů ({conv/n:.2f}/hru)  "
          f"Δchess/1 konv. drive={per*1:+.5f}×n_her… na hru {per:+.4f}")
    return mu

def scenario(games, mod):
    deltas, conv = [], []
    for g in games:
        our, their = g["our"], g["their"]
        no, nt, c = mod(g)
        deltas.append(chess(no, nt) - chess(our, their))
        conv.append(c)
    return deltas, conv

def s_plus1(g):
    return g["our"] + 1, g["their"], 1

def s_cat(cats, swing=False):
    def f(g):
        add = rem = 0
        for d in g["drives"]:
            key = d.get("subcat") or d.get("cat")
            if key in cats:
                add += 1
                if swing and d.get("opp_td_in_drive"):
                    rem += 1
        return g["our"] + add, g["their"] - rem, add
    return f

def s_full(g):
    add = sum(1 for d in g["drives"]
              if (d.get("n_our_turns") or 0) >= 7 and d["cat"] != "A")
    return g["our"] + add, g["their"], add

def s_def(g):
    rem = sum(1 for d in g["def_drives"] if d["opp_td"])
    return g["our"], g["their"] - rem, rem

print(f"her: {len(GAMES)}")
base = sum(chess(g["our"], g["their"]) for g in GAMES) / len(GAMES)
res = Counter()
marg = Counter()
for g in GAMES:
    m = g["our"] - g["their"]
    res["W" if m > 0 else "D" if m == 0 else "L"] += 1
    marg[m] += 1
print(f"chess teď: {base:.4f}   W/D/L = {res['W']}/{res['D']}/{res['L']}")
print("marže:", dict(sorted(marg.items())))
one_goal = res["D"] + marg[-1]
print(f"hry na-jeden-gól (remíza nebo prohra o 1): {one_goal} "
      f"({one_goal/len(GAMES):.1%}) → hrubý strop Δchess = {0.5*one_goal/len(GAMES):+.4f}")
# hry na jeden gól i směrem dolů (výhra o 1 / remíza): citlivost na -1 TD
frag = res["D"] + marg.get(1, 0)
print(f"hry, kde -1 náš TD zhorší chess: {frag} ({frag/len(GAMES):.1%})"
      f" → případná CENA změny má stejně velký kanál: {-0.5*frag/len(GAMES):+.4f}")

print()
for label, mod in [
        ("S+1TD", s_plus1),
        ("S_D2", s_cat({"D2"})),
        ("S_D1", s_cat({"D1"})),
        ("S_C", s_cat({"C"}, swing=True)),
        ("S_Cnoswing", s_cat({"C"})),
        ("S_full", s_full),
        ("S_def", s_def)]:
    deltas, conv = scenario(GAMES, mod)
    summarize(label, deltas, conv)

# po soupeřích: kde leží hry na jeden gól
print("\npo soupeřích (n=750 každý):")
for opp in ("skaven", "wood-elf", "human", "orc"):
    gs = [g for g in GAMES if g["opp"] == opp]
    b = sum(chess(g["our"], g["their"]) for g in gs) / len(gs)
    og = sum(1 for g in gs if g["our"] == g["their"] or g["their"] - g["our"] == 1)
    d2 = sum(1 for g in gs for d in g["drives"] if d.get("subcat") == "D2")
    c = sum(1 for g in gs for d in g["drives"] if d["cat"] == "C")
    print(f"  {opp:9} chess={b:.3f}  na-jeden-gól={og/len(gs):.1%}  "
          f"D2/hru={d2/len(gs):.2f}  C/hru={c/len(gs):.2f}")
