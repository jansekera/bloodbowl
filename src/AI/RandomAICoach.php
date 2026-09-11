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

final class RandomAICoach implements AICoachInterface
{
    public function decideAction(GameState $state, RulesEngine $rules): array
    {
        $actions = $rules->getAvailableActions($state);

        // ⛔⛔ OPRAVA 11.09.2026 -- polozka #2 auditu "kde engine ukoncuje tah".
        //   Puvodne se losovalo ze VSECH akci a teprve pak se typ prekladal
        //   pres `match`, ktery mel `default => END_TURN`. Kdyz los padl na
        //   TTM / bombu / gaze / Ball & Chain -- tedy na typ, ktery tenhle kouc
        //   neumi postavit -- UKONCIL SE CELY TAH TYMU. Totez delal `$playerId
        //   === 0` o par radku vys.
        //
        // ⭐ PRAVIDLOVA KOTVA (`rules_bb2016.txt` r. 363-367 + uzavreny
        //   sedmicленny seznam turnoveru r. 368-384): "kouc neumi zvoleny typ"
        //   v tom seznamu NENI. Ukoncit tah tady je vada, ne prisnost.
        //   Tataz trida jako `greedyPolicy` v C++ (`0630f854`) a `W7`.
        //
        // ⇒ Nepodporovany typ se z LOSOVANI VYRADI, misto aby ukoncil kolo.
        //   END_TURN zustava az kdyz nezbyde nic.
        $playableActions = [];
        foreach ($actions as $a) {
            $type = ActionType::tryFrom($a['type']);
            // ⏸ PHP25: STAND_PAT se zatim nelosuje -- viz komentar u Greedyho.
            if ($type === null || $type === ActionType::END_TURN || $type === ActionType::STAND_PAT) {
                continue;
            }
            // Akce bez hrace tenhle kouc postavit neumi -- jen vyradit.
            $playerId = (int) ($a['playerId'] ?? 0);
            if ($playerId === 0) {
                continue;
            }
            // ⭐ Enum se resi JEDNOU tady, ne znovu pri stavbe.
            $playableActions[] = [$type, $playerId];
        }

        // ⛔⛔ DRUHA POLOVINA TEHOZ (11.09.2026): vrchni `default => END_TURN`
        //   nebyl jediny vyskyt. KAZDY `build*Action` vracel END_TURN, kdyz
        //   nenasel cil (14 mist) -- `getValidMoveTargets` prazdne, zadny
        //   soused k bloku, zadny soupeř na desce, neni komu prihrat...
        //   Nabidka pritom takovou akci nabidnout MUZE (jeji podminka je
        //   hrubsi nez ta v builderu), takze se kolo ukoncovalo i tady.
        //   ⇒ Builder ted vraci `null` a losuje se DAL. END_TURN az kdyz
        //   zadny kandidat akci nepostavi.
        // ⭐ `shuffle` + `foreach`: kazdy kandidat se zkusi NEJVYS jednou a
        //   smycka je konecna uz z podstaty. (Drive `while` s `array_rand`
        //   a `unset`, tedy ucetnictvi s klici kvuli nahodnemu poradi.)
        shuffle($playableActions);
        foreach ($playableActions as [$type, $playerId]) {
            $built = $this->buildFor($state, $rules, $type, $playerId);
            if ($built !== null) {
                return $built;
            }
        }

        // Prazdna nabidka i "zadny kandidat nic nepostavil" konci stejne.
        return ['action' => ActionType::END_TURN, 'params' => []];
    }

