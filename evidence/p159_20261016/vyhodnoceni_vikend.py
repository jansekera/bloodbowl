# P211e doplněk: párové vyhodnocení P159 (spouštět z evidence/p159_20261016).
import pickle
B = {(r["prij"],r["kop"],r["nasi"],r["g"]): r for r in pickle.load(open("vikend_baze.pkl","rb"))}
N = {(r["prij"],r["kop"],r["nasi"],r["g"]): r for r in pickle.load(open("vikend_nove.pkl","rb"))}
keys = sorted(set(B)&set(N)); print("párů", len(keys))
def sec(rs, kl):
    o={}
    for r in rs:
        for k,v in r[kl].items(): o[k]=o.get(k,0)+v
    return o
for skup in ["skaven","wood-elf",None]:
    ks=[k for k in keys if skup is None or k[2]==skup]; b=[B[k] for k in ks]; n=[N[k] for k in ks]
    tb=sum(r["tahu"] for r in b); tn=sum(r["tahu"] for r in n)
    print("==", skup or "VŠE", "tahů", tb, "->", tn)
    print("  turnover/tah %.3f -> %.3f" % (sum(r["turnover"] for r in b)/tb, sum(r["turnover"] for r in n)/tn))
    print("  aktivací/tah %.2f -> %.2f" % (sum(r["makra"] for r in b)/tb, sum(r["makra"] for r in n)/tn))
    tob=sum(r["turnover"] for r in b); ton=sum(r["turnover"] for r in n)
    print("  propadlé aktivace na turnover %.2f -> %.2f  (celkem %d -> %d, na 100 tahů %.1f -> %.1f)" % (sum(r["propadle"] for r in b)/max(tob,1), sum(r["propadle"] for r in n)/max(ton,1), sum(r["propadle"] for r in b), sum(r["propadle"] for r in n), 100*sum(r["propadle"] for r in b)/tb, 100*sum(r["propadle"] for r in n)/tn))
    print("  čas na tah %.1f s -> %.1f s" % (sum(r["cas"] for r in b)/tb, sum(r["cas"] for r in n)/tn))
    import math
    for a in ["td","td_soupere"]:
        d=[N[k][a]-B[k][a] for k in ks]; m=sum(d)/len(d); sd=math.sqrt(sum((x-m)**2 for x in d)/(len(d)-1))
        print(f"  {a}: {sum(B[k][a] for k in ks)} -> {sum(N[k][a] for k in ks)}  rozdíl {m:+.3f}±{sd/math.sqrt(len(d)):.3f}")
    pb,pn,fb,fn=sec(b,"pokus"),sec(n,"pokus"),sec(b,"neuspech"),sec(n,"neuspech")
    for t in sorted(set(pb)|set(pn), key=lambda t:-pb.get(t,0)):
        print(f"  {t:10s} pokusů/tah {pb.get(t,0)/tb:.2f} -> {pn.get(t,0)/tn:.2f}   neúspěch {fb.get(t,0)}/{pb.get(t,0)} -> {fn.get(t,0)}/{pn.get(t,0)}")
    ub,un=sec(b,"to_udalost"),sec(n,"to_udalost")
    print("  TURNOVER podle události (na 100 tahů):")
    for t in sorted(set(ub)|set(un), key=lambda t:-ub.get(t,0)): print(f"     {t:22s} {100*ub.get(t,0)/tb:5.1f} -> {100*un.get(t,0)/tn:5.1f}   ({ub.get(t,0)} -> {un.get(t,0)})")
    mb,mn=sec(b,"to_makro"),sec(n,"to_makro")
    print("  TURNOVER podle makra (na 100 tahů):")
    for t in sorted(set(mb)|set(mn), key=lambda t:-mb.get(t,0))[:10]: print(f"     {t:22s} {100*mb.get(t,0)/tb:5.1f} -> {100*mn.get(t,0)/tn:5.1f}")
