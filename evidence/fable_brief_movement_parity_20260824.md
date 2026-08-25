# ZADÁNÍ PRO FABLE — PARITA POHYBU PROTI PRAVIDLŮM (24.08.2026)

## 0. Otázka, kterou máš zodpovědět

**Dělá náš engine pohyb tak, jak ho popisuje `rules_bb2016.txt`?**
Hrajeme **BB2016**, ne CRP/LRB6. **Každé tvrzení dolož číslem řádku** z
`rules_bb2016.txt`. ⛔ **Nikdy necituj pravidlo z hlavy** — 21.08. přinesli
agenti dvakrát správný nález se špatným zdůvodněním.

## 1. Proč zrovna pohyb, a proč TEĎ

⭐⭐⭐ **Vstávání je část pohybu — a bylo implementované špatně.** *(uživatel
24.08.)* **P45, 21.08.:** ležící hráč se postavil v **0,4 %** případů
*(1 067 z 280 719, při KAŽDÉM MA)*, protože `turn_planner.cpp` slovo `PRONE`
neobsahoval ani jednou. Oprava změnila **TD/hru 0,740 → 0,443 (−6,12σ)**.
⇒ **Rok jsme měřili klec nad pohybem, který se nedál.**

⭐⭐⭐ **A je to TŘÍDA, ne jeden případ.** Táž rodina, nalezená nezávisle:
* **P45 vstávání** — `resolveStandUp` byl hotový a měl testy; **nikdo se
  nepostavil**, protože se akce nikdy nenabídla.
* **F12 Leap** — `resolveLeap` je hotový, má **tři zelené testy**, a
  **NEMÁ ŽÁDNÉHO VOLAJÍCÍHO**. Oba wardanceři za celý rok neskočili.
⇒ ⭐ **„Resolver existuje, akce se nikdy nenabídne" je u nás opakující se vada,
a unit testy ji z principu nechytí.** Ptej se na ni u KAŽDÉ pohybové akce.

⭐⭐ **Druhá rodina, doložená dnes:** *„filtr oceňuje jinou akci, než jakou
resolver provede."* 14.08. pět nálezů; **24.08. znovu**: Wrestle se stal
volbou v resolveru, ale `scoreFace` ho neuměl ocenit *(BOTH_DOWN = 4, odsun =
5)*, takže větev byla **nedosažitelná**. ⇒ **U pohybu: oceňuje plánovač tentýž
pohyb, jaký resolver opravdu provede?**

## 2. Co projít

### (a) Základ pohybu
MA a jeho spotřeba · **GFI** *(2+, blizzard 3+)* · **Sprint** *(3 GFI)* ·
**Sure Feet** · pohyb do/z tackle zón · `pathfinder.cpp` *(zná strop GFI? zná
zakořenění?)*.

### (b) Vstávání — hlavní podezřelý
Cena **3 MA**; **MA ≤ 2 ⇒ hod 4+** *(ř. 691-693)*; **Jump Up**; interakce
s **Take Root** *(dnes opraveno: zakořeněný má MA 0 a nesmí GFI)*; a ⭐
**„stand-and-go"**: makro umí postavit hráče **jen NA MÍSTĚ**, přestože
ř. 670-671 dovolují **utratit zbytek pohybu** ⇒ je to **strop na naměřenou
hodnotu P45**. **Ověř, jestli je to pořád pravda.**

### (c) Dodge a vše, co ho modifikuje
Tabulka podle AG · modifikátory · **Tackle vs Dodge** · **Break Tackle** ·
**Two Heads** · **Prehensile Tail** · **Stunty** · **Diving Tackle**
*(⚠️ známá půlka: dává −2, ale hráč se NEPOKLÁDÁ a NEPŘESOUVÁ do opuštěného
pole, ř. 8072-8084)* · rerolly *(⚠️ od 24.08. platí „jeden hod = jeden reroll",
ř. 925-927)*.

### (d) Pohyb, který není vlastní pohyb
**Odsun** *(kdo ho volí, chain push, Stand Firm, Side Step, Grab, Frenzy)* ·
**Shadowing** a **Tentacles** *(⚠️ obojí dnes přepsáno na 2D6 podle ř. 8458-8464
a 8588-8591 — zkontroluj, že to sedí)* · **Leap** *(F12, mrtvý kód)* ·
**Ball & Chain** · **Throw Team-Mate** jako přesun těla.

### (e) Brány, které pohyb ruší
**Bone-head · Really Stupid · Wild Animal · Take Root · Blood Lust** — kdy se
hází, co propadá, a **spotřebuje se aktivace správně?** ⚠️ **P55: týmový blitz
se nespotřebuje** *(`blitzUsedThisTurn` je uvnitř `case BLITZ`, kam se při
propadlé akci nedojde)*, a **opravuje se to PER DOVEDNOST** — Bone-head o akci
tým připraví *(ř. 7981)*, **Take Root ne** *(ř. 8582)*.

### (f) ⭐ NABÍDKA, ne jen resolver
Pro **každou** pohybovou akci ověř **oba konce**:
**(1)** dělá resolver to, co říkají pravidla, a **(2) NABÍDNE SE ta akce
vůbec?** *(`rules_engine.cpp` generuje MOVE; `turn_planner.cpp` a
`macro_actions.cpp` vybírají.)*
**Grepni každý `resolve*` v pohybových souborech a najdi mu volajícího.**
To za minutu odhalí třídu P45/F12.

## 3. Výstup

Piš **PRŮBĚŽNĚ** do `evidence/fable_movement_parity_20260824.md`
*(předchozí agent umřel na limit a zachránilo ho jen průběžné psaní)*:
1. **Pracovní deník** nahoře s `[ ]` položkami, odškrtávej.
2. **Tabulka nálezů**: `# | co je špatně | pravidlo (řádek) | náš kód (soubor:řádek) | odhad dopadu | hraje to v korpusu?`
3. ⭐ **Zvlášť seznam „resolver bez volajícího / akce, která se nenabízí"**.
4. ⭐ **Zvlášť seznam „plánovač oceňuje jiný pohyb, než resolver provede"**.
5. **Pořadí oprav podle dopadu**, u každé řekni, jestli **hraje v dnešním
   korpusu** *(5 ras: dwarf, skaven, wood-elf, human, orc)*, nebo je latentní.
6. **Co nejde rozhodnout čtením** — jmenuj jako zadání na měření, ne závěr.

**Poslední řádek souboru, až budeš hotov: `HOTOVO`.**

## 4. Tvrdá omezení

⛔ **NEMĚŇ ŽÁDNÝ KÓD.** Je to audit čtením. ⚠️ **Engine se dnes aktivně
opravuje** — pravidlový balík, 10 commitů — takže **čti aktuální strom a
neopírej se o starší zápisy v `evidence/`**, které mohou popisovat už opravený
stav. Co je hotové k dnešku: TA1 rerolly · los před zápasem · TA2 Take Root ·
F11 Wrestle · volba Block/Wrestle · F6-F8 přihrávky · F9 vhazování · F10
touchback · TA8 Tentacles · TA9 Shadowing · TA5 Gaze · TA10 Blood Lust.
⛔ **Nespouštěj dlouhé běhy.** Hotová data: `crosses_20260821_data/`
*(18 000 her)*. ⚠️ Ten korpus má **známou vadu — TA1 řetěz rerollů byl v něm
živý** ⇒ dodgy a GFI v něm mají **nadhodnocenou úspěšnost**.

## 5. Formální

⛔ **Žádná čísla v kroužcích** (①②③) — piš `(1) (2) (3)`.
Názvy dovedností **anglicky**, text **česky**.
