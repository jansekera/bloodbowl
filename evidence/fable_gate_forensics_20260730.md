# Forenzní audit gate historie + power analýza + retro-judging (30.07.2026)

Autor: Fable 5 (forenzní datový analytik). Pouze čtení a výpočty; žádný trénink, žádné změny mimo tento soubor.

Centrální otázka: **Učíme se doopravdy — a umí to gate vůbec detekovat?**

## Mechanismus gate (ověřeno v kódu, ne z logů)

- Decisive-only skóre: `chess_score = wins / decisive`, remízy vyřazeny — `run_iteration.py:653-654`.
- `sigma = 0.5 / sqrt(decisive)` — `run_iteration.py:693`; práh `required = 0.5 + k*sigma` — `run_iteration.py:713`.
- Tiery k: 1.0 / 1.5 / 2.0 (`GATE_SIGMA_IMPROVED/SAME/DROPPED`) — `run_iteration.py:118-120`; výběr tieru podle benchmark-vs-best — `run_iteration.py:695-705`; pokles >5 % = HARD-REJECT — `run_iteration.py:707-710`.
- Benchmark je proti **random** soupeři: `home_ai, away_ai = (('random','macro_mcts') …)` — `run_iteration.py:270-271`.
- **Reset-on-reject**: při REJECTED se `weights_best.json` přepíše zpět frozen šampionem — `run_iteration.py:741`; a **každá iterace seeduje value trénink z weights_best** — `run_iteration.py:472`. Zamítnuté value zisky se tedy skutečně zahazují.
- **Výjimka: policy hlava se NEresetuje.** `_carry_over_policy` (`run_iteration.py:474`, tělo 342-376) + `_stash_policy` (`run_iteration.py:512`) přenášejí policy hlavu přes `weights_policy.json` do další iterace bez ohledu na verdikt gate. Akumulace tedy existuje jen pro policy, ne pro value.
- Gate N=600 (`GATING_MATCHES`, `run_iteration.py:53`), selection H2H N=150 (`run_iteration.py:62`).

---

## Úkol 1 — Tabulka všech dohledatelných gate verdiktů

Zdroje: `gate_history.jsonl` (3 záznamy), `training_gatefix_20260722.log`, `item1_vfblend_full_20260728.log`, `smoke_item1_vfblend_20260728.log`, `smoke_item2_mcts400_20260729.log`, `betarun_full_20260630.log`, `leverb_full_20260629.log`, `training_postfixes_20260716.log`, `training_postitem7_20260720.log` (4 iterace), `training_mc_td_mix_stage1_20260713.log`, `training_post_stepcap_fix_20260708.log`, `training_post_expandscore_fix_20260709.log`, `evidence/training_run_loss_and_gates.txt` (běh 23.06, éra best=89 %). `training_watch_20260721.log` je duplikát postitem7 it4 (identická čísla) — vyřazen.

Sloupce: cs = decisive win-rate; z_práh = (cs−práh)/σ; z_50 = (cs−0.5)/σ; bm = benchmark-vs-random kandidáta.

