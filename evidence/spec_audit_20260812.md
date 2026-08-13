# STRUKTURÁLNÍ AUDIT TRPASLIČÍ PROCEDURY — 12.08.2026

Auditováno: `dwarf_turn_procedure_spec_20260811.md` (917 řádků, ČÁSTI 0–9)
proti **vlastní kostře** — situace bez povinností, povinnosti bez kontroly,
kontroly bez nástroje, rozhodnutí bez měření, položky uzavřené jinde
a nevyškrtnuté.

> **Co tenhle audit NEHLEDÁ:** díry proti realitě a proti CRP. Ty najde
> odehraná situace a nezávislý čtenář — spec jsem z velké části psal já
> a vlastní opomenutí strukturálně nevidím.

---

## ⛔ N1 — ROZHODČÍ NEEXISTUJE. NIC SE NEKONTROLUJE.

**Nejtvrdší nález auditu.** ČÁST 4 definuje **28 predikátů** (K1–K28) a
ČÁST 5 dva nástroje, které je mají počítat:

| nástroj | stav |
|---|---|
| `diag_turn_referee_20260811.py` | **NEEXISTUJE** |
| `diag_referee_report.py` | **NEEXISTUJE** |

⇒ Z ~60 povinností katalogu se **systematicky nekontroluje ani jedna.**
Jediné měření plnění, které kdy proběhlo, je `diag_plan_compliance_20260810.py`
— zná **4 cíle**, ne S0–S10, a konec kola **rekonstruuje z událostí**, což
ČÁST 4.1 sama označuje za nepřesné (hráč, který se nehnul, nemá událost).

**A nůžky se rozevírají.** Dnešek přidal 7 povinností a 1 kontrolu; ani
jedna z nich není kontrolovatelná. Katalog roste, měření ne.

⇒ **Toto je ta „mezera v plánu", která ostatní blokuje.** Dokud rozhodčí
neběží, je každé „splnili jsme 3,7 z 8 kol" číslo o čtyřech cílech, ne
o proceduře.

---

## ⚠️ N2 — ČÁST 9 (dnešek) NEMÁ ANI JEDNU KONTROLU

R1 (roh nesmí markovat) · R2 · R3 (mimo klec značkovat, jen AG3) · R4
(bez úkolu → blíž k akci podle role) · třístupňová priorita blitzu ·
záloha u míče = potenciální nosič · `K-noblock`.

**Sedm nových pravidel, nula K-kódů.** Přitom R1 je ze všech pravidel
**nejlevnější na kontrolu, jaké v dokumentu je** — je to lookup v TZ mapě
a `diag_play_session_20260812.py` ho **už umí spočítat**. Zbývá ho jen
přenést z ruční hry do rozhodčího.

Chybějící kódy k doplnění: `K29` R1 čistota rohů (s definicí ohrožení =
stojící + ležící s Jump Up) · `K30` R3 pokrytí AG3 · `K31` R4 hráč bez
úkolu · `K32` pořadí blitzu · `K33` `K-noblock`.

---

## ⚠️ N3 — ČÁST 6 JE ZASTARALÁ: 3 ZE 7 OTEVŘENÝCH POLOŽEK JSOU UZAVŘENÉ

Uzavřely je REVIZE 1 a ČÁST 8 **v témže dokumentu**, ale v seznamu
otevřených položek zůstaly stát.

