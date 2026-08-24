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

# --- PREFLIGHT --------------------------------------------------------------
# ⚑ NEJNEBEZPEČNĚJŠÍ SELHÁNÍ NOČNÍHO BĚHU NENÍ PÁD — JE TO 14 HODIN NA STARÉM
#   KÓDU. Nic to nehlásí, výsledek vypadá normálně a připíše se změně, která
#   v binárce vůbec nebyla. 14.08. se harness přebudoval ručně (`271e9dfc`)
#   a NIC to neověřilo; kdyby se na to zapomnělo, měřila by se noc naprázdno.
#   Sdílená knihovna je horší než binárka: linkuje se přes rpath až za běhu,
#   takže starý `libbb_engine.so` nepozná ani `ls -l` na binárce.
#
# night_preflight <binárka> <výstupní_adresář> [zdroj.cpp ...]
# Vrací 0 jen když projde VŠECHNO. Volající má na nesplněné skončit, ne pokračovat.
night_preflight() {
    local bin="$1" out="$2"; shift 2
    local root; root=$(dirname "$out")
    local ok=0

    # (1) binárka existuje a je spustitelná
    if [ ! -x "$bin" ]; then
        night_log "PREFLIGHT ⛔ chybí nebo není spustitelná: $bin"; ok=1
    else
        # (2) binárka není starší než své zdroje
        local src
        for src in "$@"; do
            [ -f "$src" ] || { night_log "PREFLIGHT ⛔ chybí zdroj $src"; ok=1; continue; }
            if [ "$src" -nt "$bin" ]; then
                night_log "PREFLIGHT ⛔ $src je NOVĚJŠÍ než $bin ⇒ binárka je stará, PŘELOŽIT"
                ok=1
            fi
        done
    fi

    # (2b) ⛔⛔ ABI: binárka nesmí být STARŠÍ NEŽ .so (24.08.2026).
    #      Tohle je díra, na kterou umřela fáze B víkendového řetězu.
    #      `diag_f1_cage_advance` byl přeložený 20.08. proti tehdejším hlavičkám;
    #      21.08. přibyla do `struct Player` pole (stunnedThisTurn,
    #      dodgeRerollUsedThisTurn, sureFeetRerollUsedThisTurn,
    #      bigGuyCheckedThisTurn) a .so se přestavěl. Struktura změnila velikost,
    #      stará binárka sahala na špatné offsety ⇒ SEGFAULT hned na první hře.
    #      Kontroly (2) a (3) to NECHYTILY: zdroj harnessu se neměnil a .so bylo
    #      čerstvé -- obě prošly. Chyběl přesně tenhle vztah mezi nimi.
    #      ⚠️ Není vidět na binárce: linkuje se až za běhu přes rpath.
    local so_abi="$root/engine/build/libbb_engine.so"
    if [ -x "$bin" ] && [ -f "$so_abi" ] && [ "$so_abi" -nt "$bin" ]; then
        night_log "PREFLIGHT ⛔ ABI: $so_abi je NOVĚJŠÍ než $bin"
        night_log "   ⇒ harness je přeložený proti STARÝM hlavičkám. Pokud se od té"
        night_log "     doby změnil struct Player/GameState, spadne to na SEGFAULT."
        night_log "   Oprava: g++ -O2 -std=c++20 -Iengine/include -Iengine/third_party \\"
        night_log "           diag_f1_cage_advance_harness.cpp -Lengine/build -lbb_engine \\"
        night_log "           -Wl,-rpath,\$PWD/engine/build -o diag_f1_cage_advance"
        ok=1
    fi

    # (3) engine .so není starší než engine/src a engine/include -- tohle je ta
    #     zákeřná: linkuje se až za běhu přes rpath, na binárce není vidět
    local so="$root/engine/build/libbb_engine.so"
    if [ ! -f "$so" ]; then
        night_log "PREFLIGHT ⛔ chybí $so"; ok=1
    else
        local newer
        newer=$(find "$root/engine/src" "$root/engine/include" -name '*.cpp' -o -name '*.h' 2>/dev/null \
                | while read -r f; do [ "$f" -nt "$so" ] && echo "$f"; done | head -3)
        if [ -n "$newer" ]; then
            night_log "PREFLIGHT ⛔ engine je novější než libbb_engine.so ⇒ běželo by se na STARÉM enginu:"
            echo "$newer" | sed 's/^/       /' >> "$NIGHT_LOG"
            ok=1
        fi
    fi

    # (4) váhy, bez kterých harness stejně skončí -- ale až po startu
    local w
    for w in weights_best.json weights_policy.json; do
        [ -f "$root/$w" ] || { night_log "PREFLIGHT ⛔ chybí $root/$w"; ok=1; }
    done

    # (5) neběží už jiný sběr/harness.
    #
    # ⛔ TOHLE BYLO NAPSANÉ PŘES `pgrep -f 'diag_f1_cage_advance|…'` A ZABILO
    #    PRVNÍ OSTRÉ SPUŠTĚNÍ (17.08. 09:56). `pgrep -f` porovnává vzor s CELOU
    #    příkazovou řádkou každého procesu, takže sedne i na shell, který ten
    #    vzor jen zmiňuje -- na obalovací `bash -c`, na grep, na tenhle skript.
    #    Falešný poplach v preflightu je horší než žádný: běh se NESPUSTÍ a
    #    přijdeme o noc. (Táž rodina jako `pkill` na vzor, co sedne na sebe.)
    #
    # ⇒ Neptáme se na text, ptáme se na SPUŠTĚNOU BINÁRKU: `/proc/PID/exe`
    #   ukazuje na skutečný soubor, a shell, který o něm mluví, tam nikdy není.
    #   Sběr korpusu je python, ten se pozná podle jména skriptu v argv --
    #   ale jen u procesů, jejichž exe je opravdu python.
    local others="" binreal p exe
    binreal=$(readlink -f "$bin" 2>/dev/null)
    for p in /proc/[0-9]*; do
        local pid=${p#/proc/}
        [ "$pid" = "$$" ] && continue
        exe=$(readlink "$p/exe" 2>/dev/null) || continue
        exe=${exe% (deleted)}
        if [ -n "$binreal" ] && [ "$exe" = "$binreal" ]; then
            others="$others $pid(harness)"; continue
        fi
        # ⛔ Vzor byl `*/python3|*/python` a NECHYTAL `python3.12`, protože
        #    /proc/PID/exe ukazuje na verzovanou binárku. Zjištěno 17.08. při
        #    kontrole zdraví běhu: preflight hlásil 0 procesů sběru, zatímco
        #    osm workerů korpusu běželo. Guard by tedy dovolil spustit noc na
        #    běžící sběr. Druhá chyba téhož druhu za jeden den (po `pgrep -f`)
        #    -- vzor nad jmény je vždycky křehčí, než se zdá.
        case "$exe" in
            */python*)
                if tr '\0' ' ' < "$p/cmdline" 2>/dev/null \
                   | grep -q 'diag_replay_mine_2026'; then
                    others="$others $pid(sběr)"
                fi ;;
        esac
    done
    if [ -n "$others" ]; then
        night_log "PREFLIGHT ⛔ už běží jiný běh:$others"; ok=1
    fi

    # (6) místo na disku -- korpus 3000 her je ~30 MB, ale trénink umí sežrat víc
    local freemb; freemb=$(df -Pm "$root" | awk 'NR==2{print $4}')
    if [ "${freemb:-0}" -lt 2000 ]; then
        night_log "PREFLIGHT ⛔ volno jen ${freemb} MB (chci ≥ 2000)"; ok=1
    fi

    # (7) špinavý strom se nezakazuje, ale ZAPÍŠE SE -- jinak se výsledek připíše
    #     commitu, na kterém neběžel
    if [ -n "$(git -C "$root" status --porcelain -- engine/ 2>/dev/null)" ]; then
        night_log "PREFLIGHT ⚠️ engine/ má NEZACOMMITOVANÉ změny — výsledek NEODPOVÍDÁ HEADu"
        git -C "$root" status --porcelain -- engine/ | sed 's/^/       /' >> "$NIGHT_LOG"
        echo dirty > "$out/ENGINE_DIRTY"
    else
        rm -f "$out/ENGINE_DIRTY"
    fi

    [ "$ok" -eq 0 ] && night_log "PREFLIGHT OK"
    return $ok
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
