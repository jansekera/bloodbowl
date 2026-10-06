# Zkouška režimu „AI plánuje tah“ na partii 02.–03.10. (P146 krok 2) — 06.10.2026

**Co se dělalo.** Log partie (`evidence/play_20261002/prikazy.log`) se přehrál po začátek každého z 32 tahů
(16 trpasličích = uživatel, 16 elfích = Claude). V každé pozici AI naplánovala celý tah nanečisto:
20× `macro_mcts` (50 iterací, `weights_best.json`) a 20× `greedy`, každý plán s vlastními kostkami.
Srovnává se **plán tahu**, ne výsledek partie.

**Na čem.** Engine `4e142640` (jediný, na kterém se log přehraje) + přenesená vazba z `d07d7ed2`,
worktree `/home/jenda/claude/bb-replay-4e142640`, větev `zkouska-ai-plan-4e142640`. ⚠️ Je to engine **před**
sloučením nového výkopu; čísla patří tomuto stavu, ne dnešnímu `main`.

**Jak to zopakovat.**
`BB_ROOT=/home/jenda/claude/bb-replay-4e142640 /usr/local/bin/python3.8 bb_zkouska_ai_plan_20261006.py` (~6 min)
→ `evidence/zkouska_ai_plan_20261006/tabulka_tahu.md` (v gitu) a `tah_NN_*.md` s deskami + `souhrn.json` (mimo git, dají se znovu vyrobit).

## Kontroly měřidla

| kontrola | výsledek |
|---|---|
| přehrání = partie | 534 akcí ve stejném pořadí jako `partie_vypis.md`, skóre 2:0, 32 tahů |
| pozitivní kontrola přehrání | s enginem z `main` skript skončí „přehrání se rozešlo (akce 203 není v nabídce)“ |
| kostky plánů | hody na brnění, 2 590 kostek, χ² = 8,3 (mez 11,07) ⇒ férové |
| ⚠️ kostky bloku se takhle číst nedají | výpis ukazuje hod PO přehozu (lebek 12 % místo 16,7 %) — P130 |
| podíl turnoverů jinou cestou | `simulate_game_logged`, 12 celých her dwarf × wood-elf: trpaslíci 102/192 = 53 %, elfové 105/192 = 55 % (greedy 94 % a 88 %) ⇒ není to vada vazby `ai_plan_turn` |
| ⛔ první běh měl vadu | všech 32 pozic sdílelo stejných 20 semínek ⇒ 40 % / 68 %; po opravě (semínko = 1000·tah + s) 30 % / 56 % |

## Nálezy (vše engine C++, `macro_mcts`)

### N1. AI nedohraje tah: turnover ve 30 % trpasličích a 56 % elfích plánů; hlavní příčina = úhyby na 4+ a horší

| | skutečnost | macro_mcts | greedy |
|---|---|---|---|
| trpaslíci — turnover | 1/16 | 97/320 = 30 % | 234/320 = 73 % |
| elfové — turnover | 1/16 | 178/320 = 56 % | 308/320 = 96 % |

| | úhybů na tah | na 4+ a horší na tah | rozpad podle potřeby |
|---|---|---|---|
| uživatel (trpaslíci) | 0,06 | 0,06 | 4+: 1 |
| AI trpaslíci | 0,52 | 0,37 | 3+: 50 · 4+: 58 · 5+: 49 · 6+: 10 |
| Claude (elfové) | 1,38 | 0 | 2+: 22 |
| AI elfové | 1,80 | 0,62 | 2+: 250 · 3+: 126 · 4+: 52 · 5+: 132 · 6+: 16 |

- Úhyb je příčinou 58/97 = 60 % trpasličích a 126/178 = 71 % elfích turnoverů AI. Další: blok 21 a 17, blitz (rána) 3 a 24, faul 7 a 9.
- Když tah spadne, hráli před tím průměrně jen 3,5 (trpaslíci) a 3,2 (elfové) jiných hráčů; hned první akcí spadlo 18/97 a 56/178 plánů.
- **Situace (tah 2, elfové, kolo 1):** nosič DR4 stojí na (7,7) v čisté kleci. AI jako první akci tahu pošle Wardancera 21 blitzem
  na nosiče přes úhyby 4+, 2+, 5+. Turnover ve 20/20 plánech (13× úhyb při blitzu, 6× úhyb při pohybu). Claude v téže pozici: blitz na DBG9 mimo klec, bez turnoveru.
