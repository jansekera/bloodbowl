# Blood Bowl - Projekt Plan

## Shrnutí
Desková hra Blood Bowl jako webová aplikace. Hráč (coach) vs AI protivník.
Postupná implementace od team managementu přes herní engine až po AI.

## Tech Stack
- **Backend**: PHP 8.2+ / PostgreSQL / Twig (stejné patterny jako php-pgsql-demo)
- **Frontend**: TypeScript kompilovaný přes Vite, HTML Canvas 2D pro hřiště
- **CSS**: Moderní CSS (Grid, Custom Properties, animace)
- **Testy**: PHPUnit + Playwright
- **Umístění**: `/home/jenda/claude/blood-bowl/`

## Architektura

```
┌──────────────────────────────────────────────────┐
│  Frontend (TypeScript + Vite + Canvas 2D)        │
│  PitchRenderer │ GameStateManager │ API Client   │
├──────────────────────────────────────────────────┤
│  API Layer (PHP Controllers)                     │
│  GameAPI │ TeamAPI │ AuthAPI │ RaceAPI            │
├──────────────────────────────────────────────────┤
│  Service Layer                                   │
│  MatchService │ TeamService │ ProgressionService  │
├──────────────────────────────────────────────────┤
│  Game Engine (čistá logika, žádné IO)            │
│  GameState │ ActionResolver │ RulesEngine │ AI   │
├──────────────────────────────────────────────────┤
│  Data Layer (PDO/PostgreSQL)                     │
│  Repositories │ Event Store │ Migrations         │
└──────────────────────────────────────────────────┘
```

**Klíčový princip**: Game Engine je čistý (pure) - vstup: GameState + akce → výstup: nový GameState + eventy. Žádné IO, 100% testovatelný.

## Fáze implementace

### Fáze 1: Skeleton + Datový model ✅
- Scaffolding projektu (composer, Vite, PHPUnit, phpstan)
- Migrace: `races`, `positional_templates`, `skills`, `positional_template_skills`, `coaches`
- Seed: 3 rasy (Human, Orc, Skaven), ~30 core skills
- Entity: Race, PositionalTemplate, Skill, Coach
- Repositories + AuthService
- Value Objects: PlayerStats, Treasury, Position
- Twig layout + dashboard
- API: `GET /api/v1/races`, `GET /api/v1/skills`
- **56 testů, 210 assertions**

### Fáze 2: Team Management ✅
- Entity: Team, Player
- TeamService: tvorba týmu, nákup hráčů, správa treasury
- Roster validace (max positionals, 11-16 hráčů)
- Twig stránky: seznam týmů, detail/roster, vytvoření
- CRUD API pro teams a players
- Tabulky: `teams`, `players`, `player_skills`
- **103 testů, 365 assertions (celkem)**

### Fáze 3: Canvas Frontend ✅
- Vite konfigurace pro TypeScript
- PitchRenderer: 26×15 grid, end zóny, wide zóny
- PlayerSprite: kruhy s barvami týmu a čísly
- Souřadnicový systém (pixel ↔ grid)
- Hover tooltip, ball rendering
- API client (fetch wrapper)
- Twig match stránka hostující canvas
- **139 testů celkem (103 PHP + 36 Vitest)**

### Fáze 4: Herní engine - Pohyb
- GameState (immutable, wither metody)
- GamePhase enum + GameStateMachine
- DiceRollerInterface + RandomDiceRoller + FixedDiceRoller (testy)
- RulesEngine: validace akcí
- Pathfinder: BFS pro platné pohyby, dodge, GFI
- TacklezoneCalculator
- ActionResolver: zpracování MOVE
- Turnover detekce
- Tabulky: `matches`, `match_events`, `match_players`
- API: create match, get state, submit action
- Frontend: výběr hráče, highlight platných polí, klik pro pohyb
- **~80 testů** (nejtěžší fáze)

### Fáze 5: Blokování a zranění
- BlockResolver: síla, asistence, block dice (6 faces)
- Pushback výběr, follow-up
- InjuryResolver: armor roll → injury roll → casualty tabulka
- Blitz akce (pohyb + blok, 1× za tah)
- StrengthCalculator
- Frontend: zobrazení block dice, pushback UI
- **~60 testů**

### Fáze 6: Míč, skórování, kompletní zápas
- Pickup, Pass, Catch, Hand-off, Interception
- PassResolver, ScatterCalculator
- Kickoff sekvence + KickoffResolver (zjednodušená tabulka)
- Setup validace (3 na LoS, limity wide zón)
- Touchdown, half-time, konec zápasu
- Frontend: ScoreBoard, GameLog, animace
- **~70 testů** → **MILESTONE: hratelný kompletní zápas**

