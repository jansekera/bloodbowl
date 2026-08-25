# AUDIT METRŮ: JEDNOSTRANNOST MĚŘIDEL (Fable, 21.08.2026)

**Zadání:** `evidence/fable_brief_metric_audit_20260821.md` — rozhodnout, jestli tři
nálezy z 21.08. (`favorable_blocks`, `corridor_resistance`, `corridor_strength`)
jsou TŘÍDA vad, nebo tři náhody; najít zbytek třídy.

**Stav dokumentu:** PRŮBĚŽNĚ DOPLŇOVÁN. Pokud na konci chybí řádek `HOTOVO`,
audit byl useknut.

**Metoda:** čtení kódu (engine se nepřestavuje, dlouhé běhy se nespouští;
víkendový sběr běží na 10/12 jader). Čísla, pokud nějaká, jen ilustrativně
ze starého korpusu `corpus_baseline_20260819_data` (JINÝ engine — oprava
vstávání 21.08. → nejsou to platné hodnoty).

---

## Pracovní deník

- [ ] (a) 73 rysů hodnotové funkce — `engine/src/feature_extractor.cpp`
- [ ] (b) diagnostické metry TurnLog + `diag_*.py` / σ-tabulka
- [ ] (c) kontroly K*
- [ ] druhá osa: počet/kvalita, stav/změna, dosah/vzdálenost
- [ ] verdikt + tabulka + pořadí oprav + co potřebuje měření

*(sekce níže se plní průběžně)*

---

## 0. Klíčový strukturní fakt: jedna perspektiva, žádné duální vyhodnocení

Než má smysl mluvit o zrcadlech: **hodnota listu se VŽDY počítá z jediné
perspektivy hledající strany** a nikde se nekombinuje v(my) − v(soupeř):

- `mcts.cpp:257` / `mcts.cpp:305`: `extractFeatures(state, perspective, …)` →
  `valueFn_->evaluate(...)` → `tanh`. Jedno volání.
- `macro_mcts.cpp:928`: totéž, blend s ručně psanou heuristikou
  (`simulate()`, `macro_mcts.cpp:729`), která je také psaná z jedné perspektivy.

⇒ **Rys bez zrcadla není stylistická volba, ale skutečná slepota**: soupeřova
verze veličiny do čísla, kterým řadíme naše tahy, nikdy nevstoupí. (Soupeřovy
tahy uvnitř stromu se hodnotí toutéž perspektivou hledajícího.)

---

## (a) 73 rysů hodnotové funkce — `engine/src/feature_extractor.cpp`

`NUM_FEATURES = 73` (feature_extractor.h:8). Plná tabulka; „✓ pár" = zrcadlo
existuje jako samostatný rys, „neutrální" = veličina není stranová (skóre-rozdíl,
počasí, bias…) nebo je antisymetrická z konstrukce (z druhé perspektivy dává
tutéž informaci).

