<?php
declare(strict_types=1);

namespace App\AI;

use App\DTO\GameState;
use App\Engine\RulesEngine;
use App\Enum\ActionType;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use App\ValueObject\Position;

final class LearningAICoach implements AICoachInterface
{
    /** ⭐ PHP33: obsazeny ROH klece. */
    private const CAGE_CORNER_BONUS = 1.5;
    /** Hrana klece -- lepsi nez nic, ale nesmi prebit roh. */
    private const CAGE_EDGE_BONUS = 0.2;
    /** ⭐ Uz stojim v rohu => DRZ POZICI. Musi prebit presun na jiny roh. */
    private const CAGE_HOLD_BONUS = 1.8;
    /** ⛔ Nosic NESMI utect vlastni kleci -- pokuta za kazde pole navic. */
    private const CAGE_OUTRUN_PENALTY = 0.9;
    /**
     * ⭐ Uzivatel 12.09.: "kdyz je jen jeden z peti MA 4 a ostatni MA 5, tak
     * je jeho pohyb POSLEDNI s GFI -- riziko na konec."
     * ⇒ Nejpomalejsi hrac klec NEZASTAVI: dozene ji pres GFI. Takove pole
     * navic tedy neni zakazane, jen RIZIKOVE -- mensi pokuta, a diky
     * `RISK_FREE_BONUS` se takovy tah zahraje az na konci kola.
     */
    private const CAGE_GFI_REACH = 2;
    private const CAGE_GFI_PENALTY = 0.25;
    /**
     * ⛔⛔ Uzivatel 12.09.: "a postavit nosice vedle soupere nesmime uz vubec."
     * Nosic, ktery skonci v zone zachyceni soupere, muze byt blokovan,
     * sražen a o mic pripraven -- a klec kolem nej uz nic nezachrani.
     * Pokuta je zamerne VETSI nez cokoli, co se za pohyb da ziskat, takze
     * takove pole prohraje se vsim krome touchdownu (ten ma vlastni vetev
     * a sem se nedostane).
     */
    private const CARRIER_NEXT_TO_ENEMY_PENALTY = 5.0;
    /** Od kolika obsazenych rohu se to uz pocita za klec, kterou ma cenu drzet. */
    private const CAGE_MIN_CORNERS = 2;
    /**
     * ⭐ PORADI AKTIVACI: NEJDRIV BEZ RIZIKA (uzivatel 12.09.).
     * Bonus dostane tah, ktery nepotrebuje ANI JEDEN hod -- zadny dodge,
     * zadne GFI. Tim se bezpecne aktivace udelaji DRIV a rizikove zbydou
     * na konec kola, kdy uz turnover stoji min.
     */
    private const RISK_FREE_BONUS = 0.4;

    private string $modelType = 'linear';
    /** @var list<float> */
    private array $weights = [];
    private float $epsilon;

    // Neural network parameters (only used when modelType === 'neural')
    /** @var list<list<float>> */
    private array $W1 = [];
    /** @var list<float> */
    private array $b1 = [];
    /** @var list<list<float>> */
    private array $W2 = [];
    /** @var list<float> */
    private array $b2 = [];

    /**
     * @param string|null $weightsFile Path to weights JSON file (null = zero weights)
     */
    public function __construct(?string $weightsFile = null, float $epsilon = 0.0)
    {
        $this->epsilon = $epsilon;

        if ($weightsFile !== null && file_exists($weightsFile)) {
            $json = file_get_contents($weightsFile);
            $data = $json !== false ? json_decode($json, true) : null;
            if (is_array($data) && isset($data['type']) && $data['type'] === 'neural') {
                $this->modelType = 'neural';
                $this->W1 = $data['W1'];
                $this->b1 = $data['b1'];
                $this->W2 = $data['W2'];
                $this->b2 = $data['b2'];
            } else {
                // ⛔⛔⛔ OPRAVA 11.09.2026: TADY SE NAČÍTAL CELÝ OBJEKT JSON.
                //   `array_map('floatval', $data)` jelo přes KLÍČE NEJVYŠŠÍ
                //   ÚROVNĚ (`type`, `value_weights`, `policy_weights`,
                //   `policy_bias`, `policy_temperature`), ne přes váhy.
                //   Výsledek: `floatval('alphazero_linear')`=0, `floatval(pole)`=1,
                //   `floatval(pole)`=1, bias, temperature
                //   ⇒ `$this->weights` byl **[0, 1, 1, ~0, 1]** -- PĚT čísel
                //   místo 73 natrénovaných vah.
                //
                // ⛔ A NEBYLO TO VIDĚT, protože `dotProduct` bere
                //   `min(count($a), count($b))` = min(5, 73) = 5 a zbytek
                //   TIŠE USEKNE -- bez chyby, bez hlášky. Stav se tedy
                //   hodnotil jako `f1 + f2 + f4` a celý trénink se zahazoval.
                //   Tatáž třída jako `try/catch` v `cli/simulate.php:157`:
                //   pojistka, která vadu schová před měřením.
                //
                // ⚠️ `array_map` nad plochým seznamem byl nejspíš správný pro
                //   STARŠÍ formát souboru (holé pole floatů). Formát se změnil
                //   na strukturovaný objekt a tohle místo se neaktualizovalo.
                //   Proto se podporují OBA tvary, ne jen ten nový.
                $raw = (isset($data['value_weights']) && is_array($data['value_weights']))
                    ? $data['value_weights']
                    : $data;
                $this->weights = self::normalizeWeights(array_values(array_map('floatval', $raw)));
            }
        } else {
            $this->weights = array_fill(0, FeatureExtractor::NUM_FEATURES, 0.0);
        }
    }

