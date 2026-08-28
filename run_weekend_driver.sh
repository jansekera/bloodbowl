#!/bin/bash
# ============================================================================
# ŘIDIČ VÍKENDU — cyklus  build → testy → harness → běh → log → CLEAR
#
# Návrh uživatele 25.08.: „a co takhle na ten víkend naplánovat build test log
# clear build test log clear?" Nahrazuje dřívější nápad na ZŘETĚZENÍ nad jedním
# zmrazeným enginem, a to z konkrétního důvodu: každý cyklus staví znovu, takže
# engine NENÍ zmrazený 62 h a mezi běhy se SMÍ opravovat.
#
# ⚠️ CO TO NEDÁ: efekt KOMBINACE. Dvě ramena téže mechaniky (P35 × M1/N10 --
#   obojí blitz) dají každé svou deltu, ale ne to, co bychom nasazovali. A mizí
#   porovnatelnost MEZI cykly (každý na jiném enginu) -- párové A/B uvnitř
#   cyklu ale platí. Viz T2.20 a PLAN-WKND ve frontě.
#
# ⛔ ČTYŘI PASTI, KVŮLI KTERÝM TENHLE SKRIPT VŮBEC EXISTUJE:
#   (d1) run_night_ab.sh přeskočí běh, když v OUT najde AB_DONE/NIGHT_DONE.
#        ⇒ zopakovaný OUT = cyklus TIŠE NEUDĚLÁ NIC a do logu napíše „hotovo".
#           Řidič proto na existující OUT odmítne cyklus spustit.
#   (d2) harness se linkuje proti .so za běhu ⇒ MUSÍ se překládat AŽ PO buildu
#        enginu. Opačné pořadí = SEGFAULT na starých offsetech struct Player
#        (zabilo fázi B, commit 32f93c8f). Pořadí tady je pevné, ne volitelné.
#   (d3) červené testy nesmí zabít víkend: cyklus se PŘESKOČÍ, hlasitě zaloguje
#        a jede se dál. `exit 1` ve 03:00 v neděli by odstavil 40 hodin.
#   (d4) každý cyklus staví kód s rameny DEFAULT OFF. Hlídá to sada testů --
#        nulové testy typu WithTheArmOffTheBlitzBehavesExactlyAsBefore. Když
#        jsou testy zelené, ramena jsou vypnutá; proto se (d3) NEobchází.
#
# ⭐ ROZHODUJE SE PODLE NÁVRATOVÝCH KÓDŮ, NIKDY GREPEM VÝSTUPU.
#   25.08. mi build testů selhal třikrát po sobě a nevšiml jsem si, protože
#   jsem filtroval `error`, zatímco make hlásí `Error 1`.
#
# ⭐ A ŘIDIČ SE SÁM ZKOPÍRUJE MIMO REPOZITÁŘ.
#   Cykly přepínají git ref ⇒ soubor skriptu by se pod běžícím bashem přepsal
#   a bash si běžící skript dočítá. Táž třída jako „run_night_ab.sh se za běhu
#   needituje", jen zákeřnější, protože ji způsobí sám řidič.
#
# POUŽITÍ:
#   ./run_weekend_driver.sh MANIFEST [--dry-run]
#
# MANIFEST -- jeden cyklus na řádek, sloupce oddělené `|`, `#` je komentář:
#   jmeno | git-ref | mode | paru | prereg-cesta | corpus(0/1) | matchup
# Například:
#   p35    | main                      | 8 | 4800 | evidence/night_prereg_20260825.preds | 0 | 1:dw-we:1
#   b2     | b2-wrestle-pricing        |11 | 4800 | evidence/night_prereg_20260829_b2.preds | 0 | 2:dw-dw:1
#
# ⛔⛔ SLOUPEC `matchup` PŘIBYL 28.08.2026 A JE TO OPRAVA TICHÉ VADY.
#   Do té doby driver posílal do run_night_ab.sh natvrdo MATCHUPS="1:dw-we:1",
#   a manifest o tom NIC neříkal -- žádný sloupec, žádný řádek v logu. Cyklus
#   napsaný pro jiný matchup by tedy proběhl na dw-we, skončil rc=0 a vypadal
#   NORMÁLNĚ.
#   Změřeno týž den na mode 11 (B2), 12 párů na binárku:
#     dw-dw  9,83 repicku/hru, n_nonzero 1/12
#     dw-we  0,62 repicku/hru, n_nonzero 0/12   <= SLEPÝ VZOREK
#   ⇒ B2 puštěné bez tohohle sloupce by dalo deltu ~0 a četlo by se jako
#     „rameno nepomáhá". To není slabý efekt, to je jiná otázka.
#   Chybí-li sloupec, driver cyklus ODMÍTNE. Mlčky doplněná výchozí hodnota
#   je přesně ta vada, která se tu opravuje.
# ============================================================================
set -u
ROOT=/home/jan/claude/bloodbowl
SELF_REAL=$(readlink -f "$0")

