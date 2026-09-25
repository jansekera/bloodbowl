<?php

declare(strict_types=1);

namespace App\Engine\Action;

use App\DTO\ActionResult;
use App\DTO\GameState;
use App\Enum\GamePhase;
use App\Engine\RulesEngine;
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

        // ⭐ BALIK G 24.09.2026 -- Sweltering Heat (r. 1477-1481): kdo zkolaboval
        //   na konci minuleho drivu, NESMI byt rozestaven pri tomhle vykopu.
        if ($player->isOutNextSetup()) {
            throw new \InvalidArgumentException(
                'Player collapsed from Sweltering Heat and may not be set up for this kick-off'
            );
        }

        // ⭐ BALIK G 24.09.2026 -- strop 11 na hristi (r. 308-309); zbytek
        //   soupisky (az 16) ceka v rezervach. Prestaveni hrace, ktery uz
        //   na hristi stoji, pocet nezvysuje, takze se na nej strop nevztahuje.
        if (!$player->getState()->isOnPitch()
            && count($state->getPlayersOnPitch($side)) >= RulesEngine::MAX_PLAYERS_ON_PITCH) {
            throw new \InvalidArgumentException(
                'Cannot set up more than ' . RulesEngine::MAX_PLAYERS_ON_PITCH
                . ' players on the pitch (rest stay in Reserves)'
            );
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
            // ⭐ Zkolabovany z Sweltering Heat se nepocita mezi dostupne --
            //   jinak by se od nej cekalo, ze se postavi, a rozestaveni by neslo
            //   ukoncit (`min(11, dostupni)` by vyslo prilis vysoko).
            if ($p->isOutNextSetup()) {
                continue;
            }
            if ($p->getState() === PlayerState::OFF_PITCH || $p->getState()->isOnPitch()) {
                $availablePlayers++;
            }
        }
        $required = min(RulesEngine::MAX_PLAYERS_ON_PITCH, $availablePlayers);

        if (count($playersOnPitch) < $required) {
            throw new \InvalidArgumentException(
                "Need at least {$required} players on pitch (have " . count($playersOnPitch) . ')'
            );
        }

        // ⭐ BALIK G 24.09.2026 -- horni mez, ktera tu do ted chybela.
        if (count($playersOnPitch) > RulesEngine::MAX_PLAYERS_ON_PITCH) {
            throw new \InvalidArgumentException(
                'Cannot have more than ' . RulesEngine::MAX_PLAYERS_ON_PITCH
                . ' players on pitch (have ' . count($playersOnPitch) . ')'
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

        // ⭐ BALIK G 24.09.2026 -- SPOTREBA priznaku. C++ ho maze pri sestavovani
        //   dostupnych (`takeAvailability`, `game_simulator.cpp:213-218`), takze
        //   zkolabovany vynecha PRESNE JEDNO rozestaveni. Tady je to tentyz
        //   okamzik: obe strany uz stoji, priznak splnil ucel.
        foreach ($state->getPlayers() as $p) {
            if ($p->isOutNextSetup()) {
                $state = $state->withPlayer($p->withOutNextSetup(false));
            }
        }

        // ⭐ Secret Weapon (r. 8451-8454): kdo stoji na hristi pri vykopu, v drivu HRAL.
        foreach ($state->getPlayers() as $p) {
            if ($p->getState()->isOnPitch()) {
                $state = $state->withPlayer($p->withPlayedThisDrive(true));
            }
        }

        // Both teams set up - proceed to kickoff
        $kickingTeam = $state->getKickingTeam() ?? TeamSide::AWAY;
        $receivingTeam = $kickingTeam->opponent();

        // ⭐ PHP31: cil kopu se uz vybira, ne bere natvrdo -- viz `chooseKickTarget`.
        $kickTarget = $this->kickoffResolver->chooseKickTarget($state, $receivingTeam);
        $kickoffResult = $this->kickoffResolver->resolveKickoff($state, $kickTarget);

        $newState = $kickoffResult['state']
            ->withPhase(GamePhase::PLAY)
            ->withActiveTeam($receivingTeam);

        return ActionResult::success($newState, $kickoffResult['events']);
    }

    /**
     * Auto-setup: place all available players using a formation template.
     *
     * @param array<string, mixed> $params
     */
    public function resolveAutoSetup(GameState $state, array $params): ActionResult
    {
        $formation = (string) ($params['formation'] ?? 'standard');
        $side = $state->getActiveTeam();

        // Clear existing on-pitch players back to off_pitch
        foreach ($state->getPlayersOnPitch($side) as $player) {
            $state = $state->withPlayer(
                $player->withPosition(null)->withState(PlayerState::OFF_PITCH),
            );
        }

        $state = $this->autoSetupTeam($state, $side, $formation);

        return ActionResult::success($state, []);
    }

    private function autoSetupTeam(GameState $state, TeamSide $side, string $formation = 'standard'): GameState
    {
        $offPitchPlayers = [];
        foreach ($state->getTeamPlayers($side) as $player) {
            // ⭐ Sweltering Heat: zkolabovany tenhle vykop vynechava.
            if ($player->getState() === PlayerState::OFF_PITCH && !$player->isOutNextSetup()) {
                $offPitchPlayers[] = $player;
            }
        }

        $positions = $this->getFormationPositions($side, $formation);

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
     * @return list<Position>
     */
    private function getFormationPositions(TeamSide $side, string $formation): array
    {
        $isHome = $side === TeamSide::HOME;
        $losX = $isHome ? 12 : 13;
        $mid = $isHome ? 8 : 17;
        $back = $isHome ? 4 : 21;
        $deep = $isHome ? 2 : 23;

        return match ($formation) {
            'spread' => [
                // 3 on LOS spread wide
                new Position($losX, 4), new Position($losX, 7), new Position($losX, 10),
                // 4 midfield spread
                new Position($mid, 3), new Position($mid, 6), new Position($mid, 8), new Position($mid, 11),
                // 4 backfield
                new Position($back, 4), new Position($back, 7), new Position($back, 10), new Position($deep, 7),
            ],
            'heavy_los' => [
                // 5 on LOS
                new Position($losX, 5), new Position($losX, 6), new Position($losX, 7), new Position($losX, 8), new Position($losX, 9),
                // 3 midfield
                new Position($mid, 4), new Position($mid, 7), new Position($mid, 10),
                // 3 backfield
                new Position($back, 5), new Position($back, 7), new Position($back, 9),
            ],
            default => [
                // Standard: 3 on LOS, 4 midfield, 4 backfield
                new Position($losX, 6), new Position($losX, 7), new Position($losX, 8),
                new Position($mid, 4), new Position($mid, 6), new Position($mid, 8), new Position($mid, 10),
                new Position($back, 3), new Position($back, 5), new Position($back, 9), new Position($back, 11),
            ],
        };
    }
}
