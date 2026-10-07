# Přímá měřidla mechanismů. mech.py KOŘEN vystup.pkl rasa [semínek=20]
import sys, os, pickle, multiprocessing as mp
KOREN=sys.argv[1]; VYST=sys.argv[2]; RASA=sys.argv[3]; N=int(sys.argv[4]) if len(sys.argv)>4 else 20
RASY=["dwarf","wood-elf","skaven","orc","human"]
sys.path.insert(0,KOREN)
def hra(a):
    prij,kop,g=a
    import bb_kontrola_tahu as kt
    bb=kt.bb; H,A=bb.TeamSide.HOME,bb.TeamSide.AWAY; ST=bb.PlayerState
    s=bb.GameState(); bb.setup_half(s, bb.get_developed_roster(prij,1500), bb.get_developed_roster(kop,1500), A); s.kicking_team=A
    bb.simple_kickoff(s, bb.DiceRoller(3000+g))
    out=[]
    for tah in range(16):
        if s.phase!=bb.GamePhase.PLAY: break
        strana=s.active_team
        kroky,po,_=kt.plan_tahu(s, ai="macro_mcts", seed=50000+100*g+tah); kroky=list(kroky)
        r=dict(my=strana==H, g=g, kop=kop, dodge=0, dodge_fail=0, gfi=0, gfi_fail=0, to=False, to_cim="", zvedl=False, rohy=-1, nosic_hledani=False, td=False)
        drzel=s.ball.is_held and s.get_player(s.ball.carrier_id).team_side==strana
        nos=s.ball.carrier_id if drzel else -1
        posl=None; m=None
        for k in kroky:
            if "macro" in k:
                m=k["macro"]
                if m["player"]==nos and m["type"] in ("ADVANCE","REPOSITION") and m["source"]!="cage": r["nosic_hledani"]=True
            for e in k["events"]:
                if e["type"]=="DODGE": r["dodge"]+=1; r["dodge_fail"]+= (not e["success"])
                if e["type"]=="GFI": r["gfi"]+=1; r["gfi_fail"]+= (not e["success"])
                if e["type"]=="TURNOVER": r["to"]=True; r["to_cim"]=(posl or "?")+"/"+(m["type"] if m else "?")
                elif e["type"] not in ("MOVE","SKILL"): posl=e["type"]
        ma=po.phase==bb.GamePhase.PLAY and po.ball.is_held and po.get_player(po.ball.carrier_id).team_side==strana
        if ma:
            n=po.get_player(po.ball.carrier_id); x,y=n.position.x,n.position.y
            r["rohy"]=sum(1 for p in kt.hraci(po) if p.team_side==strana and p.state==ST.STANDING and p.id!=n.id and abs(p.position.x-x)==1 and abs(p.position.y-y)==1)
            r["zvedl"]=not drzel
        r["drzel"]=drzel
        r["td"]= po.home_team.score+po.away_team.score > s.home_team.score+s.away_team.score
        out.append(r)
        if r["td"]: break
        s=po
    return out
if __name__=="__main__":
    ukoly=[(RASA,k,g) for k in RASY if k!=RASA for g in range(N)]
    with mp.Pool(8) as pool: res=pool.map(hra, ukoly, chunksize=1)
    pickle.dump(res, open(VYST,"wb")); print("hotovo", len(res))