- Souvisí s P141 (a) „bezpečné napřed, riskantní na konec“ — tady je k tomu číslo.

### N1b. Odklad riskantních akcí (item 10 z 28.07.) existuje, je vypnutý — a zapnutý riziko jen odsune, neodstraní

`riskDeferral = false` (`engine/include/bb/mcts.h:28`), nic ho nezapíná; zkouška i běžná měření jedou s vypnutým.
Párově na týchž pozicích a semínkách (10 plánů na pozici, 160 na stranu a rameno):

| | turnover | úhybů 4+ a horších na tah | aktivováno před turnoverem |
|---|---|---|---|
| elfové, vypnuto | 90/160 = 56 % | 0,61 | 3,6 |
| elfové, zapnuto | 75/160 = 47 % | 0,51 | 5,6 |
| trpaslíci, vypnuto | 46/160 = 29 % | 0,37 | 3,7 |
| trpaslíci, zapnuto | 45/160 = 28 % | 0,35 | 5,9 |

- Odklad dělá, co má: riskantní akce přijde později (o ~2 aktivace). Riskantních úhybů ale skoro neubude — AI je zahraje stejně, jen na konci.
- Výběr blitzera podle rizika cesty (item 14, `estimateApproachFailChance` v `macro_actions.cpp`) v kódu je, a úhyby na 5+ při blitzu přesto padají.
- ⇒ Chybí druhá půlka: **riskantní akci vůbec nehrát**, když zisk nestojí za cenu. Uživatel 06.10.: měl to za vyřešené; zadáno jako P149 s vysokou prioritou.

### N2. AI za trpaslíky s míčem uprostřed drivu ukončí tah po 1–4 aktivacích a klec za nosičem nejde

Plány, které AI ukončila sama (bez turnoveru a bez TD):

| tah (kolo) | uživatel aktivoval | AI aktivovala (průměr, n) | rohy klece: uživatel | rohy: AI |
|---|---|---|---|---|
| 5 (1. pol., k3) | 10 z 10 | 1,4 (13) | 4 | 1,5 |
| 7 (k4) | 9 z 11 | 3,6 (5) | 3 | 0,0 |
| 9 (k5) | 11 z 11 | 1,9 (20) | 4 | 2,5 |
| 28 (2. pol., k6) | 11 z 11 | 1,9 (18) | 4 | 1,2 |

- Přes všechny dokončené tahy bez TD aktivuje AI-trpaslík 52 % hráčů (n = 148), uživatel 86 % (n = 13).
- Na začátku drivu je to v pořádku: tah 1 (sběr + klec) rohy 3,8 vs 4; tah 3 4,0 vs 4; tah 26 4,0 vs 4.
- Postup nosiče je přitom podobný (tah 5: 10,75 polí do TD vs 10; tah 9: 7,8 vs 6) — nosič jde, rohy stojí.
  Je to stejný obraz jako starší „nosič odešel a rohy nešly s ním“ (T2.6), teď na konkrétních pozicích z partie.

### N3. AI skóruje, jakmile dosáhne — nezdržuje (odpověď na P128 b)

| tah | kolo | pozice | uživatel | AI |
|---|---|---|---|---|
| 11 | 1. pol., k6 | nosič 6 polí od TD | postoupil na 3 pole, klec 4 rohy | TD 18/20, turnover 1/20 |
| 13 | k7 | nosič 3 pole od TD | postoupil na 1 pole | TD 17/20, turnover 3/20 (úhyb nosiče z klece v 17/20) |
| 15 | k8 | 1 pole od TD | TD | TD 20/20 |
| 30 | 2. pol., k7 | 1 pole od TD | neskóroval, bloky ⇒ turnover (2 lebky) | TD 20/20 |

- AI by v 1. poločase dala TD v 6. kole a elfům by zbyly 3 tahy na odpověď; uživatel čekal do 8. kola úmyslně.
- V tahu 30 by naopak okamžité TD ušetřilo turnover a ztrátu míče.
- „Naučit AI skórovat za trpaslíky“ (P128) tedy není o posledním kroku — ten AI udělá. Vázne postup klece (N2) a turnovery (N1).