| Běh | W/D/L | dec | cs | σ | k | práh | z_práh | z_50 | draw | bm | klasifikace |
|---|---|---|---|---|---|---|---|---|---|---|---|
| full 23.06 it1 | 77/444/79 | 156 | 49,4 % | 4,0 % | 2,0 | 58,0 % | −2,16 | −0,16 | 74 % | 85,0 | šum kolem mince |
| full 23.06 it2 | 66/446/88 | 154 | 42,9 % | 4,0 % | 1,5 | 56,0 % | −3,27 | −1,77 | 74 % | 87,5 | **decisive fail** |
| full 23.06 it3 | 73/434/93 | 166 | 44,0 % | 3,9 % | 2,0 | 57,8 % | −3,55 | −1,55 | 72 % | 85,5 | **decisive fail** |
| leverb 29.06 | 114/372/109 | 223 | 51,1 % | 3,3 % | — | HARD-REJ | — | +0,33 | 63 % | 85,4 | HARD-REJECT (bm propadl) |
| betarun 30.06 | 112/362/121 | 233 | 48,1 % | 3,3 % | 1,5 | 54,9 % | −2,09 | −0,59 | 61 % | 90,5 | šum kolem mince |
| stepcap 08.07 | 128/340/117 | 245 | 52,2 % | 3,2 % | ABORT | — | — | +0,70 | 58 % | — | ABORT (bm incomplete) |
| expandscore 09.07 | 118/367/113 | 231 | 51,1 % | 3,3 % | ABORT | — | — | +0,33 | 61 % | — | ABORT (bm incomplete) |
| mc_td_mix 13.07 | 119/356/125 | 244 | 48,8 % | 3,2 % | 2,0 | 56,4 % | −2,38 | −0,38 | 59 % | 88,0 | šum kolem mince |
| postfixes 16.07 | 155/289/156 | 311 | 49,8 % | 2,8 % | 1,0 | 52,8 % | −1,06 | −0,06 | 48 % | 95,5 | šum kolem mince |
| postitem7 it1 20.07 | 143/306/151 | 294 | 48,6 % | 2,9 % | 1,0 | 52,9 % | −1,47 | −0,47 | 51 % | 95,5 | šum kolem mince |
| postitem7 it2 20.07 | 161/269/170 | 331 | 48,6 % | 2,7 % | 1,0 | 52,7 % | −1,49 | −0,49 | 45 % | 92,5 | šum kolem mince |
| postitem7 it3 20.07 | 161/292/147 | 308 | **52,3 %** | 2,8 % | 1,0 | 52,8 % | **−0,20** | +0,80 | 49 % | 95,0 | **close miss** |
| postitem7 it4 21.07 | 158/286/156 | 314 | 50,3 % | 2,8 % | 1,0 | 52,8 % | −0,89 | +0,11 | 48 % | 96,0 | **close miss** |
| gatefix it1 22.07 | 139/278/183 | 322 | 43,2 % | 2,8 % | 1,0 | 52,8 % | −3,45 | −2,45 | 46 % | 96,0 | **decisive fail** |
| gatefix it2 22.07 | 136/283/181 | 317 | 42,9 % | 2,8 % | 1,0 | 52,8 % | −3,53 | −2,53 | 47 % | 96,5 | **decisive fail** |
| gatefix it3 22.07 | 145/313/142 | 287 | 50,5 % | 3,0 % | 1,0 | 53,0 % | −0,82 | +0,18 | 52 % | 94,0 | **close miss** |
| gatefix it4 22.07 | 141/292/167 | 308 | 45,8 % | 2,8 % | 1,0 | 52,8 % | −2,48 | −1,48 | 49 % | 94,5 | **decisive fail** |
| item1 smoke 28.07 | 28/39/33 | 61 | 45,9 % | 6,4 % | 1,0 | 56,4 % | −1,64 | −0,64 | 39 % | 96,0 | šum kolem mince |
| item1 full 28.07 | 179/228/193 | 372 | 48,1 % | 2,6 % | 1,0 | 52,6 % | −1,73 | −0,73 | 38 % | 99,5 | šum kolem mince |
| item2 smoke 29.07 | 36/29/35 | 71 | 50,7 % | 5,9 % | 1,0 | 55,9 % | −0,88 | +0,12 | 29 % | 100,0 | **close miss** |

Klasifikace (17 skutečných gate rozhodnutí, bez 2 ABORTů a 1 HARD-REJECTu):
- **close miss** (do 1σ pod prahem, cs ≥ 50 %): **4** (postitem7 it3, it4; gatefix it3; item2 smoke)
- **decisive fail** (≥ 1σ pod 50 %): **5**
- šum kolem mince (mezi tím, většinou lehce pod 50 %): **8**

**Odpověď na otázku 1: převažuje ani jedno — rozdělení je centrované LEHCE POD 50 %.** Pooled přes všech 20 běhů: 2029 W / 4249 decisive = **47,8 %, Wilson CI95 [46,3 %, 49,3 %]** — horní mez pod 50 %. Pooled jen éra best=91,5 % s k=1.0 (12 běhů): 48,0 % [46,3 %, 49,7 %]. To hypotézu „zahazujeme konzistentní malé reálné zisky" v agregátu **nepodporuje**: kdyby kandidáti byli systematicky o +1-2 pp lepší, pooled by ležel nad 50 %, ne pod. Kaveat: běhy jsou heterogenní (různé kódové změny, pooling předpokládá společný efekt) a poslední běhy trendují vzhůru (item2 mcts400: 50,7 % = nejlepší decisive skóre éry 91,5 % po postitem7 it3). Jednotlivý reálně lepší kandidát (např. postitem7 it3, −0,20σ od prahu) mohl být zahozen — ale data neukazují, že se to děje opakovaně.

---

## Úkol 2 — Power analýza (binomický model, k=1.0)

Model: decisive her D = 0,6·N (draw ~40 %); gate projde když W/D ≥ 0,5 + 0,5/√D; skutečná decisive win-rate p = 0,5 + δ. Exaktní binomické výpočty.

