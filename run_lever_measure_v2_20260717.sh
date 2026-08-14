#!/bin/bash
cd /home/jan/claude/bloodbowl
OUT=engine_build_lever_test
echo "[$(date '+%F %T')] v2: launching 4 lever arms in parallel (fixed env-flag bug)"

env -u BB_LEVER_A -u BB_LEVER_C ./$OUT/diag_lever_harness . > $OUT/arm_baseline_v2.out 2>&1 &
P1=$!
env BB_LEVER_A=1 -u BB_LEVER_C ./$OUT/diag_lever_harness . > $OUT/arm_leverA_v2.out 2>&1 &
P2=$!
env -u BB_LEVER_A BB_LEVER_C=1 ./$OUT/diag_lever_harness . > $OUT/arm_leverC_v2.out 2>&1 &
P3=$!
env BB_LEVER_A=1 BB_LEVER_C=1 ./$OUT/diag_lever_harness . > $OUT/arm_leverAC_v2.out 2>&1 &
P4=$!

echo "[$(date '+%F %T')] PIDs: baseline=$P1 leverA=$P2 leverC=$P3 leverAC=$P4"
wait $P1; echo "[$(date '+%F %T')] baseline done, exit=$?"
wait $P2; echo "[$(date '+%F %T')] leverA done, exit=$?"
wait $P3; echo "[$(date '+%F %T')] leverC done, exit=$?"
wait $P4; echo "[$(date '+%F %T')] leverAC done, exit=$?"
echo "[$(date '+%F %T')] LEVER MEASURE V2 ALL DONE"