### N4. Elfové s míčem (2. poločas, tahy 17–23): AI jde s klecí dopředu do dosahu trpaslíků, Claude ustupoval

| tah | Claude: polí do TD · rohy · trpaslíků v dosahu | AI: polí do TD · rohy · v dosahu · turnover |
|---|---|---|
| 17 (k1) | 22 · 0 · 0 | 18,0 · 4,0 · 3,9 · 4/20 |
| 19 (k2) | 22 · 1 · 1 | 14,7 · 2,7 · 8,6 · 8/20 |
| 21 (k3) | 24 · 0 · 1 | 15,7 · 1,1 · 9,1 · 6/20 |
| 23 (k4) | 19 · 0 · 5 | 15,9 · 0,4 · 8,4 · 9/20 |

- K P138: AI klec u elfů při sběru staví (tah 16 a 17: 4,0 rohy), ale od 2. kola se rozpadá, zatímco na nosiče dosáhne 8–9 trpaslíků.
- Ani jedna strana tu není vzor: Claude ztratil tempo a nakonec i míč, AI nosiče vystavuje. Otevřené pro rozhodnutí o doktríně.

### N5. Menší rozdíly

| | uživatel | AI trpaslíci | Claude | AI elfové |
|---|---|---|---|---|
| hráč na kraji (y = 0/14, proti Frenzy i o pole vedle) na tah | 0,25 | 0,25 | 0,62 | 0,63 |
| v kontaktu bez převahy na tah | 2,62 | 1,74 | 4,12 | 5,69 |
| ležící bez pokusu vstát na tah | 0,25 | 0,29 | 0,50 | 0,38 |
| blitz využit (tahů) | 88 % | 72 % | 56 % | 97 % |
| faul (tahů) | 0 % | 10 % | 0 % | 8 % |

- Kraj (P131): AI na něm stojí stejně často jako my oba ⇒ doktrína chybí všem.
- Kontakt (P137): AI-elf nechává v kontaktu bez převahy víc hráčů než Claude.
- Blitz (P134/P135): Claude ho nechal ležet ve 44 % tahů, AI skoro nikdy — ale 245 z 576 elfích úhybů AI padlo právě při blitzu (N1).
- Faul: AI fauluje, člověk v partii ani jednou; 16 turnoverů AI přišlo z faulu.

## Příčiny (06.10. večer) — diagnostika „proč tohle makro“

Do `MacroMCTSPolicy` přibyl záznam posledního rozhodnutí (`lastDecision()`: makro, kdo rozhodl — klec / hledání / záchrana K6 /
nouzové greedy, děti kořene s návštěvami, priorem a Q); vazba `ai_plan_turn` ho přikládá ke kroku plánu jako klíč `macro`.
Kontrola, že diagnostika nemění hru: stejná semínka ⇒ stejné turnovery (trpaslíci 97, elfové 178).

### A. Odkud jsou úhyby na 4+ a horší (P149)

| makro (rozhodlo hledání) | elfové | trpaslíci |
|---|---|---|
| BLITZ | 104 | 50 |
| REPOSITION (+ záchrana K6) | 82 + 9 | 41 + 7 |
| PICKUP | 3 | 11 |
| BLITZ_AND_SCORE | 0 | 8 |

- Jsou to úhyby **do** zón: elfové BLITZ do 3 zón 69×, do 2 zón 35×; REPOSITION do 3 zón 63×, do 4 zón 14×. Trpaslíci BLITZ do 1 zóny 35×.
- Turnovery podle makra: elfové BLITZ 39 %, REPOSITION 31 + 6 %, BLOCK 10 %; trpaslíci BLITZ 29 %, REPOSITION 23 + 5 %, BLOCK 21 %.
- Riskantní makro je **prvním rozhodnutím tahu** v 87 ze 174 případů (elfové) a ve 32 z 93 (trpaslíci).
- Jak ho hledání vidělo (50 iterací, průměrně 8–10 dětí kořene):

