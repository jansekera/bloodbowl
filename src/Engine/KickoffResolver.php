<?php

declare(strict_types=1);

namespace App\Engine;

use App\DTO\BallState;
use App\DTO\GameEvent;
use App\DTO\GameState;
use App\Enum\KickoffEvent;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use App\Enum\Weather;
use App\ValueObject\Position;

final class KickoffResolver
{
    public function __construct(
        private readonly DiceRollerInterface $dice,
        private readonly ScatterCalculator $scatterCalc,
        private readonly BallResolver $ballResolver,
    ) {
    }

    /**
     * Resolve the full kickoff sequence.
     * @return array{state: GameState, events: list<GameEvent>}
     */
    public function resolveKickoff(GameState $state, Position $kickTarget): array
    {
        $receivingTeam = $state->getKickingTeam()?->opponent() ?? TeamSide::HOME;

        // Scatter the ball from kick target
        $d8 = $this->dice->rollD8();
        $d6 = $this->dice->rollD6();

        // Kick skill: halve scatter distance (round up)
        $events = [];
        $kickingTeamSide = $state->getKickingTeam() ?? TeamSide::AWAY;
        if ($this->hasKickPlayer($state, $kickingTeamSide)) {
            $originalD6 = $d6;
            $d6 = (int) ceil($d6 / 2);
            $events[] = GameEvent::kickSkill(0, $originalD6, $d6);
        }

        $landingPos = $this->scatterCalc->scatterWithDistance($kickTarget, $d8, $d6);

        $events[] = GameEvent::kickoff((string) $kickTarget, (string) $landingPos);

        // Sweltering Heat: KO a random player from each team
        if ($state->getWeather() === Weather::SWELTERING_HEAT) {
            $swelterResult = $this->resolveSwelteringHeat($state);
            $state = $swelterResult['state'];
            $events = array_merge($events, $swelterResult['events']);
        }

        // Roll 2D6 kickoff table
        $kickoffTableResult = $this->resolveKickoffTable($state, $receivingTeam);
        $state = $kickoffTableResult['state'];
        $events = array_merge($events, $kickoffTableResult['events']);

        // Check if ball landed in receiving half
        if (!$landingPos->isOnPitch() || !$this->scatterCalc->isInReceivingHalf($landingPos, $receivingTeam)) {
            // Touchback
            $touchbackResult = $this->resolveTouchback($state, $receivingTeam);
            $events = array_merge($events, $touchbackResult['events']);
            return ['state' => $touchbackResult['state'], 'events' => $events];
        }

        // Ball landed on pitch in receiving half
        $state = $state->withBall(BallState::onGround($landingPos));

        // Kick-Off Return: one receiving player with KickOffReturn moves up to 3 squares toward ball
        $korResult = $this->resolveKickOffReturn($state, $landingPos, $receivingTeam);
        $state = $korResult['state'];
        $events = array_merge($events, $korResult['events']);

        // Check if there's a player at landing position
        $playerAtLanding = $state->getPlayerAtPosition($landingPos);
        if ($playerAtLanding !== null && $playerAtLanding->getState() === PlayerState::STANDING) {
            // Attempt catch
            $catchResult = $this->ballResolver->resolveCatch($state, $playerAtLanding);
            $events = array_merge($events, $catchResult['events']);
            return ['state' => $catchResult['state'], 'events' => $events];
        }

        // No player at landing - ball bounces
        $bounceResult = $this->ballResolver->resolveBounce($state, $landingPos);
        $events = array_merge($events, $bounceResult['events']);
        return ['state' => $bounceResult['state'], 'events' => $events];
    }

    /**
     * Roll 2D6 and resolve the kickoff table event.
     * @return array{state: GameState, events: list<GameEvent>}
     */
    public function resolveKickoffTable(GameState $state, TeamSide $receivingTeam): array
    {
        $d1 = $this->dice->rollD6();
        $d2 = $this->dice->rollD6();
        $roll = $d1 + $d2;

        $kickoffEvent = KickoffEvent::from($roll);
        $kickingTeam = $receivingTeam->opponent();

        return match ($kickoffEvent) {
            KickoffEvent::GetTheRef => $this->resolveGetTheRef($state, $roll),
            KickoffEvent::Riot => $this->resolveRiot($state, $roll, $receivingTeam),
            KickoffEvent::PerfectDefence => $this->resolvePerfectDefence($state, $roll),
            KickoffEvent::HighKick => $this->resolveHighKick($state, $roll, $receivingTeam),
            KickoffEvent::Cheering => $this->resolveCheering($state, $roll),
            KickoffEvent::BrilliantCoaching => $this->resolveBrilliantCoaching($state, $roll),
            KickoffEvent::ChangingWeather => $this->resolveChangingWeather($state, $roll),
            KickoffEvent::QuickSnap => $this->resolveQuickSnap($state, $roll, $receivingTeam),
            KickoffEvent::Blitz => $this->resolveBlitz($state, $roll, $kickingTeam),
            KickoffEvent::ThrowARock => $this->resolveThrowARock($state, $roll, $kickingTeam),
            KickoffEvent::PitchInvasion => $this->resolvePitchInvasion($state, $roll, $kickingTeam),
        };
    }

