# P25 — AUDIT MĚŘICÍCH NÁSTROJŮ: „počítá to, co tvrdí jeho jméno?"
*(17.08.2026; druhé kolo po `project_bloodbowl_measurement_audit_20260813`)*

První audit (13.08.) mířil na **kontroly plnění** — chyběl jmenovatel a prázdná
množina se hlásila jako „splněno". Dal `Check(ok, n, deg)` + N/A. **Tenhle míří
na diagnostické nástroje samotné.** Za jediný den se jich našlo osm.

---

## ⭐ NÁLEZ Č. 1: ČÍTAČ PŘECEŇOVAL 186×

`takeDauntlessRollCount` měl v hlavičce napsáno *„kolikrát se od posledního
volání **OPRAVDU hodil** Dauntless"*. Není to pravda: blok se resolvuje
**i uvnitř každé simulace MCTS**, takže se počítadlo zvyšuje i tam.

| | na zápas |
|---|---|
| co hlásil čítač | **349** |
| co se **opravdu odehrálo** *(SKILL_USED / Dauntless v logu, 3 000 her)* | **1,88** |
| | **poměr 186×** |

*(Z 5 651 skutečných hodů srovnalo sílu **73,8 %**; aspoň jeden padl v **60,3 %**
zápasů. To jsou čísla, která šla citovat celou dobu — jen je nikdo nespočítal,
protože se věřilo čítači.)*

⛔ **A padá s tím druhá věta téhož komentáře:** *„rozdíl mezi nabídkou
a odebráním říká, jestli si search nabídnutý blok vzal."* **Neříká.** Obě čísla
jsou z vnitřku prohledávání — poměřovaly se dvě veličiny téhož druhu a otázku
„nabídli jsme a vzal si to?" nezodpoví ani jedna.

**Opraveno přejmenováním, ne komentářem** — jméno bylo to, co lhalo:

| bylo | je |
|---|---|
| `takeDauntlessOfferCount` | `takeDauntlessOfferEvalsInSearch` |
| `takeDauntlessRollCount` | `takeDauntlessRollEvalsInSearch` |
| `takeHandOffOfferCount` | `takeHandOffOfferEvalsInSearch` |

K čemu ta čísla **jsou**: *„spustilo se to rameno vůbec?"* Nula = obě ramena
běžela na stejném kódu ⇒ pravý null. Nic víc z nich nečíst.
Odehrané akce se čtou **z logu událostí** — `SKILL_USED` s `roll ==
SkillName::Dauntless`, resp. `HAND_OFF`.

---

## NÁLEZ Č. 2: ČÍSLO CESTOVALO BEZ SVÉ DEFINICE

Kategorie drivů **A/B/C/D1/D2** byly celou dobu definované správně — jenže
v hlavičce `.py`. Čte se ale `drives.txt`, a tam stálo **holé „A B C D1 D2"**.
⇒ „A" se čte jako *„šance skórovat"*, ačkoli je to **podíl DRIVŮ**.
**Opraveno: legenda i s jmenovatelem se tiskne do výstupu.**

## NÁLEZ Č. 3: `n` SE TISKNE, JEDNOTKA `n` NE

Dvě kontroly vedle sebe mají jmenovatele lišící se 5× a čtou se jako
srovnatelné:

| | n | jednotka |
|---|---|---|
| K33 | 44 078 | **kola** |
| K30 | 207 962 | **příležitosti k dodgi** |

⇒ *„25,6 % drahých dodgů je držených"* **není** „ve čtvrtině kol".
**Opraveno: legenda jednotek nad tabulkou.** Zároveň měl `plán: NOT_CONSULTED`
jako jediný řádek procento **bez jmenovatele** — doplněn.

---

## NÁLEZY, KTERÉ VYŠLY DŘÍV TÝŽ DEN *(uzavřené, tady jen pro úplnost)*

| # | co | kde |
|---|---|---|
| 4 | pár není tatáž hra ⇒ práh splnilo mrtvé rameno | **P20** |
| 5 | korpus a baseline na jiném enginu | **P22** |
| 6 | binárka tiskla práh `+0.03`, dokument `±0,02`; hlavička attrition tvrdila `cageAdvance` v mode 4; řádek „vyskočilo to rameno?" netiskl nikdo | **P24** |
| 7 | Q1 sweep počítal `HAND_OFF` a `PASS` do jednoho čísla | **P21** |
| 8 | „12 nezávislých nullů" z 13.08. bylo **8 unikátních** — zbytek bit-identické duplikáty | Fable |

