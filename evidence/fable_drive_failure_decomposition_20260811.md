# ZADÁNÍ (Fable, 11.08.2026): Proč naše přijímací drivy končí bez TD?

## Proč tohle a proč teď

Změřeno 11.08. na 1600 hrách (`era_measure_20260810/*_post`, D-vlna 1):

| | trpaslík vs skaven | trpaslík vs wood-elf |
|---|---|---|
| trpaslík dá **0 TD** | **65 %** zápasů | **82 %** |
| nejčastější výsledek | **0:0** (31 %) | **0:0** (41 %) |
| soupeř dá 0 TD | 57 % | 55 % |
| výhra / remíza / prohra | 26 / 40 / 34 % | 14 / 46 / 40 % |

**Obranu máme dobrou, útok nulový.** Spočítaná páka: kdyby se 0:0 změnilo
v 1:0, výhra jde z 26 % na ~57 % (skaven) a ze 14 % na ~55 % (wood-elf).
⇒ **Proměnit jediný drive stojí ~30 procentních bodů.** Nic jiného na našem
seznamu se tomu neblíží.

**Otázka tedy zní: KDE PŘESNĚ se ten drive láme?** Potřebujeme rozklad na
příčiny, ne další agregát.

---

## Úkol

Napiš nástroj `diag_drive_failure_20260811.py`, který každý **trpasličí
drive** zařadí do právě jedné příčiny, a spočítej jejich rozdělení.

### Kategorie (vzájemně výlučné, v tomto pořadí)
| # | kategorie | definice |
|---|---|---|
| A | **SKÓROVALI** | v drivu je naše TOUCHDOWN událost |
| B | **NIKDY JSME MÍČ NEZÍSKALI** | v celém drivu jsme míč ani jednou nedrželi |
| C | **ZTRATILI JSME HO** | drželi jsme ho a na konci drivu ho drží soupeř / leží volný |
| D | **DOŠLA KOLA** | na konci půle míč držíme, ale nejsme v endzone |

### ⭐ Pro kategorii D rozliš dvě různé vady — to je jádro zadání
Uživatel předpovídá (zapsáno PŘED během, 11.08.): *„odhaduju, že máme
pomalý pohyb klece v útoku pro poločas, který máme na útok."*

Rozhodni mezi:
* **D1 POZDNÍ START** — kol, která zbývala od prvního držení míče, bylo
  málo i při doktrinálním tempu. Test: `zbývalo_kol × 2,61 < zbývalo_polí`.
* **D2 POMALÁ KLEC** — kol bylo dost při doktrinálním tempu, ale dosažené
  tempo bylo nižší. Test: `zbývalo_kol × 2,61 ≥ zbývalo_polí` a přesto TD nepadl.
* (mohou nastat obě; vykaž i překryv)

`2,61 pole/kolo` = doktrinální nutné tempo (pochod 20,9 pole ÷ 8 kol).

**Konkurenční předpověď (moje, zapsáno rovněž předem):** myslím, že bude
dominovat **B + D1** (míč získáváme pozdě nebo vůbec — první držení bylo
v kole 4,1 a ve 20 % drivů jsme míč nedostali), ne D2. Uživatel čeká D2.
**Předpovědi jsou rozlišitelné — to je smysl tohohle měření.**
Nesnaž se vyhovět ani jedné.

### Doplňkové rozpady pro každou kategorii
* kdo nesl míč (Runner / Longbeard / Blitzer / Troll Slayer)
* kolik kol drive trval, v kterém kole jsme míč získali poprvé
* průměrné tempo nosiče a průměrný odpor v koridoru
* přijímali jsme, nebo jsme se k míči dostali jinak?
* u C: čím jsme ho ztratili (neúspěšný dodge / blok / fumble / GFI / …)

---

## Data

**Primárně:** `diag_replay_mine_20260811_data/` — čerstvý korpus na
aktuálním buildu, 120 her, **v každé hře je trpaslík**, obě orientace,
soupeři skaven/wood-elf/ork/člověk. Sbírá se od 08:16, marker
`COLLECT_DONE` v adresáři. Konfigurace TV1200, MCTS-100, vf_blend=0.0.

**Než dosbírá,** postav a odlaď nástroj na `diag_replay_mine_20260730_data/`
(24 her, z toho 10 s trpaslíkem — na závěry je to málo, na ladění dost).
**Čísla vykazuj z čerstvého korpusu.**

Formát dat: `{seed, home_race, away_race, home_score, away_score,
turn_logs[]}`, kde každý turn_log má `half, turn, active_team, ball_x,
ball_y, ball_held, ball_carrier_id, turnover, touchdown, weather,
home_players[], away_players[], events[]`.
Hráč: `{id, x, y, state (0=stojící,1=ležící,2=omráčený,3=mimo), has_ball,
name, ma, st, ag, av}`. Událost: `{type, player_id, target_id, from_x,
from_y, to_x, to_y, roll, success, die1, die2}`.

### ⚑ Dvě věci, které ti ušetří chyby
1. **Konec kola JE v datech.** `turn_logs[i+1]` je snímek pořízený hned po
   konci mého kola — nerekonstruuj konec kola z událostí (hráč, který se
   nehnul, žádnou událost nemá). **Výjimka:** kolo, po kterém následuje TD
   nebo poločas — tam mezi tím proběhne přestavění hřiště.
2. **Dělení na drivy** dosud dělá heuristika (reset čísla kola). Uděláš-li
   to spolehlivěji (TD událost + poločas + výkop), napiš jak.

Endzone: HOME útočí na `x=25`, AWAY na `x=0`. Půle má 8 kol
(`zbývá_kol = 9 − turn`).

---

## Co odevzdat
`evidence/fable_drive_failure_decomposition_report_20260811.md`:
1. **Tabulka A/B/C/D1/D2** s počty a procenty, per soupeř.
2. **Verdikt k předpovědím** — potvrzena uživatelova (D2), moje (B+D1),
   nebo ani jedna? Řekni to rovnou a čísly.
3. **Tři nejčastější konkrétní situace** v dominantní kategorii —
   s pozicemi, ne jen statistikou. Uživatel chce situace, ne matematiku.
4. **Co z toho plyne pro pořadí prací** — které z položek [1] roh klece
   v TZ, [2] tempo jako funkce odporu, [3] dopad opravy rozestavení má
   největší dopad, a proč.
5. **Čeho sis všiml, na co jsem se nezeptal.**

## Hranice
* **Nic neměň v enginu ani nespouštěj zápasy** — souběžně běží měření
  balíku G na 6 jádrech z 12 a sběr korpusu na 4. Jsi **čtenář dat**.
* Vše pod `nice -19`.
* Nevymýšlej prahy zpětně; ty dva testy výše jsou předregistrované.
* Když ti něco v datech nesedí, **napiš to** místo obcházení.
