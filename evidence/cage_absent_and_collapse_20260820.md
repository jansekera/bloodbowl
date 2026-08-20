# PROČ KLEC NESTOJÍ — a proč se rozpadá (20.08.2026, 3 000 her)

Uživatel 20.08.: *„to, že klec často nestojí vůbec, je zajímavější než
čistota… čím to je, že nestojí? nebo máme konkrétní kolo před a co se stalo
a následek neúplná klec v dalším kole? první a poslední kola vynech."*

**Obě otázky mají odpověď a obě ukazují na nás.**

---

## A. KDYŽ KLEC NIKDY NEVZNIKNE — 28 % kol *(kola 2.–7.)*

Z **20 251** kol s naším stojícím nosičem nemá nosič **ani jeden roh
v 5 671 = 28,0 %**. Proč *(kategorie výlučné)*:

| | | |
|---|---:|---:|
| **(a) naše tělo stojí ORTOGONÁLNĚ u nosiče** *(zakázané pole)* | **2 790** | **49,2 %** |
| **(d) těla by na roh DOSÁHLA, a přesto tam nikdo nestojí** | **2 752** | **48,5 %** |
| (c) nikdo nedosáhne — nosič utekl vpřed | 119 | 2,1 % |
| (b) všichni leží | 10 | 0,2 % |

⇒ ⭐ **97,7 % je VOLBA, ne nemožnost.**
U **(d)** je nejbližší tělo ⌀ **2,7 pole** daleko a **4,5 těla** by na roh
dosáhlo — *„nestihli"* to tedy není.
**(a)** je táž vada jako 19.08.: **tělo stojí jedno pole od volného rohu, ale
na zakázané ortogonále.**

---

## B. KDYŽ KLEC VZNIKNE A ROZPADNE SE — 53,9 %

Z **4 886** dvojic po sobě jdoucích našich kol, kde klec stála *(≥2 rohy)*,
jich **2 634 = 53,9 %** má v příštím kole rohů méně.

**Příčina** *(nosič se mezi našimi koly hýbe jen v NAŠEM kole ⇒ jeho posun je
naše rozhodnutí)*:

| | | z rozpadů |
|---|---:|---:|
| **(3) nosič ODEŠEL a rohy nešly s ním** ⇒ **MY** | 1 735 | **65,9 %** |
| **(1) nosič STÁL a rohy stejně odešly** ⇒ **MY** | 417 | **15,8 %** |
| (4) obojí | 283 | 10,7 % |
| **(2) nosič stál, soupeř rohy sundal** ⇒ **SOUPEŘ** | 199 | **7,6 %** |

⇒ ⭐⭐⭐ **Za 81,7 % rozpadů si můžeme sami. Soupeř má 7,6 %.**

**Dvě věci, které to utahují:**
* když nosič odejde, posune se ⌀ o **3,04 pole** — a rohy jsou Longbeardi
  s **MA 4**. ⇒ **Dojely by.** Není to rychlost, je to **že jim to nikdo
  neřekne** *(P46)*.
* kategorie **(1)** je nejhorší v tabulce: **nosič stojí, do rohů nikdo
  nemlátí, a rohy stejně odejdou.** To není rozpad, to je **rozchod**.

---

## C. Konkrétní kolo *(uživatel si o ně řekl)*

`g0022.json.gz`, 1. půle, kolo **3 → 4**:

```
  kolo 3 (my):      nosič Runner +Block na (10,6)   rohů 4/4  ← PLNÁ klec
  kolo 3 (soupeř):  BLOCK PUSH BLOCK PUSH KNOCKED_DOWN BLOCK BLOCK
  kolo 4 (my):      nosič na (12,5)                 rohů 0/4  ← nic
```

⚠️ **Podstatné: nosič se přesunul z (10,6) na (12,5).** Klec se nerozpadla
tím, že do ní soupeř mlátil — **nosič z ní odešel a rohy nešly s ním.**
Totéž ve dvou dalších vypsaných případech: (19,9)→(20,10) rohů 3→0
a (9,7)→(10,7) rohů 3→0.

---

## ⇒ Co z toho plyne

**Doklad pro P46** *(těla klece nenásledují nosiče)* — nejsilnější, jaký
zatím máme. A rozšiřuje ho: nejde jen o pohyb **do boku**, ale o **následování
vůbec**.

⚠️ **A mění to, co se má opravovat.** Dosud se klec četla jako *„stavíme ji
špinavou"*. Ve skutečnosti **z 28 % kol nevznikne** a z těch, co vzniknou,
se **54 % rozejde** — a obojí **naší volbou**. ⇒ **Čistota rohů je až třetí
problém v pořadí.**

Skripty `diag_cage_absent_20260820.py` · `diag_cage_collapse_20260820.py`.


---

# D. HYPOTÉZA O MECHANISMU — s pojmenovaným testem, ne závěr *(20.08. večer)*

Čtení kódu dává mechanismus, který **vysvětlí obě čísla naráz**. ⚠️ **Není
potvrzený** — kontrola, která by ho potvrdila, u nás neexistuje.

## Co v kódu je

1. ✅ **`expandCage` míří na čtyři diagonály správně** *(`macro_actions.cpp:1466`)*
   — engine to **umí**.
2. ⛔ Ale kotví je na **`cp = carrier.position`**, tedy tam, kde nosič stojí
   **v okamžiku provedení makra**.
3. ⛔ **CAGE a ADVANCE jsou dvě samostatná makra** a **nic nevynucuje jejich
   pořadí**.

⇒ **Zahraje-li se CAGE dřív než ADVANCE, rohy se postaví kolem STARÉHO pole
a nosič jim pak odejde.** To je přesně kategorie **(3)** — *nosič odešel
a rohy nešly s ním*, **65,9 % rozpadů**.

## A druhá půlka sedí taky

Tělo blízko nosiče, které **nedostane CAGE**, spadne v `expandReposition`
*(ř. 1040)* na:

```cpp
} else if (carrierDist <= 3) {
    // Already near carrier — move to cage/screen position ahead of carrier
    target = {carrier->position.x + dx * 2, carrier->position.y};
}
```

⇒ **Jedno jediné pole, dvě vpřed, ve STEJNÉ ŘADĚ — a pro všechna těla totéž.**
Komentář slibuje *„cage/screen position"*, kód počítá **ortogonálu**.
Kdo na `x+2` nedosáhne, zastaví na `x+1` ve stejné řadě ⇒ **stojí ortogonálně
vedle nosiče** = kategorie **(a)**, **49,2 %** kol bez jediného rohu.

## ⛔ Proč to zůstává HYPOTÉZOU

Potvrdit ji znamená vidět **pořadí zahraných maker v kole**. To je **X3
z T2.6** — kontrola, která **nikdy nevznikla** *(v paměti vedená jako
„nejlepší poměr odemčeno/cena v celém aparátu")*.

⇒ **Test, který ji rozhodne:** zaznamenat do logu **deklarovaná makra
s pořadím**, a pak se ptát: **kolik kol zahraje CAGE PŘED ADVANCE?**
* hodně ⇒ hypotéza potvrzena, oprava je **vynutit pořadí** *(nebo kotvit
  rohy na CÍLOVÉ pole nosiče, ne na současné)*;
* málo ⇒ vada je jinde a hledá se dál.

⚠️ **Nedělat opravu před tím testem.** 20.08. se třikrát ukázalo, že tvrzení
předběhlo ověření.
