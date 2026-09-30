# P93 diag (30.09.2026): hraje hrac po neuspesnem Bone-head v dalsich kolech znovu?
import sys, collections
sys.path.insert(0, '.')
import bb_engine as b
BH = 21   # SkillName::BoneHead (overeno: enums.h)
N = int(sys.argv[1]) if len(sys.argv) > 1 else 40
fails = rolls_after_fail = passes_after_fail = games = 0
for seed in range(N):
    r = b.simulate_game_logged(b.get_human_roster(), b.get_orc_roster(), 'greedy', 'greedy', seed=1000 + seed)
    games += 1
    stuck = set()
    half = None
    for t in r.get_turn_logs():
        if t['half'] != half:                  # polocas = novy drive => stav konci
            half = t['half']
            stuck.clear()
        for e in t['events']:
            if e['type'] == 'TOUCHDOWN':        # konec drivu => stav konci
                stuck.clear()
            if e['type'] != 'SKILL' or e['roll'] != BH:
                continue
            pid = e['player_id']
            if pid in stuck:
                rolls_after_fail += 1
                if e['success']:
                    passes_after_fail += 1
                    stuck.discard(pid)
            if not e['success']:
                fails += 1
                stuck.add(pid)
print(f"hry {games} | Bone-head neuspechu {fails} | znovu hozeno po neuspechu {rolls_after_fail} | z toho proslo {passes_after_fail}")
