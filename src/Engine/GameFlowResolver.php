<?php

declare(strict_types=1);

namespace App\Engine;

use App\DTO\BallState;
use App\DTO\GameEvent;
use App\DTO\GameState;
use App\DTO\MatchPlayerDTO;
use App\Enum\GamePhase;
use App\Enum\PlayerState;
use App\Enum\Weather;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use App\ValueObject\Position;

final class GameFlowResolver
{
    public function __construct(
        private readonly DiceRollerInterface $dice,
    ) {
    }

    /**
     * Check if a touchdown has occurred.
     * Standing player carrying ball in opponent's end zone.
     */
    public function checkTouchdown(GameState $state): ?TeamSide
    {
        $ball = $state->getBall();
        if (!$ball->isHeld()) {
            return null;
        }

        $carrierId = $ball->getCarrierId();
        if ($carrierId === null) {
            return null;
        }

        $carrier = $state->getPlayer($carrierId);
        if ($carrier === null || $carrier->getState() !== PlayerState::STANDING) {
            return null;
        }

        $pos = $carrier->getPosition();
        if ($pos === null) {
            return null;
        }

        // Home team scores in away end zone (x=25), away team scores in home end zone (x=0)
        if ($carrier->getTeamSide() === TeamSide::HOME && $pos->getX() === Position::PITCH_WIDTH - 1) {
            return TeamSide::HOME;
        }
        if ($carrier->getTeamSide() === TeamSide::AWAY && $pos->getX() === 0) {
            return TeamSide::AWAY;
        }

        return null;
    }

    /**
     * Apply touchdown: increment score, transition to TOUCHDOWN phase.
     * @return array{state: GameState, events: list<GameEvent>}
     */
    public function resolveTouchdown(GameState $state, TeamSide $scoringTeam): array
    {
        $teamState = $state->getTeamState($scoringTeam);
        $newTeamState = $teamState->withScore($teamState->getScore() + 1);
        $state = $state->withTeamState($scoringTeam, $newTeamState);

        $carrierId = $state->getBall()->getCarrierId();
        $events = [GameEvent::touchdown($carrierId ?? 0, $teamState->getName())];

        return ['state' => $state, 'events' => $events];
    }

    /**
     * Transition from TOUCHDOWN to SETUP.
     * Scoring team kicks off next. Reset players.
     * @return array{state: GameState, events: list<GameEvent>}
     */
    public function resolvePostTouchdown(GameState $state): array
    {
        // Check if we have remaining turns
        if (!$this->hasRemainingTurns($state)) {
            if ($state->getHalf() >= 2) {
                $state = $state->withPhase(GamePhase::GAME_OVER);
                return ['state' => $state, 'events' => [
                    GameEvent::gameOver(
                        $state->getHomeTeam()->getName(),
                        $state->getHomeTeam()->getScore(),
                        $state->getAwayTeam()->getName(),
                        $state->getAwayTeam()->getScore(),
                    ),
                ]];
            }

            // Half time
            return $this->resolveHalfTime($state);
        }

        // Secret Weapon: eject players with Secret Weapon at end of drive
        $swEvents = [];
        $state = $this->ejectSecretWeapons($state, $swEvents);

        // ⛔ DOPLNENO 16.09.2026: hody na navrat KO hracu patri i sem (r. 1007-1012),
        //   engine je delal jen o polocase.
        // ⚠️ Poradi: heat se hazi, DOKUD hraci jeste stoji na hristi -- po
        //   `resetPlayersForSetup` uz je hriste prazdne a hazelo by se za nikoho.
        $state = $this->hodyNaSwelteringHeat($state, $swEvents);
        $state = $this->hodyNaNavratKo($state, $swEvents);

        // Reset for new kickoff - scoring team kicks
        $state = $this->resetPlayersForSetup($state);
        $state = $state
            ->withPhase(GamePhase::SETUP)
            ->withBall(BallState::offPitch());

        // The team that was scored against receives (kicks team scored)
        // After a touchdown, the scoring team kicks off
        // We need to figure out who scored. Check which team has higher score than before.
        // Actually, the scoring team is determined by the caller. For setup, the receiving team goes first.
        // The active team for setup should be the receiving team.
        // In BB rules: after a touchdown, the scoring team kicks off.
        // kickingTeam is already set from match creation. We update it here.
        // The scoring team will be the kicking team for the next kickoff.

        return ['state' => $state, 'events' => $swEvents];
    }

