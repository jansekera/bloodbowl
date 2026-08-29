#!/usr/bin/env python3
# ============================================================================
# NOC PO KUSECH — pro sezení s tvrdým limitem (Colab: 8 h)   (29.08.2026)
#
# ⛔ PROČ NESTAČÍ `run_night_ab.sh`: NENAVAZUJE. `run_one` dělá
#   `rm -f "$d/OK" "$d/FAIL"` a frontu `.queue` na začátku maže, takže druhé
#   spuštění hotové kusy UDĚLÁ ZNOVU. Přeskočí se jedině celé `AB_DONE`, tedy
#   noc, která doběhla celá. Na stroji, který tě po 8 hodinách odpojí, by se
#   tak nikdy nedoběhlo nic.
#
# Tenhle runner dělá TOTÉŽ, jen s pamětí:
#   · kus k dostane offset `k * CHUNK_PAIRS` a `CHUNK_PAIRS` párů    (jako noc)
#   · výstup jde do `OUT/<jméno>_s<k>/` se semaforem `OK` / `FAIL`   (jako noc)
#   ⇒ výsledky se slučují týmž `night_summarize.py`, nic se neliší.
#
# Co přidává:
#   (1) NAVAZUJE — kus s `OK` se přeskočí a NIKDY se nemaže
#   (2) OTISK BĚHU — commit, sha256 harnessu a .so, zadání. Při návratu do
#       nového sezení se porovná a při NESHODĚ SE ODMÍTNE POKRAČOVAT.
#       „Jedna noc = jedno měření" platí i tehdy, když se ta noc roztáhne přes
#       pět sezení; přestavěný engine mezi kusy znamená DVĚ RŮZNÉ HRY v jednom
#       datovém souboru, a na tom by se nepoznalo nic.
#   (3) ROZPOČET SEZENÍ — pustí jen tolik kusů, kolik se do zbytku sezení
#       vejde, a skončí čistě s výpisem, co zbývá. Kus useknutý odpojením
#       nemá `OK`, takže se příště udělá znovu celý.
# ============================================================================
import argparse, hashlib, json, os, shutil, subprocess, sys, time

ROOT = os.path.dirname(os.path.abspath(__file__))
BIN  = os.path.join(ROOT, 'diag_f1_cage_advance')
SO   = os.path.join(ROOT, 'engine', 'build', 'libbb_engine.so')