    /**
     * ⭐ KLEC (PHP33, zadani uzivatele 12.09.2026): klec je nosic + CTYRI
     *    DIAGONALNI ROHY. Ortogonalni soused je k nicemu -- souper na nosice
     *    dosahne stejne a jeste si to pole sam zabere.
     *
     * ⚠️ Uzivatel k tomu dodal: "jeste je varianta, kdy je nosic u kraje
     *    a jsou jen dva rohy -- ale to je nebezpecne." Dvourohá klec je tedy
     *    NOUZE, ne cil; tahle metoda o ni nic netvrdi, jen rika, jestli
     *    dane pole JE roh.
     */
    private static function isCageCorner(Position $carrierPos, Position $pos): bool
    {
        return abs($carrierPos->getX() - $pos->getX()) === 1
            && abs($carrierPos->getY() - $pos->getY()) === 1;
    }

    /**
     * Kolik SPOLUHRACU stoji v rozích klece kolem daneho pole.
     *
     * ⭐ PHP33 (B): pouziva se dvakrat -- kolem soucasne pozice nosice
     *   (mam vubec klec?) a kolem ciloveho pole (udrzel by se tvar?).
     */
    private function cornersHeldAround(GameState $state, TeamSide $side, Position $center, int $exceptId): int
    {
        $n = 0;
        foreach ($state->getPlayersOnPitch($side) as $p) {
            if ($p->getId() === $exceptId) {
                continue;
            }
            $pos = $p->getPosition();
            if ($pos !== null && self::isCageCorner($center, $pos)) {
                $n++;
            }
        }

        return $n;
    }

    /**
     * ⭐ PHP33: hraci vlastniho tymu stojici v ROZICH kolem daneho pole.
     */
    private function cornerPlayers(GameState $state, TeamSide $side, Position $center, int $exceptId): array
    {
        $out = [];
        foreach ($state->getPlayersOnPitch($side) as $p) {
            if ($p->getId() === $exceptId) {
                continue;
            }
            $pos = $p->getPosition();
            if ($pos !== null && self::isCageCorner($center, $pos)) {
                $out[] = $p;
            }
        }

        return $out;
    }

    /**
     * Kolik poli ujde NEJPOMALEJSI z klece. Uzivatel 12.09.: "max pohyb
     * klece podle nejmensiho MA ze vsech peti."
     */
    private function slowestRemaining(array $players): int
    {
        $min = PHP_INT_MAX;
        foreach ($players as $p) {
            $min = min($min, $p->getMovementRemaining());
        }

        return $min === PHP_INT_MAX ? 0 : max(0, $min);
    }

    /** Nosic vlastniho tymu, nebo `null`. */
    private function ownCarrierPosition(GameState $state, TeamSide $side): ?Position
    {
        $ball = $state->getBall();
        if (!$ball->isHeld() || $ball->getCarrierId() === null) {
            return null;
        }
        $carrier = $state->getPlayer($ball->getCarrierId());
        if ($carrier === null || $carrier->getTeamSide() !== $side) {
            return null;
        }

        return $carrier->getPosition();
    }

