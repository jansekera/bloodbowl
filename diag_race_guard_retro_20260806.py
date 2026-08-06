#!/usr/bin/env python3
"""Retro-analýza rasové pojistky (06.08.) na historických gate log souborech.

Parsuje řádky '  Game N [H/A]: cand X-Y' z anti-regression sekce logu a
rekonstruuje per-race tabulku + verdikt pojistky přesně formulí produkce:
home=_RACES[i%5], away=_RACES[(i+1)%5], cand away při i%2==1 (i = N-1).
PLATÍ JEN pro běhy bez přeskočených her (total 600/600) — jinak se
atribuce z pořadí rozjede (proto produkce echuje race_idx z workeru).
"""
import re
import sys
sys.path.insert(0, '.')
from run_iteration import _RACES, _race_guard_verdict

GAME_RE = re.compile(r'^  Game (\d+) \[([HA])\]: cand (\d+)-(\d+)$')


def analyze(path: str, z_thr: float = 1.28) -> None:
    # Anti-regression sekce je poslední blok Game řádků následovaný
    # 'New vs Frozen' — vezmi posledních 600 Game řádků před ním.
    games = []
    with open(path) as f:
        for line in f:
            m = GAME_RE.match(line.rstrip())
            if m:
                games.append((int(m.group(1)), m.group(2),
                              int(m.group(3)), int(m.group(4))))
            elif line.startswith('New vs Frozen:'):
                break
    # Selection H2H sekce má jiný počet (150) a nemá číslování do 600;
    # anti-regression Game čísluje 1..600 — vyfiltruj poslední souvislou
    # řadu začínající 1 a končící maximem.
    last_start = max(i for i, g in enumerate(games) if g[0] == 1)
    games = games[last_start:]
    n = len(games)
    print(f'{path}: {n} her v anti-regression bloku')
    if n != 600:
        print('  ⚠ POZOR: != 600 → atribuce z pořadí NEPLATÍ, jen orientačně!')

    per_cand = {r: [0, 0, 0] for r in _RACES}
    per_frozen = {r: [0, 0, 0] for r in _RACES}
    for num, side, cs, fs in games:
        i = num - 1
        ca = 1 if side == 'A' else 0
        exp = 'A' if i % 2 == 1 else 'H'
        assert side == exp, f'orientace nesedí u hry {num} ({side}!={exp})'
        outcome = 0 if cs > fs else (1 if cs == fs else 2)
        per_cand[_RACES[(i + ca) % 5]][outcome] += 1
        per_frozen[_RACES[(i + 1 - ca) % 5]][outcome] += 1

    print('  rasa      | kandidát hraje         | frozen hraje (W/D/L kandidáta)')
    for r in _RACES:
        cw, cd, cl = per_cand[r]
        fw, fd, fl = per_frozen[r]
        cwr = f'{cw / (cw + cl):.1%}' if cw + cl else 'n/a'
        fwr = f'{fl / (fw + fl):.1%}' if fw + fl else 'n/a'
        print(f'  {r:>9} | {cw:>2}W {cd:>2}D {cl:>2}L wr={cwr:>6} | '
              f'{fw:>2}W {fd:>2}D {fl:>2}L frozen-wr={fwr:>6}')
    for race in _RACES:
        v = _race_guard_verdict(per_cand, per_frozen, race, z_thr)
        if v:
            flag = 'VETO' if v['veto'] else 'ok'
            print(f"  pojistka[{race:>9}]: cand {v['cand_wr']:.1%} vs frozen "
                  f"{v['frozen_wr']:.1%}  Δ {v['delta']:+.1%}  "
                  f"z={v['z']:+.2f}  → {flag}")
    print()


if __name__ == '__main__':
    for p in sys.argv[1:]:
        analyze(p)
