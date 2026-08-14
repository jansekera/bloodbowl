#!/bin/bash
cd /home/jan/claude/bloodbowl

echo "[$(date '+%F %T')] chain: waiting for macro-floor audit (PID 55218) to exit"
while ps -p 55218 > /dev/null 2>&1; do sleep 5; done
echo "[$(date '+%F %T')] chain: macro-floor audit process gone, tail of its log:"
tail -20 diag_macro_floor_audit_20260717.log

echo "[$(date '+%F %T')] chain: launching policyBlend bring-up (all arms)"
PB_WORKERS=8 ./venv/bin/python3 diag_policy_blend_bringup_20260716.py all \
    > diag_policy_blend_bringup_20260717.log 2>&1
echo "[$(date '+%F %T')] chain: policyBlend bring-up finished, exit=$?"
echo "[$(date '+%F %T')] CHAIN ALL DONE"
