"""Team1 v2 / T2 mcts-quality — STEP 1 (no rebuild).

Question: with both heads connected (vf=0.5, pol=0.15), does a larger sim budget
or a lower exploration constant concentrate the MCTS visit distribution? If yes,
the diffuseness is undersampling/over-exploration (cheap config fix). If H_norm
stays ~0.93 regardless, the binding constraint is the open-loop macro Q-variance
(macro_mcts.cpp:509-540) -> STEP 2 (C++ change).

Reuses the T1 methodology (entropy of visit_fraction / ln(n_actions)).
Run: PYTHONPATH=$(pwd)/engine/build:$(pwd)/python venv/bin/python3 -u /tmp/team1v2_mcts_sweep.py
"""
import os, sys, math, statistics as st
sys.path.insert(0, 'engine/build'); sys.path.insert(0, 'python')
import bb_engine

AWAY    = ['orc', 'skaven', 'dwarf', 'wood-elf']
TV      = 1200
VALUE   = 'weights_best.json'   # carries value + 93-dim policy head
POLICY  = 'weights_best.json'
N_GAMES = int(os.environ.get('T2_GAMES', 3))
VFB, PB = 0.5, 0.15             # heads connected (the intended regime)
EPS     = 0.1

# (mcts_iterations, exploration_c)
CONFIGS = [
    (100, 2.0),   # reference (≈ T1 both-on)
    (400, 2.0),
    (800, 2.0),
    (400, 1.0),
    (400, 0.5),
    (800, 0.5),
]


def entropy_nats(ps):
    return -sum(p * math.log(p) for p in ps if p > 0)


def measure(sims, c):
    Hn, top1, top3, n_act = [], [], [], []
    near = nd = 0
    for g in range(N_GAMES):
        away = AWAY[g % len(AWAY)]
        hr = bb_engine.get_developed_roster('human', TV)
        ar = bb_engine.get_developed_roster(away, TV)
        logged = bb_engine.simulate_game_logged(
            hr, ar, 'macro_mcts', 'macro_mcts',
            seed=9000 + g, weights_path=VALUE, policy_weights_path=POLICY,
            epsilon=EPS, mcts_iterations=sims,
            policy_blend=PB, vf_blend=VFB, exploration_c=c,
        )
        for d in logged.get_policy_decisions():
            vf = sorted((float(v['visit_fraction']) for v in d['visits']), reverse=True)
            s = sum(vf)
            if s <= 0 or len(vf) < 2:
                continue
            vf = [x / s for x in vf]; n = len(vf)
            Hn.append(entropy_nats(vf) / math.log(n))
            top1.append(vf[0]); top3.append(sum(vf[:3])); n_act.append(n)
            if vf[0] < 1.25 * vf[1]:
                near += 1
            nd += 1
    print(f'  [sims={sims} c={c}] {nd} decisions, n_act mean={st.mean(n_act):.1f}', flush=True)
    return dict(sims=sims, c=c, nd=nd, Hn=st.mean(Hn),
                top1=st.mean(top1), top3=st.mean(top3), near=100*near/max(nd,1))


rows = []
for sims, c in CONFIGS:
    print(f'== measuring sims={sims} exploration_c={c} (vf={VFB} pol={PB}, {N_GAMES} games) ==', flush=True)
    rows.append(measure(sims, c))

print('\n===== T2 STEP1: do more sims / lower exploration_c sharpen the target? =====')
print(f"{'sims':>6} {'expl_c':>7} {'H_norm':>8} {'top1':>7} {'top3':>7} {'near%':>7} {'nd':>6}")
for r in rows:
    print(f"{r['sims']:>6} {r['c']:>7} {r['Hn']:>8.3f} {r['top1']:>7.3f} "
          f"{r['top3']:>7.3f} {r['near']:>7.1f} {r['nd']:>6}")

base = rows[0]; best = min(rows, key=lambda r: r['Hn'])
print('\nVERDICT:')
print(f"  reference (sims=100,c=2.0) H_norm={base['Hn']:.3f}")
print(f"  best config (sims={best['sims']},c={best['c']}) H_norm={best['Hn']:.3f}")
if best['Hn'] < 0.70:
    print("  => sims/exploration_c IS a lever: tune config (no C++ change needed).")
elif base['Hn'] - best['Hn'] > 0.10:
    print("  => PARTIAL: helps but not enough -> proceed to STEP 2 (Q-variance C++ fix).")
else:
    print("  => NO SHARPEN from sims/exploration_c -> confirms STEP 2: open-loop macro "
          "Q-variance (macro_mcts.cpp:509-540) is the binding constraint.")
