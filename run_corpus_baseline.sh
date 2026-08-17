#!/usr/bin/env bash
# ============================================================================
# ČERSTVÁ BASELINE KORPUSU NA DNEŠNÍM ENGINU              (17.08.2026)
#
# ⚑ PROČ
#   Po víkendu nemáme **žádnou použitelnou baseline**. `night_big_20260813/`
#   běžela na `engine@9f98070c`, dnešek je `engine@5e5ab352` — a mezi nimi jsou
#   změny, které mění chování (2× hand-off, odmítnutí darovaného TD, a teď
#   navíc P13 Dauntless v produkci). `night_check_baseline` proto každé
#   srovnání s ní správně odmítne (P22) ⇒ **měna drivů je dnes neměřitelná,
#   dokud nevznikne korpus se známým otiskem enginu.**
#
#   Tenhle běh ten otisk vyrobí. Produkční nastavení, žádné rameno:
#   brána klece OFF (produkce), Dauntless ON (produkce od 17.08. — a default
#   collectoru je teď taky 1, takže se nepředává; kdyby se předával, byla by to
#   další příležitost, jak sbírat korpus jiného enginu, než jaký běží).
#
# ⚑ PROČ NEPOUŽÍVÁ `night_preflight`
#   Preflight odmítá spuštění, když už jiný běh běží — což je správné pro A/B,
#   které si konkurují o tytéž adresáře a jádra. Sběr korpusu je jednoprocesový
#   a běží vedle A/B bez konfliktu. Bere si ale **vlastní zámek** a odmítne se
#   spustit, když už jiný SBĚR běží; to je ten skutečný konflikt (sdílený
#   `DATA_ROOT` a jeden a týž skript).
#
# ⛔ POZOR: NEEDITOVAT SPOUŠTĚČ, KTERÝ PRÁVĚ BĚŽÍ. Bash čte skript průběžně
#    podle bytového offsetu, takže úprava za běhu umí běžící instanci rozhodit.
#    Proto tenhle soubor vznikl jako nový, a ne jako záplata do `run_night_ab.sh`.
# ============================================================================
set -u
ROOT="$(cd "$(dirname "$0")" && pwd)"
. "$ROOT/run_night_lib.sh"

GAMES=${GAMES:-3000}
OUT="$ROOT/${OUT:-corpus_baseline_20260817}"
DATA="$ROOT/${DATA:-corpus_baseline_20260817_data}"

night_init "$OUT" "corpus-baseline"
night_stamp_head "$OUT"

# vlastní kontrola souběhu: jen jiný SBĚR vadí, A/B ne
for p in /proc/[0-9]*; do
    pid=${p#/proc/}; [ "$pid" = "$$" ] && continue
    exe=$(readlink "$p/exe" 2>/dev/null) || continue
    case "${exe% (deleted)}" in
        */python3|*/python)
            if tr '\0' ' ' < "$p/cmdline" 2>/dev/null | grep -q 'diag_replay_mine_2026'; then
                night_log "⛔ už běží jiný sběr (pid $pid) — nespouštím"; exit 1
            fi ;;
    esac
done

if [ -n "$(git -C "$ROOT" status --porcelain -- engine/ 2>/dev/null)" ]; then
    night_log "⛔ engine/ má nezacommitované změny — baseline by nesla otisk,"
    night_log "   který neodpovídá kódu. To je přesně vada P22. Nespouštím."
    exit 2
fi

if [ -f "$DATA/COLLECT_DONE" ]; then
    night_log "korpus už existuje, přeskakuji sběr"
else
    night_log "START sběr $GAMES her, produkční nastavení, žádné rameno"
    if CAGE_GATE=0 DATA_ROOT="$DATA" SEED_BASE=20261000 \
            nice -n 19 python3 "$ROOT/diag_replay_mine_20260813_gate.py" collect "$GAMES" \
            > "$OUT/collect.log" 2>&1; then
        night_log "sběr HOTOV"
    else
        night_log "FAIL sběr — viz $OUT/collect.log"; exit 1
    fi
fi

night_log "START rozklad drivů"
nice -n 19 python3 "$ROOT/diag_drive_failure_20260811.py" "$DATA" > "$OUT/drives.txt" 2>&1
night_log "START kontroly"
nice -n 19 python3 "$ROOT/diag_rules_checks_20260812.py" "$DATA/*.json.gz" > "$OUT/checks.txt" 2>&1

if grep -q "PŘIJÍMACÍ DRIVY" "$OUT/drives.txt" && grep -q "K33" "$OUT/checks.txt"; then
    night_log "HOTOVO — baseline s otiskem engine $(cut -c1-8 < "$OUT/ENGINE_HEAD")"
    night_log "  od teď se s ní SMÍ srovnávat (night_check_baseline to ověří)"
    touch "$OUT/BASELINE_DONE"
else
    night_log "PARTIAL: analýzy nedoběhly celé"
fi
