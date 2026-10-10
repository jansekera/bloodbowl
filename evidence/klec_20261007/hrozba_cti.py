# Stav po našem tahu podle měřítka enginu (blitzThreat): bezpečí ≤ 0,15 × vystaven; a jak často se pak míč ztratí.
import sys, pickle, collections
for f in sys.argv[1:]:
    R=pickle.load(open(f,"rb")); S=collections.defaultdict(lambda:[0,0]); n=0
    for r in R:
        T=r["tahy"]
        for i,t in enumerate(T):
            if not t["my"] or t["s1"]["kdo"]!="H" or t["s1"].get("hrozba") is None: continue
            h=t["s1"]["hrozba"]; s1=t["s1"]; n+=1
            nxt=T[i+1] if i+1<len(T) else None
            z=bool(nxt) and (not nxt["my"]) and nxt["s1"]["kdo"] in ("A","zem","TD")
            k=("1 hrozba 0 (nikdo nedosáhne / jen přes těžký hod)" if h<=0.001 else "2 hrozba do 0,05" if h<=0.05 else "3 hrozba 0,05–0,15 (bezpečí)" if h<=0.15 else "4 hrozba 0,15–0,35 (rána na 1 kostku)" if h<=0.35 else "5 hrozba nad 0,35 (2+ kostky)")
            S[k][0]+=1; S[k][1]+=z
            S["  "+k[0]+f": rohů {s1['rohy']}"][0]+=1; S["  "+k[0]+f": rohů {s1['rohy']}"][1]+=z
    bez=sum(v[0] for k,v in S.items() if k[0] in "123"); zb=sum(v[1] for k,v in S.items() if k[0] in "123")
    vy=sum(v[0] for k,v in S.items() if k[0] in "45"); zv=sum(v[1] for k,v in S.items() if k[0] in "45")
    print(f"=== {f}: tahů s míčem po tahu {n} · V BEZPEČÍ (≤ 0,15) {100*bez/n:.0f} % (ztráta {100*zb/max(1,bez):.0f} %) · VYSTAVEN {100*vy/n:.0f} % (ztráta {100*zv/max(1,vy):.0f} %)")
    for k in sorted(S, key=lambda z:(z.strip()[0], z.startswith(' '), z)):
        a,b=S[k]; print(f"   {k:52s} {a:4d} = {100*a/n:3.0f} % · ztráta {b:3d} = {100*b/max(1,a):3.0f} %")