    /**
     * @return array{action: ActionType, params: array<string, mixed>}|null
     */
    private function buildFor(GameState $state, RulesEngine $rules, ActionType $actionType, int $playerId): ?array
    {
        return match ($actionType) {
            ActionType::MOVE => $this->buildMoveAction($state, $rules, $playerId),
            ActionType::BLOCK => $this->buildBlockAction($state, $rules, $playerId),
            ActionType::BLITZ => $this->buildBlitzAction($state, $rules, $playerId),
            ActionType::PASS => $this->buildPassAction($state, $rules, $playerId),
            ActionType::HAND_OFF => $this->buildHandOffAction($state, $rules, $playerId),
            ActionType::FOUL => $this->buildFoulAction($state, $rules, $playerId),
            ActionType::MULTIPLE_BLOCK => $this->buildMultipleBlockAction($state, $rules, $playerId),
            // ⭐ Ball & Chain je pro takoveho hrace JEDINA povolena akce --
            //   vyradit ji z losovani by ho na desce umrtvilo. Parametr je
            //   jen `playerId` (viz `dd6b229c`, kde tenhle klic chybel obema
            //   ostatnim koucum a hrac kvuli tomu nejednal nikdy).
            ActionType::BALL_AND_CHAIN => ['action' => ActionType::BALL_AND_CHAIN,
                                           'params' => ['playerId' => $playerId]],
            // ⭐ Nepodporovany typ (TTM, bomba, gaze) vypada z losovani TOUTEZ
            //   cestou jako builder, ktery nenasel cil -- jeden mechanismus
            //   misto dvou. Drive to hlidal jeste druhy uzavreny seznam
            //   `canBuild()`, ktery se musel rucne drzet v souladu s timhle
            //   `match`; komentar si to sam priznaval. `LearningAICoach` to
            //   tak dela odjakziva.
            default => null,
        };
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
                new Position(4, 3), new Position(4, 5), new Position(4, 9), new Position(4, 11),
            ];
        } else {
            $positions = [
                new Position(13, 6), new Position(13, 7), new Position(13, 8),
                new Position(17, 4), new Position(17, 6), new Position(17, 8), new Position(17, 10),
                new Position(21, 3), new Position(21, 5), new Position(21, 9), new Position(21, 11),
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
     * @return array{action: ActionType, params: array<string, mixed>}|null
     */
    private function buildMoveAction(GameState $state, RulesEngine $rules, int $playerId): ?array
    {
        $targets = $rules->getValidMoveTargets($state, $playerId);
        if ($targets === []) {
            return null;   // nenaslo se -- losuje se dal, kolo se NEUKONCUJE
        }

        $target = $targets[array_rand($targets)];

        return [
            'action' => ActionType::MOVE,
            'params' => [
                'playerId' => $playerId,
                'x' => $target['x'],
                'y' => $target['y'],
            ],
        ];
    }

    /**
     * @return array{action: ActionType, params: array<string, mixed>}|null
     */
    private function buildBlockAction(GameState $state, RulesEngine $rules, int $playerId): ?array
    {
        $player = $state->getPlayer($playerId);
        if ($player === null) {
            return null;   // nenaslo se -- losuje se dal, kolo se NEUKONCUJE
        }

        $targets = $rules->getBlockTargets($state, $player);
        if ($targets === []) {
            return null;   // nenaslo se -- losuje se dal, kolo se NEUKONCUJE
        }

        $target = $targets[array_rand($targets)];

        return [
            'action' => ActionType::BLOCK,
            'params' => [
                'playerId' => $playerId,
                'targetId' => $target->getId(),
            ],
        ];
    }

    /**
     * @return array{action: ActionType, params: array<string, mixed>}|null
     */
    private function buildBlitzAction(GameState $state, RulesEngine $rules, int $playerId): ?array
    {
        $player = $state->getPlayer($playerId);
        if ($player === null) {
            return null;   // nenaslo se -- losuje se dal, kolo se NEUKONCUJE
        }

        // Find enemies on pitch to blitz
        $side = $player->getTeamSide();
        $enemies = $state->getPlayersOnPitch($side->opponent());
        if ($enemies === []) {
            return null;   // nenaslo se -- losuje se dal, kolo se NEUKONCUJE
        }

        $target = $enemies[array_rand($enemies)];

        return [
            'action' => ActionType::BLITZ,
            'params' => [
                'playerId' => $playerId,
                'targetId' => $target->getId(),
            ],
        ];
    }

    /**
     * @return array{action: ActionType, params: array<string, mixed>}|null
     */
    private function buildPassAction(GameState $state, RulesEngine $rules, int $playerId): ?array
    {
        $player = $state->getPlayer($playerId);
        if ($player === null) {
            return null;   // nenaslo se -- losuje se dal, kolo se NEUKONCUJE
        }

        $targets = $rules->getPassTargets($state, $player);
        if ($targets === []) {
            return null;   // nenaslo se -- losuje se dal, kolo se NEUKONCUJE
        }

        $target = $targets[array_rand($targets)];

        return [
            'action' => ActionType::PASS,
            'params' => [
                'playerId' => $playerId,
                'targetX' => $target['x'],
                'targetY' => $target['y'],
            ],
        ];
    }

    /**
     * @return array{action: ActionType, params: array<string, mixed>}|null
     */
    private function buildHandOffAction(GameState $state, RulesEngine $rules, int $playerId): ?array
    {
        $player = $state->getPlayer($playerId);
        if ($player === null) {
            return null;   // nenaslo se -- losuje se dal, kolo se NEUKONCUJE
        }

        $targets = $rules->getHandOffTargets($state, $player);
        if ($targets === []) {
            return null;   // nenaslo se -- losuje se dal, kolo se NEUKONCUJE
        }

        $target = $targets[array_rand($targets)];

        return [
            'action' => ActionType::HAND_OFF,
            'params' => [
                'playerId' => $playerId,
                'targetId' => $target->getId(),
            ],
        ];
    }

    /**
     * @return array{action: ActionType, params: array<string, mixed>}|null
     */
    private function buildMultipleBlockAction(GameState $state, RulesEngine $rules, int $playerId): ?array
    {
        $player = $state->getPlayer($playerId);
        if ($player === null) {
            return null;   // nenaslo se -- losuje se dal, kolo se NEUKONCUJE
        }

        $targets = $rules->getBlockTargets($state, $player);
        if (count($targets) < 2) {
            return null;   // nenaslo se -- losuje se dal, kolo se NEUKONCUJE
        }

        // Pick 2 random targets
        $keys = array_rand($targets, 2);
        return [
            'action' => ActionType::MULTIPLE_BLOCK,
            'params' => [
                'playerId' => $playerId,
                'targetId' => $targets[$keys[0]]->getId(),
                'targetId2' => $targets[$keys[1]]->getId(),
            ],
        ];
    }

    /**
     * @return array{action: ActionType, params: array<string, mixed>}|null
     */
    private function buildFoulAction(GameState $state, RulesEngine $rules, int $playerId): ?array
    {
        $player = $state->getPlayer($playerId);
        if ($player === null) {
            return null;   // nenaslo se -- losuje se dal, kolo se NEUKONCUJE
        }

        $targets = $rules->getFoulTargets($state, $player);
        if ($targets === []) {
            return null;   // nenaslo se -- losuje se dal, kolo se NEUKONCUJE
        }

        $target = $targets[array_rand($targets)];

        return [
            'action' => ActionType::FOUL,
            'params' => [
                'playerId' => $playerId,
                'targetId' => $target->getId(),
            ],
        ];
    }
}
