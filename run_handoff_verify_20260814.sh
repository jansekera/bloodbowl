#!/usr/bin/env bash
# ============================================================================
# OVĚŘENÍ OPRAVY HAND-OFFU NA KONTROLÁCH — 14.08.2026
#
# ⚑ PROČ NE A/B
#   Fronta (P5) to říká výslovně: oprava rozestavení 41c3570 srazila podíl kol,
#   kdy nese Longbeard, na 1-4 % (změřeno na korpusu 3000 her). Očekávaný zisk
#   je proto pod šumovým dnem +-5,3 pp a párový běh by koupil drahou nulu.
#   Ptáme se tedy „CO oprava dělá", ne „hraje se s ní líp".
#
# ⚑ PÁROVOST
#   Tentýž kolektor, tentýž SEED_BASE=20260900, tíž soupeři, totéž pořadí,
#   brána OFF — jediný rozdíl proti diag_replay_mine_20260813_big_data je
#   commit 38dcad6. Harness je deterministický (změřeno 13.08.), takže rozdíl
#   v kontrolách je dopad opravy, ne šum mezi vzorky. NEMĚNIT konstanty.
#
# ⚑ CO SE ČTE
#   1. Nabízí se vůbec předání? (dosud NIKDY - žádná dvojice z našeho rosteru
#      neprošla prahem 0,5, když se hand-off oceňoval jako hod.)
#   2. Změnilo to, KDO nese? (Runner byl 88-91 %, Longbeard 1-4 %.)
#   3. Nezhoršilo to něco jiného — K29 rohy, K31 idle těla, K33 bloky,
#      K34 REACH0, K36 zámky, a rozklad drivů A/B/C/D1/D2.
#   ⚠️ Predikce PŘEDEM, ať se to nedá číst zpětně: čekám vyšší výskyt
#      HAND_OFF v událostech a jinak čísla v mezích šumu. Kdyby se výrazně
#      hnul podíl nosičů, je to podezřelé - Longbeard nese málo kol.
#
# Doba: ~3,1 h na 12 workerech (3000 her, měřeno 13.08.).
# ============================================================================
set -u
ROOT=/home/jan/claude/bloodbowl
OUT=$ROOT/handoff_verify2_20260814
LOG=$OUT/chain.log
DATA=$ROOT/diag_replay_mine_20260814_handoff2_data
BASE=$ROOT/night_big_20260813
GAMES=${GAMES:-3000}
LOCK=$OUT/.lock
STAMP() { date -u '+%H:%M'; }

mkdir -p "$OUT"
[ -f "$OUT/VERIFY_DONE" ] && { echo "[$(STAMP)] hotovo, končím" >> "$LOG"; exit 0; }
if ! mkdir "$LOCK" 2>/dev/null; then echo "[$(STAMP)] ABORT: drží lock" >> "$LOG"; exit 1; fi
trap 'rmdir "$LOCK" 2>/dev/null' EXIT
if pgrep -f "python3 .*diag_replay_mine_2026081" > /dev/null; then
    echo "[$(STAMP)] ABORT: jiný sběr běží" >> "$LOG"; exit 1; fi

cd "$ROOT" || exit 1

if [ -f "$DATA/COLLECT_DONE" ]; then
    echo "[$(STAMP)] korpus už existuje, přeskakuji sběr" >> "$LOG"
else
    echo "[$(STAMP)] START korpus $GAMES her, brána OFF, hand-off opravený," \
         "HEAD=$(git rev-parse --short HEAD)" >> "$LOG"
    if CAGE_GATE=0 DATA_ROOT="$DATA" SEED_BASE=20260900 \
            nice -n 19 python3 diag_replay_mine_20260813_gate.py collect "$GAMES" \
            > "$OUT/collect.log" 2>&1; then
        echo "[$(STAMP)] korpus HOTOV" >> "$LOG"
    else
        echo "[$(STAMP)] FAIL sběr — viz $OUT/collect.log" >> "$LOG"
        touch "$OUT/VERIFY_PARTIAL"; exit 1
    fi
fi

[ -f "$DATA/COLLECT_DONE" ] || {
    echo "[$(STAMP)] ABORT: korpus nemá COLLECT_DONE" >> "$LOG"
    touch "$OUT/VERIFY_PARTIAL"; exit 1; }

echo "[$(STAMP)] START kontroly K29–K36" >> "$LOG"
nice -n 19 python3 diag_rules_checks_20260812.py "$DATA/*.json.gz" \
    > "$OUT/checks.txt" 2>&1
echo "[$(STAMP)] START rozklad drivů" >> "$LOG"
nice -n 19 python3 diag_drive_failure_20260811.py "$DATA" \
    > "$OUT/drives.txt" 2>&1

# Vlastní otázka běhu: nabízí se předání, a kdo nese?
echo "[$(STAMP)] START počítadlo předání" >> "$LOG"
nice -n 19 python3 - "$DATA" > "$OUT/handoff.txt" 2>&1 <<'PY'
import glob, gzip, json, os, sys
from collections import Counter
data = sys.argv[1]
ev, games_with, n = Counter(), 0, 0
for p in sorted(glob.glob(os.path.join(data, "*.json.gz"))):
    g = json.load(gzip.open(p, "rt"))
    if "dwarf" not in (g["home_race"], g["away_race"]):
        continue
    n += 1
    side = "home" if g["home_race"] == "dwarf" else "away"
    ids = {p_["id"] for p_ in g["turn_logs"][0][side + "_players"]} if g["turn_logs"] else set()
    seen = 0
    for tl in g["turn_logs"]:
        if tl["active_team"] != side:
            continue
        for e in tl["events"]:
            t = str(e.get("type", "")).upper()
            if t in ("HAND_OFF", "HANDOFF", "PASS", "CATCH"):
                ev[t + ("/ok" if e.get("success") else "/fail")] += 1
                if t.startswith("HAND"):
                    seen += 1
    if seen:
        games_with += 1
print("her: %d | her s aspoň jedním naším hand-offem: %d (%.1f %%)"
      % (n, games_with, 100.0 * games_with / max(n, 1)))
for k in sorted(ev):
    print("   %-14s %6d   (%.3f na hru)" % (k, ev[k], ev[k] / max(n, 1)))
PY

if grep -q "K33" "$OUT/checks.txt" && grep -q "PŘIJÍMACÍ DRIVY" "$OUT/drives.txt"; then
    echo "[$(STAMP)] DONE — $OUT/{checks,drives,handoff}.txt" >> "$LOG"
    echo "[$(STAMP)] baseline k porovnání: $BASE/{checks,drives}.txt" >> "$LOG"
    touch "$OUT/VERIFY_DONE"
else
    echo "[$(STAMP)] PARTIAL: analýzy nedoběhly celé" >> "$LOG"
    touch "$OUT/VERIFY_PARTIAL"
fi