    /**
     * Get the Ref! (2): Both teams gain a fouling bonus for this drive.
     * Simplified: no-op (would need a per-drive flag for foul armour +1).
     * @return array{state: GameState, events: list<GameEvent>}
     */
    private function resolveGetTheRef(GameState $state, int $roll): array
    {
        return [
            'state' => $state,
            'events' => [GameEvent::kickoffTableEvent($roll, 'Get the Ref!', 'Both teams get away with more fouls this drive')],
        ];
    }

    /**
     * Riot! (3): Turn counter changes by 1.
     * If receiving team's turn 1: turn counter advances (+1 effectively loses a turn).
     * Otherwise: turn counter goes back (-1 effectively gains a turn).
     * Simplified: receiving team turn ±1.
     * @return array{state: GameState, events: list<GameEvent>}
     */
    private function resolveRiot(GameState $state, int $roll, TeamSide $receivingTeam): array
    {
        $teamState = $state->getTeamState($receivingTeam);
        $currentTurn = $teamState->getTurnNumber();

        // BB rules: If receiving team is about to take first turn, +1 (lose a turn).
        // Otherwise -1 (gain a turn).
        if ($currentTurn <= 1) {
            $newTurn = $currentTurn + 1;
            $effect = 'Fans riot! Receiving team loses a turn';
        } else {
            $newTurn = max(1, $currentTurn - 1);
            $effect = 'Fans riot! Receiving team gains a turn';
        }

        $state = $state->withTeamState($receivingTeam, $teamState->withTurnNumber($newTurn));

        return [
            'state' => $state,
            'events' => [GameEvent::kickoffTableEvent($roll, 'Riot!', $effect)],
        ];
    }

    /**
     * Perfect Defence (4): No-op (auto - keep positions).
     * @return array{state: GameState, events: list<GameEvent>}
     */
    private function resolvePerfectDefence(GameState $state, int $roll): array
    {
        return [
            'state' => $state,
            'events' => [GameEvent::kickoffTableEvent($roll, 'Perfect Defence', 'Kicking team keeps their defensive positions')],
        ];
    }

    /**
     * High Kick (5): Move the closest receiving player under the ball.
     * @return array{state: GameState, events: list<GameEvent>}
     */
    private function resolveHighKick(GameState $state, int $roll, TeamSide $receivingTeam): array
    {
        $ball = $state->getBall();
        $ballPos = $ball->getPosition();

        if ($ballPos === null || $ball->isHeld()) {
            return [
                'state' => $state,
                'events' => [GameEvent::kickoffTableEvent($roll, 'High Kick', 'No ball on pitch to move under')],
            ];
        }

        // Find the closest receiving team player who is standing
        $players = $state->getPlayersOnPitch($receivingTeam);
        $bestPlayer = null;
        $bestDist = PHP_INT_MAX;

        foreach ($players as $player) {
            $pos = $player->getPosition();
            if ($pos === null || $player->getState() !== PlayerState::STANDING) {
                continue;
            }
            $dist = $pos->distanceTo($ballPos);
            if ($dist < $bestDist) {
                $bestDist = $dist;
                $bestPlayer = $player;
            }
        }

        if ($bestPlayer === null) {
            return [
                'state' => $state,
                'events' => [GameEvent::kickoffTableEvent($roll, 'High Kick', 'No player to move under the ball')],
            ];
        }

        // Move the player to ball position
        $state = $state->withPlayer($bestPlayer->withPosition($ballPos));

        return [
            'state' => $state,
            'events' => [GameEvent::kickoffTableEvent($roll, 'High Kick', "Player {$bestPlayer->getName()} moved under the ball")],
        ];
    }

