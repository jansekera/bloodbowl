"""EXPLORATORY (post-hoc, NOT pre-registered) decomposition of test-1 rows:
within-target-square vs across-target-square value spread, and search
behaviour on the value-informative subset (S_val >= 0.03). Run from the
directory containing vrows_*.jsonl. Results quoted in
evidence/fable_teacher_signal_report_20260810.md par 2.2."""
import json
import numpy as np

decs=[]
for fn in ["vrows_dwsk_a.jsonl","vrows_dwsk_b.jsonl","vrows_wesk.jsonl"]:
    for line in open(fn):
        line=line.strip()
        if not line: continue
        try: d=json.loads(line)
        except: continue
        if len(d["cands"])<3: continue
        race=d["race_h"] if d["persp"]=="home" else d["race_a"]
        decs.append(dict(race=race,
            v015=np.array([c["v015"] for c in d["cands"]]),
            vis=np.array([c["v"] for c in d["cands"]]),
            nv=np.array([c["n"] for c in d["cands"]]),
            sq=[(c["tx"],c["ty"]) for c in d["cands"]]))

def report(sel, label):
    nsq=np.array([len(set(d["sq"])) for d in sel])
    print(f"--- {label}: n={len(sel)}")
    print(f"  distinct target squares: median={np.median(nsq)}, share(==1)={np.mean(nsq==1):.3f}, share(>=3)={np.mean(nsq>=3):.3f}")
    s_within, s_across = [], []
    for d in sel:
        groups={}
        for s,v in zip(d["sq"], d["v015"]): groups.setdefault(s,[]).append(v)
        w=[max(g)-min(g) for g in groups.values() if len(g)>=2]
        if w: s_within.append(max(w))
        if len(groups)>=2:
            means=[np.mean(g) for g in groups.values()]
            s_across.append(max(means)-min(means))
    print(f"  S_within-square: median={np.median(s_within):.4f} (n={len(s_within)})")
    print(f"  S_across-square: median={np.median(s_across):.4f} (n={len(s_across)})")
    sval=np.array([d["v015"].max()-d["v015"].min() for d in sel])
    sub=[d for d,s in zip(sel,sval) if s>=0.03]
    if sub:
        ss=np.mean([d["vis"].max()-d["vis"].min() for d in sub])
        cm=np.mean([(d["nv"]==d["nv"].max()).sum()/len(d["nv"]) for d in sub])
        hit=np.mean([int((d["nv"]==d["nv"].max())[int(np.argmax(d["v015"]))]) for d in sub])
        print(f"  subset S_val>=0.03: n={len(sub)} ({len(sub)/len(sel):.2%}), search_spread={ss:.4f}, chance_maxset={cm:.3f}, P(value-argmax in max-visit set)={hit:.3f}")

for race in ["dwarf","wood-elf","skaven"]:
    report([d for d in decs if d["race"]==race], race)
report(decs,"all")
