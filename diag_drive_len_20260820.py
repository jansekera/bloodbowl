import sys, glob
sys.path.insert(0,'/home/jan/claude/bloodbowl')
import diag_rules_checks_20260812 as R
from collections import Counter
st=Counter(); lens=[]; lens_all=[]
for path in sorted(glob.glob('corpus_baseline_20260819_data/*.json.gz')):
    r=R.load(path)
    ours = "home" if r.get("home_race")=="dwarf" else "away"
    fwd = 1 if ours=="home" else -1
    logs=r["turn_logs"]
    # drive = souvislý úsek našich kol s míčem; konec = TD, změna půle, ztráta míče
    cur=0; start_x=None
    for i,S in enumerate(logs):
        if S["active_team"]!=ours: continue
        car=next((p for p in R.players(S,ours) if p["has_ball"]), None)
        if car is None:
            if cur: lens_all.append(cur)
            cur=0; start_x=None; continue
        if cur==0: start_x=car["x"]
        cur+=1
        if S.get("touchdown"):
            lens.append(cur); lens_all.append(cur)
            st["TD drivů"]+=1
            st["součet kol na TD"]+=cur
            if start_x is not None:
                st["součet ujetých polí"]+=abs(car["x"]-start_x)
            cur=0; start_x=None
    if cur: lens_all.append(cur)
print("TD drivů:", st["TD drivů"])
if lens:
    lens.sort()
    print(f"  ⌀ kol na TD          {sum(lens)/len(lens):.2f}")
    print(f"  medián               {lens[len(lens)//2]}")
    print(f"  min / max            {lens[0]} / {lens[-1]}")
    import collections
    d=collections.Counter(lens)
    print("  rozložení kol na TD:")
    for k in sorted(d):
        print(f"    {k:2d} kol  {d[k]:5d}  {100*d[k]/len(lens):5.1f} %")
print(f"\n  všech drivů s míčem: {len(lens_all)}, ⌀ délka {sum(lens_all)/len(lens_all):.2f} kol")
