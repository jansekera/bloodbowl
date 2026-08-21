# ZADÁNÍ PRO FABLE 21.08. — Je kritérium klece BRZDA, a je brzda RASOVÁ?

## 0. Proč tohle zadání vzniklo

Noc P40 (placebo = P38 bez `cageScoreForSquare`) doběhla 21.08. 01:21.
Podle **předregistrovaného** pravidla čtení vyšlo „nález se nejmenuje klec":

| rameno | delta chess (dvoustranně) | σ |
|---|---|---|
| P38 (s kritériem klece) | +0,0827 ± 0,0065 | +12,80 |
| P40 placebo (bez něj) | +0,0699 ± 0,0066 | +10,58 |

rozdíl **+0,0128 ± 0,0093 (1,38σ)**, což je **uvnitř** prahu 0,015 ⇒ pravidlo
říká „přejmenuj P38 na boční volnost".

**⛔ Jenže souhrnná delta ten rozdíl zprůměrovala přes dvě rasy, které se
chovají opačně.** Dopočet z `diag_*_rows.jsonl` obou nocí (2 × 13 600 řádků):

| rameno | trpaslík TD/hru | wood-elf TD/hru |
|---|---|---|
| P38 | 0,4401 → **0,5378** (+0,098) | 0,5513 → **0,6466** (+0,095) |
| placebo | 0,4279 → **0,4913** (+0,063) | 0,5674 → **0,6731** (+0,106) |
| **cena kritéria klece** | **+0,0343 ± 0,0139 (+2,46σ)** | −0,0104 ± 0,0157 (−0,66σ) |

Přímé srovnání ramen (bez odečtu základny):
trpaslík **+0,0465 ± 0,0101 (+4,59σ)** · wood-elf **−0,0265 ± 0,0112 (−2,37σ)**.

**Hypotéza dne: kritérium klece je BRZDA. Brzda má hodnotu jen pro toho, kdo
nemá rychlost — trpaslíkovi platí, elfovi škodí.** Sedí to na tvůj vlastní
rozklad z 20.08.: kritérium klece rameno **blokuje ve 2 z 5 idle kol**
(picky 58,9 % proti 97,9 % u placeba), a na uživatelův diskriminátor
*trpaslík PROLOMÍ / elf OBĚHNE*.

⚠️ Je to **post-hoc rozpad na 2,46σ**, ne předregistrovaná hypotéza.
Zadání je ji **rozhodnout, ne potvrdit**. Falzifikace je plnohodnotný výsledek.

## 1. Co si nejdřív ověř (můj dopočet můžeš vyvrátit)

Dekódoval jsem `rows` takto — **ověř, ať nestavíme na špatném čtení**:
matchup je vždy `race_h=dwarf` vs `race_a=wood-elf`; `cand_home` říká, na které
straně běželo rameno; `cand` = skóre ramene, `base` = skóre základny v témž
zápase. Tedy:
* trpaslík **s ramenem** = `cand` v řádcích `cand_home=true`
* trpaslík **bez ramene** = `base` v řádcích `cand_home=false`
* wood-elf s/bez = zrcadlově.

Kontrola, kterou to musí projít: takto dekódované číslo P38 dá
0,4401 → 0,5378 a 0,5513 → 0,6466, což se **shoduje s číslem zapsaným
20.08.** z nezávislého zdroje.

⚠️ **Noci mají RŮZNÉ seedy** — základny se shodují ve 46,1 %, což je
náhodná shoda. Srovnání P38 vs placebo je tedy **nepárové** a nese plný šum.
Základny se ale posunuly **proti nálezu**: trpaslíkova je u P38 vyšší
(+0,0122, brzdí P38 deltu), elfova nižší (−0,0160, nafukuje P38 elfí deltu).
⇒ **Skutečná asymetrie je ≥ ta v tabulce.** Ověř i tenhle argument.

## 2. Tři otázky, všechny rozhodnutelné BEZ nové noci

### Q-A — Rozděl blokované picky podle rasy a podle toho, co bylo alternativou
Pusť `diag_p38_decomposition_20260820.py` (už je rasově dělený) nad
`corpus_baseline_20260819_data` a v kolech, kde **placebo pole najde a P38 ne**,
zjisti **jaké to pole bylo**:
* `corridor_resistance` toho pole vs pole, které vybralo P38 (když nějaké našlo);
* vzdálenost k lajně (U1: lajna je soupeřova asistence zdarma);
* postup k EZ (`prog`), který se zablokováním obětoval.

