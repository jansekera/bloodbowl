#!/usr/bin/env bash
# ============================================================================
# NOC NA LAPTOPU — dohlížený, restartovatelný běh            (29.08.2026)
#
# ⛔ PROČ SAMOSTATNÝ SPOUŠTĚČ A NE JEN `colab_night_chunked.py`:
#   laptop je NÁCHYLNÝ K PŘERUŠENÍ způsobem, jakým server přes SSH nebyl.
#   Tam stačilo `setsid` + odpojit se. Tady hrozí čtyři různé věci a každá
#   se řeší jinak:
#
#     (1) USPÁNÍ / ZAVŘENÉ VÍKO  -- procesy se zmrazí a po probuzení jedou
#         dál, takže o práci nepřijdeš, ALE stroj mezitím nic nepočítá.
#         ⇒ `systemd-inhibit` uspání na dobu běhu ZAKÁŽE.
#     (2) ZAVŘENÝ TERMINÁL       -- shell pošle SIGHUP a děti umřou.
#         ⇒ `setsid` + `nohup`, běh má vlastní sezení. (Táž praxe jako
#            u dlouhých běhů na serveru.)
#     (3) PÁD KUSU               -- ⇒ smyčka pustí runner znovu; kus bez `OK`
#         se udělá celý znovu, hotové se přeskočí.
#     (4) RESTART STROJE         -- všechno umře; po naběhnutí spusť TENTÝŽ
#         příkaz. Otisk běhu ohlídá, že je to pořád týž engine.
#
# ⚠️ Co tenhle skript NEUMÍ: nastartovat se sám po rebootu. Kdyby to mělo
#    přežít i restart bez člověka, patří to do systemd unit, ne sem.
#
# POUŽITÍ
#   ./run_laptop_night.sh                # spustí na pozadí a hned se vrátí
#   ./run_laptop_night.sh --status       # jak to jde
#   ./run_laptop_night.sh --stop         # zastaví (hotové kusy zůstanou)
# ============================================================================
set -u
ROOT="$(cd "$(dirname "$0")" && pwd)"
SELF="$ROOT/$(basename "$0")"

OUT=${OUT:-$ROOT/ab_b2_20260829}
MODE=${MODE:-11}
MATCHUPS=${MATCHUPS:-"2:dw-dw:1 7:dwnw:0"}
PAIRS=${PAIRS:-4800}
CHUNKS=${CHUNKS:-48}
NULL_PAIRS=${NULL_PAIRS:-400}
WORKERS=${WORKERS:-8}
LOG="$OUT/laptop_night.log"
PIDFILE="$OUT/laptop_night.pid"
MAX_RETRY=${MAX_RETRY:-20}
# ⭐ ROZDĚLENÍ NA POLOVINY: `SESSION_HOURS=11` nechá běh po ~11 h čistě
#   skončit (rozběhlé kusy dojedou, nové se nepouštějí) a druhý den se
#   TÝMŽ příkazem naváže. Default 999 = bez limitu, jeden dlouhý běh.
#   ⛔ MEZI POLOVINAMI SE NESMÍ PŘESTAVĚT ENGINE ani přepnout commit --
#     otisk běhu to odmítne, a odmítne správně: jedna noc = jedno měření.
SESSION_HOURS=${SESSION_HOURS:-999}

alive() {   # ⛔ NE `pgrep -f`: ten si namatchne sám sebe (past z 21.08.).
    [ -f "$PIDFILE" ] || return 1
    local p; p=$(cat "$PIDFILE" 2>/dev/null) || return 1
    [ -n "$p" ] && [ -d "/proc/$p" ] || return 1
    # ⛔ `/proc/$p` existuje i pro CIZÍ proces, kterému systém recykloval PID.
    #   Bez téhle kontroly by běh falešně hlásil "už běží" a odmítl start.
    #   (Táž rodina jako past `pgrep -f`, jen z druhé strany.)
    tr '\0' ' ' < "/proc/$p/cmdline" 2>/dev/null | grep -q 'run_laptop_night\|bloodbowl-night'
}

