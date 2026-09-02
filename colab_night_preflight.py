#!/usr/bin/env python3
# ============================================================================
# KONTROLA BĚHU PRO COLAB                                       (29.08.2026)
#
# Neodpovídá na otázku „co vyšlo", ale na otázku „SMÍ SE TA NOC VŮBEC PUSTIT
# NA TOMHLE STROJI" -- a odpovídá na ni PŘED během, ne ráno po něm.
#
# ⛔ PROČ TO NENÍ BUŇKA V NOTEBOOKU: aby se to dalo otestovat jinde než
#   v Colabu. Notebook `colab_night_preflight.ipynb` tenhle skript jen zavolá.
#
# Kontroly jsou schválně TYTÉŽ, které dělá `run_night_ab.sh`, plus tři, které
# na serveru nebyly potřeba a v Colabu rozhodují:
#   (A) POČET JADER   -- tempo 8h31m u B2 předpokládá 8 workerů; Colab dává
#                        v základu 2 vCPU, tedy ~4x delší běh
#   (B) DÉLKA SEZENÍ  -- noc se do jednoho sezení nevejde, musí se nakrájet
#   (C) KRÁJENÍ       -- kusy musí být STEJNĚ VELKÉ, jinak se rozbije sdružená
#                        SE (`night_summarize` váží shardy stejně)
#
# ⚠️ Co tenhle skript NEVÍ a vědět nemůže: jestli Colab sezení vydrží, jestli
#    se běh po odpojení dá navázat (semafory `AB_DONE` / `OK` tomu nasvědčují,
#    ale ověřeno to není), a jak se mezi sezeními přenese `.so` a korpus.
# ============================================================================
import argparse, os, re, shutil, subprocess, sys, time

ROOT = os.path.dirname(os.path.abspath(__file__))
BIN  = os.path.join(ROOT, 'diag_f1_cage_advance')
SRC  = os.path.join(ROOT, 'diag_f1_cage_advance_harness.cpp')
SO   = os.path.join(ROOT, 'engine', 'build', 'libbb_engine.so')

# ⚠️ POŘADÍ JE VÝZNAMOVÉ a musí sedět s `MATCHUPS[]` v
# `diag_f1_cage_advance_harness.cpp` -- index se zapisuje do každého řádku na
# disku. Doplňuje se na KONEC, nikdy doprostřed.
MATCHUP_NAMES = ['dw-sk', 'dw-we', 'dw-dw', 'orc-sk', 'dw-orc', 'dw-hum',
                 'we-we', 'dwnw-dwnw']   # 7 = nula pro B2 (bez Wrestle)

results = []   # (stav, název, detail)
def rec(state, name, detail=''):
    results.append((state, name, detail))
    mark = {'OK': '  OK  ', 'STOP': ' STOP ', 'WARN': ' WARN ', 'INFO': ' info '}[state]
    print('[%s] %-34s %s' % (mark, name, detail))

def sh(cmd, **kw):
    return subprocess.run(cmd, shell=True, capture_output=True, text=True, **kw)

# --- 1. STROJ ---------------------------------------------------------------
def step_machine(args):
    print('\n=== 1. STROJ ===')
    ncpu = os.cpu_count() or 1
    in_colab = 'COLAB_GPU' in os.environ or os.path.isdir('/content')
    rec('INFO', 'prostředí', 'Google Colab' if in_colab else 'jiný stroj')
    mem = ''
    try:
        with open('/proc/meminfo') as f:
            kb = int(re.search(r'MemTotal:\s+(\d+)', f.read()).group(1))
        mem = '%.1f GB RAM' % (kb / 1048576.0)
    except Exception:
        pass
    if ncpu < args.workers:
        rec('STOP', 'jader', '%d jader < WORKERS=%d %s' % (ncpu, args.workers, mem))
        print('       ⛔ 26.08. bylo 16 workerů na 12 jader HORŠÍ než méně workerů:')
        print('          nedoběhlo NIC. Přeplnění se neplatí zpomalením, ale pádem.')
    elif ncpu <= 2:
        rec('WARN', 'jader', '%d %s — tempo bude ~4x horší než na 8' % (ncpu, mem))
    else:
        rec('OK', 'jader', '%d %s' % (ncpu, mem))
    r = sh('git -C "%s" rev-parse --short HEAD' % ROOT)
    b = sh('git -C "%s" rev-parse --abbrev-ref HEAD' % ROOT)
    rec('INFO', 'commit', '%s na větvi %s' % (r.stdout.strip(), b.stdout.strip()))
    dirty = sh('git -C "%s" status --porcelain engine' % ROOT).stdout.strip()
    if dirty:
        rec('WARN', 'engine není čistý', 'noc se musí pustit z commitnutého stavu')
    return ncpu

