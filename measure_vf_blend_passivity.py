#!/usr/bin/env python3
"""Team1 Agent B — measure why vf_blend increases passivity (draw collapse cause #4).

Pure-python: loads value head + replay buffer, evaluates V(s) on real states,
partitions by scoring-relevant features, and compares the VF's implied
"reward for advancing the ball" against the hand-coded leaf heuristic's
hard bonuses (macro_mcts.cpp simulate(), lines 358-398).

Run: PYTHONPATH=engine/build:python venv/bin/python measure_vf_blend_passivity.py
"""
import json, pickle, numpy as np

W = json.load(open('/tmp/weights_iter1_rejected_train.json'))
W1 = np.array(W['value_W1'], dtype=np.float64)   # (n_features, hidden) or (hidden, n_features)
b1 = np.array(W['value_b1'], dtype=np.float64)
W2 = np.array(W['value_W2'], dtype=np.float64)
b2 = np.array(W['value_b2'], dtype=np.float64)
NF = W['n_features']

# orient W1 so that features @ W1 works
if W1.shape[0] != NF:
    W1 = W1.T
def vf(F):  # F: (N, NF)
    h = np.maximum(F @ W1 + b1, 0.0)
    o = h @ W2 + b2
    return np.tanh(o).ravel()

buf = pickle.load(open('replay_buffer.pkl','rb'))
F = np.array([t.features for t in buf], dtype=np.float64)
if F.shape[1] != NF:
    F = F[:, :NF] if F.shape[1] > NF else np.pad(F, ((0,0),(0,NF-F.shape[1])))
mc = np.array([t.mc_return for t in buf])
V = vf(F)

# Feature indices (feature_extractor.cpp):
# [12] iHaveBall, [14] ball_on_ground, [59] carrier_can_score, [67] loose_ball_proximity
IHAVE, GROUND, CANSCORE, LOOSEPROX = 12, 14, 59, 67

have = F[:,IHAVE] > 0.5
ground = F[:,GROUND] > 0.5
canscore = F[:,CANSCORE] > 0.5

def stats(mask, name):
    if mask.sum()==0:
        print(f"  {name:30s} n=0"); return
    print(f"  {name:30s} n={mask.sum():5d}  V mean={V[mask].mean():+.3f} std={V[mask].std():.3f}   mc mean={mc[mask].mean():+.3f}")

print("=== VF value by ball situation (rejected/80% head) ===")
stats(have, "I have ball")
stats(~have & ground, "ball loose on ground")
stats(~have & ~ground, "opp has ball / no ball")
stats(have & canscore, "I have ball & CAN SCORE")
stats(have & ~canscore, "I have ball & cannot score")

print("\n=== Heuristic leaf bonus (macro_mcts.cpp simulate) for these same buckets ===")
print("  have ball:          +0.10 base +0.25*prox  (+0.4 safe-score, +0.8 last-turn)")
print("  loose on ground:    -0.10  (+0.04..0.08 if near)")
print("  opp has ball:       -0.10 -0.25*prox  (-0.4 if opp can score)")

# Key contrast: VF *delta* between 'have ball & can score' and 'have ball & cannot'
if (have&canscore).sum() and (have&~canscore).sum():
    d_vf = V[have&canscore].mean() - V[have&~canscore].mean()
    print(f"\nVF delta (canscore - cannot, given have ball): {d_vf:+.3f}")
    print("Heuristic delta for same: +0.4 (safe-score bonus) up to +1.2 last turn")

# Does VF reward ADVANCING (loose->held)?  proxy: V(have) - V(loose)
if have.sum() and (~have&ground).sum():
    d = V[have].mean() - V[~have&ground].mean()
    print(f"\nVF delta (have ball - loose ball): {d:+.3f}")
    print("Heuristic delta (have - loose): ~+0.20 base (0.10 - (-0.10)) plus proximity/score")

# Cross-correlation: VF vs heuristic-style ball bonus.  Build a crude heuristic-ball score
prox = np.zeros(len(buf))
# carrier dist features if present: use carrier_can_score + iHaveBall as coarse proxy
heur_ball = 0.10*have - 0.10*(~have&ground) + 0.40*(have&canscore)
print(f"\ncorr(V, crude-heuristic-ball-bonus) = {np.corrcoef(V, heur_ball)[0,1]:+.3f}")
print(f"corr(V, mc_return)                  = {np.corrcoef(V, mc)[0,1]:+.3f}")