    /**
     * ⭐ BALIK G 24.09.2026 -- SWELTERING HEAT, `rules_bb2016.txt` r. 1477-1481:
     *   "It's so hot and humid that some players collapse from heat exhaustion.
     *   Roll a D6 for each player on the pitch AT THE END OF A DRIVE. On a roll
     *   of 1 the player collapses and MAY NOT BE SET UP for the next kick-off."
     *
     * ⛔ Do 24.09. to `KickoffResolver` delal PRI VYKOPU a jinak: jeden D6 na tym,
     *   nahodne vybrana obet, a poslal ji do KO. Nesedelo ani kdy, ani za koho,
     *   ani kam -- KO ma navratovy hod (4+), heat exhaustion zadny nema.
     *
     * ⭐ Hazi se za KAZDEHO hrace NA HRISTI, tedy i za lezici a omracene --
     *   pravidlo rika "each player on the pitch", ne "each standing player".
     *
     * Vzor: C++ `engine/src/game_simulator.cpp:389-393`.
     *
     * @param list<GameEvent> $events
     */
    private function hodyNaSwelteringHeat(GameState $state, array &$events): GameState
    {
        if ($state->getWeather() !== Weather::SWELTERING_HEAT) {
            return $state;
        }

        foreach ($state->getPlayers() as $player) {
            if (!$player->getState()->isOnPitch()) {
                continue;
            }
            if ($this->dice->rollD6() !== 1) {
                continue;
            }

            $state = $state->withPlayer($player->withOutNextSetup(true));
            $teamName = $state->getTeamState($player->getTeamSide())->getName();
            $events[] = GameEvent::swelteringHeat($player->getId(), $player->getName(), $teamName);
        }

        return $state;
    }

    /**
     * Hody na navrat KO hracu: 1 D6 za kazdeho KO hrace, na 4+ jde do rezerv.
     *
     * ⭐ 16.09.2026: `rules_bb2016.txt` r. 1007-1012 -- "After a touchdown has been
     *   scored, AND at the start of the second half ... each coach should roll one D6
     *   for each KO'd player ... On a roll of 4, 5 or 6 the player is fit enough to
     *   return to play." ⇒ NEJEN o polocase, ale i po KAZDEM touchdownu.
     *
     * @param list<GameEvent> $events
     */
    private function hodyNaNavratKo(GameState $state, array &$events): GameState
    {
        foreach ($state->getPlayers() as $player) {
            if ($player->getState() !== PlayerState::KO) {
                continue;
            }
            $roll = $this->dice->rollD6();
            $success = $roll >= 4;
            $events[] = GameEvent::koRecovery($player->getId(), $roll, $success);

            if ($success) {
                // Hrac se vraci do rezerv (OFF_PITCH, pripraven k rozestaveni)
                $state = $state->withPlayer(
                    $player->withState(PlayerState::OFF_PITCH)->withPosition(null),
                );
            }
        }

        return $state;
    }

