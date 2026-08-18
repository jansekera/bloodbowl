#!/usr/bin/env bash
# ============================================================================
# SEBETEST `run_night_lib.sh`                             (17.08.2026)
#
# Knihovna, která hlídá 14hodinové běhy, nesmí být neověřená — to je horší než
# žádná pojistka, protože se na ni spoléhá. Každý test odpovídá jedné vadě
# z noci 14.08. (viz hlavička libu a `evidence/weekend_result_20260817.md`).
#
#   ./test_night_lib.sh          # vypíše PASS/FAIL, návratový kód = počet FAILů
# ============================================================================
set -u
ROOT="$(cd "$(dirname "$0")" && pwd)"
LIB="$ROOT/run_night_lib.sh"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
FAILS=0
RUNS=0

ok()   { RUNS=$((RUNS+1)); printf "  \033[32mPASS\033[0m %s\n" "$1"; }
bad()  { RUNS=$((RUNS+1)); FAILS=$((FAILS+1)); printf "  \033[31mFAIL\033[0m %s\n" "$1"; }
check(){ if [ "$2" = "$3" ]; then ok "$1"; else bad "$1 (čekáno '$3', vyšlo '$2')"; fi; }

echo "== 1. zámek =="

# 1a: zámek po MRTVÉM procesu se sebere (vada: kill -9 nechá zámek napořád)
mkdir -p "$TMP/a/.lock"; echo 999999 > "$TMP/a/.lock/pid"   # pid, který neběží
out=$( . "$LIB"; NIGHT_LOG="$TMP/a/chain.log"; night_lock "$TMP/a" && echo GOT || echo BLOCKED )
check "starý zámek po mrtvém pid se sebere" "$out" "GOT"
grep -q "beru si ho" "$TMP/a/chain.log" \
    && ok "sebrání zámku se ZALOGUJE" || bad "sebrání zámku se nezalogovalo"

# 1b: zámek ŽIVÉHO procesu se NESMÍ sebrat (jinak by dvě spuštění jela naráz)
mkdir -p "$TMP/b/.lock"
sleep 30 & LIVE=$!; echo $LIVE > "$TMP/b/.lock/pid"
out=$( . "$LIB"; NIGHT_LOG="$TMP/b/chain.log"; night_lock "$TMP/b" && echo GOT || echo BLOCKED )
check "zámek živého procesu drží" "$out" "BLOCKED"
kill $LIVE 2>/dev/null; wait $LIVE 2>/dev/null

# 1c: zámek bez souboru pid (poškozený) se taky sebere, ne zablokuje navždy
mkdir -p "$TMP/c/.lock"
out=$( . "$LIB"; NIGHT_LOG="$TMP/c/chain.log"; night_lock "$TMP/c" && echo GOT || echo BLOCKED )
check "poškozený zámek bez pid se sebere" "$out" "GOT"

echo "== 2. úklid dětí =="

# Vada 14.08.: zabití rodiče nechá 12 harness procesů běžet dál a psát do
# adresářů, do kterých začne psát druhé spuštění.
cat > "$TMP/parent.sh" <<EOF
#!/usr/bin/env bash
set -u
. "$LIB"
night_init "$TMP/kids" "test-kids"
night_run_bg sleep 300
night_run_bg sleep 300
echo \$NIGHT_KIDS > "$TMP/kids/pids"
night_wait
EOF
chmod +x "$TMP/parent.sh"
"$TMP/parent.sh" & PARENT=$!
for _ in $(seq 1 50); do [ -s "$TMP/kids/pids" ] && break; sleep 0.1; done
KIDS=$(cat "$TMP/kids/pids" 2>/dev/null)
alive=0; for p in $KIDS; do kill -0 "$p" 2>/dev/null && alive=$((alive+1)); done
check "děti běží, než se sáhne na rodiče" "$alive" "2"

