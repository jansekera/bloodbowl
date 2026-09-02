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
| **C2** | **Tři možnosti vstávání.** zůstat vedle *(blok zdarma, neomezený)* · odejít *(blitz 1×)* · zůstat ležet *(faul 1×)*. ⚠️ Vedle nosiče je zůstání **cena za něco**. | ✅ **měří se dnes** — `Q3/ODPOVED`: 23,9 % ran blitzem, zbytek blok zdarma | delta ramene Q3 *(noc 02.09.)* |
| **C3** | **Rozpočet blitzu.** Jediná akce s limitem 1/kolo — kdy si ho **schovat**? | na jak **hodnotný cíl** blitz padne; kolikrát zůstane neutracen *(dnes 25,1 %)* | zda se blitz drží do okamžiku, kdy je nejdražší pro soupeře |
| **C4** | **Nosiči zavazí VLASTNÍ hráči** *(149/149)*. ⇒ napřed uhnout, pak jít. | kolikrát je pole před nosičem obsazené **naším** tělem | zda pořadí aktivací tu překážku odstraní dřív |
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

## B. OTÁZKY, KTERÉ Z TOHO PLYNOU *(k projití spolu)*

* **Pořadí aktivací.** C4 ukazuje, že záleží — kdo jde první? Dnes se bere, co přijde *(ověřuje audit 02.09.)*.
* **Kdy je akce investice a kdy útrata?** C1 a C3 jsou tatáž otázka z různých stran.
* **Co je „hotový tah"?** Kdy je lepší nechat aktivaci nevyužitou než ji utratit špatně.
* **Kde končí plán a začne improvizace?** Když makro selže uprostřed tahu — co dál?

---

## C. MÍSTO NA DALŠÍ

*(sem se dopisuje průběžně; nesnažit se to uzavřít)*