| | Q vybraného riskantního | Q nejlepšího jiného | Q END_TURN |
|---|---|---|---|
| trpaslíci (n = 93) | +0,475 | +0,481 | +0,402 |
| elfové (n = 174) | −0,379 | −0,377 | −0,497 |

- Vybrané makro mělo méně než polovinu návštěv ve 254 z 267 případů; dítě s vyšším Q existovalo ve 154.
- ⇒ Dvě příčiny: (1) nabídka BLITZ a REPOSITION obsahuje cesty přes úhyby do 2–4 zón bez stropu rizika;
  (2) hledání neúspěch nepočítá jako ztrátu — „skončit tah“ hodnotí hůř než riskantní akci, protože ztracené aktivace zbytku týmu v ohodnocení nejsou,
  a při 50 iteracích se děti liší o tisíciny, takže rozhoduje prior.

### B. Proč tah končí s nepohnutými hráči (P150)

- Dobrovolných konců tahu u trpaslíků 148, z toho 73 s pěti a více neaktivovanými hráči (39× s devíti). U elfů 9 ze 132.
- Kdo rozhoduje (počet rozhodnutí ve 20 plánech): tahy 1, 3, 26 — klec 88 / 100 / 100; tahy 5, 7, 9, 28 — klec 0, vše hledání.
- Tah 5 s `BB_CAGE_DEBUG=1`: `[cage DICEY] leg 0/4 player=8 gfi=0 pto=0.938 ceil=0.020 target=(20,7)` — plánovač klece plán zahodil
  (první roh by měl 94 % na turnover; cíl je 7 polí před nosičem). Tahy 9 a 28: plánovač odmítl beze stopy ve výpisu.
- Po odmítnutí žádný náhradní postup není. Hledání pak typicky zahraje `ADVANCE nosič > END_TURN` (tah 5), `END_TURN` (tah 9), `BLITZ > END_TURN` (tah 28).
- END_TURN vyhrává těsně: Q +0,671 proti +0,660 u nejlepšího jiného (lepší v 51 z 66), ač v nabídce byl REPOSITION (60 z 66), BLOCK (26), BLITZ (25).
- ⇒ Otevřené: proč plánovač míří rohem na (20,7) a proč v tazích 9 a 28 mlčí. Teprve pak návrh (menší krok klece, přeskupení rohů podle P143).

### C. Klec po fázích na dnešním `main` (P154)

Uživatel 06.10.: *„měla by se pohybovat celá klec bezpečně kupředu už s aktuálními změnami — nebo se pletu?“*
40 prvních poločasů AI × AI (trpaslíci přijímají, macro_mcts 50 iterací), po tahu trpaslíků; TD trpaslíků 8 ze 40.

| fáze | tahů | turnover | s míčem po tahu | 4 rohy | 0–1 roh | rohy průměr | postup nosiče |
|---|---|---|---|---|---|---|---|
| 1 zvednutí (míč volný) | 54 | 35 = 65 % | 34 | 14 | 9 | 2,53 | — |
| 2 klec — řadič rozhodl | 95 | 59 = 62 % | 91 | 22 | 50 | 1,77 | 2,69 pole |
| s míčem, řadič nerozhodl nic | 48 | 18 = 38 % | 45 (+ 8 TD) | 2 | 37 | 0,89 | 3,36 pole |

- Hody uvnitř maker, která řídí klec (mají být bez hodu kromě zvednutí): přesun rohu — úhyb na 4+ a horší 35×, na 2–3+ 8×;
  zvednutí — 9× na 4+ a horší, 33× na 2–3+, cestou k míči 7 úhybů a 2 GFI.
- Turnover ve fázi 2: 19× uvnitř makra klece (18× přesun rohu), 40× v makru hledání po kleci (blitz 10, přesun 8 + 4, faul 7 + 2, blok 6).
- Stopa řadiče na pozicích z partie: tahy 5 a 7 — největší krok (6) vyšel DICEY a plán se zahodil celý; tahy 9, 11, 28 — cíl tahu SCORE_BALL, řadič ustoupil.
- ⇒ Záměr „klec jde bezpečně kupředu“ v kódu je, ale neplní se: čtyři rohy má klec po tahu jen ve 22 z 91 tahů, které řadič řídil.
- Výhrady: soupeřem je táž AI (elfové), 50 iterací, jedna dvojice ras.

