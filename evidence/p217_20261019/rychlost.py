# P217 (19.10.2026): čas na náš tah s P217 (3) zapnutým / vypnutým, tytéž stavy a semínka.
import sys, os, time
sys.path.insert(0, "/home/jenda/claude/blood-bowl")
import bb_kontrola_tahu as kt
bb = kt.bb; H, A = bb.TeamSide.HOME, bb.TeamSide.AWAY
casy = {0: [], 16: []}
for nasi in ["skaven", "wood-elf"]:
    for g in range(3):
        s = bb.GameState(); bb.setup_half(s, bb.get_developed_roster(nasi, 1500), bb.get_developed_roster("dwarf", 1500), A)
        s.kicking_team = A; bb.simple_kickoff(s, bb.DiceRoller(3000 + g))
        for tah in range(16):
            if s.phase != bb.GamePhase.PLAY: break
            nas = s.active_team == H
            for off in (0, 16):
                bb.set_extra_features_off(off)
                t0 = time.time(); kroky, po_, _ = kt.plan_tahu(s, ai="macro_mcts", seed=50000 + 100 * g + tah)
                if nas: casy[off].append(time.time() - t0)
                if off == 0: po = po_
            if po.home_team.score != s.home_team.score or po.away_team.score != s.away_team.score: break
            s = po
for off, v in casy.items():
    v = sorted(v); print("P217", "vypnuto" if off else "zapnuto", f"tahů {len(v)} průměr {sum(v)/len(v):.2f} s medián {v[len(v)//2]:.2f} max {v[-1]:.2f}")