def sha(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for b in iter(lambda: f.read(1 << 20), b''):
            h.update(b)
    return h.hexdigest()[:16]

def fingerprint(args):
    commit = subprocess.run(['git', '-C', ROOT, 'rev-parse', 'HEAD'],
                            capture_output=True, text=True).stdout.strip()
    return {'commit': commit, 'harness': sha(BIN), 'engine_so': sha(SO),
            'mode': args.mode, 'matchups': args.matchups,
            'total_pairs': args.pairs, 'chunks': args.chunks}

def check_fingerprint(out, fp):
    path = os.path.join(out, 'RUN_IDENTITY.json')
    if not os.path.exists(path):
        json.dump(fp, open(path, 'w'), indent=1)
        print('otisk běhu založen: %s' % path)
        return True
    old = json.load(open(path))
    diff = [k for k in fp if old.get(k) != fp[k]]
    if not diff:
        print('otisk běhu SEDÍ (commit %s, harness %s, .so %s)'
              % (fp['commit'][:8], fp['harness'], fp['engine_so']))
        return True
    print('\n⛔ ODMÍTÁM POKRAČOVAT — běh se od minulého sezení ZMĚNIL:')
    for k in diff:
        print('   %-12s bylo %s   je %s' % (k, old.get(k), fp[k]))
    print('   Jedna noc = jedno měření. Kusy z různých enginů v jednom')
    print('   datovém souboru vypadají jako jeden běh a nejsou jím.')
    print('   Buď se vrať na ten commit, nebo založ nový OUT a začni znovu.')
    return False

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--mode', type=int, required=True)
    ap.add_argument('--matchups', required=True, help='"idx:jméno:expozice ..."')
    ap.add_argument('--out', required=True, help='výstupní adresář (ideálně na Disku)')
    ap.add_argument('--pairs', type=int, required=True, help='CELKEM párů na matchup')
    # ⚠️ GRANULARITA: celkový čas na počtu kusů skoro nezávisí (fronta je
    #   vytíží), ale ODOLNOST ANO -- když kus umře nebo se stroj uspí, přijdeš
    #   o CELÝ kus, protože bez `OK` se dělá znovu od začátku. 4800/48 = 100
    #   párů na kus je při 8 workerech ~3 h; 4800/15 = 320 párů je ~10,5 h.
    ap.add_argument('--chunks', type=int, required=True, help='na kolik STEJNÝCH kusů')
    # ⭐ NULA SMÍ BÝT KRATŠÍ (uživatel 29.08., a fronta má precedens u Leapu:
    #   „nulový test je LEVNÝ, stačí mu ~200 párů: jeho úkol je ukázat NULU,
    #   ne změřit deltu"). Bez tohohle by nula běžela na plný počet a
    #   zdvojnásobila noc, aniž by to komukoli k něčemu bylo.
    ap.add_argument('--null-pairs', type=int, default=0,
                    help='párů pro matchupy s expozicí 0 (0 = stejně jako --pairs)')
    ap.add_argument('--null-chunks', type=int, default=0,
                    help='kusů pro nulu (0 = největší dělitel --null-pairs <= --chunks)')
    ap.add_argument('--workers', type=int, default=2)
    ap.add_argument('--session-hours', type=float, default=8.0)
    ap.add_argument('--session-use', type=float, default=0.85)
    ap.add_argument('--dry-run', action='store_true')
    args = ap.parse_args()

    started = time.time()
    out = args.out if os.path.isabs(args.out) else os.path.join(ROOT, args.out)
    os.makedirs(out, exist_ok=True)

    def split(pairs, chunks, label):
        if pairs % chunks:
            print('⛔ %s: %d párů se nedá rozdělit na %d stejných kusů (zbývá %d).'
                  % (label, pairs, chunks, pairs % chunks))
            print('   Nestejné kusy rozbijí sdruženou SE — night_summarize váží shardy stejně.')
            div = [c for c in range(1, 201) if pairs % c == 0]
            print('   Dělitelé %d: %s' % (pairs, ' '.join(map(str, div)) or 'žádný do 200'))
            return None
        return pairs // chunks

    chunk_pairs = split(args.pairs, args.chunks, 'expozice')
    if chunk_pairs is None:
        return 7
    # Nula: vlastní počet párů i kusů, ať se kus nezvětší proti expozici.
    null_pairs = args.null_pairs or args.pairs
    if args.null_chunks:
        null_chunks = args.null_chunks
    else:
        cands = [c for c in range(1, args.chunks + 1) if null_pairs % c == 0]
        null_chunks = cands[-1] if cands else 1
    null_chunk_pairs = split(null_pairs, null_chunks, 'nula')
    if null_chunk_pairs is None:
        return 7
    if null_pairs != args.pairs:
        print('nula je kratší: %d párů v %d kusech po %d  (expozice %d v %d po %d)'
              % (null_pairs, null_chunks, null_chunk_pairs,
                 args.pairs, args.chunks, chunk_pairs))

    if not check_fingerprint(out, fingerprint(args)):
        return 8
    if os.path.exists(os.path.join(out, 'AB_DONE')):
        print('AB_DONE už existuje — noc je hotová. Slučuj:')
        print('   PREREG=... THRESHOLD=... python3 night_summarize.py %s <jména>' % out)
        return 0

    specs = [s.split(':') for s in args.matchups.split()]
    todo, done, total = [], 0, 0
    for idx, name, exp in specs:
        nch = args.chunks if exp != '0' else null_chunks
        npr = chunk_pairs if exp != '0' else null_chunk_pairs
        total += nch
        for k in range(nch):
            d = os.path.join(out, '%s_s%d' % (name, k))
            if os.path.exists(os.path.join(d, 'OK')):
                done += 1
            else:
                todo.append((idx, name, k, d, npr, nch))
    print('\nkusů celkem %d · hotovo %d · zbývá %d   (expozice %d párů na kus)'
          % (total, done, len(todo), chunk_pairs))
    if not todo:
        open(os.path.join(out, 'AB_DONE'), 'w').close()
        print('✅ všechny kusy mají OK — AB_DONE založeno')
        return 0

    budget = args.session_hours * 3600 * args.session_use

    # ⭐⭐ FRONTA MÍSTO DÁVEK (29.08.2026). Do téhle opravy pouštěl runner
    #   `workers` kusů naráz a ČEKAL NA VŠECHNY, než pustil další dávku --
    #   tedy přesně vada, kterou shellový harness odstranil v T2.15:
    #     "Shard dostával pevný blok seedů, jenže zápasy nejsou stejně dlouhé
    #      => shardy se rozejdou a běh čeká na nejpomalejšího."
    #   Navíc poslední dávka bývá poloprázdná (15 kusů na 8 workerech = 8+7).
    #   Teď si slot bere DALŠÍ VOLNÝ KUS, jakmile dodělá.
    #
    # ⏰ ROZPOČET SEZENÍ: když dojde čas, PŘESTANOU SE POUŠTĚT NOVÉ kusy, ale
    #   rozběhnuté se nechají dojet. Zabít je by znamenalo zahodit jejich práci
    #   -- kus bez `OK` se příště dělá celý znovu.
    per_chunk = None            # nejdelší dosud doběhlý kus (horní odhad)
    ran, failed = 0, 0
    running = []                # [(Popen, d, name, k, t0)]
    stop_launching = False

    def launch(item):
        idx, name, k, d, npr, nch = item
        os.makedirs(d, exist_ok=True)
        # ⛔ NIKDY nemazat existující OK -- to je právě to, co noc dělá a proč
        #    se na ni nedá navázat.
        f = os.path.join(d, 'FAIL')
        if os.path.exists(f): os.remove(f)
        cmd = [BIN, ROOT, str(npr), idx, str(args.mode), str(k * npr)]
        print('   ▶ %s kus %d/%d  (offset %d, %d párů)'
              % (name, k + 1, nch, k * npr, npr), flush=True)
        if args.dry_run:
            return (None, d, name, k, time.time())
        log = open(os.path.join(d, 'run.log'), 'w')
        return (subprocess.Popen(cmd, cwd=d, stdout=log, stderr=log),
                d, name, k, time.time())

    while todo or running:
        while todo and len(running) < args.workers and not stop_launching:
            if per_chunk is not None:
                left = budget - (time.time() - started)
                if left < per_chunk * 1.1:
                    print('\n⏹  KONEC SEZENÍ: na další kus (~%.0f min) zbývá %.0f min.'
                          % (per_chunk / 60, max(left, 0) / 60), flush=True)
                    if running:
                        print('   %d rozběhlých kusů se nechává dojet.' % len(running))
                    stop_launching = True
                    break
            running.append(launch(todo.pop(0)))
            ran += 1
        if args.dry_run:
            running = []
            continue
        if not running:
            break
        time.sleep(2)
        still = []
        for proc, d, name, k, t0 in running:
            rc = proc.poll()
            if rc is None:
                still.append((proc, d, name, k, t0)); continue
            dt = time.time() - t0
            per_chunk = dt if per_chunk is None else max(per_chunk, dt)
            open(os.path.join(d, 'OK' if rc == 0 else 'FAIL'), 'w').close()
            if rc:
                failed += 1
                print('   ⛔ FAIL %s kus %d (rc=%d) — viz %s/run.log'
                      % (name, k, rc, d), flush=True)
            else:
                print('   ✓ %s kus %d hotov za %.1f min   (zbývá %d kusů)'
                      % (name, k + 1, dt / 60, len(todo)), flush=True)
        running = still

    oks = 0
    for i, n, e in specs:
        for k in range(args.chunks if e != '0' else null_chunks):
            if os.path.exists(os.path.join(out, '%s_s%d' % (n, k), 'OK')):
                oks += 1
    print('\n' + '=' * 70)
    print('SEZENÍ HOTOVO: pustilo %d kusů (%d selhalo), celkem %d/%d má OK'
          % (ran, failed, oks, total))
    if oks == total:
        open(os.path.join(out, 'AB_DONE'), 'w').close()
        print('✅ VŠECHNY KUSY HOTOVÉ — AB_DONE založeno. Teď sloučit:')
        print('   PREREG=<prereg> THRESHOLD=0.015 python3 night_summarize.py \\')
        print('       %s %s' % (out, ' '.join(s[1] for s in specs)))
    else:
        print('⏸  Zbývá %d kusů. Pusť tenhle skript znovu v novém sezení se')
        print('   STEJNÝMI parametry — otisk běhu ohlídá, že engine je týž.')
        print('   ⛔ Mezi sezeními NEPŘESTAVUJ engine a nepřepínej commit.')
    print('=' * 70)
    return 0

if __name__ == '__main__':
    sys.exit(main())
