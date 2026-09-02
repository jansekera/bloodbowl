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

## B. OTÁZKY, KTERÉ Z TOHO PLYNOU *(k projití spolu)*

* **Pořadí aktivací.** C4 ukazuje, že záleží — kdo jde první? Dnes se bere, co přijde *(ověřuje audit 02.09.)*.
* **Kdy je akce investice a kdy útrata?** C1 a C3 jsou tatáž otázka z různých stran.
* **Co je „hotový tah"?** Kdy je lepší nechat aktivaci nevyužitou než ji utratit špatně.
* **Kde končí plán a začne improvizace?** Když makro selže uprostřed tahu — co dál?

---

## C. MÍSTO NA DALŠÍ

*(sem se dopisuje průběžně; nesnažit se to uzavřít)*
