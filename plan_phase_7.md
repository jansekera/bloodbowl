# Blood Bowl - Fáze 7: Rerolly, Stand Up, Foul, Setup UX, AI, Python

## Cíl
Kompletní hratelná hra s AI soupeřem. Zahrnuje chybějící herní mechaniky (rerolly, stand up, foul), setup UX, rule-based AI, rozšíření skills a Python wrapper pro budoucí ML trénování.

## Pořadí implementace (9 kroků)

| # | Úkol | Typ | Testy |
|---|------|-----|-------|
| 1 | Team Rerolls (auto-reroll) | úprava | ~12 |
| 2 | Stand Up pro prone hráče | úprava | ~10 |
| 3 | Foul akce + ejection | nový + úprava | ~12 |
| 4 | Rozšíření skills (Tackle, Frenzy, Stand Firm, Strip Ball, Side Step) | úprava | ~12 |
| 5 | Setup UX (frontend reserves panel + auto-setup) | nový + úprava | ~6 |
| 6 | AI Coach interface + RandomAI | nový | ~8 |
| 7 | GreedyAI (rule-based) | nový | ~8 |
| 8 | AI integrace do MatchService + frontend | úprava | ~6 |
| 9 | Python wrapper + CLI simulator | nový (Python) | ~8 |
| | **Celkem** | | **~82** |

---

## Krok 1: Team Rerolls (Auto-Reroll)

Data model už existuje v `TeamStateDTO`: `canUseReroll()`, `withRerollUsed()`, `resetForNewTurn()`. Chybí mechanismus pro jejich použití.

**Přístup: Auto-reroll** - pokud roll selže a tým má reroll k dispozici, automaticky ho použije. Důvod: synchronní request/response architektura neumožňuje "pauzu" pro interaktivní dotaz.

**Pravidlo "nelze rerollovat reroll"**: Pokud skill reroll (Sure Hands, Catch, Dodge) selhal, team reroll se NEpoužije na stejný hod.

### Změny

**`src/DTO/GameEvent.php`** - přidat `rerollUsed()`
**`src/Engine/ActionResolver.php`** - reroll logika pro:
- **Dodge** (resolveMove): roll selže → Dodge skill reroll (NOVÉ - skill zatím jen modifikuje target, ne reroll) → pokud nepoužit skill, team reroll
- **GFI** (resolveMove): roll selže → team reroll (žádný skill)
- Po každém rerollu: `$state->withTeamState($side, $teamState->withRerollUsed())`

**`src/Engine/BallResolver.php`** - přidat `bool $teamRerollAvailable = false` parametr do:
- `resolvePickup()`: fail → Sure Hands (existuje) → pokud ne, team reroll
- `resolveCatch()`: fail → Catch skill (existuje) → pokud ne, team reroll
- Vrací navíc `rerollUsed: bool` v result array

**`src/Engine/PassResolver.php`** - reroll pro accuracy:
- Přidat Pass skill reroll (NOVÉ - skill zatím jen modifikuje target)
- Pokud ne, team reroll

**Frontend**: tlačítko "Rerolls: X" v ActionPanelu (informativní, auto-reroll)

---

## Krok 2: Stand Up

Prone hráči momentálně nemohou nic dělat. Pravidla: vstání stojí 3 MA. Pokud MA < 3, nutný roll 4+.

### Změny

**`src/DTO/MatchPlayerDTO.php`** - `canMove()`: povolit PRONE hráčům
```php
return ($this->state === PlayerState::STANDING || $this->state === PlayerState::PRONE) && !$this->hasMoved;
```

**`src/Engine/ActionResolver.php`** - na začátku `resolveMove()`:
- Pokud hráč PRONE → stand up (3 MA), pak pokračovat pohybem
- Pokud MA < 3 → roll D6, 4+ úspěch (stane, ale 0 MA zbývá)
- Pokud cíl = aktuální pozice → jen vstání bez pohybu
- Event: `GameEvent::standUp()`

**`src/Engine/Pathfinder.php`** - pro PRONE hráče: `availableMA = max(0, MA - 3)`

**`src/Engine/RulesEngine.php`**:
- `getValidMoveTargets()`: pro PRONE zahrnout aktuální pozici (stand in place)
- `getAvailableActions()`: MOVE dostupný i pro PRONE hráče

**`src/DTO/GameEvent.php`** - `standUp(int $playerId, int $roll, bool $success)`

---

## Krok 3: Foul akce

Once per turn. Cíl: adjacent prone/stunned enemy.

### Mechanika
1. Armor roll: 2×D6 + 1 (prone bonus). Pokud > AV → armor broken → injury roll
2. Referee: pokud obě kostky stejné (doubles) → hráč vyloučen
3. Mighty Blow NEplatí u foulů
4. Foul NEZPŮSOBUJE turnover (ani vyloučení)

