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