# --- 2. BINÁRKY A ABI (totéž co night_preflight) ----------------------------
def step_build():
    print('\n=== 2. BINÁRKY A ABI ===')
    for path, label in ((SO, 'libbb_engine.so'), (BIN, 'harness')):
        if not os.path.exists(path):
            rec('STOP', 'chybí ' + label, path)
            return False
    src_newest = 0
    for d in (os.path.join(ROOT, 'engine', 'src'), os.path.join(ROOT, 'engine', 'include')):
        for dp, _, fs in os.walk(d):
            for f in fs:
                src_newest = max(src_newest, os.path.getmtime(os.path.join(dp, f)))
    if os.path.getmtime(SO) < src_newest:
        rec('STOP', '.so je STARŠÍ než engine/src', 'přestav engine, jinak měříš starý kód')
        return False
    rec('OK', '.so je čerstvé')
    # ⭐ VZTAH, ne objekt: binárka × .so. Právě tenhle pár nikdo nehlídal
    #   a 24.08. na něm umřela fáze B víkendu (SEGFAULT z ABI nesouladu).
    if os.path.getmtime(SO) > os.path.getmtime(BIN):
        rec('STOP', 'ABI: .so je novější než harness',
            'přelož harness, jinak SEGFAULT na první hře')
        print('       g++ -O2 -std=c++20 -Iengine/include -Iengine/third_party \\')
        print('           diag_f1_cage_advance_harness.cpp -Lengine/build -lbb_engine \\')
        print('           -Wl,-rpath,$PWD/engine/build -o diag_f1_cage_advance')
        return False
    rec('OK', 'ABI harness × .so')
    if os.path.getmtime(SRC) > os.path.getmtime(BIN):
        rec('STOP', 'harness je starší než jeho zdroj')
        return False
    rec('OK', 'harness × jeho zdroj')
    return True

# --- 2b. SOUBORY, KTERÉ BĚH POTŘEBUJE A GIT JE NENESE ----------------------
# ⛔ TŘÍDA, KTEROU ODHALIL AŽ COLAB (29.08.2026): `.gitignore` drží mimo repo
#   soubory, BEZ KTERÝCH SE NOC NESPUSTÍ. Na serveru to nikdo nepoznal -- ležely
#   tam od začátku. Čerstvý klon (a Colab klonuje VŽDY) je nemá:
#     · weights_policy.json  -- `/weights*.json` v .gitignore; harness bez něj
#       skončí rc=1 a vypíše "weights_best.json + weights_policy.json required"
#     · rules_bb2016.txt     -- bez něj se nedá ověřit ani jedna citace pravidel
#   ⇒ Do Colabu se musí DOSTAT JINUDY (Drive, upload), a preflight to musí
#     říct PŘED během, ne uprostřed něj.
def step_payload():
    print('\n=== 2b. SOUBORY MIMO GIT, BEZ KTERÝCH SE NEBĚŽÍ ===')
    ok = True
    need = [('weights_best.json',  True,  'váhy hodnotové funkce'),
            ('weights_policy.json', True, 'váhy policy — bez nich harness končí rc=1'),
            ('rules_bb2016.txt',   False, 'text pravidel pro ověřování citací')]
    for name, blocking, why in need:
        f = os.path.join(ROOT, name)
        if os.path.exists(f):
            tracked = sh('git -C "%s" ls-files --error-unmatch %s' % (ROOT, name)).returncode == 0
            rec('OK', name, 'v gitu' if tracked else 'je tady, ale MIMO git')
            if not tracked:
                print('       ⏰ je v .gitignore ⇒ PŘÍŠTÍ sezení si ho musí nakopírovat')
                print('          znovu (klon ho nepřinese). Teď je všechno v pořádku.')
        else:
            rec('STOP' if blocking else 'WARN', 'chybí ' + name, why)
            ok = ok and not blocking
    return ok