| N her | decisive | práh | δ s 50% šancí projít | δ s 80% šancí | P(pass\|+1pp) | P(pass\|+2pp) | P(pass\|+3pp) |
|---|---|---|---|---|---|---|---|
| 100 | 60 | 56,5 % | **+5,8 pp** | **+11,1 pp** | 22,7 % | 27,7 % | 33,1 % |
| 600 | 360 | 52,6 % | **+2,6 pp** | **+4,8 pp** | 26,7 % | 40,4 % | 55,5 % |
| 1200 | 720 | 51,9 % | **+1,9 pp** | **+3,4 pp** | 31,9 % | 52,7 % | 72,7 % |

Obráceně — N pro 80% šanci detekce (k=1.0, draw 40 %):

| skutečné zlepšení | decisive her | celkem her | ~dní gatingu (600 her/gate) |
|---|---|---|---|
| +1 pp | ~8 500 | **~14 100** | ~24 gate běhů |
| +2 pp | ~2 100 | **~3 500** | ~6 gate běhů |
| +3 pp | ~940 | **~1 570** | ~3 gate běhy |

False-positive rate gate při p=0,5 (nulový kandidát): ~16-18 % pro všechna N — k=1.0 je jednostranný test na hladině α≈0,16.

**Závěr: při N=600 je gate slepý na cokoli pod ~+3 pp** (šance projít ≤ 55 %) a smoke N=100 je slepý na cokoli pod ~+6 pp (medián). Zlepšení +1 pp je při současném N prakticky nedetekovatelné (27 % šance na běh — a protože value se při rejectu resetuje (`run_iteration.py:472,741`), nezbývá nic, co by se mezi pokusy sčítalo; každý pokus hází stejnou mincí znovu).

**Srovnání s AlphaZero praxí:** AlphaGo Zero evaluator = 55 % z 400 her — na pohled PŘÍSNĚJŠÍ práh než náš (52,6 % z 600). Zásadní rozdíl není v prahu, ale v tom, co se děje při zamítnutí: (1) self-play data se generují dál nejlepší sítí, ale **trénink pokračuje z aktuálních parametrů sítě** — optimalizační trajektorie se nikdy neresetuje, takže malé zisky se akumulují přes libovolně mnoho zamítnutí a gate jen rozhoduje, kdo generuje data; AlphaZero (2017) gate úplně zrušil. (2) U nás se value síť při každé iteraci znovu seeduje z frozen šampiona (`run_iteration.py:472`) — zamítnutí = skartace. Náš systém tedy kombinuje **nejhorší ze dvou světů: přísný práh + plný reset**. Jediný akumulační kanál je policy carry-over (`run_iteration.py:474,512`).

---

## Úkol 3 — Retro-judging pod alternativními pravidly

(a) chess_score s remízou=0.5 proti 0,5 + 1σ_chess (korektní σ nuly pro skóre {0, ½, 1}: σ = 0,5·√((1−d)/N)); (a2) totéž s naivní σ = 0,5/√N; (b) fixní práh 52 % decisive; (c) k=0,5 na decisive.

| Běh | (a) chess+1σ | (a2) chess naiv. | (b) 52 % dec | (c) k=0,5 |
|---|---|---|---|---|
| stepcap 08.07 (ABORT) | rej | rej | **PASS** | **PASS** |
| postitem7 it3 20.07 | rej | rej | **PASS** | **PASS** |
| všech ostatních 18 | rej | rej | rej | rej |

- Pod pravidlem **(a) i (a2)** neprojde **nic** — chess_score s remízami je stlačený k 0,5 (max 51,2 % u postitem7 it3) a žádný běh nepřekročí ani 1σ. Půl-bodové remízy gate nezmírní, naopak.
- Pod **(b) 52 % decisive** a **(c) k=0,5** projdou jen **postitem7 it3 (52,3 %)** a **stepcap 08.07 (52,2 % — ten ale skončil ABORTem kvůli nedoběhlému benchmarku, gate rozhodnutí nikdy nepadlo)**.
- Čili: ani výrazně měkčí pravidla nezmění obraz — 1 promoce za ~5 týdnů místo 0. Problém není primárně kalibrace prahu, ale to, že kandidáti reálně nejsou lepší (pooled 47,8 %).

---

## Úkol 4 — Selection audit (az_train vs train_best, 150 her)