    public function decideAction(GameState $state, RulesEngine $rules): array
    {
        $side = $state->getActiveTeam();
        $actions = $rules->getAvailableActions($state);

        // Cache base state evaluation (used by many scoring functions)
        $baseScore = $this->evaluateState($state, $side);

        // Deduplicate action types per player to avoid redundant work
        $seenActions = [];

        // Build scored candidate actions
        /** @var list<array{action: ActionType, params: array<string, mixed>, score: float}> */
        $candidates = [];
        foreach ($actions as $action) {
            $type = ActionType::from($action['type']);
            $playerId = $action['playerId'] ?? null;

            if ($type === ActionType::SETUP_PLAYER || $type === ActionType::END_SETUP) {
                continue;
            }
            // ⏸ PHP25: schopnost "nic nedelat" uz v enginu JE, ale kouc ji
            //   zatim NEPOUZIVA -- kdy ji ma volit, je vlastni rozhodnuti
            //   (jinak by se tise vratila vada, kterou PHP13/PHP24 zaviraly).
            if ($type === ActionType::STAND_PAT) {
                // ⭐ PHP25 ODBLOKOVAN 12.09. (uzivatel: "odblokuj PHP25") --
                //   a prvni pouziti je prave klec: hrac, ktery UZ STOJI
                //   v rohu klece, nema kam chodit. Do ted se musel hnout,
                //   protoze END_TURN smi az kdyz nabidka nic nema (PHP13/24),
                //   a tim se klec kazde kolo rozsypala.
                // ⛔ Zadna jina situace sem zatim nepatri; "kdy jeste nechat
                //   hrace stat" je porad otevrena otazka (PHP20/PHP25).
                $carrierPos = $this->ownCarrierPosition($state, $side);
                $mujPos = $playerId !== null ? ($state->getPlayer((int) $playerId)?->getPosition()) : null;
                if ($carrierPos !== null && $mujPos !== null
                    && self::isCageCorner($carrierPos, $mujPos)) {
                    $candidates[] = [
                        'action' => ActionType::STAND_PAT,
                        'params' => ['playerId' => (int) $playerId],
                        // Zustat stat nestoji ani jeden hod => patri mezi bezrizikove.
                        'score' => $baseScore + self::CAGE_HOLD_BONUS + self::RISK_FREE_BONUS,
                    ];
                }
                continue;
            }


            // ⛔⛔⛔ OPRAVA 11.09.2026 -- polozka PHP24 fronty, MEKCI TVAR
            //   tehoz, co `PHP13` opravil u `GreedyAICoach` (`f44270f5`).
            //   END_TURN tu byl SKOROVANY KANDIDAT se skore `baseScore - 0.01`.
            //   Nemuselo tedy selhat nic: stacilo, aby vsichni POSTAVENI
            //   kandidati spadli pod tu hranici, a END_TURN VYHRAL SKOREM,
            //   i kdyz bylo co hrat. `buildMoveAction` pritom zaporne skore
            //   vraci snadno -- odecita za GFI (`gfis * 0.08`), za dodge
            //   a za postranni caru.
            //
            // ⭐ ZMERENO PRED OPRAVOU (`cli/diag_ai_endturn_20260911.php`):
            //   23 ze 7 666 rozhodnuti = **1,2 % KOL**, prumer **8,09 hrace**
            //   propadlo. ⚠️ A je to kouc, proti kteremu hraje CLOVEK
            //   (`ServiceProvider.php:121-127`).
            //
            // ⭐ PRAVIDLOVA KOTVA (`rules_bb2016.txt` r. 363-367 + uzavreny
            //   katalog turnoveru r. 368-384): "kandidati meli nizke skore"
            //   v tom katalogu NENI.
            //
            // ⇒ Oprava je TATAZ jako v C++ (`0630f854`), v `RandomAICoach`
            //   (`b0d01ccb`) a v `GreedyAICoach` (`f44270f5`): END_TURN az
            //   tehdy, kdyz nabidka nic jineho nema. Zadne nove skore se
            //   nevymysli -- END_TURN se jen prestane ucastnit souteze.
            //
            // ⚠️ CO SE TIM ZTRACI: kouc uz nemuze kolo ukoncit DOBROVOLNE
            //   (drive to umel prave pres tenhle kandidat). Zadny z ostatnich
            //   tri koucu to neumi taky -- parita je tim uplna, ale je to
            //   zmena chovani, ne jen odstraneni vady.
            if ($type === ActionType::END_TURN) {
                continue;
            }

            if ($playerId === null) {
                continue;
            }

            // Deduplicate: only evaluate one action per type+player
            $key = $type->value . '_' . $playerId;
            if (isset($seenActions[$key])) {
                continue;
            }
            $seenActions[$key] = true;

            $built = $this->buildScoredAction($state, $rules, $type, (int) $playerId, $side, $baseScore);
            if ($built !== null) {
                $candidates[] = $built;
            }
        }

        // ⭐ END_TURN az tady: kdyz nabidka opravdu nic hratelneho nemela.
        if ($candidates === []) {
            return ['action' => ActionType::END_TURN, 'params' => []];
        }

        // Epsilon-greedy: with probability epsilon, pick random
        if ($this->epsilon > 0 && (mt_rand() / mt_getrandmax()) < $this->epsilon) {
            $pick = $candidates[array_rand($candidates)];
            return ['action' => $pick['action'], 'params' => $pick['params']];
        }

        // Pick the highest-scored candidate
        $bestScore = -PHP_FLOAT_MAX;
        $bestAction = $candidates[0];
        foreach ($candidates as $candidate) {
            if ($candidate['score'] > $bestScore) {
                $bestScore = $candidate['score'];
                $bestAction = $candidate;
            }
        }

        return ['action' => $bestAction['action'], 'params' => $bestAction['params']];
    }

    public function setupFormation(GameState $state, TeamSide $side): GameState
    {
        $offPitchPlayers = [];
        foreach ($state->getTeamPlayers($side) as $player) {
            if ($player->getState() === PlayerState::OFF_PITCH) {
                $offPitchPlayers[] = $player;
            }
        }

        if ($side === TeamSide::HOME) {
            $positions = [
                new Position(12, 6), new Position(12, 7), new Position(12, 8),
                new Position(8, 4), new Position(8, 6), new Position(8, 8), new Position(8, 10),
                // ⭐ ČTYŘI VZADU JSOU POSÁDKA KLECE (uživatel 12.09.: "na to máš
                //   mít při rozestavení 4 další hráče vzadu"). Dřív stáli
                //   rozházení po šířce (y 3, 5, 9, 11) a k nosiči uprostřed
                //   se sbíhali půl kola. Teď stojí u středu, aby klec vznikla
                //   hned, jakmile nosič zvedne míč a vyrazí.
                new Position(4, 5), new Position(4, 6), new Position(4, 8), new Position(4, 9),
            ];
        } else {
            $positions = [
                new Position(13, 6), new Position(13, 7), new Position(13, 8),
                new Position(17, 4), new Position(17, 6), new Position(17, 8), new Position(17, 10),
                // ⭐ Totéž pro AWAY -- čtyři vzadu u středu jako posádka klece.
                new Position(21, 5), new Position(21, 6), new Position(21, 8), new Position(21, 9),
            ];
        }

        $count = min(count($offPitchPlayers), count($positions));
        for ($i = 0; $i < $count; $i++) {
            $state = $state->withPlayer(
                $offPitchPlayers[$i]
                    ->withPosition($positions[$i])
                    ->withState(PlayerState::STANDING),
            );
        }

        return $state;
    }

