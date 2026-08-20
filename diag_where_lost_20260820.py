"""KDE ZTRÁCÍME ZÁPASY — nedáme, nebo dostaneme? (20.08.2026)

Vzniklo z uživatelovy otázky „s tím tématem L vs 2 sloupce — předbíháme moc?".
Prioritu mezi útokem a obranou nemá rozhodnout názor, ale rozpad nevyhraných
zápasů podle toho, KDO nedal.

⚠️ ZNÁMÁ VADA: rozpad podle rasy soupeře NEFUNGUJE -- jméno týmu se čte
z prvních čtyř hráčů a u všech ras vyjde "Lineman". Na hlavní otázku to nemá
vliv, ale číslo v tom sloupci se NESMÍ číst.
"""
import sys, glob, json, gzip
from collections import Counter
sys.path.insert(0,'/home/jan/claude/bloodbowl')
import diag_rules_checks_20260812 as R
st=Counter(); byopp={}
for path in sorted(glob.glob('corpus_baseline_20260819_data/*.json.gz')):
    r=R.load(path)
    L=r["turn_logs"]
    if not L: continue
    first=L[0]
    def names(s): return " ".join(p["name"] for p in first[f"{s}_players"][:4])
    ours = "home" if ("Longbeard" in names("home") or "Troll Slayer" in names("home")) else (
           "home" if False else ("away" if ("Longbeard" in names("away") or "Troll Slayer" in names("away")) else None))
    if ours is None: continue
    theirs = "away" if ours=="home" else "home"
    last=L[-1]
    us=last[f"{ours[0]=='h' and 'home' or 'away'}_score"] if False else last[f"{ours}_score"]
    them=last[f"{theirs}_score"]
    # rasa soupeře
    opp = names(theirs).split()[0]
    st["her"]+=1; st["našich TD"]+=us; st["jejich TD"]+=them
    if us>them: st["V"]+=1
    elif us<them: st["P"]+=1
    else: st["R"]+=1
    if us==0: st["my 0 TD"]+=1
    if them==0: st["oni 0 TD"]+=1
    if us==0 and them==0: st["0:0"]+=1
    # kde se ztrácí: neremízové a prohrané
    if us<=them:
        if us==0 and them==0: st["  nevyhráli: 0:0 (nedali jsme)"]+=1
        elif us==0: st["  nevyhráli: my 0, oni dali"]+=1
        elif us==them: st["  nevyhráli: remíza se skóre"]+=1
        else: st["  nevyhráli: dali jsme, ale míň"]+=1
    d=byopp.setdefault(opp,Counter()); d["n"]+=1; d["us"]+=us; d["them"]+=them
    if us>them: d["V"]+=1
    elif us<them: d["P"]+=1
    else: d["R"]+=1
n=st["her"]
print(f"her: {n}")
print(f"  naše TD/hru   {st['našich TD']/n:.3f}    jejich TD/hru {st['jejich TD']/n:.3f}")
print(f"  V {st['V']} ({100*st['V']/n:.1f} %)  R {st['R']} ({100*st['R']/n:.1f} %)  P {st['P']} ({100*st['P']/n:.1f} %)")
print(f"  MY 0 TD:  {st['my 0 TD']:5d}  {100*st['my 0 TD']/n:5.1f} %")
print(f"  ONI 0 TD: {st['oni 0 TD']:5d}  {100*st['oni 0 TD']/n:5.1f} %")
print(f"  0:0:      {st['0:0']:5d}  {100*st['0:0']/n:5.1f} %")
nw = n - st['V']
print(f"\n  NEVYHRANÝCH {nw} ({100*nw/n:.1f} %), z toho:")
for k in ["  nevyhráli: 0:0 (nedali jsme)","  nevyhráli: my 0, oni dali","  nevyhráli: remíza se skóre","  nevyhráli: dali jsme, ale míň"]:
    print(f"   {k[13:]:34s} {st[k]:5d}  {100*st[k]/nw:5.1f} % z nevyhraných")
print("\n  podle soupeře:")
print("  %-14s %6s %8s %8s %7s %7s %7s"%("soupeř","her","naše TD","jejich","V %","R %","P %"))
for k,d in sorted(byopp.items(), key=lambda x:-x[1]["n"]):
    m=d["n"]
    print("  %-14s %6d %8.2f %8.2f %6.1f %6.1f %6.1f"%(k,m,d["us"]/m,d["them"]/m,100*d["V"]/m,100*d["R"]/m,100*d["P"]/m))
