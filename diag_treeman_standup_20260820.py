import sys, glob
sys.path.insert(0,'/home/jan/claude/bloodbowl')
import diag_rules_checks_20260812 as R
from collections import Counter
STANDING=0
st=Counter()
for path in sorted(glob.glob('corpus_baseline_20260819_data/*.json.gz')):
    r=R.load(path)
    ours = "home" if r.get("home_race")=="dwarf" else "away"
    theirs = "away" if ours=="home" else "home"
    logs=r["turn_logs"]
    if not logs: continue
    tre=[p for p in logs[0][f"{theirs}_players"] if "Treeman" in p.get("name","")]
    if not tre: continue
    tid=tre[0]["id"]
    st["her s Treemanem"]+=1
    # drive = úsek mezi resety (touchdown / změna půle)
    fell=False
    for i,S in enumerate(logs):
        reset = (i==0) or logs[i-1].get("touchdown") or (logs[i-1]["half"]!=S["half"])
        if reset: fell=False
        p=next((q for q in R.players(S,theirs) if q["id"]==tid), None)
        if p is None: continue
        if p["state"]!=STANDING:
            if not fell: st["sražení v drivu (příležitostí vstát)"]+=1
            fell=True
        elif fell:
            st["  …VSTAL V TÉMŽE DRIVU"]+=1
            fell=False
n=st["her s Treemanem"]; f=st["sražení v drivu (příležitostí vstát)"]; u=st["  …VSTAL V TÉMŽE DRIVU"]
print(f"her s Treemanem u soupeře: {n}")
print(f"  sražení (v rámci drivu):     {f:5d}   = {f/n:.2f} na hru")
print(f"  …VSTAL v témže drivu:        {u:5d}   {100*u/f:5.1f} % z {f}")
print(f"  …zůstal ležet do konce drivu:{f-u:5d}   {100*(f-u)/f:5.1f} % z {f}")
