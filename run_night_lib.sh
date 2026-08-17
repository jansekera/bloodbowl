# ============================================================================
# SPOLEČNÉ PRIMITIVY PRO NOČNÍ SPOUŠTĚČE           (zavedeno 17.08.2026)
#
# ⚑ PROČ VZNIKL
#   14.08. se noční běh spouštěl DVAKRÁT: první spuštění se po ~5 minutách
#   zabilo (Q1 test na jedné postavené pozici tvrdil, že si search Black Orka
#   nevybere), sweep přes 36 geometrií to vyvrátil a běh se spustil znovu.
#   Výsledek dopadl dobře, ale POUZE ŠTĚSTÍM — v tehdejším spouštěči byly
#   čtyři vady, které z druhého spuštění dělají loterii:
#
#   1. LOCK NENÍ ODOLNÝ PROTI KILLU. `mkdir .lock` + `trap ... EXIT`. Při
#      `kill -9` (a `pkill` na vzor, který sedí i na rodiče — viz
#      feedback_pkill_self_kill) trap NEBĚŽÍ, zámek zůstane a druhé spuštění
#      tiše skončí hláškou v souboru, který se v tu chvíli nečte.
#   2. ZABITÍ RODIČE NEZABIJE DĚTI. 12 shardů se pouští na `&`. Po zabití
#      rodiče běží dál jako sirotci a zapisují do TÝCHŽ adresářů, do kterých
#      začne psát druhé spuštění. 14.08. to dopadlo dobře jen proto, že se
#      shardy ještě nestihly rozjet.
#   3. ZÁZNAM O PRVNÍM SPUŠTĚNÍ ZMIZEL. `chain.log` má první řádek až v 15:15;
#      po zabitém běhu vypadá noc jako jedno čisté spuštění. To je táž rodina
#      chyby jako audit měřicího aparátu: SNÍMEK SE VYDÁVÁ ZA STAV.
#   4. BASELINE SE NEOVĚŘUJE PROTI COMMITU. Korpus 14.08. běžel na `1dc9ecd2`,
#      baseline `night_big_20260813/` na `e4b99ee` — a mezi nimi jsou TŘI
#      změny enginu (cena hand-offu, kritérium hand-offu, odmítnutí darovaného
#      TD) plus oprava atribuce TD v rozkladu. Rozdíl korpusů se proto nesmí
#      připsat měřenému ramenu, a spouštěč to nikde neřekl.
#
#   Táž křehká kopie zámku je v devíti spouštěčích. Proto lib, ne desátá kopie.
#
# ⚑ POUŽITÍ
#   ROOT=/home/jan/claude/bloodbowl
#   . "$ROOT/run_night_lib.sh"
#   night_init "$OUT" "run-dauntless"      # zámek, úklid dětí, záznam pokusu
#   night_stamp_head "$OUT"                # zapíše ENGINE_HEAD do $OUT
#   night_run_bg <cmd...>                  # potomek pod dohledem (místo `&`)
#   night_wait                             # počká na všechny
#   night_check_baseline "$OUT" "$BASE"    # commit baseline vs náš; vrací 1
#
# Vše loguje do $OUT/chain.log a NIKDY do něj nepíše přes `>`.
# ============================================================================

NIGHT_KIDS=""

night_log() { echo "[$(date -u '+%m-%d %H:%M')] $*" >> "$NIGHT_LOG"; }

# --- zámek, který přežije kill -9 -------------------------------------------
# Do zámku se píše PID. Zámek po mrtvém procesu je STARÝ, ne platný: sebere se
# a ZALOGUJE se to. Zámek živého procesu drží dál (jeden běh na stroj).
night_lock() {
    local dir="$1/.lock"
    while : ; do
        if mkdir "$dir" 2>/dev/null; then echo $$ > "$dir/pid"; return 0; fi
        local old; old=$(cat "$dir/pid" 2>/dev/null || echo "")
        if [ -n "$old" ] && kill -0 "$old" 2>/dev/null; then
            night_log "ABORT: zámek drží ŽIVÝ pid $old — nespouštím druhý běh"
            return 1
        fi
        night_log "⚠️ starý zámek po mrtvém pid '${old:-?}' — beru si ho"
        rm -rf "$dir"
    done
}

