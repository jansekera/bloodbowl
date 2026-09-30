# P71 diag (30.09.2026): vysledek krmeni podle udalosti BLOODLUST_FEED
import sys
sys.path.insert(0, '.')
import bb_engine as b
N, OFF = int(sys.argv[1]), int(sys.argv[2])
W = '/home/jenda/claude/blood-bowl/weights_best.json'
fails = fed = starved = 0
for seed in range(N):
    r = b.simulate_game_logged(b.get_roster('vampire'), b.get_orc_roster(), 'macro_mcts', 'macro_mcts',
                               seed=OFF + seed, weights_path=W, mcts_iterations=50)
    for t in r.get_turn_logs():
        for e in t['events']:
            if e['type'] == 'SKILL' and e['roll'] == 60 and not e['success']: fails += 1
            if e['type'] == 'BLOODLUST_FEED':
                if e['success']: fed += 1
                else: starved += 1
print(f"her {N} | neuspechu {fails} | nakrmil se {fed} | rezervy {starved}")