    /**
     * Score a state using the loaded model (linear dot product or neural network).
     */
    public function evaluateState(GameState $state, TeamSide $perspective): float
    {
        $features = FeatureExtractor::extract($state, $perspective);
        if ($this->modelType === 'neural') {
            return $this->evaluateNeural($features);
        }
        return self::dotProduct($this->weights, $features);
    }

    /**
     * Neural network inference: h = ReLU(features @ W1 + b1), output = tanh(h @ W2 + b2).
     *
     * @param list<float> $features
     */
    public function evaluateNeural(array $features): float
    {
        $nFeatures = count($this->W1);
        $hiddenSize = count($this->b1);

        // h = features @ W1 + b1 (then ReLU)
        $h = [];
        for ($j = 0; $j < $hiddenSize; $j++) {
            $sum = $this->b1[$j];
            $len = min(count($features), $nFeatures);
            for ($i = 0; $i < $len; $i++) {
                $sum += $features[$i] * $this->W1[$i][$j];
            }
            // ReLU
            $h[$j] = max(0.0, $sum);
        }

        // output = tanh(h @ W2 + b2)
        $out = $this->b2[0];
        for ($j = 0; $j < $hiddenSize; $j++) {
            $out += $h[$j] * $this->W2[$j][0];
        }

        return tanh($out);
    }

    public function getModelType(): string
    {
        return $this->modelType;
    }

    /**
     * @return list<float>
     */
    public function getWeights(): array
    {
        return $this->weights;
    }

    /**
     * @param list<float> $weights
     */
    public function setWeights(array $weights): void
    {
        // ⭐ Druha cesta, kterou se vahy dostanou dovnitr -- musi projit tymz
        //   srovnanim delky jako nacteni ze souboru, jinak by `dotProduct`
        //   zase tise usekaval (viz `1b26717a`).
        $weights = self::normalizeWeights(array_values(array_map('floatval', $weights)));
        $this->modelType = 'linear';
        $this->weights = $weights;
    }

    /**
     * Set neural network weights directly.
     *
     * @param list<list<float>> $W1
     * @param list<float> $b1
     * @param list<list<float>> $W2
     * @param list<float> $b2
     */
    public function setNeuralWeights(array $W1, array $b1, array $W2, array $b2): void
    {
        $this->modelType = 'neural';
        $this->W1 = $W1;
        $this->b1 = $b1;
        $this->W2 = $W2;
        $this->b2 = $b2;
    }

    /**
     * Build a scored action: picks the best target and evaluates resulting hypothetical state.
     *
     * @return array{action: ActionType, params: array<string, mixed>, score: float}|null
     */
    private function buildScoredAction(
        GameState $state,
        RulesEngine $rules,
        ActionType $type,
        int $playerId,
        TeamSide $side,
        float $baseScore,
    ): ?array {
        return match ($type) {
            ActionType::MOVE => $this->buildMoveAction($state, $rules, $playerId, $side),
            ActionType::BLOCK => $this->buildBlockAction($state, $rules, $playerId, $side, $baseScore),
            ActionType::BLITZ => $this->buildBlitzAction($state, $playerId, $side, $baseScore),
            ActionType::PASS => $this->buildPassAction($state, $rules, $playerId, $side, $baseScore),
            ActionType::HAND_OFF => $this->buildHandOffAction($state, $rules, $playerId, $side, $baseScore),
            ActionType::FOUL => $this->buildFoulAction($state, $rules, $playerId, $side, $baseScore),
            ActionType::BALL_AND_CHAIN => $this->buildBallAndChainAction($baseScore, $playerId),
            ActionType::HYPNOTIC_GAZE => $this->buildHypnoticGazeAction($state, $rules, $playerId, $side, $baseScore),
            ActionType::BOMB_THROW => $this->buildBombThrowAction($state, $rules, $playerId, $side, $baseScore),
            ActionType::MULTIPLE_BLOCK => $this->buildMultipleBlockAction($state, $rules, $playerId, $side, $baseScore),
            default => null,
        };
    }

