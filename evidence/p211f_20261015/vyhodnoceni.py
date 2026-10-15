# P211e: párové vyhodnocení baze.pkl × nove.pkl (spouštět z adresáře evidence/p211f_20261015).
import pickle, math
B = {(r["prij"],r["kop"],r["nasi"],r["g"]): r for r in pickle.load(open("baze.pkl","rb"))}
N = {(r["prij"],r["kop"],r["nasi"],r["g"]): r for r in pickle.load(open("nove.pkl","rb"))}
keys = sorted(set(B)&set(N)); print("párů", len(keys))
def podil(rs, a, b): s=sum(r[a] for r in rs); t=sum(r[b] for r in rs); return f"{s}/{t}={100*s/max(t,1):.1f}%"
for skup in ["skaven","wood-elf",None]:
    ks=[k for k in keys if skup is None or k[2]==skup]
    b=[B[k] for k in ks]; n=[N[k] for k in ks]
    print("==", skup or "VŠE", len(ks))
    for a,c in [("vsichni_po_tz","vsichni_po"),("agil_po_tz","agil_po"),("turnover","tahu"),("zacal_tz_odesel","zacal_tz")]:
        print(f"  {a}/{c}: {podil(b,a,c)} -> {podil(n,a,c)}")
    for a in ["bit_vsichni","bit","bit_v_tz","srazen","zraneni","utek_konec","utek_turnover","utek_tahu","tahu"]:
        print(f"  {a}: {sum(r[a] for r in b)} -> {sum(r[a] for r in n)}")
    for a in ["td","td_soupere"]:
        d=[N[k][a]-B[k][a] for k in ks]; m=sum(d)/len(d); sd=math.sqrt(sum((x-m)**2 for x in d)/(len(d)-1)); 
        print(f"  {a}: {sum(B[k][a] for k in ks)} -> {sum(N[k][a] for k in ks)}  rozdíl {m:+.3f}±{sd/math.sqrt(len(d)):.3f}")
    od_b={};od_n={}
    for r in b:
        for d,c in r["obrana_dist"].items(): od_b[d]=od_b.get(d,0)+c
    for r in n:
        for d,c in r["obrana_dist"].items(): od_n[d]=od_n.get(d,0)+c
    print("  obrana_dist", dict(sorted(od_b.items())), "->", dict(sorted(od_n.items())))
