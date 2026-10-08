# Přímá měřidla jednotlivých úprav klece. prime.py KOŘEN vystup.pkl rasa [semínek=20]   (MASKA = které úpravy vypnout, obě strany)
import sys, os, pickle, multiprocessing as mp
KOREN=sys.argv[1]; VYST=sys.argv[2]; RASA=sys.argv[3]; N=int(sys.argv[4]) if len(sys.argv)>4 else 20
RASY=["dwarf","wood-elf","skaven","orc","human"]
sys.path.insert(0,KOREN)
def hra(a):
    prij,kop,g=a
    import bb_kontrola_tahu as kt
    bb=kt.bb; H,A=bb.TeamSide.HOME,bb.TeamSide.AWAY; ST=bb.PlayerState
    m=int(os.environ.get("MASKA","0"))
    if hasattr(bb,"set_cage_features_off"): bb.set_cage_features_off(m & ~1024); bb.set_foul_only_last((m & 1024)==0)   # starší enginy vypínače nemají
    def stav(s):
        if not s.ball.is_held: return dict(kdo="zem", x=s.ball.position.x, y=s.ball.position.y)
        n=s.get_player(s.ball.carrier_id); x,y=n.position.x,n.position.y
        if n.team_side!=H: return dict(kdo="A", x=x, y=y)
        opp=[p for p in kt.hraci(s) if p.team_side==A and p.state==ST.STANDING]
        d=[max(abs(p.position.x-x),abs(p.position.y-y)) for p in opp]
        return dict(kdo="H", x=x, y=y, id=n.id, ma=n.stats.movement,
                    rohy=sum(1 for p in kt.hraci(s) if p.team_side==H and p.state==ST.STANDING and p.id!=n.id and abs(p.position.x-x)==1 and abs(p.position.y-y)==1),
                    vedle=sum(1 for v in d if v==1), nejbl=min(d) if d else 99,
                    v_dosahu=sum(1 for p in opp if max(abs(p.position.x-x),abs(p.position.y-y))-1 <= p.stats.movement+2))
    s=bb.GameState(); bb.setup_half(s, bb.get_developed_roster(prij,1500), bb.get_developed_roster(kop,1500), A); s.kicking_team=A
    bb.simple_kickoff(s, bb.DiceRoller(3000+g))
    r=dict(kop=kop,g=g,vysl="bez TD",tahy=[])
    for tah in range(16):
        if s.phase!=bb.GamePhase.PLAY: break
        strana=s.active_team
        s0=stav(s)
        kroky,po,_=kt.plan_tahu(s, ai="macro_mcts", seed=50000+100*g+tah); kroky=list(kroky)
        td_h=po.home_team.score>s.home_team.score; td_a=po.away_team.score>s.away_team.score
        s1=stav(po) if po.phase==bb.GamePhase.PLAY and not (td_h or td_a) else dict(kdo="TD" if (td_h or td_a) else "konec")
        nos=s0.get("id",-1); hody=[]; mk=None; makra=[]
        for k in kroky:
            if "macro" in k: mk=k["macro"]; makra.append((mk["source"],mk["type"],mk["player"]==nos))
            for e in k["events"]:
                if e["type"] in ("DODGE","GFI"): hody.append((e["type"], e["player"]==nos or e["player"]==s1.get("id",-2), bool(e["success"])))
        r["tahy"].append(dict(my=strana==H, kolo=(s.home_team if strana==H else s.away_team).turn_number, s0=s0, s1=s1, hody=hody, makra=makra,
                              to=any(k["turnover"] for k in kroky), td=td_h or td_a))
        if td_h: r["vysl"]="TD"; break
        if td_a: r["vysl"]="TD soupeře"; break
        s=po
    return r
if __name__=="__main__":
    ukoly=[(RASA,k,g) for k in RASY if k!=RASA for g in range(N)]
    with mp.Pool(8) as pool: res=pool.map(hra, ukoly, chunksize=1)
    pickle.dump(res, open(VYST,"wb")); print("hotovo", len(res))