### Změny

**`src/Enum/PlayerState.php`** - přidat `EJECTED = 'ejected'` (isOnPitch=false, canAct=false)

**`src/DTO/GameEvent.php`** - `foulAttempt()`, `playerEjected()`

**`src/Engine/ActionResolver.php`** - `resolveFoul(state, params)`:
- Validate: adjacent, target prone/stunned, foul not used
- Roll 2×D6 individuálně (pro doubles detection)
- Armor check: sum + 1 > defender.AV → injury
- Doubles check: die1 === die2 → eject attacker
- Mark: `withFoulUsed()`

**`src/Engine/RulesEngine.php`**:
- `validateFoul()`: canAct, adjacent prone/stunned enemy, foul not used
- `getFoulTargets(state, player)`: adjacent prone/stunned enemies
- `getAvailableActions()`: FOUL (once per turn, pro hráče s adjacent prone/stunned)

**API + Routes**: `GET /matches/{id}/players/{pid}/foul-targets`
**Frontend**: Foul toggle button (disabled when used), foul mode v match.ts

---

## Krok 4: Rozšíření Skills

### 4A. Tackle
- Block: DEFENDER_STUMBLES → neguje Dodge (defender padá i s Dodge)
- Dodge roll: neguje Dodge skill reroll (pokud adjacent enemy má Tackle)
- `applyBlockResult()`: `if (!$defender->hasSkill('Dodge') || $attacker->hasSkill('Tackle'))`
- `resolveMove()` dodge reroll: check adjacent enemies for Tackle

### 4B. Frenzy
- Po bloku kde defender pushed → povinný follow-up + druhý blok (pokud adjacent)
- `resolveBlock()`: po applyBlockResult, if attacker hasSkill('Frenzy') && defenderPushed && defender stále adjacent → druhý blok

### 4C. Stand Firm
- Defender s Stand Firm nemůže být pushed (jen knockdown pokud applicable)
- `applyBlockResult()`: if defender hasSkill('Stand Firm'), skip pushback

### 4D. Strip Ball
- Při pushback: pokud attacker má Strip Ball a defender nese míč → míč padá
- `resolvePushback()`: if attacker hasSkill('Strip Ball') && ball carrier → drop ball

### 4E. Side Step
- Defender s Side Step si vybírá push square (auto: preferovat bezpečný square)
- `resolvePushback()`: if defender hasSkill('Side Step'), sort push squares by safety

---

## Krok 5: Setup UX (Frontend)

Backend SETUP_PLAYER už funguje. Chybí frontend UI.

### Nové soubory

**`frontend/src/ui/ReservesPanel.ts`** - scrollovatelný seznam hráčů OFF_PITCH:
- Číslo, jméno, pozice, stats
- Klik = vyber hráče pro umístění
- Viditelný jen během SETUP fáze

### Změny

**`templates/matches/show.html.twig`** - přidat `<div id="reserves-panel"></div>`

**`frontend/src/match.ts`** - setup mode:
- Klik na reserves panel → select player
- Klik na pitch → SETUP_PLAYER action (pokud valid square)
- Po END_SETUP → skrýt reserves, přepnout na další tým / kickoff

**`frontend/src/canvas/PitchRenderer.ts`** - highlight valid setup squares

**`frontend/src/state/GameStateManager.ts`** - přidat 'setup' do SelectionMode

**`public/css/styles.css`** - reserves panel styling

### Auto-setup (backend)
**`src/Engine/ActionResolver.php`** - v `resolveEndSetup()`:
- Po END_SETUP lidského týmu, pokud AI tým nemá hráče → auto-place
- Default formace: 3 na LoS, 4 midfield, 4 backfield
- Respektovat wide zone limity (max 2)

---

## Krok 6: AI Coach Interface + RandomAI

### Nové soubory

**`src/AI/AICoachInterface.php`**:
```php
interface AICoachInterface {
    public function decideAction(GameState $state, RulesEngine $rules): array;
    // Returns: ['action' => ActionType, 'params' => array]

    public function setupFormation(GameState $state, TeamSide $side): GameState;
    // Places 11 players in valid positions
}
```

**`src/AI/RandomAICoach.php`**:
- `decideAction()`: získá available actions, vybere náhodnou
  - Pro MOVE: náhodný hráč, náhodný valid target
  - Pro BLOCK: náhodný attacker, náhodný target
  - Fallback: END_TURN
- `setupFormation()`: default formace (3 LoS, rest random valid)

---

## Krok 7: GreedyAI (Rule-based)

