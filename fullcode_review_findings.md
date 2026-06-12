# Full-code review (Opus, multi-agent, Team 1 styl) — 2026-06-12

5 paralelních Opus agentů, každý vlastní doména. Detaily v dílčích souborech:
`fullcode_review_rules.md`, `_mcts.md`, `_training.md`, `_gating.md`, `_webapp.md`.

⚠️ **Pozn. ke skepsi:** Dvě „CRITICAL" hlášky (MCTS C1, Training C1/C2) tvrdí korupci tréninku. Model ale stabilně dosahuje 85-87 %, což u skutečně obráceného sign-backupu nečekáš (spíš near-random hra). → **degradace/strop, ne katastrofa**; nejdřív OVĚŘIT konvenci perspektivy value funkce, teprve pak opravovat. Nepouštět se do velkých přepisů bez reprodukčního testu.

---

## TIER 1 — potenciálně lámou plateau (ověřit → opravit, vysoký dopad)

### T1.1 MCTS value backup — perspektiva napříč tahy `[OVĚŘIT PRVNÍ]`
`macro_mcts.cpp:509-515`, `mcts.cpp:276-286`. Backprop přičítá leaf value **stejným znaménkem** ve všech uzlech; END_TURN přepíná `activeTeam` bez negace. Pokud value net vrací hodnotu z pohledu side-to-move → potřeba negamax flip na hranici tahu; pokud z fixní perspektivy → současný kód OK. **Nutno přečíst `value_function.cpp` perspektivu před jakýmkoli zásahem.** Reprodukční test: END_TURN pouštějící soupeře na TD musí mít nižší Q než blokující makro.

### T1.2 Replay buffer credit assignment `[CONFIRMED korupce]`
`training_loop.py:386-405` + `replay_buffer.py:50-69`. Replay razí finální výsledek hry (±1) na KAŽDÝ stav (i nepoterminální) a staví fake 2-stavové mini-hry → mezilehlá odměna (0) se nikdy nereprezentuje, bootstrap o krok dřív. Auto-zapnuté pro každý MCTS běh. Ničí intra-game credit assignment. **Top kandidát na plateau/43% draws.**

### T1.3 Feature parity drift C++ ↔ Python `[latentní landmine]`
`features.py[15]` carrier_dist_to_td (nejtěžší shaping váha −1.5) a `[17/18]` avg-x se liší od C++ ve větvích: soupeř nese míč / prázdná strana / stojící carrier. Dnes latentní (C++ loguje vlastní featury), ale **parity test je slepý** — porovnává s PHP, ne s `bb_engine`, a netestuje tyto větve (`tests/test_features.py` H3). Smrtelné pro plánovaný per-player přechod (warm start přes `_align_features` tiše pad/truncate).

---

## TIER 2 — robustnost gatingu (levné, brání zahozeným běhům)

- **C-1 `run_iteration.py:173-175`** `[sibling právě opraveného bugu]`: `except Exception:` vynuluje `frozen_bm`+`all_time_best_bm` BEZ `baseline_reset=True` → corrupt/transient meta read vypne regresní gate a natrvalo přepíše all-time-best nižším skóre. **Fix: v except nastavit baseline_reset nebo abort.**
- **C-3 `:61-81`** watchdog `pool.terminate()` vrací partial results bez floor/abort → gate může promovat na 30-150 hrách (SD ±9 %).
- **C-2 `:219-248`** obě benchmark půlky jedou stejnou `race_idx` sekvenci, gate = `max(bm_az,bm_tb)` → max-of-two pozitivně vychýlený ~+1.5-2 % (≈ BM_DROP_LIMIT). Seed 1..999999 kolize.
- **H-2 `:312,:355`** neatomický `json.dump` meta → kill uprostřed zápisu vyrobí corrupt meta, který krmí C-1. **Fix: write-temp+rename.**
- **H-3 `:341-378`** při selhání `git push` další iterace `git reset --hard origin/main` zahodí nepushnutý promote commit; návratový kód `git commit` ignorován.
- **H-4** chess gate ANTI_REGRESSION=0.51 při 66-72 % remíz mechanicky ~0.50, uvnitř šumu (známé).

## TIER 3 — věrnost pravidel (vychyluje self-play data)

- **HIGH** Guard chybně dává foul asistence `foul_handler.cpp:19` → `helpers.cpp:139` — nafukuje armor rolly u faulů. (BB2016: Guard fauly neasistuje.)
- **HIGH** Chainsaw block vynechává +3 armor modifikátor `block_handler.cpp:197-219`.
- **MED/HIGH** Nepřesné (non-fumble) přihrávky nelze rerollnout `pass_handler.cpp:222-300` (reroll gated jen na `roll==1`) → AI zaujatá proti přihrávání.
- **MED** Piling On neimplementováno; Really Stupid asist počítá i druhého Really Stupid `big_guy_handler.cpp:27`; Take Root nepersistentní `big_guy_handler.cpp:73`. (Jen pokud rostery tyto skilly fieldují.)
- ✅ Ověřeno OK: **Sure Hands ruší Strip Ball** `block_handler.cpp:461` korektní.

## TIER 4 — bezpečnost webové appky (⏸️ ODLOŽENO 2026-06-12)

**Stav: DEFERRED.** Lokální single-user prostředí, žádný přístup zvenčí → není akutní. Dořešit až bude web vystaven. Nálezy zůstávají platné, jen nemají prioritu.


- **CRITICAL C1** `MatchApiController.php:68` / `MatchService.php:222`: `submitAction` jen `requireAuth()`, nekontroluje že coach je účastník/na tahu → kdokoli přihlášený hraje cizí zápas (i soupeřův tah).
- **CRITICAL C2** `MatchPageController.php:35`: createMatch nekontroluje vlastnictví týmů.
- **CRITICAL C3** `TeamApiController.php:149`: advancePlayer/getAvailableSkills IDOR — skilly cizím hráčům.
- **HIGH H1** všechny match read endpointy bez auth (scrape stavu). **H2** žádné CSRF + cookie bez SameSite. **H3** TOCTOU last-write-wins na match akcích a treasury (overspend).
- ✅ Pozitiva: parametrizované SQL (no injection), integer money guarded, Twig autoescape (no XSS), engine ověřuje příslušnost hráče k aktivnímu týmu. Žádné Phase-7 stuby ve web vrstvě.

---

## Doporučené pořadí
1. **TIER 2 gating fixy** (C-1, C-3, H-2) — levné, hned, chrání příští dlouhý běh. Dělat jako první commit.
2. **TIER 1.1 ověřit** value perspektivu (přečíst value_function.cpp) — rozhodnout, jestli je T1.1 reálný bug.
3. **TIER 1.2 replay** — nejjistější korupce; opravit reprezentaci transitions.
4. **TIER 3 Guard foul + Chainsaw** — rychlé rules fixy, čistší self-play data (vyžadují rebuild + re-train).
5. **TIER 1.3 parity test** opravit, než se sáhne na per-player.
6. **TIER 4 web security** — samostatná kolej, kdykoli (netýká se AI tréninku).
