#!/bin/bash
# teacher_signal_watcher: runs analyses when collections finish, then copies
# artifacts into evidence/diag_teacher_signal_20260810/. Survives the session.
DIR=/tmp/claude-1000/-home-jan-claude/01724f63-791e-456a-936f-c5215d2631bf/scratchpad/teacher_signal
EV=/home/jan/claude/bloodbowl/evidence/diag_teacher_signal_20260810
cd "$DIR" || exit 1
mkdir -p "$EV"

# --- test 1 analysis after value collection ---
for i in $(seq 1 240); do
    if [ -f done_val_dwsk_a ] && [ -f done_val_dwsk_b ] && [ -f done_val_wesk ]; then
        break
    fi
    sleep 30
done
if [ -f done_val_dwsk_a ] && [ -f done_val_dwsk_b ] && [ -f done_val_wesk ] \
   && [ ! -f done_val_analysis ]; then
    python3 diag_analyze_teacher_value.py > teacher_value_analysis.log 2>&1 \
        && touch done_val_analysis
    cp -f vrows_*.jsonl val_*.log diag_teacher_value_collect.cpp \
          diag_analyze_teacher_value.py "$EV/" 2>/dev/null
fi

# --- test 3: copy results when training done ---
for i in $(seq 1 480); do
    if [ -f done_train_cap ]; then break; fi
    sleep 60
done
cp -f train_cap.log diag_train_ab_cap.py "$EV/" 2>/dev/null

# --- test 2 analysis after m400 collection (may take until morning) ---
for i in $(seq 1 1440); do
    if [ -f done_m400_all ]; then break; fi
    sleep 60
done
if [ -f done_m400_all ] && [ ! -f done_m400_analysis ]; then
    python3 diag_analyze_m400.py > m400_analysis.log 2>&1 \
        && touch done_m400_analysis
    cp -f rows_m400_*.jsonl m400_collect.log diag_feature_ab_collect_m400.cpp \
          diag_analyze_m400.py m400_analysis.log "$EV/" 2>/dev/null
fi
touch done_watcher
