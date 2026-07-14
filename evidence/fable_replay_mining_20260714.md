# Re-mining post-fix logů: co přetrvává a co se změnilo (2026-07-14)

**Datum:** 2026-07-14 (Fable 5) · **Data:** `training_logs/epoch_002..016` ze Stage-1 běhu
mc_td_mix 2026-07-13 (600 her: v každé epoše hry 1–20 = mirror kandidát vs frozen champion
@MCTS100 + epsilon 0.35→0.10, hry 21–40 = vs random; pořadí ověřeno v
`python/blood_bowl/training_loop.py:347-357`). **epoch_001 vyřazena** — právě běžící trénink
(PID 2474, null_alpha1 běh) ji od rána 14.07. přepisuje. Cross-check: snapshot
`stage1_mc_td_mix_backup_20260713/replay_buffer.pkl` (10 000 transitions) a gate fáze logu
(600 her New vs Frozen bez šumu).

Srovnávací základ: `evidence/fable_replay_mining_findings.md` (07-02, data z běhu 2026-06-30,
640 her). Mezi oběma miningy byly opraveny: halfclock reset (676bb50), kicking-team po TD
(4ec718c), TURNOVER mislabeling (e8e2388), PICKUP step-cap (2899cd5), expandScore hang
(39f689a), stall-guard blitz-awareness (a88f5e2), blitz follow-up guard (ca297c9).

**Metodika = identická s 07-02** (stejné featury f12/f13/f14 míč, f15 dist-TD, f41/f42
scoring threat, f59 can-score+GFI, f62 screen, f35 stall, f70–72 loose-ball; indexy ověřeny
proti aktuálnímu `engine/src/feature_extractor.cpp:500-626` — beze změny). Objemy se liší
(300 vs 320 her/bucket), proto srovnávám per-game a v poměrech. Mining skript:
scratchpad `mine_fresh.py` (jednorázový, výstup `mine_fresh_out.txt`).

**Caveaty čerstvých dat:** (1) Stage-1 běžel PŘED fixem H2-kickoff bugu (nalezen 13.07.,
fix dnes) — druhé poločasy mají špatný směr výkopu, může zkreslovat H2 dynamiku;
(2) mirror hry obsahují epsilon-exploration šum (gate fáze ne — uvádím obojí).

---

## Souhrn v jedné větě

**Pickup-miss 82 % platí beze změny na čerstvých datech** (82.1 %, klíčové číslo pro
master-list item 7), loose-ball scramble zůstává hlavním mechanismem remíz; fixy zlepšily
obranu a držení míče, ale útok ne → podíl 0-0 mirror her VZROSTL z 39 % na 49.7 %.

---

## (a) Které mechanismy z 07-02 přetrvávají

### Pickup-miss: BEZE ZMĚNY — 82.1 % (07-02: 82 %)

- Mirror turn-instance „míč na zemi + můj hráč do 1 pole (f71≤0.13)":
  **1 306 nesebráno vs 284 sebráno = 82.1% per-turn miss** (07-02: 1906/407 = 82 %).
- **Čistý pickup (f72=1, žádná soupeřova TZ na míči): miss 73.9 %** (601 miss / 212 hit) —
  i zcela nekontestované sběry selhávají ve ¾ tahů.
- Per-turn recovery všech ground-tahů: **28.1 %** (07-02: 29–33 %); epochy 2–8 = 26.8 %,
  9–16 = 29.4 % → **žádný učební trend**, stejně jako v 07-02.