    /**
     * Cheering Fans (6): Random extra reroll for one team.
     * Each team rolls D6, higher roll gets +1 reroll. Tie: no effect.
     * @return array{state: GameState, events: list<GameEvent>}
     */
    private function resolveCheering(GameState $state, int $roll): array
    {
        $homeRoll = $this->dice->rollD6();
        $awayRoll = $this->dice->rollD6();

        if ($homeRoll > $awayRoll) {
            $teamState = $state->getHomeTeam();
            $state = $state->withHomeTeam($teamState->withRerolls($teamState->getRerolls() + 1));
            $effect = "Home fans cheer louder ({$homeRoll} vs {$awayRoll})! Home team gets +1 reroll";
        } elseif ($awayRoll > $homeRoll) {
            $teamState = $state->getAwayTeam();
            $state = $state->withAwayTeam($teamState->withRerolls($teamState->getRerolls() + 1));
            $effect = "Away fans cheer louder ({$homeRoll} vs {$awayRoll})! Away team gets +1 reroll";
        } else {
            $effect = "Fans equally loud ({$homeRoll} vs {$awayRoll}), no effect";
        }

        return [
            'state' => $state,
            'events' => [GameEvent::kickoffTableEvent($roll, 'Cheering Fans', $effect)],
        ];
    }

    /**
     * Brilliant Coaching (7): Random extra reroll for one team.
     * Same mechanic as Cheering Fans.
     * @return array{state: GameState, events: list<GameEvent>}
     */
    private function resolveBrilliantCoaching(GameState $state, int $roll): array
    {
        $homeRoll = $this->dice->rollD6();
        $awayRoll = $this->dice->rollD6();

        if ($homeRoll > $awayRoll) {
            $teamState = $state->getHomeTeam();
            $state = $state->withHomeTeam($teamState->withRerolls($teamState->getRerolls() + 1));
            $effect = "Home coach inspires ({$homeRoll} vs {$awayRoll})! Home team gets +1 reroll";
        } elseif ($awayRoll > $homeRoll) {
            $teamState = $state->getAwayTeam();
            $state = $state->withAwayTeam($teamState->withRerolls($teamState->getRerolls() + 1));
            $effect = "Away coach inspires ({$homeRoll} vs {$awayRoll})! Away team gets +1 reroll";
        } else {
            $effect = "Coaches equally brilliant ({$homeRoll} vs {$awayRoll}), no effect";
        }

        return [
            'state' => $state,
            'events' => [GameEvent::kickoffTableEvent($roll, 'Brilliant Coaching', $effect)],
        ];
    }

    /**
     * Changing Weather (8): Roll 2D6 for new weather.
     * @return array{state: GameState, events: list<GameEvent>}
     */
    private function resolveChangingWeather(GameState $state, int $roll): array
    {
        $oldWeather = $state->getWeather();
        $weatherRoll = $this->dice->rollD6() + $this->dice->rollD6();
        $newWeather = Weather::fromRoll($weatherRoll);
        $state = $state->withWeather($newWeather);

        $events = [
            GameEvent::kickoffTableEvent($roll, 'Changing Weather', "Weather changes from {$oldWeather->label()} to {$newWeather->label()}"),
            GameEvent::weatherChange($oldWeather->label(), $newWeather->label()),
        ];

        return ['state' => $state, 'events' => $events];
    }

    /**
     * Quick Snap! (9): Receiving team players each move 1 square forward (towards LoS).
     * Forward = towards opponent's half: HOME moves right (+x), AWAY moves left (-x).
     * @return array{state: GameState, events: list<GameEvent>}
     */
    private function resolveQuickSnap(GameState $state, int $roll, TeamSide $receivingTeam): array
    {
        $dx = $receivingTeam === TeamSide::HOME ? 1 : -1;
        $movedCount = 0;

        foreach ($state->getPlayersOnPitch($receivingTeam) as $player) {
            $pos = $player->getPosition();
            if ($pos === null || $player->getState() !== PlayerState::STANDING) {
                continue;
            }

            $newPos = new Position($pos->getX() + $dx, $pos->getY());

            // Only move if destination is on pitch and unoccupied
            if ($newPos->isOnPitch() && $state->getPlayerAtPosition($newPos) === null) {
                $state = $state->withPlayer($player->withPosition($newPos));
                $movedCount++;
            }
        }

        return [
            'state' => $state,
            'events' => [GameEvent::kickoffTableEvent($roll, 'Quick Snap!', "Receiving team moves {$movedCount} players 1 square forward")],
        ];
    }

