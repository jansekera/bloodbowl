# FRONTA ÚKOLŮ — TRVALÁ KNIHA

**Tenhle soubor se NEPŘEPISUJE.** Položky se do něj jen přidávají a mění stav.
Nahrazuje `task_queue_20260812.md` a `task_queue_20260813.md` (oba archivní).

> **Proč vznikl (14.08.2026).** Fronta se od 10.08. každý den psala znovu kolem
> nejčerstvějšího nálezu. Průtok byl slušný (12.→13.08. se uzavřelo 8 položek),
> ale ze ~40 položek z 12.08. jich nová fronta z 13.08. vypisovala **9** —
> ~20 otevřených položek z papíru zmizelo, aniž se uzavřely nebo odložily.
> Namátkou ověřeno, že žijí dál: rozhodčí N2 (soubor neexistuje), rerolly
> paušál 3 (`game_simulator.cpp:153`, `:324`).
> Je to porušení pravidla *„odložené zapsat HNED, s prioritou"* a táž rodina
> chyby jako audit měřicího aparátu: **snímek se vydává za stav.**

## Jak se to čte

| stav | význam |
|---|---|
| **OTEVŘENO** | čeká na práci |
| **BLOKOVÁNO** | čeká na jinou položku, uveden blokátor |
| **ODLOŽENO** | vědomě, uveden **spouštěč** návratu |
| **UZAVŘENO** | hotovo, uveden commit / doklad |
| **ZAMÍTNUTO** | změřeno a rozhodnuto nedělat, uveden důvod |
| **?** | neví se — **ověřit, než se podle toho plánuje** |

⚠️ **Jediná část, která se přepisuje, je poslední oddíl „CO JE TEĎ PRVNÍ".**
Vše ostatní jen mění stav ve sloupci. ID se **nikdy nepřečíslovávají** —
uživatel na položky ukazuje číslem.

---

# 1. REPERTOÁR *(rozhovor; nesoupeří o strojový čas)*

| ID | co | stav |
|---|---|---|
| T1.1 | Bilance soupeřova kola — chybějící *dimenze*, ne měření | **UZAVŘENO** — spec ČÁST 13, `evidence/exposure_scan_20260812.md`; dalo E1 (`REACH0=0`) a E2 (`FB2≤1`) |
| — | Big Guy soupeře | **UZAVŘENO** — spec ČÁST 10 (S-BG.1–6, Z15, Z16). *Nebyla na žádném seznamu; vyšla z rozhovoru.* |
| T1.2 | Chybějící situace: kolo po turnoveru · po obdrženém TD *(doktrína záporné rezervy je v paměti, ne v katalogu)* · utrácení rerollů · hranice poločasu · počasí | **OTEVŘENO** |
| T1.3 | Odehraná situace na **S5** | **ZAMÍTNUTO** — číslo „96 % dosažitelnost / 53 % pokus" bylo z korpusu 30.07. (n=53, engine o 6 oprav starší). Dnes pokus v **89 %** kol S5, čistá vada volby 2,7 %. |
| T1.3′ | Odehraná situace na **S7 (boxing-in)** — **32,4 % kol**, jediné robustní číslo rozložení | **OTEVŘENO** |
| T1.4 | [8]/O2 **předání míče zapsat jako povinnost** — rozhodnuto („skoro nikdy"; „záloha = potenciální nosič"), nezapsáno | **OTEVŘENO** |
| T1.4′ | O6 **nouze: prorazit vs. držet** — S4 = 27,7 % kol, doktrína NEROZHODNUTÁ | **OTEVŘENO** |
| T1.5′ | **S5.3/S5.4 zajištění sběru** — záloha u míče 22,2 %, nosič krytý po sběru 28,8 %. *Sbírat umíme, pojistit sběr ne.* | **OTEVŘENO** |
| T1.6 | [4] Blitzer končí na lajně — na lajně má stát tělo, které NENÍ určeným rohem klece | **OTEVŘENO** |
| T1.7 | Rozpočet těl pro R3 — 12.08. prošla náhodou; R1 spolyká všechna pohyblivá těla | **OTEVŘENO** |

# 2. MĚŘICÍ APARÁT

