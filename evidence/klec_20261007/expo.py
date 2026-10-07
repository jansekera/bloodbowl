import sys, pickle, collections
for f in sys.argv[1:]:
    R=pickle.load(open(f,"rb"))
    EXP=collections.Counter(); LOSS=collections.Counter(); own=collections.Counter()
    for r in R:
        t=r["tahy"]
        for i,x in enumerate(t):
            if x["strana"]!="my": continue
            if x["pred"][0]=="náš" and x["po"][0] not in ("náš","TD"): own[x["to"][0][1] if x["to"] else "bez turnoveru"]+=1
            if x["po"][0]!="náš": continue
            rohy=x["po"][2]; zona=x["po"][3]>0; zved=x["pred"][0]!="náš"
            kdo="zvedl" if zved else ("hledání" if any(k!="cage" for k in x["nosic_hybal"]) else "s klecí" if x["nosic_hybal"] else "stál")
            nxt=t[i+1] if i+1<len(t) else None
            z = nxt is not None and nxt["strana"]=="on" and nxt["po"][0]!="náš"
            kraj = len(x["po"])>4 and x["po"][4] in (0,1,13,14)
            for k in (("rohy","4" if rohy==4 else "3" if rohy==3 else "2" if rohy==2 else "0-1"),("jak",kdo),("vedle soupeře",zona),("vše","")) + ((("u kraje",kraj),) if len(x["po"])>4 else ()):
                EXP[k]+=1; LOSS[k]+=z
    n=EXP[("vše","")]; V=collections.Counter(r["vysl"] for r in R)
    print(f"== {f}: TD {V['TD']}/{len(R)}, TD soupeře {V['TD soupeře']} · tahů končících s míčem {n} · ztráta v dalším tahu soupeře {LOSS[('vše','')]} = {100*LOSS[('vše','')]/n:.1f} % · ztráta ve vlastním tahu {sum(own.values())} {dict(own)}")
    for k in sorted(EXP, key=str):
        if k[0]=="vše": continue
        print(f"     {k[0]:14s} {str(k[1]):8s}: podíl {100*EXP[k]/n:4.0f} % tahů ({EXP[k]:3d}) · ztráta {LOSS[k]:3d} = {100*LOSS[k]/max(1,EXP[k]):3.0f} %")
