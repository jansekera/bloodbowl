#!/usr/bin/env python3
"""KONTROLA: implementace proti STAŽENÉMU TEXTU PRAVIDEL.

Vzniklo 24.08.2026. Důvod: za jediný den se třikrát ukázalo, že se kód psal
podle sekundárního zdroje, ne podle rulebooku (Gaze počítala plochý 2+ jako
komunitní/AI přehledy, ne Agility roll; ~40 citací v testech míří na CRP/LRB6;
tři testy vadu přímo certifikovaly). Ruční čtení tuhle třídu nechytí, protože
ověří jen to, co nás napadne ověřit.

Co to dělá -- NEROZHODUJE, jen ukazuje, kde se dá lhát:
  (1) najde všechny citace pravidel v kódu a testech,
  (2) u každé VYTISKNE, co na tom řádku v pravidlech doopravdy stojí,
  (3) najde dovednosti, které jsou IMPLEMENTOVANÉ a NEMAJÍ ANI JEDNU citaci,
  (4) najde dovednosti v enumu, které nemá v src nikdo (mrtvý kód),
  (5) najde citace ukazující na CRP/LRB6 (špatný zdroj po nálezu z 20.08.).
"""
import re, sys, os, glob, collections

ROOT = os.path.dirname(os.path.abspath(__file__))
RULES = os.path.join(ROOT, 'rules_bb2016.txt')
SRC = glob.glob(os.path.join(ROOT, 'engine/src/*.cpp'))
TESTS = glob.glob(os.path.join(ROOT, 'engine/tests/*.cpp'))
ENUMS = os.path.join(ROOT, 'engine/include/bb/enums.h')

CITE = re.compile(r'(?:ř\.|l\.|line|řádk\w*|radek)\s*(\d{2,5})(?:\s*[-–—]\s*(\d{2,5}))?')
BADSRC = re.compile(r'\b(CRP|LRB6|LRB\s*6)\b')

def rules_lines():
    with open(RULES, encoding='utf-8', errors='replace') as f:
        return f.read().split('\n')

def skills():
    txt = open(ENUMS, encoding='utf-8').read()
    m = re.search(r'enum class SkillName[^{]*\{(.*?)\}', txt, re.S)
    out = []
    for tok in m.group(1).split(','):
        tok = re.sub(r'//.*', '', tok).strip()
        tok = tok.split('=')[0].strip()
        if tok and tok[0].isupper() and tok != 'SKILL_COUNT':
            out.append(tok)
    return out

CORPUS_GETTERS = ['getDwarfRoster1200', 'getSkavenRoster1200', 'getWoodElfRoster1200',
                  'getHumanRoster1200', 'getOrcRoster1200']

def corpus_skills():
    """Dovednosti, ktere v dnesnim korpusu SKUTECNE hraji -- pet rosteru TV1200.
    Bez tohohle je seznam 'bez citace' jen seznam, ne priorita."""
    txt = open(os.path.join(ROOT, 'engine/src/roster.cpp'), encoding='utf-8').read()
    out = set()
    for g in CORPUS_GETTERS:
        i = txt.find(g)
        if i < 0:
            continue
        j = txt.find('return roster;', i)
        for m in re.finditer(r'SkillName::(\w+)', txt[i:j]):
            out.add(m.group(1))
    return out

def scan(paths):
    hits = collections.defaultdict(list)   # file -> [(lineno, cite_from, cite_to, text)]
    bad  = collections.defaultdict(list)
    for p in paths:
        for i, line in enumerate(open(p, encoding='utf-8', errors='replace'), 1):
            for m in CITE.finditer(line):
                a = int(m.group(1)); b = int(m.group(2)) if m.group(2) else a
                hits[p].append((i, a, b, line.strip()))
            if BADSRC.search(line):
                bad[p].append((i, line.strip()))
    return hits, bad

