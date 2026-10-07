import sys, pickle, collections
def rohy_pred(r):
    b=r["pred"]; return sum(1 for (dx,dy) in ((-1,-1),(1,-1),(-1,1),(1,1)) if b[3+dy][3+dx]=="o")
for f in sys.argv[1:]:
    R=pickle.load(open(f,"rb")); ma=[r for r in R if r["ma"] and r["ztrata"] is not None]; n=len(ma)
    vsech=[r for r in R if r["ma"]]
    td=sum(r["td"] for r in R)
    odesli=[r for r in ma if r["drzel"] and not any(m[2] for m in r["makra"]) and rohy_pred(r)==4 and r["rohy"]<4]
    plan4=[r for r in ma if r["drzel"] and any("klec napřed: plán" in l for l in r["dbg"])]
    foul=sum(1 for r in R for m in r["makra"] if m[1]=="FOUL"); foulto=sum(1 for r in R for m in r["makra"] if m[1]=="FOUL" and m[3])
    print(f"== {f}: TD {td}/80 · tahů {len(R)}, s turnoverem {sum(r['to'] for r in R)} ({100*sum(r['to'] for r in R)/len(R):.0f} %) · faulů {foul}, z toho turnover {foulto}")
    print(f"   po tahu s míčem (soupeř pak hrál) {n}: ztráta {sum(r['ztrata'] for r in ma)} = {100*sum(r['ztrata'] for r in ma)/n:.1f} %")
    for nz,fn in (("4 rohy",lambda r:r["rohy"]==4),("3 rohy",lambda r:r["rohy"]==3),("2 rohy",lambda r:r["rohy"]==2),("0–1 roh",lambda r:r["rohy"]<=1),("nosič vedle soupeře",lambda r:r["zona"]>0),("nosič u kraje (y 0,1,13,14)",lambda r:r["xy"][1] in (0,1,13,14))):
        q=[r for r in ma if fn(r)]; print(f"     {nz:28s}: {100*len(q)/n:4.0f} % tahů ({len(q):3d}) · ztráta {sum(r['ztrata'] for r in q):2d} = {100*sum(r['ztrata'] for r in q)/max(1,len(q)):3.0f} %")
    print(f"   klec stála se 4 rohy, nosič se nehnul, po tahu méně rohů: {len(odesli)} · plán se 4 rohy → po tahu 4 rohy: {sum(r['rohy']==4 for r in plan4)}/{len(plan4)}")
    print(f"   našich stojících hráčů po tahu (průměr): {sum(r['nasich_stoji'] for r in vsech)/len(vsech):.1f}")