## Oprava P149 bod 1: strop na úhyb (06.10. večer)

Schváleno uživatelem („1 ano“): makra BLITZ a REPOSITION nevedou přes úhyb na 4+ a horší; výjimky — poslední aktivace tahu,
míč v posledním kole poločasu; skórující makra strop nemají. Přesun se zastaví před rizikovým krokem (hráč zůstane, kam bezpečně došel),
u blitzu se bere nejbezpečnější blitzující, jehož doběh přes takový úhyb nevede (jinak se blitz na ten cíl nehraje).

**Párově na pozicích z partie** (engine `4e142640` + oprava, stejná semínka, 160 plánů na řádek):

| | turnover | úhybů 4+ a horších na tah | z toho BLITZ / REPOSITION | aktivováno hráčů | blitz využit | TD | s míčem po tahu |
|---|---|---|---|---|---|---|---|
| elfové, bez stropu | 56 % | 0,61 | 54 / 42 | 5,2 | 97 % | 0 | 31 % |
| elfové, se stropem | 28 % | 0,00 | 0 / 0 | 6,7 | 97 % | 0 | 32 % |
| trpaslíci, bez stropu | 29 % | 0,37 | 26 / 21 | 4,4 | 72 % | 38 | 71 % |
| trpaslíci, se stropem | 16 % | 0,11 | 0 / 0 | 5,1 | 73 % | 39 | 70 % |

Zbylé úhyby na 4+ a horší u trpaslíků: PICKUP 13 (mimo schválený rozsah) a skórující makra 4 (výjimka).

**Celé první poločasy na dnešním `main`** (AI × AI, 40 poločasů na rameno, stejná výchozí semínka — po první odlišné akci už hry párové nejsou):

| | TD trpaslíci | TD elfové | turnover trpaslíci | turnover elfové | úhybů 4+ a horších na tah (trp / elf) | aktivováno (trp / elf) |
|---|---|---|---|---|---|---|
| bez stropu | 8 / 40 | 18 / 40 | 55 % | 59 % | 0,49 / 0,49 | 5,5 / 4,4 |
| se stropem | 20 / 40 | 12 / 40 | 28 % | 44 % | 0,07 / 0,06 | 6,7 / 5,6 |

- Cena: plán tahu trvá déle (pozice z partie 175 → 292 s, celé poločasy 324 → 429 s). Rozhodnutí je o 29 % víc, protože tah nekončí předčasně;
  zbytek je cena kontroly cesty blitzu a „záchranných“ voleb (hledání vybere makro, které strop zastaví hned — 37 → 138 případů na 160 plánů). Otevřené k optimalizaci.
- ⛔ První verze stropu u blitzu nefungovala: opřela se o `estimateApproachFailChance`, který oceňuje starou cestu (`pickApproachStep`),
  kdežto hra od 09.09. chodí přes `nextStepTowardAdjacent`. Runner 5 v tahu 5 dál blitzoval přes úhyby 5+, 4+, 4+. Opraveno dotazem na tutéž cestu
  (`worstDodgeOnBlitzApproach`); samotné ocenění blitzujícího měří jinou cestu dál ⇒ kniha **P155**.
- Výhrady: jedna dvojice ras, 50 iterací, soupeřem táž AI; rozdíl v TD (8 → 20) je z nepárových her.

**Po review a rozšíření na zvedání míče (konečná verze, tatáž semínka, 160 plánů na řádek):**

| | turnover | úhybů 4+ a horších na tah | BLITZ / REPOSITION / PICKUP / skórující | aktivováno | blitz využit | TD |
|---|---|---|---|---|---|---|
| elfové, bez stropu | 56 % | 0,61 | 54 / 42 / 1 / 0 | 5,2 | 97 % | 0 |
| elfové, se stropem | 26 % | 0,02 | 0 / 3 / 0 / 0 | 6,7 | 96 % | 0 |
| trpaslíci, bez stropu | 29 % | 0,37 | 26 / 21 / 8 / 4 | 4,4 | 72 % | 38 |
| trpaslíci, se stropem | 16 % | 0,06 | 0 / 0 / 5 / 5 | 5,2 | 68 % | 39 |