    /**
     * Blitz! (10): Kicking team gets a free turn before receiving team.
     * Simplified: each kicking team player moves 1 square forward.
     * @return array{state: GameState, events: list<GameEvent>}
     */
    private function resolveBlitz(GameState $state, int $roll, TeamSide $kickingTeam): array
    {
        $dx = $kickingTeam === TeamSide::HOME ? 1 : -1;
        $movedCount = 0;

        foreach ($state->getPlayersOnPitch($kickingTeam) as $player) {
            $pos = $player->getPosition();
            if ($pos === null || $player->getState() !== PlayerState::STANDING) {
                continue;
            }

            $newPos = new Position($pos->getX() + $dx, $pos->getY());

            if ($newPos->isOnPitch() && $state->getPlayerAtPosition($newPos) === null) {
                $state = $state->withPlayer($player->withPosition($newPos));
                $movedCount++;
            }
        }

        return [
            'state' => $state,
            'events' => [GameEvent::kickoffTableEvent($roll, 'Blitz!', "Kicking team surges forward! {$movedCount} players moved")],
        ];
    }

    /**
     * Throw a Rock! (11): Random opponent player gets stunned.
     * @return array{state: GameState, events: list<GameEvent>}
     */
    private function resolveThrowARock(GameState $state, int $roll, TeamSide $kickingTeam): array
    {
        // Each team throws a rock at a random opponent
        $results = [];

        foreach ([TeamSide::HOME, TeamSide::AWAY] as $targetSide) {
            $targets = $state->getPlayersOnPitch($targetSide);
            $standingTargets = array_values(array_filter(
                $targets,
                fn($p) => $p->getState() === PlayerState::STANDING,
            ));

            if (empty($standingTargets)) {
                continue;
            }

            // Random target: use D6 mod number of targets
            $targetIndex = ($this->dice->rollD6() - 1) % count($standingTargets);
            $victim = $standingTargets[$targetIndex];

            $state = $state->withPlayer($victim->withState(PlayerState::STUNNED));
            $results[] = $victim->getName();
        }

        $effect = empty($results) ? 'No one was hit' : implode(' and ', $results) . ' stunned by rocks from the crowd';

        return [
            'state' => $state,
            'events' => [GameEvent::kickoffTableEvent($roll, 'Throw a Rock!', $effect)],
        ];
    }

    /**
     * Pitch Invasion! (12): D6 for each opponent player; on 6, stunned.
     * @return array{state: GameState, events: list<GameEvent>}
     */
    private function resolvePitchInvasion(GameState $state, int $roll, TeamSide $kickingTeam): array
    {
        $stunnedCount = 0;

        // Both teams are affected
        foreach ([TeamSide::HOME, TeamSide::AWAY] as $side) {
            foreach ($state->getPlayersOnPitch($side) as $player) {
                if ($player->getState() !== PlayerState::STANDING) {
                    continue;
                }

                $invasionRoll = $this->dice->rollD6();
                if ($invasionRoll === 6) {
                    $state = $state->withPlayer($player->withState(PlayerState::STUNNED));
                    $stunnedCount++;
                }
            }
        }

        $effect = $stunnedCount === 0
            ? 'Fans invade the pitch but no one is hurt'
            : "{$stunnedCount} player(s) stunned by pitch invasion";

        return [
            'state' => $state,
            'events' => [GameEvent::kickoffTableEvent($roll, 'Pitch Invasion!', $effect)],
        ];
    }

    /**
     * Sweltering Heat: KO a random standing player from each team.
     * @return array{state: GameState, events: list<GameEvent>}
     */
    private function resolveSwelteringHeat(GameState $state): array
    {
        $events = [];

        foreach ([TeamSide::HOME, TeamSide::AWAY] as $side) {
            $players = $state->getPlayersOnPitch($side);
            $standing = array_values(array_filter(
                $players,
                fn($p) => $p->getState() === PlayerState::STANDING,
            ));

            if (empty($standing)) {
                continue;
            }

            $index = ($this->dice->rollD6() - 1) % count($standing);
            $victim = $standing[$index];

            $state = $state->withPlayer(
                $victim->withState(PlayerState::KO)->withPosition(null),
            );

            $teamName = $state->getTeamState($side)->getName();
            $events[] = GameEvent::swelteringHeat($victim->getId(), $victim->getName(), $teamName);
        }

        return ['state' => $state, 'events' => $events];
    }

