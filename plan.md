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

# test

cd /home/jenda/claude/blood-bowl/python && python3 -m blood_bowl.train_cli --epochs=10 --games=15 --opponent=random --lr=0.01

# Evaluate trained agent
python -m blood_bowl.evaluate --weights=weights.json --opponent=random --matches=100

# Visualize training curve
python -m blood_bowl.visualize --csv=training_results.csv

# Výstupy:
- weights.json — natrénované váhy (použitelné v PHP)
- training_results.csv — win rate po každé epoše
- training_logs/ — JSONL logy ze všech her
