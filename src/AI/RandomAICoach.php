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

        // Filter out END_TURN - we'll use it as fallback
        $playableActions = array_filter(
            $actions,
            fn(array $a) => $a['type'] !== ActionType::END_TURN->value,
        );

        if ($playableActions === []) {
            return ['action' => ActionType::END_TURN, 'params' => []];
        }

        $chosen = $playableActions[array_rand($playableActions)];
        $actionType = ActionType::from($chosen['type']);
        $playerId = (int) ($chosen['playerId'] ?? 0);

        if ($playerId === 0) {
            return ['action' => ActionType::END_TURN, 'params' => []];
        }

        return match ($actionType) {
            ActionType::MOVE => $this->buildMoveAction($state, $rules, $playerId),
            ActionType::BLOCK => $this->buildBlockAction($state, $rules, $playerId),
            ActionType::BLITZ => $this->buildBlitzAction($state, $rules, $playerId),
            ActionType::PASS => $this->buildPassAction($state, $rules, $playerId),
            ActionType::HAND_OFF => $this->buildHandOffAction($state, $rules, $playerId),
            ActionType::FOUL => $this->buildFoulAction($state, $rules, $playerId),
            ActionType::MULTIPLE_BLOCK => $this->buildMultipleBlockAction($state, $rules, $playerId),
            default => ['action' => ActionType::END_TURN, 'params' => []],
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
     * @return array{action: ActionType, params: array<string, mixed>}
     */
    private function buildMoveAction(GameState $state, RulesEngine $rules, int $playerId): array
    {
        $targets = $rules->getValidMoveTargets($state, $playerId);
        if ($targets === []) {
            return ['action' => ActionType::END_TURN, 'params' => []];
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
     * @return array{action: ActionType, params: array<string, mixed>}
     */
    private function buildBlockAction(GameState $state, RulesEngine $rules, int $playerId): array
    {
        $player = $state->getPlayer($playerId);
        if ($player === null) {
            return ['action' => ActionType::END_TURN, 'params' => []];
        }

        $targets = $rules->getBlockTargets($state, $player);
        if ($targets === []) {
            return ['action' => ActionType::END_TURN, 'params' => []];
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
     * @return array{action: ActionType, params: array<string, mixed>}
     */
    private function buildBlitzAction(GameState $state, RulesEngine $rules, int $playerId): array
    {
        $player = $state->getPlayer($playerId);
        if ($player === null) {
            return ['action' => ActionType::END_TURN, 'params' => []];
        }

        // Find enemies on pitch to blitz
        $side = $player->getTeamSide();
        $enemies = $state->getPlayersOnPitch($side->opponent());
        if ($enemies === []) {
            return ['action' => ActionType::END_TURN, 'params' => []];
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
     * @return array{action: ActionType, params: array<string, mixed>}
     */
    private function buildPassAction(GameState $state, RulesEngine $rules, int $playerId): array
    {
        $player = $state->getPlayer($playerId);
        if ($player === null) {
            return ['action' => ActionType::END_TURN, 'params' => []];
        }

        $targets = $rules->getPassTargets($state, $player);
        if ($targets === []) {
            return ['action' => ActionType::END_TURN, 'params' => []];
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
     * @return array{action: ActionType, params: array<string, mixed>}
     */
    private function buildHandOffAction(GameState $state, RulesEngine $rules, int $playerId): array
    {
        $player = $state->getPlayer($playerId);
        if ($player === null) {
            return ['action' => ActionType::END_TURN, 'params' => []];
        }

        $targets = $rules->getHandOffTargets($state, $player);
        if ($targets === []) {
            return ['action' => ActionType::END_TURN, 'params' => []];
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
     * @return array{action: ActionType, params: array<string, mixed>}
     */
    private function buildMultipleBlockAction(GameState $state, RulesEngine $rules, int $playerId): array
    {
        $player = $state->getPlayer($playerId);
        if ($player === null) {
            return ['action' => ActionType::END_TURN, 'params' => []];
        }

        $targets = $rules->getBlockTargets($state, $player);
        if (count($targets) < 2) {
            return ['action' => ActionType::END_TURN, 'params' => []];
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
     * @return array{action: ActionType, params: array<string, mixed>}
     */
    private function buildFoulAction(GameState $state, RulesEngine $rules, int $playerId): array
    {
        $player = $state->getPlayer($playerId);
        if ($player === null) {
            return ['action' => ActionType::END_TURN, 'params' => []];
        }

        $targets = $rules->getFoulTargets($state, $player);
        if ($targets === []) {
            return ['action' => ActionType::END_TURN, 'params' => []];
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
