# Dwarf playstyle gap — analýza fairtestu, dokumentace vs. kód (2026-08-03, Fable)

Zadání: prověřit hypotézu „dwarfí grind/cage styl je popsaný v dokumentaci, ale chybí v kódu,
a proto policy blend 0.2 dwarfy poškozuje (32,7 % decisive WR, z=−5,0)".
Data: `diag_policy_confirm_20260731_results.json` (N=1600), replaye `diag_replay_mine_20260730_data/`,
policy snapshot `evidence/policy_snapshot_postnoreset_20260801.json`, zdrojáky engine + pipeline.

## Exec summary

1. **Titulek „policy dwarfy poškozuje o ~17 pp" je artefakt designu testu, ne reálný efekt.** Rozvrh
   fairtestu je period-5 cyklus (`diag_policy_confirm_20260731.py:48–49`): dwarf hraje VÝHRADNĚ proti
   skavenům a wood-elfům. Per-race z-skóre se počítá proti prahu 50 % (`:84–88`), který ignoruje sílu matchupu.
2. **Párová (within-seed, mirror) analýza ukazuje opak: policy dwarfům pomáhá nejvíc ze všech ras.**
   Dwarf vs skaven: kandidát-dwarf 44,8 % vs baseline-dwarf 26,0 % decisive (+18,8 pp; párový test t=2,86, p<0,01).
   Dwarf vs wood-elf: 20,0 % vs 9,7 % (+10,3 pp; t=1,24, nesignifikantní). Žádná jiná rasa nemá větší zisk.
3. **Skutečný a velký problém je intrinsický: šampion (bez policy) dwarf je proti agility rasám katastrofální**
   (26 % decisive vs skaven, 9,7 % vs wood-elf). To je gap heuristik/plánovačů engine, ne policy.
