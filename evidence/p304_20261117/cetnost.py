# P304 bod 5, pozitivní kontrola četnosti: v kolika stavech s volným míčem změní oprava šanci soupeře zvednout míč
# (`opponent_pickup_chance` s ramenem kP304bActivation a bez). Hry greedy × greedy, soupeř s velkými hráči.
import sys, os
sys.path.insert(0, sys.argv[1]); import bb_kontrola_tahu as kt; bb = kt.bb
N = int(sys.argv[2]) if len(sys.argv) > 2 else 4
stavu = jine = 0; rozdil = 0.0
for nasi in ["skaven", "woodelf", "human"]:
    for sou in ["woodelf", "skaven", "norse", "human"]:
        if sou == nasi: continue
        for g in range(N):
            s = bb.GameState(); bb.setup_half(s, bb.get_developed_roster(nasi, 1500), bb.get_developed_roster(sou, 1500), bb.TeamSide.AWAY)
            bb.simple_kickoff(s, bb.DiceRoller(5000 + g))
            for tah in range(32):
                if s.phase != bb.GamePhase.PLAY: break
                strana = s.active_team
                kroky, po, _ = kt.plan_tahu(s, ai="greedy", seed=1000 * g + tah)
                if po.phase == bb.GamePhase.PLAY and not po.ball.is_held and po.ball.is_on_pitch():
                    oop = bb.TeamSide.AWAY if strana == bb.TeamSide.HOME else bb.TeamSide.HOME
                    bb.set_extra_features_off(0); a = bb.opponent_pickup_chance(po, oop)
                    bb.set_extra_features_off(1 << 38); b = bb.opponent_pickup_chance(po, oop); bb.set_extra_features_off(0)
                    stavu += 1
                    if abs(a - b) > 1e-12: jine += 1; rozdil += b - a
                if po.home_team.score != s.home_team.score or po.away_team.score != s.away_team.score: break
                s = po
print(f"stavů s volným míčem {stavu}, šance změněna v {jine}, průměrné snížení tam {rozdil / max(jine, 1):.3f}")