**Predikce hypotézy:** u **trpaslíka** má blokovaný pick systematicky **vyšší
odpor koridoru a/nebo je blíž lajně** — tj. brzda ruší výběh do zdi.
U **elfa** je blokovaný pick **stejně dobrý nebo lepší** než to, co zbude ⇒
brzda tam ruší legitimní obejití. Kdyby to vyšlo u obou rasy stejně, hypotéza
o brzdě **padá** a rozdíl v nocích je šum.

### Q-B — Je trpaslíkův zisk právě tou vrácenou 40 % kol?
Spočítej, kolik z trpaslíkových idle kol brzda vrátí, a porovnej velikost
s naměřeným +0,034 TD/hru. Tj. **stačí ta kola na ten efekt, nebo je efekt
větší, než kolik brzda vůbec ovlivní?** Když je efekt větší než mechanismus,
nese ho něco jiného a musíme to najmenovat.
⭐ Použij `corridor_resistance` (má **−9,6σ** v σ-tabulce) jako most mezi
mechanismem a výsledkem. Připomínka z 20.08.: `cageScoreForSquare`
o odporu koridoru **neví nic** — takže když brzda funguje přes odpor, dělá
to **náhodou**, a jde to nahradit levnějším a rasově neutrálním pravidlem.

### Q-C — Otestuj konkurenční vysvětlení (attrition)
Z řádků: trpaslíkova attrition je pod placebem **vyšší** (`home_attr` 1,043
vs 0,989 u P38, +5,4 %). Elfova beze změny (2,914 vs 2,899).
Nese trpaslíkův zisk **klec**, nebo prostě to, že pod placebem **víc chodí
a víc schytá**? Rozhodni to — pokud attrition, je správná náprava **P42**
(zákaz končit kolo v kontaktu, K38 už stojí), ne kritérium klece,
⭐ **a P42 by byla rasově NEUTRÁLNÍ**.


### Q-D — ⭐⭐ MÁME KLEC VŮBEC IMPLEMENTOVANOU SPRÁVNĚ? (doplněk uživatele 21.08.)

⛔ **Tohle se ptej DŘÍV než na to, jestli klec pomáhá** — protože pokud
`cageScoreForSquare` neměří klec, jsou Q-A až Q-C otázky o něčem jiném
a celý rasový nález může být nález o vadné implementaci (větev (ii) výše).

**Pravidlo klece, jak ho zapsal uživatel 19.08., je KONJUNKCE tří klauzulí:**
`4 rohy ∧ všechny rohy ČISTÉ ∧ nosič BEZ dalších sousedů`.
⚠️ Třetí klauzule se už jednou ztratila: kontrola **K29⭐ hlásila 12,3 %**
místo skutečných 2,7 %, protože jí chyběla — a měla useknutý jmenovatel
(291× stálo vedle nosiče ležící soupeř).

**Přečti `cageScoreForSquare` (`macro_actions.cpp`, kolem ř. 1426) a
`expandCage` a odpověz bod po bodu:**
1. **Jsou tam všechny tři klauzule?** Zvlášť ta třetí — *nosič nemá kromě
   rohů žádného dalšího souseda*.
2. **Je to konjunkce, nebo se to někde SČÍTÁ do skóre?** ⭐ σ-tabulka
   18.08. konjunkci rozložila na sčítance a **proto neviděla nic**
   (rohů ČISTÝCH −6,8σ, ale rohů celkem −0,2σ). Pokud i rameno skóruje
   aditivně, kupuje si 3 rohy za cenu 4 a **hraje jiné pravidlo než spec**.
3. **Rozlišuje STOJÍCÍ a LEŽÍCÍ soupeře?** Pravidlo 20.08. říká, že to je
   kategorický rozdíl: **stojící soused dá blok zdarma, ležící stojí BLITZ,
   a blitz je jeden za kolo.** ⛔ Ale u nás `resolveStandUp` nenastaví
   `hasActed`, takže ležící se postaví za 3 MA a **hned blokuje za obyčejnou
   akci** (to je P45, opravuje se dnes). ⇒ **Řekni, jestli rameno počítá
   ležícího jako bezpečného** — pokud ano, měří klec v prostředí, kde ta
   bezpečnost neplatí, a **systematicky ji podhodnocuje**.
4. **Kotví se rohy na `carrier.position` v okamžiku PROVEDENÍ?**
   Pokud makro CAGE proběhne PŘED ADVANCE, staví se klec kolem starého pole
   (to je X3 a zatím to nikdo nezměřil, protože `TurnLog` makra neloguje).
   Z korpusu to možná jde odhadnout nepřímo — zkus, a když ne, řekni to.