4. **Hypotéza uživatele o dokumentace-vs-kód gapu se potvrzuje, jen na jiné úrovni:** grind doktrína
   (cage advance, L-pin, Guard-aware rohy, attrition→ball control) je detailně popsaná v
   `team1_brief_per_player.md`, ale kód implementuje jen statickou klec (expandCage plní 4 diagonály
   „nejbližším volným hráčem", nikdy klec neposouvá), žádný ofenzivní screen, žádný L-pin.
5. **Policy hlava je na macro úrovni rasově slepá — změřeno:** guard fraction 0→0,64 mění distribuci
   <1 pp; 3-dice vs 1-dice block mění logit o −0,045 (šum); risk feature je konstanta per typ makra.
   Policy korpus je z 50 % human (HOME_RACE fixní), dwarf jen 12,5 %.
6. Konkrétní situace z replayů: perfektní cage (4/4 rohy), která se 3 tahy nepohne → 0-0 (g0018);
   carrier opakovaně opouští klec na sólo běhy (g0003, g0023); 6 dwarfích tahů ležící volný míč
   při drtivé attrition převaze (g0002).

---

## 1) Dokumentace: kde je dwarfí/grind styl popsán

Primární zdroj je `team1_brief_per_player.md` (spec ~492 per-player features + taktická doktrína):

- **Rasová charakteristika** (`team1_brief_per_player.md:528`):
  > | **Dwarf** | 4-5 | 3-4 | 2 | Block, Guard, Tackle, Stand Firm | Pomalý cage, neprolomitelný, žádný Dodge |
- **Taktický styl** (`team1_brief_per_player.md:555`):
  > | **Slow plodding cage** | Dwarf, Orc, Nurgle | Pomalu posouvat cage, 8 tahů na TD | Rozpoznat neprolomitelný cage → defenzivní screen |
- **Cage advance sekvence** (`team1_brief_per_player.md`, sekce „Cage advance (pomalý tým)", ~ř. 800):
  > 1. Drž cage — 4 hráče na diagonálách kolem nosiče 2. Blitz/block soupeřovy hráče kteří blokují postup
  > 3. Posuň cage o 1-2 čtverce dopředu 4. Opakuj — cíl: 8 tahů, 1 TD
- **Trpasličí L-pozice / sideline pin** (`team1_brief_per_player.md:754–777`): celá sekce včetně ASCII
  diagramu; „Guard masivně přítomný → 2-dice bloky i bez početní výhody… Stand Firm → dwarfové z L
  nemůžou být vyblokováni zpět… Tackle → nosič s Dodge nemůže snadno vyskočit. Nízké MA nevadí — L je
  pozicová taktika, ne sprint." Implikované featury: pinnable (soupeř u sideline), escape routes, Guard v dosahu.
- **Attrition doktrína** (`team1_brief_per_player.md:731–735`): „Silné týmy (Dwarf, Orc, Chaos) řeší obranu
  přesilou: přivedou asistenty → 2-dice bloky → shazují na zem… více soupeřů na zemi = volnější pohyb."
- **Rasová podmíněnost důležitosti featur** (`team1_brief_per_player.md:539–545`): agregátní metriky přes
  5 ras zaretušují, že featura je u jedné rasy zásadní; validaci rozpadat po matchupech.
- **Cage breakdown per-player featury** (`team1_results_opus.md:180–182`): `cage_corners_filled`,
  per roh `corner_eff_st`, `corner_has_guard`, `corner_has_standfirm`, `min_dist_my_blitzer_to_weakest_corner`.
- Dwarf handoff-only hra: `team1_diagnostic_brief.md:149` („Dwarf | Cage grind, žádní catchers | Handoff maximálně").
- Roster (kód, ale definuje identitu): `engine/src/roster.cpp:536–551` — Dwarf TV1200 má **7/11 hráčů
  s Guard**, všichni Block, MA 4–6, AG 2–3; wood-elf TV1200 naopak AG4, MA7–8, Dodge/Leap (`roster.cpp:96–107`).

Závěr sekce: doktrína je popsaná bohatě a konkrétně (včetně sekvencí a navržených featur), primárně
v briefu pro per-player features — jehož implementace byla NO-GO (Fáze A retest 16.07., viz paměť).

## 2) Kód: co je implementováno a co chybí

### Co v kódu JE (grind-relevantní)

- **State features (73 agregátů, `feature_extractor.cpp`):** cage count [21] (:533), guard fraction [52]
  (:585), cage_diagonal_quality [56] (:595), cage_overload_risk [57], opp_cage_diagonal_quality [58],
  favorable_blocks [65] (:384), surfable_opponents [64] (:370), carrier_blitzable [63]. Tj. stav klece
  a Guard hustota v hodnotové funkci viditelné jsou — ale jen jako týmové průměry, žádný per-player detail
  (žádné corner_has_guard, escape routes, pinnable — spec z team1_results_opus.md neimplementována).
- **CAGE macro:** kandidát `macro_actions.cpp:361–370`, prior floor 0.12 v paritě s BLOCK
  (`macro_mcts.cpp:476–492`, fix 2026-07-03 proti vyhladovění CAGE).
- **Cage-escort bonus:** `macro_mcts.cpp:676–694` — scoringBonus odměňuje postup celé klece s carrierem
  („cage advanced, carrier screened > cage sat back") — jediný signál směrem ke grind postupu, jen v rolloutu.
- **Sideline trap náznak:** výběr blitz cíle +3/+1 skóre za soupeře u sideline (`macro_actions.cpp:400–405`)
  — jediný pozůstatek L-pin myšlenky; jde o výběr cíle blitzu, žádné obklíčení/pin follow-up.
- **Item 14:** výběr blitzera přes `estimateBlitzFailChance` (cesta + kostky, `macro_actions.cpp:1068–1078`)
  — riziko cesty zohledněno, rasově neutrální (pomáhá i dwarfům nepouštět AG2 blockera skrz TZ).
- **Item 13 (staged planner, právě ve validaci):** celo-tahový safe→PICKUP plán
  (`evidence/fable_item13_mvp_20260731.md`) — první krok k celotahovému plánování, zatím jen PICKUP cíl.

### Co v kódu CHYBÍ (dokumentováno, neimplementováno)

1. **CAGE se nikdy neposouvá.** `expandCage` (`macro_actions.cpp:1014–1050`) pouze plní 4 diagonály kolem
   aktuální pozice carriera „nejbližším volným hráčem" (max 4 kroky). Neexistuje makro „posuň klec o 1–2
   pole dopředu jako celek" (dokumentovaná sekvence Cage advance). Jediný způsob postupu je ADVANCE
   carriera — který klec opustí (viz situace v §3).
2. **Výběr hráčů do rohů ignoruje skilly.** `findNearestFreePlayer` — žádná preference Guard/StandFirm na
   rohy klece (spec `team1_results_opus.md:182`).
3. **L-pin/sideline pin neexistuje** — žádné makro, žádná featura (pinnable, escape routes).
4. **Ofenzivní screen neexistuje.** REPOSITION strategie screen/safety jsou jen defenzivní
   (`macro_actions.cpp:734–830`, Strategy 0–4 podmíněné `onDef`); na ofenzivě REPOSITION jen „support
   carrier" (cage/screen ahead, :726), bez formace.
5. **Policy macro features jsou rasově a rizikově slepé** (`macro_actions.cpp:1450–1566`):
   - [13] risk_level je **KONSTANTA per typ makra** (BLOCK=0.15, PICKUP=0.33, BLITZ=0.25…) — dwarfí 2d
     Block-vs-bez-Block blok má v policy stejné „riziko" jako elfí 1d blok bez rerollu; AG2 dodge-move
     stejné jako AG4.
   - Chybí AG hráče (v micro `action_features.cpp:42` je, ale policy blend se aplikuje na MACRO úrovni,
     `macro_mcts.cpp:358–380` používá `extractMacroFeatures`).
   - Chybí TZ kontext cíle, Guard-assist dostupnost, delta cage-completion po akci.
   - [11] block_dice_quality sice existuje, ale naučená síť ji fakticky ignoruje (změřeno, §3c).
6. **Per-player features (492) neimplementovány** — Fáze A NO-GO; state je 73 agregátů. Bimodalita a
   „poznej dwarfa ze statlin" (brief :564) tedy funguje jen přes průměry (guard fraction ano, ale hrubě).
7. **Policy trénink je human-dominantní:** `run_iteration.py:29–30` `HOME_RACE='human'`,
   `AWAY_RACE='orc,skaven,dwarf,wood-elf'`; decisions se sbírají z obou stran
   (`engine/python/bb_module.cpp:558–577`) → korpus ≈ 50 % human, 12,5 % dwarf.
8. **Pacing formule je MA-škálovaná, ale lineární:** `idealDist = turnsLeft*MA` (`macro_mcts.cpp:706–711`)
   předpokládá volnou dráhu; grind reálně postupuje ~2–3 pole/tah s boji — u MA4 týmu formule
   systematicky říká „jsi pozadu" a stall bonus je mimo. (Hypotéza, neměřeno.)

Heuristické prior floory (`macro_mcts.cpp:437–530`) jsou všechny rasově agnostické — to samo o sobě
není chyba, ale znamená to, že JEDINÝ mechanismus, kterým se může projevit rasový styl, je search
(value/rollout) — a value je 73-agregátová, policy rasově slepá.

## 3) Evidence z her

### 3a) Rozvrh fairtestu — klíčový artefakt

`diag_policy_confirm_20260731.py:48–49`: `ra = RACES[seed_idx % 5]; rb = RACES[(seed_idx+1) % 5]`
→ hraje se jen 5 párů: human–orc, orc–skaven, **skaven–dwarf**, **dwarf–wood-elf**, wood-elf–human.
**Dwarf potkává výhradně obě agility rasy; wood-elf naopak dwarfa a humana.** Per-race z v summary
se počítá proti 50 % (`:84`), takže míchá (i) efekt policy s (ii) intrinsickou silou rasy v přiděleném
matchupu. Stejný period-5 cyklus používá i gate/benchmark (`run_iteration.py:273–274, 325–326`) —
per-race čtení z gatingu má identický confound.

### 3b) Správné (párové) čtení: policy dwarfům pomáhá nejvíc ze všech ras

Každý seed se hraje 2× se stranami prohozenými (mirror; 800 párů, ověřeno group-size=2 pro všech 800).
Within-matchup, z pohledu dané rasy (decisive WR kandidát-rasa vs baseline-rasa; párový t-test na
chess skóre W=1/D=0.5/L=0):

| Matchup | cand-X decisive | base-X decisive | Δ decisive | párový Δ skóre (t) |
|---|---|---|---|---|
| **dwarf** vs skaven | 44,8 % (n=105) | 26,0 % (n=100) | **+18,8 pp** | +0,116 (t=2,86, sig.) |
| **dwarf** vs wood-elf | 20,0 % (n=100) | 9,7 % (n=93) | **+10,3 pp** | +0,047 (t=1,24, n.s.) |
| human vs orc | 65,0 % | 55,6 % | +9,4 pp | — |
| human vs wood-elf | 43,0 % | 38,6 % | +4,4 pp | — |
| orc vs skaven | 44,2 % | 43,1 % | +1,1 pp | — |

Dwarfí z=−5,0 tedy neměří škodu policy; měří, že **baseline dwarf v tomto engine proti agility rasám
skoro nevyhrává** (26 %/9,7 %). Wood-elfích 74,4 % je zrcadlově nafouknuto (proti base-dwarf 90,3 % decisive).

### 3c) Vzorce dwarfích výsledků

- **Prohry jsou těsné, ne debakly:** margin distribuce kandidát-dwarf: −1× 92, −2× 42, ≤−3× 4;
  nejčastější scorelines 0-0 (92), 0-1 (90), 1-0 (64), 0-2 (41). Dwarf skóruje 96 : 216 obdrženým na 320 her.
- **Draw rate:** dwarf 35,9 % vs orc 47,8 %, human 41,6 %, skaven 34,7 %, we 35,3 % — dwarf hry nejsou
  remízovější; nízký draw-rate je tažen tím, že soupeř skóruje (0-1/0-2 jsou 2. a 4. nejčastější výsledek).
- Mirror dwarf–dwarf se v rozvrhu vůbec nehraje.

### 3d) Sonda do policy hlavy (offline, bez her)

Snapshot `evidence/policy_snapshot_postnoreset_20260801.json` (neural, hidden 64, vstup 96 = 73 state
+ 23 macro features), forward dle `python/blood_bowl/policy_trainer.py:212–219`. Stavové vektory =
reálné stavy z `replay_buffer.pkl` (10 000 přechodů).

- **Rasová slepota:** průměrná softmax distribuce přes menu {ADVANCE, CAGE, BLOCK2d, BLITZ2d, REPO, END}
  na stavech s guard_fraction≥0,5 („dwarfí") vs ≤0,02 („agility"): rozdíly <1,5 pp ve všech položkách
  (CAGE 0,116 vs 0,121; ADVANCE 0,382 vs 0,345 — ADVANCE vede v obou). Kontrafaktuál na týchž stavech
  (force f52: 0→0,64): posun <1 pp.
- **Slepota ke kostkám bloku:** Δlogit BLOCK(3 dice) − BLOCK(1 die) = **−0,045 ± 0,13** přes 400 stavů —
  policy fakticky nerozlišuje 3-kostkový a 1-kostkový blok (jádro dwarfí zbraně).
- **Co policy tlačí:** průměrné logity SCORE −0,01 > PICKUP −0,18 > ADVANCE −0,64 ≫ BLOCK −1,17 ≈
  BLITZ −1,23 > CAGE −1,47 > END −2,19. Tedy univerzální „ball-progress" prior (agility-flavored),
  identický pro všechny rasy. Že i tak dwarfům pomáhá, znamená, že šampionova heuristika dwarfa brzdila
  ještě víc (např. přestřelený BLOCK/kontakt bez postupu) — konzistentní s g0002/g0018 níže.
- Pozn.: risk [13] má váhu (Δlogit ≈ −0,6 na +0,3 risku), ale protože je to konstanta per typ makra,
  funguje jen jako globální re-ranking typů, ne jako situační risk assessment.

### 3e) Konkrétní situace (replaye `diag_replay_mine_20260730_data/`, šampion, policy_blend=0, TV1200)

Turn-boundary snapshoty (per-akční logy k dispozici nejsou), přesto vzorce jasné:

1. **g0018 (dwarf 0-0 wood-elf) — perfektní klec, která stojí.** H1T5–T7: Longbeard+Guard carrier
   @(5,4), **cageDiag=4/4 tři tahy po sobě, pozice se nezmění ani o pole**; attrition běží
   (oppDown 3→6), ale drive skončí bez TD. Přesně chybějící „posuň klec o 1–2 pole" — engine umí klec
   postavit (floor 0.12 + expandCage), ale nemá akci, jak ji posunout; ADVANCE by ji rozbil.
2. **g0003 / g0023 (dwarf vs we) — carrier opouští klec na sólo run.** g0003 H1T7→T8: cageDiag 3→0,
   mates≤3: 9→**0**, carrier Runner sám @(22,7) (zde to vyšlo, TD); g0023 H1T8: carrier @(18,4),
   cageDiag=0, mates≤3: 2 → míč v H2 ztracen, jen 1-1. Klec se rozpadá pokaždé, když se carrier pohne —
   důsledek bodu §2.1 (cage je statická, postup jen přes sólo ADVANCE).
3. **g0002 (skaven 2-0 dwarf) — attrition dominance nekonvertovaná na míč.** H1T3–T8: míč leží volný
   na (13,2) **šest dwarfích tahů po sobě**, zatímco oppDown roste 3→7 (myDown 3). Dwarf vyhrává rvačku
   a nechává míč ležet; skaven pak 2× skóruje. (Post ball-stuck-fix korpus, tj. nejde o starý deadlock —
   míč je kontestovaný a PICKUP se nikdy neprosadí.) Přesně scénář pro item13 staged planner.
4. **g0013 (dwarf 0-1 we) H2T5: carrier Longbeard @(2,1)** — vlastní roh, cageDiag=0, 1 elf adjacent,
   1 spoluhráč do 3 polí: dwarf sám sobě vyrobil sideline pin (doktrína říká pinovat SOUPEŘE, brief :754).

Situace 1–3 jsou intrinsické chyby šampiona (blend 0) — potvrzují, že gap je v plánovačích/makrech,
ne v policy hlavě.

## 4) Návrhy fixů (seřazeno podle poměru dopad/úsilí)

**F0. Opravit per-race metodiku vyhodnocování (metodický fix, udělat VŽDY).**
Effort: XS (úprava `summarize()` + rozvrhu). Risk: žádný. Dopad: zabrání dalším chybným závěrům typu
„policy škodí dwarfům".
– (i) per-race výsledky reportovat **párově within-matchup** proti baseline téže rasy (kód z §3b);
– (ii) rozvrh rozšířit na plný round-robin (`rb = RACES[(seed_idx + 1 + seed_idx//5) % 5]` apod.),
ať každá rasa potká všechny včetně mirroru.
Validace: reanalýza existujících 1600 her (hotovo zde) + příští fairtest s round-robinem.

**F1. CAGE_ADVANCE makro — posun klece jako celku (§2.1) + Guard-aware rohy (§2.2).**
Effort: M (nové expand v `macro_actions.cpp`, kandidát když cageDiag≥2; prior floor jako CAGE; výběr
rohových hráčů preferencí Guard/StandFirm místo nearest). Risk: střední (interakce s floor rozpočtem —
viz item7 lekce o rodinné mase priorů; jedna změna najednou). Očekávaný dopad: vysoký pro dwarf/orc
(g0018-typ stallů), malý až nulový pro agility rasy (kandidát vzniká jen při postavené kleci).
Validace: A/B arm harness na dwarf matchupech (dwarf–skaven, dwarf–we, mirror), N=2×400 párovaně,
práh: párový Δ chess-skóre ≥ +3 pp (šumové dno draw-rate ±8–11 pp na N=150 ⇒ N=800 dává ~±4 pp);
plus celkový gate beze změny prahů.

**F2. State-dependent macro features pro policy (§2.5) — místo filtrování dat.**
Effort: M (rozšířit `extractMacroFeatures`: skutečný risk_level akce — dodge/GFI/pickup pravděpodobnost
z pathfinderu, AG akčního hráče, TZ na cílovém poli, Δcage-completion, Guard-assist count; NUM_ACTION_FEATURES
+~6). Risk: střední — mění vstupní dimenzi policy (nutný retrén od nuly nebo padding, `policy_trainer.py:130–135`
padding už umí). Dopad: policy se může naučit rasovou kondicionalitu implicitně (dwarfí 2d blok přestane
mít „stejné riziko" jako elfí dodge-run); dnes se to naučit NEMŮŽE, ať je korpus jakýkoli.
Validace: retrén policy na stávajícím decision korpusu → top1-agreement per rasa (levné, offline);
pak fairtest N=1600 round-robin, párové per-race čtení; práh: celkové decisive WR ≥ prahu gate a
dwarf párový Δ ≥ 0.

**F3. Vyvážení policy korpusu podle rasy (uživatelův bod b, upravený).**
Effort: S (`run_iteration.py:29–30` — rotovat HOME_RACE přes 5 ras, nebo vážit decisions při tréninku
1/podíl rasy). Risk: nízký; ale dopad je omezený, dokud platí §2.5 (síť rasový kontext skoro nevidí —
guard fraction sondou hýbe <1 pp; samotné vyvážení korpusu tedy pravděpodobně nestačí). Dělat AŽ PO F2,
jinak neměřitelné. Validace: jako F2.
Pozn.: „filtrovat podle kvality zdrojové hry" — decisions dnes nenesou race/výsledek metadata; nejdřív
je přidat do `PolicyDecision` (bb_module.cpp:177–213 už umí board snapshot, race tag je triviální).

**F4. Per-race policy blend (dwarf=0) — uživatelův bod (a): NEDOPORUČUJI.**
Effort: S. Ale evidence je PROTI: párově policy dwarfům pomáhá (+18,8 pp vs skaven, sig.); vypnutí
blendu pro dwarfa by zahodilo největší per-race zisk celého testu. Ponechat jako nouzovou páku, kdyby
budoucí (round-robin, párový) test ukázal reálnou škodu. Validace, kdyby přesto: fairtest jen dwarf
matchupy, blend 0 vs 0.2, N=2×400 párovaně.

**F5. Race/styl-conditional větve v plánovačích (uživatelův bod d) — implementovat přes staty, ne přes rasu.**
Effort: M–L. Doktrína (brief :564) říká, že rasu netřeba labelovat — podmínky stavět na statech/skillech:
např. „tým s guard_fraction>0,3 & avg MA<5 → preferovat CAGE_ADVANCE/BLOCK koridor, penalizovat sólo
ADVANCE carriera do >0 TZ" jako podmíněný prior tweak. Riziko overfitu heuristik je vyšší; dělat až po
F1/F2 a jen s A/B evidencí. Sem patří i L-pin makro (velký kus, samostatná položka do zadní fronty).
Validace: vždy A/B arm harness per matchup + celkový gate; jedna změna najednou.

**F6. Pacing formule pro pomalé týmy (§2.8).** Effort: S (nahradit `idealDist=turnsLeft*MA` efektivním
tempem ~min(MA, MA*0.6+1) při kontaktu). Nejdřív změřit prevalenci (levné: rozdíl dist-vs-ideal na
dwarf turnech v replay korpusu). Risk: nízký, ale globální — validovat i na agility rasách.

## Doporučený jeden další krok

**F0 + F1 v tomto pořadí:** (1) dnes/zítra opravit vyhodnocovací metodiku (round-robin + párové per-race
čtení — XS effort) a překlopit závěr o dwarf výsledku pro uživatele, protože z něj teď plyne špatná
implikace „vypnout dwarfům policy"; (2) jako první kódový fix implementovat **CAGE_ADVANCE makro
s Guard-aware rohy** (F1) — má nejkonkrétnější evidenci (g0018: 4/4 klec stojící 3 tahy; g0003/g0023:
klec se rozpadá při každém postupu), míří přesně na intrinsický gap 26 %/9,7 % baseline dwarfa, a je
měřitelný levným párovým A/B harness na dwarf matchupech (N=2×400, práh +3 pp chess-skóre) bez čekání
na celý gate cyklus. Nezačínat, dokud běží validace item13 (stejný subsystém plánování — nemíchat proměnné).
