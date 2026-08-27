#!/usr/bin/env bash
# ============================================================================
# NOČNÍ A/B — OBECNÝ SPOUŠTĚČ                              (17.08.2026)
#
# Nahrazuje zvyk psát na každou noc nový skript s vlastní kopií zámku. Devět
# takových kopií = devět různých způsobů, jak přijít o noc. Tenhle sourcuje
# `run_night_lib.sh` a přidá dvě věci, které A/B tohoto projektu musí mít:
#
#   ⛔ MATCHUP S NULOVOU EXPOZICÍ JE POVINNÝ.
#      15.08. splnil pre-registrovaný práh „PROŠLO" i matchup, kde se rameno
#      ANI JEDNOU nespustilo (+2,28 pp, +2,3 SE při `cand_daunt = 0` v 6 000
#      hrách). Pár není tatáž hra s jedním přehozeným bitem — jsou to dvě hry
#      na spřízněných seedech. ⇒ Efekt se čte PROTI NULOVÉMU RAMENI, ne proti
#      nule, a bez nuly se běh NESPOUŠTÍ. Viz P20.
#
#   ⛔ PREFLIGHT PŘED 14 HODINAMI, NE PO NICH.
#      Nejhorší selhání není pád, ale noc odběhnutá na starém `libbb_engine.so`.
#
#   ⛔ NOC SI MUSÍ VYTISKNOUT VLASTNÍ VÝSLEDEK  (přidáno 18.08.)
#      Noc 17.→18.08. doběhla bezvadně -- 8/8 shardů, žádný FAIL -- a skončila
#      BEZ VÝSLEDKU: `chain.log` končil `NIGHT DONE`, 6 000 párů leželo jako osm
#      čísel po ±0,019, tedy osm JEDNOTLIVĚ NEPRŮKAZNÝCH výsledků, a sloučenou
#      deltu spočítal ráno člověk. Táž rodina jako audit aparátu: SNÍMEK SE
#      VYDÁVÁ ZA STAV -- a je to přesně krok, kde si unavené čtení vybere shard,
#      který se hodí. ⇒ `night_summarize.py` na konci, v POŘADÍ ČTENÍ
#      z předregistrace ((1) leak → (2) jmenovatel → (3) n_nonzero → (4) delta).
#      Kontrola, kterou nikdo nepřečte, se od chybějící kontroly neliší.
#
# POUŽITÍ
#   MODE=4 PAIRS=750 SHARDS=4 THRESHOLD=0.015 \
#   (volitelně CHUNKS=32 WORKERS=8 -- T2.15: táž práce nakrájená na víc
#    kusů, worker si bere další, až dodělá. NEMĚNÍ počet párů ani sílu.)
#   MATCHUPS="4:dw-orc:1 0:dw-sk:0 3:orc-sk:0" \
#   OUT=ab_wrestle_20260818 ./run_night_ab.sh
#
#   `THRESHOLD` je PRÁH Z PŘEDREGISTRACE (default 0,015). Zapíše se do
#   `chain.log` PŘI STARTU a vyhodnotí se strojově -- 17.→18.08. si harness
#   tiskl vlastní natvrdo zadané `>= +0.03`, zatímco předregistrace na tutéž
#   noc říkala ±0,015. Dva prahy v jedné noci = otevřená branka pro doladění.
#
#   Formát matchupu je `index:jméno:expozice`, kde expozice 1 = tam se rameno
#   spustí (ta otázka), 0 = tam se spustit NEMŮŽE (null). Aspoň jedna nula.
#
#   ⚠️ NULA SE DĚLÁ DVĚMA ZPŮSOBY PODLE TOHO, JAK JE RAMENO VZÁCNÉ (17.08.):
#     · rameno se opírá o dovednost nebo rasu (Dauntless, Wrestle) ⇒ existuje
#       matchup, kde se spustit NEMŮŽE. Ten je nejlepší nula, jakou lze mít:
#       je to zároveň kontrola implementace i podlahy aparátu.
#     · rameno sahá na každé kolo (brána klece) ⇒ TAKOVÝ MATCHUP NEEXISTUJE
#       a trvat na něm by znamenalo buď ho vymyslet, nebo pravidlo obejít.
#       Použij CONTROL_MODE2=1: pustí se krátká noha v mode 2, kde mají obě
#       ramena TOUTÉŽ konfiguraci, takže s CRN musí vyjít delta exaktně 0.
#       Když nevyjde, hlavní výsledek se NEČTE -- nevíme, co jsme měřili.
#
#   Volitelně: CORPUS=1 sebere po A/B korpus 3000 her se zapnutým ramenem,
#   BASELINE=<adresář> ho porovná — a odmítne to, když baseline běžela na jiném
#   commitu enginu (P22: korpus 14.08. byl s baseline 6 commitů rozejitý).
# ============================================================================
set -u
ROOT="$(cd "$(dirname "$0")" && pwd)"
. "$ROOT/run_night_lib.sh"

