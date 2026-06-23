"""Team1 v2 / T1 search-wiring decisive experiment.

Question: when the trained value AND policy heads are actually CONNECTED to
MacroMCTS (vf_blend>0 at the leaf eval, policy_blend>0 in the priors), does the
MCTS visit distribution -- which is the policy training target -- SHARPEN?

We do NOT retrain. We directly drive simulate_game_logged with the trained heads
loaded (weights_best.json carries both a 70->64->1 value head and a 93->64->1
policy head) and read the per-decision visit distributions. This isolates
"heads wired into search" from any training-loop noise.

PRIMARY metric: H_norm = entropy(visit_fraction) / ln(n_actions).
  1.0 = perfectly uniform; SUCCESS = drops below ~0.7 with blend>0.
Also: top1 visit mass, top3, near-tie%.

Sweep pairs (vf_blend, policy_blend):
  (0.0, 0.0)  baseline -- both heads OFF (current production config)
  (0.5, 0.0)  value only
  (0.0, 0.15) policy only
  (0.5, 0.15) BOTH on (the proposed fix)
  (1.0, 0.30) both fully on (upper-bound probe)

Run from /home/jan/claude/bloodbowl:
  PYTHONPATH=$(pwd)/engine/build:$(pwd)/python venv/bin/python3 /tmp/team1v2_wiring_measure.py
"""
import os, sys, math, statistics as st
sys.path.insert(0, 'engine/build')
sys.path.insert(0, 'python')
import bb_engine

AWAY    = ['orc', 'skaven', 'dwarf', 'wood-elf']
TV      = 1200
VALUE   = os.environ.get('T1_VALUE', 'weights_best.json')
POLICY  = os.environ.get('T1_POLICY', 'weights_best.json')
MCTS    = int(os.environ.get('T1_MCTS', 100))
N_GAMES = int(os.environ.get('T1_GAMES', 16))
EPS     = 0.1

# (vf_blend, policy_blend)
PAIRS = [
    (0.0, 0.0),
    (0.5, 0.0),
    (0.0, 0.15),
    (0.5, 0.15),
    (1.0, 0.30),
]


def entropy_nats(ps):
    return -sum(p * math.log(p) for p in ps if p > 0)


def measure(vfb, pb):
    Hs, Hn, top1, top3, n_act = [], [], [], [], []
    near = 0
    nd = 0
    for g in range(N_GAMES):
        away = AWAY[g % len(AWAY)]
        hr = bb_engine.get_developed_roster('human', TV)
        ar = bb_engine.get_developed_roster(away, TV)
        logged = bb_engine.simulate_game_logged(
            hr, ar, 'macro_mcts', 'macro_mcts',
            seed=7000 + g, weights_path=VALUE,
            policy_weights_path=POLICY,
            epsilon=EPS, mcts_iterations=MCTS,
            policy_blend=pb, vf_blend=vfb,
        )
        for d in logged.get_policy_decisions():
            vf = sorted((float(v['visit_fraction']) for v in d['visits']), reverse=True)
            s = sum(vf)
            if s <= 0 or len(vf) < 2:
                continue
            vf = [x / s for x in vf]
            n = len(vf)
            Hs.append(entropy_nats(vf))
            Hn.append(entropy_nats(vf) / math.log(n))
            top1.append(vf[0]); top3.append(sum(vf[:3])); n_act.append(n)
            if vf[0] < 1.25 * vf[1]:
                near += 1
            nd += 1
    print(f'  [vf={vfb} pol={pb}] {nd} decisions, n_act mean={st.mean(n_act):.1f}', flush=True)
    return dict(vfb=vfb, pb=pb, nd=nd,
                H=st.mean(Hs), Hn=st.mean(Hn),
                top1=st.mean(top1), top3=st.mean(top3),
                near=100 * near / max(nd, 1))


rows = []
for vfb, pb in PAIRS:
    print(f'== measuring vf_blend={vfb} policy_blend={pb} '
          f'(MCTS={MCTS}, {N_GAMES} games) ==', flush=True)
    rows.append(measure(vfb, pb))

print('\n===== T1 SEARCH-WIRING: do connected heads sharpen the MCTS target? =====')
print(f"{'vf_blend':>8} {'pol_blend':>9} {'H(nats)':>9} {'H_norm':>8} "
      f"{'top1':>7} {'top3':>7} {'near%':>7} {'nd':>6}")
base = rows[0]
for r in rows:
    print(f"{r['vfb']:>8} {r['pb']:>9} {r['H']:>9.3f} {r['Hn']:>8.3f} "
          f"{r['top1']:>7.3f} {r['top3']:>7.3f} {r['near']:>7.1f} {r['nd']:>6}")

print('\nVERDICT:')
both = rows[3]  # (0.5, 0.15)
dHn = base['Hn'] - both['Hn']
print(f"  baseline (0,0)   H_norm={base['Hn']:.3f} top1={base['top1']:.3f}")
print(f"  both on (.5,.15) H_norm={both['Hn']:.3f} top1={both['top1']:.3f}  "
      f"(deltaH_norm={dHn:+.3f})")
if both['Hn'] < 0.70:
    print("  => SUCCESS: target SHARPENS below 0.70 -> the fix is WIRING (turn blends on).")
elif dHn > 0.10:
    print("  => PARTIAL: meaningful sharpening but H_norm still >=0.70 -> wiring helps, "
          "may need mcts-quality too.")
else:
    print("  => NO SHARPEN: connecting the heads does NOT concentrate visits "
          "-> next suspect = open-loop macro Q-variance (hand to team1-mcts-quality), "
          "NOT per-player features.")