---

## ⭐ PRAVIDLO, KTERÉ Z TOHO PLYNE

> **Ke každému číslu patří jmenovatel a věta, co se počítá — a musí cestovat
> S NÍM, ne zůstat v komentáři zdrojáku.** Nástroj, který to nemá, se dřív nebo
> později přečte jako něco jiného; osm případů za jediný den.

A druhé, konkrétnější: **jméno funkce je dokumentace, kterou lidé opravdu
čtou.** `…Count` se čte jako „kolikrát jsme to udělali". Když to tak není,
musí to říct **jméno**, ne komentář pod ním.

## ⚠️ CO ZŮSTÁVÁ OTEVŘENÉ

* **Nezaručeno, že to byly poslední.** Prošly tři čítače, `diag_rules_checks`
  a `diag_drive_failure` — tedy nástroje, které se používají v nočních bězích.
  **Ostatní `diag_*` (přes 20 souborů) projité nejsou.**
* **Rebuild za běhu není hlídaný.** `night_preflight` odmítne *spustit* běh se
  starou binárkou, ale nic nebrání přeložit binárku, **kterou právě běžící běh
  používá**. Dnes to prošlo jen díky tomu, že linker starý inode odlinkuje
  a běžící proces si ho drží (`/proc/PID/exe` → `(deleted)`). **Spoléhat se na
  to nelze.**

---