# --- 3. ZADÁNÍ BĚHU ---------------------------------------------------------
def step_spec(args):
    print('\n=== 3. ZADÁNÍ BĚHU ===')
    specs = args.matchups.split()
    nulls = [s for s in specs if s.endswith(':0')]
    for s in specs:
        idx = int(s.split(':')[0])
        nm = MATCHUP_NAMES[idx] if 0 <= idx < len(MATCHUP_NAMES) else '???'
        exp = 'expozice' if s.endswith(':1') else 'NULA'
        rec('INFO', 'matchup %s' % s, '%s — %s' % (nm, exp))
    if not nulls and not args.control_mode2:
        rec('STOP', 'chybí nulová kontrola',
            'bez nuly se efekt nedá odlišit od podlahy aparátu (P20)')
        return False
    # ⛔ HLÁŠKA MUSÍ ŘÍCT, ČÍM to prošlo. Do 30.08. tiskla „OK — 0 z 1 matchupů",
    #   což je samo o sobě protimluv: nula nulových matchupů a přesto OK.
    #   Prošlo to díky `--control-mode2`, jenže to z výpisu nešlo poznat --
    #   a harness sám o mode 2 píše, že je to pod CRN TAUTOLOGIE a verdikt na
    #   něm stát nesmí. Takový popisek je horší než žádný.
    if nulls:
        rec('OK', 'nulová kontrola', '%d z %d matchupů' % (len(nulls), len(specs)))
    else:
        rec('WARN', 'nulová kontrola JEN přes CONTROL_MODE2',
            'žádný matchup s nulovou expozicí — mode 2 je pod CRN TAUTOLOGIE')
        print('       ⚠️ Chytí jedině hrubou chybu v seedování. Verdikt na něm')
        print('          NESTOJÍ — skutečná nula je matchup, kde se rameno')
        print('          spustit NEMŮŽE (u B2 je to `7:dwnw:0`).')
    if args.prereg:
        p = args.prereg if os.path.isabs(args.prereg) else os.path.join(ROOT, args.prereg)
        if not os.path.exists(p):
            rec('STOP', 'předregistrace neexistuje', args.prereg); return False
        txt = open(p, encoding='utf-8', errors='replace').read()
        need = len(re.findall(r'(?m)^\s*corpus:', txt))
        if need and not args.corpus:
            rec('STOP', 'předregistrace chce korpus, CORPUS=0',
                '%d předpověď/i by zůstalo bez odpovědi' % need)
            return False
        rec('OK', 'předregistrace', '%d B, %d předpovědí na korpus' % (len(txt), need))
    else:
        rec('WARN', 'bez předregistrace', 'noc bez ní nemá co potvrzovat')
    return True