    /**
     * @return array{action: ActionType, params: array<string, mixed>, score: float}|null
     */
    private function buildMoveAction(GameState $state, RulesEngine $rules, int $playerId, TeamSide $side): ?array
    {
        $targets = $rules->getValidMoveTargets($state, $playerId);
        if ($targets === []) {
            return null;
        }

        $player = $state->getPlayer($playerId);
        if ($player === null) {
            return null;
        }

        // Quick heuristic scoring without full feature extraction per target
        // Use the endzone direction and ball-related logic
        $ball = $state->getBall();
        $isCarrier = $ball->isHeld() && $ball->getCarrierId() === $playerId;
        $endZoneX = $side === TeamSide::HOME ? 25 : 0;
        $currentPos = $player->getPosition();

        $bestScore = -PHP_FLOAT_MAX;
        $bestTarget = $targets[0];

        foreach ($targets as $target) {
            $score = 0.0;
            $riskPenalty = $target['dodges'] * 0.15 + $target['gfis'] * 0.08;

            // Touchdown: carrier reaching endzone
            if ($isCarrier) {
                $pos = new Position($target['x'], $target['y']);
                if ($pos->isInEndZone($side !== TeamSide::HOME)) {
                    $tdScore = $this->shouldStall($state, $side) ? 1.0 : 10.0;
                    $score = $tdScore - $riskPenalty;
                    if ($score > $bestScore) {
                        $bestScore = $score;
                        $bestTarget = $target;
                    }
                    continue;
                }
            }

            // Pick up ball
            if (!$ball->isHeld() && $ball->isOnPitch()) {
                $ballPos = $ball->getPosition();
                if ($ballPos !== null && $target['x'] === $ballPos->getX() && $target['y'] === $ballPos->getY()) {
                    $score = 5.0 - $riskPenalty;
                    if ($score > $bestScore) {
                        $bestScore = $score;
                        $bestTarget = $target;
                    }
                    continue;
                }
            }

            // Forward advancement
            $distToEndZone = abs($target['x'] - $endZoneX);
            if ($currentPos !== null) {
                $currentDist = abs($currentPos->getX() - $endZoneX);
                $advancement = $currentDist - $distToEndZone;
                $score = $advancement * 0.1 - $riskPenalty;
            }

            // Defensive positioning: penalize moving to sideline
            $targetY = $target['y'];
            if ($targetY === 0 || $targetY === 14) {
                $score -= 0.2;
            } elseif ($targetY === 1 || $targetY === 13) {
                $score -= 0.05;
            }

            // Carrier movement strategy
            if ($isCarrier) {
                if ($this->shouldStall($state, $side)) {
                    // When stalling: prefer 2-5 squares from endzone, central Y positions
                    if ($distToEndZone >= 2 && $distToEndZone <= 5) {
                        $score += 1.5;
                    }
                    // Centrality bonus (Y=7 is center of 0-14 pitch)
                    $centralityScore = 1.0 - abs($target['y'] - 7) / 7.0;
                    $score += $centralityScore * 0.3;
                } else {
                    // Normal: move closer to endzone
                    $score += (26 - $distToEndZone) * 0.05;
                }

                // ⛔⛔ PHP33 (B) -- POHYB CELE KLECE, NE UTEK NOSICE.
                //   Uzivatel 12.09.: "pohyb cele klece je jako mala cast pred
                //   celotahem." Kouc rozhoduje HRAC PO HRACI a nema kde drzet
                //   zamer na cele kolo, takze plan formace se sem napsat neda.
                //   Da se ale zaridit to podstatne: NOSIC NESMI KLECI UTECT.
                //
                //   Rohy klece jsou vzdy diagonala od nosice, takze kdyz se
                //   nosic posune o JEDNO pole, kazdy roh ho dozene taky jednim
                //   krokem a tvar zustane. Pri dvou a vice polich uz ne --
                //   klec zustane vzadu a nosic stoji sam.
                //
                //   ⇒ Pokuta roste se vzdalenosti, ale POUZE kdyz klec vubec
                //   existuje (aspon dva rohy). Bez klece se nosic pohybuje
                //   jako driv.
                // ⛔⛔ NOSIC NESMI SKONCIT VEDLE SOUPERE (uzivatel 12.09.).
                //   Pocitaji se jen STOJICI souperi -- lezici zonu zachyceni
                //   nemaji, takze vedle nich je to bezpecne.
                $cil = new Position($target['x'], $target['y']);
                foreach ($state->getPlayersOnPitch($side->opponent()) as $nepritel) {
                    if ($nepritel->getState() !== PlayerState::STANDING) {
                        continue;
                    }
                    $np = $nepritel->getPosition();
                    if ($np !== null && $np->distanceTo($cil) === 1) {
                        $score -= self::CARRIER_NEXT_TO_ENEMY_PENALTY;
                        break;
                    }
                }

                if ($currentPos !== null) {
                    $rohovi = $this->cornerPlayers($state, $side, $currentPos, $playerId);
                    if (count($rohovi) >= self::CAGE_MIN_CORNERS) {
                        // ⭐ UPRESNENO UZIVATELEM 12.09.: "max pohyb klece podle
                        //   NEJMENSIHO MA ze vsech peti." Klec se posouva cela,
                        //   takze dal, nez ujde nejpomalejsi z ni, jit nemuze --
                        //   jinak nekdo zustane vzadu a roh zustane prazdny.
                        //   Do ted tu byl natvrdo jeden krok, coz nejpomalejsiho
                        //   hrace ignorovalo a klec zbytecne brzdilo.
                        $limit = $this->slowestRemaining($rohovi);
                        $krok = max(abs($target['x'] - $currentPos->getX()),
                                    abs($target['y'] - $currentPos->getY()));
                        if ($krok > $limit + self::CAGE_GFI_REACH) {
                            // Tam uz nejpomalejsi nedojde ani na GFI.
                            $score -= ($krok - $limit) * self::CAGE_OUTRUN_PENALTY;
                        } elseif ($krok > $limit) {
                            // Dojde, ale pres GFI => riziko, ne zakaz.
                            $score -= ($krok - $limit) * self::CAGE_GFI_PENALTY;
                        }
                    }
                }
            }

            // ⭐ KLEC (PHP33) -- ROH ANO, HRANA SKORO NE.
            //   Do 12.09.2026 tu stalo `distanceTo($pos) === 1`, tedy +1.0 za
            //   JAKEKOLI sousedni pole. Z toho vznika HVEZDA kolem nosice,
            //   ne klec: ortogonalni soused nosice nechrani -- souper na nej
            //   dosahne stejne, a jeste tim pole zabere sam sobe.
            //   Uzivatel 12.09.: klec = nosic + ctyri DIAGONALNI rohy.
            if (!$isCarrier && $ball->isHeld() && $ball->getCarrierId() !== null) {
                $carrier = $state->getPlayer($ball->getCarrierId());
                if ($carrier !== null && $carrier->getTeamSide() === $side && $carrier->getPosition() !== null) {
                    $carrierPos = $carrier->getPosition();
                    $pos = new Position($target['x'], $target['y']);
                    if (self::isCageCorner($carrierPos, $pos)) {
                        $score += self::CAGE_CORNER_BONUS;
                    } elseif ($carrierPos->distanceTo($pos) === 1) {
                        // Hrana je porad lepsi nez nic (telo mezi soupere
                        // a nosice), ale nesmi konkurovat rohu.
                        $score += self::CAGE_EDGE_BONUS;
                    }
                }
            }

            // ⭐ PORADI AKTIVACI -- NEJDRIV BEZ RIZIKA (uzivatel 12.09.).
            //   Turnover na konci kola stoji min nez na zacatku: co uz je
            //   odehrane, to se neztrati. Bezpecne tahy tedy patri DOPREDU.
            //   Pocita se cil BEZ jedineho hodu -- ani dodge, ani GFI.
            if ($target['dodges'] === 0 && $target['gfis'] === 0) {
                $score += self::RISK_FREE_BONUS;
            }

            if ($score > $bestScore) {
                $bestScore = $score;
                $bestTarget = $target;
            }
        }

        // Add base state evaluation to the score
        $baseScore = $this->evaluateState($state, $side);

        return [
            'action' => ActionType::MOVE,
            'params' => ['playerId' => $playerId, 'x' => $bestTarget['x'], 'y' => $bestTarget['y']],
            'score' => $baseScore + $bestScore,
        ];
    }

