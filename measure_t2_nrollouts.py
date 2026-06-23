"""Team1 v2 / T2 STEP 2 measurement — does averaging K rollouts cut Q-variance
and concentrate MCTS visits? Heads connected (vf=0.5, pol=0.15), exploration_c=0.5
(the step-1 config win). Vary n_rollouts. Compare H_norm to step-1 best (0.812).

Run: PYTHONPATH=$(pwd)/engine/build:$(pwd)/python venv/bin/python3 -u /tmp/team1v2_nrollout_sweep.py
"""
import sys, math, statistics as st
sys.path.insert(0, 'engine/build'); sys.path.insert(0, 'python')
import bb_engine

AWAY = ['orc', 'skaven', 'dwarf', 'wood-elf']
TV = 1200; W = 'weights_best.json'
VFB, PB, EPS, C = 0.5, 0.15, 0.1, 0.5

# (sims, n_rollouts, games)
CONFIGS = [
    (400, 1, 3),   # reference at c=0.5 (≈ step-1)
    (400, 4, 3),
    (400, 8, 3),
    (800, 4, 2),   # best sims + averaging
]


def H(ps):
    return -sum(p * math.log(p) for p in ps if p > 0)


def measure(sims, K, games):
    Hn, top1, top3 = [], [], []
    near = nd = 0
    for g in range(games):
        hr = bb_engine.get_developed_roster('human', TV)
        ar = bb_engine.get_developed_roster(AWAY[g % len(AWAY)], TV)
        lg = bb_engine.simulate_game_logged(
            hr, ar, 'macro_mcts', 'macro_mcts', seed=12000 + g,
            weights_path=W, policy_weights_path=W, epsilon=EPS,
            mcts_iterations=sims, policy_blend=PB, vf_blend=VFB,
            exploration_c=C, n_rollouts=K)
        for d in lg.get_policy_decisions():
            vf = sorted((float(v['visit_fraction']) for v in d['visits']), reverse=True)
            s = sum(vf)
            if s <= 0 or len(vf) < 2:
                continue
            vf = [x / s for x in vf]; n = len(vf)
            Hn.append(H(vf) / math.log(n)); top1.append(vf[0]); top3.append(sum(vf[:3]))
            if vf[0] < 1.25 * vf[1]:
                near += 1
            nd += 1
    print(f'  [sims={sims} K={K}] {nd} decisions', flush=True)
    return dict(sims=sims, K=K, nd=nd, Hn=st.mean(Hn),
                top1=st.mean(top1), top3=st.mean(top3), near=100*near/max(nd,1))


rows = []
for sims, K, games in CONFIGS:
    print(f'== measuring sims={sims} n_rollouts={K} (c={C}, vf={VFB} pol={PB}, {games} games) ==', flush=True)
    rows.append(measure(sims, K, games))

print('\n===== T2 STEP2: does averaging K rollouts concentrate visits? =====')
print(f"{'sims':>6} {'K':>4} {'H_norm':>8} {'top1':>7} {'top3':>7} {'near%':>7} {'nd':>6}")
for r in rows:
    print(f"{r['sims']:>6} {r['K']:>4} {r['Hn']:>8.3f} {r['top1']:>7.3f} "
          f"{r['top3']:>7.3f} {r['near']:>7.1f} {r['nd']:>6}")

ref = rows[0]; best = min(rows, key=lambda r: r['Hn'])
print('\nVERDICT (vs step-1 best H_norm=0.812):')
print(f"  ref (sims=400,K=1,c=0.5) H_norm={ref['Hn']:.3f}")
print(f"  best (sims={best['sims']},K={best['K']}) H_norm={best['Hn']:.3f} top1={best['top1']:.3f}")
if best['Hn'] < 0.70:
    print("  => SUCCESS: Q-variance WAS the binding constraint -> averaging rollouts concentrates visits.")
elif ref['Hn'] - best['Hn'] > 0.05:
    print("  => PARTIAL: rollout averaging helps; stack with exploration_c / more sims.")
else:
    print("  => NO EFFECT: Q-variance is NOT the bottleneck -> pivot to value-target (T3), not MCTS.")
