#!/usr/bin/env bash
# ============================================================================
# BEZPEČNÉ ZASTAVENÍ VÍKENDOVÉHO ŘETĚZU                    (21.08.2026)
#
# ⚑ PROČ EXISTUJE
#   Uživatel 21.08.: „pozor — minule nám restart nadělal paseku."
#   Ta paseka (19.08.) NEBYLA z restartu, ale z toho, že se `.so` PŘESTAVĚL
#   POD BĚŽÍCÍM SBĚREM. Bezpečné pořadí je tvrdě:
#       ZABÍT → OVĚŘIT, ŽE NIC NEBĚŽÍ → přestavět → otestovat → spustit
#   Tenhle skript dělá první dva kroky a NIC JINÉHO. Nestaví, nespouští.
#
# ⚑ PAST, KTERÁ MĚ DNES DVAKRÁT CHYTILA
#   `pgrep -f run_weekend` matchne i shell, ve kterém ten pgrep běží, protože
#   vzorek je v jeho vlastní příkazové řádce. Zabít ho pak znamená zabít sebe.
#   ⇒ Níž se každý PID ověřuje proti /proc/<pid>/cmdline a shelly se vyhazují.
# ============================================================================
set -u
ROOT="$(cd "$(dirname "$0")" && pwd)"
PATTERN='run_weekend_20260821|run_crosses_20260821|run_night_ab.sh|diag_replay_mine_20260813_gate'

find_pids() {
    for p in /proc/[0-9]*; do
        pid=${p#/proc/}
        [ "$pid" = "$$" ] && continue
        cl=$(tr '\0' ' ' < "$p/cmdline" 2>/dev/null) || continue
        # vyhoď obalové shelly a vlastní snapshoty
        case "$cl" in *shell-snapshots*|*stop_weekend*) continue;; esac
        echo "$cl" | grep -qE "$PATTERN" && echo "$pid"
    done
}

PIDS=$(find_pids | tr '\n' ' ')
if [ -z "${PIDS// /}" ]; then
    echo "✅ nic neběží — není co zastavovat"; exit 0
fi
echo "zastavuji PIDy: $PIDS"
for p in $PIDS; do kill -TERM "$p" 2>/dev/null; done
for i in 1 2 3 4 5 6 7 8 9 10; do
    sleep 2
    [ -z "$(find_pids | tr '\n' ' ' | tr -d ' ')" ] && break
done
LEFT=$(find_pids | tr '\n' ' ')
if [ -n "${LEFT// /}" ]; then
    echo "⚠️ po TERM zbylo: $LEFT — posílám KILL"
    for p in $LEFT; do kill -KILL "$p" 2>/dev/null; done
    sleep 3
fi
LEFT=$(find_pids | tr '\n' ' ')
if [ -n "${LEFT// /}" ]; then
    echo "⛔ POŘÁD BĚŽÍ: $LEFT — NEPŘESTAVUJ .so, dokud to nezmizí"; exit 1
fi
echo "✅ zastaveno, nic neběží"
echo
echo "Sebráno do zastavení:"
for d in "$ROOT"/crosses_20260821_data/*/; do
    [ -d "$d" ] || continue
    n=$(ls "$d" 2>/dev/null | grep -c '\.json\.gz$')
    printf "  %-22s %5d her%s\n" "$(basename "$d")" "$n" \
        "$([ -f "$d/COLLECT_DONE" ] && echo '  (hotovo)')"
done
echo
echo "⇒ Data ZŮSTÁVAJÍ na disku. Než pustíš znovu, SMAŽ je —"
echo "  jinak nový běh dvojice s hotovým COLLECT_DONE přeskočí a korpus bude"
echo "  míchaný ze dvou enginů, což je přesně vada, kvůli které restartujeme."
echo "  rm -rf crosses_20260821 crosses_20260821_data weekend_20260821.log"
