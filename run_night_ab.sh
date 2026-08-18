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
#      z předregistrace (① leak → ② jmenovatel → ③ n_nonzero → ④ delta).
#      Kontrola, kterou nikdo nepřečte, se od chybějící kontroly neliší.
#
# POUŽITÍ
#   MODE=4 PAIRS=750 SHARDS=4 THRESHOLD=0.015 \
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
    if ( cd "$probe" && "$BIN" "$ROOT" 1 "$pidx" "$MODE" 0 > run.log 2>&1 ) \
       && grep -q "MOVED WITHOUT THE ARM ACTING" "$probe/run.log"; then
        night_log "PREFLIGHT sonda OK — binárka tiskne per-pair kontrolu ramene"
        rm -rf "$probe"
    else
        night_log "⛔ PREFLIGHT sonda: binárka NETISKNE 'MOVED WITHOUT THE ARM ACTING'"
        night_log "   (mode $MODE, matchup $pidx). Verdikt by neměl na čem stát ⇒ NESPOUŠTÍM."
        night_log "   Viz $probe/run.log. Přelož harness a pusť znovu."
        exit 4
    fi
fi

# --- A/B --------------------------------------------------------------------
if [ -f "$OUT/AB_DONE" ]; then
    night_log "A/B už hotové, přeskakuji"
else
    night_log "START A/B mode $MODE, ${SHARDS}×${PAIRS} párů na matchup"
    run_one() {  # $1=index  $2=jméno  $3=shard
        local off=$(( $3 * PAIRS )) d="$OUT/$2_s$3"
        mkdir -p "$d"; rm -f "$d/OK" "$d/FAIL"
        if ( cd "$d" && nice -n 19 "$BIN" "$ROOT" "$PAIRS" "$1" "$MODE" "$off" > run.log 2>&1 ); then
            touch "$d/OK";  night_log "done $2 shard $3"
        else
            touch "$d/FAIL"; night_log "FAIL $2 shard $3 — viz $d/run.log"
        fi
    }
    for spec in $MATCHUPS; do
        idx=${spec%%:*}; rest=${spec#*:}; name=${rest%%:*}
        for s in $(seq 0 $((SHARDS - 1))); do night_run_bg run_one "$idx" "$name" "$s"; done
    done
    night_wait
    EXPECT=$(( total * SHARDS ))
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
    cline=$(grep -h "NULL-TEST" "$d/run.log" 2>/dev/null | head -1)
    night_log "  ${cline:-(kontrola nic nevrátila)}"
    if echo "$cline" | grep -q "+0.0000 +- 0.0000"; then
        night_log "  ✅ seedování v pořádku (nic víc to netvrdí — viz MOVED WITHOUT ARM)"
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
        night_log "START korpus 3000 her se zapnutým ramenem"
        if CAGE_GATE=0 DAUNTLESS=1 DATA_ROOT="$CORPUS_DATA" SEED_BASE=20260900 \
                nice -n 19 python3 diag_replay_mine_20260813_gate.py collect 3000 \
                > "$OUT/collect.log" 2>&1; then
            night_log "korpus HOTOV"
        else
            night_log "FAIL korpus — viz $OUT/collect.log"; exit 1
        fi
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
