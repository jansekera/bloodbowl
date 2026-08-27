#!/usr/bin/env python3
"""Q16 strop: jak často je obklíčení (L = boxing-in) PROVEDITELNÉ v jednom kole?

Definice L (uživatel 10.08.): „není to rozestavení ani screen, ale ODEBÍRÁNÍ
ÚNIKOVÝCH POLÍ — vytlačit soupeře k lajně a obklopit půlkruhem L/U, aby neměl kam."
⇒ Lajna dělá půlku práce: pole mimo hřiště se jako úniková nepočítají.

Měří se NAŠE OBRANNÁ kola (míč drží soupeř):
  úniková pole  = volná pole sousedící s jejich nosičem, NA HŘIŠTI
  odebratelné   = dosáhne tam naše stojící tělo (Chebyshev <= MA, horní mez)
  L proveditelné= všechna úniková pole jsou odebratelná (a máme dost těl)

⚠️ HORNÍ MEZ, a volná: dosah bez TZ/dodge/GFI, těla se nepřekrývají jen podle
   greedy přiřazení, a neptá se to na CENU (co ta těla přestanou dělat jinde).
   ⇒ Když i tak vyjde nízko, signál je k ničemu. Když vysoko, chce to těsnější číslo.
"""
import gzip, json, glob, sys
from collections import Counter

def cheb(ax,ay,bx,by): return max(abs(ax-bx),abs(ay-by))
def onp(x,y): return 0<=x<=25 and 0<=y<=14

def main(limit):
    T=Counter(); dist=Counter()
    for f in sorted(glob.glob('blitzlanding_replic_20260825_corpus_data/g*.json.gz'))[:limit]:
        d=json.load(gzip.open(f))
        dw='home' if d['home_race']=='dwarf' else 'away'
        opp='away' if dw=='home' else 'home'
        for t in d['turn_logs']:
            if t['active_team']!=dw: continue          # naše kolo
            cid=t.get('ball_carrier_id',-1)
            if not cid or cid<1: continue
            their=[p for p in t[opp+'_players'] if p['state']==0]
            car=[p for p in their if p['id']==cid]
            if not car: continue                        # míč nedrží soupeř => nejsme v obraně
            c=car[0]
            T['obrannych_kol']+=1
            occ={(p['x'],p['y']) for p in t[dw+'_players']+t[opp+'_players'] if p['state']==0}
            mine=[p for p in t[dw+'_players'] if p['state']==0]
            # úniková pole nosiče: volná, NA HŘIŠTI (lajna je zdarma)
            esc=[(c['x']+dx,c['y']+dy) for dx in(-1,0,1) for dy in(-1,0,1)
                 if (dx or dy) and onp(c['x']+dx,c['y']+dy) and (c['x']+dx,c['y']+dy) not in occ]
            T['souc_unikovych']+=len(esc)
            dist[min(len(esc),8)]+=1
            if not esc:
                T['UZ_uzavren']+=1; continue
            # kolik z nich umíme odebrat (greedy, jedno tělo = jedno pole)
            used=set(); covered=0
            for q in esc:
                best,bd=None,99
                for b in mine:
                    if b['id'] in used: continue
                    dd=cheb(b['x'],b['y'],q[0],q[1])
                    if dd> b.get('ma',6): continue
                    if dd<bd: bd,best=dd,b['id']
                if best is not None: used.add(best); covered+=1
            if covered==len(esc): T['L_PROVEDITELNE']+=1
            elif covered>=len(esc)-1: T['chybi_1_pole']+=1
    n=T['obrannych_kol'] or 1
    print(f"trpaslík v OBRANĚ (soupeř drží míč), {limit} her: {T['obrannych_kol']} kol\n")
    print(f"  ⌀ únikových polí nosiče: {T['souc_unikovych']/n:.2f}")
    print(f"  už uzavřen (0 únikových)          {T['UZ_uzavren']:6d}  {100*T['UZ_uzavren']/n:5.1f} %")
    print(f"  ⭐ L PROVEDITELNÉ v jednom kole    {T['L_PROVEDITELNE']:6d}  {100*T['L_PROVEDITELNE']/n:5.1f} %")
    print(f"     ...chybí jediné pole            {T['chybi_1_pole']:6d}  {100*T['chybi_1_pole']/n:5.1f} %")
    print(f"\n  rozdělení počtu únikových polí:")
    for k in sorted(dist): print(f"     {k}{'+' if k==8 else ' '}: {dist[k]:6d}  {100*dist[k]/n:5.1f} %")

if __name__=='__main__':
    main(int(sys.argv[1]) if len(sys.argv)>1 else 400)
