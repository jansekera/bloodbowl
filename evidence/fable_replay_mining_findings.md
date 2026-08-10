# Replay/Game-Log Mining: Concrete Scoring Failure Modes

**Datum:** 2026-07-02 (Fable 5) · **Data:** `training_logs/epoch_001..016` z běhu 2026-06-30
(640 her: v každé epoše hry 1–20 = mirror self-play kandidát vs frozen champion @MCTS100,
hry 21–40 = vs random — pořadí ověřeno v `training_loop.py:343-353`), plus
`replay_buffer.pkl` (10 000 transitions, cross-check) a `decisions_*.json`
(každé makro-rozhodnutí MCTS s top-20 visit distribucí; `visits[0]` = vykonaná akce,
`mcts.cpp:79` argmax visits).

**Schéma dat (klíč k reprodukci):**
- State záznamy = hranice tahů, 73 featur pojmenovaných v `engine/src/feature_extractor.cpp`
  (f12 mám míč, f13 soupeř, f14 míč na zemi, f15 vzdálenost carriera k TD/26, f41 scoringThreat,
  f42 opp threat, f59 can_score+GFI, f62 screen, f70–72 loose-ball featury, f35 stallIncentive).
- Decision záznamy používají **makro** featury (`engine/src/macro_actions.cpp:extractMacroFeatures`):
  one-hot [0]=SCORE, [1]=ADVANCE, [2]=CAGE, [3]=BLITZ, [4]=BLOCK, [5]=PICKUP, [6]=PASS,
  [7]=FOUL, [8]=REPOSITION, [9]=END_TURN. **POZOR: nejde o schéma `action_features.cpp`** —
  záměna vede k úplně chybné interpretaci (ověřeno: trénink jede `macro_mcts`).
- Mezi dvěma po sobě jdoucími state záznamy proběhl přesně jeden tah aktivního týmu
  (`game_simulator.cpp:simulateGameLogged`); změna skóre = TD v intervalu.

Mining skripty: scratchpad `mine_games.py`, `mine_decisions2.py`, `mine_sequences.py`
(metodika popsaná níže u každé kategorie; čísla jsou přesné počty, ne odhady).

---

## Hlavní zjištění (master finding)

**Míč leží na zemi obrovskou část mirror her a per-turn recovery je jen ~30 %.**
V mirror 0-0 hrách je míč na zemi ve **43 %** hranic tahů (medián 44 %, n=125 her);
v rozhodnutých mirror hrách 30 %. Souvislé "ground runs" ≥6 tahů: 237× v mirror hrách
(max 32 tahů v kuse). Remízy 0-0 tedy nejsou primárně "stall u remízy" ani "pasivní
přetlačovaná s drženým míčem" — je to **scramble o míč na zemi, který nikdo neumí
spolehlivě sebrat**. Cross-check replay bufferu: 35 % z 10 000 transitions má f14=1.

Mirror 0-0 her je 125/320 (39 %) — konzistentní s ~48% draw rate v gatingu
(gating hraje bez Dirichlet šumu, tady je root noise 0.25, jinak stejná konfigurace).

---

## 1. Ztracené/upuštěné míče

**Počty (za 320 mirror + 320 vsrand her):**
- Carrier ztratil míč **během vlastního tahu** (failed dodge/GFI/handoff při expanzi makra,
  bez TD v intervalu): **343× mirror, 222× vsrand** (~1,1/mirror hru).
- Míč vyražen **soupeřem** (držel ho neaktivní tým, po tahu aktivního je na zemi):
  **582× mirror, 235× vsrand** (~1,8/mirror hru). Mirror soupeř (champion) vyráží
  2,5× častěji než random — držení míče je proti silnému soupeři velmi křehké.

**Příklady (pozdní epochy, u konce drivu):**
- ep12/game_03 (mirror), boundary 14, stav 0-0: home carrier 6 polí od endzóny,
  1 TZ na sobě, poslední tah drivu → míč na zemi (chance_wasted:lost_ball tamtéž).
- ep13/game_14 (mirror), boundary 27, 1-0: away carrier 5 polí od TD, 1 TZ, turns_left=1 → drop.
- ep14/game_16 (mirror), boundary 41, 1-0: home carrier 5 polí od TD, 2 TZ → drop.
- ep15/game_10 (mirror), boundary 14, 0-0: carrier 4 pole od TD, 1 TZ, poslední tah → drop.

