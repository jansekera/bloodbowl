# P205 — zkouška „AI plánuje tah“ na obou odehraných partiích (14.10.2026)

Engine: značka `zkouska-ai-plan-20261013-baze` (`5d7df49d`; main se od ní liší jen knihou úkolů).
V každé pozici na začátku tahu 20 plánů `macro_mcts` (50 iterací, `weights_best.json`) a 20 plánů
`greedy`, každý s vlastními kostkami (semínko 1000·tah + s). Srovnává se **plán tahu**, ne výsledek
partie — jakmile AI zahraje jinak, kostky se rozejdou.

Skripty: `bb_zkouska_ai_plan_20261013.py` (část A), `bb_zkouska_ai_plan_20261013_b.py` (část B).
Tabulka tahu po tahu části A: `tabulka_tahu.md`; jednotlivé tahy `tah_NN_*.md` a `souhrn.json`
(mimo git, jako 06.10.).

## Kontroly měřidla (napřed)

| kontrola | výsledek |
|---|---|
| A: přehrání logu lidé × orkové | 1:1, 32 tahů, TD lidí 1. pol. kolo 5, orků 2. pol. kolo 7 ✅ |
| A: odmítnuté řádky logu | 1 — ř. 818 `actions HOME` při čekající otázce kouče; server ho tehdy odmítl stejně |
| A: pozitivní kontrola (vynechán první `odp`) | přehrání se rozešlo: 0:0, 17 tahů, 745 odmítnutých řádků ✅ |
| B: pozice ze starého enginu (`4e142640`) složená na dnešním | 32/32 pozic bez neshody, i skilly (podle jména) ✅ |
| B: pozitivní kontrola (posunutý hráč) | neshoda nalezena ✅ |
| přenos zkoušený na části A (přehraná × složená pozice) | nabídka akcí shodná 32/32; posunutý hráč nabídku změní ✅ |
| `greedy` v části B 06.10. × dnes | turnovery 234/320 a 308/320 v obou během — greedy se od 06.10. neměnil, shoda potvrzuje, že přenesená pozice hraje stejně jako původní |

⚠️ Nepřenáší se (vazba je nezpřístupňuje): `stunnedThisTurn`, `lostTacklezones`, příznaky Big Guy,
`rooted`, `proUsedThisTurn`, `stoodUpNextToEnemy`, lékárník. Na začátku tahu trpaslíků a elfů na plán
jednoho tahu bez vlivu (kromě lékárníka, který plán tahu nemění).
⚠️ Neudělán χ² férovosti kostek plánů — události plánů se neukládají (jen plán semínka 1 v `tah_NN_*.md`);
semínka jsou různá pro každý tah i plán.

## Část B — co se od 06.10. změnilo (stejné pozice, dnešní engine)

| strana | AI | turnover 06.10. | turnover dnes | z toho na úhybu | turnover hned první akcí | hrálo jiných hráčů před turnoverem |
|---|---|---|---|---|---|---|
| trpaslíci | macro_mcts | 97/320 = 30 % | **63/320 = 20 %** | 58 → **7** | 18 → 6 | 3,5 → 5,3 |
| elfové | macro_mcts | 178/320 = 56 % | **86/320 = 27 %** | 126 → **32** | 56 → 12 | 3,2 → 4,4 |

- **P149 (úhyby 4+)**: turnoverů na úhybu je o 88 % (trpaslíci) a 75 % (elfové) méně. Z 30 % / 56 % je 20 % / 27 %.
- **P150 (tah končí po 1–4 aktivacích, klec nejde za nosičem)**: před turnoverem hraje víc hráčů
  (riziko je víc na konci tahu) a klec u nosiče je teď ve většině tahů se 4 rohy
  (tah 5: 1,2 → 4,0; tah 7: 0,25 → 4,0; tah 28: 1,1 → 4,0).
- **TD trpaslíků 75 → 51** je hlavně tah 30 (2. pol. kolo 7, vedou 1:0): 06.10.
  skórovali 20/20, dnes 0/20 — nosič se drží v kleci 4 pole od TD a skóruje se až v kole 8. **Totéž udělala
  skutečná partie** (v kole 7 zůstali 1 pole před TD, skórovali v kole 8). Dnešní AI tu tedy hraje jako
  partie a líp než 06.10. (soupeř nedostane kolo na vyrovnání) — účinek P187 „s časovou rezervou se
  nosič nevypouští“. ⚠️ Ale 7/20 plánů v tomto tahu skončí turnoverem, 6× na bloku: když se jen čeká
  na kolo 8, blok s rizikem lebky nic nepřidává.

## Část A — lidé × orkové, tah po tahu (vyhodnocení Claude)

Součty: skutečnost 1 turnover z 16 tahů za každou stranu; `macro_mcts` lidé 85/320 = 27 %, orkové
66/320 = 21 %; `greedy` 86 % a 98 % (greedy hraje do první chyby, jako srovnání slouží jen macro_mcts).

### AI hraje hůř než partie

