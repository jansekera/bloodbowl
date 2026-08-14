#!/usr/bin/env python3
"""OQ1 doplněk: prediktory uvnitř košů need0 + kontroly po kategoriích drivů.
(Tentýž kód, kterým vznikla tabulka v §2 a §5 reportu.)"""
import json, math, sys
from collections import defaultdict
DR = [json.loads(l) for l in open("scratchpad/open_q1_20260814/drive_checks.jsonl")]
GM = {g["game"]: g for g in (json.loads(l) for l in open("scratchpad/open_q1_20260814/games.jsonl"))}

def agg(d, sel=None, drop_last=True):
    ball = [t for t in d["turns"] if "k9a_got" in t]
    if drop_last and d["scored"] and ball: ball = ball[:-1]
    if sel: ball = ball[:sel]
    if not ball: return None
    r0 = [t["reach0"] for t in ball if t.get("reach0") is not None]
    return {"tempo": sum(t["k9a_got"] for t in ball)/len(ball),
            "blocks": sum(t["blocks"] for t in ball)/len(ball),
            "clean": sum(t.get("clean",0) for t in ball)/len(ball),
            "reach0": sum(r0)/len(r0) if r0 else None,
            "idle": sum(t["idle"] for t in ball)/len(ball),
            "fb2": sum(t["fb2"] for t in ball)/len(ball)}

def welch(yes,no):
    if len(yes)<5 or len(no)<5: return None
    my,mn=sum(yes)/len(yes),sum(no)/len(no)
    vy=sum((x-my)**2 for x in yes)/(len(yes)-1); vn=sum((x-mn)**2 for x in no)/(len(no)-1)
    se=math.sqrt(vy/len(yes)+vn/len(no))
    return my,mn,(my-mn)/se if se else 0.0

full = [d for d in DR if d["receiving"] and d["n_our_turns"]>=7]
print("=== prediktory UVNITŘ koše need0 (první 3 kola s míčem, bez posl. kola TD) ===")
for lo,hi,lab in [(0,2.61,"stihnutelné (<=2.61)"),(2.61,3.5,"2.61-3.5"),(3.5,99,">3.5")]:
    rows=[]
    for d in full:
        fh=d.get("first_hold")
        if not fh or not fh["turns_left"] or fh["turns_left"]<=0: continue
        n0=fh["dist"]/fh["turns_left"]
        if not (lo < n0 <= hi): continue
        a=agg(d,sel=3)
        if a: rows.append((d["scored"],a))
    print(f"\n-- need0 {lab}: n={len(rows)}, TD={sum(1 for s,_ in rows if s)} --")
    for k in ("tempo","blocks","clean","reach0","idle","fb2"):
        yes=[a[k] for s,a in rows if s and a[k] is not None]
        no=[a[k] for s,a in rows if not s and a[k] is not None]
        w=welch(yes,no)
        if w: print(f"   {k:<7} TD={w[0]:6.3f} bez={w[1]:6.3f} d={w[0]-w[1]:+6.3f} {w[2]:+5.1f}sigma")

print("\n=== kontroly po kategoriích drivů (plné přijímací, bez posl. kola TD) ===")
seq=defaultdict(int); cats=defaultdict(list); miss=0
for d in DR:
    if not d["receiving"]: continue
    g=GM.get(d["game"])
    i=seq[d["game"]]; seq[d["game"]]+=1
    if g is None or i>=len(g["drives"]): miss+=1; continue
    gd=g["drives"][i]
    if d["n_our_turns"]!= (gd.get("n_our_turns") or -1): miss+=1; continue
    if d["n_our_turns"]<7: continue
    a=agg(d)
    if a: cats[gd.get("subcat") or gd["cat"]].append(a)
print("nespárováno:",miss)
for c in ("A","C","D1","D2"):
    rs=cats[c]
    if not rs: continue
    def m(k):
        v=[a[k] for a in rs if a[k] is not None]; return sum(v)/len(v)
    print(f"  {c:3} n={len(rs):<5} tempo={m('tempo'):5.2f} bloky={m('blocks'):5.2f} "
          f"clean={m('clean'):5.2f} reach0={m('reach0'):5.2f} idle={m('idle'):5.2f}")