**Proč:** vzorec je "carrier v kontaktu (TZ≥1) blízko endzóny v posledním tahu drivu".
Makro expanze (`expandScore`/`expandAdvance` → `movePlayerToward`) hází dodge/GFI greedy
cestou; kombinace stall chování (viz §3) nechává skórování na poslední tah, kdy už je
carrier obklíčen, a jediný neúspěšný roll = konec drivu. f63 (carrier_blitzable) je
u těchto stavů typicky 1.

## 2. Neúspěšné pickupy

**Počty (turn-instance = tah, na jehož začátku je míč na zemi a hráč aktivního týmu
do 1 pole, f71≤0.13):**
- Mirror: **1906 tahů míč nesebrán** vs 407 sebrán → **82% per-turn miss rate**.
  Z toho **850 tahů s f72=1 (žádná soupeřova TZ na míči — čistý pickup!)**.
- Vsrand: 1406 nesebráno vs 163 sebráno.
- Přes všechny vzdálenosti: recovery ground-tahů jen 29 % (ep1-8) / 33 % (ep9-16) mirror —
  **trend se mezi epochami prakticky nelepší**, není to warmup artefakt.

**Decision-level (proč):** v stavech "míč na zemi, hráč do 1 pole" (10 914 mirror rozhodnutí):
- PICKUP makro zvoleno 5 777× (53 %). Když zvoleno NEbylo (5 137×), ve 4 822 případech
  **PICKUP v top-20 visitované nabídce BYL** — search ho aktivně podvažuje.
- Místo něj zvoleno: REPOSITION 2 486×, BLITZ 1 080×, **FOUL 853×**, BLOCK 491×, END_TURN 227×.
- Konkrétně: ep1/game_02 dec16 — REPOSITION visit-fraction 0.94 vs PICKUP 0.05 (čistý míč);
  ep1/game_04 dec155 — REPOSITION 0.43 vs PICKUP 0.25; ep1/game_03 dec113 — FOUL 0.52
  vs PICKUP 0.12; ep1/game_07 dec1 — END_TURN 0.17 vs PICKUP 0.14.
- Druhá polovina problému: i zvolený PICKUP často selže na kostce (fail = turnover
  + scatter → soupeř je u míče → jeho pickup selže → ping-pong; proto ground runs
  ≥6 tahů 237×). `getAvailableMacros` navíc generuje **jen jedno** PICKUP makro
  ("best picker" dle AG/dist) — search nemá alternativu jiného sběrače.

## 3. Promarněné skórovací šance

**Turn-level (f41=1 na začátku vlastního tahu, tj. carrier MA ≥ vzdálenost):**
- Mirror: 126 proměněno / 32 promarněno (**80% konverze** — turn-level slušné).
  Z 32: 15× lost_ball, 17× kept_ball. GFI šance (f59=1, f41=0): 88 conv / 63 wasted.
- Vsrand: 249 / 254 = **49% konverze** — z 254 promarněných je **238 kept_ball**,
  typicky stall_incentive 0.56–1.12 a turns_left 3–6 → **záměrný stall** (vs random
  beztrestný, hry stejně končí 3-0).
- Příklady stall: ep1/game_26 (vsrand) b20: dist_td=1(!), 0 TZ, stall=0.94, turns_left=5
  → nekóruje; totéž b22. Mirror: ep13/game_03 b60+b62 (2-1): dist 7→6, stall 0.75/0.56,
  blitzable=True, drží míč.

**Decision-level (2 026 mirror rozhodnutí se stavem f12=1 & f59=1):**
- SCORE makro zvoleno jen 392× (19 %). Když nezvoleno: v **1 346/1 634** případů SCORE
  **vůbec nebylo v top-20** — mechanická příčina: `getAvailableMacros` generuje SCORE jen
  když `carrier->canAct()` (macro_actions.cpp:151). Jakmile jiné makro v tahu carrierem
  pohne (CAGE/REPOSITION/ADVANCE), **šance je pro ten tah nenávratně spálená**, i když
  f59 dál svítí (featura počítá s plným MA).
- Když SCORE v top-20 bylo a nezvoleno (288 mirror): průměrná max visit-fraction SCORE
  jen **0.061** — search ho skoro nenavštěvuje. 2 098 z ~2 510 těchto případů má
  stall_incentive > 0.3.