kill -TERM $PARENT 2>/dev/null; sleep 3
alive=0; for p in $KIDS; do kill -0 "$p" 2>/dev/null && alive=$((alive+1)); done
check "po zabití rodiče nezůstal ŽÁDNÝ sirotek" "$alive" "0"
wait $PARENT 2>/dev/null
[ -d "$TMP/kids/.lock" ] && bad "zámek zůstal po úklidu" || ok "zámek se po úklidu uvolnil"

echo "== 3. záznam pokusů =="

# Vada 14.08.: chain.log přišel o první spuštění a noc vypadala jako jedno
# čisté. Druhý pokus musí být ze souboru vidět.
( . "$LIB"; night_init "$TMP/att" "test" >/dev/null 2>&1 )
( . "$LIB"; night_init "$TMP/att" "test" >/dev/null 2>&1 )
n=$(grep -c "POKUS " "$TMP/att/chain.log")
check "druhé spuštění je v logu jako POKUS 2" "$n" "2"
grep -q "POKUS 2" "$TMP/att/chain.log" && ok "pokusy se číslují" || bad "pokusy se nečíslují"

echo "== 4. preflight: stará binárka =="

# Nejnebezpečnější selhání: 14 hodin na starém kódu. Nic to nehlásí.
mkdir -p "$TMP/pf"
touch "$TMP/pf/bin"; chmod +x "$TMP/pf/bin"
sleep 1.1
touch "$TMP/pf/src.cpp"          # zdroj NOVĚJŠÍ než binárka
out=$( . "$LIB"; NIGHT_LOG="$TMP/pf/chain.log"
       night_preflight "$TMP/pf/bin" "$TMP/pf" "$TMP/pf/src.cpp" >/dev/null 2>&1 \
       && echo PASSED || echo BLOCKED )
check "zdroj novější než binárka běh ZASTAVÍ" "$out" "BLOCKED"
grep -q "PŘELOŽIT" "$TMP/pf/chain.log" && ok "preflight řekne CO udělat" || bad "preflight neřekne co"

echo "== 4b. preflight: souběžný běh =="

# Vada 17.08. 09:56: `pgrep -f` sedla na SHELL, který vzor jen zmiňoval, a
# preflight zabil první ostré spuštění. Falešný poplach tu stojí celou noc.
mkdir -p "$TMP/conc"
cp /bin/sleep "$TMP/conc/fakebin"                # „harness"
touch "$TMP/conc/fakebin.cpp"; sleep 0.1; touch "$TMP/conc/fakebin"
mkdir -p "$TMP/conc/engine/build" "$TMP/conc/engine/src" "$TMP/conc/engine/include"
touch "$TMP/conc/engine/build/libbb_engine.so"
: > "$TMP/conc/weights_best.json"; : > "$TMP/conc/weights_policy.json"
mkdir -p "$TMP/conc/out"

# (a) shell, který jméno binárky jen ZMIŇUJE, nesmí být nahlášen.
#     ⚠️ Netvrdíme "celý preflight projde": na stroji může legitimně běžet
#     ostrý sběr nebo A/B, a pak preflight ZASTAVIT MÁ. Testujeme přesně to,
#     co má tenhle případ ověřit -- že se ten shell neobjeví v hlášení.
#     (Původní verze tvrdila PASSED a procházela jen proto, že vzor
#     `*/python3` nechytal python3.12, takže neviděla ani reálný sběr.)
bash -c 'x="fakebin diag_f1_cage_advance diag_replay_mine_2026"; sleep 20' & MENTION=$!
sleep 0.3
: > "$TMP/conc/out/chain.log"
( . "$LIB"; NIGHT_LOG="$TMP/conc/out/chain.log"
  night_preflight "$TMP/conc/fakebin" "$TMP/conc/out" "$TMP/conc/fakebin.cpp" ) >/dev/null 2>&1
if grep -q "už běží jiný běh.*$MENTION" "$TMP/conc/out/chain.log"; then
    bad "shell, co vzor jen zmiňuje, se NEMÁ hlásit (pid $MENTION nahlášen)"
else
    ok "shell, co vzor jen zmiňuje, se nehlásí"
