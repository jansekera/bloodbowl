#!/usr/bin/env bash
# ============================================================================
# NOC 13.→14.08.2026, hlavní běh — VELKÝ KORPUS (3000 her, produkční config)
#
# ⚑ PROČ TOHLE A NE DALŠÍ A/B
#   Zvažoval jsem A/B opravy hand-offu a zamítl ho: Longbeard nese jen 9 %
#   kol (změřeno 13.08. na korpusu PO opravě rozestavení 41c3570 -- starých
#   „44-49 %" bylo předopravní číslo). I kdyby oprava ta kola odstranila
#   úplně, zisk je ~0,09 x 1,9 = 0,17 pole/kolo, což je na chess hluboko pod
#   šumovým dnem ±5,3 pp. Předem vyhozený běh.
#
# ⚑ CO TENHLE BĚH ODEMYKÁ
#   Všechno, co dnes vzniklo, má příliš malé n:
#     K36 LOCKED      koše po 7-16 vzorcích, značené „⚠ málo vzorků"
#     fázový model    zatím neměřený vůbec (uživatelův plán trasy, 13.08.)
#     K34/K35 po fázích   fáze 3 je zlomek kol
#     rozklad drivů   kategorie C/D1/D2 se na 120 hrách dělí na jednotky
#   3000 her = 25x dnešní vzorek. Ráno se fázový model měří rovnou.
#
# ⚑ BRÁNA JE VYPNUTÁ (produkční stav). A/B brány doběhlo 13.08. v 14:27 a
#   NEPROŠLO (dw-we -0,0297 = -2,0 SE, dw-sk +0,0083 = 0,6 SE), takže korpus
#   má popisovat to, co se hraje, ne odmítnutou variantu.
#
# Doba: ~4 s/hra při 12 workerech ⇒ ~3,3 h.
# Pojistky: marker + lockfile + kontrola běžícího interpretu (ne pouhé zmínky
#           jména souboru -- ta past nás dnes zastavila dvakrát).
# ============================================================================
set -u
ROOT=/home/jan/claude/bloodbowl
OUT=$ROOT/night_big_20260813
LOG=$OUT/chain.log
DATA=$ROOT/diag_replay_mine_20260813_big_data
GAMES=${GAMES:-3000}
LOCK=$OUT/.lock
STAMP() { date -u '+%H:%M'; }

mkdir -p "$OUT"
[ -f "$OUT/BIG_DONE" ] && { echo "[$(STAMP)] hotovo, končím" >> "$LOG"; exit 0; }
if ! mkdir "$LOCK" 2>/dev/null; then echo "[$(STAMP)] ABORT: drží lock" >> "$LOG"; exit 1; fi
trap 'rmdir "$LOCK" 2>/dev/null' EXIT
if pgrep -f "python3 .*diag_replay_mine_20260813" > /dev/null; then
    echo "[$(STAMP)] ABORT: jiný sběr běží" >> "$LOG"; exit 1; fi
if pgrep -f "^$ROOT/diag_f1_cage_advance " > /dev/null; then
    echo "[$(STAMP)] ABORT: běží měření harnessem" >> "$LOG"; exit 1; fi

cd "$ROOT" || exit 1

if [ -f "$DATA/COLLECT_DONE" ]; then
    echo "[$(STAMP)] korpus už existuje, přeskakuji sběr" >> "$LOG"
else
    echo "[$(STAMP)] START velký korpus, $GAMES her, brána OFF," \
         "HEAD=$(git rev-parse --short HEAD)" >> "$LOG"
    if CAGE_GATE=0 DATA_ROOT="$DATA" SEED_BASE=20260900 \
            nice -n 19 python3 diag_replay_mine_20260813_gate.py collect "$GAMES" \
            > "$OUT/collect.log" 2>&1; then
        echo "[$(STAMP)] korpus HOTOV" >> "$LOG"
    else
        echo "[$(STAMP)] FAIL sběr — viz $OUT/collect.log" >> "$LOG"
        touch "$OUT/BIG_PARTIAL"; exit 1
    fi
fi

[ -f "$DATA/COLLECT_DONE" ] || {
    echo "[$(STAMP)] ABORT: korpus nemá COLLECT_DONE" >> "$LOG"
    touch "$OUT/BIG_PARTIAL"; exit 1; }

echo "[$(STAMP)] START kontroly K29–K36" >> "$LOG"
nice -n 19 python3 diag_rules_checks_20260812.py "$DATA/*.json.gz" \
    > "$OUT/checks.txt" 2>&1
echo "[$(STAMP)] START rozklad drivů" >> "$LOG"
nice -n 19 python3 diag_drive_failure_20260811.py "$DATA" \
    > "$OUT/drives.txt" 2>&1

if grep -q "K33" "$OUT/checks.txt" && \
   grep -qE "PŘIJÍMACÍ DRIVY" "$OUT/drives.txt"; then
    echo "[$(STAMP)] DONE — $OUT/checks.txt, $OUT/drives.txt" >> "$LOG"
    touch "$OUT/BIG_DONE"
else
    echo "[$(STAMP)] PARTIAL: analýzy nedoběhly celé" >> "$LOG"
    touch "$OUT/BIG_PARTIAL"
fi