    /**
     * Resolve half-time: KO recovery, reset, swap sides.
     * @return array{state: GameState, events: list<GameEvent>}
     */
    public function resolveHalfTime(GameState $state): array
    {
        $events = [GameEvent::halfTime($state->getHalf())];

        // Secret Weapon: eject players at end of drive
        $state = $this->ejectSecretWeapons($state, $events);

        // ⭐ Polocas je take konec drivu, takze heat patri i sem (viz vyse).
        $state = $this->hodyNaSwelteringHeat($state, $events);
        $state = $this->hodyNaNavratKo($state, $events);

        // Reset all players for new setup
        $state = $this->resetPlayersForSetup($state);

        // Advance to second half
        $state = $state
            ->withPhase(GamePhase::SETUP)
            ->withHalf(2)
            ->withBall(BallState::offPitch());

        // In second half, the kicking team from first half now receives
        $firstHalfKicker = $state->getKickingTeam();
        if ($firstHalfKicker !== null) {
            $state = $state
                ->withKickingTeam($firstHalfKicker->opponent())
                ->withActiveTeam($firstHalfKicker); // first half kicker now receives = sets up first
        }

        // Reset turn counters for both teams
        $homeTeam = $state->getHomeTeam()->withTurnNumber(1);
        $awayTeam = $state->getAwayTeam()->withTurnNumber(1);
        $state = $state->withHomeTeam($homeTeam)->withAwayTeam($awayTeam);

        // ⭐ DOPLNENO 16.09.2026: tymove prehozy se o polocase vraci na vychozi pocet
        //   (`rules_bb2016.txt` r. 941-943: "At half time ... their team re-rolls are
        //   restored to their starting level"). Engine je neobnovoval vubec.
        foreach ([TeamSide::HOME, TeamSide::AWAY] as $strana) {
            $tym = $state->getTeamState($strana);
            $state = $state->withTeamState($strana, $tym->withRerolls($tym->getRerollsStart()));
        }

        // Leader: +1 reroll per team (max 1) if they have a Leader player
        $state = $this->applyLeaderBonus($state, $events);

        return ['state' => $state, 'events' => $events];
    }

    /**
     * Strip all players from pitch for setup reset.
     */
    public function resetPlayersForSetup(GameState $state): GameState
    {
        foreach ($state->getPlayers() as $player) {
            if ($player->getState()->isOnPitch()) {
                $state = $state->withPlayer(
                    $player
                        ->withState(PlayerState::OFF_PITCH)
                        ->withPosition(null)
                        ->withHasMoved(false)
                        ->withHasActed(false)
                        // ⭐ „**or the drive ends**" (r. 7986, r. 8575) --
                        //   konec drivu oba pretrvavajici stavy RUSI.
                        ->withRooted(false)
                        ->withBigGuyStupefied(false)
                        ->withLostTacklezones(false)
                        ->withMovementRemaining($player->getStats()->getMovement()),
                );
            }
        }

        return $state;
    }

    /**
     * Eject all Secret Weapon players from the pitch.
     * @param list<GameEvent> $events
     */
    private function ejectSecretWeapons(GameState $state, array &$events): GameState
    {
        foreach ($state->getPlayers() as $player) {
            if ($player->hasSkill(SkillName::SecretWeapon) && $player->getState()->isOnPitch()) {
                $events[] = GameEvent::secretWeaponEjection($player->getId());
                $state = $state->withPlayer(
                    $player->withState(PlayerState::EJECTED)->withPosition(null),
                );
            }
        }
        return $state;
    }

    /**
     * Apply Leader bonus: +1 reroll for each team with a Leader player (max 1).
     * @param list<GameEvent> $events
     */
    private function applyLeaderBonus(GameState $state, array &$events): GameState
    {
        foreach ([TeamSide::HOME, TeamSide::AWAY] as $side) {
            $hasLeader = false;
            foreach ($state->getPlayers() as $player) {
                if ($player->getTeamSide() === $side && $player->hasSkill(SkillName::Leader)
                    && !in_array($player->getState(), [PlayerState::INJURED, PlayerState::EJECTED], true)) {
                    $hasLeader = true;
                    break;
                }
            }
            if ($hasLeader) {
                $teamState = $state->getTeamState($side);
                $state = $state->withTeamState($side, $teamState->withRerolls($teamState->getRerolls() + 1));
                $events[] = GameEvent::leaderBonus($teamState->getName());
            }
        }
        return $state;
    }

    /**
     * Check if the current half has remaining turns for at least one team.
     */
    public function hasRemainingTurns(GameState $state): bool
    {
        $home = $state->getHomeTeam();
        $away = $state->getAwayTeam();

        return $home->getTurnNumber() < 8 || $away->getTurnNumber() < 8;
    }
}