# --- 4. SONDA (1 pár v cílovém režimu) --------------------------------------
def step_probe(args):
    print('\n=== 4. SONDA 1 PÁRU V REŽIMU %d ===' % args.mode)
    idx = args.matchups.split()[0].split(':')[0]
    probe = os.path.join(ROOT, '.preflight_probe')
    shutil.rmtree(probe, ignore_errors=True); os.makedirs(probe)
    t0 = time.time()
    r = subprocess.run([BIN, ROOT, '1', idx, str(args.mode), '0'],
                       cwd=probe, capture_output=True, text=True)
    dt = time.time() - t0
    log = r.stdout + r.stderr
    open(os.path.join(probe, 'run.log'), 'w').write(log)
    if r.returncode != 0:
        rec('STOP', 'sonda SPADLA', 'rc=%d%s' % (
            r.returncode,
            ' ⇒ signál %d (139 = SEGFAULT, skoro vždy ABI)' % (r.returncode - 128)
            if r.returncode >= 128 else ''))
        return None
    if 'MOVED WITHOUT THE ARM ACTING' not in log:
        rec('STOP', 'režim nemá signál ramene',
            'mode %d chybí v armSignalAvailable ⇒ verdikt by neměl na čem stát' % args.mode)
        return None
    rec('OK', 'sonda běží a tiskne kontrolu ramene', '%.1f s na pár' % dt)

    # ⛔⛔ KAŽDÉ REGISTROVANÉ ČTENÍ MUSÍ MÍT ŘÁDEK VE VÝPISU (02.09.2026).
    #   T2.13 sem přidalo kontrolu, že se tiskne LEAK řádek -- a ta se od té
    #   doby nikdy neopakovala, protože se SPOUŠTÍ. Tohle je její zobecnění.
    #   ⛔ Proč: 01.09. (P35) jsem registroval čtení, na které harness neměl
    #     pole. 02.09. (M13) jsem čítače pro registrované čtení do enginu
    #     PŘIDAL a nezapojil do výpisu -- noc pak vytiskla přesně to zavádějící
    #     číslo, které jsem den předtím označil za zavádějící.
    #   Řetěz je čtyřdílný: čítač v enginu -> sběr v harnessu -> printf ->
    #   výstup. Kontrolovat se dá jen ten poslední díl, a to stačí.
    if args.expect:
        chybi = [lbl for lbl in args.expect.split(',') if lbl.strip()
                 and lbl.strip() not in log]
        if chybi:
            rec('STOP', 'registrované čtení se NEVYTISKNE',
                'chybí ve výstupu: %s ⇒ noc by doběhla a odpověď by v ní nebyla'
                % ', '.join(chybi))
            return None
        rec('OK', 'všechna registrovaná čtení mají řádek ve výpisu',
            args.expect)
    else:
        rec('WARN', 'nezadáno --expect',
            'předregistrace má ruční čtení? pak sem patří jména jejich řádků')
    m = re.search(r'ARM PICKS TOTAL: (\d+)', log)
    if m:
        rec('INFO', 'picků ramene', '%s (na 1 páru)' % m.group(1))
    shutil.rmtree(probe, ignore_errors=True)
    return dt

# --- 5. TEMPO NA TOMHLE STROJI (manifest to žádá jako PODMÍNKU) -------------
def step_tempo(args, ncpu):
    print('\n=== 5. TEMPO — MĚŘENO ZDE, NEPŘEVZATO ===')
    print('    ⛔ Manifest zakazuje převzít tempo z jiného matchupu nebo stroje:')
    print('       26.08. stál takový odhad dvě hodiny a projekce vyšla o den vedle.')
    idx = args.matchups.split()[0].split(':')[0]
    n = args.tempo_pairs

    d1 = os.path.join(ROOT, '.preflight_tempo1')
    shutil.rmtree(d1, ignore_errors=True); os.makedirs(d1)
    t0 = time.time()
    subprocess.run([BIN, ROOT, str(n), idx, str(args.mode), '0'],
                   cwd=d1, capture_output=True, text=True)
    solo = (time.time() - t0) / n
    rec('INFO', 'jeden proces', '%.1f s/pár' % solo)

    w = min(args.workers, ncpu)
    dw = os.path.join(ROOT, '.preflight_tempoW')
    shutil.rmtree(dw, ignore_errors=True); os.makedirs(dw)
    procs = []
    t0 = time.time()
    for k in range(w):
        sub = os.path.join(dw, 'w%d' % k); os.makedirs(sub)
        procs.append(subprocess.Popen(
            [BIN, ROOT, str(n), idx, str(args.mode), str(1000 * (k + 1))],
            cwd=sub, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL))
    for p in procs: p.wait()
    par = (time.time() - t0) / n          # s/pár/worker při w naráz
    shutil.rmtree(d1, ignore_errors=True); shutil.rmtree(dw, ignore_errors=True)
    cont = par / solo if solo > 0 else float('nan')
    rec('INFO', '%d workerů naráz' % w, '%.1f s/pár/worker' % par)
    state = 'WARN' if cont > 2.0 else 'OK'
    rec(state, 'přirážka za soupeření', '%.2fx  (server měl 1,45x na 8 workerech)' % cont)

    # ⭐⭐ ROZHODUJE PROPUSTNOST, NE s/pár. Změřeno na Colabu 29.08.: solo
    #   70,5 s/pár, dva workery 191,5 s/pár/worker => 0,0142 vs 0,0104 páru/s,
    #   tedy DVA WORKERY JSOU O 27 % POMALEJŠÍ NEŽ JEDEN. Přirážka horší než
    #   lineární u dvou "jader" znamená dva HYPERTHREADY jednoho fyzického
    #   jádra -- druhý worker si nebere volné jádro, rve se o totéž.
    thr_solo = 1.0 / solo if solo > 0 else 0.0
    thr_par  = w / par if par > 0 else 0.0
    rec('INFO', 'propustnost', 'solo %.4f  ·  %dx %.4f páru/s' % (thr_solo, w, thr_par))
    if thr_par < thr_solo:
        rec('WARN', 'VÍC WORKERŮ JE POMALEJŠÍ',
            'jeď --workers 1 (%.0f %% rychleji celkem)' % ((thr_solo / thr_par - 1) * 100))
        return solo, solo, 1
    return solo, par, w