# --- (0) re-exec z kopie mimo repozitář (viz hlavička) -----------------------
case "$SELF_REAL" in
  "$ROOT"/*)
    TMPSELF=$(mktemp /tmp/weekend_driver.XXXXXX.sh) || exit 1
    cp "$SELF_REAL" "$TMPSELF"; chmod +x "$TMPSELF"
    echo "[driver] běžím z kopie $TMPSELF (git ref se bude přepínat)"
    exec "$TMPSELF" "$@"
    ;;
esac

MANIFEST=${1:?zadej manifest}
DRY=${2:-}
STAMP=$(date -u '+%Y%m%d_%H%M')
LOG=$ROOT/weekend_driver_$STAMP.log

say() { echo "[$(date -u '+%m-%d %H:%M')] $*" | tee -a "$LOG"; }

# --- kontrola: neběží už jiný harness? --------------------------------------
# ⛔ argv[0], NE podřetězec celého cmdline: 25.08. si takový test třikrát
#    namatchoval SÁM SEBE a jednou kvůli tomu odmítl spustit noc.
harness_running() {
    local me=$$ pid a0
    for p in /proc/[0-9]*; do
        pid=${p#/proc/}; [ "$pid" = "$me" ] && continue
        a0=$(tr '\0' '\n' < "$p/cmdline" 2>/dev/null | head -1)
        case "$a0" in */diag_f1_cage_advance) return 0;; esac
    done
    return 1
}

say "=== ŘIDIČ VÍKENDU START (manifest $MANIFEST${DRY:+, $DRY}) ==="
cd "$ROOT" || exit 1

# --dry-run jen ověřuje manifest a nic nespouští, takže běžící harness mu
# nevadí -- a je to jediný režim, ve kterém se řidič smí pustit za noci.
if [ -z "$DRY" ] && harness_running; then say "⛔ už běží harness — NESPOUŠTÍM"; exit 1; fi

