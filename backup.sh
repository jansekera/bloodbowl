#!/bin/bash
# Blood Bowl backup script
# Copies important training data to backup location
# Usage: ./backup.sh [optional: backup_dir]
#
# ZÁLOHOVACÍ KRAJINA k 29.08.2026 (ověřeno, ne odhadnuto):
#   /home/jenda/zal/claude/            záloha DOMOVSKÉHO ADRESÁŘE serveru
#                                      jan@linuxs, stav 26.08. -- server od
#                                      29.08. neexistuje, tohle je jediné, co
#                                      po něm zbylo (paměť, bb-data, snapshoty)
#   /home/jenda/zal/claude2/blood-bowl/ dubnová kopie repa (424 MB) -- výchozí
#                                      cíl tohohle skriptu, funguje
#   /home/jenda/claude/bloodbowl/      druhý klon téhož remotu, ne záloha
#
# ⚠️ Skript zálohuje jen weights/logy/benchmark CSV. NEZÁLOHUJE dva soubory,
#    které jsou v .gitignore a bez kterých se noc nespustí:
#      weights_policy.json   (spadá pod /weights*.json -- POZOR, `cp weights*.json`
#                             níž ho ve skutečnosti vezme, takže je krytý)
#      rules_bb2016.txt      NENÍ krytý -- doplnit ručně, viz níž

BACKUP_DIR="${1:-/home/jenda/zal/claude2/blood-bowl}"
SRC="/home/jenda/claude/blood-bowl"
DATE=$(date +%Y%m%d_%H%M)

mkdir -p "$BACKUP_DIR/weights"
mkdir -p "$BACKUP_DIR/logs"

# 1. All weights files (small, critical)
cp -v "$SRC"/weights*.json "$BACKUP_DIR/weights/" 2>/dev/null

# 2. Training logs (medium, useful for analysis)
cp -v "$SRC"/*.log "$BACKUP_DIR/logs/" 2>/dev/null

# 3. Benchmark CSV
cp -v "$SRC"/benchmark_results.csv "$BACKUP_DIR/" 2>/dev/null

# 4. Soubory mimo git, bez kterých se A/B noc nespustí (29.08.2026).
#    Čerstvý klon je nemá -- Colab klonuje vždycky a harness bez
#    weights_policy.json končí rc=1.
cp -v "$SRC"/rules_bb2016.txt "$BACKUP_DIR/" 2>/dev/null

# 5. Summary
echo ""
echo "=== Backup complete: $DATE ==="
echo "Destination: $BACKUP_DIR"
du -sh "$BACKUP_DIR/weights/" "$BACKUP_DIR/logs/" 2>/dev/null