**`src/AI/GreedyAICoach.php`** - skorovací systém:

### Prioritní řetězec akcí
1. **Score touchdown** - pokud ball carrier může dojít do end zóny → MOVE (score 1000)
2. **Blitz ball carrier** - pokud soupeř nese míč → BLITZ (score 500)
3. **Pick up ball** - pokud míč na zemi, hráč nearby → MOVE na míč (score 400)
4. **Cage formation** - posunout hráče kolem ball carriera (score 200)
5. **Block** - silnější bloky preferovat, 2-dice > 1-dice (score 100-300)
6. **Move forward** - posunout hráče směrem k soupeřově end zóně (score 50-150)
7. **Foul** - pokud available a enemy prone nearby (score 80)
8. **Pass** - pokud lepší pozice pro score (score 100-200)
9. **END_TURN** - když nic lepšího (score 0)

### Skorování pozice
- Distance to opponent end zone (-10 per square)
- Ball carrier safety (+50 per friendly adjacent, -50 per enemy adjacent)
- Tackle zone threat (-20 per enemy TZ on path)

---

## Krok 8: AI Integrace

### Změny

**`src/Service/MatchService.php`**:
- Nový field: `?AICoachInterface $aiCoach`
- Po `submitAction()`: pokud nový activeTeam je AI → auto-play celý tah
- Loop: dokud AI tým je aktivní, volej `$aiCoach->decideAction()` + `submitAction()`
- Timeout ochrana: max 100 akcí za tah (anti-infinite-loop)

**`src/DTO/GameState.php`** - přidat `?TeamSide $aiTeam` field (který tým řídí AI)

**Frontend (`match.ts`)**:
- Po submitAction, pokud nový state.activeTeam === aiTeam → server už odehrál AI tah
- Animace: zobrazit AI akce s malým zpožděním (events postupně)
- Nový API response: zahrnout AI events v odpovědi

**Match creation**: nový checkbox "Play vs AI" → matchService creates match s aiTeam=AWAY

---

## Krok 9: Python Wrapper + CLI Simulator

### Nové soubory (PHP CLI)

**`cli/simulate.php`** - headless match simulátor:
```php
// Vstup: --home-team=ID --away-team=ID --home-ai=random|greedy --away-ai=random|greedy
// Výstup: JSON s výsledkem zápasu, events, final state
// Použití: php cli/simulate.php --home-team=1 --away-team=2 --away-ai=greedy
```

### Nové soubory (Python)

**`python/blood_bowl/__init__.py`**
**`python/blood_bowl/client.py`** - REST API client:
- `create_match()`, `get_state()`, `submit_action()`, `get_available_actions()`
- `get_move_targets()`, `get_block_targets()`, etc.

**`python/blood_bowl/game_state.py`** - dataclass zrcadlení PHP DTO:
- `GameState`, `MatchPlayer`, `TeamState`, `BallState`, `Position`
- `from_dict(data)` a `to_dict()` metody

**`python/blood_bowl/match_runner.py`** - orchestrace:
- `run_match(home_ai, away_ai)` - kompletní zápas
- `run_tournament(ai_list, num_rounds)` - turnaj AI proti AI

**`python/blood_bowl/cli_runner.py`** - alternativní runner přes PHP CLI:
- Volá `php cli/simulate.php` přes subprocess
- Rychlejší než REST (žádný HTTP overhead)

**`python/examples/play_100_matches.py`** - benchmark:
```python
from blood_bowl import MatchRunner, GreedyAI, RandomAI
runner = MatchRunner(api_url="http://localhost:8000")
results = runner.run_tournament([GreedyAI(), RandomAI()], rounds=100)
print(f"Greedy win rate: {results['greedy_wins']/100:.1%}")
```

**`python/requirements.txt`**: requests, dataclasses-json
**`python/setup.py`**: package setup

---

## Nové API endpointy
- `GET /api/v1/matches/{id}/players/{pid}/foul-targets` → FoulTarget[]

## Zjednodušení
- Auto-reroll (ne interaktivní) - server automaticky použije reroll
- AI auto-play celý tah najednou (ne akce po akci s pauzou)
- Setup auto-placement pro AI tým
- Kickoff table se odkládá na pozdější fázi
- Python wrapper primárně přes REST API

## Verifikace
- `vendor/bin/phpunit` - 319 + ~82 = ~401 testů
- `vendor/bin/phpstan` - 0 errors
- `npm run build` - TypeScript kompilace
- `npx vitest run` - frontend testy
- `php cli/simulate.php` - AI vs AI match běží do konce
- `python -m pytest python/tests/` - Python testy
- Manuální test: kompletní zápas člověk vs AI přes browser
