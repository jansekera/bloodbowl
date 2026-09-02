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

## A. UŽ DOLOŽENÉ Z NAŠÍ PRÁCE *(mají číslo nebo zdroj)*

| # | situace | co má celotah rozeznat | zdroj |
|---|---|---|---|
| C1 | **Blitz na souseda jako UVOLNĚNÍ** | Srazím souseda ⇒ zmizí jeho tacklezóna ⇒ zbytkem pohybu odejdu **bez dodge** tam, kde jsem potřeba. Hodnota se neměří ranou, ale **zbytkem tahu**. | uživatel 01.09.; ř. 552-553 |
| C2 | **Tři možnosti vstávání** | vstát a zůstat *(blok zdarma, neomezený)* · vstát a odejít *(blitz, 1×/kolo)* · zůstat ležet *(faul, 1×/kolo)*. ⚠️ Ale **zůstat vedle nosiče je cena za něco** — riziko nesmí přebít účel. | Q3, uživatel 20.08. a 31.08. |
| C3 | **Rozpočet blitzu** | Blitz je jediná akce s tvrdým limitem 1/kolo. Kdy si ho **schovat**? Změřeno: neutracen vůbec 25,1 % kol, ale 48,5 % padne na nosiče. | `T1.9`, spec část 14 |
| C4 | **Nosiči zavazí VLASTNÍ hráči** | Postup nosiče blokují naše těla, ne soupeřova. 149/149 vlastními. ⇒ pořadí aktivací: **napřed uhnout, pak jít**. | `M11`, 26.08. |
| C5 | **Nosič si nechává pohyb v záloze** | Šetřit pohyb má smysl jen dokud na něj soupeř nedosáhne blitzem — a dosah je `MA + GFI`, ne `MA`. | `P37`, 31.08. |
| C6 | **Obrana: dva sloupce → skok do L** | Přechod mezi fázemi obrany řídí **převaha**, ne pozice. Engine fáze nemá vůbec. | uživatel 20.08., `project_bloodbowl_phase_model_missing` |
| C7 | **Zeď: prolomit vs oběhnout** | Univerzální objekt, **rasová odpověď**: trpaslík prolomí, elf oběhne/přeskočí. | uživatel 20.08. |
| C8 | **Klec má DVA cíle naráz** | Dojít co nejdál k TD **a** chránit nosiče. Neplníme ani jeden. | uživatel 20.08. |

---

## B. OTÁZKY, KTERÉ Z TOHO PLYNOU *(k projití spolu)*

* **Pořadí aktivací.** C4 ukazuje, že záleží — kdo jde první? Dnes se bere, co přijde *(ověřuje audit 02.09.)*.
* **Kdy je akce investice a kdy útrata?** C1 a C3 jsou tatáž otázka z různých stran.
* **Co je „hotový tah"?** Kdy je lepší nechat aktivaci nevyužitou než ji utratit špatně.
* **Kde končí plán a začne improvizace?** Když makro selže uprostřed tahu — co dál?

---

## C. MÍSTO NA DALŠÍ

*(sem se dopisuje průběžně; nesnažit se to uzavřít)*
