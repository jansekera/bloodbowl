# ZADÁNÍ pro Fable agenta: Corner-release groundwork (diskuze A) — 04.08.2026

## Kontext (přečti PŘED prací)

F1 CAGE_ADVANCE core fix je hotový (worktree `f1-cage-fix`, 5 commitů, 493/493
testů; v době tvého běhu už pravděpodobně zmergováno v main — ověř `git log`):
MA-computed krok, bank-while-clear, carrier GFI v tempo-nouzi, stavba klece
from-scratch na cílovém poli carriera, vacate-first, substituce rohů (volné
tělo přebije vázaného kandidáta). Viz commity + evidence/fable_f0f1_cage_advance_20260803.md.

Sonda `diag_f1_adoption_probe` (8 her dwarf–skaven, produkční config) po fixu:
PLAN_READY 4, **DICEY 29**, TEMPO_INSUFFICIENT 49 (mean required 6,6 —
převážně pozdní beznadějné, veto korektní), NOT_APPLICABLE 0 z 82 ADVANCE
turnů. **DICEY je hlavní zbývající brzda adoption** a jeho zbytek po
substituci = situace „volná těla nejsou" → potřeba RELEASE vrstvy.

**Pokyn uživatele (04.08., doslova jeho výpočetní osnova):** „nutno spočítat
kolik rohů potřebuje dodge — kolik je dalších volných hráčů pro nové rohy —
kolik z rohů lze uvolnit blockem — případně použít blitz na uvolnění rohu."
Jedna z variant výslovně počítá s blitzem na uvolnění rohu. Uživatel to
klasifikoval jako diskuzi kalibru blitzu → tvůj úkol je PODKLAD, ne
implementace a ne rozhodnutí.

## Úkoly

1. **Galerie DICEY situací (konkrétní stavy, ne jen agregáty!):** rozšiř
   `diag_f1_adoption_probe.cpp` (kopie v repu) o dump celých DICEY stavů
   (board + která noha plánu padla + proč). Nasbírej ≥40 DICEY situací napříč
   matchupy (dwarf–skaven, dwarf–we, dwarf mirror, orc–skaven; seedy disjunktní
   od 30/31/34/35M). Kategorizuj: (a) marked roh bez substituta, (b) TZ stín
   na cestě při přesném budgetu, (c) málo těl vůbec, (d) jiné.

2. **Uživatelova čtveřice čísel PER SITUACE:** kolik rohů potřebuje dodge;
   kolik volných těl na substituci je (a proč nestačila); kolik marked rohů
   by uvolnil BLOCK jiného hráče (spočti reálné kostky: asisty, Block/Tackle
   skilly, 2d/1d); kolik by uvolnil BLITZ (path-aware dle item14 logiky +
   kostky + koliduje s jediným blitzem tahu). Agreguj: jaký podíl DICEY by
   každá vrstva odemkla (horní odhad adoption zisku).

3. **Návrh design constraintů release vrstvy — JAKO OPCE k diskuzi:**
   varianty (block-only / block+blitz / s prioritizací dle alokačního pořadí
   rolí z 03.08.: BLITZER → asistenti → fauler → rohy), jejich konflikty
   (jediný blitz tahu vs. čištění koridoru vs. uvolnění rohu), doporučené
   acceptance kritérium per varianta. ŽÁDNÁ implementace.

4. **Checklist závazných constraintů** (poučení z MAX_STEP incidentu — viz
   feedback paměť): u každého návrhu explicitně ověř soulad s: tempo =
   výpočet ne konstanta; klec vždy na cílovém poli carriera; pořadí exekuce
   situační (dependency); bank-while-clear; carrier GFI jen v nouzi; standard
   „postav správnou klec vždy"; RESERVE_TURNS=1.

## Omezení

- Měření pouze `nice -n 19`, max 2 souběžné procesy — **v noci běží F1 A/B
  (4 matchupy), nesmíš ho vyhladovět ani ovlivnit** (žádné zápisy do jeho
  souborů, žádný zásah do main buildů; pracuj ve vlastním worktree).
- Šampiona a weights se NEDOTÝKEJ; žádné tréninky.
- Výstup: `evidence/fable_corner_release_report_20260804.md` — stručně
  (mechanismus dle feedback-concise-status-reports), galerii situací přilož
  jako samostatný soubor/přílohu.
- Hlídej tokeny (týdenní limit!) — cíl ≤150k.
