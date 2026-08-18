"""Když jde udeřit na soupeřova nosiče, UDĚLÁME to? (18.08.)

Q1 na postavené pozici řekl, že si search nosiče bere v 98 % — jenže postavená
pozice může být nereprezentativní. Tohle je táž otázka na REÁLNÉM korpusu:
   jmenovatel = kola, kde blok na soupeřova nosiče byl k dispozici
   čitatel    = kola, kde náš BLOCK/BLITZ opravdu mířil na nosiče
Bez jmenovatele by číslo neznamenalo nic (audit aparátu 13.08.).
"""
import sys, glob
from collections import Counter
sys.path.insert(0, '/home/jan/claude/bloodbowl')
from diag_rules_checks_20260812 import load, players, STANDING

DATA = sys.argv[1] if len(sys.argv) > 1 else \
    '/home/jan/claude/bloodbowl/corpus_baseline_20260817_data'

def cheb(a, b): return max(abs(a[0]-b[0]), abs(a[1]-b[1]))

st = Counter()
for path in sorted(glob.glob(DATA + '/*.json.gz')):
    r = load(path)
    ours = "home" if r.get("home_race") == "dwarf" else "away"
    them = "away" if ours == "home" else "home"
    for S in r["turn_logs"]:
        if S["active_team"] != ours:
            continue
        us = players(S, ours); th = players(S, them)
        car = next((p for p in th if p["has_ball"]), None)
        if car is None:
            continue
        c = (car["x"], car["y"])
        adj = [p for p in us if p["state"] == STANDING and cheb((p["x"], p["y"]), c) == 1]
        ourIds = {p["id"] for p in us}
        hits = [e for e in S["events"]
                if e["type"] in ("BLOCK", "BLITZ") and e["player_id"] in ourIds]
        onCar = [e for e in hits if e.get("target_id") == car["id"]]
        if adj:
            st["příležitost (soused u nosiče)"] += 1
            if onCar: st["… a udeřili jsme na nosiče"] += 1
            elif hits: st["… udeřili jsme JINAM"] += 1
            else:      st["… neudeřili jsme vůbec"] += 1
        else:
            st["nosič bez souseda (jen blitz by dosáhl)"] += 1
            if onCar: st["… a přesto jsme na něj udeřili (blitz)"] += 1

n = st["příležitost (soused u nosiče)"]
print(f"korpus: {DATA.rsplit('/',1)[-1]}")
for k, v in st.items():
    print(f"  {k:44s} {v:7d}")
print(f"\n  ⇒ z příležitostí jsme na nosiče udeřili v "
      f"{100*st['… a udeřili jsme na nosiče']/max(1,n):.1f} %")