    /**
     * @return array{action: ActionType, params: array<string, mixed>, score: float}|null
     */
    private function buildBlockAction(GameState $state, RulesEngine $rules, int $playerId, TeamSide $side, float $baseScore): ?array
    {
        $player = $state->getPlayer($playerId);
        if ($player === null) {
            return null;
        }

        $targets = $rules->getBlockTargets($state, $player);
        if ($targets === []) {
            return null;
        }

        $bestScore = -PHP_FLOAT_MAX;
        $bestTarget = $targets[0];
        $ball = $state->getBall();

        foreach ($targets as $target) {
            $score = $baseScore + 0.05;
            if ($ball->isHeld() && $ball->getCarrierId() === $target->getId()) {
                $score += 0.5;
            }

            // Sideline surfing bonus
            $targetPos = $target->getPosition();
            if ($targetPos !== null) {
                $ty = $targetPos->getY();
                if ($ty === 0 || $ty === 14) {
                    $score += 0.3;
                } elseif ($ty === 1 || $ty === 13) {
                    $score += 0.1;
                }
            }

            if ($score > $bestScore) {
                $bestScore = $score;
                $bestTarget = $target;
            }
        }

        return [
            'action' => ActionType::BLOCK,
            'params' => ['playerId' => $playerId, 'targetId' => $bestTarget->getId()],
            'score' => $bestScore,
        ];
    }

    /**
     * @return array{action: ActionType, params: array<string, mixed>, score: float}|null
     */
    private function buildBlitzAction(GameState $state, int $playerId, TeamSide $side, float $baseScore): ?array
    {
        $player = $state->getPlayer($playerId);
        if ($player === null) {
            return null;
        }

        $enemies = $state->getPlayersOnPitch($side->opponent());
        if ($enemies === []) {
            return null;
        }

        $ball = $state->getBall();
        $bestScore = -PHP_FLOAT_MAX;
        $bestTarget = $enemies[0];

        foreach ($enemies as $enemy) {
            $score = $baseScore + 0.03;
            if ($ball->isHeld() && $ball->getCarrierId() === $enemy->getId()) {
                $score += 0.8;
            }

            // Sideline surfing bonus (higher for blitz — it's a scarce resource)
            $enemyPos = $enemy->getPosition();
            if ($enemyPos !== null) {
                $ey = $enemyPos->getY();
                if ($ey === 0 || $ey === 14) {
                    $score += 0.4;
                } elseif ($ey === 1 || $ey === 13) {
                    $score += 0.15;
                }
                // End zone edges
                $ex = $enemyPos->getX();
                if ($ex === 0 || $ex === 25) {
                    $score += 0.2;
                }
            }

            if ($score > $bestScore) {
                $bestScore = $score;
                $bestTarget = $enemy;
            }
        }

        return [
            'action' => ActionType::BLITZ,
            'params' => ['playerId' => $playerId, 'targetId' => $bestTarget->getId()],
            'score' => $bestScore,
        ];
    }

