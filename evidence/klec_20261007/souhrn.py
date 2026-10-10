import pickle, collections, os, sys
def radek(f):
    R=pickle.load(open(f,"rb")); V=collections.Counter(r["vysl"] for r in R)
    n=cl=mimo=ani=ztr_ani=ztr=ho=hotd=0; pozd=0; tahu=to=0
    for r in R:
        T=r["tahy"]
        if r["vysl"]=="TD":
            k=[t["kolo"] for t in T if t["my"] and t["td"]]; pozd += bool(k) and k[-1]>=7
        for i,t in enumerate(T):
            if not t["my"]: continue
            tahu+=1; to+=bool(t["to"])
            if any(x[0]=="cage" and x[1]=="HAND_OFF_SCORE" for x in t["makra"]): ho+=1; hotd+=bool(t["td"])
            s1=t["s1"]
            if s1["kdo"]!="H": continue
            n+=1; nxt=T[i+1] if i+1<len(T) else None
            z=bool(nxt) and (not nxt["my"]) and nxt["s1"]["kdo"] in ("A","zem","TD"); ztr+=z
            if s1["rohy"]==4 and s1["vedle"]==0: cl+=1
            elif s1["v_dosahu"]==0: mimo+=1
            elif s1["rohy"]==4: pass
            else: ani+=1; ztr_ani+=z
    return f"TD {V['TD']:2d} (7.–8.: {pozd:2d}) · soupeř první {V['TD soupeře']:2d} · čistá klec {100*cl/n:3.0f} % · mimo dosah {100*mimo/n:2.0f} % · ani-ani {100*ani/n:3.0f} % (ztráta {100*ztr_ani/max(1,ani):2.0f} %) · ztráta na tah s míčem {100*ztr/n:4.1f} % · turnover {100*to/tahu:2.0f} % · TD předávkou {hotd}/{ho}"
for rasa in ("dwarf","skaven","wood-elf"):
    print(rasa)
    for jm,f in (("starý engine",f"pr_{rasa}_stary.pkl"),("odpoledne 7.10.",f"pr_{rasa}_odpoledne.pkl"),("v15 bez P178",f"pr_{rasa}_98304.pkl"),("v15 s P178",f"pr_{rasa}_0.pkl"),("v16 opravy review",f"pr_{rasa}_v16.pkl"),("v17 jedno měřítko",f"pr_{rasa}_v17.pkl"),("v18 předávka+laťka",f"pr_{rasa}_v18.pkl"),("v19 P187",f"pr_{rasa}_v19.pkl"),("v20 zákaz v kořeni",f"pr_{rasa}_v20.pkl"),("v21 P190",f"pr_{rasa}_v21.pkl")):
        if os.path.exists(f): print(f"   {jm:20s} {radek(f)}")
