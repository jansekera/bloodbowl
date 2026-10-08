# Měřidlo pravidla „nosič po našem tahu: v kleci, nebo mimo dosah, nebo TD – nic čtvrtého“.  stav_cti.py soubor.pkl [...]
import sys, pickle, collections
for f in sys.argv[1:]:
    R=pickle.load(open(f,"rb")); V=collections.Counter(r["vysl"] for r in R)
    print(f"=== {f}: poločasů {len(R)} · TD {V['TD']} · soupeř první {V['TD soupeře']}")
    sk=collections.defaultdict(lambda:[0,0]); kdo=collections.Counter(); n=0; td=0
    for r in R:
        T=r["tahy"]
        for i,t in enumerate(T):
            if not t["my"]: continue
            s1=t["s1"]
            if s1["kdo"]=="TD": td+=1; continue
            if s1["kdo"]!="H": continue
            n+=1
            dosah_td = (25-s1["x"]) <= s1["ma"]+2
            if s1["rohy"]==4 and s1["vedle"]==0: k="1 klec 4 rohy, nikdo u nosiče"
            elif s1["v_dosahu"]==0: k="2 mimo dosah soupeře"
            elif s1["rohy"]==4: k="3 4 rohy, ale soupeř vedle nosiče"
            elif dosah_td: k="4 ANI-ANI, příště může skórovat"
            else: k="5 ANI-ANI, skórovat příště nemůže"
            nxt=T[i+1] if i+1<len(T) else None
            ztr = bool(nxt) and nxt["s1"]["kdo"] in ("A","zem","TD") and not nxt["my"]
            sk[k][0]+=1; sk[k][1]+=ztr
            if k[0] in "45":
                m=[x for x in t["makra"] if x[2]]
                kdo[(k[0], m[-1][0]+" "+m[-1][1]) if m else (k[0],"nosič se nehnul")]+=1
                sk[f"  {k[0]}: rohů {s1['rohy']}"][0]+=1; sk[f"  {k[0]}: rohů {s1['rohy']}"][1]+=ztr
    print(f"   TD v našem tahu {td} · tahů, po kterých míč držíme, {n}")
    for k in sorted(sk, key=lambda z:(z.strip()[0], z.startswith(' '), z)):
        a,b=sk[k]; print(f"   {k:40s} {a:4d} = {100*a/max(1,n):3.0f} % · míč pak ztracen {b:3d} = {100*b/max(1,a):3.0f} %")
    # přímé měřidlo „raději skórovat“: tah začal s nosičem na dosah zóny (do MA bez hodu / do MA+2 přes GFI)
    for jm,lim in (("do MA (bez GFI)",0),("do MA+2 (přes GFI)",2)):
        q=[(r,i,t) for r in R for i,t in enumerate(r["tahy"]) if t["my"] and t["s0"]["kdo"]=="H" and 0 < 25-t["s0"]["x"] <= t["s0"]["ma"]+lim and (lim==0 or 25-t["s0"]["x"] > t["s0"]["ma"])]
        tdn=sum(t["td"] for _,_,t in q); bezp=0; cil=0; ztr=0; pryc=0
        for r,i,t in q:
            if t["td"]: continue
            s1=t["s1"]
            if s1["kdo"]!="H": pryc+=1; continue
            if s1["v_dosahu"]==0 or (s1["rohy"]==4 and s1["vedle"]==0): bezp+=1; continue
            cil+=1; nxt=r["tahy"][i+1] if i+1<len(r["tahy"]) else None
            ztr += bool(nxt) and nxt["s1"]["kdo"] in ("A","zem","TD")
        print(f"   nosič na začátku tahu {jm:20s}: {len(q):3d} tahů · TD {tdn:3d} · neskóroval a je v bezpečí/kleci {bezp:3d} · NESKÓROVAL A ZŮSTAL CÍLEM {cil:3d} (pak ztráta {ztr}) · míč v tom tahu ztratil {pryc}")
    ho=[(r,i,t) for r in R for i,t in enumerate(r["tahy"]) if t["my"] and any(x[0]=="cage" and x[1]=="HAND_OFF_SCORE" for x in t["makra"])]
    print(f"   TD předávkou příkazem řadiče: {len(ho)} tahů · TD {sum(t['td'] for _,_,t in ho)} · turnover bez TD {sum(1 for _,_,t in ho if t['to'] and not t['td'])}")
    print("   kdo nosiče do ANI-ANI dovedl (skupina, poslední makro nosiče):", dict(kdo.most_common(8)))
