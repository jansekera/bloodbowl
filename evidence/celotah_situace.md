# SITUACE, KTERÉ MÁ CELOTAH UMĚT — sběrné místo

> **Uživatel 02.09.:** *„před začátkem práce na celotahu spolu projdeme co nejvíc
> situací, co by měl celotah umět — protože čím víc jich najdeme, tím líp bude
> celotah fungovat."*
> A k zařazení sekce: *„celotah je sice na konci fronty, ale skládá se ze všech
> předchozích akcí a dělá z nich kombinace — tak jej doplňujme postupně."*

**K čemu to je:** každá zapsaná situace je **test, který se dá napsat dřív, než
vznikne kód**. Zároveň je to obrana proti tomu, aby se celotah navrhl podle
jedné situace, která byla zrovna po ruce.

⛔ **PRAVIDLO SBĚRU:** situace se sem zapisuje, i když k ní nemáme řešení.
Nezapisovat řešení — zapisovat **co má hráč rozeznat**.

---

## ⛔⛔ VAROVÁNÍ PŘED MĚŘENÍM — MODEL CELOTAH JEŠTĚ NEUMÍ

> **Uživatel 02.09.:** *„celotah ještě model neumí — tak dej pozor, ať nepočítáš
> něco, co model neumí."*

Kdybychom měřili **rozhodnutí, která engine nedělá**, každé číslo spadne do N/A
a bude říkat jen to, co už víme: *že to neumí*. Týž tvar jako **Leap měřený
proti dodge** — porovnávalo se s něčím, o co se ani nesnaží.

```
⛔ NEMĚŘIT:  „schoval si plánovač blitz na správný okamžik?"
             (mechanismus schovávání NEEXISTUJE ⇒ vždy „ne")
⛔ NEMĚŘIT:  „zvolil dobré pořadí aktivací?"
             (pořadí se nevolí ⇒ otázka nemá referenci)

✅ MĚŘIT:    DŮSLEDKY, které dnes nastávají
             · na jak hodnotný cíl blitz padne
             · jak často makro selže uprostřed tahu a co se pak stane
             · kolikrát nosiči zavazí vlastní tělo
```

⭐ **Rozdíl:** to první měří **chybějící schopnost** *(a vyjde vždy nula)*,
to druhé měří **cenu její nepřítomnosti** *(a to je číslo, které se dá porovnat
po nasazení)*.

⚠️ Platí to i pro **předregistrace**: registrovat se smí jen čtení, které má
dnes co vytisknout. [[feedback_registered_reading_needs_a_print_line]] ·
[[feedback_na_bucket_is_the_finding]] · [[feedback_measure_what_the_change_does]]

---

## A. UŽ DOLOŽENÉ Z NAŠÍ PRÁCE

