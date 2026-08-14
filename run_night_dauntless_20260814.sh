#!/usr/bin/env bash
# ============================================================================
# NOC 14.→15.08.2026 — A/B: DAUNTLESS V NABÍDCE BLOKU (P13)
#
# ⚑ CO SE MĚŘÍ
#   `getBlockDiceCount` váží Horns, ale Dauntless nikdy. Troll Slayer ST3 vedle
#   Black Orka ST4 se ocení jako do kopce, počet kostek vyjde záporný a nabídka
#   se zahodí — pro blok, který se při provedení srovná na rovnocenný v 83 %
#   (d6+3 > 4 je 2+). `block_handler.cpp:386` přitom Dauntless ctí správně.
#   ⇒ Slayerovi se blok na Black Orka NIKDY nenabídne.
#
# ⚑ PROČ PRÁVĚ TOHLE A PRÁVĚ TEĎ
#   Ork je jediný soupeř se čtyřmi ST4 těly, a je to zdaleka náš nejhorší
#   matchup: 86 našich TD na 750 zápasů proti 451 na skavena (změřeno 14.08.).
#   Dauntless je nejsilnější přesně proti ST4 (2+), proti ST5 3+, proti
#   Treemanovi ST6 už jen 4+ — proto si toho na wood-elfovi nikdo nevšiml.
#
# ⚑ 3000 PÁRŮ, NE 1500
#   Fable 14.08. spočítal z reálných řádků brány: SD páru 0,54–0,56 ⇒ na 1 pp
#   chess je potřeba ~12k párů, na 2 pp ~3k. Na 1500 párech bychom viděli jen
#   efekt ≥ 3 pp. Uživatel: „kontrolní run má trvat dvakrát tak dlouho, ať
#   vyleze ze šumu — OK." ⇒ 4 shardy × 750 párů = 3000 na matchup.
#
# ⚑ RAMENO PLATÍ PRO OBĚ STRANY
#   Není to doktrína, kterou zkoušíme na trpaslících — je to filtr, který
#   neviděl dovednost, kterou resolver už ctí. Na jedné straně by to
#   srovnávalo dva různé enginy, ne dvě ramena.
#
# ⚑ DRUHÁ ČÁST: KORPUS PRO ROZKLAD DRIVŮ
#   Chess samotné efekt pod ~2 pp neuvidí. Fable proto radí měřit i v MĚNĚ
#   DRIVŮ: podíl kategorií A/C/D1/D2 a příčiny ztrát, s převodem
#   Δchess ≈ 0,42 × Δ(drivy/hru). Sbírá se 3000 her se zapnutým ramenem;
#   baseline je `night_big_20260813/` (HEAD e4b99ee, tentýž SEED_BASE).
#   ⚠️ Korpus je popisný, NENÍ to druhé měření výsledku — verdikt dává A/B.
#
# Doba: A/B ~14 h (3000 párů × 3 matchupy), korpus ~3,5 h. Pořadí: A/B první.
# ============================================================================
set -u
ROOT=/home/jan/claude/bloodbowl
OUT=$ROOT/dauntless_ab_20260814
LOG=$OUT/chain.log
BIN=$ROOT/diag_f1_cage_advance
PAIRS=${PAIRS:-750}
SHARDS=${SHARDS:-4}
CORPUS_OUT=$ROOT/dauntless_corpus_20260814
CORPUS_DATA=$ROOT/diag_replay_mine_20260814_dauntless_data
LOCK=$OUT/.lock
STAMP() { date -u '+%H:%M'; }

mkdir -p "$OUT"
[ -f "$OUT/NIGHT_DONE" ] && { echo "[$(STAMP)] hotovo, končím" >> "$LOG"; exit 0; }
if ! mkdir "$LOCK" 2>/dev/null; then echo "[$(STAMP)] ABORT: drží lock" >> "$LOG"; exit 1; fi
trap 'rmdir "$LOCK" 2>/dev/null' EXIT
[ -x "$BIN" ] || { echo "[$(STAMP)] ABORT: chybí $BIN — přeložit harness" >> "$LOG"; exit 1; }
if pgrep -f "python3 .*diag_replay_mine_2026081" > /dev/null; then
    echo "[$(STAMP)] ABORT: běží jiný sběr" >> "$LOG"; exit 1; fi

cd "$ROOT" || exit 1

# ---------- ČÁST 1: A/B (mode 4) ----------
if [ -f "$OUT/AB_DONE" ]; then
    echo "[$(STAMP)] A/B už hotové, přeskakuji" >> "$LOG"