MODE=${MODE:?nastav MODE (režim harnessu, např. 4 = Dauntless v nabídce)}
MATCHUPS=${MATCHUPS:?nastav MATCHUPS "idx:jméno:expozice ..."}
PAIRS=${PAIRS:-750}
SHARDS=${SHARDS:-4}
OUT="$ROOT/${OUT:?nastav OUT (jméno výstupního adresáře)}"
BIN="$ROOT/diag_f1_cage_advance"
SRC="$ROOT/diag_f1_cage_advance_harness.cpp"
CORPUS=${CORPUS:-0}
CORPUS_GAMES=${CORPUS_GAMES:-3000}
# Nulová kontrola pro VŠUDYPŘÍTOMNÉ rameno: matchup s nulovou expozicí u něj
# neexistuje, tak se nula vyrobí jinak -- mode 2 dá obě ramena TÁŽ konfigurace.
# S CRN pak musí vyjít delty exaktně 0; cokoli jiného je vada aparátu.
CONTROL_MODE2=${CONTROL_MODE2:-0}
CONTROL_PAIRS=${CONTROL_PAIRS:-50}
BASELINE=${BASELINE:-}
# ⛔ PRÁH JE VSTUP BĚHU, NE KONSTANTA VE ZDROJÁKU (18.08.). Harness si tiskne
#   vlastní natvrdo zadané `[pre-reg: >= +0.03]`, zatímco předregistrace noci
#   17.→18.08. říkala ±0,015 — dva různé prahy v jedné noci. Práh se proto
#   předává sem, zapisuje se do chain.log PŘI STARTU a vyhodnocuje se strojově.
THRESHOLD=${THRESHOLD:-0.015}
# ⛔ PŘEDREGISTRACE JAKO VSTUP BĚHU, NE JAKO DOKUMENT VEDLE (18.08.).
#   Noc 17.→18.08. měla šest předpovědí; DVĚ z nich ten běh nemohl zodpovědět
#   (`CORPUS=0`) a nikdo to nezkontroloval PŘED spuštěním. Předá-li se sem
#   soubor předpovědí, spouštěč to chytne za minutu místo za 14 hodin a
#   `night_summarize.py` na konci vytiskne PŘEDPOVĚĎ vs VÝSLEDEK.
PREREG=${PREREG:-}

night_init "$OUT" "ab-mode$MODE"
night_stamp_head "$OUT"

# --- povinná nula -----------------------------------------------------------
nulls=0; total=0
for spec in $MATCHUPS; do
    total=$((total+1))
    [ "${spec##*:}" = "0" ] && nulls=$((nulls+1))
done
if [ "$nulls" -eq 0 ] && [ "$CONTROL_MODE2" = "0" ]; then
    night_log "⛔ ODMÍTÁM SPUSTIT: žádná nulová kontrola."
    night_log "   Bez nuly se efekt nedá odlišit od podlahy aparátu (P20)."
    night_log "   Dvě možnosti podle toho, JAK JE RAMENO VZÁCNÉ:"
    night_log "     · rameno se opírá o dovednost/rasu (Dauntless, Wrestle) ⇒"
    night_log "       přidej matchup, kde se spustit NEMŮŽE: 'idx:jméno:0'"
    night_log "     · rameno sahá na každé kolo (brána klece) ⇒ takový matchup"
    night_log "       NEEXISTUJE. Použij CONTROL_MODE2=1 — pustí se navíc krátká"
    night_log "       noha v mode 2, kde jsou obě ramena TÁŽ konfigurace."
    exit 2
fi
night_log "matchupů $total, z toho nulových $nulls, control_mode2=$CONTROL_MODE2 — OK"
night_log "PRÁH pre-registrován PŘED během: ±$THRESHOLD (párová delta chess)"