| Běh | az_train W | train_best W | D | decisive | az podíl | Wilson CI95 |
|---|---|---|---|---|---|---|
| item1 smoke 28.07 | 49 | 46 | 55 | 95 | 51,6 % | [41,7 %, 61,4 %] |
| item1 full 28.07 | 41 | 46 | 63 | 87 | 47,1 % | [37,0 %, 57,5 %] |
| item2 smoke 29.07 | 44 | 47 | 59 | 91 | 48,4 % | [38,4 %, 58,5 %] |
| item3.5 diag 24.07 | 45 | 38 | 67 | 83 | 54,2 % | [43,5 %, 64,5 %] |

**Všechny 4 CI mají šířku ~20-21 pp a všechny obsahují 50 %.** Rozdíl vítěze je 3-7 her ze ~150 — výběr kandidáta je fakticky hod mincí. Aby selection H2H spolehlivě (80 %) rozlišila +5 pp rozdíl, potřebovala by ~620 decisive her (~1000 celkem), tj. 7× víc. Praktický dopad je ale omezený: oba kandidáti pocházejí ze stejného tréninku a následný gate je stejně skoro jistě zamítne; selection šum jen přidává ~1 bit náhody do toho, KTERÝ ~50% kandidát jde do gate. (Mechanismus: `run_iteration.py:529-611`, `SELECTION_H2H_MATCHES=150` na řádku 62.)

---

## Úkol 5 — Remízy a degenerovaný tiering

**Remízy (29-74 % her, v současné éře 29-52 %):**
- Kolik signálu se zahazuje? **Pod modelem „remíza nenese informaci o síle" NIC** — matematicky: z-skóre efektu je identické pro decisive-only i půl-bodové skórování (σ_chess = (1−d)·σ_dec, ale efekt se škáluje stejným faktorem). Retro-judging (a) to potvrzuje empiricky: půl-bodové pravidlo nepustí vůbec nic.
- Zahazuje se ale **druhý moment**: pokud se zlepšení projevuje konverzí proher na remízy (defenzivní zisk), decisive-only ho z principu nevidí. A draw-rate se DRAMATICKY hýbe: 72-74 % (23.06) → 58-63 % (červenec začátek) → 38-48 % (16.-28.07) → **29 % (item2, mcts400)**. Tento trend je reálný, měřitelný signál o změně chování systému, který žádná gate metrika nesleduje. Doporučení: logovat draw-rate kandidáta vs frozen jako sekundární metriku (bez rozhodovací role), nikoli měnit skórování.

**Tiering (k=1.0/1.5/2.0):**
- Od 16.07 padlo všech **12 verdiktů v tieru „benchmark zlepšen" (k=1.0)** — benchmark-vs-random kandidáta je 92,5-100 % a all-time-best je zamrzlý na 91,5 %, takže podmínka `new_bm >= all_time_best_bm` (`run_iteration.py:698-699`) je prakticky vždy splněna. Tiery 1.5/2.0 naposledy vystřelily 13.07 (mc_td_mix, záměrně riskantní experiment) a v éře best=89 %.
- Trojúrovňový design tedy **není rozbitý, ale je degenerovaný**: benchmark proti random soupeři saturuje (96-100 %), takže tier nese ~0 bitů — gate je fakticky jednoprahový test na k=1.0. Funkci si zachovává jen jako pojistka proti katastrofické regresi (HARD-REJECT chytil leverb 29.06 při bm 85,4 %). Pozn.: all_time_best se aktualizuje jen při promoci (`run_iteration.py:735-738`), takže saturovaný benchmark ani nemůže laťku zvýšit — degenerace je trvalá, dokud nikdo neprojde.

---

## Celkový verdikt

1. **Gate strukturálně NEMŮŽE vidět malá zlepšení** (+1 pp ≈ 27 % šance, +2 pp ≈ 40 % při N=600) a reset-on-reject (`run_iteration.py:472,741`) zaručuje, že se nedetekované zisky u value sítě nekumulují — hypotéza z 29.07 je mechanisticky správně.
2. **ALE forenzní data neukazují, že by tam malé zisky konzistentně byly:** pooled decisive win-rate 47,8 % [46,3; 49,3] je pod 50 %; close missy jsou 4 ze 17 a nepřevažují. Nejpravděpodobnější čtení: kandidáti jsou zatím ≈ stejně dobří nebo o chlup horší než frozen — konzistentní s diagnózou policy plateau (17.07) a value-blind self-play.
3. Prakticky: pokud chceme umět DETEKOVAT +1-2 pp, potřebujeme buď ~3 500-14 000 her na verdikt, nebo akumulační design (netrénovat každou iteraci znovu z frozen), nebo sekvenční test (SPRT) přes více iterací stejného kandidáta. Změkčení prahu samo o sobě (retro-judging) by za 5 týdnů pustilo 1 kandidáta navíc.