### Fáze 7: AI Protivník
- AICoachInterface: decide(), chooseBlockDice(), setupFormation()
- RandomAICoach (baseline)
- GreedyAICoach (hodnocení okamžitého přínosu)
- PositionalAICoach (cage, screen, pozice)
- BoardEvaluator, ActionScorer
- Frontend: animace AI tahu
- **~40 testů** → **MILESTONE: cílový stav - hráč vs AI**

### Fáze 8: Skills systém + Progrese
- ~30 core skills s efekty (Block, Dodge, Sure Hands, Mighty Blow, Guard...)
- SkillEffect pattern (jeden effect class per skill)
- SPP tracking, level-up systém
- Post-match sekvence
- **~50 testů**

### Fáze 9: Re-rolly, počasí, polish
- Team re-rolly (1× za tah)
- Weather effects (5 typů)
- Foul akce
- Game replay z event logu
- UI: odds preview, path preview, animace kostek
- **~30 testů**

### Fáze 10: Ligy a rozšířený obsah
- League systém, round-robin schedule
- Další rasy (Wood Elves, Dwarves, Dark Elves, Undead, Chaos, Nurgle)
- Inducements, Star Players
- Coach statistiky, leaderboard
- **~30 testů**

## DB Schema (hlavní tabulky)

```
races, positional_templates, skills, positional_template_skills  (Fáze 1) ✅
coaches                                                           (Fáze 1) ✅
teams, players, player_skills                                     (Fáze 2) ✅
matches, match_events, match_players                              (Fáze 4)
leagues, league_teams, league_rounds                              (Fáze 10)
```

## Event Sourcing
- Každá akce ve hře = `MatchEvent` v DB
- Aktuální stav cachován jako JSONB v `matches.game_state`
- Replay: přehrání eventů krok po kroku

## Adresářová struktura

```
blood-bowl/
├── public/index.php          # Entry point + routing
├── public/css/               # Moderní CSS
├── public/dist/              # Vite build output
├── src/
│   ├── Controller/           # API + Page controllers
│   ├── Entity/               # Immutable entity classes
│   ├── DTO/                  # GameStateDTO, ActionResult, ...
│   ├── Enum/                 # GamePhase, PlayerAction, BlockDiceFace, ...
│   ├── Engine/               # ČISTÁ herní logika (žádné IO)
│   ├── AI/                   # AI coach implementace
│   ├── Repository/           # Data access (PDO)
│   ├── Service/              # Business logic orchestrace
│   ├── ValueObject/          # Position, PlayerStats, Treasury
│   ├── Exception/            # Custom exceptions
│   └── Validation/           # Validator
├── frontend/src/             # TypeScript (Vite)
│   ├── canvas/               # PitchRenderer, PitchGeometry, DemoState
│   ├── state/                # GameStateManager, SelectionState
│   ├── api/                  # API client, types
│   └── ui/                   # Tooltip, ScoreBoard, GameLog
├── templates/                # Twig šablony
├── tests/                    # PHPUnit testy
├── e2e/                      # Playwright E2E testy
├── migrations/               # SQL migrace
└── seed.php                  # Seed data (rasy, skills)
```

## Verifikace
- `composer test` - PHPUnit unit + integration testy
- `composer phpstan` - statická analýza (level 8)
- `npm run build` - Vite build pro frontend
- `npm run test` - Vitest frontend testy
- `npm run test:e2e` - Playwright E2E testy
- Každá fáze: ruční test v prohlížeči (team management, match play)
- AI stress test: 100 AI vs AI zápasů bez chyb

## C++ Engine & AI — Kroky

### Hotovo

1. **Phase 1 — Data struktury** ✅
   - Enums, Position, Player, SkillSet, GameState, DiceRoller, BallState, TeamState, GameEvent

2. **Phase 2 — Core mechaniky** ✅
   - Move/dodge/GFI, block, foul, injury, ball handling, pathfinder, rules engine, action resolver

3. **Phase 3 — MCTS simulace** ✅
   - 26 rosterů, game simulator, feature extractor (70 features), value function (linear+neural), MCTS tree search, random/greedy/MCTS policies, CLI

4. **Phase 4A — Rosters + Pass/HandOff** ✅
   - Všech 26 rosterů s CRP stats, resolvePass, resolveHandOff, Bresenham interception, accuracy rolls, HailMaryPass, SafeThrow, StrongArm, NervesOfSteel

