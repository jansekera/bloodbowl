#!/bin/bash
# Rychlý přehled stavu Blood Bowl strojovny — pro kontrolu přes SSH bez Clauda.
# Použití: ./status_bb.sh   (z /home/jan/claude/bloodbowl)
# Aktualizováno 06.08.: promotion freeze éra — per-race audit, měřicí řetěz
# tempo doktríny, archivy, noční launcher.
cd "$(dirname "$0")"
echo "=== $(date '+%d.%m. %H:%M %Z') ==="

echo
echo "--- Běžící procesy (měření/trénink/launchery) ---"
ps -eo pid,etime,ni,cmd | grep -E "run_iteration|train_cli|diag_f1|probe |run_tempo_measure|launch_staged" \
    | grep -v grep || echo "(nic neběží)"

echo
echo "--- Denní iterace #2 (training_staged_20260806.log) ---"
if [ -f training_staged_20260806.log ]; then
    grep -E "PROMOTED|REJECTED|FREEZE|New vs Frozen" training_staged_20260806.log | tail -4 \
        || echo "(běží — verdikt zatím není)"
fi

echo
echo "--- Noční iterace #3 (training_staged_20260806_night.log) ---"
[ -f launch_staged_night_20260806.done ] && cat launch_staged_night_20260806.done
if [ -f training_staged_20260806_night.log ]; then
    grep -E "PROMOTED|REJECTED|FREEZE|New vs Frozen" training_staged_20260806_night.log | tail -4 \
        || echo "(běží — verdikt zatím není)"
else
    echo "(ještě nestartovala — launcher čeká na ALL_DONE + 18:30 UTC)"
fi

echo
echo "--- Per-race audit poslední brány (kdo hraje rasu → WR kandidáta) ---"
last_gate=$(ls -t training_staged_20260806*.log 2>/dev/null | head -1)
[ -n "$last_gate" ] && grep -A6 "Per-race audit" "$last_gate" | tail -7

echo
echo "--- Tempo doktrína: měřicí řetěz (tempo_measure_20260806/) ---"
if [ -d tempo_measure_20260806 ]; then
    tail -3 tempo_measure_20260806/chain.log 2>/dev/null
    ls tempo_measure_20260806/*DONE* 2>/dev/null || echo "(markery zatím žádné)"
fi
[ -f evidence/fable_tempo_doctrine_report_20260806.md ] \
    && echo "report: $(wc -l < evidence/fable_tempo_doctrine_report_20260806.md) řádků, změněn $(date -r evidence/fable_tempo_doctrine_report_20260806.md '+%H:%M')"

echo
echo "--- Archivy (data se neztrácí) ---"
echo "kandidáti: $(ls candidates_archive/ 2>/dev/null | wc -l) | policy zálohy: $(ls policy_backups/ 2>/dev/null | wc -l) | replay: $(ls replay_archive/ 2>/dev/null | wc -l) | metriky: $(ls metrics_archive/ 2>/dev/null | wc -l)"

echo
echo "--- Šampion (musí být 17578260... — pod PROMOTION FREEZE se nesmí měnit) ---"
md5sum weights_best.json weights_policy.json 2>/dev/null
echo "(weights_policy.json = Živá, mění se každou iterací — to je v pořádku)"