    /**
     * @return array{action: ActionType, params: array<string, mixed>, score: float}|null
     */
    private function buildPassAction(GameState $state, RulesEngine $rules, int $playerId, TeamSide $side, float $baseScore): ?array
    {
        $player = $state->getPlayer($playerId);
        if ($player === null) {
            return null;
        }

        $targets = $rules->getPassTargets($state, $player);
        if ($targets === []) {
            return null;
        }

        $bestScore = -PHP_FLOAT_MAX;
        $bestTarget = $targets[0];

        foreach ($targets as $target) {
            $rangePenalty = match ($target['range']) {
                'quick' => 0.0,
                'short' => 0.02,
                'long' => 0.05,
                'bomb' => 0.1,
                default => 0.03,
            };
            $score = $baseScore - $rangePenalty;

            if ($score > $bestScore) {
                $bestScore = $score;
                $bestTarget = $target;
            }
        }

        return [
            'action' => ActionType::PASS,
            'params' => ['playerId' => $playerId, 'targetX' => $bestTarget['x'], 'targetY' => $bestTarget['y']],
            'score' => $bestScore,
        ];
    }

    /**
     * @return array{action: ActionType, params: array<string, mixed>, score: float}|null
     */
    private function buildHandOffAction(GameState $state, RulesEngine $rules, int $playerId, TeamSide $side, float $baseScore): ?array
    {
        $player = $state->getPlayer($playerId);
        if ($player === null) {
            return null;
        }

        $targets = $rules->getHandOffTargets($state, $player);
        if ($targets === []) {
            return null;
        }

        $target = $targets[0];

        return [
            'action' => ActionType::HAND_OFF,
            'params' => ['playerId' => $playerId, 'targetId' => $target->getId()],
            'score' => $baseScore + 0.01,
        ];
    }

    /**
     * @return array{action: ActionType, params: array<string, mixed>, score: float}|null
     */
    private function buildFoulAction(GameState $state, RulesEngine $rules, int $playerId, TeamSide $side, float $baseScore): ?array
    {
        $player = $state->getPlayer($playerId);
        if ($player === null) {
            return null;
        }

        $targets = $rules->getFoulTargets($state, $player);
        if ($targets === []) {
            return null;
        }

        $bestTarget = $targets[0];
        $lowestArmour = $bestTarget->getStats()->getArmour();
        foreach ($targets as $target) {
            if ($target->getStats()->getArmour() < $lowestArmour) {
                $lowestArmour = $target->getStats()->getArmour();
                $bestTarget = $target;
            }
        }

        return [
            'action' => ActionType::FOUL,
            'params' => ['playerId' => $playerId, 'targetId' => $bestTarget->getId()],
            'score' => $baseScore - 0.02,
        ];
    }

    /**
     * Ball & Chain is the only action B&C players can take — must always be used.
     *
     * @return array{action: ActionType, params: array<string, mixed>, score: float}
     */
    private function buildBallAndChainAction(float $baseScore, int $playerId): array
    {
        // ⛔ OPRAVA 11.09.2026 -- viz `GreedyAICoach::scoreBallAndChain`.
        //   Chybejici `playerId` => `getPlayer(0)` => 'Player not found'.
        return [
            'action' => ActionType::BALL_AND_CHAIN,
            'params' => ['playerId' => $playerId],
            'score' => $baseScore + 0.1,
        ];
    }

    /**
     * @return array{action: ActionType, params: array<string, mixed>, score: float}|null
     */
    private function buildHypnoticGazeAction(GameState $state, RulesEngine $rules, int $playerId, TeamSide $side, float $baseScore): ?array
    {
        $player = $state->getPlayer($playerId);
        if ($player === null) {
            return null;
        }

        $targets = $rules->getHypnoticGazeTargets($state, $player);
        if ($targets === []) {
            return null;
        }

        $ball = $state->getBall();
        $bestScore = -PHP_FLOAT_MAX;
        $bestTarget = $targets[0];

        foreach ($targets as $target) {
            $score = $baseScore + 0.05;

            // High priority: gaze the ball carrier
            if ($ball->isHeld() && $ball->getCarrierId() === $target->getId()) {
                $score += 0.4;
            }

            // Bonus for gazing strong players
            if ($target->getStats()->getStrength() >= 4) {
                $score += 0.1;
            }

            if ($score > $bestScore) {
                $bestScore = $score;
                $bestTarget = $target;
            }
        }

        return [
            'action' => ActionType::HYPNOTIC_GAZE,
            'params' => ['playerId' => $playerId, 'targetId' => $bestTarget->getId()],
            'score' => $bestScore,
        ];
    }