- Vsrand: champion strana missuje 87.8 % near-pickupů → slabina není specifická soupeři.
- **Mechanická příčina trvá v kódu:** `getAvailableMacros` generuje stále **jen jedno**
  PICKUP makro („bestPicker" dle AG/dist, `macro_actions.cpp:434-473`) — search nemá
  alternativního sběrače.
- Decision-level (mirror, 48 823 rozhodnutí): v 8 118 stavech „míč na zemi + hráč do 1 pole"
  PICKUP zvolen **50.2 %** (07-02: 53 %). Když nezvolen, v **3 548/4 043 (88 %) v top-20 BYL**
  — search ho dál aktivně podvažuje (průměrná max visit-fraction nezvoleného PICKUP 0.18).
  Místo něj: REPOSITION 2 184, FOUL 599, BLITZ 503, BLOCK 404, END_TURN 353.
  Příklady (čistý míč, ep/g/dec, zvoleno vf vs PICKUP vf): 3/5/154 REPOSITION 0.56 vs 0.01;
  3/1/130 BLITZ 0.60 vs 0.11; 3/4/93 FOUL 0.32 vs 0.13.

### Loose-ball scramble: PŘETRVÁVÁ jako master mechanismus

- Mirror 0-0 hry: míč na zemi ve **40.8 %** hranic tahů (medián 37.5 %, n=149)
  vs 31.4 % v rozhodnutých mirror hrách (07-02: 43 %/44 % resp. 30 %).
- Cross-check replay buffer: **40.6 %** z 10 000 transitions má f14=1 (07-02: 35 %).
- Souvislé ground runs ≥6 tahů: 176× mirror (0.59/hru; 07-02: 237× = 0.74/hru), max 32 beze změny.

### SCORE dostupnost: HORŠÍ než v 07-02

- Stavy f12=1 & f59=1 (mirror, 2 588 rozhodnutí): SCORE zvoleno jen **198× (7.7 %)** —
  07-02 bylo 19 %. Když nezvoleno: v **1 836/2 390 (77 %) SCORE vůbec nebylo v top-20**
  (07-02: 82 %) — mechanická příčina trvá: SCORE se generuje jen když `carrier->canAct()`
  (`macro_actions.cpp:152`), první pohnutí carrierem jiným makrem šanci pro tah maže.
- Když v top-20 bylo a nezvoleno: průměrná max visit-fraction **0.065** (07-02: 0.061).
- stall_incentive>0.3 u 60 % nezvolených (07-02: ~84 % — pokles konzistentní se stall-guard
  fixem a88f5e2, ale stall zůstává zadrátovaný v `expandAdvance`).
- Příklady: ep2/g2 dec49 FOUL 0.62 vs SCORE 0.07; dec50 END_TURN 0.50 vs 0.02;
  ep3/g1 dec29 CAGE 0.34 vs 0.02.

### FOUL overuse: zmírněný, ale přítomný

- V loose-ball-near stavech FOUL místo PICKUP 599/4 043 = 14.8 % (07-02: 853/5 137 = 16.6 %).
- Celkový mirror decision mix: REPOSITION 39.5 %, PICKUP 15.0 %, BLITZ 12.0 %, END_TURN 7.7 %,
  BLOCK 7.3 %, CAGE 6.4 %, **FOUL 5.4 %**, ADVANCE 4.8 %, PASS 1.4 %, SCORE 0.5 %.

## (b) Co se po fixech změnilo

| Metrika (mirror, per game) | 07-02 (320 her) | 07-14 (300 her) | Δ |
|---|---|---|---|
| Carrier ztratil míč ve vlastním tahu | 1.07/hru (343×) | **0.61/hru (182×)** | **−43 %** |
| Míč vyražen soupeřem | 1.82/hru (582×) | **1.48/hru (445×)** | −18 % |
| f42=1 hrozba → conceded TD | **51 %** (135/265) | **31.9 %** (74/232) | **−19 pp** |
| f41=1 šance/hru | 0.49 (158×) | **0.36 (107×)** | −27 % |
| f41 konverze | 80 % | **67.3 %** | −13 pp |
| GFI šance (f59=1,f41=0) konverze | 58 % (88/151) | **31.5 %** (53/168) | −27 pp |
| Mirror 0-0 podíl | 39 % (125/320) | **49.7 %** (149/300) | **+11 pp** |
| Pickup-miss (do 1 pole) | 82 % | 82.1 % | beze změny |
| Ground recovery per-turn | 29–33 % | 28.1 % | beze změny |

- **Obrana se výrazně zlepšila** (hrozby končí TD jen ve 32 % místo 51 %; u conceded je
  screen=0 v 39/74 = 53 % případů — screen fixy 8+9 z master listu stále relevantní).
- **Držení míče je bezpečnější** (−43 % vlastních ztrát — konzistentní se stall-guardem,
  který už netlačí blitzable carriera do „arrive on last turn").
- **Ale útok se nezlepšil** — šancí je míň a konvertují se hůř. Halfclock fix nadto odstranil
  reset hodin po každém gólu (= reálně míň tahů na drive než v datech 07-02), takže
  pozdě vzniklé šance častěji umírají na konec poločasu.
- Čistý efekt: **víc remíz** — mirror 0-0 49.7 %, gate fáze (bez šumu):
  **119W 356D 125L = 59.3 % remíz** z 600 her (benchmark vs random 93.0 %).
- Defenzivní decision mix při f42=1 (868): REPOSITION 483, BLITZ 243, FOUL 64, BLOCK 47,
  END_TURN 31 → „utrácení" hrozbového tahu za FOUL/END_TURN/BLOCK kleslo na 16.4 % (07-02: ~18 %).

## (c) Hlavní mechanismy remíz TEĎ (seřazeno)

1. **Loose-ball scramble bez recovery** — 40.8 % hranic tahů v 0-0 hrách míč na zemi,
   per-turn recovery 28 %, miss i u čistého míče 74 %. Nezměněno žádným z fixů (žádný na to nemířil).
2. **Šance skoro nevznikají a hůř se konvertují** — 0.36 šance/hru (−27 %), konverze 67 %;
   SCORE makro je pro search téměř neviditelné (zvoleno 0.5 % všech rozhodnutí, 7.7 %
   can-score stavů, vf 0.065).
3. **Symetricky zlepšená obrana** — 68 % one-turn hrozeb zastaveno (dřív 49 %) → i vzniklé
   šance častěji umírají; kombinace 1+2+3 = 0-0 plošina vzrostla.

## Doporučení

1. **Item 7 (PICKUP top-2/3 kandidáti): GO, číslo 82 % REPRODUKOVÁNO na post-fix datech.**
   Všechny tři složky mechanismu trvají: (i) jediný bestPicker v `getAvailableMacros`,
   (ii) search podvažuje PICKUP (zvolen 50.2 %, vf 0.18 když přeskočen), (iii) miss 74 %
   i u nekontestovaného míče. Doplněk z 07-02 dál platí: zvážit prior boost PICKUP při f72=1.
   Očekávaný dopad je teď ještě větší než v 07-02, protože scramble je jediný z top
   mechanismů, na který zatím žádný fix nemířil.
2. **SCORE dostupnost (07-02 páka #2) povýšit** — situace se zhoršila (7.7 % chosen,
   77 % not-in-top-20). Nejlevnější varianta: generovat SCORE i pro carriera s částečným MA
   (featura f59 už teď počítá s plným MA — nesoulad featura/makro), nebo SCORE-first ordering
   před makry, která carrierem hýbou.
3. **Screen fixy 8+9** zůstávají odůvodněné: u 53 % conceded hrozeb screen=0.
4. FOUL overuse: zmírněný (14.8 % v loose-ball stavech), samostatný fix nízká priorita.
5. **Po dnešním H2-kickoff fixu** (resetuje baseline) čísla znovu ověřit levně na prvním
   novém běhu — zejména podíl 0-0 a f41 šance/hru (H2 bug mohl deformovat druhé poločasy).

## Limity dat / co by pomohlo logovat (stejné jako 07-02, trvá)

- State záznamy jen na hranicích tahů → „pickup miss" nerozlišuje neúspěšný roll od
  „nepokusil se"; decision záznamy nemají game/turn id, takže je nejde spárovat s tahy.
- `TurnLog` snapshoty engine pořád vytváří, ale do `game_*.jsonl` se neserializují
  (`cpp_runner.py`) — zapnout před příštím dlouhým během = přesná lokalizace dropů/pickupů
  bez zásahu do enginu a bez generování nových dat.
- epoch_001 tohoto běhu už není k dispozici (přepsána živým během) — příště zálohovat
  celý `training_logs/` do backup adresáře spolu s bufferem.
