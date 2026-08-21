#!/usr/bin/env bash
# ============================================================================
# KŘÍŽOVÝ KORPUS NA OPRAVENÉM ENGINU                       (21.08.2026, pátek)
#
# ⚑ PROČ
#   Dnešní oprava vstávání (P45) mění akční ekonomiku OBOU týmů: ležící se
#   dosud postavil v 0,4 % případů (1 067 z 280 719), nově v ~70 %. Na 21 hrách
#   to znamená 3,04 -> 1,42 ležících těl na kolo, +20 % vlastních stojících,
#   +65 % bloků, +59 % sražení.
#   ⇒ `corpus_baseline_20260819_data` je ZASTARALÝ a každé číslo z něj je
#     z jiného enginu (uživatel 21.08.: „porovnávat jen s odstupem").
#
# ⚑ CO JE NOVÉHO PROTI `run_corpus_baseline.sh`
#   Dosud byl domácí VŽDY trpaslík, takže korpus uměl jen pět z patnácti
#   dvojic. `HOME_RACE` (přidáno 21.08.) to otevírá ⇒ sbíráme VŠECH 15 křížů.
#   Každý sběr sám střídá obě orientace (idx % 2), takže se nemíchá bias.
#
# ⚑ ROZPOČET (měřeno, ne odhadnuto)
#   5,57 s/hru wall-clock při WORKERS=10 (dnešní smoke, 21 her za 117 s).
#   ⚠️ Před opravou to bylo 3,79 s/hru (3 000 her za 11 365 s) -- oprava
#     zdražila sběr o 47 %, protože je víc těl na nohou a tedy víc akcí.
#   ⇒ 15 dvojic × GAMES her ≈ 15 × GAMES × 5,6 s.
#     GAMES=1200 -> ~28 h · GAMES=1500 -> ~35 h · GAMES=2000 -> ~47 h
#
# ⚑ POUŽITÍ
#   GAMES=1200 ./run_crosses_20260821.sh
#   ⛔ NEEDITOVAT ZA BĚHU (bash čte skript po offsetech).
# ============================================================================
set -u
ROOT="$(cd "$(dirname "$0")" && pwd)"
. "$ROOT/run_night_lib.sh"

GAMES=${GAMES:-1200}
OUT="$ROOT/${OUT:-crosses_20260821}"
DATA_BASE="$ROOT/${DATA_BASE:-crosses_20260821_data}"
RACES=${RACES:-"dwarf wood-elf orc human skaven"}

night_init "$OUT" "crosses"
night_stamp_head "$OUT"

# --- preflight ------------------------------------------------------------
if [ -n "$(git -C "$ROOT" status --porcelain -- engine/ 2>/dev/null)" ]; then
    night_log "⛔ engine/ má nezacommitované změny — korpus by nesl otisk,"
    night_log "   který neodpovídá kódu (vada P22). Nespouštím."
    exit 2
