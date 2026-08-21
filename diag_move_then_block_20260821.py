#!/usr/bin/env python3
"""P45b 21.08. — SMÍ U NÁS BLOKOVAT HRÁČ, KTERÝ SE UŽ POHNUL?

BB2016 ř. 674-676: "a player who stands up may not take a Block Action,
because YOU MAY NOT MOVE WHEN YOU TAKE A BLOCK ACTION."

Podezření z kódu (ověřuje se tímhle skriptem, ne tvrdí):
  * `Player::canAct()` NEZKOUMÁ `hasMoved` (player.h:56)
  * `getAvailableActions` nabízí BLOCK každému `canAct()` hráči se sousedem
  * uzávěrka aktivace (`action_resolver.cpp:193`) se spustí, teprve když začne
    jednat JINÝ hráč -> uvnitř téže aktivace nic nebrání MOVE -> BLOCK

Měří se na `corpus_baseline_20260819_data` (3 000 her).

⚠️ BLITZ je legální MOVE->BLOCK a v logu vypadá stejně. Proto se počítá
KONZERVATIVNĚ: na každé týmové kolo se JEDEN takový hráč odečte jako blitz.
Zbytek je nelegální, pokud engine blitz loguje jako BLOCK (ověřuje se dole
proti počtu kol, kde je takových hráčů 2+).
"""
import gzip, json, glob, collections

STANDING, PRONE = 0, 1
c = collections.Counter()
per_turn = collections.Counter()

for f in sorted(glob.glob('corpus_baseline_20260819_data/g*.json.gz')):
    d = json.load(gzip.open(f))
    for S in d['turn_logs']:
        c['kol'] += 1
        # kdo se v tomhle kole pohnul PŘED tím, než blokoval
        moved = set()
        movers_then_block = set()
        blockers = set()
        for e in S['events']:
            t, pid = e.get('type'), e.get('player_id')
            if pid is None or pid < 0:
                continue
            if t in ('MOVE', 'DODGE', 'GFI'):
                moved.add(pid)
            elif t == 'BLOCK':
                blockers.add(pid)
                if pid in moved:
                    movers_then_block.add(pid)
        c['kol s blokem'] += 1 if blockers else 0
        n = len(movers_then_block)
        per_turn[n] += 1
        c['hráčů MOVE->BLOCK'] += n
        if n >= 2:
            c['kol s 2+ (nelze vysvětlit blitzem)'] += 1
            c['nadbyteční nad blitz'] += n - 1

        # (A) ležící na začátku kola, který v tomtéž kole BLOKOVAL
        snap = S['home_players'] if S['active_team'] == 'home' else S['away_players']
        prone_ids = {p['id'] for p in snap if p['state'] == PRONE}
        c['ležících na začátku kola'] += len(prone_ids)
        stood_and_blocked = prone_ids & blockers
        c['LEŽÍCÍ, KTERÝ TÉHOŽ KOLA BLOKOVAL'] += len(stood_and_blocked)
        if stood_and_blocked:
            c['kol s takovým hráčem'] += 1

print("=== JMENOVATELE ===")
for k in ('kol', 'kol s blokem', 'ležících na začátku kola'):
    print(f"  {k}: {c[k]}")
print("\n=== (B) MOVE -> BLOCK v témž kole ===")
print(f"  hráčů celkem: {c['hráčů MOVE->BLOCK']}  ({c['hráčů MOVE->BLOCK']/c['kol']:.3f}/kolo)")
tot = sum(per_turn.values())
for n in sorted(per_turn):
    if n:
        print(f"    kol s {n} takovými hráči: {per_turn[n]} ({per_turn[n]/tot*100:.2f} % z {tot})")
print(f"  ⭐ kol, kde je jich 2+ (blitz vysvětlí nejvýš JEDNOHO): {c['kol s 2+ (nelze vysvětlit blitzem)']}"
      f" ({c['kol s 2+ (nelze vysvětlit blitzem)']/c['kol']*100:.2f} % z {c['kol']})")
print(f"  ⭐ nadbytečných hráčů nad blitz: {c['nadbyteční nad blitz']}")
print("\n=== (A) LEŽÍCÍ, KTERÝ SE POSTAVIL A TÉHOŽ KOLA BLOKOVAL ===")
d0 = c['ležících na začátku kola']
print(f"  hráčů: {c['LEŽÍCÍ, KTERÝ TÉHOŽ KOLA BLOKOVAL']} z {d0} ležících"
      f" ({c['LEŽÍCÍ, KTERÝ TÉHOŽ KOLA BLOKOVAL']/max(1,d0)*100:.2f} %)")
print(f"  kol s takovým hráčem: {c['kol s takovým hráčem']} z {c['kol']}"
      f" ({c['kol s takovým hráčem']/c['kol']*100:.2f} %)")