# ⭐⭐⭐ DRUHÁ VLNA: NÁSTROJ, KTERÝ ŘADÍ CELOU FRONTU
*(`diag_drive_predictors_20260813.py` — vyrobil tabulku „CO PŘEDPOVÍDÁ TD")*

Tahle tabulka je v knize úkolů uvedená slovy *„podle čeho se řadí zbytek"*.
Stojí tedy na ní pořadí veškeré další práce. Audit našel **čtyři věci**.

## ① Oprava atribuce TD ze 14.08. šla jen do JEDNOHO ze dvou sourozenců

`14c7d035` *(„touchdown patří tomu, kdo ho dal, ne tomu, kdo byl na tahu")*
sáhla **jen** na `diag_drive_failure`. Tenhle skript dál dělal
`logs[i]["active_team"] == ours` — a `scored` je přitom **cílová proměnná celé
regrese**, ne vedlejší statistika.
✅ **Opraveno** (`td_scorer_side`).
⚠️ **Dopad: ŽÁDNÝ** — na obou zkoušených korpusech vyjde tabulka bit po bitu
stejně. *(Poctivý negativní výsledek: chyba byla skutečná, ale nic nepohnula.)*

## ② Strana se hádala ze jmen, ačkoli je rasa přímo v datech

`ours = "home" if "Longbeard" in prvních_třech_jmen else "away"`. Kdyby první
tři domácí byli Blitzeři, skript by **tiše měřil soupeře jako nás**.
✅ Opraveno na `home_race == "dwarf"`.

## ③ ⭐ TABULKA SE NEREPRODUKUJE NA JINÉM KORPUSU TÉŽE VELIKOSTI

| veličina | `20260811b` *(to, co je v knize)* | `20260813_gate` |
|---|---|---|
| K9a tempo | **4,2σ** | 4,0σ |
| **bloků na kolo** | **2,7σ** | **5,6σ** |
| **K33 (blok ano/ne)** | **0,6σ** | **4,0σ** |
| čistota rohů K29 | **2,6σ** | 1,2σ |
| FB2 (K35) | 2,6σ | 3,6σ |
| **REACH0 jako počet** | **−1,8σ** | **−4,0σ** |
| špinavé rohy | −2,2σ | −1,9σ |
| Δx | 2,4σ | 3,7σ |
| *počet rohů* | *−0,2σ* | *−1,5σ* |

195 vs 194 drivů — **prakticky týž rozsah, a přesto se tři položky hnou
o 2σ a víc.** Korpusy se liší i enginem, takže část toho může být skutečná
změna. **Právě to je ale ten problém: z čísla v knize se nepozná, co z toho je
co**, protože tam stojí bez korpusu, bez data a bez commitu.

## ④ ⭐⭐ A ten rozsah je mnohem menší, než se čte

Doplnil jsem tisk původu — a vyšlo najevo, že `20260811b` je **120 HER**.
Tabulka, která řadí veškerou práci, tedy stojí na:

* **120 hrách**, 195 „plných" drivech, a hlavně jen **35 drivech se skórováním**;
* **11 veličinách porovnávaných naráz, bez jakékoli korekce** — při 35 kladných
  případech je pár položek kolem 2,5σ přesně to, co čekáme od náhody;
* **filtru „jen plné drivy ≥7 kol", což je VÝBĚR, ne vzorek** — drive, který
  skončí dřív, se do tabulky nedostane, a „skončí dřív" koreluje s výsledkem.
  *(Fable to 14.08. pojmenoval; tady je vidět, jak malý ten základ je.)*
* engine **starší nejméně o šest commitů** než ten, co dnes běží.

⇒ **Ta tabulka není zjištění o hře. Je to jeden malý vzorek jednoho starého
enginu.** Používat ji dál jako pořadí priorit je totéž jako číst `cand_daunt`
jako počet bloků.

✅ **Opraveno v nástroji:** tiskne teď korpus, počet her, otisk enginu
(nebo „NEZNÁMÝ"), povahu filtru a jednotku `n`.

## ⇒ CO S TÍM

**Přepočítat σ-tabulku na čerstvé baseline** (`corpus_baseline_20260817`,
3 000 her, sbírá se od 17.08. 10:15, s otiskem enginu) a teprve pak
o pořadí fronty mluvit. **3 000 her proti 120** — a poprvé s uvedeným původem.

---

# TŘETÍ VLNA: `diag_exposure_scan` *(vyrobil doktrínu E1/E2)*

Nástroj v podstatně lepším stavu než předchozí dva: stranu čte z `home_race`,
tiskne korpus i počet vzorků, a **sám si přiznává vlastní zkreslení**
*(„obranné asistence u neznámé pozice útočníka ~ 0 → mírně nadsazuje jejich
kostky")*. Přesto tři poznámky.

## ① `OUT = 3` je nesprávné jméno — ale MĚŘENÍ JE SPRÁVNÉ

Enum enginu má na 3 **KO**, a dál INJURED/DEAD/EJECTED/OFF_PITCH (4–7).
Test `p["state"] == OUT` by tedy zachytil jen KO. **Ověřeno v datech:**
exportér **hráče mimo hřiště do seznamu vůbec nedává** — vyskytují se pouze
stavy 0/1/2 a seznam se zkracuje z 11 až na 5. Odchod ze hřiště se tedy pozná
**nepřítomností**, a přesně tak to skript i dělá (`st_m = q["state"] if q else
OUT`). ⇒ **jedna větev je mrtvý kód, dopad na výsledek žádný.**
*(Zapsáno i s tím „žádný" — nález bez dopadu je pořád nález.)*

## ② ⭐ HVĚZDIČKA NEMĚLA KOREKCI NA POČET SROVNÁNÍ

`*` = `|r| > 2/sqrt(n)`, tedy **~2σ na každou buňku zvlášť** — a buněk je
**36**. ⇒ **~1,8 hvězdičky se čeká i tehdy, kdyby neplatilo nic.** Z téhle
tabulky se přitom vybrala doktrína E1/E2.
✅ Opraveno: tiskne se počet srovnání i očekávaný počet planých hvězdiček,
a `**` označuje buňky, které projdou korekcí na 36 srovnání.

## ③ ✅ A DOKTRÍNA E1/E2 TU KOREKCI PŘEŽIJE

| | down | lost | ball_lost | appr |
|---|---|---|---|---|
| FB2 | **0,399** | 0,096** | 0,098** | −0,070* |
| REACH | 0,219** | 0,122** | 0,178** | −0,116** |
| **REACH0** | 0,077* | 0,097** | **0,336**** | −0,039 |
| BLZ | 0,164** | 0,080* | 0,205** | −0,241** |
| SURF | 0,032 | 0,010 | −0,007 | 0,008 |
| CCBAD | 0,005 | 0,010 | 0,165** | 0,068 |

`REACH0 → ball_lost` **0,336** je nejsilnější buňka celého sloupce a projde
korekcí s velkou rezervou. **E1/E2 stojí.** *(Korekce chyběla, ale závěr se
nemění — a to je potřeba říct stejně jasně jako opak.)*
⚠️ Naopak **SURF neměří nic** a **CCBAD skoro nic** — obojí bylo v tabulce
vedle ostatních, jako by to byly rovnocenné veličiny.