    /**
     * Handle touchback: ball given to any receiving team player on pitch.
     * Picks the player closest to center of own half.
     * @return array{state: GameState, events: list<GameEvent>}
     */
    public function resolveTouchback(GameState $state, TeamSide $receivingTeam): array
    {
        $players = $state->getPlayersOnPitch($receivingTeam);

        if (empty($players)) {
            // No players on pitch - ball stays off
            return ['state' => $state->withBall(BallState::offPitch()), 'events' => []];
        }

        // Find player closest to center of their half
        $centerX = $receivingTeam === TeamSide::HOME ? 6 : 19;
        $centerY = 7;
        $centerPos = new Position($centerX, $centerY);

        $bestPlayer = null;
        $bestDist = PHP_INT_MAX;

        foreach ($players as $player) {
            $pos = $player->getPosition();
            if ($pos === null) {
                continue;
            }
            $dist = $pos->distanceTo($centerPos);
            if ($dist < $bestDist) {
                $bestDist = $dist;
                $bestPlayer = $player;
            }
        }

        if ($bestPlayer === null || $bestPlayer->getPosition() === null) {
            return ['state' => $state->withBall(BallState::offPitch()), 'events' => []];
        }

        $pos = $bestPlayer->getPosition();
        $teamName = $state->getTeamState($receivingTeam)->getName();
        $events = [GameEvent::touchback($bestPlayer->getId(), $teamName)];

        $state = $state->withBall(BallState::carried($pos, $bestPlayer->getId()));

        return ['state' => $state, 'events' => $events];
    }

    /**
     * Kick-Off Return: closest standing receiving player with KickOffReturn
     * moves up to 3 squares toward the ball (free move, no dodge/GFI).
     * @return array{state: GameState, events: list<GameEvent>}
     */
    private function resolveKickOffReturn(GameState $state, Position $ballPos, TeamSide $receivingTeam): array
    {
        $bestPlayer = null;
        $bestDist = PHP_INT_MAX;

        foreach ($state->getPlayersOnPitch($receivingTeam) as $player) {
            if ($player->getState() !== PlayerState::STANDING) {
                continue;
            }
            if (!$player->hasSkill(SkillName::KickOffReturn)) {
                continue;
            }
            $pos = $player->getPosition();
            if ($pos === null) {
                continue;
            }
            $dist = $pos->distanceTo($ballPos);
            if ($dist < $bestDist) {
                $bestDist = $dist;
                $bestPlayer = $player;
            }
        }

        if ($bestPlayer === null) {
            return ['state' => $state, 'events' => []];
        }

        $pos = $bestPlayer->getPosition();
        if ($pos->equals($ballPos)) {
            return ['state' => $state, 'events' => []];
        }

        $events = [];
        $currentPos = $pos;

        for ($step = 0; $step < 3; $step++) {
            $nextPos = $this->stepToward($currentPos, $ballPos);
            if ($nextPos === null || $nextPos->equals($currentPos)) {
                break;
            }
            if (!$nextPos->isOnPitch()) {
                break;
            }
            if ($state->getPlayerAtPosition($nextPos) !== null) {
                break;
            }
            $currentPos = $nextPos;
            if ($currentPos->equals($ballPos)) {
                break;
            }
        }

        if (!$currentPos->equals($pos)) {
            $events[] = GameEvent::kickOffReturn($bestPlayer->getId(), (string) $pos, (string) $currentPos);
            $state = $state->withPlayer($bestPlayer->withPosition($currentPos));
        }

        return ['state' => $state, 'events' => $events];
    }

    private function stepToward(Position $from, Position $to): ?Position
    {
        $dx = $to->getX() - $from->getX();
        $dy = $to->getY() - $from->getY();

        $sx = $dx === 0 ? 0 : ($dx > 0 ? 1 : -1);
        $sy = $dy === 0 ? 0 : ($dy > 0 ? 1 : -1);

        if ($sx === 0 && $sy === 0) {
            return null;
        }

        return new Position($from->getX() + $sx, $from->getY() + $sy);
    }

    private function hasKickPlayer(GameState $state, TeamSide $side): bool
    {
        foreach ($state->getPlayersOnPitch($side) as $player) {
            if ($player->hasSkill(SkillName::Kick) && $player->getState() === PlayerState::STANDING) {
                return true;
            }
        }
        return false;
    }

    /**
     * Get default kick target position (center of receiving half).
     */
    public function getDefaultKickTarget(TeamSide $receivingTeam): Position
    {
        return $receivingTeam === TeamSide::HOME
            ? new Position(6, 7)
            : new Position(19, 7);
    }
}