# --- vlastní smyčka: běží uvnitř systemd-inhibit, aby se stroj neuspal -------
supervise() {
    echo "=== START $(date '+%F %T') — mode $MODE, $PAIRS+$NULL_PAIRS párů, $CHUNKS kusů, $WORKERS workerů, limit ${SESSION_HOURS} h"
    local try=0
    while [ "$try" -lt "$MAX_RETRY" ]; do
        if [ -f "$OUT/AB_DONE" ]; then
            # ⚠️ `try` je počet POKUSŮ, ne restartů: první průchod je pokus 1
            #    a restartů bylo nula. Popisek, který tvrdí něco jiného, než co
            #    se stalo, je přesně to, co se 27.08. našlo třikrát za den.
            echo "=== AB_DONE $(date '+%F %T') — hotovo na $try. pokus (restartů: $((try - 1)))"
            echo "Teď sloučit:"
            echo "  PREREG=evidence/night_prereg_20260829_b2.preds THRESHOLD=0.015 \\"
            echo "  python3 night_summarize.py $OUT dw-dw dwnw"
            return 0
        fi
        try=$((try + 1))
        echo "--- pokus $try/$MAX_RETRY  $(date '+%F %T')"
        python3 -u "$ROOT/colab_night_chunked.py" \
            --mode "$MODE" --matchups "$MATCHUPS" --out "$OUT" \
            --pairs "$PAIRS" --chunks "$CHUNKS" --null-pairs "$NULL_PAIRS" \
            --workers "$WORKERS" --session-hours "$SESSION_HOURS" || {
                rc=$?
                # ⛔ 8 = neshoda otisku běhu. To NENÍ pád, ze kterého se dá
                #   zotavit opakováním -- engine se změnil a opakování by jen
                #   dvacetkrát zopakovalo tutéž odmítnutou hlášku.
                if [ "$rc" = "8" ]; then
                    echo "⛔ otisk běhu nesedí (rc=8). Nepokračuji — viz výpis výš."
                    return 8
                fi
                if [ "$rc" = "3" ]; then
                    # Vyčerpaný rozpočet sezení NENÍ pád. Opakovat by znamenalo
                    # limit obejít -- což je přesně to, co uživatel nechtěl.
                    echo "⏸  Rozpočet sezení vyčerpán $(date '+%F %T'). Zbytek"
                    echo "   dojede příště: pusť TENTÝŽ příkaz, hotové kusy se"
                    echo "   přeskočí. ⛔ Nepřestavuj mezitím engine."
                    return 3
                fi
                echo "runner skončil rc=$rc, zkusím znovu za 30 s"
                sleep 30
            }
    done
    echo "⛔ vyčerpáno $MAX_RETRY pokusů, končím. Podívej se do $OUT/*/run.log"
    return 1
}

case "${1:-}" in
  --_supervise)
        # ⛔ SKRYTÝ REŽIM: skript se v odpojeném sezení volá SÁM SEBE.
        #   Do 29.08. se sem místo toho vkládala funkce přes
        #   `$(declare -f supervise)` do `bash -c '...'` -- jenže ta funkce
        #   obsahuje apostrofy (`'+%F %T'`), takže uvozování skončilo uprostřed
        #   a odpojený běh umřel na "unexpected EOF". Chytila to až ostrá
        #   zkouška; `bash -n` na hlavním skriptu je v pořádku, protože ta
        #   chyba vzniká až SLOŽENÍM řetězce za běhu.
        # ⛔ ÚKLID PIDFILE PATŘÍ SEM: původně se mazal na řádku ZA
        #   `bash -c`, jenže oprava přes `exec` ten shell nahradila, takže se
        #   ten řádek nikdy neprovedl a pidfile zůstával ležet. Objevila to až
        #   zkouška rozpočtu sezení, ne čtení.
        trap 'rm -f "$PIDFILE"' EXIT
        supervise; exit $?;;
  --status)
        if alive; then echo "BĚŽÍ (pid $(cat "$PIDFILE"))"; else echo "neběží"; fi
        if [ -d "$OUT" ]; then
            ok=$(find "$OUT" -name OK 2>/dev/null | wc -l)
            fail=$(find "$OUT" -name FAIL 2>/dev/null | wc -l)
            echo "kusů hotových: $ok, selhalých: $fail"
            [ -f "$OUT/AB_DONE" ] && echo "✅ AB_DONE — noc je celá hotová"
        fi
        [ -f "$LOG" ] && { echo "--- posledních 12 řádků logu:"; tail -12 "$LOG"; }
        exit 0;;
  --stop)
        if alive; then
            p=$(cat "$PIDFILE"); kill -- -"$p" 2>/dev/null || kill "$p" 2>/dev/null
            echo "zastaveno (pid $p). Hotové kusy zůstávají, běh se dá navázat."
        else echo "neběží"; fi
        rm -f "$PIDFILE"; exit 0;;
esac

if alive; then
    echo "⛔ už běží (pid $(cat "$PIDFILE")). Nespouštím podruhé — dva běhy do"
    echo "   téhož OUT by si přepisovaly kusy. Viz --status."
    exit 1
fi

mkdir -p "$OUT"

export OUT MODE MATCHUPS PAIRS CHUNKS NULL_PAIRS WORKERS MAX_RETRY LOG PIDFILE ROOT SESSION_HOURS
INHIBIT=""
command -v systemd-inhibit >/dev/null && \
    INHIBIT="systemd-inhibit --what=sleep:idle --why=bloodbowl-night --mode=block"

# `exec` drží PID: zapsané číslo je zároveň PGID celé skupiny (setsid udělal
# nové sezení), takže `--stop` může poslat signál CELÉ skupině a nezůstanou
# viset běžící kusy.
setsid nohup bash -c "
    echo \$\$ > '$PIDFILE'
    exec $INHIBIT '$SELF' --_supervise
" >> "$LOG" 2>&1 &
disown 2>/dev/null || true

sleep 1
echo "spuštěno na pozadí, log: $LOG"
[ -n "$INHIBIT" ] && echo "uspání stroje je po dobu běhu ZAKÁZÁNO (systemd-inhibit)" \
                  || echo "⚠️ systemd-inhibit nenalezen — hlídej, ať se stroj neuspí"
echo "stav:  ./run_laptop_night.sh --status"
echo "stop:  ./run_laptop_night.sh --stop"
