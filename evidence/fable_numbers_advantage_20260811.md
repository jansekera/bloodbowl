# ZADÁNÍ (Fable, 11.08.2026 odpoledne): Proč se početní převaha nepromění na skóre?

## Rozpor, který to má vysvětlit

Po balíku G (persistence attrition, změřeno dnes na 4800 hrách) víme:

| chybí hráčů na konci zápasu | **my (trpaslík)** | **soupeř** |
|---|---|---|
| 0 | 45 % | 6 % |
| 1 | 34 % | 17 % |
| 2 | 16 % | 23 % |
| **3+** | **5 %** | **54 %** |
| průměr | 0,81 | **2,82** |

**Soupeře umlátíme v průměru o tři hráče. V 54 % zápasů hraje o tři a víc
dole, tedy v osmi a méně.**

A přesto: **trpaslík neskóruje vůbec v 65 % zápasů proti skavenům a v 82 %
proti wood-elfům.** Nejčastější výsledek je 0:0.

⇒ **Attrition funguje. Proměna nefunguje.** Tohle je největší otevřená
otázka projektu a tvým úkolem je zjistit proč.

## ⭐ HYPOTÉZA UŽIVATELE (zapsaná PŘED během)

> *„Pokud je otázka, co dělat v sedmi skavenech proti trpaslíkům — odpověď
> je utéct."*
> *„Trpaslík obklíčí málo skavenů — ti pak přes dodge utečou na druhou
> stranu."*

Tedy: **umlácený rychlý tým se přestane prát.** Vyhýbá se kontaktu, utíká
přes dodge na volnou stranu hřiště, a my ho s MA4–6 nedoženeme. Další
attrition tím není k mání a míč zůstává u nich.

**Moje konkurenční hypotéza** (rovněž předem): převaha se nepromění proto,
že ji **vůbec nezkoušíme proměnit** — pokračujeme v lovu těl místo abychom
šli po míči, tj. cíl blitzu se v režimu převahy nemění.

**⭐⭐ A UŽIVATEL TO PAK ZPŘESNIL — tohle je hlavní hypotéza:**
> *„Početní převaha se nepromění ve skóre, protože jim nesebereme míč —
> natož abychom s ním stihli dojít."*

Tedy **dvoustupňové selhání**, a druhý stupeň je čistá aritmetika:
1. **míč jim vůbec neodebereme** ⇒ převaha se nikdy nestane držením;
2. **i kdybychom ho odebrali, nedojdeme** — trpaslík potřebuje ~7 kol
   a krádež přijde pozdě a daleko.

⇒ **Testovatelná předpověď:** rozdělení *kola, ve kterém míč získáme
zpět* a *vzdálenosti k jejich endzone v tu chvíli*. Kdyby medián vyšel
kolem 6. kola a ~20 polí, je převaha **nekonvertovatelná už z principu**
a nemá smysl ladit, co s ní děláme.

**Všechny tři hypotézy jsou rozlišitelné a všechny mohou být špatně.
Nesnaž se vyhovět žádné.**

---

## Úkol

Napiš `diag_numbers_advantage_20260811.py` a zodpověz **v tomto pořadí**:

### 1. Vzniká převaha včas, nebo až na konci?
Průběžný rozdíl v počtu stojících hráčů na hřišti, po kolech.
**Kdyby převaha vznikala až v 7.–8. kole, není co proměňovat** a obě
hypotézy padají. To se musí ověřit jako první.

### 2. Chová se soupeř v převaze jinak? (hypotéza uživatele)
Pro kola, kde máme převahu ≥ 2, proti kolům, kde je vyrovnáno:
* **počet kontaktů** (bloky, marky) — klesá?
* **vzdálenost soupeřova nosiče** od nejbližšího našeho hráče — roste?
* **kolik jejich hráčů stojí v naší polovině** — klesá?
* **postup jejich nosiče k naší endzone** — zpomaluje (stalling)?
* **kolik dodgů udělají za kolo** — roste (utíkají)?

### 3. Chováme se MY v převaze jinak? (moje hypotéza)
V týchž kolech:
* míří náš **blitz na nosiče**, nebo na kohokoli?
* mění se **podíl kol, kde nosiče markujeme**?
* zkoušíme **odebrat míč** (blitz na nosiče, blok na nosiče), nebo mlátíme
  vedle?