# --- předpovědi: umí na ně tenhle běh vůbec odpovědět? -----------------------
if [ -n "$PREREG" ]; then
    if [ ! -f "$PREREG" ]; then
        night_log "⛔ PREREG=$PREREG neexistuje — nespouštím."; exit 5
    fi
    night_log "předregistrace: $PREREG"
    sed 's/^/    /' "$PREREG" >> "$NIGHT_LOG"
    need_corpus=$(grep -c '^[[:space:]]*corpus:' "$PREREG" || true)
    if [ "${need_corpus:-0}" -gt 0 ] && [ "$CORPUS" = "0" ]; then
        night_log "⛔ ODMÍTÁM SPUSTIT: $need_corpus předpověď/i potřebuje korpus, ale CORPUS=0."
        night_log "   Běh by na ně NEUMĚL ODPOVĚDĚT a zjistilo by se to až ráno — přesně to"
        night_log "   se stalo 17.→18.08. (K9a tempo, bloky). Buď CORPUS=1, nebo je z"
        night_log "   předregistrace vyškrtni. Předpověď, na kterou běh neodpoví, tam nepatří."
        exit 6
    fi
fi

night_preflight "$BIN" "$OUT" "$SRC" || {
    night_log "⛔ preflight neprošel — nespouštím. Oprav a pusť znovu."
    exit 3
}

# --- PREFLIGHT (2. část): UMÍ BINÁRKA TU KONTROLU VŮBEC VYTISKNOUT? ----------
# ⚑ `night_preflight` hlídá STÁŘÍ binárky a `libbb_engine.so`. To ale neřekne
#   nic o tom, jestli ta binárka umí `MOVED WITHOUT THE ARM ACTING` -- řádek,
#   na kterém podle předregistrace stojí CELÝ verdikt. Kdyby ho neuměla, noc
#   proběhne normálně, sloučení vypíše ⛔ ... až RÁNO, a 14 hodin je pryč.
#   ⇒ Sonda na 1 páru v CÍLOVÉM režimu, dřív než se sáhne na noc. (Rodina T2.7:
#   stará binárka tiše měří něco jiného -- tady tiše NEMĚŘÍ kontrolu.)
if [ ! -f "$OUT/AB_DONE" ]; then
    probe="$OUT/.probe"; rm -rf "$probe"; mkdir -p "$probe"
    pidx=${MATCHUPS%%:*}
    ( cd "$probe" && "$BIN" "$ROOT" 1 "$pidx" "$MODE" 0 > run.log 2>&1 )
    probe_rc=$?
    # ⛔ 24.08.2026: rozlišit PÁD od CHYBĚJÍCÍHO ŘÁDKU. Do dneška obojí hlásilo
    #   „binárka NETISKNE ...", a fáze B víkendu 22.08. na to umřela: skutečná
    #   příčina byl SEGFAULT (rc=139) z ABI nesouladu, ne chybějící řádek.
    #   Hláška poslala hledat vzor pro režim, zatímco šlo o starý harness.
    if [ "$probe_rc" -ne 0 ]; then
        night_log "⛔ PREFLIGHT sonda SPADLA (rc=$probe_rc), mode $MODE, matchup $pidx."
        if [ "$probe_rc" -ge 128 ]; then
            night_log "   rc >= 128 ⇒ signál $((probe_rc-128)). 139 = SEGFAULT, a to skoro"
            night_log "   vždycky znamená ABI: harness je přeložený proti starým hlavičkám,"
            night_log "   zatímco libbb_engine.so je novější. PŘELOŽ HARNESS."
        fi
        night_log "   Viz $probe/run.log. NESPOUŠTÍM."
        exit 4
    fi
    if grep -q "MOVED WITHOUT THE ARM ACTING" "$probe/run.log"; then
        night_log "PREFLIGHT sonda OK — binárka běží a tiskne per-pair kontrolu ramene"
        rm -rf "$probe"
    else
        night_log "⛔ PREFLIGHT sonda: běh prošel, ale NETISKNE 'MOVED WITHOUT THE ARM ACTING'"
        night_log "   (mode $MODE, matchup $pidx) ⇒ tenhle režim nemá signál ramene."
        night_log "   Verdikt by neměl na čem stát. Doplň mode $MODE do armSignalAvailable"
        night_log "   v diag_f1_cage_advance_harness.cpp. Viz $probe/run.log. NESPOUŠTÍM."
        exit 4
    fi