def main():
    verbose = '--verbose' in sys.argv
    rl = rules_lines()
    n_rules = len(rl)
    src_hits, src_bad = scan(SRC)
    tst_hits, tst_bad = scan(TESTS)

    all_hits = {**{k: v for k, v in src_hits.items()}, **{k: v for k, v in tst_hits.items()}}
    total = sum(len(v) for v in all_hits.values())

    print("=" * 78)
    print("KONTROLA CITACÍ PRAVIDEL  —  implementace proti", os.path.basename(RULES))
    print("=" * 78)
    print(f"text pravidel: {n_rules} řádků")
    print(f"citací celkem: {total}   (src {sum(len(v) for v in src_hits.values())}, "
          f"testy {sum(len(v) for v in tst_hits.values())})")
    print()

    # (1) citace mimo rozsah = jistá chyba
    print("--- (1) CITACE MIMO ROZSAH TEXTU (jistá chyba) ---")
    bad_range = 0
    for p, hs in sorted(all_hits.items()):
        for (ln, a, b, txt) in hs:
            if a < 1 or b > n_rules:
                print(f"  {os.path.relpath(p, ROOT)}:{ln}  ř. {a}-{b}  MIMO 1..{n_rules}")
                bad_range += 1
    print(f"  celkem: {bad_range}")
    print()

    # (2) špatný zdroj
    print("--- (2) ODKAZY NA CRP / LRB6 (špatný zdroj, nález 20.08.) ---")
    nbad = 0
    for d in (src_bad, tst_bad):
        for p, hs in sorted(d.items()):
            for (ln, txt) in hs:
                nbad += 1
                if verbose:
                    print(f"  {os.path.relpath(p, ROOT)}:{ln}  {txt[:100]}")
    if not verbose:
        for d, name in ((src_bad, 'src'), (tst_bad, 'testy')):
            per = {os.path.basename(p): len(v) for p, v in d.items()}
            if per:
                print(f"  {name}: " + ", ".join(f"{k}={v}" for k, v in sorted(per.items())))
    print(f"  celkem: {nbad}    (--verbose vypíše řádky)")
    print()

    # (3) dovednosti bez jediné citace
    print("--- (3) DOVEDNOSTI: implementovaná mechanika BEZ CITACE ---")
    sk = skills()
    src_txt = {p: open(p, encoding='utf-8', errors='replace').read() for p in SRC}
    tst_txt = {p: open(p, encoding='utf-8', errors='replace').read() for p in TESTS}
    nocite, dead = [], []
    for s in sk:
        impl = [p for p, t in src_txt.items()
                if f'SkillName::{s}' in t and os.path.basename(p) not in ('roster.cpp',)]
        if not impl:
            dead.append(s)
            continue
        # citace v okolí zmínky o dovednosti (±25 řádků) v src NEBO v testech
        cited = False
        for p, t in list(src_txt.items()) + list(tst_txt.items()):
            lines = t.split('\n')
            idxs = [i for i, L in enumerate(lines) if f'SkillName::{s}' in L or f'{s} (' in L]
            for i in idxs:
                window = '\n'.join(lines[max(0, i - 25):i + 25])
                if CITE.search(window):
                    cited = True
                    break
            if cited:
                break
        if not cited:
            nocite.append(s)
    corpus = corpus_skills()
    hot = [s for s in nocite if s in corpus]
    cold = [s for s in nocite if s not in corpus]
    print(f"  {len(nocite)} z {len(sk) - len(dead)} implementovaných.")
    print(f"  ⛔ HRAJE V KORPUSU ({len(hot)}) -- tady se to platí každou hrou:")
    for s in hot:
        print(f"    {s}")
    print(f"  latentní ({len(cold)}):")
    print("    " + ", ".join(cold))
    print()

    print("--- (4) DOVEDNOSTI V ENUMU, KTERÉ NEMÁ V src NIKDO (mrtvý kód) ---")
    for s in dead:
        mark = "  ⛔ A JE V KORPUSOVÉM ROSTERU" if s in corpus else ""
        print(f"    {s}{mark}")
    print(f"  celkem: {len(dead)}")
    print()

    # (5) vypis citovaneho textu
    if verbose:
        print("--- (5) CO NA CITOVANÝCH ŘÁDCÍCH DOOPRAVDY STOJÍ ---")
        for p, hs in sorted(all_hits.items()):
            print(f"\n### {os.path.relpath(p, ROOT)}")
            for (ln, a, b, txt) in hs:
                if a < 1 or b > n_rules:
                    continue
                print(f"  :{ln}  ř. {a}-{b}")
                for k in range(a, min(b, a + 6) + 1):
                    print(f"        {k}| {rl[k-1].strip()}")
    else:
        print("(--verbose vypíše i text z citovaných řádků, ať jde ověřit obsah)")

    return 1 if bad_range else 0

if __name__ == '__main__':
    sys.exit(main())
