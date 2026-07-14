# Návrh: side-swap v produkčním gatingu a benchmarku (2026-07-14, Fable 5)

Navazuje na `proposals_first_possession_measurement_20260714.md` sekci 5
(„Vedlejší důsledek pro gating"). Kandidát na master-list položku měřicího
stacku. Jen návrh — žádný kód zatím nezměněn, žádný běh nespuštěn. (Stav
2026-07-14: trénink už neběží, ale na stroji poběží multi-arm A/B — ověřovací
běhy §3b/§3c plánovat až mimo něj; offline stub test §3a nekoliduje s ničím.)

---

## 0. Problém — přesně a s čísly

Produkční gate v `run_iteration.py` má **konstantní orientaci v obou fázích**:

- **HtH gate** (`_gate_game`, :211-249): kandidát je vždy `weights_path` =
  HOME, frozen vždy `away_weights_path` = AWAY. Ověřeno v bindingu
  (`engine/python/bb_module.cpp:418-427`): `weights_path` → home VF,
  `away_weights_path` → away VF. Skóre gate = W/decisive z pohledu HOME.
- **Benchmark** (`_benchmark_game`, :188-208): kandidát vždy
  `home_ai='macro_mcts'`, `away_ai='random'` — také 100 % her v HOME slotu.

HOME slot přitom nese first-possession výhodu: engine nemá los
(`game_simulator.cpp:409/:521` — `kickingTeam = AWAY`, home přijímá první)
a **i po dnešním H2 kickoff fixu** to platí (fixes tree
`bloodbowl-fixes-20260714/engine/src/game_simulator.cpp:411-413`:
`openingKickingTeam = TeamSide::AWAY` natvrdo; H2 pak kope H1 receiver).
Post-fix zbývá „jen" výhoda pořadí (HOME má opening drive dřív), pre-fix byla
i početní. Velikost třídy efektu ze stall-guard paired A/B
(`diag_stall_guard_ab_20260713.log`, candidate rameno, mirror váhy):
home_win 32.5 % vs home_loss 21.8 % → **home podíl na rozhodnutých
= 32.5/54.3 ≈ 59.9 %**, McNemar p=0.02.

**Důsledek pro gate verdikt:** práh je `0.5 + k·σ`, σ = 0.5/√decisive
(:538, :558). Formule předpokládá, že null (kandidát == frozen síla) sedí na
50 %. V biased setupu ale null sedí na 50 % + slot-edge. Konkrétně
v post-stall-guard režimu: N=600, draws ~46 % → decisive ≈ 324,
σ ≈ 2.78 %, k=2.0 → práh ≈ 55.6 %; null ≈ 59.9 % je **+1.5σ NAD prahem**
→ P(null kandidát projde HtH laťku) ≈ 93 %. HtH signál gatu je v tomto
režimu efektivně rozbitý ve prospěch kandidáta. (Čísla z eval configu
diag běhu — TV=1000, MCTS=100, pre-H2fix build; přesná post-fix velikost
edge není známa a **nemusí být**: side-swap je vůči její velikosti i znaménku
robustní, to je jeho pointa.)

**Povinný kontext, který swap NEřeší** (paměť
`project_bloodbowl_gating_null_test_finding.md`): při vf_blend=0 gate HtH
neměří obsah natrénovaných vah vůbec (kandidát BEZ vah ≈ self-mirror,
`diag_null_weights.py` PASS). Side-swap opravuje **slotový bias měřidla**,
ne jeho **citlivost na váhy** — to jsou dva nezávislé defekty. Post-swap
PROMOTED tedy stále nelze číst jako „váhy se zlepšily"; čte se jako „gate už
aspoň systematicky nenadržuje kandidátovi".

## 1. Patch `run_iteration.py`

Návrhové principy:

- **Orientace deterministicky z indexu úlohy** (`i % 2`): sudé i → kandidát
  HOME, liché → kandidát AWAY. Přesně 50/50 (žádná binomická nerovnováha
  losu), reprodukovatelné, a protože rasové matchupy cyklují s periodou 5
  (`races[i%5]` vs `races[(i+1)%5]`) a orientace s periodou 2, gcd=1 →
  perioda 10: při N=600 přesně **60 her na každou (matchup × orientace)**
  buňku, při half_bm=200 přesně 20. (Caveat: env override `BB_GATE`/`BB_BM`
  na liché N nechá 1 hru navíc v HOME orientaci — zanedbatelné, ale držet
  sudá N.)
- **Flip dělá WORKER z args, nikdy caller z pozice ve výsledcích.**
  `_imap_watchdog` skipnuté hry z yield streamu vynechává → odvození
  orientace z pořadí výsledků by po prvním skipu převrátilo atribuci všech
  dalších her (a kdyby na tom stálo skórování, i verdikt). Orientační flag
  proto cestuje v task tuple a echem zpět v návratové hodnotě.
- **Návratová hodnota `_gate_game` je vždy z perspektivy kandidáta**
  (cand_score, frozen_score) → skórovací smyčka W/D/L zůstává významově
  identická. Legacy volání (≤9-tuple: diag skripty, `diag_utils.run_arm`,
  `diag_mirror_budget.py`, `fuzz_gate.py`) jsou beze změny chování i arity —
  10. prvek je opt-in.

### 1a. `_gate_game` (:226-249)

```python
def _gate_game(args: tuple) -> tuple:
    # 10th element (cand_is_away) optional/backward-compatible: production
    # side-swap (2026-07-14). Legacy <=9-tuple callers unchanged (candidate
    # always HOME, 2-tuple return). With cand_is_away the weights paths are
    # swapped and the return is STILL candidate-first, plus a 3rd element
    # echoing the orientation (skip-proof attribution for the side audit).
    cand_is_away = False
    if len(args) >= 10:
        (seed, race_idx, gate_path, frozen_path, mcts_iterations, vf_blend,
         tv, leaf_lookahead, policy_path, cand_is_away) = args[:10]
    elif len(args) >= 9:
        seed, race_idx, gate_path, frozen_path, mcts_iterations, vf_blend, tv, leaf_lookahead, policy_path = args[:9]
    elif len(args) >= 8:
        seed, race_idx, gate_path, frozen_path, mcts_iterations, vf_blend, tv, leaf_lookahead = args[:8]
        policy_path = ''
    else:
        seed, race_idx, gate_path, frozen_path, mcts_iterations, vf_blend, tv = args
        leaf_lookahead = False
        policy_path = ''
    import bb_engine
    hr = bb_engine.get_developed_roster(_RACES[race_idx % len(_RACES)], tv)
    ar = bb_engine.get_developed_roster(_RACES[(race_idx + 1) % len(_RACES)], tv)
    home_w, away_w = ((frozen_path, gate_path) if cand_is_away
                      else (gate_path, frozen_path))
    result = bb_engine.simulate_game_logged(
        hr, ar,
        home_ai='macro_mcts', away_ai='macro_mcts',
        seed=seed, mcts_iterations=mcts_iterations,
        weights_path=home_w, away_weights_path=away_w,
        epsilon=0.0, vf_blend=vf_blend,
        leaf_lookahead=leaf_lookahead,
        policy_weights_path=policy_path,
        dirichlet_alpha=GATE_DIRICHLET_ALPHA,
        exploration_c=GATE_EXPLORATION_C,
    ).result
    if len(args) >= 10:
        cs, fs = ((result.away_score, result.home_score) if cand_is_away
                  else (result.home_score, result.away_score))
        return cs, fs, int(cand_is_away)
    return result.home_score, result.away_score
```

Policy net je v bindingu jeden pro obě strany (`bb_module.cpp:430-457`) a
seed dostávají obě MCTS stejný — swap vah je jediná potřebná změna, žádná
další skrytá HOME asymetrie v predikčním aparátu volání není.

Pozn. k `diag_utils.run_arm`: jeho průběžné W/D/L tally je podmíněné
`len(res) == 2` — 3-tuple výsledky ukládá, jen průběžně netally-uje. Žádný
existující diag skript 10-tuple neposílá, takže se nic nerozbije; budoucí
diag s 10-tuples si tally přizpůsobí (nebo se `run_arm` podmínka rozšíří na
`len(res) in (2, 3)` — kosmetika, mimo tento patch).

### 1b. Konstrukce gate tasků (:479-483)

```python
    gate_tasks = [
        (random.randint(1, 999999), i, str(gate_path), str(frozen_path), MCTS_ITERATIONS, VF_BLEND, TV,
         False, gate_policy_path, i % 2 == 1)   # sudé i: cand=HOME, liché: cand=AWAY
        for i in range(GATING_MATCHES)
    ]
```

Seedy záměrně zůstávají `random.randint` (produkční vlastnost: každá iterace
vzorkuje nové herní situace; deterministický seed-list by korelaci verdiktů
mezi iteracemi zavedl, nechceme). Párování stejným seedem přes obě orientace
(fwd/swp jako v diag harnessech) tu nic nepřináší: v páru se liší váhy stran,
takže McNemar párování slotového efektu stejně nejde interpretovat čistě, a
reuse seedů půlí počet navzorkovaných odlišných situací. Reprodukovatelné a
exaktní má být **přiřazení orientací**, a to je (`i % 2`).

**Tvrdý zákaz navíc (past paired-seed doktríny):** dokud platí vf_blend=0 +
policy_blend=0, jsou kandidát a frozen **mechanicky identičtí agenti**
(potvrzený null-weights test). Stejný seed + prohozené váhové cesty pak
vyprodukuje **bit-identickou hru** — pár (fwd, swp) nese nulovou informaci,
pooled skóre je deterministicky přesně 50 % a efektivní N se tiše zhroutí na
polovinu (v null testu na nulu). Kdyby někdo v budoucnu chtěl gate „vylepšit"
paired-seed metodikou (kterou šumové dno jinak správně vyžaduje pro diag A/B),
přes orientace se párovat NESMÍ, dokud váhy reálně nevstupují do searche
(vf_blend>0 / policy_blend>0) — a i pak jen jako samostatná, zvlášť ověřená
změna.

### 1c. Skórovací smyčka + side audit (:485-511)

```python
    gate_results: list[tuple[int, int, int]] = []
    with Pool(WORKERS, initializer=_pool_init, initargs=init_args) as pool:
        for res in _imap_watchdog(pool, _gate_game, gate_tasks, 'Anti-regression',
                                  mcts_iterations=MCTS_ITERATIONS):
            gate_results.append((res[0], res[1], res[2] if len(res) > 2 else 0))
            done = len(gate_results)
            if done % 10 == 0 or done == GATING_MATCHES:
                print(f'  Anti-regression {done}/{GATING_MATCHES}', flush=True)

    wins = draws = losses = 0
    arm = {0: [0, 0, 0], 1: [0, 0, 0]}   # orientace -> [W, D, L] kandidáta
    for i, (cs, fs, ca) in enumerate(gate_results):
        if cs > fs:
            wins += 1; arm[ca][0] += 1
        elif cs == fs:
            draws += 1; arm[ca][1] += 1
        else:
            losses += 1; arm[ca][2] += 1
        print(f'  Game {i + 1} [{"A" if ca else "H"}]: cand {cs}-{fs}', flush=True)
```

a za stávající `New vs Frozen:` řádek (:510) přidat trvalý null-monitor:

```python
    hW, hD, hL = arm[0]; aW, aD, aL = arm[1]
    home_slot_wins = hW + aL          # výhry HOME slotu bez ohledu na to, kdo v něm sedí
    hs_dec = hW + hL + aW + aL
    print(f'Side audit: cand@H {hW}W {hD}D {hL}L | cand@A {aW}W {aD}D {aL}L | '
          f'home-slot wins {home_slot_wins}/{hs_dec} '
          f'({home_slot_wins / hs_dec:.1%} decisive)' if hs_dec else
          'Side audit: no decisive games', flush=True)
```

`home-slot wins` je strukturní metrika nezávislá na síle kandidáta — každý
gate běh tak zadarmo průběžně měří velikost slot-edge post-H2-fix (doplněk
k Fázi A z first-possession návrhu, ne její náhrada).

**Log-format pozor:** per-game řádek se mění z `Game N: hs-as` (home-away)
na `Game N [H/A]: cand cs-fs` (kandidát první). Cokoliv, co gate logy
parsuje (log mining), musí o změně vědět — proto nový prefix `cand`, ať
starý parser radši spadne, než aby tiše četl prohozené skóre.

### 1d. Benchmark fáze (`_benchmark_game` :188-208 + tasky :438-441)

Ano, benchmark má **stejný bias** (kandidát vždy HOME vs random). Pro
relativní srovnání new_bm vs all_time_best se bias krátí (obě strany měřeny
stejně), ale absolutní kotvy `BM_FLOOR=0.77` a tier hranice ±2 %/±5 % jsou
kalibrované na nafouknuté (home-slot) škále — a hlavně: nechat gate
vyvážený a benchmark biased by znamenalo dvě metriky téhož běhu na dvou
různých škálách. Swap je zde levný, doporučuji parity:

```python
def _benchmark_game(args: tuple) -> bool:
    # 8th element (cand_is_away) optional/backward-compatible (2026-07-14
    # side-swap): odd games put the measured model in the AWAY slot vs a
    # random HOME. away VF falls back to weights_path when away_weights_path
    # is empty (bb_module.cpp:424), and a 'random' home never loads a VF
    # (:418), so no extra path plumbing is needed.
    cand_is_away = False
    if len(args) >= 8:
        seed, race_idx, gate_path, mcts_iterations, vf_blend, tv, policy_path, cand_is_away = args[:8]
    elif len(args) >= 7:
        seed, race_idx, gate_path, mcts_iterations, vf_blend, tv, policy_path = args[:7]
    else:
        seed, race_idx, gate_path, mcts_iterations, vf_blend, tv = args
        policy_path = ''
    import bb_engine
    hr = bb_engine.get_developed_roster(_RACES[race_idx % len(_RACES)], tv)
    ar = bb_engine.get_developed_roster(_RACES[(race_idx + 1) % len(_RACES)], tv)
    home_ai, away_ai = (('random', 'macro_mcts') if cand_is_away
                        else ('macro_mcts', 'random'))
    result = bb_engine.simulate_game_logged(
        hr, ar,
        home_ai=home_ai, away_ai=away_ai,
        seed=seed, mcts_iterations=mcts_iterations,
        weights_path=gate_path, epsilon=0.0, vf_blend=vf_blend,
        policy_weights_path=policy_path,
        dirichlet_alpha=GATE_DIRICHLET_ALPHA,
        exploration_c=GATE_EXPLORATION_C,
    ).result
    return (result.away_score > result.home_score) if cand_is_away \
        else (result.home_score > result.away_score)
```

a v `_run_benchmark` (:438-441):

```python
        tasks = [
            (random.randint(1, 999999), i, str(path), MCTS_ITERATIONS, VF_BLEND, TV,
             gate_policy_path, i % 2 == 1)
            for i in range(half_bm)
        ]
```

Kritérium zůstává striktní výhra (remíza = neúspěch) v obou orientacích.
Selekce gate_path (az_train vs train_best) je vnitřně konzistentní — obě
kandidátní sady se měří identickým rozvrhem orientací.

**Podmíněnost doporučení:** benchmark-swap sám o sobě žádný verdiktový bias
neopravuje (new_bm i all_time_best_bm byly historicky na stejné, home-slot
škále → bias se v jejich rozdílu krátí; deformoval jen absolutní kotvy
BM_FLOOR/tiery a citlivost na „away" zlepšení). Dává smysl **jen proto, že
dnešní H2/screen vlna beztak resetuje baseline** — přibalením vznikne jediný
diskontinuitní bod zadarmo. Pokud by se tento patch nasazoval AŽ PO dnešním
resetu, benchmark-swap z něj vyčlenit (HtH swap nasadit hned — ten opravuje
skutečný verdiktový bias — a benchmark nechat na příští přirozený reset),
jinak vyrobíme druhou diskontinuitu benchmark škály bez kompenzujícího zisku.

Volitelná jednořádková pojistka do env-override bloku (:46-50), viz §2:

```python
BM_FLOOR = float(os.environ.get('BB_BM_FLOOR', BM_FLOOR))
```

## 2. Dopad na práh a historickou srovnatelnost

**HtH práh: formule `0.5 + k·σ` se NEMĚNÍ.** Side-swap ji opravuje, ne
obchází: null se konstrukčně vrací přesně na 50 %, takže k=1.0/1.5/2.0σ
tiery znovu znamenají to, co znamenat měly (P(falešný promote přes HtH
signál | null) ≈ 16 %/7 %/2.3 % místo dnešních ~93 % v post-stall-guard
režimu). Nekalibrovat žádný nový empirický práh — přesně proto swap děláme,
abychom velikost slot-edge nemuseli znát.

**Historická srovnatelnost verdiktů:**

- **Zpětný přepočet NENÍ možný ani potřebný.** Orientace byla konstantní →
  slotový a váhový příspěvek nejdou v starých log datech oddělit. Navíc éra
  remízové plošiny měla edge ≈ 0 (baseline rameno stall-guard A/B:
  −0.8pp), takže staré verdikty bias prakticky nekontaminoval; kontaminace
  narůstá až s ofenzivními fixy (stall-guard dál). A nezávisle na tom platí
  null-test finding: HtH při vf_blend=0 váhy neměřil, takže staré HtH
  verdikty stejně nemají váhovou interpretaci, kterou by šlo „zachraňovat".
- **Nová baseline: dnes, v jednom balíku.** H2 kickoff fix + screen fixy 8+9
  dnes stejně resetují baseline (paměť
  `project_bloodbowl_h2_kickoff_bug_20260713.md`) — side-swap přibalit do
  téhož resetu, aby vznikl JEDEN diskontinuitní bod, ne dva. Od tohoto
  commitu: HtH i benchmark čísla nesrovnávat s pre-swap historií; do commit
  message a do paměti zapsat hash jako „gate side-balanced od".
- **Benchmark kotvy:** před prvním post-reset během jednorázově nastavit
  v `weights_best_meta.json` `all_time_best_benchmark: 0.0` → kód sám spadne
  do větve „bez reference" (k=1.0, :542) a HARD-REJECT vs starou škálou
  nehrozí; první post-reset promote založí novou all_time_best škálu
  (:580). `BM_FLOOR=0.77` je absolutní kotva ze staré škály — post-swap bm
  klesne nejvýš o řád slot-edge (benchmark vs random je stropovaný, kandidát
  ~86.7 % vyhrává z obou slotů), takže floor pravděpodobně dál drží; kdyby
  první post-reset běh ukázal opak, je to důvod floor přeukotvit přes nový
  `BB_BM_FLOOR`, ne důvod swap vracet.

## 3. Testy / levné ověření

Vzestupně podle ceny; (a) lze dnes při běžícím tréninku, (b)+(c) až po něm.

**(a) Offline logika, nulová cena, bez enginu** (~1 s, nekoliduje s PID
2474 — engine `.so` se vůbec neimportuje): stub `sys.modules['bb_engine']`
fake modulem, jehož `simulate_game_logged` si zapíše kwargs a vrátí
deterministické skóre (`home_score=7, away_score=3`). Asserty:

- 10-tuple s `cand_is_away=False`: worker volal `weights_path=gate`,
  `away_weights_path=frozen`, vrátil `(7, 3, 0)`;
- s `cand_is_away=True`: `weights_path=frozen`, `away_weights_path=gate`,
  vrátil `(3, 7, 1)` (flip skóre na perspektivu kandidáta);
- legacy 7/8/9-tuple: identické kwargs jako dnes, návrat 2-tuple `(7, 3)`;
- `_benchmark_game` 8-tuple away: `home_ai='random'`, `away_ai='macro_mcts'`,
  výsledek `away>home`; legacy 6/7-tuple beze změny;
- rozvrh: pro N=600 přesně 300/300 orientací a 60 her na každou
  (race_idx%5 × orientace) buňku.

**(b) Null gate (rozhodující test, po doběhnutí tréninku a rebuildu):**
kandidát := frozen := `weights_best.json`, N=200-300 přes
`diag_utils.run_arm` + `paired_seeds(base=20260715)` s 10-tuple tasky
(orientace `i % 2`). Očekávání:

- pooled HtH kandidáta ≈ 50 % decisive, |odchylka| < 2σ (σ = 0.5/√dec);
  **tohle je přímá replika staré null-test metodiky — v biased setupu by
  vyšlo ~60 %, po swapu musí vyjít ~50 %**;
- home-slot win share ≈ shodná v obou ramenech (cand@H win rate ≈
  cand@A loss rate) — potvrzuje, že strukturní edge se přelévá mezi ramena
  symetricky a v poolu se ruší;
- draw rate per rameno bez velkého rozdílu (vstup do §4 kvantifikace).

Cena ≈ polovina jednoho gate (200-300 her, MCTS=100) — desítky minut,
běžný detached diag běh.

**(c) První ostrý gate:** nový `Side audit:` řádek zkontrolovat ručně —
`home-slot wins` podíl je post-H2-fix odhad zbytkového edge a zároveň
regresní kanárek celé konstrukce (kdyby po nějakém budoucím engine zásahu
vystřelil, gate to sám nahlásí v každém logu).

## 4. Riziko šumu — kvantifikace

Side-swap **nepůlí N verdiktu**. Verdikt (chess_score) se počítá pooled přes
všechny rozhodnuté hry; σ = 0.5/√decisive na orientacích nezávisí.

- **Rozptyl pooled skóre za nullu se swapem mírně KLESÁ, ne roste:** hry
  jsou nezávislé Bernoulli s pₕ = 0.5+δ a pₐ = 0.5−δ (δ = slot-edge);
  Var(Σ)/N = p̄(1−p̄) − δ² ≤ 0.25, tj. o δ² pod binomickým maximem. Žádná
  penalizace.
- **Decisive count:** jediný kanál, kudy by swap σ hnul, je odlišná draw
  rate v AWAY orientaci. I kdyby pooled draws stouply o 4pp (46 %→50 %),
  σ jde z 2.78 % na 2.89 % a práh při k=2.0 z 55.6 % na 55.8 % — hluboko
  pod rozlišením gatu. Skutečnou deltu změří null gate (b).
- **Co se půlí, je jen diagnostika:** per-orientace metriky v Side auditu
  mají N/2 na rameno → σ ramene ×√2 (≈ 3.9pp decisive-share při 600 hrách,
  ~162 decisive/rameno). To nic negatuje — ramena nejsou vstup verdiktu,
  jen monitor.
- **Bonus, ne šum:** kandidát je nově hodnocen na obou úlohách (receive-first
  i kick-first). Model přeučený na jednu roli už nedostane celý gate na
  míru — to je záměrné zpřísnění správným směrem, ne varianční artefakt.

Čistá bilance: bias −(až ~10pp posun nully), šum ±≈0. Jednoznačně výhodná
směna.

## 5. Shrnutí doporučení

1. Aplikovat patch §1 (gate + benchmark swap, orientace `i % 2`, flip ve
   workeru, Side audit řádek) **v jednom balíku s dnešním H2/screen baseline
   resetem** — jeden diskontinuitní bod v historii metrik. (Když balík dnešní
   reset nestihne: HtH swap nasadit stejně hned, benchmark-swap odložit —
   viz podmíněnost v §1d.)
2. Práh nechat (`0.5 + k·σ`, GATE_SIGMA_* beze změny); před prvním
   post-reset během nastavit `all_time_best_benchmark: 0.0` v
   `weights_best_meta.json`; přidat `BB_BM_FLOOR` env pojistku.
3. Ověření: offline stub test (a) hned, null gate (b) po doběhnutí tréninku,
   kontrola Side auditu (c) v prvním ostrém běhu.
4. Nezapomenout: swap řeší slotový bias, NE váhovou necitlivost HtH při
   vf_blend=0 (null-weights finding) — interpretační strop gatu trvá, dokud
   se nevyřeší vf_blend/policy_blend disconnect (samostatná master-list
   linie).
