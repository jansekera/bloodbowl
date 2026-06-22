"""Strop 4: zašpičatí se MCTS cíl, když zapneme naučenou value síť do searche?
Měření i trénink běží s vfBlend=0.0 (listy hodnotí heuristika, ne value síť).
Test: vfBlend 0.0 vs 0.5 vs 1.0 při fixním MCTS=400.
  H klesá s vfBlend → value rozlišuje akce → FIX: zapnout vfBlend v self-play.
  H ~stejná i s vfBlend=1.0 → value je taky plochá → hlubší problém (value-kvalita / Tým 1)."""
import sys, math, statistics as st
sys.path.insert(0, 'engine/build')
sys.path.insert(0, 'python')
import bb_engine

AWAY = ['orc', 'skaven', 'dwarf', 'wood-elf']
TV = 1200
WEIGHTS = 'weights_best.json'
N_GAMES = 4
MCTS = 400
VF_LEVELS = [0.0, 0.5, 1.0]

def entropy_nats(ps):
    return -sum(p * math.log(p) for p in ps if p > 0)

def measure(vfb):
    Hs, Hn, top1, top3, n_act, near = [], [], [], [], [], 0
    nd = 0
    for g in range(N_GAMES):
        away = AWAY[g % len(AWAY)]
        hr = bb_engine.get_developed_roster('human', TV)
        ar = bb_engine.get_developed_roster(away, TV)
        logged = bb_engine.simulate_game_logged(
            hr, ar, 'macro_mcts', 'macro_mcts',
            seed=3000 + g, weights_path=WEIGHTS, epsilon=0.1,
            mcts_iterations=MCTS, policy_blend=0.0, vf_blend=vfb,
        )
        for d in logged.get_policy_decisions():
            vf = sorted((float(v['visit_fraction']) for v in d['visits']), reverse=True)
            s = sum(vf)
            if s <= 0 or len(vf) < 2:
                continue
            vf = [x / s for x in vf]
            n = len(vf)
            Hs.append(entropy_nats(vf)); Hn.append(entropy_nats(vf)/math.log(n))
            top1.append(vf[0]); top3.append(sum(vf[:3])); n_act.append(n)
            if vf[0] < 1.25 * vf[1]:
                near += 1
            nd += 1
    print(f'  [vfBlend={vfb}] {nd} decisí, n_act mean={st.mean(n_act):.1f}', flush=True)
    return dict(vfb=vfb, nd=nd, H=st.mean(Hs), Hn=st.mean(Hn),
                top1=st.mean(top1), top3=st.mean(top3), near=100*near/max(nd,1))

rows = []
for vfb in VF_LEVELS:
    print(f'== měřím vfBlend={vfb} (MCTS={MCTS}, {N_GAMES} her) ==', flush=True)
    rows.append(measure(vfb))

print('\n========= STROP 4: VALUE V SEARCHI vs ŠPIČATOST CÍLE (MCTS=400) =========')
print(f"{'vfBlend':>8} {'H(nats)':>9} {'H_norm':>8} {'top1':>7} {'top3':>7} {'near-tie%':>10}")
for r in rows:
    print(f"{r['vfb']:>8} {r['H']:>9.3f} {r['Hn']:>8.3f} {r['top1']:>7.3f} {r['top3']:>7.3f} {r['near']:>10.1f}")
print('\nINTERPRETACE:')
print('  H výrazně klesá s vfBlend → value ROZLIŠUJE akce → FIX: zapnout vfBlend v self-play (sami)')
print('  H ~stejná i s vfBlend=1.0 → value je taky plochá → hlubší problém → Tým 1')