# --- 6. ROZVRH DO SEZENÍ ----------------------------------------------------
def step_plan(args, par, w):
    print('\n=== 6. KOLIK SE TOHO VEJDE DO SEZENÍ ===')
    # ⛔ OPRAVA 29.08.: práce je MATCHUPY x PÁRY, ne jen páry. Do téhle opravy
    #   skript u dvou matchupů hlásil polovinu potřebného času -- tedy říkal,
    #   ze se noc do sezení vejde, a nevešla by se.
    specs = args.matchups.split()
    nm = len(specs)
    n_null = sum(1 for x in specs if x.endswith(':0'))
    n_exp = nm - n_null
    null_pairs = args.null_pairs or args.pairs
    # Práce se počítá PO MATCHUPECH podle toho, kolik má který párů.
    work = n_exp * args.pairs + n_null * null_pairs
    budget = args.session_hours * 3600 * args.session_use
    fits = int(budget * w / par) if par > 0 else 0
    rec('INFO', 'do %.1f h při využití %.0f %%' % (args.session_hours, args.session_use * 100),
        '≈ %d párů práce celkem' % fits)
    need_h = work * par / w / 3600.0 if w else float('inf')
    rec('INFO', 'zadání: %d expozice x %d + %d nula x %d = %d párů'
        % (n_exp, args.pairs, n_null, null_pairs, work),
        '≈ %.1f h na %d workeru/ech' % (need_h, w))

    if fits >= work:
        rec('OK', 'vejde se do jednoho sezení')
    else:
        n_sessions = -(-work // max(fits, 1))
        rec('WARN', 'na jedno sezení to nestačí', 'potřeba ≈ %d sezení' % n_sessions)
        print('       ⛔ Engine se mezi sezeními NESMÍ přestavět: jedna noc = jedno')
        print('          měření, jen roztažené přes víc sezení. Týž commit, tatáž binárka.')

    # ⭐ KRÁJENÍ NEMĚNÍ CELKOVÝ ČAS -- fronta workery vytíží tak jako tak.
    #   Mění ODOLNOST: když kus umře nebo se stroj uspí, přijdeš o CELÝ kus,
    #   protože bez `OK` se dělá znovu od začátku. Doporučujeme tedy podle
    #   toho, jak dlouhý kus jsi ochoten ztratit, ne podle sezení.
    div = [c for c in range(w, 401) if args.pairs % c == 0]
    if div:
        best, best_h = None, None
        for c in div:
            # ⛔ NEDĚLIT `w`: kus běží na JEDNOM workeru. `par` je s/pár/worker,
            #   tedy už zahrnuje, že jich běží `w` naráz. Dělení navíc dalo
            #   u 600 párů „2,7 h" místo 21,6 -- číslo, které vypadá věrohodně.
            h = args.pairs / c * par / 3600.0
            if h <= 3.5 and (best is None or h > best_h):
                best, best_h = c, h
        print('       použitelné CHUNKS (dělitelé %d, >= WORKERS=%d): %s'
              % (args.pairs, w, ' '.join(map(str, div[:20]))))
        if best:
            rec('OK', 'doporučené krájení',
                'CHUNKS=%d ⇒ %d párů na kus ≈ %.1f h  (tolik ztratíš, když kus umře)'
                % (best, args.pairs // best, best_h))
        else:
            rec('WARN', 'žádný dělitel nedá kus pod 3,5 h',
                'nejjemnější je CHUNKS=%d ⇒ %.1f h na kus'
                % (div[-1], args.pairs / div[-1] * par / 3600.0))
    else:
        rec('STOP', 'žádný dělitel >= WORKERS', 'změň PAIRS na číslo s vhodnými děliteli')
    print('       ⛔ Kusy MUSÍ být stejně velké UVNITŘ matchupu — night_summarize')
    print('          váží shardy stejně, nestejné kusy rozbijí sdruženou SE.')

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--expect', default='',
                    help='cárkou oddělená jména řádků výstupu, která '
                         'předregistrace slibuje jako ruční čtení '
                         '(napr. "M13/CENA,M13/BLITZ,BLITZ/PROC")')
    ap.add_argument('--mode', type=int, required=True)
    ap.add_argument('--matchups', required=True, help='"idx:jméno:expozice ..."')
    ap.add_argument('--prereg', default='')
    ap.add_argument('--pairs', type=int, default=4800, help='cílový počet párů na matchup')
    # ⛔ Bez tohohle počítal rozvrh KAŽDÝ matchup na plný počet párů, takže
    #   u B2 (4800 expozice + 400 nula) hlásil 43,3 h místo 23,5 -- tedy
    #   skoro dvojnásobek. Přeceněný odhad je méně nebezpečný než podceněný,
    #   ale pořád je to číslo, podle kterého se rozhoduje o zabrání stroje.
    ap.add_argument('--null-pairs', type=int, default=0,
                    help='párů pro matchupy s expozicí 0 (0 = stejně jako --pairs)')
    ap.add_argument('--workers', type=int, default=8)
    ap.add_argument('--session-hours', type=float, default=12.0)
    ap.add_argument('--session-use', type=float, default=0.85,
                    help='jakou část sezení si troufneme využít')
    ap.add_argument('--tempo-pairs', type=int, default=2, help='manifest žádá 2 na segment')
    ap.add_argument('--corpus', action='store_true')
    ap.add_argument('--control-mode2', action='store_true')
    args = ap.parse_args()

    print('=' * 74)
    print('KONTROLA BĚHU  —  mode %d, matchupy "%s", cíl %d párů'
          % (args.mode, args.matchups, args.pairs))
    print('=' * 74)

    ncpu = step_machine(args)
    if step_build() and step_payload() and step_spec(args):
        if step_probe(args) is not None:
            solo, par, w = step_tempo(args, ncpu)
            step_plan(args, par, w)

    print('\n' + '=' * 74)
    stops = [r for r in results if r[0] == 'STOP']
    warns = [r for r in results if r[0] == 'WARN']
    if stops:
        print('⛔ NESPOUŠTĚT — %d blokujících nálezů:' % len(stops))
        for _, n, d in stops: print('   · %s: %s' % (n, d))
    else:
        print('✅ ŽÁDNÁ BLOKUJÍCÍ ZÁVADA' + ('  (%d varování)' % len(warns) if warns else ''))
        for _, n, d in warns: print('   ⚠️ %s: %s' % (n, d))
    print('=' * 74)
    return 1 if stops else 0

if __name__ == '__main__':
    sys.exit(main())