- Zbylé úhyby jsou výjimky: 8. kolo a míč (PICKUP 5, REPOSITION 2), poslední aktivace tahu (1), skórující makra (5).
- Review (oddělený průchod) našlo 2 vysoké vady první verze: kontrola cesty blitzu četla jedno hledání místo smyčky hry
  (337 úniků z 30 000 náhodných pozic — Break Tackle, reroll Dodge) a výjimka „poslední aktivace“ nenastala po přesunu spoluhráče. Obojí opraveno a hlídáno testy.
- Cena konečné verze: plán tahu na pozicích z partie 184 → 392 s (kopie stavu a hledání na každý krok doběhu blitzu) ⇒ kniha P157 (f).


## Oprava P149 bod 2: cena turnoveru v hledání (06.10. noc)

Schváleno uživatelem („2 hned po bodu 1“, „pokračuj bodem 2“). Listový odhad hledání přičítá straně na tahu hodnotu každé aktivace,
kterou v tomto tahu ještě má; turnover i END_TURN o ni přijdou tím, že tah skončí. Zapnuto ve výchozím stavu.

**Hodnota jedné aktivace je změřená, ne zvolená:** průměrný zisk listového odhadu na jedno úspěšné makro mimo zvednutí míče,
vlastní hry AI, 3 dvojice ras (dwarf × wood-elf, human × orc, skaven × dwarf), n = 1 632 ⇒ **+0,024**
(REPOSITION +0,014, BLOCK +0,023, BLITZ +0,065, ADVANCE +0,039; zvednutí míče +0,271 se nepočítá). Makro zakončené turnoverem: průměr −0,048.

**Párově na pozicích z partie** (strop na úhyb zapnutý v obou ramenech):

| | iterací | plánů | turnover | před turnoverem hrálo | aktivováno | konec tahu s ≥ 5 nehrajícími | první rána jako kolikátá aktivace | TD |
|---|---|---|---|---|---|---|---|---|
| trpaslíci, bez ceny | 50 | 160 | 16 % | 3,4 | 48 % | 30 z 96 | 2,2 | 39 |
| trpaslíci, s cenou | 50 | 160 | 12 % | 3,6 | 52 % | 18 ze 101 | 2,3 | 39 |
| elfové, bez ceny | 50 | 160 | 26 % | 3,7 | 70 % | 4 ze 119 | 2,5 | 0 |
| elfové, s cenou | 50 | 160 | 28 % | 4,0 | 70 % | 2 ze 116 | 2,6 | 0 |
| trpaslíci, bez ceny | 200 | 48 | 21 % | 3,6 | 51 % | 4 z 26 | 2,2 | 12 |
| trpaslíci, s cenou | 200 | 48 | 12 % | 5,5 | 58 % | 4 ze 30 | 2,5 | 12 |
| elfové, bez ceny | 200 | 48 | 31 % | 3,7 | 69 % | 1 ze 33 | 2,5 | 0 |
| elfové, s cenou | 200 | 48 | 29 % | 4,1 | 71 % | 0 ze 34 | 2,4 | 0 |

**Celé první poločasy na `main`** (40 + 40, nepárové, strop zapnutý v obou): turnover trpaslíci 33 → 29 %, elfové 47 → 47 %; TD trpaslíci 17 → 18, elfové 9 → 10.

- Účinek je malý: u trpaslíků ubylo předčasných konců tahu (30 → 18) a turnoverů (16 → 12 %), u elfů se nezměnilo nic měřitelného.
- **Pořadí „bezpečné napřed, riskantní nakonec“ cena nezařídila** — první rána padá pořád jako 2.–3. aktivace. Důvod: hledání porovnává stav po JEDNOM makru.
  Blitz (zisk +0,065) i po odečtení ceny rizika vychází líp než bezpečný přesun (+0,014), a že jde zahrát obojí v lepším pořadí, vidí až hledání do hloubky 2,
  na kterou 50 ani 200 iterací při 8–10 dětech kořene nestačí. Pořadí je práce pro odklad riskantních akcí (kniha P159) nebo plán celého tahu.