else
    echo "[$(STAMP)] START A/B mode 4 (Dauntless v nabídce), ${SHARDS}×${PAIRS}" \
         "párů na matchup, HEAD=$(git rev-parse --short HEAD)" >> "$LOG"
    run_one() {  # $1=matchup idx  $2=jméno  $3=shard
        local off=$(( $3 * PAIRS ))
        local d="$OUT/$2_s$3"
        mkdir -p "$d"; rm -f "$d/diag_dauntless_rows.jsonl" "$d/OK" "$d/FAIL"
        if ( cd "$d" && nice -n 19 "$BIN" "$ROOT" "$PAIRS" "$1" 4 "$off" > run.log 2>&1 ); then
            touch "$d/OK";  echo "[$(STAMP)] done $2 shard $3" >> "$LOG"
        else
            touch "$d/FAIL"; echo "[$(STAMP)] FAIL $2 shard $3 — viz $d/run.log" >> "$LOG"
        fi
    }
    # dw-orc je TA otázka (Dauntless vyskočí jen při defST > attST, a ST4 má
    # jedině ork); dw-we drží jednoho Treemana ST6; orc-sk je A/A null test,
    # protože Dauntless nemá ani jedna z těch dvou ras.
    # Tři matchupy, každý s vlastní rolí:
    #   dw-orc  TA OTÁZKA -- 4x Black Orc ST4, Dauntless vyskočí na 2+ (83 %)
    #   dw-sk   PRAVÝ NULL -- skaven má max ST3, Dauntless NEMŮŽE vyskočit ani
    #           jednou, takže delta MUSÍ být nula. Čistší kontrola než dw-we,
    #           kde by ten jeden Treeman ST6 občas zafungoval.
    #   orc-sk  A/A NULL -- Dauntless nemá ani jedna z těch dvou ras.
    # dw-hu a dw-we se NEBĚŽÍ: expozice je 0,20 resp. 0,15 orkovy, a orc sám je
    # na hraně rozlišení ⇒ vrátily by po 9 h navíc zaručené „nerozhodnuto".
    for spec in "4 dw-orc" "0 dw-sk" "3 orc-sk"; do
        set -- $spec
        for s in $(seq 0 $((SHARDS - 1))); do run_one "$1" "$2" "$s" & done
    done
    wait
    EXPECT=$(( 3 * SHARDS ))
    OKS=$(find "$OUT" -name OK | wc -l); FAILED=$(find "$OUT" -name FAIL | wc -l)
    if [ "$FAILED" -eq 0 ] && [ "$OKS" -eq "$EXPECT" ]; then
        echo "[$(STAMP)] A/B DONE ($OKS/$EXPECT)" >> "$LOG"; touch "$OUT/AB_DONE"
    else
        echo "[$(STAMP)] A/B PARTIAL: ok=$OKS fail=$FAILED" >> "$LOG"
        touch "$OUT/AB_PARTIAL"; exit 1
    fi
fi

# ---------- ČÁST 2: korpus se zapnutým ramenem, pro rozklad drivů ----------
mkdir -p "$CORPUS_OUT"
if [ -f "$CORPUS_DATA/COLLECT_DONE" ]; then
    echo "[$(STAMP)] korpus už existuje, přeskakuji sběr" >> "$LOG"
else
    echo "[$(STAMP)] START korpus 3000 her, Dauntless ON, brána OFF" >> "$LOG"
    if CAGE_GATE=0 DAUNTLESS=1 DATA_ROOT="$CORPUS_DATA" SEED_BASE=20260900 \
            nice -n 19 python3 diag_replay_mine_20260813_gate.py collect 3000 \
            > "$CORPUS_OUT/collect.log" 2>&1; then
        echo "[$(STAMP)] korpus HOTOV" >> "$LOG"
    else
        echo "[$(STAMP)] FAIL korpus — viz $CORPUS_OUT/collect.log" >> "$LOG"
        touch "$OUT/NIGHT_PARTIAL"; exit 1
    fi
fi

echo "[$(STAMP)] START rozklad drivů" >> "$LOG"
nice -n 19 python3 diag_drive_failure_20260811.py "$CORPUS_DATA" \
    > "$CORPUS_OUT/drives.txt" 2>&1
echo "[$(STAMP)] START kontroly" >> "$LOG"
nice -n 19 python3 diag_rules_checks_20260812.py "$CORPUS_DATA/*.json.gz" \
    > "$CORPUS_OUT/checks.txt" 2>&1

if grep -q "PŘIJÍMACÍ DRIVY" "$CORPUS_OUT/drives.txt" && \
   grep -q "K33" "$CORPUS_OUT/checks.txt"; then
    echo "[$(STAMP)] DONE — A/B: $OUT/*_s*/diag_dauntless_rows.jsonl" >> "$LOG"
    echo "[$(STAMP)]        drivy: $CORPUS_OUT/drives.txt" >> "$LOG"
    echo "[$(STAMP)]        baseline: $ROOT/night_big_20260813/{checks,drives}.txt" >> "$LOG"
    touch "$OUT/NIGHT_DONE"
else
    echo "[$(STAMP)] PARTIAL: analýzy nedoběhly celé" >> "$LOG"
    touch "$OUT/NIGHT_PARTIAL"
fi