| # | stav podle ČÁSTI 6 | skutečnost |
|---|---|---|
| O3 lajna/střed | otevřená | ✅ **UZAVŘENO** revizí R2 — tlačíme k lajně |
| O4 tvar klece proti markerům | otevřená | ✅ **UZAVŘENO** 11.08. — Longbeard rohy |
| O5 výběr cíle ve zdi | otevřená | ✅ **UZAVŘENO** revizí R5b — je to výpočet, ne doktrína |
| O2 předání míče | otevřená | ⚠️ **fakticky zodpovězeno** („skoro nikdy", 11.08.) + 12.08. doplněno „záloha = potenciální nosič"; zbývá zapsat jako povinnost |
| O1 kopat/přijímat | otevřená | otevřená (R4 jen zúžila) |
| O6 nouze prorazit/držet | otevřená | otevřená — **jediná trpasličí učební úloha** |
| O7 Underworld | otevřená | otevřená, nízká |

⇒ Seznam otevřených položek je **nedůvěryhodný**, a to je horší než dlouhý
seznam — nedá se podle něj plánovat.

---

## ⚠️ N4 — X-SEZNAM JE TAKÉ ZASTARALÝ: X6 Z VELKÉ ČÁSTI PŘISTÁL

Commit `31efa93` („record what the turn planner decided") přidal
`TurnPlanRecord` (`engine/include/bb/turn_plan_record.h`), navěšený na
`game_simulator.h:78`. Obsahuje `goal`, `verdict`, `adopted`,
`distToEndzone`, `turnsLeft`, `requiredPace`, `achievablePace`, `step`,
`resistance`, `filledCorners`, `openCorners`, `carrierGfi`, `exposure`.

| kód | stav podle ČÁSTI 4.3 | skutečnost |
|---|---|---|
| **X6** plán enginu | „nelze" | ⚠️ **z ~80 % HOTOVO**; chybí jen `resistanceIds[]` — máme `int8_t resistance` (počet, ne kdo) |
| X1 `isBlitz` na BLOCK | nelze | **potvrzeno chybí** — `GameEvent` to pole nemá |
| X2 počet kostek bloku | nelze | **potvrzeno chybí** |
| X3 deklarovaná makra + pořadí | nelze | chybí |
| X4 pořadí rizika | nelze | plyne z X3 |
| X5 legální makra | nelze | chybí |
| X7 GFI bez důvodu | nelze | plyne z X6 ⇒ **skoro dosažitelné** |

⭐ **X2 + X3 jsou úzké hrdlo pro ZÁKAZY.** Bez nich se nedá zkontrolovat
**Z4, Z5, Z9, Z14** ani povinnosti **S2.14** a **S10.3**. Jedna oprava
odemkne šest pravidel — nejlepší poměr v celém dokumentu.

---

## ⚠️ N5 — K9 KONTROLUJE NĚCO JINÉHO, NEŽ CO S2.7 PŘIKAZUJE

* **S2.7** (po revizi 1): *kvóta postupu **JE FUNKCÍ ODPORU**, ne konstanta*
* **K9**: `Δx ≥ ceil(zbývá / (9−turn))` — **konstanta**

Kontrola neimplementuje povinnost, kterou má hlídat. Kdyby rozhodčí běžel,
**měřil by špatně** — a nikdo by to nepoznal, protože obojí je v dokumentu
a vypadá to konzistentně. To je horší druh chyby než chybějící kontrola.

---

## ⛔ N6 — CELÁ DIMENZE CHYBÍ: „CO TATO POZICE DÁVÁ SOUPEŘI?"

Všech jedenáct situací S0–S10 popisuje **naše kolo**. Jediné místo, kde se
dokument ptá, co pozice dává soupeři, je **S1.2** (nedat mu víc než 4 bloky)
— a to jen pro rozestavení.

**Nikde není povinnost tvaru: „na konci našeho kola smí mít soupeř nejvýš
X".** Přitom:
* dnešní past jsme našli ručně: až SBZ18 vstane, oba přední rohy jsou
  v jeho zóně a klec se bez hodů nehne — **spec pro to nemá ani slovo**
* uživatelovo pravidlo R1 je ve skutečnosti přesně tenhle druh povinnosti
  (*„kdo nás může udeřit, aniž ho to stojí blitz"*), jen formulovaný z naší
  strany

⇒ **Navrhuji novou ČÁST: „bilance soupeřova kola"** — kolik bezplatných
bloků, kolik cílů pro blitz, kolik čistých cílových polí pro dodge.
Je to jediná díra auditu, která není o měření, ale **o chybějícím způsobu
uvažování**.

---

## ⚠️ N7 — DEVĚT OBLASTÍ Z AUDITU ÚPLNOSTI 11.08. SE DO KATALOGU NIKDY NEDOSTALO

Audit z 11.08. našel 9 chybějících oblastí. Katalog S0–S10 pro ně **nemá
situaci** (slova se v próze vyskytují, povinnosti ne):

kickoff tabulka · **kolo po turnoveru** · **kolo po obdrženém TD** ·
hranice poločasu · počasí · **utrácení rerollů** · balík G · fauly · eskorta

Nejcitelnější: **po turnoveru**, **po obdrženém TD** (doktrína záporné
rezervy z 11.08. existuje v paměti, ale **není v katalogu**) a **rerolly**
— reroll je zdroj jako blitz, 8 na půli, a nikde není pravidlo, kdy ho
utratit.

---

## ⚠️ N8 — POVINNOSTI NEJVĚTŠÍ DÍRY (S5) JSOU NEKONTROLOVATELNÉ

S5 je označená za **největší měřenou díru** (96 % dosažitelnost, **53 %
pokus**). Jenže:

| povinnost | kontrola |
|---|---|
| S5.1 pokus o sběr | K1 ✅ |
| S5.2 sbírá Runner | ✅ triviální |
| S5.3 záloha u míče před hodem | K24 ✅ |
| **S5.4 zajištění, ne sebrání** | ❌ **žádná** |
| **S5.5 nejde-li S5.4 ⇒ přepni na S6** | ❌ **žádná** |
| **S5.6 nejdřív odmarkovat, pak sbírat** | ❌ **žádná** |

Tři ze šesti povinností té nejdůležitější situace se nedají ověřit —
a jsou to zrovna ty tři, které říkají **jak** sbírat, ne **jestli**.

---

## ⚠️ N9 — NEZNÁME ROZLOŽENÍ SITUACÍ

`K28` (klasifikace S0–S10) je definovaná a **neimplementovaná**. ⇒ Nevíme,
jak často která situace vůbec nastane. S5 má číslo, protože se měřila zvlášť;
**S3, S8, S9, S10 nemají žádné.** Píšeme povinnosti pro situace, jejichž
četnost neznáme — a podle toho se nedá řadit práce.

---

## ⚠️ N10 — PRAVIDLO, KTERÉ PROŠLO NÁHODOU, NENÍ OVĚŘENÉ PRAVIDLO

12.08. se **R3 splnila náhodou**: R1 spolykala všech pět pohyblivých těl
a R3 naplnila dvě zaseknutá, která shodou okolností stála u AG3 soupeřů.
**R3 nemá vlastní rozpočet těl.** Je to dluh dnešního dne, ne otevřená
položka.

Obecněji: dokument nikde nerozlišuje **„pravidlo platí"** od **„pravidlo
bylo vynuceno"**. Bez rozhodčího (N1) se to rozlišit nedá vůbec.

---

# POŘADÍ PRÁCE, KTERÉ Z AUDITU PLYNE

Uživatel 12.08.: *„ať je toto co nejvíc kompletní, než se vrátíme k učení —
a tím potvrdíme, že to učení trpaslíky ovlivní minimálně"* a *„pak teprve
budou na řadě úkoly plánované z dřívějška"*.

⇒ **Kompletace repertoáru má přednost před frontou oprav i před učením.**
Tohle pořadí to respektuje.

| | co | proč tady |
|---|---|---|
| **A1** | **N6 — bilance soupeřova kola** | jediná chybějící *dimenze*, ne měření; nejlevněji se doplňuje ROZHOVOREM, ne kódem |
| **A2** | **N7 — chybějící situace** (po turnoveru, po obdrženém TD, rerolly) | doktrína existuje v paměti, jen není v katalogu; taky rozhovor |
| **A3** | **N3 + N4 — vyškrtat uzavřené** (O3/O4/O5, X6) | levné, a bez toho se podle seznamů nedá plánovat |
| **A4** | **N5 — srovnat K9 se S2.7** | tichá chyba, opravit dřív, než podle ní kdokoli měří |
| **A5** | **N1 + N2 — postavit rozhodčího** včetně K29–K33 | odemkne ověřování všeho výše; **R1 je hotová, jen ji přenést** |
| **A6** | **N4 kalibrace proti uživateli** (20 kol, shoda ≥ 18/20) | než se agregátu uvěří |
| **A7** | **X2 + X3** (kostky bloku, deklarovaná makra s pořadím) | jedna oprava odemkne Z4, Z5, Z9, Z14, S2.14, S10.3 |
| **A8** | *pak teprve* fronta oprav `fix_queue_20260812.md` (P0…) | úkoly plánované z dřívějška |
| **A9** | *pak teprve* učení | s úplnou procedurou jako nulovou hypotézou |

**A1 a A2 jsou nejlevnější a nejdřív** — jsou to dvě odehraná sezení nebo
dvě debaty, ne kód. A přesně ty dvě zvětší repertoár nejvíc.