1. **Tah 5 (lidé, 1. pol. kolo 3) — tempo.** Partie: blitz otevřel cestu, přihrávka throwera 7 na catchera 5
   a běh na 2 pole od TD, nikdo z orků na něj nedosáhne. AI: 13,9 pole do TD, klec se 4 rohy, nosič
   zůstal stát, 1 blok, konec tahu. Klec je bezpečná, ale za tři kola má lidský nosič stejně daleko
   jako na začátku. (P204 B: počítat pole × kola, přihrávka o kolo dřív.) ⭐ Nejcennější rozdíl partie.
2. **Tah 15 (lidé, 1. pol. kolo 8, poslední tah lidí v poločase, orkové mají míč 12 polí od TD) —
   riziko bez užitku.** Partie: postavit ležící, blitz, blok, konec. AI: 18/20 turnover, z toho 10× na
   úhybu — hráči se přesouvají přes úhyby (i 6+) na pozice, které po poločase nic neznamenají.
   Turnover tu soupeři skóre nedá, ale padající hráč se zraňuje. (P204 A: riziko na konec — tady riziko
   vůbec ne.)
3. **Tah 29 (orkové, 2. pol. kolo 7, prohrávají 0:1) — přihrávka bez vyčištění dráhy.** Partie: bloky
   na lidi 3 a 2 (oba v dráze přihrávky), blitz 22, pak přihrávka na blitzera 20 a doběh do TD. AI
   (plán semínka 1): blok jen na 3, pak přihrávka na 21 přes pole, kde stojí lidský hráč 2 — ten
   přihrávku zachytil. AI: TD 2/20, turnover 15/20 (7× nepovedená přihrávka, 2× zachycení).
   Pozor na kostky: přihrávka v partii potřebovala 5+ a vyšla — výsledek partie byl i štěstí; rozdíl
   je v **pořadí** (P204 A: napřed uvolnit dráhu a zrušit soupeřovu asistenci, přihrávka nakonec). → **celotah**.
4. **Tah 32 (lidé, poslední tah zápasu, 1:1, TD není dosažitelné) a tah 30 v části B** — bloky
   a blitzy bez možného zisku (10/20 turnover). Malá váha (po tahu už nic není), ale týž vzor jako
   tah 15: AI nerozlišuje, kdy riziko nemá protihodnotu.

### AI hraje líp než partie

5. **Tah 7 (lidé, 1. pol. kolo 4, catcher 5 na (22,11), 3 pole od TD, se skillem Dodge).** AI: skóruje hned (16/20 TD,
   jeden úhyb 3+ se skillem Dodge). Partie: zdržování — nosič na 1 pole od TD, v kole 5 ho už značil
   ork 19 s Tackle (Dodge neplatí) a TD vyšlo na úhyb 3+ bez přehozu. Zdržení dalo orkům o kolo míň, ale
   za cenu rizika, které AI nebere. Vyrovnané hodnocení; spíš AI (jistota před tím, než soupeř přivede Tackle).

### Jiné, nerozhodnuto

6. **Tah 1 (lidé, 1. pol. kolo 1)** — AI fauluje (4/20; 2 turnovery z vyloučení). V kole 1 se míč
   teprve zvedá; faul tu nic neurychlí. Spíš hůř, ale vzorek malý.
7. **Tah 30 (lidé, 2. pol. kolo 7 po vyrovnání, 15 polí do TD)** — partie i AI stejně (míč drží,
   15,8 pole do TD). Nikdo nezkusil dvoukolový útok (přihrávka v kole 8); nerozhodnuto.
8. **Obrana orků (tahy 2, 4, 6, 8)** — AI 3–7 turnoverů z 20 na úhybech a blocích, partie 0. Orkové
   bez míče ztrácejí turnoverem jen zbytek tahu; počet bloků AI a partie je podobný.

## Otázky z P204 nad konkrétními tahy

| P204 | tah | co ukazuje |
|---|---|---|
| A asistence / uvolnit dráhu před akcí | 29 | AI přihrává před bloky, které by dráhu uvolnily |
| A riziko na konec | 15, 32, B30 | AI riskuje, i když riziko nemá protihodnotu |
| B pole × kola, přihrávka o kolo dřív | 5 | AI nechá nosiče v kleci, partie posune míč o 12 polí |
| B zdržování | 7, B30 | AI v části B zdržuje jako partie (P187); v části A skóruje hned |
| C krýt přijímače / značit za linií | — | v plánech jednoho tahu se nedá posoudit (je to tah soupeře) |

## Návrhy úprav AI (jen k představení, nic se neimplementuje)

- **N1 (celotah, tahy 29 a 5):** před přihrávkou napřed bloky na hráče v dráze a kolem přijímače;
  přihrávka a běh až potom. V dnešním plánovači tahu je to pořadí makroakcí.
- **N2 (tahy 15, 32, B30):** riziko bez protihodnoty — v posledním tahu poločasu, když skóre nejde
  změnit, a při zdržování s míčem v kleci, nebrat úhyby a bloky s možností lebky. Pravidlo se dá
  přečíst z pozice (kolo, poločas, vzdálenost míče), nehledat ho v MCTS.
- **N3 (tah 5):** tempo útoku — když nosič v kole K ze 8 nemá šanci dojít do TD do konce poločasu
  pohybem klece, zvážit přihrávku dopředu. Souvisí s P204 B; měřit až na víc partiích.

⚠️ Pozorování z jedné ručně hrané partie (A) a jedné AI × AI partie (B), ne měření síly.
