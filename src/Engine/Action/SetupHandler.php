<?php

declare(strict_types=1);

namespace App\Engine\Action;

use App\DTO\ActionResult;
use App\DTO\GameState;
use App\Enum\GamePhase;
use App\Enum\PlayerState;
use App\Enum\TeamSide;
use App\ValueObject\Position;
use App\Engine\KickoffResolver;

final class SetupHandler implements ActionHandlerInterface
{
    public function __construct(
        private readonly KickoffResolver $kickoffResolver,
    ) {
    }

    /**
     * Resolve setup player action.
     *
     * @param array<string, mixed> $params
     */
    public function resolve(GameState $state, array $params): ActionResult
    {
        $playerId = (int) $params['playerId'];
        $x = (int) $params['x'];
        $y = (int) $params['y'];
        $position = new Position($x, $y);

        $player = $state->getPlayer($playerId);
        if ($player === null) {
            throw new \InvalidArgumentException("Player {$playerId} not found");
        }

        // Validate position is on the team's side
        $side = $player->getTeamSide();
        if ($side === TeamSide::HOME && $x > 12) {
            throw new \InvalidArgumentException('Home team must set up on left half');
        }
        if ($side === TeamSide::AWAY && $x < 13) {
            throw new \InvalidArgumentException('Away team must set up on right half');
        }

        // Check position not occupied by another player
        $occupant = $state->getPlayerAtPosition($position);
        if ($occupant !== null && $occupant->getId() !== $playerId) {
            throw new \InvalidArgumentException('Position already occupied');
        }

        $placed = $player->withPosition($position);
        if ($placed->getState() === PlayerState::OFF_PITCH) {
            $placed = $placed->withState(PlayerState::STANDING);
        }
        $newState = $state->withPlayer($placed);

        return ActionResult::success($newState, []);
    }

    public function resolveEndSetup(GameState $state): ActionResult
    {
        $side = $state->getActiveTeam();
        $playersOnPitch = $state->getPlayersOnPitch($side);

        // Count total available players (off-pitch + on-pitch)
        $availablePlayers = 0;
        foreach ($state->getTeamPlayers($side) as $p) {
            if ($p->getState() === PlayerState::OFF_PITCH || $p->getState()->isOnPitch()) {
                $availablePlayers++;
            }
        }
        $required = min(11, $availablePlayers);

        if (count($playersOnPitch) < $required) {
            throw new \InvalidArgumentException(
                "Need at least {$required} players on pitch (have " . count($playersOnPitch) . ')'
            );
        }

        // Check line of scrimmage - need at least 3 (or all available if < 3)
        $losCount = 0;
        $losX = $side === TeamSide::HOME ? 12 : 13;
        foreach ($playersOnPitch as $player) {
            $pos = $player->getPosition();
            if ($pos !== null && $pos->getX() === $losX) {
                $losCount++;
            }
        }

        $losRequired = min(3, count($playersOnPitch));
        if ($losCount < $losRequired) {
            throw new \InvalidArgumentException(
                "Need at least {$losRequired} players on Line of Scrimmage (have {$losCount})"
            );
        }

        // Auto-setup opponent if they have no players on pitch yet
        $opponent = $side->opponent();
        $opponentOnPitch = $state->getPlayersOnPitch($opponent);

        if (count($opponentOnPitch) < 11) {
            $state = $this->autoSetupTeam($state, $opponent);
        }

        // Both teams set up - proceed to kickoff
        $kickingTeam = $state->getKickingTeam() ?? TeamSide::AWAY;
        $receivingTeam = $kickingTeam->opponent();

        $kickTarget = $this->kickoffResolver->getDefaultKickTarget($receivingTeam);
        $kickoffResult = $this->kickoffResolver->resolveKickoff($state, $kickTarget);

        $newState = $kickoffResult['state']
            ->withPhase(GamePhase::PLAY)
            ->withActiveTeam($receivingTeam);

        return ActionResult::success($newState, $kickoffResult['events']);
    }

    private function autoSetupTeam(GameState $state, TeamSide $side): GameState
    {
        $offPitchPlayers = [];
        foreach ($state->getTeamPlayers($side) as $player) {
            if ($player->getState() === PlayerState::OFF_PITCH) {
                $offPitchPlayers[] = $player;
            }
        }

        // Default formation: 3 on LoS, 4 midfield, 4 backfield
        // Wide zone limits respected (max 2 per wide zone)
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
}