fi
for p in /proc/[0-9]*; do
    pid=${p#/proc/}; [ "$pid" = "$$" ] && continue
    tr '\0' ' ' < "$p/cmdline" 2>/dev/null | grep -q 'diag_replay_mine_2026' && {
        night_log "⛔ už běží jiný sběr (pid $pid) — nespouštím"; exit 1; }
done
FREE_MB=$(df -Pm "$ROOT" | awk 'NR==2{print $4}')
# 27 MB / 3 000 her na starém enginu; nový engine loguje navíc STAND_UP, takže
# se počítá s 1,5násobkem a ještě 2x rezerva.
NEED_MB=$(( 15 * GAMES * 27 / 3000 * 3 + 500 ))
if [ "$FREE_MB" -lt "$NEED_MB" ]; then
    night_log "⛔ málo místa: volno ${FREE_MB} MB, potřeba ~${NEED_MB} MB"; exit 3
fi

# --- ověření, že binárka JE ta opravená ------------------------------------
# Bez tohohle se dá celý víkend sbírat korpus proti staré .so (lekce 18.08.:
# noc proběhla správně a nedala se přečíst; a 19.08.: přestavba .so pod
# běžícím sběrem). STAND_UP je event, který stará binárka NEZNÁ.
if ! python3 - <<'PY'
import sys
sys.path[:0] = ["engine/build", "python"]
import bb_engine
r = bb_engine.get_developed_roster("dwarf", 1200)
lgr = bb_engine.simulate_game_logged(r, r, home_ai="macro_mcts", away_ai="macro_mcts",
                                     seed=4242, mcts_iterations=100,
                                     weights_path="weights_best.json",
                                     away_weights_path="weights_best.json",
                                     epsilon=0.0, vf_blend=0.0,
                                     policy_weights_path="weights_policy.json")
turns = lgr.get_turn_logs()
bad = []

# (1) vstávání (P45) -- stará .so tenhle event NEZNÁ
n = sum(1 for t in turns for e in t["events"] if e["type"] == "STAND_UP")
print("STAND_UP v sondě:", n, file=sys.stderr)
if n == 0:
    bad.append("STAND_UP = 0 (stará binárka, nebo se plánovač nezvedá)")

# (2) nová pole desky (21.08.). Klíč, který stará .so vůbec nemá, shodí
# KeyError -- a to je taky správná odpověď, jen míň čitelná, proto .get().
missing = [k for k in ("corridor_strength", "required_pace",
                       "achievable_pace", "dist_to_endzone_board")
           if k not in turns[0]]
if missing:
    bad.append("chybí pole v turn logu: %s" % missing)
else:
    # ⛔ Nestačí, že pole EXISTUJE -- musí se i PLNIT. Přesně tahle vada nás
    # stála celý rok u plan.*: pole tam bylo, bylo v něm 0, a nula se četla
    # jako fakt. N/A je -1, takže "živé" kolo je to s required_pace >= 0.
    live = [t for t in turns if t["required_pace"] >= 0]
    print("kol s živým tempem: %d / %d" % (len(live), len(turns)), file=sys.stderr)
    if not live:
        bad.append("required_pace je N/A ve VŠECH kolech -- metr se neplní")
    elif not any(t["corridor_strength"] > 0 for t in live):
        bad.append("corridor_strength je 0 ve všech živých kolech")

for b in bad:
    print("PREFLIGHT FAIL:", b, file=sys.stderr)
sys.exit(1 if bad else 0)
PY
then
    night_log "⛔ PREFLIGHT SELHAL — viz důvod výš. Nejčastěji: běží STARÁ .so."
    night_log "   Přestav (cmake --build engine/build) a spusť znovu."
    exit 4
fi
night_log "PREFLIGHT OK — binárka tiskne STAND_UP i nové metry desky (tempo, síla koridoru)"
night_log "ROZPOČET: 15 dvojic × $GAMES her ≈ $(( 15 * GAMES * 56 / 10 / 3600 )) h při 5,6 s/hru"

# --- 15 křížů, POSTUPNĚ ----------------------------------------------------
i=0
DONE_PAIRS=""
for A in $RACES; do
  for B in $RACES; do
    # neuspořádané dvojice: každou jen jednou
    case " $DONE_PAIRS " in *" $B|$A "*) continue;; esac
    DONE_PAIRS="$DONE_PAIRS $A|$B"
    i=$(( i + 1 ))
    TAG="${A}-${B}"
    DATA="${DATA_BASE}/${TAG}"
    if [ -f "$DATA/COLLECT_DONE" ]; then
        night_log "[$i/15] $TAG — už hotovo, přeskakuji"; continue
    fi
    mkdir -p "$DATA"
    night_log "[$i/15] START $TAG — $GAMES her"
    if HOME_RACE="$A" OPPONENTS="$B" CAGE_GATE=0 DATA_ROOT="$DATA" \
           SEED_BASE=$(( 20270000 + i * 100000 )) \
           nice -n 19 python3 "$ROOT/diag_replay_mine_20260813_gate.py" collect "$GAMES" \
           > "$OUT/collect_${TAG}.log" 2>&1; then
        night_log "[$i/15] HOTOVO $TAG ($(tail -1 "$OUT/collect_${TAG}.log"))"
    else
        night_log "[$i/15] ⛔ FAIL $TAG — viz $OUT/collect_${TAG}.log"
    fi
  done
done

night_log "VŠECHNY KŘÍŽE HOTOVY — $(ls -d ${DATA_BASE}/*/ 2>/dev/null | wc -l) adresářů"
touch "$OUT/CROSSES_DONE"