⭐ **KAŽDÁ SITUACE NESE TŘI VĚCI** *(uživatel 02.09.: „na jednu stranu chci
zaznamenat všechny možnosti, na druhou si dát pozor, ať to neměřím, dokud to
neumí")*. Tím se ta dvě pravidla přestanou tlouct: **sbírá se neomezeně,
měří se jen prostřední sloupec.**

| # | situace — CO MÁ HRÁČ ROZEZNAT | ✅ jde měřit DNES *(cena nepřítomnosti)* | ⏰ změří se AŽ TO BUDE UMĚT |
|---|---|---|---|
| **C1** | **Blitz na souseda jako UVOLNĚNÍ.** Srazím souseda ⇒ zmizí tacklezóna ⇒ zbytkem pohybu odejdu **bez dodge**. Hodnota není v ráně, ale ve zbytku tahu. *(ř. 552-553)* | kolik blitzů skončí **stáním na místě**, ač měl hráč pohyb i volné pole | podíl blitzů, po kterých hráč **odešel a byl jinde užitečný** |
| **C2** | **Tři možnosti vstávání.** zůstat vedle *(blok zdarma, neomezený)* · odejít *(⛔ OPRAVENO 08.09. — je to REPOSITION/pohyb, ne blitz; stojí jen vlastní pohyb hráče po vstání + GFI, netýká se týmového rozpočtu blitzu — ověřeno `macro_actions.cpp:1161`, `out.push_back({MacroType::REPOSITION,...})`, `usedBlitz` se nenastavuje)* · zůstat ležet *(faul 1×)*. ⚠️ Vedle nosiče je zůstání **cena za něco**. | ✅ **měří se dnes** — `Q3/ODPOVED`: 23,9 % ran blitzem, zbytek blok zdarma *(⚠️ tohle je odpověď SOUPEŘE na naši volbu, ne cena naší volby — neověřeno, jestli je i tenhle popisek přesný)* | delta ramene Q3 *(noc 02.09.)* |
| **C3** | **Rozpočet blitzu.** Jediná akce s limitem 1/kolo — kdy si ho **schovat**? | na jak **hodnotný cíl** blitz padne; kolikrát zůstane neutracen *(dnes 25,1 %)* | zda se blitz drží do okamžiku, kdy je nejdražší pro soupeře |
| **C4** | **Nosiči zavazí VLASTNÍ hráči** *(149/149)*. ⇒ napřed uhnout, pak jít. | ✅ **ZMĚŘENO 10.09.:** pole před stojícím volným nosičem je obsazené **naším** tělem v **27,5 %** *(bylo 50,1 % v baseline 19.08.)*; z obsazených jsou naši **96,7 %** — viz `A8` níž | zda pořadí aktivací tu překážku odstraní dřív |
| **C5** | **Nosič si nechává pohyb v záloze** — má smysl jen dokud soupeř nedosáhne blitzem *(`MA + GFI`)*. | kolik kol nosič šetří pohyb, ačkoli je **v dosahu blitzu** | zda se rezerva drží jen tam, kde něco koupí |
| **C6** | **Obrana: dva sloupce → skok do L**, přechod řídí **převaha**. Engine fáze nemá. | ⛔ **NIC** — fáze v enginu neexistuje, každé číslo by bylo N/A | podíl kol strávených ve správné fázi |
| **C7** | **Zeď: prolomit vs oběhnout** — univerzální objekt, **rasová odpověď**. | kolikrát se u zdi zvolí průchod přes tacklezónu vs obchůzka | zda volba odpovídá rase a situaci |
| **C8** | **Klec má DVA cíle naráz** — dojít k TD **a** chránit nosiče. | ✅ máme: tempo drivu, `cageSnapshot` rohy | zda se cíle váží proti sobě, ne střídají |

⛔ **C6 je příklad, proč ten prostřední sloupec existuje:** je to reálná situace,
ale **dnes se na ní nedá změřit vůbec nic**, protože fázový model chybí. Kdyby
sloupec nebyl, někdo by to zkusil měřit a dostal by nulu, kterou by četl jako
nález.

## A2. SEKVENCE — CO JE VLASTNĚ „SITUACE PRO CELOTAH"

⛔⛔ **UPŘESNĚNO 02.09. (uživatel):** *„s těmi informacemi mi šlo o příklady jako
blitz na toho, kdo nám markuje nosiče, a pak nosičem pohyb a TD — tvoje odpověď
mi přijde o něčem jiném."*

**Situace pro celotah je SEKVENCE, ne vlastnost jedné akce.** Má tvar:

```
akce A jednoho hráče  →  ta UMOŽNÍ akci B jiného hráče  →  výsledek
```

⇒ Nálezy typu *„plánovač oceňuje makra izolovaně"* nebo *„plán se postaví
a zahodí"* jsou **architektura**, ne situace. Patří do auditu
*(`fable_wholeturn_audit_20260902.md`)*, ne sem. Sem patří **konkrétní tah**,
který jde nakreslit na desku.

| # | SEKVENCE | ✅ jde měřit DNES | ⏰ až to bude umět | zdroj |
|---|---|---|---|---|
| **C9** | **Odmarkovat nosiče a skórovat.** Soupeř nám značí nosiče ⇒ **blitz na toho, kdo markuje** ⇒ zmizí jeho tacklezóna ⇒ **nosič vyrazí bez dodge** ⇒ TD. Blitz tu není za zranění, je to **klíč k nosičovu pohybu**. | kolikrát je nosič markovaný **a blitz jde jinam**; kolikrát nosič dodgeuje, ač šel soused srazit | podíl blitzů, po kterých se **nosič posunul dál**, a z toho TD |

⛔ **C9 je vzor, jak to psát:** jmenuje **kdo**, **v jakém pořadí** a **proč to
druhé bez toho prvního nejde**. Tím se liší od C1, který popisuje jen tu první
polovinu *(blitz uvolní blitzujícího)* — **C9 uvolňuje NĚKOHO JINÉHO.**

---

## A3. SBĚR 02.09. — 46 SEKVENCÍ Z MĚSÍCŮ KONVERZACE *(`Q01`-`Q46`)*

Uživatel 02.09.: *„ty sekvence by měly být zapsané z měsíců konverzace a bylo
jich hodně"* — a bylo. Prošly se spec, evidence, fronta úkolů i paměť.
**Detail každé je v `evidence/celotah_situace_sber_20260902.md`**, tady je
jmenný seznam, aby se ve sběrném místě **žádná neztratila**.

⭐ **Tvar je u všech stejný:** *akce A → umožní akci B **jiného hráče** →
výsledek*, a u každé je napsáno **proč to druhé bez prvního nejde**.
⇒ To je ta hranice, kvůli které vznikl `C9`: vlastnost jedné akce sem nepatří.

⛔ **Prostřední sloupec (co jde měřit DNES) je v tom sběru povinný a smí být
„NIC".** Z 46 sekvencí má **11** položek `⛔ NIC` — pořadí aktivací, fáze
obrany, elfí obrana, rerolly, apothecary, výkop, počasí. Tam mechanismus
v enginu **neexistuje** a číslo by vyšlo nula ⇒ **neměřit**, jinak bychom
změřili chybějící schopnost. Viz varování nahoře.

| **`Q01`** | ODMARKOVAT NOSIČE A JÍT |
| **`Q02`** | BLITZ SE VYBÍRÁ PODLE TOHO, KOLIK DODGŮ UBERE CESTĚ NOSIČE — ne podle kostek |
| **`Q03`** | PROŽENÍ ZDÍ JE JEDEN TAH, NE DVA |
| **`Q04`** | DÍRA SE MĚŘÍ CENOU PRŮCHODU V DODGÍCH, ne šířkou |
| **`Q05`** | UHNOUT VLASTNÍM TĚLEM ⇒ NOSIČ PROJDE |
| **`Q06`** | ESKORTA UKLIDÍ CESTU, TEPRVE PAK JEDE KLEC |
| **`Q07`** | POŘADÍ UVNITŘ TAHU: napřed volné tahy, jejichž hodnota NEZÁVISÍ na zbytku kola |
| **`Q08`** | SELHÁNÍ SE ŘADÍ PODLE TOHO, CO PO SOBĚ ZANECHÁ |
| **`Q09`** | STAND-AND-GO |
| **`Q10`** | HIT-AND-RUN |
| **`Q11`** | BLITZUJÍCÍ ZŮSTANE VOLNÝ ⇒ STANE SE ROHEM KLECE |
| **`Q12`** | BLITZ Z LEHU + CHAIN PUSH |
| **`Q13`** | CHAIN PUSH PRO NÁS: blok na cizí tělo posune NAŠEHO nosiče o pole blíž k endzóně |
| **`Q14`** | WRESTLE UDĚLÁ MEZERU — a projde jí NĚKDO JINÝ |
| **`Q15`** | NACHYSTAT PŘÍJEMCE DOPŘEDU ⇒ V POSLEDNÍM TAHU MU PŘEDAT |
| **`Q16`** | SRAZIT PRONÁSLEDOVATELE ⇒ PRODLOUŽIT SÓLOVÝ VÝBĚH |
| **`Q17`** | ROH, KTERÝ SÁM UDEŘÍ, PŘESTÁVÁ BÝT ROHEM |
| **`Q18`** | VOLNÝ SOUSED SRAZÍ MARKERA ROHU ZDARMA ⇒ BLITZ ZŮSTANE NA JINOU PRÁCI |
| **`Q19`** | IDLE TĚLO DOJDE A ASISTUJE ⇒ Z JEDNOKOSTKOVÉHO BLOKU JE DVOUKOSTKOVÝ |
| **`Q20`** | KDYŽ NOSIČ ODEJDE, ROHY MUSÍ JÍT S NÍM — a je to otázka POŘADÍ |
| **`Q21`** | KDYŽ SE TĚLO TAK JAKO TAK HÝBE, MÁ CÍLIT ROVNOU NA ROH |
| **`Q22`** | „CLEAR BLOCKERS", PAK POSUN — v jednom tahu |
| **`Q23`** | POSTAV SE TAM, KDE NĚKDO TEPRVE BUDE |
| **`Q24`** | VACATE-FIRST: kdo překáží, jde první — a žádná noha plánu nesmí zabít pozdější nohu |
| **`Q25`** | PŘIVEĎ ASISTENCI, PAK UDEŘ |
| **`Q26`** | NA BIG GUYE SE NECHODÍ — ČEKÁ SE, AŽ PŘIJDE |
| **`Q27`** | BLOK NA SOUPEŘOVA NOSIČE SE PŘIPRAVUJE, NE POPADÁ |
| **`Q28`** | SRAZIT → FAULNOUT → ODSTRANIT |
| **`Q29`** | FRENZY JE DVOUKROKOVÁ SEKVENCE, A DRUHÝ BLOK SE HÁZÍ Z NOVÉ POZICE |
| **`Q30`** | PO NEÚSPĚŠNÉM BLITZU SE STÁHNOUT ZA ŠTÍT |
| **`Q31`** | CHAIN PUSH JE ZPŮSOB, JAK POHNOUT OZNAČKOVANÝM HRÁČEM BEZ HODU — odsun není dodge |
| **`Q32`** | ODMARKOVAT MÍČ, ZŮSTAT U NĚJ, PAK SBÍRAT |
| **`Q33`** | VZÍT MÍČ JEN KDYŽ HO UMÍM ZAJISTIT |
| **`Q34`** | KOLO PO TURNOVERU: MÍČ BRÁT, ne dostavovat klec |
| **`Q35`** | „PŘEDEJ ZA SEBE A ODHOĎ PŘES ZEĎ" — pass a hand-off mají VLASTNÍ povolenky ⇒ v jednom kole jde obojí |
| **`Q36`** | SVÍRAT POSTUPNĚ, BEZ KONTAKTU, DOKUD TRASA PŘÍJEMCE NENÍ DOST DLOUHÁ |
| **`Q37`** | VÍC MARKERŮ ⇒ VÍC VYNUCENÝCH HODŮ ⇒ JEJICH KOLO SKONČÍ PŘEDČASNĚ |
| **`Q38`** | ROH SOUPEŘOVY KLECE, KTERÝ STOJÍ V NAŠÍ TZ, SE SRAZÍ OBYČEJNÝM BLOKEM ⇒ BLITZ ZŮSTANE NA NOSIČE |
| **`Q39`** | PROAKTIVNÍ REZERVNÍ BLITZER („vyprošťovač") |
| **`Q40`** | OKNO SE ZAVÍRÁ NEČINNOSTÍ — otázka není „proč se neblitzovalo v kole 8", ale „co udělat v kole 6, aby v kole 8 bylo koho hledat" |
| **`Q41`** | PŘEDÁ ZA SEBE → THROWER HODÍ PŘES NAŠI ZEĎ → VOLNÝ V DOSAHU TD |
| **`Q42`** | WARDANCER SEBERE → CATCHER UŽ STOJÍ NACHYSTANÝ VEPŘEDU → HOD NEBO DOBĚH ⇒ TD V TÉMŽ TAHU |
| **`Q43`** | L BEZ ZADNÍ STĚNY: blok stranou → dokročí do uvolněného pole → jeden dodge 2+ → 10 polí |
| **`Q44`** | SOUPEŘ PŘIPRAVUJE ÚNIKOVÝ KORIDOR 2-3 KOLA DOPŘEDU a hledá pruhy, které jsme opustili ZA AKCÍ |
| **`Q45`** | SOUPEŘ NÁM VYROBÍ CHAIN PUSH DO NOSIČE — NAŠIMI VLASTNÍMI TĚLY |
| **`Q46`** | STRIP BALL SHODÍ MÍČ POUHÝM ODSUNEM — bez sražení |
| **`Q47`** | *(04.09., uživatel — u W-GFI: „hráči se musí posouvat kupředu, ať jsou u akce nebo k míči")* VOLNÉ TĚLO BEZ CÍLE POSTUPUJE SMĚREM K DĚNÍ, NE STOJÍ — kdo dnes nemíří k akci ani k míči, zítra tam není, až bude potřeba (stejná rodina jako `Q40` „okno se zavírá nečinností"; sedí i na [[project_bloodbowl_doing_nothing_never_wins]]) |
| **`Q48`** | *(08.09., uživatel — rozbor situace u P9c: „dál od nosiče... ať jej kdyžtak praští ještě náš druhý, kdyby nespadl")* BLOK ODSUNE SOUPEŘE → POKUD ZŮSTAL STÁT (nespadl), ODSOUVAJÍCÍ NEBO JINÉ NAŠE STOJÍCÍ TĚLO NA NĚJ MŮŽE ÚTOČIT PODRUHÉ, POKUD ZŮSTAL V DOSAHU — hodnota destinace odsunu není jen geometrická (roh/nosič), ale i v tom, jestli **umožní druhou akci jiného/téhož hráče**. P9c (`pushDestScore`, nasazeno 08.09.) tohle nepočítá vůbec, řeší jen vzdálenost od rohu/nosiče. ⛔ Rozbor ukázal, že tahle úvaha váží MÍŇ, když existuje skutečná klec (riziko průniku za zeď převáží) — patří tedy do celotahu jako otázka pořadí/kombinace akcí, ne jako oprava P9c samotného. |

⚠️ **Oddíl 2 sběru (`V01`-`V70`) sem NEPATŘÍ** — to jsou **vlastnosti jedné
akce** (kam se smí postavit roh, kdy se nesmí skórovat, prahy kostek). Jsou to
*podmínky a ceny, se kterými sekvence počítají*, ne kandidáti na `C`.

⏰ **K PROJITÍ SPOLU** — uživatel řekl, že situace projdeme, **než se začne
pracovat na celotahu**. Tenhle seznam je podklad k tomu projití, ne plán práce.

## A4. ⭐⭐⭐ DEFINICE KLECE — přesně, od uživatele 02.09.

Uživatel 02.09.: *„roh klece musí být přesně roh a musí být přesně 4 rohy
a nikdo další vedle nosiče — navíc rohy nesmí být vedle soupeře, ale to je
už součást klece."*

**Čtyři podmínky, a každá zakazuje něco jiného:**

| # | podmínka | co zakazuje |
|---|---|---|
| **K-a** | roh je **přesně roh** | ⛔ **jen DIAGONÁLA.** Ortogonální soused nosiče **není roh** |
| **K-b** | rohů je **přesně 4** | ⛔ ani tři *(díra)*, ani „pátý na pomoc" |
| **K-c** | **nikdo další vedle nosiče** | ⛔ ortogonální pole u nosiče zůstávají **PRÁZDNÁ** |
| **K-d** | roh **není vedle soupeře** | ⛔ *(uživatel to sám zařadil do sekce KLEC — neřešit teď)* |

⭐⭐ **`K-c` je nejpřekvapivější a hned má doklad:** sběr `Q21` měří, že
**50,2 % těl na ortogonále tam došlo vlastním pohybem** a **73,6 %** z nich
mělo **čistý roh hned vedle cíle**. ⇒ Chodíme na pole, která podle definice
klece mají **zůstat prázdná**, a roh vedle necháváme volný.

⭐ **Dopad na `W-CIL` (pohyb) HNED:** větev „podpoř nosiče" nesmí po opravě
mířit na libovolné volné pole vedle nosiče — musí mířit na **diagonálu**.
Jinak by oprava aktivně vyráběla porušení `K-c`.
⇒ Pomocná funkce `standableNextTo` dostává přepínač **jen rohy / všech osm**:
značkovač k soupeři chce **kterékoli** sousední pole *(jde mu o tacklezónu)*,
doprovod nosiče **výhradně diagonálu**.

⛔ **`K-b` a `K-d` se teď NEŘEŠÍ** — jsou to vlastnosti klece jako celku
*(kolik rohů je obsazeno, a kdo stojí vedle nich)*, ne vlastnost jednoho cíle
pohybu. Patří k `K-CIL`.

## A5. ⭐⭐⭐ JEDNA SEKVENCE CELOTAHU UŽ V ENGINU JE — `BLITZ_AND_SCORE`

Uživatel 02.09.: *„blitz and score patří do celotahu — teď je ve hrách
prezentováno bez naplánování?"*

**Ověřeno v `expandBlitzAndScore()`:** je to **dvoukrokový plán dvou různých
hráčů**, provedený jako **jedno makro**:

```
Krok 1  vyber blitzujícího -- tie-break SCHVALNE pryc od nosice
        („aby se nosic neriskoval, kdyz je stejne bezpecny spoluhrac")
        -> proved blitz, odkliď blokujícího
Krok 2  „Now move the carrier to score" -> nosic jde do endzony
```

⇒ **Naplánovaná je — ale napevno, rukou.** Hledání rozhoduje jen *„vzít, nebo
nevzít"*. Neumí ji **složit**, **obměnit**, ani **najít podobnou** — třeba
`Q01` *(blitz na markera ⇒ nosič odejde bez dodge)* je týž tvar a engine ji nezná.

⭐⭐ **PROČ JE TO DŮLEŽITÉ: je to PŘEDLOHA, a zároveň jediná svého druhu.**
Dokazuje, že engine **umí provést** dvoučlennou sekvenci — ⇒ to, co u celotahu
chybí, **není provedení, ale HLEDÁNÍ**. Sedí to na naše tři patra
*(`project_bloodbowl_three_tiers_20260824`)*: `BLITZ_AND_SCORE` je v patře 1
„funguje samo", ale jen proto, že ten jeden plán **někdo napsal ručně**.

⚠️ **A nese to s sebou i varování:** právě u téhle jediné ruční sekvence se
ukázalo *(`T5.35a`, 27.08.)*, že se **nabízela tam, kde se dojít nedalo** —
944 z 1 281 nabídek mělo nosiče průměrně **10 polí** od endzóny. ⇒ Ruční plán
**nekontroluje vlastní předpoklady**; to je práce, kterou by plánovač dělal
sám. Viz `Q19` ve frontě.

## A6. ⭐⭐⭐ ZEĎ JE PŘEDMĚT CELOTAHU, NE JEDNÉ AKTIVACE *(uživatel 03.09.)*

Uživatel 03.09.: *„zeď budeme umět až v celotahu — a i tam doufám."*

**Proč:** zeď je **týmová struktura**. Jestli moje tělo drží zeď, závisí na tom,
**kde stojí ostatní a kam se v tomhle kole pohnou**. Rozhodnutí o **jedné
aktivaci** to vědět nemůže — neví, jestli mezeru zaplní spoluhráč, ani jestli
se zeď zrovna staví, nebo rozpouští.

⚠️ **Co je v enginu od 03.09.: POJISTKA, ne zeď.** `stayEarnsItsKeep` byla
rozšířena tak, že tělo si drží místo, když jeho tacklezóna drží soupeře
ohrožujícího náš míč nebo nosiče. ⇒ **Zabrání tomu, aby se zeď rozpustila tělo
po těle** *(to Q3 dělalo)*, ale zeď **nestaví ani neudržuje**.
⛔ Nezachytí ani tělo ve **druhé řadě**, které se soupeřem nesousedí, ale zavírá
cestu za první řadou — a právě druhá řada je to, co ze sloupce dělá zeď
*(spec ř. 2549-2553: „i když jednoho z předních srazíš, stejně neprojdeš")*.

⭐ **Sedí to na nález z téhož dne** *(`A5`, `BLITZ_AND_SCORE`)*: u celotahu
nechybí **provedení**, chybí **hledání**. Zeď je přesně ten případ — engine
umí tělo postavit, neumí rozhodnout, **která těla mají tvořit linii**.

⏰ **Do celotahu tedy patří:** kdo tvoří zeď · kolik řad · kdo drží druhou řadu ·
v jakém pořadí se aktivují, aby si nezavřeli cestu *(viz `Q05`, `Q24` — vacate-first)*.

## A7. ⭐⭐ TÝMOVÝ REROLL JE SDÍLENÝ ZDROJ TAHU — CENA HO NESMÍ POČÍTAT NAPEVNO *(09.09.)*

Vzešlo z `M6/B3` *(cena dodge ignoruje rerolly)*. Rozbor ukázal, že „dodge se
necení rerollem" jsou ve skutečnosti **dvě různé věci**:

* **Dodge SKILL reroll** *(pravidla ř. 8078-8090: „may only re-roll one failed
  Dodge roll per turn" — omezení je NA HRÁČE)* — žádná kolize s jinými hráči
  ani akcemi, dá se cenit izolovaně. **Řeší se přímo, mimo celotah** — viz
  `dodgeSequenceFailProb` v `macro_actions.cpp`.
* **Týmový reroll** — jeden na tým na tah, sdílený napříč VŠEMI hráči a VŠEMI
  typy hodů *(blok, GFI, dodge, přihrávka…)*. Kdyby jedna cesta v plánovači
  začala počítat „reroll je k dispozici", potřebuje vědět, jestli ho už
  virtuálně neutratila jiná akce ve stejném tahu — jinak se stejný reroll
  „utratí" vícekrát napříč různými rozhodnutími.

⭐ **Přesně tahle past už byla pojmenovaná u Q3 útěku** *(macro_actions.cpp
~ř. 1140: „nedá se převést bez vymyšlené hodnoty jedné aktivace")* — a `GFI`
už má scaffolding pro reroll (`gfiSequenceFailProb(n, rerollAvailable, ...)`),
jen je `rerollAvailable` natvrdo `false` na všech volajících místech
(`pathfinder.cpp:82,321`) — protože nikdo neumí odpovědět „je ten reroll ještě
volný v tomhle bodě tahu?".

⇒ **Do celotahu patří:** sledovat spotřebu týmového rerollu napříč aktivacemi
v jednom tahu (kdo ho už virtuálně použil, kolik aktivací ještě čeká) — teprve
pak smí kterákoliv cenová funkce *(dodge, GFI, blok)* číst `rerollAvailable`
jako `true`. Bez toho by zapnutí `rerollAvailable=true` kdekoliv v `pathfinder.cpp`
znamenalo, že si engine reroll „půjčuje" pokaždé znovu, jako by ho měl
neomezeně.

## A8. ⭐⭐⭐ PŘED NOSIČEM STOJÍ NÁŠ — ZMĚŘENO, A JE TO CELOTAH *(uživatel 10.09.)*

Uživatel 10.09.: *„Před nosičem stojí náš — je část pro celotah."* ⇒ Sem, ne do
pohybu. **Důvod je v povaze opravy, ne v tom, kde vada vzniká:** tělo před
nosičem není chyba toho těla ani nosiče — je to **chyba pořadí**. Jediné
rozhodnutí o jedné aktivaci to napravit nemůže; musel by ho udělat někdo, kdo
ví, že **tohle tělo má jít z cesty DŘÍV, než se rozhodne nosič**.

## Změřeno 10.09. *(`evidence/prereg_20260909_m11_remeasure.md`, 1000 her, engine `cf8634e8`)*

| co je na poli PŘÍMO VPŘED u stojícího volného nosiče | 19.08. | 10.09. |
|---|---|---|
| **NÁŠ** hráč | 50,1 % | **27,5 %** |
| **JEJICH** hráč | 0,5 % | 0,9 % |
| **PRÁZDNÉ** | 49,4 % | **71,5 %** |

⭐ **Ustoupilo to na polovinu** *(50,1 → 27,5 %, ~10 σ)*, ale **nezmizelo** — a to
je přesně to, co se od pohybové opravy čekat dá: BFS `nextStepToward` umí nosiče
**obejít** vlastní tělo, ale **nezabrání tomu, aby tam to tělo stálo**.

⛔⛔ **A OPRAVA VÝKLADU, KTEROU MĚŘENÍ VYNUTILO: „149/149" nikdy neznamenala, co
se z ní čte.** Už v baseline 19.08. mělo **49,4 %** stojících volných nosičů
před sebou **PRÁZDNO**. Ta věta tedy nikdy neříkala *„nosič je zablokovaný,
kdykoliv stojí"* — říkala jen, že **KDYŽ je zablokovaný, je to naše tělo**,
a to platí dál *(96,7 %)*. ⇒ Číslo 149/149 popisovalo **podmnožinu**, ne jev.

⏰ **Co z toho pro celotah zůstává:** ta **27,5 %** jsou skutečná práce pro
pořadí aktivací *(uhnout dřív, než jde nosič)* — táž třída jako `C5`, `Q05`
a `Q24` *(vacate-first)*. ⚠️ Ale **není to už dominantní případ**; dominantní je
teď *„nosič stojí, ač má vpřed volno" (71,5 %)*, a to je **jiná otázka** a jiné
patro *(volba, ne geometrie)* — nesmí se to slít do jednoho.

## A9. ⭐⭐⭐ `P27` — NACHYSTAT PŘÍJEMCE DOPŘEDU, „JAKO ELFOVÉ" *(uživatel 10.09.)*

Uživatel 10.09.: *„P27 teď píšeš nově — a to patří do celotahu."* ⇒ Zapsáno sem.
Odkaz v `task_queue.md` na to ukazoval už od 27.08. *(„druhá, skutečná oprava
patří do CELOTAH")*, ale **obsah tady nikdy nebyl** — jen ukazatel do prázdna.

**Odkud to přišlo.** `P27` začalo jako *„`BLITZ_AND_SCORE` se nabízí a
nekonvertuje"* a **dvakrát změnilo diagnózu**:
1. ⛔ **Stall to NEVYSVĚTLUJE** *(27.08.)* — strop prioru 0,02 při vedení
   pokrývá jen **30 z 1 281** nabídek *(2,3 %)*. Domněnka „je to záměr, doktrína
   stall" **padla na číslech**.
2. ⛔ **Nebyla to vada ve VOLBĚ, ale v ADMISI** — nabízelo se to tam, kde se
   dojít nedalo *(944 kol, nosič ve VŠECH dál než `MA + 2 GFI`, průměr 10,0
   pole; TD 0,6 % a **bylo jedno, co nosič udělal**)*. Opraveno jako `T5.35a`
   *(`12b7137d`)*.

⭐⭐⭐ **A TEPRVE ZBYTEK JE TA SKUTEČNÁ VĚC — A JE CELOTAHOVÁ.** Uživatel 27.08.
řekl *„je to neřešitelné — pozdě"*, a měření mu dalo za pravdu tvrdě:
v **93,6 %** těch kol **není v dosahu endzóny NIKDO z jedenácti**.

⇒ ⛔ **V okamžiku, kdy se rozhoduje o skórování, je už rozhodnuto.** Není koho
poslat — a to se nedá spravit lepší volbou v tom kole. Musí se to spravit
**o několik kol dřív**, tím, že tam někdo **je nachystaný**. To je celotah
v nejčistší podobě: rozhodnutí, jehož hodnota se projeví až za tři aktivace.

⏰ **Co do celotahu z `P27` patří:** kdo je **určený příjemce** · kolik těl se
smí vyčlenit dopředu místo do klece · v kterém kole se to má stát *(a jak to
váží proti tempu nosiče)* · a jak se ta investice pozná od plýtvání.

⚠️ **Souvisí, a nesmí se to slít:** `A5`/`BLITZ_AND_SCORE` říká, že engine
**umí provést** dvoučlennou sekvenci, ale **neumí ji najít**. `P27` je totéž
o patro dřív: neumí ji **připravit**. A `P62` *(elfí plán — sebrat míč a týž tah
skórovat)* je ta samá schopnost u soupeře.

## A10. ⭐⭐⭐ LEAP: DVĚ ČÁSTI SEM, JEDNA DO BLITZE *(uživatel 10.09.)*

Uživatel 10.09.: *„zapiš to leap k celotahu, ať to tam pak je"* a k rozdělení:
*„vyhodnoť, jestli tuto část nepředat do sekce blitz."* ⇒ Vyhodnoceno, **sem
patří dvě části ze tří**. Plné měření: `evidence/leap_opportunity_20260910.md`.

### (A) „PŘESKOČENÍ ZDI" — sem, a je to PŘEDČASNÉ, ne vyvrácené

> Uživatel: *„část leap o přeskočení zdi budeme řešit až v celotahu — kde
> nejdříve trpaslík postaví zeď."*

Změřeno 10.09. na 4 396 aktivacích Wardancera *(200 her)*: cíl **nedosažitelný
chůzí** má jen **0,2 %** aktivací, **a v kombinaci s nosičem 0 ze 4 396**.
⛔ **Ale to číslo neříká „přeskakovat zeď je bezcenné" — říká, že v korpusu
skoro žádná zeď nestojí.** A to je vlastnost NAŠEHO enginu: zeď je týmová
struktura *(`A6`)*, kterou engine neumí postavit ani udržet *(`A8`)*.
⇒ ⭐ **`A-tvrdé` je PODMÍNĚNÉ MĚŘENÍ:** platí *„dokud zdi nestojí"*. **Až
celotah zdi postaví, MUSÍ se to přeměřit** — teprve pak to bude odpověď.
⚠️ Riziko toho pokusu se přeměřením nemění a je vysoké: `7−AG` ⇒ AG4 hodí 3+,
**selže 33,3 %**, nezdar = leží **v cílovém poli** + brnění + **turnover**,
AV7 ⇒ **13,9 % pokusů končí hodem na zranění**.

### (C) „1× ZA KOLO" JE ZDROJ CELÉHO TAHU — patří k `A7`

`leapUsedThisTurn` je **turnový zdroj**, přesně jako **týmový reroll**: cenová
funkce na jedné hraně nesmí předpokládat, že je volný, protože o něj soutěží
celý tah. ⇒ **Táž nevyřešená věc jako `A7`**, a dvouvrstvová Dijkstra
z `3bd48fc2` ukázala i hranici: stav *„zdroj ještě mám"* se do Dijkstry přidat
dá, ale **jen jako aproximace**, ne exaktně.
⭐ A pravidla to prohlubují *(uživatel 10.09.: „na leap neplatí dodge reroll —
jen team RR")*: dodge zachrání **hráčův vlastní** reroll zdarma, leap **jen
týmový** ⇒ leap platí **sdíleným** zdrojem, dodge nikoliv.

### (B) „LEAP DO KLECE NA BLITZ NOSIČE" — ⛔ SEM NEPATŘÍ, patří do BLITZE

⇒ **Vyhodnoceno 10.09.: je to JEDNO TĚLO, JEDNA AKTIVACE.** Žádná koordinace:
**soupeřova klec už stojí**, nic nestavíme *(na rozdíl od (A))*, a v korpusu
stojí běžně ⇒ **žádná závislost na celotahu**. Veškerá mašinérie je blitzová:
doběh `nextStepTowardAdjacent` *(M14b)* a blok.
⛔ **Zaparkovat to sem by znamenalo pověsit ho za celou chybějící vrstvu**,
přitom v okruhu BLITZ je to implementovatelné dnes. ⇒ **Vedeno v `task_queue.md`
pod BLITZEM jako `B8`**, sem jen tento ukazatel.

### ⛔⛔⛔ A VADA, KTEROU SI MUSÍŠ PŘEČÍST, NEŽ CELOTAH SÁHNE NA LEAP

*(uživatel 10.09.: „nezapomeň na ni pak u blitz a celotah u leap")*

> **`macro_actions.cpp:2494` — když je `leapWalkArm` ON, `movePlayerToward`
> použije STARÝ HLADOVÝ `findMoveToward` místo nasazeného BFS `nextStepToward`.**

⇒ Rameno mění **DVĚ věci naráz**: leap jako kandidát **a** regresi obecného
pohybu na hladový výběr. **Jakýkoli běh s ním zapnutým měří jejich SOUČET**
a přiřkne ho jednomu jménu — táž třída jako noc `Q3`, která se proto musela
dělit na `16`/`17`.

⭐ **Rozhodnutí uživatele 10.09.: rameno se NECHÁVÁ ZAPARKOVANÉ** *(varianta „b")*,
**neopravuje se a neruší.** Důvod, proč se neruší: je to **jediná cesta, kterou
se `LEAP` dostane do makra** *(nabídka v `rules_engine.cpp:82` existuje, ale MCTS
bere makra, ne holé akce)* ⇒ zrušením by LEAP spadl do **mrtvého kódu** jako
před 24.08. *(`F12`)*.

⇒ ⛔ **Až celotah zdi postaví a bude se `A-tvrdé` přeměřovat *(viz (A) výš)*,
NESMÍ se to měřit přes `leapWalkArm`** — buď se ta dvojitost napřed opraví
**jako samostatná změna s vlastním ověřením**, nebo měření dostane vlastní bránu.

## A11. ⭐⭐⭐ MÍČ MEZI VÍC NEPŘÁTELI — NEZVEDAT, PŘIVÉST VÍC SVÝCH *(uživatel 12.09.)*

**Doslova:** *„když je míč v sousedství více nepřátel, je i po odsunutí blitzem
jednoho z nich možná zvednutí stále nebezpečné — pak je třeba jen postoupit
k míči více našimi a ne pokoušet se o nepravděpodobné zvednutí."*

⛔ **Proč to nejde rozhodnout po jednom hráči.** Zvednutí má práh `7 − AG − 1 +
počet cizích zón zachycení`. Každý soused míče přidá **+1**. Blitz odsune
JEDNOHO — a to je jediný blitz, který tým za kolo má *(`isOncePerTurn`)*.
⇒ Při dvou a víc sousedech zůstane práh vysoký **i po blitzu**, takže tah,
který vypadá jako příprava, je ve skutečnosti utracený zdroj za nic.

⭐ **Správná odpověď je tahová, ne akční:** *„letos ne"* — přivést k míči víc
svých, obsadit okolí, a zvedat **až příští kolo**, kdy soupeřovy zóny ubyly
nebo je náš hráč kryje. To je přesně rozhodnutí, které jednotlivá akce
neumí učinit, protože jeho cena i výnos leží **mimo ni**.

⚠️ **Dnešní kouč to neumí a nemá kde:** rozhoduje hráč po hráči, každou akci
oceňuje zvlášť a nemá kam zapsat záměr *„tohle kolo míč nezvedáme"*.
Dnešní oprava `c72b46cd` *(zvednutí se oceňuje podle šance)* ho jen **odradí
od beznadějného hodu** — ale nenahradí ji tím, aby místo toho stavěl převahu.

**Co bude potřeba změřit, až na to dojde:**
1. Podíl kol, kdy míč leží u **dvou a více** soupeřů *(tam tahle situace nastává)*.
2. Kolik z těch kol dnes končí **neúspěšným zvednutím** = turnover.
3. A protiotázka: jak často soupeř míč mezitím **sebere sám**, když čekáme.

---

## A12. ⭐⭐ PRORÁŽET PŘED KLECÍ, AŤ MÁ KAM *(uživatel 12.09., odloženo sem)*

**Doslova:** *„a ostatní hráči mají prorážet kupředu, ať je tam pak místo
na posun klece."*

⇒ Klec má pět hráčů; **zbylých šest má jít napřed a uvolnit cestu**, aby
formace měla kam postoupit. Není to clona *(ta je zvlášť — bránit soupeři
cestu ke kleci)*, je to **otevírání prostoru před klecí**.

⛔ **Proč to nejde udělat v akci:** hráč, který prorazí, tím sám o sobě nic
nezíská — cenu má **až v tom, že o kolo později projede klec**. Jeho tah se
tedy nedá ocenit z jeho vlastního výsledku, což je přesně definice celotahu.
A platí to i obráceně: prorážet má smysl **jen tam, kudy klec opravdu
pojede**, takže obojí musí vzniknout v jednom plánu.

⭐ **Souvisí:** `A11` *(míč mezi víc nepřáteli — přivést víc svých)*,
`P48 SCREEN PRO ELFA`, a `PHP36` v enginové frontě *(clona)*.

**Co změřit, až na to dojde:** kolik kol klec **nemá kam** postoupit
*(všechna pole vpřed obsazená nebo ohrožená)* — bez toho se nepozná,
jestli proražení něco koupí.

---

## B. OTÁZKY, KTERÉ Z TOHO PLYNOU *(k projití spolu)*

* **Pořadí aktivací.** C4 ukazuje, že záleží — kdo jde první? Dnes se bere, co přijde *(ověřuje audit 02.09.)*.
* **Kdy je akce investice a kdy útrata?** C1 a C3 jsou tatáž otázka z různých stran.
* **Co je „hotový tah"?** Kdy je lepší nechat aktivaci nevyužitou než ji utratit špatně.
* **Kde končí plán a začne improvizace?** Když makro selže uprostřed tahu — co dál?

---

## C. MÍSTO NA DALŠÍ

*(sem se dopisuje průběžně; nesnažit se to uzavřít)*
