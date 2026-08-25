# NÁVRH ZADÁNÍ — LEAP: jak se má plánovat *(DRAFT, k domluvě 25.08. ve 14:00)*

⛔ **NESPOUŠTĚT bez domluvy.** Uživatel 25.08.: *„zatím jej nespouštěj —
domluvíme se dnes ve 14:00."*

## Otázka, kterou to má zodpovědět

**Ne „naimplementuj Leap".** Leap se umí **provést**; neumí se **naplánovat**.
Zadání zní: **kudy má Leap vstoupit do plánování, aby se nevyrobila nová vada
rodiny B** *(plánovač oceňuje jiný pohyb, než resolver provede)*.

## Stav ověřený v kódu 25.08. — z čeho se vychází, ne co se má znovu hledat

| vrstva | stav |
|---|---|
| **resolver** | ✅ `resolveLeap` — `move_handler.cpp:225`, správný proti BB2016 ř. 8270-8283, tři zelené testy |
| **nabídka** | ✅ od 24.08. — `rules_engine.cpp:46-60`, `ActionType::LEAP`, `Player::leapUsedThisTurn` *(limit 1×/kolo, ř. 8283)*, dispatch `action_resolver.cpp:190` |
| **N7 dva GFI** | ✅ opraveno 24.08. — skok stojí 2 pole ⇒ na deficitu dvou potřebuje **DVA** hody GFI *(ř. 1701)* |
| **N6 Tentacles/Shadowing** | ✅ opraveno 24.08. — Tentacles skok jmenují výslovně *(ř. 8586-8587)*, Shadowing platí při opuštění TZ „for any reason" *(ř. 8456-8458)* |
| ⛔ **makro** | **CHYBÍ** — a **korpus hraje PŘES MAKRA**, takže wardancer pořád neskočí |
| ⛔ **pathfinder** | **o Leapu neví** — `canReachAdjacentTo` *(`pathfinder.cpp:20`)* ani `getValidMoveTargets` *(`:111`)* ho neumí ⇒ **dosah wardancera se počítá, jako by tělo v cestě bylo neprůchodné** |

## Co je PŘEDEM ROZHODNUTO — nerozporovat, jen respektovat

**(1) Leap NENÍ nový `MacroType`.** Uživatel 24.08.: je to **primitivum
v generování cesty** *(„přeskoč tělo a pokračuj")* uvnitř `ADVANCE` / `SCORE` /
`REPOSITION`. Vlastní `MacroType` patří Gaze a TTM, ne Leapu. Dnešní seznam má
14 maker a **nemá se rozšiřovat**.

**(2) Leap pomáhá SOUPEŘI, ne nám.** V korpusových sestavách TV1200 ho mají
**jen wood-elfí wardanceři** *(`roster.cpp:583,585` — MA8 AG4, Block+Dodge+Leap)*;
trpaslík **žádný**. ⇒ **Až to zapneme, naše chess čísla PŮJDOU DOLŮ**, a to je
správně — implementujeme pravidlo, ne výsledek. *(Táž situace jako F6/F7/F8
u přihrávek.)* **Musí to být předregistrované, jinak se to přečte jako
„zhoršili jsme AI".**

**(3) Změna musí jít za VYPNUTELNÉ RAMENO.** Jako `setBlitzLandingArm` u P35:
default OFF + čítač, který tiká jen při skutečné změně volby ⇒ nulový test
a párové A/B. **Bez ramene se to nedá změřit a nesmí se to zapnout.**

## ⛔⛔ HLAVNÍ BRZDA: nevyrobit druhé P35

Dnes v noci měříme **B1/P35** — vadu, kde **plánovač oceňuje blok z jiného pole,
než z jakého se hází**. Leap má na to ideální podmínky, aby tu vadu zopakoval:

- když Leap vstoupí do **dosahu** *(pathfinder)*, ale ne do **chůze**, kterou
  executor cestu doopravdy projde ⇒ plán slíbí cíl, kam se nedojde
- když vstoupí do **chůze**, ale ne do **ceny** ⇒ hráč skočí, ale plánovač si
  myslel, že to bylo zadarmo

⇒ **Návrh musí výslovně říct, která tři místa se mění SPOLU a proč se nedají
změnit odděleně:** *dosah* · *chůze* · *cena rizika*.

## Cena, kterou musí návrh umět ocenit

Skok **není krok navíc, je to hod**. BB2016 ř. 8270-8283 — ověř si to v souboru,
nepiš to z hlavy:
- stojí **2 pole** pohybu, cíl je **prázdné pole do vzdálenosti 2**
- hod na **AG bez modifikátorů** *(výjimka Very Long Legs)*
- **neplatí se dodge** za opuštění výchozího pole ⇒ z obklíčení je skok
  **levnější** než dodge, a to je jeho hlavní hodnota
- ⛔ při **nezdaru** jde hráč k zemi **v cílovém poli** + hod na brnění;
  wardancer je **AV7** ⇒ nezdar je drahý
- **1× za kolo** *(`leapUsedThisTurn`)* ⇒ plánovač nesmí počítat s druhým

## Co má návrh vrátit

1. **Kde přesně** se sahá *(soubor:řádek)*, a proč právě tam.
2. **Jak se skok ocení** v `estimateApproachFailChance` / cestě — včetně toho,
   že nezdar končí **ležícím tělem a hodem na brnění**, ne jen ztrátou pohybu.
3. **Návrh ramene** *(jméno, čítač, co má počítat)*.
4. ⭐ **ZADÁNÍ NA MĚŘENÍ STROPU, které jde udělat OFFLINE na korpusu, ještě než
   se napíše řádka kódu:** v kolika kolech má wardancer situaci, kde by mu skok
   otevřel cíl, jenž je dnes nedosažitelný? ⇒ **řekne, jestli je to velká vada,
   nebo hygiena** — přesně jako M9 u P31.
5. **Rizika a co se může pokazit**, včetně toho, co se probudí *(24.08. lekce:
   „latentní" = „odložené"; zapojení nabídky Leapu okamžitě probudilo N6 a N7)*.

## Co NEDĚLAT

- **nepsat kód**, nespouštět testy ani engine
- **neodpovídat z vlastní znalosti Blood Bowlu** — citovat `rules_bb2016.txt`
  s číslem řádku *(tři doklady z 24.08., kdy AI text popsal naše STARÉ chování)*
- nerozšiřovat `MacroType`
- neodhadovat dopad čísly, která nejsou změřená

## Otevřené, co má rozhodnout domluva ve 14:00

- **(a)** Fable, nebo Opus? *(T5.21 jel na Opus, protože šlo o odolání prioru;
  tady jde o návrh architektury — jiný typ úlohy.)*
- **(b)** Má návrh rovnou obsahovat i **strop změřený offline**, nebo jen jeho
  zadání a strop si změřím sám?
- **(c)** Patří sem i **A2 Jump Up** *(blok z lehu, ř. 8200-8204 — ležícímu se
  BLOCK vůbec nenabízí)*? Je to táž třída „akce existuje, makro ji neemituje",
  ale jiná dovednost.