ok=0; failed=0; skipped=0
while IFS='|' read -r name ref mode pairs prereg corpus matchup; do
    name=$(echo "$name" | xargs); [ -z "$name" ] && continue
    case "$name" in \#*) continue;; esac
    ref=$(echo "$ref" | xargs); mode=$(echo "$mode" | xargs)
    pairs=$(echo "$pairs" | xargs); prereg=$(echo "$prereg" | xargs)
    corpus=$(echo "${corpus:-0}" | xargs)
    matchup=$(echo "${matchup:-}" | xargs)
    OUT="wknd_${name}_${STAMP}"

    say ""
    say "--- CYKLUS '$name': ref=$ref mode=$mode párů=$pairs korpus=$corpus matchup=${matchup:-CHYBÍ}"

    # (d5) MATCHUP SE NEDOPLŇUJE MLČKY. Viz hlavička: do 28.08. tu byla
    # natvrdo dw-we a manifest o tom neměl sloupec, takže cyklus napsaný pro
    # jiný matchup doběhl rc=0 na slepém vzorku. Výchozí hodnota by tu vadu
    # jen přemalovala.
    if [ -z "$matchup" ]; then
        say "⛔ '$name' PŘESKOČEN: chybí sloupec matchup (tvar 'idx:jméno:expozice', např. 2:dw-dw:1)"
        skipped=$((skipped+1)); continue
    fi
    case "$matchup" in
        *:*:*) ;;
        *) say "⛔ '$name' PŘESKOČEN: matchup '$matchup' nemá tvar 'idx:jméno:expozice'"
           skipped=$((skipped+1)); continue;;
    esac

    # (d1) CLEAR je PODMÍNKA, ne úklid: existující OUT by běh tiše přeskočil
    if [ -e "$OUT" ]; then
        say "⛔ '$name' PŘESKOČEN: $OUT už existuje ⇒ běh by se tiše přeskočil (AB_DONE)"
        skipped=$((skipped+1)); continue
    fi
    # ⚠️ Předregistrace se ověřuje V TOM REFU, který cyklus postaví, ne
    # v aktuálním checkoutu -- 25.08. to dry-run odhalil hned na prvním pokusu:
    # prereg pro M1/N10 žije na větvi, řidič ji hledal na main a cyklus zahodil.
    # `git cat-file -e` to umí bez checkoutu, takže to funguje i v --dry-run.
    if ! git cat-file -e "$ref:$prereg" 2>/dev/null; then
        say "⛔ '$name' PŘESKOČEN: v refu '$ref' není $prereg — bez předregistrace se neměří"
        skipped=$((skipped+1)); continue
    fi
    if ! git rev-parse -q --verify "$ref^{commit}" >/dev/null; then
        say "⛔ '$name' PŘESKOČEN: ref '$ref' neexistuje"
        skipped=$((skipped+1)); continue
    fi

    if [ -n "$DRY" ]; then say "   (dry-run: cyklus by běžel)"; ok=$((ok+1)); continue; fi

    # --- checkout ------------------------------------------------------------
    if ! git checkout -q "$ref" 2>>"$LOG"; then
        say "⛔ '$name' PŘESKOČEN: checkout '$ref' selhal"; failed=$((failed+1)); continue
    fi
    say "    HEAD $(git rev-parse --short HEAD)"

    # --- (d2) POŘADÍ JE PEVNÉ: build enginu → testy → AŽ POTOM harness -------
    if ! cmake --build engine/build -j"$(nproc)" >>"$LOG" 2>&1; then
        say "⛔ '$name' PŘESKOČEN: BUILD ENGINU SELHAL"; failed=$((failed+1)); continue
    fi
    if ! ( cd engine/build && ./bb_tests ) >>"$LOG" 2>&1; then
        say "⛔ '$name' PŘESKOČEN: TESTY ČERVENÉ — na červené sadě se neměří"
        say "    (d4: nulové testy hlídají i to, že ramena jsou default OFF)"
        failed=$((failed+1)); continue
    fi
    say "    testy OK"
    if ! g++ -O2 -std=c++20 -Iengine/include -Iengine/third_party \
            diag_f1_cage_advance_harness.cpp \
            -Lengine/build -lbb_engine -Wl,-rpath,"$ROOT/engine/build" \
            -o diag_f1_cage_advance >>"$LOG" 2>&1; then
        say "⛔ '$name' PŘESKOČEN: překlad harnessu selhal"; failed=$((failed+1)); continue
    fi
    say "    harness přeložen PO .so (d2)"

    # --- běh -----------------------------------------------------------------
    MODE="$mode" PAIRS=$((pairs/8)) SHARDS=8 THRESHOLD=0.015 \
    CHUNKS=40 WORKERS=8 CONTROL_MODE2=1 CONTROL_PAIRS=50 \
    CORPUS="$corpus" CORPUS_GAMES=3000 \
    MATCHUPS="$matchup" PREREG="$ROOT/$prereg" OUT="$OUT" \
        ./run_night_ab.sh >>"$LOG" 2>&1
    rc=$?
    if [ $rc -eq 0 ]; then say "✅ '$name' HOTOV (rc=0), výsledek v $OUT/chain.log"; ok=$((ok+1))
    else say "⚠️ '$name' skončil rc=$rc — přečti $OUT/chain.log, POKRAČUJI DÁL"; failed=$((failed+1)); fi
done < "$MANIFEST"

say ""
say "=== ŘIDIČ KONEC: hotových $ok · selhalo/přeskočeno $((failed+skipped)) ==="
say "⚠️ Deltu z různých cyklů NEPOROVNÁVAT — každý běžel na jiném enginu."