### 4. Kolik volných polí má jejich nosič?
Uživatelovo *„obklíčíme málo"* je měřitelné: **počet volných polí kolem
soupeřova nosiče na konci NAŠEHO kola** (pole prázdné = může tam jít).
Klesá to k nule, nebo se drží vysoko i v převaze?
⚠️ **Tackle zóna pohyb nezastaví, jen ho zdraží** — past chce **obsazená
pole**, ne pokrytá. Měř obsazenost, ne zóny.

### 5. ⭐ DVA STUPNĚ SELHÁNÍ (hlavní hypotéza)
**5a — sebereme jim vůbec míč?** Podíl kol s převahou ≥ 2, ve kterých
soupeři míč odebereme. A kolikrát za zápas se o to vůbec pokusíme
(blitz/blok na nosiče).

**5b — a kdybychom ho sebrali, stihli bychom dojít?** Pro každé získání
míče vykaž **kolo** a **vzdálenost k jejich endzone**. Porovnej
s potřebou ~7 kol (medián pochodu 22 polí, dosahované tempo 1,73).
⇒ **Kolik procent získání míče je aritmeticky nezhodnotitelných?**

**5c** — podíl zápasů s převahou ≥ 2 aspoň tři kola, kde jsme neskórovali;
a u těch se skóre: přišel gól z krádeže, nebo z regulérního drivu?

---

## Data

**`diag_replay_mine_20260811_data/`** — 120 her, build z 08:16
(po opravě rozestavení, PŘED dnešními opravami klece/bloků/apothecary).
**`diag_replay_mine_20260811b_data/`** — 120 her, **dnešní build se všemi
opravami**; sbírá se od ~13:00, marker `COLLECT_DONE`, tytéž seedy.
V obou je v každé hře trpaslík, obě orientace, soupeři
skaven/wood-elf/ork/člověk. TV1200, MCTS-100, vf_blend=0.

⇒ **Hlavní čísla vykazuj z `b` (dnešní build). Kde to jde, ukaž i rozdíl
proti staršímu korpusu** — je to nechtěné A/B dnešních čtyř oprav
(rozestavení včetně náhradníků · jednokostkové bloky · apothecary).
Klecové opravy se neprojeví, brána `cageAdvance` je vypnutá.

**Formát:** `{seed, home_race, away_race, home_score, away_score,
turn_logs[]}`; turn_log má `half, turn, active_team, ball_x, ball_y,
ball_held, ball_carrier_id, turnover, touchdown, weather, home_players[],
away_players[], events[]` a **nově `plan{}`** (goal, verdict, adopted,
required_pace, achievable_pace, resistance, step, exposure…).
Hráč: `{id, x, y, state (0=stojící,1=ležící,2=omráčený,3=mimo), has_ball,
name, ma, st, ag, av}`.

**Dvě věci, které ti ušetří chyby:**
1. **Konec kola JE v datech** — `turn_logs[i+1]` je snímek hned po konci
   mého kola. Nerekonstruuj ho z událostí (kdo se nehnul, nemá událost).
   Výjimka: kolo před TD nebo poločasem.
2. Endzone: HOME útočí na `x=25`, AWAY na `x=0`; půle má 8 kol.

---

## Co odevzdat
`evidence/fable_numbers_advantage_report_20260811.md`:
1. **Odpověď na bod 1 jako první** — je vůbec co proměňovat?
2. **Verdikt k oběma hypotézám** — uživatelova (utíkají), moje (nezkoušíme),
   obě, ani jedna. Rovnou a čísly.
3. **Tři konkrétní situace** s pozicemi hráčů, ne jen statistika —
   uživatel chce situace, ne matematiku.
4. **Rozdíl mezi korpusy** — pohnuly dnešní opravy něčím?
5. **Čeho sis všiml, na co jsem se nezeptal.**

## Hranice
* **Nic neměň v enginu, nespouštěj zápasy** — jsi čtenář dat. Souběžně
  běží sběr korpusu `b`.
* Vše pod `nice -19`.
* Nevymýšlej prahy zpětně.
* Když ti něco v datech nesedí, **napiš to** místo obcházení.