- `expandAdvance` má stall **natvrdo zadrátovaný**: "move just enough to reach endzone
  on the last turn" (macro_actions.cpp:765-771) + max půl MA. Stall tedy není jen naučený —
  je vestavěný v makro heuristice. V mirror hrách se kombinace "stall do posledního tahu"
  × "carrier blitzable" mění ve ztráty míče z §1.

## 4. Obranná selhání pod tlakem

**Počty (f42=1 na začátku mého tahu = soupeřův carrier dosáhne endzóny příští tah):**
- Mirror: **135 conceded vs 130 stopped (51 % hrozeb končí TD)**. Vsrand: 251/276.
- Ve 32 mirror conceded případech (ep≥12) byl **screen=0** — žádný obránce mezi
  carrierem a endzónou; one_turn_td_vulnerability=1 prakticky vždy.
- Příklady: ep12/game_08 b30 (0-0), ep12/game_10 b43 (1-0), ep13/game_01 b7 (0-0) —
  vše screen=0, hrozba viditelná ve featurách celý obranný tah, TD stejně padl.
- Decision mix při f42=1 (669 mirror rozhodnutí): REPOSITION 276, BLITZ 272, FOUL 55,
  END_TURN 33, BLOCK 33 — tj. ~13 % rozhodnutí v "soupeř příští tah skóruje" utrácí
  tah za FOUL/END_TURN/BLOCK mimo carriera.

---

## Syntéza: co reálně žere skóre v mirror hrách

Řetěz: (1) stall zadrátovaný v `expandAdvance` + naučený přes stallIncentive drží
carriera blízko soupeře víc tahů, (2) champion-level soupeř míč vyráží (582× vs 235×
u randomu), (3) následný pickup-scramble má ~30% per-turn recovery a generuje
multi-tahové ground runs, (4) search při loose ballu podvažuje PICKUP (visit-fraction
często <0.1) ve prospěch REPOSITION/FOUL, (5) když už se šance objeví, jediné
carrier-pohnutí bez skórování ji mechanicky maže (SCORE vyžaduje canAct()).
Výsledek: 39 % mirror her 0-0, přestože turn-level konverze deklarovaných šancí je 80 %.
Šance prostě proti silnému soupeři skoro nevznikají — a když míč spadne, nikdo ho nezvedne.

**Doporučené páky (v pořadí očekávaného dopadu):**
1. **Pickup priorita/spolehlivost:** víc PICKUP kandidátů (ne jen 1 "best picker"),
   pickup dřív v tahu (před bloky — teď o pořadí rozhoduje search, který ho podvažuje),
   případně prior boost PICKUP makra při f72=1.
2. **SCORE dostupnost:** generovat SCORE dřív, resp. zakázat makrům hýbat carrierem,
   dokud je SCORE na stole (nebo SCORE-first ordering v expanzi).
3. **Stall guard:** v `expandAdvance` netlačit "arrive on last turn", když je carrier
   blitzable (f63=1) — právě tahle kombinace končí ztrátou v posledním tahu.
4. **Obrana:** screen=0 při f42=1 je čistě poziční selhání — REPOSITION expanze
   negarantuje pozici mezi carrierem a endzónou.

## Limity dat / co by pomohlo logovat

- State záznamy jsou jen na hranicích tahů → vnitro-tahové příčiny (který roll selhal:
  dodge vs GFI vs pickup vs catch) jsou **nerozlišitelné**; "pickup missed" zahrnuje
  "neúspěšný roll" i "nepokusil se". Rozseknout to umí až event-level log.
- `TurnLog` snapshoty (pozice hráčů, pozice míče, carrier id, TD flag) engine **už dnes
  vytváří** (`captureTurnSnapshot`), ale do `game_*.jsonl` se zapisují jen skóre
  (`cpp_runner.py:67-73`) — zapnout jejich serializaci = okamžitě přesnější analýza
  (kdo upustil, kde přesně míč leží) bez zásahu do enginu.
- V decisions chybí (a) flag „která akce byla vykonána" (dovozeno přes argmax visits),
  (b) výsledek expanze makra (uspěl roll? turnover?), (c) game/turn id — decision index
  není chronologický napříč stranami (home dávka, pak away, `bb_module.cpp:487-507`).
- Ideální přírůstek: per-akce event log `{turn, player, action, roll_target, roll_result,
  outcome}` aspoň pro pickup/dodge/GFI/pass — pak jde spočítat skutečná per-roll úspěšnost.