fi
kill $MENTION 2>/dev/null; wait $MENTION 2>/dev/null

# (b) ale SKUTEČNĚ běžící binárka ho zastavit MUSÍ
"$TMP/conc/fakebin" 25 & REAL=$!
sleep 0.3
out=$( . "$LIB"; NIGHT_LOG="$TMP/conc/out/chain.log"
       night_preflight "$TMP/conc/fakebin" "$TMP/conc/out" "$TMP/conc/fakebin.cpp" \
       >/dev/null 2>&1 && echo PASSED || echo BLOCKED )
check "skutečně běžící binárka běh ZASTAVÍ" "$out" "BLOCKED"
kill $REAL 2>/dev/null; wait $REAL 2>/dev/null

# (c) verzovaná binárka pythonu (python3.12) musí být poznaná -- vzor
#     `*/python3` ji NECHYTAL a preflight pak souběžný sběr neviděl (17.08.)
cat > "$TMP/conc/diag_replay_mine_2026_fake.py" <<'PYEOF'
import time
time.sleep(25)
PYEOF
python3 "$TMP/conc/diag_replay_mine_2026_fake.py" & PYRUN=$!
sleep 0.5
out=$( . "$LIB"; NIGHT_LOG="$TMP/conc/out/chain.log"
       night_preflight "$TMP/conc/fakebin" "$TMP/conc/out" "$TMP/conc/fakebin.cpp" \
       >/dev/null 2>&1 && echo PASSED || echo BLOCKED )
check "běžící sběr pod python3.12 běh ZASTAVÍ" "$out" "BLOCKED"
kill $PYRUN 2>/dev/null; wait $PYRUN 2>/dev/null

echo "== 5. kontrola baseline =="

mkdir -p "$TMP/run" "$TMP/base"
echo "aaaaaaaa" > "$TMP/run/ENGINE_HEAD"
echo "aaaaaaaa" > "$TMP/base/ENGINE_HEAD"
out=$( . "$LIB"; NIGHT_LOG="$TMP/run/chain.log"
       night_check_baseline "$TMP/run" "$TMP/base" >/dev/null 2>&1 && echo SAME || echo DIFF )
check "týž engine commit projde" "$out" "SAME"

echo "bbbbbbbb" > "$TMP/base/ENGINE_HEAD"
out=$( . "$LIB"; NIGHT_LOG="$TMP/run/chain.log"
       night_check_baseline "$TMP/run" "$TMP/base" >/dev/null 2>&1 && echo SAME || echo DIFF )
check "JINÝ engine commit se odmítne" "$out" "DIFF"

rm -f "$TMP/base/ENGINE_HEAD"
out=$( . "$LIB"; NIGHT_LOG="$TMP/run/chain.log"
       night_check_baseline "$TMP/run" "$TMP/base" >/dev/null 2>&1 && echo SAME || echo DIFF )
check "baseline BEZ otisku se odmítne (běhy před 17.08.)" "$out" "DIFF"

echo "== 6. sloučení shardů (night_summarize.py, 18.08.) =="
# Vada: noc 17.->18.08. doběhla čistě a SKONČILA BEZ VÝSLEDKU -- 6 000 párů
# leželo jako osm jednotlivě neprůkazných čísel a součet udělal ráno člověk.
SUM="$ROOT/night_summarize.py"
mkshard() {  # $1=dir $2=delta $3=se $4=leak $5=n_nonzero
    mkdir -p "$1"
    { echo "SUMMARY matchup 1 (dwarf vs wood-elf), 750 pairs (1500 games), n_nonzero $5:"
      echo "  arm acted in 750/750 pairs; pairs that moved: $5; MOVED WITHOUT THE ARM ACTING: $4  (0 = clean)"
      echo "  PAIRED delta chess as dwarf: $2 +- $3 SE (~0.0 SE)"
    } > "$1/run.log"
}
# 6a: dva shardy pod prahem každý zvlášť se SLOUČÍ nad práh
D="$TMP/sum1"; mkshard "$D/m_s0" "-0.0200" "0.0190" 0 480; mkshard "$D/m_s1" "-0.0300" "0.0190" 0 480
out=$(THRESHOLD=0.015 python3 "$SUM" "$D" m 2>&1)
echo "$out" | grep -q -- "-0.0250" && ok "sloučená delta ze dvou shardů" || bad "sloučená delta: $out"
echo "$out" | grep -q "ŠKODÍ" && ok "verdikt proti prahu se vynese strojově" || bad "verdikt chybí"
echo "$out" | grep -q "2/2 shardů záporných" && ok "znaménka shardů se počítají" || bad "znaménka chybí"

