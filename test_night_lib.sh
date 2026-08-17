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

# (a) shell, který jméno binárky jen ZMIŇUJE, nesmí běh zastavit
bash -c 'x="fakebin diag_f1_cage_advance diag_replay_mine_2026"; sleep 20' & MENTION=$!
sleep 0.3
out=$( . "$LIB"; NIGHT_LOG="$TMP/conc/out/chain.log"
       night_preflight "$TMP/conc/fakebin" "$TMP/conc/out" "$TMP/conc/fakebin.cpp" \
       >/dev/null 2>&1 && echo PASSED || echo BLOCKED )
check "shell, co vzor jen zmiňuje, běh NEZASTAVÍ" "$out" "PASSED"
kill $MENTION 2>/dev/null; wait $MENTION 2>/dev/null

# (b) ale SKUTEČNĚ běžící binárka ho zastavit MUSÍ
"$TMP/conc/fakebin" 25 & REAL=$!
sleep 0.3
out=$( . "$LIB"; NIGHT_LOG="$TMP/conc/out/chain.log"
       night_preflight "$TMP/conc/fakebin" "$TMP/conc/out" "$TMP/conc/fakebin.cpp" \
       >/dev/null 2>&1 && echo PASSED || echo BLOCKED )
check "skutečně běžící binárka běh ZASTAVÍ" "$out" "BLOCKED"
kill $REAL 2>/dev/null; wait $REAL 2>/dev/null

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

echo
if [ "$FAILS" -eq 0 ]; then
    printf "\033[32mVŠECH %s KONTROL PROŠLO\033[0m\n" "$RUNS"
else
    printf "\033[31m%s SELHÁNÍ\033[0m\n" "$FAILS"
fi
exit "$FAILS"