    /**
     * @return array{action: ActionType, params: array<string, mixed>, score: float}|null
     */
    private function buildBombThrowAction(GameState $state, RulesEngine $rules, int $playerId, TeamSide $side, float $baseScore): ?array
    {
        $player = $state->getPlayer($playerId);
        if ($player === null || !$player->hasSkill(SkillName::Bombardier)) {
            return null;
        }

        $targets = $rules->getPassTargets($state, $player);
        if ($targets === []) {
            return null;
        }

        $enemies = $state->getPlayersOnPitch($side->opponent());
        $myPlayers = $state->getPlayersOnPitch($side);

        $bestScore = -PHP_FLOAT_MAX;
        $bestTarget = null;

        foreach ($targets as $target) {
            $tx = $target['x'];
            $ty = $target['y'];

            $enemyCount = 0;
            $allyCount = 0;

            foreach ($enemies as $enemy) {
                $ePos = $enemy->getPosition();
                if ($ePos !== null && abs($ePos->getX() - $tx) <= 1 && abs($ePos->getY() - $ty) <= 1) {
                    $enemyCount++;
                }
            }

            foreach ($myPlayers as $ally) {
                $aPos = $ally->getPosition();
                if ($aPos !== null && abs($aPos->getX() - $tx) <= 1 && abs($aPos->getY() - $ty) <= 1) {
                    $allyCount++;
                }
            }

            if ($enemyCount === 0) {
                continue;
            }

            $score = $baseScore + $enemyCount * 0.08 - $allyCount * 0.06;

            if ($score > $bestScore) {
                $bestScore = $score;
                $bestTarget = $target;
            }
        }

        if ($bestTarget === null) {
            return null;
        }

        return [
            'action' => ActionType::BOMB_THROW,
            'params' => ['playerId' => $playerId, 'targetX' => $bestTarget['x'], 'targetY' => $bestTarget['y']],
            'score' => $bestScore,
        ];
    }

    /**
     * @return array{action: ActionType, params: array<string, mixed>, score: float}|null
     */
    private function buildMultipleBlockAction(GameState $state, RulesEngine $rules, int $playerId, TeamSide $side, float $baseScore): ?array
    {
        $player = $state->getPlayer($playerId);
        if ($player === null) {
            return null;
        }

        $targets = $rules->getBlockTargets($state, $player);
        if (count($targets) < 2) {
            return null;
        }

        $ball = $state->getBall();
        $bestPairScore = -PHP_FLOAT_MAX;
        $bestT1 = $targets[0];
        $bestT2 = $targets[1];

        // Evaluate all pairs (small count so O(n^2) is fine)
        for ($i = 0; $i < count($targets); $i++) {
            for ($j = $i + 1; $j < count($targets); $j++) {
                $score = $baseScore + 0.03;

                // Ball carrier bonus
                if ($ball->isHeld()) {
                    if ($ball->getCarrierId() === $targets[$i]->getId()) {
                        $score += 0.4;
                    }
                    if ($ball->getCarrierId() === $targets[$j]->getId()) {
                        $score += 0.4;
                    }
                }

                if ($score > $bestPairScore) {
                    $bestPairScore = $score;
                    $bestT1 = $targets[$i];
                    $bestT2 = $targets[$j];
                }
            }
        }

        return [
            'action' => ActionType::MULTIPLE_BLOCK,
            'params' => [
                'playerId' => $playerId,
                'targetId' => $bestT1->getId(),
                'targetId2' => $bestT2->getId(),
            ],
            'score' => $bestPairScore,
        ];
    }

    /**
     * Should the AI stall (hold the ball) rather than score immediately?
     */
    private function shouldStall(GameState $state, TeamSide $side): bool
    {
        $myTeam = $state->getTeamState($side);
        $oppTeam = $state->getTeamState($side->opponent());
        $scoreDiff = $myTeam->getScore() - $oppTeam->getScore();
        $turnsLeft = max(0, 9 - $myTeam->getTurnNumber());

        // Behind: never stall
        if ($scoreDiff < 0) {
            return false;
        }

        // Last 2 turns: always score
        if ($turnsLeft <= 2) {
            return false;
        }

        // Tied with 3 or fewer turns: score
        if ($scoreDiff === 0 && $turnsLeft <= 3) {
            return false;
        }

        // Ahead or tied with plenty of time: stall
        return true;
    }

    /**
     * @param list<float> $a
     * @param list<float> $b
     */
    /**
     * Srovná vektor vah na `NUM_FEATURES`.
     *
     * ⭐ PROČ JE DOPLNĚNÍ NULAMI BEZPEČNÉ: tři příznaky, o které jde
     *   (`70-72`, loose-ball field position), byly PŘIDÁNY NA KONEC
     *   (`30539d65`, `NUM_FEATURES` 70 -> 73), ne vloženy doprostřed.
     *   Indexy 0-69 tedy pořád znamenají totéž, co když se váhy trénovaly.
     *   Doplněná nula = "tenhle příznak zatím nemá váhu", ne posun.
     *   ⛔ Kdyby se někdy příznak vložil DOPROSTŘED, tohle by přestalo platit
     *   a váhy by se musely přetrénovat -- doplnit nulami by je rozházelo.
     *
     * ⛔ Dřív to dělal `min()` v `dotProduct` potichu. Teď se to děje na
     *   jednom místě a je to otestované.
     *
     * @param list<float> $w
     * @return list<float>
     */
    private static function normalizeWeights(array $w): array
    {
        // Jeden vyraz pokryva kratsi, delsi i presnou delku.
        return array_slice(
            array_pad($w, FeatureExtractor::NUM_FEATURES, 0.0),
            0,
            FeatureExtractor::NUM_FEATURES,
        );
    }

    private static function dotProduct(array $a, array $b): float
    {
        $sum = 0.0;
        $len = min(count($a), count($b));
        for ($i = 0; $i < $len; $i++) {
            $sum += $a[$i] * $b[$i];
        }
        return $sum;
    }
}