# 6b: PRÁH JE VSTUP, ne konstanta -- táž data, jiný práh, jiný verdikt
out=$(THRESHOLD=0.05 python3 "$SUM" "$D" m 2>&1)
echo "$out" | grep -q "NEROZHODNUTO" && ok "vyšší práh dá NEROZHODNUTO (práh je vstup)" || bad "práh se neuplatnil"
echo "$out" | grep -q "se zapisuje JAKO NEROZHODNUTO" \
    && ok "NEROZHODNUTO si vyžádá vlastní zápis" || bad "chybí věta o zápisu NEROZHODNUTO"

# 6c: LEAK V JEDINÉM SHARDU MUSÍ ZASTAVIT ČTENÍ DELTY.
#     Původní blok grepoval `head -1`, takže leak v shardu 5 by neprobublal.
D2="$TMP/sum2"; mkshard "$D2/m_s0" "-0.0200" "0.0190" 0 480; mkshard "$D2/m_s1" "-0.0300" "0.0190" 7 480
out=$(THRESHOLD=0.015 python3 "$SUM" "$D2" m 2>&1); rc=$?
check "leak v NEPRVNÍM shardu vrátí nenulový kód" "$rc" "3"
echo "$out" | grep -q "DELTA SE NEČTE" && ok "leak zastaví čtení delty" || bad "leak deltu nezastavil"
echo "$out" | grep -q "m_s1" && ok "leak ukáže, KTERÝ shard" || bad "neřekne který shard"
echo "$out" | grep -q "DELTA SLOUČENĚ" && bad "delta se vytiskla i přes leak" || ok "delta se při leaku NEVYTISKNE"

# 6d: chybějící kontrola (stará binárka) se pozná a verdikt se NEVYNESE
D3="$TMP/sum3"; mkdir -p "$D3/m_s0"
{ echo "SUMMARY matchup 1 (dwarf vs wood-elf), 375 pairs (750 games):"
  echo "  PAIRED delta chess as dwarf: -0.0297 +- 0.0145 SE (~-2.0 SE)"; } > "$D3/m_s0/run.log"
out=$(THRESHOLD=0.015 python3 "$SUM" "$D3" m 2>&1); rc=$?
check "log bez per-pair kontroly vrátí nenulový kód" "$rc" "2"
echo "$out" | grep -q "VERDIKT SE NEVYNÁŠÍ" && ok "stará binárka: verdikt se nevynáší" || bad "starý log prošel"

# 6e: overdisperze -- shardy si neodpovídají, sloučení je podezřelé
D4="$TMP/sum4"
mkshard "$D4/m_s0" "+0.2000" "0.0190" 0 480; mkshard "$D4/m_s1" "-0.2000" "0.0190" 0 480
out=$(THRESHOLD=0.015 python3 "$SUM" "$D4" m 2>&1)
echo "$out" | grep -q "SHARDY SI NEODPOVÍDAJÍ" && ok "overdisperze se ohlásí" || bad "overdisperze neohlášena"

echo
if [ "$FAILS" -eq 0 ]; then
    printf "\033[32mVŠECH %s KONTROL PROŠLO\033[0m\n" "$RUNS"
else
    printf "\033[31m%s SELHÁNÍ\033[0m\n" "$FAILS"
fi
exit "$FAILS"
