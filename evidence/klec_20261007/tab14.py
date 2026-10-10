import pickle, collections
def nacti(f, prij): return {(r["kop"],r["g"]):r for r in pickle.load(open(f,"rb")) if r["prij"]==prij}
S=(("starý engine","td_stara.pkl"),("ráno 7.10.","td_baze.pkl"),("odpoledne 7.10.","td_nova.pkl"),("večer 7.10.","td_v9.pkl"),("v14 (8.10., vše zapnuto)","td_v14.pkl"),("v18 (předávka, laťka 0,15)","td_v18.pkl"),("v21 (opravy z rozboru skavenů)","td_v21.pkl"))
for prij in ("dwarf","wood-elf","skaven","orc","human"):
    print(prij)
    for jm,f in S:
        d=nacti(f,prij); td=sum(r["td"] for r in d.values()); k=collections.Counter(r["kolo_td"] for r in d.values() if r["td"])
        cl=sum(r["ciste"] for r in d.values())/max(1,sum(r["s_micem"] for r in d.values())); to=sum(r["to"] for r in d.values())/sum(r["tahu"] for r in d.values())
        print(f"   {jm:26s} TD {td:3d} (kola 2–6: {sum(k[i] for i in range(1,7)):3d}, 7–8: {k[7]+k[8]:3d}) · soupeř první {sum(r['td_soupere'] for r in d.values()):3d} · 4 rohy {100*cl:3.0f} % · turnover {100*to:3.0f} %")
print("celkem")
for jm,f in S:
    R=pickle.load(open(f,"rb")); print(f"   {jm:26s} TD {sum(r['td'] for r in R):3d} · soupeř první {sum(r['td_soupere'] for r in R):3d}")
# trpaslíci proti orkům
for jm,f in S:
    d=[r for r in pickle.load(open(f,"rb")) if r["prij"]=="dwarf" and r["kop"]=="orc"]; print(f"   trpaslíci × orkové {jm:26s} TD {sum(r['td'] for r in d)}/{len(d)}")