5. **Chybí `cageScoreForSquare` odpor koridoru?** (20.08. ověřeno: **neví
   o něm nic**.) Tedy vybírá pole slepé k tomu, jestli leží za zdí.
6. ⭐ **A hlavní otázka nakonec: kdyby byla klec implementovaná PODLE SPEC,
   blokovala by pořád ve 2 z 5 idle kol?** Pokud ta 40% brzda vzniká
   z VADY (aditivní skóre, ležící počítaní jako hrozba, špatné kotvení),
   pak „brzda" není doktrína, je to bug — a rasová asymetrie z §0 je
   **artefakt, ne nález**.

⛔ **Neopravuj kód.** Tohle je audit: napiš, co je špatně a jak moc to mění
čtení §0. Opravy jdou přes uživatele (dnes se staví P45 a P37).

## 3. ⚠️ Napětí, které musíš přiznat, ne obejít

Uživatelovo pravidlo z 20.08. zní: **klec je univerzální ochrana nosiče
a rasová je jen její CENA.** Tenhle nález říká, že rasový je i **UŽITEK**.
Existují dvě větve a zadání ti dává **obě**, ne jen tu pohodlnou:

* **(i) pravidlo se zapsalo příliš široce** — klec je univerzální jako *cíl*,
  ale `cageScoreForSquare` není klec, je to *brzda*, a brzda univerzální není;
* **(ii) `cageScoreForSquare` prostě neměří klec** — pak elfí ztráta není
  nález o kleci, ale o vadné implementaci, a klec zůstává univerzální.

Uživatel 21.08. dodal třetí možnost, kterou ber vážně:
⭐ **„elf teoreticky může zkusit i SCREEN místo klece — ale klec je univerzální
pravidlo s jasným zadáním."** Tedy: pokud elf klec neplní, je otázka, jestli
hraje **něco jiného a smysluplného** (screen — řídká clona, která kupuje
vzdálenost místo obklopení), nebo **nic**. To jde změřit: v elfích kolech,
kde brzda pick zablokuje, vypadá výsledná formace jako **screen**
(≥3 naše stojící těla mezi nosičem a nejbližšími soupeři, rozestup > 1,
nesousedící s nosičem), nebo jako **rozsyp**?
⚠️ **Nedefinuj screen tak, aby vyšel** — napiš definici PŘED měřením
a vytiskni jmenovatel.

## 4. Data, která máš k dispozici (všechno leží na disku, nic se nesbírá)

* `placebo_20260820/dw-we_s*/diag_placebo_rows.jsonl` — 13 600 řádků, mode 7
* `cageadvance_20260819/dw-we_s*/diag_cageadvance_rows.jsonl` — 13 600, mode 6
* `corpus_baseline_20260819_data/` — **3 000 her**, `turn_logs` mají
  `corridor_resistance`, `plan`, `home_players`/`away_players`, `events`
* `corpus_mirror_20260819_data/` — 750 her (zrcadlo)
* `diag_p38_decomposition_20260820.py` — tvoje replika P38 i placeba, **rasově
  dělená**; `diag_rules_checks_20260812.py` jako modul `R`
* `evidence/night_prereg_20260820.preds`, `placebo_20260820/chain.log`
* **zdrojáky enginu** — `engine/` (`macro_actions.cpp`, `action_resolver.cpp`,
  `move_handler.cpp`) a **spec** v `evidence/` — pro Q-D

⛔ **Engine NEPŘESTAVUJ a žádnou noc NESPOUŠTĚJ.** Tohle je celé analýza
nad ležícími daty.

## 5. Co má být na výstupu

0. ⭐ **Verdikt k Q-D NEJDŘÍV**: měří `cageScoreForSquare` klec podle spec,
   nebo něco jiného? Na tom stojí platnost všeho ostatního.
1. **Verdikt k hypotéze o brzdě**: platí / padá / nerozhodnuto — a čím.
2. **Rozhodnutí pro Q8** (nasadit P38?) ve třech tvarech, s doporučením:
   nasadit všem · nasadit podmíněně (podle čeho — rasa? max MA? odpor
   koridoru?) · nenasadit a jít rovnou na P42.
3. **Jestli se P38 má přejmenovat** — a pokud ano, na co.
4. ⚠️ **Každý podíl s vypsaným jmenovatelem** (lekce 13.08./18.08.), a u každého
   metru řekni, jestli není **vyhladovělý** (σ 0,0 u pravidla klece 19.08. bylo
   přesně tohle).
5. **Co bys chtěl změřit a nejde to z těchto dat** — jmenuj to jako zadání
   pro noc, ne jako závěr.