| ID | co | stav |
|---|---|---|
| — | Audit aparátu: 7 míst, kde se měřilo něco jiného; `Check(ok,n,deg)` + N/A | **UZAVŘENO** — `dd295e5`, `e4b99ee`, `95e0223` |
| T2.1 | **N2 rozhodčí** `diag_turn_referee_20260811.py` — konec kola z `turnLogs[i+1]`, klasifikace S0–S10, karta kola s `proc` | **OTEVŘENO** — ⚠️ ověřeno 14.08.: soubor **neexistuje**. Základní nástroj, na kterém stojí víc kontrol. |
| T2.2 | K29–K33 pro ČÁST 9 (R1–R4) | **UZAVŘENO** — `diag_rules_checks_20260812.py` |
| T2.3 | N3 sestavy (na kolo / drive / zápas / rasu) | **OTEVŘENO** |
| T2.4 | K28 rozložení S0–S10 | **UZAVŘENO s výhradou** — spec ČÁST 12; hranice S2/S3/S4 stojí na `paceAch`, který je 0.0 (`NOT_CONSULTED` 100 %) ⇒ robustní je jen S7 |
| T2.5 | N4 kalibrace proti uživateli — 20 kol, shoda ≥ 18/20, **než se agregátu uvěří** | **OTEVŘENO** |
| T2.6 | **X2 + X3** (kostky bloku · deklarovaná makra s pořadím) — jedna oprava odemkne **Z4, Z5, Z9, Z14, S2.14, S10.3** | **OTEVŘENO** — nejlepší poměr odemčeno/cena v celém aparátu |
| T2.7 | [14] diag binárky staticky — jinak stará binárka tiše měří jiný engine | **OTEVŘENO** |
| T2.8 | E1/E2 jako K34/K35 | **UZAVŘENO** — `bc9cf17` |
| T2.9 | **K36 `LOCKED`** — zamčená vlastní těla jako chybějící člen tempa | **UZAVŘENO** — `bc9cf17`; potvrzeno na 3000 hrách, monotónní: ≤2 → Δx **+2,18** (n=15314) · 3–5 → **+1,89** (n=2162) · 6–8 → **+1,06** (n=50) |
| K9b | — | **BLOKOVÁNO** na T3.1 (`resistance` je 0, plánovač se nezeptá) |
| K32 | blitz se v logu nepozná od bloku | **BLOKOVÁNO** na X1 |
| T0.1 | Srovnat K9 se S2.7 — kontrola měří konstantu, povinnost přikazuje funkci odporu. Tichá chyba. | **?** — mohlo padnout v auditu 13.08., neověřeno |
| T0.2 | Vyškrtat uzavřené: O3, O4, O5, X6 | **?** — neověřeno |

# 3. TEMPO A POSTUP

| ID | co | stav |
|---|---|---|
| T3.1 | Brána klece — veto jen při `achievable == 0` + cage-fill | **ZAMÍTNUTO** — 1500 párů, dw-we **−0,0297 (−2,0σ)**, dw-sk +0,008. ⚠️ Nezahazovat kód: zlepšila skoro všechny kontroly, vyměnila tempo (20,6→28,4 %) za bití (76,1→73,2 %) a čistotu rohů (79,4→72,6 %) ⇒ nula. **Chybí jí plán trasy, ne schopnost.** |
| T3.2 | [1] Kontrola `c085331` (exposure = uživatelovo R1) — přeměřit K7 na korpusu PO opravě klece | **BLOKOVÁNO** — viz A2 |
| A2 | ⭐ **Exposure/R1 vyzvednout z `cage_advance.cpp` do obecného pohybu** | **OTEVŘENO** *(nové 14.08.)* — ověřeno: `c085331` sahá jen do `cage_advance.cpp`, ten se instancuje výhradně při `config.cageAdvance` (`macro_mcts.cpp:907`) ⇒ **hotová práce nejde zapnout nezávisle na zamítnuté bráně.** Je to tvoje vlastní pravidlo *„BLITZ pohyb → obecný pohyb"*. |
| T3.3 | [2] Tempo cílit na 3,14, ne 2,61 | **PŘEFORMULOVÁNO** → P3 (rovnoměrná podlaha je špatný model) |
| T3.4 | [3] Změřit `41c3570` — kdo nese, kdy je první držení | **UZAVŘENO** — korpus 3000: Runner **88–91 %** ve všech kategoriích, Longbeard 1–4 % |
| T3.5 | [5] Na lajně stojí 4 hráči místo 3 — dává soupeři blok navíc; je to v poli formace, ne v přiřazení | **OTEVŘENO** |
| P3 | ⭐ **Fázový plán trasy** — sólo Runner + kick-off return → klec → sólo výběh u endzone. Rozvrh pozpátku od TD **po fázích**. Vstup pro `classifyTurnGoal` i `K9a`. **Bez fáze v modelu nejde odlišit chybu od záměru.** | **OTEVŘENO** |