- Vedlejší nález: test `K6StillEndsTheTurnWhenNothingCanBeDone` stojí na hodnotové funkci, která dává listu skoro přesně strop odhadu 1,0; s cenou se rozdíl mezi dětmi ztratil
  a hledání zvolilo END_TURN rovnou (správná akce, jiná cesta). Test běží bez ceny, důvod je v něm zapsaný.

## Oprava klece P154 (06.10. noc)

Uživatel: *„opravu klece proveď — měl jsem skupinu úkolů okolo klece za dořešenou“*.

| vada | oprava |
|---|---|
| K1 přesuny rohů házely úhyb | zastavuje strop na úhyb (P149) |
| K2 po riskantním nejdelším kroku se zahodil celý plán | plánovač zkouší kratší krok (ne do zóny soupeře) |
| K3 v dosahu TD řadič ustoupil a hledání neskórovalo | když nosič do zóny bez hodu nedojde a není poslední kolo, tah vede klec |
| K2 + K3 tah končil s nepohnutými rohy | než hledání ukončí tah, řadič dotáhne klec (postup, nebo rohy kolem nosiče) |
| K4 zvedač se vybíral jen podle cesty | do rizika se počítá i hod na zvednutí (Sure Hands = přehoz) |

**Pozice z partie** (trpasličí tahy s míčem, 20 plánů na tah):

| tah | uživatel: do TD, rohy | AI ráno: do TD · rohy | AI po opravě: do TD · rohy · 4 rohy |
|---|---|---|---|
| 5 | 10, 4 | 10,8 · 1,2 | 10,0 · 3,90 · 18/20 |
| 7 | 8, 3 | 9,1 · 0,25 | 8,4 · 2,70 · 13/20 |
| 9 | 6, 4 | 7,8 · 2,6 | 8,0 · 3,90 · 18/20 |
| 28 | 4, 4 | 6,2 · 1,1 | 4,0 · 4,00 · 20/20 |

Celkem 4 rohy po tahu ve 124 ze 138 plánů s míčem, turnover 19 z 200.

**Celé první poločasy na `main`** (40, AI × AI, trpaslíci přijímají; hry mezi řádky nejsou párové):

| stav | TD trpaslíků | tah se zvednutím: turnover | tahy řízené klecí: turnover | z toho uvnitř maker klece |
|---|---|---|---|---|
| ráno 06.10. | 8 | 65 % | 62 % | 19 |
| + strop na úhyb + cena turnoveru | 18 | — | — | — |
| + oprava klece | 22 | 33 % | 30 % | 4 |

**Co zůstává špatně:** proti soupeři-AI klec po tahu většinou nestojí. Postup klece vyšel ve 48 tazích (4 rohy 19 ze 47),
v 79 tazích jen dostavění nebo bloky — a hledání pak nosiče odvedlo o 2,8 pole (průměrně 1,04 rohu). Po zvednutí míče 4 rohy ve 12 ze 41.

**Zkoušeno a vráceno:** držet nosiče v kleci, když postup nevyjde. TD trpaslíků 22 → 12 ze 40, rohy jen 1,04 → 1,40 — klec se nedostaví a nosič stojí. Nenasazeno.

## Co zkouška neumí

- 16 skutečných tahů na stranu je malý vzorek; „1 turnover z 16“ je hrubé číslo. Opírat se dá o rozdíly v obtížnosti úhybů a v počtu aktivací.
- Pozice jsou z partie člověka proti člověku (uspořádané klece, málo ležících) — AI se do nich sama dostává zřídka.
- Starý engine (`4e142640`), 50 iterací MCTS.
- „Kontakt bez převahy“ a „dosah na nosiče“ počítá zjednodušený nástroj `bb_kontrola_tahu.py` (jeho výhrady jsou v hlavičce souboru).

## Návrhy oprav AI — k představení, nic neimplementováno

1. **N1:** riskantní akce (úhyb 4+ a horší, blitz přes zóny) až po bezpečných aktivacích, a strop na riziko cesty, když neúspěch stojí zbytek tahu (P149).
2. **N2:** po pohybu nosiče dotáhnout rohy klece v témže tahu; zjistit, proč AI volí END_TURN s 8–9 nepohnutými hráči (P150).
3. **N3:** rozhodnout, zda má AI zdržovat TD do posledního kola (P151).