fi

# --- A/B --------------------------------------------------------------------
if [ -f "$OUT/AB_DONE" ]; then
    night_log "A/B už hotové, přeskakuji"
else
    # ⭐ T2.15 (20.08.2026): FRONTA ÚLOH MÍSTO PEVNÉHO DĚLENÍ.
    #
    # ⚑ PROČ. Shard dostával pevný blok seedů, jenže zápasy nejsou stejně
    #   dlouhé => shardy se rozejdou a běh čeká na nejpomalejšího. Změřeno:
    #   17.08. rozptyl 2,8 h, noc 19.->20.08. rovné 4 h (22:59 vs 02:53).
    #   Polovina jader poslední hodiny stojí naprázdno a odhad konce se řídí
    #   nejpomalejším shardem, ne průměrem.
    #
    # ⛔ NEMĚNÍ TO N (uživatel 20.08.: "jestli by noc zkrátit mělo za následek
    #   spadnutí výsledku do šumu, jsem proti zkrácení noci"). Párů na matchup
    #   zůstává SHARDS*PAIRS; mění se JEN to, na kolik kusů se ta práce
    #   nakrájí a jak se rozdává. SE ani síla se nehnou. Ubírají se PROSTOJE.
    #
    # ⭐ CRN ZŮSTÁVÁ. Seed se v harnessu odvozuje z `off + index páru`, tedy
    #   z GLOBÁLNÍHO indexu, ne z pořadí zpracování. Úloha nese svůj `off`
    #   s sebou, takže je jedno, který worker si ji kdy vezme -- pár k páru
    #   sedne stejně jako dřív. Kdyby se `off` počítal z pořadí, párování by
    #   se rozbilo a nulové rameno by přestalo dávat exaktní nulu.
    #
    # CHUNKS = na kolik kusů krájíme (default SHARDS => chování jako dřív)
    # WORKERS = kolik jich běží naráz (default SHARDS)
    CHUNKS=${CHUNKS:-$SHARDS}
    WORKERS=${WORKERS:-$SHARDS}
    TOTAL_PAIRS=$(( SHARDS * PAIRS ))
    if [ $(( TOTAL_PAIRS % CHUNKS )) -ne 0 ]; then
        night_log "⛔ ODMÍTÁM: $TOTAL_PAIRS párů se nedá beze zbytku rozdělit na $CHUNKS kusů."
        night_log "   Zbylo by $(( TOTAL_PAIRS % CHUNKS )) párů — tiše by ZMIZELY, tedy změna N."
        night_log "   Nestejně velké kusy navíc rozbijí sdruženou SE (night_summarize váží shardy stejně)."
        # Odmítnutí, které neporadí, se obchází místo opravy. Nabídneme dělitele.
        cands=""
        for c in $(seq 1 $(( TOTAL_PAIRS < 200 ? TOTAL_PAIRS : 200 ))); do
            [ $(( TOTAL_PAIRS % c )) -eq 0 ] && [ "$c" -ge "$WORKERS" ] && cands="$cands $c"
        done
        night_log "   Použitelné CHUNKS (dělitelé $TOTAL_PAIRS, >= WORKERS=$WORKERS):${cands:- žádný do 200}"
        exit 7
    fi
    CHUNK_PAIRS=$(( TOTAL_PAIRS / CHUNKS ))
    night_log "START A/B mode $MODE, $TOTAL_PAIRS párů na matchup = ${CHUNKS}×${CHUNK_PAIRS}, ${WORKERS} naráz"

    run_one() {  # $1=index  $2=jméno  $3=kus  $4=offset  $5=párů
        local d="$OUT/$2_s$3"
        mkdir -p "$d"; rm -f "$d/OK" "$d/FAIL"
        if ( cd "$d" && nice -n 19 "$BIN" "$ROOT" "$5" "$1" "$MODE" "$4" > run.log 2>&1 ); then
            touch "$d/OK";  night_log "done $2 shard $3 (off $4, $5 párů)"
        else
            touch "$d/FAIL"; night_log "FAIL $2 shard $3 — viz $d/run.log"
        fi
    }

    # Fronta: worker si bere DALŠÍ volný index, až dodělá. Index se přiděluje
    # atomicky přes `mkdir` (mkdir buď uspěje, nebo ne -- žádný závod).
    QDIR="$OUT/.queue"; rm -rf "$QDIR"; mkdir -p "$QDIR"
    worker() {  # $1=index matchupu  $2=jméno
        local k
        for k in $(seq 0 $((CHUNKS - 1))); do
            mkdir "$QDIR/$2_$k" 2>/dev/null || continue   # už si ho vzal jiný
            run_one "$1" "$2" "$k" "$(( k * CHUNK_PAIRS ))" "$CHUNK_PAIRS"
        done
    }
    for spec in $MATCHUPS; do
        idx=${spec%%:*}; rest=${spec#*:}; name=${rest%%:*}
        w=0; while [ "$w" -lt "$WORKERS" ]; do
            night_run_bg worker "$idx" "$name"; w=$((w + 1))
        done
    done
    night_wait
    rm -rf "$QDIR"
    EXPECT=$(( total * CHUNKS ))
    OKS=$(find "$OUT" -name OK | wc -l); FAILED=$(find "$OUT" -name FAIL | wc -l)
    if [ "$FAILED" -eq 0 ] && [ "$OKS" -eq "$EXPECT" ]; then
        night_log "A/B DONE ($OKS/$EXPECT)"; touch "$OUT/AB_DONE"
    else
        night_log "A/B PARTIAL: ok=$OKS fail=$FAILED"; touch "$OUT/AB_PARTIAL"; exit 1
    fi
fi

# --- mode 2: SMOKE TEST SEEDOVÁNÍ, ne kontrola ramene ------------------------
# ⚠️ Pod CRN je tohle TAUTOLOGIE: obě orientace hrají doslova tutéž hru, takže
# chessCandAway = 1 - chessCandHome algebraicky a delta MUSÍ být 0, ať je na
# rameni cokoli špatně. Chytí to jedinou věc -- hrubou chybu v seedování -- a
# to za pár minut, takže se to vyplatí. Ale VERDIKT NA TOM NESTOJÍ.
# Skutečná kontrola je per-pair "MOVED WITHOUT THE ARM ACTING" ze SUMMARY.
if [ "$CONTROL_MODE2" != "0" ]; then
    cidx=${MATCHUPS%%:*}
    d="$OUT/control_mode2"
    if [ -f "$d/OK" ]; then
        night_log "kontrola mode 2 už hotová, přeskakuji"
    else
        mkdir -p "$d"; rm -f "$d/OK" "$d/FAIL"
        night_log "START smoke test seedování: mode 2, $CONTROL_PAIRS párů, matchup $cidx"
        ( cd "$d" && nice -n 19 "$BIN" "$ROOT" "$CONTROL_PAIRS" "$cidx" 2 0 > run.log 2>&1 ) \
            && touch "$d/OK" || touch "$d/FAIL"
    fi
    # ⚠️ 27.08.: harness ten řádek přejmenoval z „NULL-TEST" na „SEED-CHECK",
    # protože tvrdil, že je to šumové dno, a není (viz níže). Grep se musel
    # posunout s ním -- jinak by kontrola tiše nevrátila nic a noc by si
    # označila CONTROL_FAILED. Starý název se hledá taky, aby šly číst i logy
    # z běhů před 27.08.
    cline=$(grep -hE "SEED-CHECK|NULL-TEST" "$d/run.log" 2>/dev/null | head -1)
    night_log "  ${cline:-(kontrola nic nevrátila)}"
    if echo "$cline" | grep -q "+0.0000 +- 0.0000"; then
        night_log "  ✅ seedování drží. ⛔ A NIC VÍC: delta je tu 0 Z DEFINICE"
        night_log "     (obě řádky jsou TÁŽ hra čtená z obou stran), takže to"
        night_log "     NENÍ šumové dno a nesmí se tak citovat."
    else
        night_log "  ⛔ SMOKE TEST NEVYŠEL NULOVÝ. Obě ramena tam mají TOUTÉŽ konfiguraci"
        night_log "     a přesto se liší ⇒ ROZBITÉ SEEDOVÁNÍ. Hlavní výsledek NEČÍST."
        touch "$OUT/CONTROL_FAILED"
    fi
fi

# --- VÝSLEDEK NOCI ----------------------------------------------------------
# ⛔ TENHLE BLOK NAHRAZUJE „ARM" ŘÁDEK, KTERÝ 18.08. LHAL.
#   Grepoval `^  ARM `, což je řádek POUZE pro mode 4 (Dauntless). V mode 0
#   neexistuje ⇒ do chain.log se vytiskl fallback „(harness nic netiskl — stará
#   binárka?)“ přesně nad testem, který proběhl a byl čistý 8/8. Navíc `head -1`
#   četl jen shard 0, takže leak v shardu 5 by neprobublal.
#
# ⛔ A DRUHÁ, HORŠÍ VADA: noc končila `NIGHT DONE` BEZ VÝSLEDKU. 6 000 párů
#   leželo jako osm čísel po ±0,019 -- osm JEDNOTLIVĚ NEPRŮKAZNÝCH výsledků --
#   a sloučenou deltu musel ráno spočítat člověk. Snímek se vydával za stav.
#   ⇒ Noc si od 18.08. tiskne vlastní výsledek, v POŘADÍ ČTENÍ z předregistrace.
night_log "--- VÝSLEDEK (pořadí je pořadí čtení z předregistrace) ---"
names=""
for spec in $MATCHUPS; do rest=${spec#*:}; names="$names ${rest%%:*}"; done
if THRESHOLD="$THRESHOLD" PREREG="$PREREG" python3 "$ROOT/night_summarize.py" "$OUT" $names >> "$NIGHT_LOG" 2>&1; then
    :
else
    night_log "⚠️ sloučení skončilo nenulově — přečti výpis výš, verdikt NEVYNÁŠEJ z jednoho shardu"
fi

# --- korpus (volitelně) -----------------------------------------------------
if [ "$CORPUS" != "0" ]; then
    CORPUS_DATA="$ROOT/${OUT##*/}_corpus_data"
    if [ -f "$CORPUS_DATA/COLLECT_DONE" ]; then
        night_log "korpus už existuje, přeskakuji"
    else
        night_log "START korpus $CORPUS_GAMES her se zapnutým ramenem"
        if CAGE_GATE=0 DAUNTLESS=1 DATA_ROOT="$CORPUS_DATA" SEED_BASE=20260900 \
                nice -n 19 python3 diag_replay_mine_20260813_gate.py collect "$CORPUS_GAMES" \
                > "$OUT/collect.log" 2>&1; then
            night_log "korpus HOTOV"
        else
            night_log "FAIL korpus — viz $OUT/collect.log"; exit 1
        fi
    fi
    # ⭐ 25.08.: KONTROLA INVARIANTŮ na PRVNÍM korpusu po pravidlovém kole.
    # Předregistrace 24.08. si ji vyžádala a nemohla ji dostat (ten běh byl
    # CORPUS=0). Běží PRVNÍ ze tří rozborů: rozbitý invariant není odchylka od
    # pravidel, je to stav, se kterým se dál počítá, takže znehodnocuje i drives
    # a checks pod ním. Nezastavuje běh -- data už jsou sebraná a je lepší je mít
    # i s poplachem než je zahodit.
    nice -n 19 python3 diag_state_invariants_20260824.py "$CORPUS_DATA" \
        > "$OUT/invariants.txt" 2>&1
    if grep -q "ŽÁDNÝ ROZBITÝ INVARIANT" "$OUT/invariants.txt"; then
        night_log "invarianty ✅ ČISTO ($(grep -m1 '^her:' "$OUT/invariants.txt"))"
    else
        night_log "⛔⛔ INVARIANTY: NÁLEZ — přečti $OUT/invariants.txt PŘED čímkoli jiným"
        head -12 "$OUT/invariants.txt" | while IFS= read -r l; do night_log "    $l"; done
    fi
    nice -n 19 python3 diag_drive_failure_20260811.py "$CORPUS_DATA" > "$OUT/drives.txt" 2>&1
    nice -n 19 python3 diag_rules_checks_20260812.py "$CORPUS_DATA/*.json.gz" > "$OUT/checks.txt" 2>&1
    if [ -n "$BASELINE" ]; then
        if night_check_baseline "$OUT" "$ROOT/$BASELINE"; then
            night_log "korpus SE SMÍ srovnat s $BASELINE"
        else
            night_log "⛔ korpus se s $BASELINE srovnat NESMÍ — rozdíl NENÍ efekt ramene"
        fi
    fi
fi

night_log "NIGHT DONE"
touch "$OUT/NIGHT_DONE"
