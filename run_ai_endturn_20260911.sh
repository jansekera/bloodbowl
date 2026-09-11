#!/usr/bin/env bash
# Tri kouci, jedno meridlo. Sekvencne, ať si neberou jádra navzájem.
set -u
cd /home/jenda/claude/blood-bowl || exit 3
for c in greedy learning random; do
    echo "=== $c ==="
    timeout 3000 php cli/diag_ai_endturn_20260911.php "$c" 60 20260911 2>&1 | grep -v "^PHP Warning"
    echo
done