# 4. ROZHODOVÁNÍ ENGINU *(živé chyby)*

| ID | co | stav |
|---|---|---|
| P4 | **`CHAIN_SCORE` je aktivní bug** — krok 1 (pass) spálí `passUsedThisTurn`, krok 2 (hand-off) se pak nenabídne ⇒ přihrávka se provede, předání selže, **tah je pryč**. Opravit nebo odstranit. | **OTEVŘENO** |
| P5 | **Hand-off pro výměnu nosiče** — filtr váží předání cenou přihrávky (33 %), i když by ho provedl jako hand-off (83 %); práh 0,5 zahodí i Runner→Runner (44 %) ⇒ nenabízí se žádné předání. Kritérium: **„nosič je špatný"** (AG≤2 bez Sure Hands a nedoběhne), ne „příjemce je lepší". Patch: `scratchpad/handoff_fix_plan.md` | **OTEVŘENO — POVÝŠENO uživatelem 14.08.** („spravil bych ho dřív"). Zisk je pod šumovým dnem (Longbeard nese 1–4 % kol) ⇒ **neměřit A/B, opravit a ověřit na kontrolách.** |
| P6 | **Zobecnit item 14 na výběr cíle a na pickup** — BLITZ vybírá cíl podle surových kostek (blitzera podle kostek + cesty); PICKUP váží cenu sebrání, ne cestu k míči (připouští 2 GFI = 30 % pád). Nástroj existuje: `estimateApproachFailChance` (`macro_actions.cpp:206`), použitý jen 2× a oba u blitzu. Porušené vlastní pravidlo z 03.08. | **OTEVŘENO** |
| P8 | **Výběr cíle faulu** bere prvního ležícího v pořadí sousedních polí, nehodnotí nic. Přitom Gutter Runner je 4,4× lepší cíl a Thick Skull se nefauluje. | **OTEVŘENO** |
| T4.1 | Záloha u míče = potenciální nosič (Blitzer AG3 MA5) | **OTEVŘENO** |
| T4.2 | Ověřit bránu přihrávek na korpusu — dopočítané, ne změřené | **OTEVŘENO** |
| T4.3 | Priorita blitzu: zeď kupředu → odmarkovat nosiče → příležitost | **OTEVŘENO** — P2 je jeho zostření |
| P2 | **Doktrína „BÍT TY, KDO ŠPINÍ ROH"** — špinavý roh **−2,2σ**, počet rohů **0σ**. *Čistý roh se nevyrábí obsazováním, ale sražením toho, kdo ho špiní.* ⇒ blitz na soupeře sousedícího s rohem klece před ostatními cíli. | **BLOKOVÁNO** na P0.1 **a nově na P0.6** |
| P0.6 | ⭐ **Co očištění rohu STOJÍ — rozpočet, ne jen výnos** *(uživatel 14.08.: „když použijeme blitz na očištění rohu, bude nám chybět pro prolomení zdi — omezený počet zdrojů = hráčů a jeden blitz na kolo")*. **Blitz je 1/kolo, blok je bez limitu** — kdo už sousedí, udeří zadarmo; blitz je potřeba jen tehdy, když se k cíli musí DOJÍT. ⇒ klíčové číslo: **jaký podíl špinavých rohů jde očistit BEZ blitzu**. Když je vysoký, konflikt se zdí mizí a P2 se má formulovat jako priorita **bloku**, ne blitzu. Dál: cena, když blitz nutný je (Δx v N a N+1 podle toho, na co blitz padl) · jsou ta **1,77 idle těla** (K31) na roh vůbec použitelná, nebo jsou zamčená (K36)? | **OTEVŘENO** — zadáno Fablemu 14.08. jako část C |
| T4.4 | Generování chain pushe | **ZAMÍTNUTO** (Fable 12.08.) — tempo RAW 0,345 / vážené 0,294 (brána 0,3, a je to HORNÍ odhad); únik 0,29/10 kol proti brance 1/10 ⇒ **řádově chybí**. Beneficient je v 78 % sám filler, nosič v 1,6 %. Podmíněná výjimka: skaven 0,379 / human 0,317 projdou i váženě. |
| T4.5 | Jump Up: Block Action vleže (+2) | **ODLOŽENO** — **spouštěč:** až budeme hrát roster, který Jump Up má |

# 5. PARITA S CRP

| ID | co | stav |
|---|---|---|
| — | Push back hledá prázdné pole, řetěz pokračuje | **UZAVŘENO** — `fea042c` |
| — | Stand Firm zastaví řetěz + follow-up nešlape na neuvolněné pole | **UZAVŘENO** — `0ec69f3` |
| T5.7 | Dauntless se řeší PŘED asistencemi | **UZAVŘENO** — `9f98070` |
| T5.10 | ⚠️ **Týmové rerolly jsou paušál 3** pro všechny rasy (`game_simulator.cpp:153`, `:324`), ačkoli roster veze `rerollCost` a liší se (dwarf **40**, skaven **60**) ⇒ trpaslík by si za týž rozpočet koupil **víc**. **Nepřesnost v NÁŠ NEPROSPĚCH.** Bolí u AG2 dodge (50 % vs 75 %) a u GFI (turnover 16,7 % vs 2,8 %). | **OTEVŘENO** — ⚠️ ověřeno živé 14.08. |
| T5.11 | Náš **herní nástroj rerolly vůbec nesleduje** ⇒ odhady rizika v ručních partiích jsou konzervativní horní mez | **OTEVŘENO** |
| T5.12 | **TD v soupeřově kole nestojí skórujícího tým kolo.** CRP *(„Scoring in the opponent's turn")*: kdo skóruje tím, že je odsunut do endzony, *„must move their Turn marker one space along the Turn track"*. `action_resolver.cpp:207–213` bod připíše a značku tahu **neposune**. | **OTEVŘENO — NÍZKÁ PRIORITA** *(uživatel 14.08.: „zajímavé pravidlo s menším výskytem")*. Výskyt 15 z 2183 TD (0,7 %), bilance **8× proti nám / 7× pro nás** ⇒ prakticky neutrální. ⚠️ Ale viz výhrada níže — 8 je **podlaha, ne strop**. |
| — | **Doktrína: nikdy netlačit soupeřova nosiče směrem k JEHO endzoně.** Blok, který odsune nosiče do endzony, kterou útočí, mu **daruje TD** (CRP to výslovně umožňuje). 8× ve 3000 hrách. | **OTEVŘENO** — zapsat jako zákaz do spec (patří k prioritě blitzu, T4.3/P2) |
| P7 | **Sdílený limit pass / hand-off** — CRP má dva nezávislé limity, engine jeden (`passUsedThisTurn`). ⚠️ **Jediný nález, který po opravě hraje PROTI nám** ⇒ měřit zvlášť. | **OTEVŘENO** |
| T5.2 | [6] **Kick-Off Return** — 3 vady | **OTEVŘENO — POVÝŠENO** *(fáze 1 fázového plánu na něm stojí, přestává být okrajové)* |
| T5.1 | [7] Tabulka výkopu — 5 z 11 výsledků vadných; 22 % výkopů zahazuje volné tempo | **ODLOŽENO uživatelem** — **spouštěč:** až se bude řešit fáze 1 / kick-off (tedy spolu s T5.2) |
| T5.3 | Zranění nepřetrvávají přes drive — každý TD staví 11 čerstvých ⇒ attrition čísla měří jen poslední drive | **OTEVŘENO** |
| T5.8 | Frenzy druhý blok se v hodnocení příležitostí nikde nemodeluje — týká se Slayerů | **OTEVŘENO** |
| T5.4 | [13] M1 přeběhnout — smazat falešný `M1_DONE`, přestavět `diag_m1` | **OTEVŘENO** |
| T5.5 | O1 **kopat, nebo přijímat** — volbu vůbec nemodelujeme; potenciálně větší páka než cokoli uvnitř kola | **OTEVŘENO** |
| T5.6 | O7 Underworld | **OTEVŘENO** |

# 6. DOMĚŘENÍ NA VELKÉM KORPUSU *(korpus stojí hotový, strojový čas nestojí)*

Korpus: `diag_replay_mine_20260813_big_data`, **3000 her**, brána OFF, HEAD `e4b99ee`,
44 177 našich kol. Doklad `night_big_20260813/`.

| ID | co | stav |
|---|---|---|
| P0.1 | **Předpovídá blok v kole N čistotu rohů v N+1?** Když ano, P2 má zelenou. | **OTEVŘENO** |
| P0.2 | REACH0 **jako počet** (na 195 drivech jen −1,8σ) | **OTEVŘENO** |
| P0.3 | K36 `LOCKED` — koše měly n=4 až 16 | **UZAVŘENO** — viz T2.9 |
| P0.4 | Skórovací podíl **po fázích** | **OTEVŘENO** — potřebuje P3 |
| P0.5 | ⭐ **Řetěz „špinavý roh → zamčené tělo → chybějící roh příště"** — koreluje počet špinavých rohů v N s počtem zamčených těl a obsazených rohů v N+1? **Vysvětluje, proč je špinavý roh −2,2σ, i když ztrátu míče nezpůsobí hned: účet nepřijde v kole, kdy se chyba udělá.** Táž chyba má proti různým soupeřům různou splatnost — proti wood-elfovi okamžitě (ztráta míče), proti skavenovi na splátky (zámky). **Odložená varianta je nebezpečnější, protože se u stolu nespojí s příčinou.** *(uživatel 13.08., z vlastní hry)* | **OTEVŘENO** |
| P1 | **Přepsat K33 a K34 na spojité** — jako prahy nepředpovídají nic (0,6σ / 0,8σ), jako počty patří k nejsilnějším. Platí i pro E1: *„ani jeden otevřený roh"* je správný **cíl**, ale jako **kontrola** se má měřit `REACH0` jako počet (1 → 8,3 %, 4+ → 33 % ztráty). | **OTEVŘENO** — levné, opravuje metr |
| A1 | **Anomálie: TD flag nesedí se skóre delta** — 9 z 3000 her (0,3 %) | **OTEVŘENO — zadáno uživatelem na 14.08. přes den** |

# 7. AŽ NAPOSLED

| ID | co | stav |
|---|---|---|
| T6.1 | N5 vstřelení plánu priorem `f(rezerva)` — jediná změna chování, až po měřicím aparátu | **OTEVŘENO** |
| T6.2 | **Učení** — až po dokončení repertoáru, s úplnou procedurou jako nulovou hypotézou | **OTEVŘENO** |

---

# ⚠️ VÝHRADY, PODLE KTERÝCH SE NESMÍ NAVRHOVAT

* **Soupeřova AI nehraje proti našim slabinám cíleně.** Runner nevypadne ze
  hřiště ani jednou ve 120 hrách, protože si pro něj nikdo nechodí. Lidský
  soupeř by to dělal (AG3 máme jen 4 z 11).
  ⚠️ **Platí i na naměřené četnosti chyb, ne jen na naše ztráty.** „8 darovaných
  TD ve 3000 hrách" (T5.12) je četnost, jak často do toho **naše AI** shodou
  okolností spadne. Kolikrát by nás do toho dostal soupeř, který tu situaci
  hledá, korpus neříká. **Nízký výskyt v korpusu = podlaha, ne strop.**
* **Sdílený limit pass/hand-off** (P7) dělá hbité rasy slabšími, než jsou.
* **SPP se nesledují** — elfí AI nemá důvod házet kvůli bodům.
* **T5.9: snímek je začátek kola** — skutečné pořadí akcí může vzor zničit
  dřív. Obecné omezení všech našich skenů.
* **Šumové dno ±5,3 pp na 400 párech** ⇒ na efekt 3 pp je potřeba **1500+
  párů** (= ~7 h stroje). Harness je deterministický.
* **T2.4: hranice S2/S3/S4** nejsou robustní, dokud `paceAch` loguje 0.0.

# ⭐ CHYBĚJÍCÍ DIMENZE — dvě, a jsou to sourozenci

Audit spec (12.08.) našel, že celá procedura popisuje jen NAŠE kolo a chybí jí
dimenze **„co pozice dává soupeři"** — doplněno jako ČÁST 13 (E1/E2).

14.08. vyšla najevo **druhá, stejného tvaru: „co ta akce bere jiným akcím".**
Doporučení se u nás počítají jako čistý výnos, protože se měří jedna veličina
proti výsledku. Jenže **blitz je 1 za kolo a těl je 11** — každé „dělejme X"
je ve skutečnosti „dělejme X **místo** Y". Objevilo se to na P2 (očistit roh
blitzem = nemít ho na prolomení zdi), ale platí to na každou položku, která
předepisuje akci.

⇒ **Pravidlo pro každý budoucí návrh doktríny:** vedle výnosu uveď, z jakého
rozpočtu se platí (blitz 1/kolo · pass 1/kolo · foul 1/kolo · 11 těl, z toho
~2,6 leží a ~1,0 zamčené) a co se za to nedělá. Bez toho je σ jen polovina
odpovědi.

# ⭐ OTEVŘENÁ OTÁZKA Č. 1

**Proč zlepšení procesu nevede k výsledku.** Brána zlepšila skoro všechny
kontroly a chess se nehnul. Část je vysvětlená (vyměnila tempo za bití),
zbytek ne. Dokud to nevíme, stojí *„úplná procedura jako nulová hypotéza pro
učení"* (T6.2) na kontrolách, o kterých nevíme, že k něčemu jsou.

# ⭐⭐⭐ CO PŘEDPOVÍDÁ TD *(podle čeho se řadí zbytek)*

Plné drivy ≥7 kol, 195 drivů:

| | σ |
|---|---|
| K9a tempo | **4,2σ** |
| bloků na kolo | **2,7σ** |
| čistota rohů / `FB2 ≤ 1` | 2,6σ |
| **špinavých rohů** | **−2,2σ** |
| REACH0 (počet) | −1,8σ |
| *počet rohů klece* | *−0,2σ = nic* |
| *K33, K34 jako ano/ne* | *0,6σ / 0,8σ = nic* |

**Tempo a bití jsou dva nezávislé prediktory a brána je proti sobě vyměnila.**

---

# CO JE TEĎ PRVNÍ
*(jediný oddíl, který se přepisuje — stav k 14.08.2026 ráno)*

| | co | proč |
|---|---|---|
| **1.** | **A1 anomálie** TD flag vs skóre delta | zadáno uživatelem na dnešek; 0,3 % her měří nesprávný výsledek — dokud to platí, každé A/B má šum navíc |
| **2.** | **P5 hand-off** | povýšeno uživatelem; oprava bez A/B, ověřit na kontrolách |
| **3.** | **P0.1 + P0.5 + P0.2** na velkém korpusu | korpus stojí hotový, nestojí strojový čas, odemyká P2 |
| **4.** | **P1** K33/K34 na spojité | levné, opravuje metr |
| **5.** | **P3** fázový plán trasy | bez fáze nejde odlišit chybu od záměru |
| **6.** | **A2** vyzvednout exposure/R1 z plánovače klece | jediná cesta, jak změřit hotovou práci za zamítnutou branou |

**Na strojový čas víkendu** (~60 h, jedno A/B = 7 h) se vejde 5–6 A/B.
Kandidáti v pořadí: P2 *(po P0.1)* · A2 · P6 · P7 *(zvlášť, zhorší nás)*.

**T1 (repertoár) a strojová část spolu nesoupeří o zdroj** — dají se nechat
běžet vedle sebe. Uvnitř strojové části platí „jedna změna najednou".