5. **Phase 4B — BigGuy checks + skills** ✅
   - BoneHead, ReallyStupid, WildAnimal, TakeRoot, Bloodlust, Leap, Tentacles, Shadowing

6. **Phase 4C — Kickoff events** ✅
   - Celá 2D6 tabulka: Riot, High Kick, Cheering, Coaching, Weather, Quick Snap, Blitz, Throw A Rock, Pitch Invasion, touchback, Kick-Off Return

7. **Phase 4D — pybind11 binding** ✅
   - `bb_engine` Python modul, enums, GameState, actions, features (numpy), simulate_game

8. **Phase 5 — Zbylé action handlery** ✅
   - TTM, BombThrow, HypnoticGaze, BallAndChain, MultipleBlock

9. **Phase 6 — Policy Network** ✅
   - Lineární policy 85→1, PUCT formula, PolicyDecision logging, Python PolicyTrainer, combined weights save/load

10. **Macro-actions** ✅
    - 11 makro typů (SCORE, ADVANCE, CAGE, BLITZ, BLOCK, PICKUP, PASS_ACTION, FOUL, REPOSITION, END_TURN, BLITZ_AND_SCORE)
    - Branching ~200→~15, MCTS hloubka 2-3 s 400 iteracemi
    - MacroMCTSPolicy: stateful search → expand → validate

11. **MCTS quality fixes** ✅
    - FPU (First Play Urgency), tanh VF normalizace, progressive widening (maxChildren=40), Dirichlet noise (alpha=0.3)

12. **Heuristiky fáze 1** ✅
    - BLITZ_AND_SCORE, macro ordering, last-turn scoring, prior boosts (BLOCK 12%, CAGE 8%, END_TURN cap 10%)

13. **Heuristiky fáze 2** ✅
    - Urgency (trailing 2+ → SCORE 50%), carrier safety (cage prior), sideline avoidance (+6), safety player, screen formation

14. **Vrstva 2 — Defensive kickoff** ✅
    - 3 LOS + 7 contain wall + 1 deep safety, deep kick (x=22/3), Kick skill (halves scatter)

15. **Vrstva 3 — Roster-aware kickoff** ✅
    - RosterSpeed enum (SLOW/MIXED/FAST), classifyRosterSpeed (avg MA), pressure formace vs fast, deep receiver formace, short/deep kick

16. **Vrstva 4 — Defensivní strategie** ✅
    - BLITZ: carrier targeting (+10), scoring threat (+4), multi-target na obraně
    - REPOSITION: 4-layer defense (safety, carrier marker, endzone guard, screen)
    - Best-blitzer selection, MCTS defense priors (BLITZ 20%, REPOSITION 5%, TZ-on-carrier heuristic)

17. **Neural policy network** ✅
    - 85→32(ReLU)→1, ~2752 parametrů
    - C++ forward pass + JSON loading, Python NeuralPolicyTrainer s backprop
    - `--policy-model=neural` CLI argument

### Probíhá

18. **Trénink neural policy** 🔄
    - 6 epoch × 20 her, Human vs orc/skaven/dwarf/wood-elf, 400 MCTS iterací
    - Benchmark: best 86.7% vs random (30 her)

### Plánované — heuristiky & search

19. **Policy tuning**
    - Temperature optimalizace, hidden size (32→64), learning rate, více epoch

20. **One-turn TD**
    - Rozpoznání a realizace 1-tahového touchdownu (sprint do endzone)

21. **Vs bash/agility adaptace**
    - Dodge-back vs bash, contain & squeeze vs agility, sideline traps, hunter/shield split, tag cage corners

22. **Transposition table**
    - Sdílení MCTS uzlů mezi podobnými stavy

23. **Deeper search**
    - Víc MCTS iterací, efektivnější expand, pruning

### Plánované — AlphaZero

24. **Value function trénink**
    - Zapnout VF learning (teď lr=0), neural VF (70→32→1), trénovat z MCTS outcomes

25. **Self-play pipeline**
    - Generování trénovacích dat self-play místo vs greedy/random

26. **Joint training**
    - Policy + value simultánně z jedné hry

27. **Deeper policy network**
    - 85→64→32→1 (2 hidden layers), případně residual connections

28. **Iterativní self-play**
    - Checkpoint systém, ELO tracking, best-model gating (nový model musí porazit starý)

29. **Full AlphaZero loop**
    - Self-play → train → evaluate → repeat, plně automatický cyklus
