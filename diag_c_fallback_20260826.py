#!/usr/bin/env python3
"""Složka (C) P38: jak často záložní smyčka v `expandAdvance` stáhne steps na 0
   a nosič se NEHNE VŮBEC -- a je to týž jev jako "volný nosič stojí" (M11b)?

Přepis smyčky (macro_actions.cpp:1558-1566):
    while (steps > 0 && (obsazeno(target) || tacklezony(target) > 0)) {
        --steps; targetX = carrier.x + dx*steps;      // ⇐ MĚNÍ SE JEN X
    }
    if (steps <= 0) return result;                    // ⇐ nosič se nehne

⭐ Smyčka hledá JEN PO PŘÍMCE: y zůstává na biasované hodnotě. Když je celá
přímka vpřed obsazená nebo v TZ, ADVANCE rezignuje -- i kdyby bylo volno vedle.

⚠️ APROXIMACE: `steps` = carrierStallAwareSteps(stav skóre, zbývající kola).
Tady se bere `movementRemaining` jako HORNÍ MEZ ⇒ smyčka má VÍC pokusů než
v enginu, takže "dojde na 0" je zde SPODNÍ odhad. Skutečnost je horší, ne lepší.
"""
import gzip, json, glob, sys
from collections import Counter

def cheb(ax,ay,bx,by): return max(abs(ax-bx),abs(ay-by))

def main(limit):
    T=Counter()
    for f in sorted(glob.glob('blitzlanding_replic_20260825_corpus_data/g*.json.gz'))[:limit]:
        d=json.load(gzip.open(f)); L=d['turn_logs']
        for side in ('home','away'):
            if d[side+'_race']!='dwarf': continue
            opp='away' if side=='home' else 'home'; dx=1 if side=='home' else -1
            for i,t in enumerate(L):
                if t['active_team']!=side: continue
                cid=t.get('ball_carrier_id',-1)
                if not cid or cid<1: continue
                me=[p for p in t[side+'_players'] if p['id']==cid]
                if not me or me[0]['state']!=0: continue
                c=me[0]; cx,cy=c['x'],c['y']
                mine=[p for p in t[side+'_players'] if p['state']==0]
                theirs=[p for p in t[opp+'_players'] if p['state']==0]
                occ={(p['x'],p['y']) for p in mine+theirs}
                T['kol_s_nosicem']+=1
                # y bias jako v enginu
                ty = cy + (1 if cy<5 else (-1 if cy>9 else 0))
                steps=max(0,int(c.get('ma',6)))
                # záložní smyčka: stahuj po PŘÍMCE
                while steps>0:
                    tx=max(1,min(24,cx+dx*steps))
                    tz=sum(1 for p in theirs if cheb(p['x'],p['y'],tx,ty)==1)
                    if (tx,ty) not in occ and tz==0: break
                    steps-=1
                stuck = steps<=0
                if stuck: T['C_smycka_stahla_na_0']+=1
                # byla alternativa VEDLE přímky? (to, co přidává složka A)
                if stuck:
                    alt=False
                    for ox in range(1,int(c.get('ma',6))+1):
                        for oy in (-2,-1,1,2):
                            qx,qy=cx+dx*ox, cy+oy
                            if not (0<=qx<26 and 0<=qy<15): continue
                            if (qx,qy) in occ: continue
                            if sum(1 for p in theirs if cheb(p['x'],p['y'],qx,qy)==1): continue
                            alt=True; break
                        if alt: break
                    if alt: T['  ...a PŘITOM bylo volno VEDLE přímky']+=1
                # skutečně se nehnul?
                nxt=next((L[j] for j in range(i+1,len(L)) if L[j]['active_team']==side), None)
                moved=None
                if nxt:
                    m2=[p for p in nxt[side+'_players'] if p['id']==cid]
                    if m2: moved=(m2[0]['x'],m2[0]['y'])!=(cx,cy)
                if moved is False:
                    T['nosic_STAL']+=1
                    if stuck: T['  ...a smycka ho vysvetluje']+=1
    n=T['kol_s_nosicem'] or 1
    print(f"trpaslík, {limit} her — kol s nosičem: {T['kol_s_nosicem']}\n")
    for k in ('C_smycka_stahla_na_0','  ...a PŘITOM bylo volno VEDLE přímky','nosic_STAL','  ...a smycka ho vysvetluje'):
        base = T['nosic_STAL'] if k=='  ...a smycka ho vysvetluje' else (T['C_smycka_stahla_na_0'] if k.startswith('  ...a PŘITOM') else n)
        print(f"  {k:42s} {T[k]:6d}  {100*T[k]/max(1,base):5.1f} %")

if __name__=='__main__':
    main(int(sys.argv[1]) if len(sys.argv)>1 else 300)