| # | rys | strana | zrcadlo existuje? | záměr / díra |
|---|---|---|---|---|
| 0 | score_diff | rozdíl | — | neutrální ✓ |
| 1/2 | my_score / opp_score | obě | ✓ pár | ✓ |
| 3 | turn_progress | moje hodiny | — | záměr (herní čas) |
| 4/5 | my/opp_standing | obě | ✓ pár | ✓ |
| 6/7 | my/opp_KO | obě | ✓ pár | ✓ |
| 8/9 | my/opp_injured | obě | ✓ pár | ✓ |
| 10/11 | my/opp_rerolls | obě | ✓ pár | ✓ |
| 12/13 | i_have_ball / opp_has_ball | obě | ✓ pár | ✓ |
| 14 | ball_on_ground | — | — | neutrální ✓ |
| 15 | carrier_dist_to_td | **jen náš nosič** | ✗ (při soupeřově držení konstanta 0,5) | **DÍRA** — kontinuální hloubka soupeřova nosiče neexistuje; jen binární [42] |
| 16 | ball_in_my_half | antisym. | — | záměr |
| 17/18 | my/opp_avg_x | obě | ✓ pár | ✓ |
| 19/20 | my/opp_avg_ST | obě | ✓ pár | ✓ |
| 21 | cage_count (ortogonální klec) | **jen náš nosič** | ✗ (soupeřova klec jen diagonály [58]) | **DÍRA** (částečná) — soupeřovu ortogonální klec nevidíme |
| 22 | is_receiving | antisym. | — | záměr |
| 23 | is_my_turn | antisym. | — | záměr |
| 24–26 | weather | — | — | neutrální ✓ |
| 27 | blitz_available | jen my | ✗ | záměr-ish (flag soupeře je mimo jeho kolo bez významu); poznámka níže |
| 28 | pass_available | jen my | ✗ | dtto |
| 29 | bias | — | — | neutrální ✓ |
| 30/31 | my/opp_sideline_frac | obě | ✓ pár | ✓ |
| 32 | turns_remaining | moje hodiny | ✗ | záměr (vlastní hodiny) |
| 33 | score_adv_with_ball | **jen „my vedeme a držíme"** | ✗ | **DÍRA** — „soupeř vede a drží míč" (zrcadlové nebezpečí) jako interakce neexistuje |
| 34 | carrier_near_endzone | jen náš | ✗ | **DÍRA** — soupeřův nosič ≤3 od naší endzóny není vidět (jen hrubě přes [42]) |
| 35 | stall_incentive | jen my | ✗ | díra menší / hraniční záměr (shaping); „soupeř stalluje proti nám" neexistuje |
| 36/37 | my/opp_avg_AV | obě | ✓ pár | ✓ |
| 38/39 | my/opp_avg_AG | obě | ✓ pár | ✓ |
| 40 | carrier_tz_count | **tlak NA NÁS** | ✗ | **VELKÁ DÍRA** — NÁŠ tlak na JEJICH nosiče v rysech vůbec není (obrana je pro hodnotovou hlavu skoro neviditelná; částečně kryje jen [62]) |
| 41/42 | scoring_threat / opp_scoring_threat | obě | ✓ pár | ✓ |
| 43/44 | my/opp_engaged_frac | obě | ✓ pár | ✓ |
| 45/46 | my/opp_prone_stunned | obě | ✓ pár | ✓ |
| 47 | free_players | jen my | ✗ (odvoditelné jen nelineárně z [5]·(1−[44])) | díra malá (pro lineární hlavu neodvoditelné) |
| 48/49 | my/opp_block_frac | obě | ✓ pár | ✓ |
| 50/51 | my/opp_dodge_frac | obě | ✓ pár | ✓ |
| 52 | guard_fraction | **jen my** | ✗ — soupeřův Guard se v extraktoru ANI NEPOČÍTÁ (smyčka řádky 146–174) | **DÍRA** — soupeřovy asistence proti nám neviditelné |
| 53 | mighty_blow_fraction | **jen my** | ✗ dtto | **DÍRA** — přesně případ z briefu: cena „stal jsem se cílem ST6+MB" nemá nosič |
| 54 | claw_fraction | **jen my** | ✗ dtto | **DÍRA** — AV9 trpaslíka proti Claw týmu je iluze; nevidíme |
| 55 | regen_fraction | jen my | ✗ | díra menší — cena zranění soupeře s Regen je nižší, nevidíme |
| 56/58 | cage_diagonal / opp_cage_diagonal | obě | ✓ pár | ✓ (⭐ důkaz, že konvence zrcadlení platila i v „nových" rysech) |
| 57 | cage_overload_risk | jen my | ✗ | díra menší — soupeřova přetížená klec = naše chain-push šance, nevidíme |
| 59 | carrier_can_score | jen náš nosič | polopár s [66] | **nesouměrná definice**: [66] žádá „mimo TZ" a bere každého hráče, [59] TZ ignoruje a bere jen nosiče |
| 60 | pass_scoring_threat | jen my | ✗ | **DÍRA** — soupeřova přihrávková hrozba neexistuje; navíc kvalita: nekouká na TZ příjemce |
| 61 | frenzy_trap_risk | jen my (riziko) | ✗ | díra menší — soupeřův Frenzy jako naše past-příležitost nevidíme |
| 62 | screen_between_ball | jen obrana (my mezi jejich nosičem a naší EZ) | ✗ | **VELKÁ DÍRA** — „kolik SOUPEŘŮ stojí mezi NAŠÍM nosičem a jejich endzónou" NEEXISTUJE = chybějící koridor v hodnotové funkci (přesně doktrína „trpaslík zeď prolomí / elf oběhne") |
| 63 | carrier_blitzable | hrozba pro nás | ✗ | **VELKÁ DÍRA** — „jejich nosič blitzovatelný NÁMI" (šance na sack) neexistuje |
| 64 | surfable_opponents | naše příležitost | ✗ | **DÍRA** — naši hráči surfovatelní soupeřem nevidět ([30] kryje jen „stojí na čáře", ne dosah) |
| 65 | favorable_blocks | naše příležitost | ✗ | **DÍRA (doloženo, brief)** — soupeřovy 2+kostkové bloky proti nám neexistují |
| 66 | one_turn_td_vulnerability | hrozba pro nás | polopár s [59] | nesouměrná definice (viz [59]) |
| 67 | loose_ball_proximity | rozdíl | — | neutrální ✓ |
| 68 | deep_safety_count | jen my (obrana) | ✗ | díra střední — JEJICH safeties za naším nejhlubším hráčem (průchodnost jejich obrany) nevidíme |
| 69 | isolation_count | jen my (riziko) | ✗ | díra menší — izolovaní soupeři = naše cíle, nevidíme |
| 70 | loose_ball_dist_to_td | antisym. | — | neutrální-ish ✓ |
| 71 | my_nearest_to_ball | jen my (absolutní) | ✗ (rozdíl nese [67]) | záměr-ish; odvoditelné v pásmu ±5 z [67]+[71] |
| 72 | pickup_clear | TZ soupeře na míči | ✗ | díra malá — naše TZ na míči proti JEJICH sběru nevidíme |

### Bilance (jmenovatel = 73 rysů)

- **34/73** rysů tvoří 17 zrcadlených párů (konvence zavedená a viditelná).
- **13/73** je neutrálních/antisymetrických z konstrukce.
- **26/73** je jednostranných; z toho hodnotím **~6 jako záměr/hraniční**
  (3, 16, 22, 23, 27, 28, 32, 35, 71 — některé sporné) a **~20 jako díry**,
  z toho **4 velké**: [40] (náš tlak na jejich nosiče), [62] (koridor před
  naším nosičem), [63] (sack-příležitost), [52–54] (soupeřovy bojové skilly
  se ani nesbírají).

### Generativní mechanismus (proč je to třída, ne náhody)

V základním bloku [0–55] je zrcadlení normou (16 z 17 párů je tam).
V „novém strategickém" bloku [56–69] ze 14 rysů je zrcadlený **jediný**
([56]/[58]); zbylých 12 je jednostranných. **Nové rysy se přidávají
z perspektivy problému, který se zrovna řešil, a zrcadlo se nepřidá nikdy.**
To je přesně profil generativního mechanismu třídy vad, ne tří náhod.

Druhý mechanismus: smyčka přes soupeřovy hráče (řádky 146–174) sbírá jen
podmnožinu toho, co smyčka přes naše (Guard/MightyBlow/Claw/Regen/Frenzy/
SureHands u soupeře chybí) — kdo přidává skill-rys, přidá ho jen do „naší"
smyčky.
