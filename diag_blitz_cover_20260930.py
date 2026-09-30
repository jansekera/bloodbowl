# Kryti po blitzu (30.09.2026): kolikrat se hrac po sve rane v tomtez kole jeste pohnul?
import sys
sys.path.insert(0, sys.argv[4] if len(sys.argv) > 4 else '.')
import bb_engine as b
W = '/home/jenda/claude/blood-bowl/weights_best.json'
ITER = int(sys.argv[2]) if len(sys.argv) > 2 else 50
OFF = int(sys.argv[3]) if len(sys.argv) > 3 else 2000
N = int(sys.argv[1]) if len(sys.argv) > 1 else 40
blocks = moved_after = 0
for seed in range(N):
    r = b.simulate_game_logged(b.get_roster('wood-elf'), b.get_orc_roster(), 'macro_mcts', 'macro_mcts', seed=OFF + seed, weights_path=W, mcts_iterations=ITER)
    for t in r.get_turn_logs():
        hit = set()
        for e in t['events']:
            if e['type'] == 'BLOCK':
                hit.add(e['player_id']); blocks += 1
            elif e['type'] == 'MOVE' and e['player_id'] in hit:
                moved_after += 1; hit.discard(e['player_id'])
print(f"her {N} | ran {blocks} | pohyb po vlastni rane {moved_after}")