# --- úklid dětí -------------------------------------------------------------
# Bez tohohle přežije zabití rodiče 12 harness procesů, které pak píšou do
# stejných adresářů jako druhé spuštění. Chytáme i INT/TERM, ne jen EXIT.
night_cleanup() {
    local code=$?
    if [ -n "$NIGHT_KIDS" ]; then
        for p in $NIGHT_KIDS; do
            if kill -0 "$p" 2>/dev/null; then
                night_log "úklid: zabíjím potomka $p"
                kill -TERM "$p" 2>/dev/null
            fi
        done
        sleep 2
        for p in $NIGHT_KIDS; do kill -KILL "$p" 2>/dev/null; done
    fi
    [ -n "${NIGHT_LOCKDIR:-}" ] && rm -rf "$NIGHT_LOCKDIR"
    [ "$code" -ne 0 ] && night_log "konec s kódem $code"
    return $code
}

night_run_bg() { "$@" & NIGHT_KIDS="$NIGHT_KIDS $!"; }
night_wait()   { wait; NIGHT_KIDS=""; }

# --- inicializace -----------------------------------------------------------
# Každý POKUS o spuštění dostane vlastní řádek, i ten, který skončí abortem.
# Pokus se čísluje, takže „spustili jsme to dvakrát" je vidět ze souboru.
night_init() {
    local out="$1" name="${2:-night}"
    mkdir -p "$out"
    NIGHT_LOG="$out/chain.log"
    NIGHT_LOCKDIR="$out/.lock"
    local n; n=$(( $(grep -c '^\[.*POKUS ' "$NIGHT_LOG" 2>/dev/null || echo 0) + 1 ))
    night_log "POKUS $n — $name, pid $$, HEAD $(git -C "$(dirname "$out")" rev-parse --short HEAD 2>/dev/null || echo '?')"
    trap night_cleanup EXIT INT TERM
    night_lock "$out" || exit 1
}

# --- otisk commitu ----------------------------------------------------------
# Píše se JEN commit, který sahá na engine/. Doc commity se mezi baseline
# a měřením lišit smějí, engine ne.
night_stamp_head() {
    local out="$1" root; root=$(dirname "$out")
    git -C "$root" rev-parse --short HEAD > "$out/HEAD" 2>/dev/null
    git -C "$root" log -1 --format=%H -- engine/ > "$out/ENGINE_HEAD" 2>/dev/null
    night_log "engine HEAD $(cut -c1-8 < "$out/ENGINE_HEAD" 2>/dev/null || echo '?')"
}

# --- kontrola baseline ------------------------------------------------------
# Vrací 0 jen když baseline vznikla na TÉMŽE commitu enginu. Jinak 1 a hlasitý
# záznam — a volající NESMÍ ten rozdíl připsat svému ramenu.
night_check_baseline() {
    local out="$1" base="$2" root; root=$(dirname "$out")
    local mine theirs
    mine=$(cat "$out/ENGINE_HEAD" 2>/dev/null)
    theirs=$(cat "$base/ENGINE_HEAD" 2>/dev/null)
    if [ -z "$theirs" ]; then
        night_log "⛔ baseline $base NEMÁ ENGINE_HEAD ⇒ srovnatelnost NEPROKÁZÁNA"
        night_log "   (běhy před 17.08. otisk nemají — ověřit ručně přes git log engine/)"
        return 1
    fi
    if [ "$mine" != "$theirs" ]; then
        local n; n=$(git -C "$root" rev-list --count "$theirs..$mine" -- engine/ 2>/dev/null || echo '?')
        night_log "⛔ baseline běžela na JINÉM enginu (${n} commitů rozdíl)"
        night_log "   ⇒ rozdíl korpusů NENÍ efekt ramene. Buď baseline přeběhnout, nebo nesrovnávat."
        git -C "$root" log --oneline "$theirs..$mine" -- engine/ 2>/dev/null \
            | sed 's/^/       /' >> "$NIGHT_LOG"
        return 1
    fi
    night_log "baseline OK — týž engine commit"
    return 0
}
