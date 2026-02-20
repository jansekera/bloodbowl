<?php
declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Enum\ActionType;
use App\Enum\GamePhase;
use App\Enum\PlayerState;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

final class SetupTest extends TestCase
{
    public function testSetupPlayerChangesStateToStanding(): void
    {
        $builder = new GameStateBuilder();
        $builder->withPhase(GamePhase::SETUP);
        $builder->withActiveTeam(TeamSide::HOME);
        $builder->addOffPitchPlayer(TeamSide::HOME, id: 1);

        $state = $builder->build();
        $original = $state->getPlayer(1);
        $this->assertNotNull($original);
        $this->assertSame(PlayerState::OFF_PITCH, $original->getState());

        $resolver = new ActionResolver(new FixedDiceRoller([]));
        $result = $resolver->resolve($state, ActionType::SETUP_PLAYER, [
            'playerId' => 1,
            'x' => 5,
            'y' => 7,
        ]);

        $player = $result->getNewState()->getPlayer(1);
        $this->assertNotNull($player);
        $pos = $player->getPosition();
        $this->assertNotNull($pos);
        $this->assertSame(PlayerState::STANDING, $player->getState());
        $this->assertSame(5, $pos->getX());
        $this->assertSame(7, $pos->getY());
    }

    public function testEndSetupAutoSetsUpOpponent(): void
    {
        $builder = new GameStateBuilder();
        $builder->withPhase(GamePhase::SETUP);
        $builder->withActiveTeam(TeamSide::HOME);

        // Home: 11 players manually placed (3 on LoS + 8 behind)
        for ($i = 0; $i < 3; $i++) {
            $builder->addPlayer(TeamSide::HOME, 12, 5 + $i, agility: 3, id: $i + 1);
        }
        for ($i = 0; $i < 8; $i++) {
            $builder->addPlayer(TeamSide::HOME, 6, $i + 3, id: $i + 4);
        }

        // Away: 11 players OFF_PITCH (to be auto-setup)
        for ($i = 0; $i < 11; $i++) {
            $builder->addOffPitchPlayer(TeamSide::AWAY, id: 100 + $i);
        }

        $state = $builder->build();

        // Kickoff dice: D8=1, D6=1 (scatter), D6+D6=4+4=8 (kickoff table: Changing Weather), catch roll=6
        $dice = new FixedDiceRoller([1, 1, 4, 4, 3, 3, 6]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::END_SETUP, []);

        // Should proceed to PLAY (auto-setup + kickoff happened)
        $this->assertSame(GamePhase::PLAY, $result->getNewState()->getPhase());

        // All 11 away players should be on pitch
        $awayOnPitch = $result->getNewState()->getPlayersOnPitch(TeamSide::AWAY);
        $this->assertCount(11, $awayOnPitch);
    }

    public function testAutoSetupPlaces3OnLoS(): void
    {
        $builder = new GameStateBuilder();
        $builder->withPhase(GamePhase::SETUP);
        $builder->withActiveTeam(TeamSide::HOME);

        for ($i = 0; $i < 3; $i++) {
            $builder->addPlayer(TeamSide::HOME, 12, 5 + $i, agility: 3, id: $i + 1);
        }
        for ($i = 0; $i < 8; $i++) {
            $builder->addPlayer(TeamSide::HOME, 6, $i + 3, id: $i + 4);
        }

        for ($i = 0; $i < 11; $i++) {
            $builder->addOffPitchPlayer(TeamSide::AWAY, id: 100 + $i);
        }

        $state = $builder->build();
        $dice = new FixedDiceRoller([1, 1, 4, 4, 3, 3, 6]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::END_SETUP, []);
        $newState = $result->getNewState();

        $losCount = 0;
        foreach ($newState->getPlayersOnPitch(TeamSide::AWAY) as $player) {
            $pos = $player->getPosition();
            $this->assertNotNull($pos);
            if ($pos->getX() === 13) {
                $losCount++;
            }
        }
        $this->assertSame(3, $losCount);
    }

    public function testAutoSetupRespectsWideZoneLimits(): void
    {
        $builder = new GameStateBuilder();
        $builder->withPhase(GamePhase::SETUP);
        $builder->withActiveTeam(TeamSide::HOME);

        for ($i = 0; $i < 3; $i++) {
            $builder->addPlayer(TeamSide::HOME, 12, 5 + $i, agility: 3, id: $i + 1);
        }
        for ($i = 0; $i < 8; $i++) {
            $builder->addPlayer(TeamSide::HOME, 6, $i + 3, id: $i + 4);
        }

        for ($i = 0; $i < 11; $i++) {
            $builder->addOffPitchPlayer(TeamSide::AWAY, id: 100 + $i);
        }

        $state = $builder->build();
        $dice = new FixedDiceRoller([1, 1, 4, 4, 3, 3, 6]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::END_SETUP, []);
        $newState = $result->getNewState();

        $topWide = 0;
        $bottomWide = 0;
        foreach ($newState->getPlayersOnPitch(TeamSide::AWAY) as $player) {
            $pos = $player->getPosition();
            $this->assertNotNull($pos);
            $y = $pos->getY();
            if ($y < 4) {
                $topWide++;
            }
            if ($y >= 11) {
                $bottomWide++;
            }
        }
        $this->assertLessThanOrEqual(2, $topWide, 'Max 2 players in top wide zone');
        $this->assertLessThanOrEqual(2, $bottomWide, 'Max 2 players in bottom wide zone');
    }

    public function testAutoSetupHomeFormation(): void
    {
        $builder = new GameStateBuilder();
        $builder->withPhase(GamePhase::SETUP);
        $builder->withActiveTeam(TeamSide::AWAY);

        for ($i = 0; $i < 3; $i++) {
            $builder->addPlayer(TeamSide::AWAY, 13, 5 + $i, agility: 3, id: $i + 1);
        }
        for ($i = 0; $i < 8; $i++) {
            $builder->addPlayer(TeamSide::AWAY, 19, $i + 3, id: $i + 4);
        }

        for ($i = 0; $i < 11; $i++) {
            $builder->addOffPitchPlayer(TeamSide::HOME, id: 100 + $i);
        }

        $state = $builder->build();
        $dice = new FixedDiceRoller([1, 1, 4, 4, 3, 3, 6]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::END_SETUP, []);
        $newState = $result->getNewState();

        $homeOnPitch = $newState->getPlayersOnPitch(TeamSide::HOME);
        $this->assertCount(11, $homeOnPitch);

        $losCount = 0;
        foreach ($homeOnPitch as $player) {
            $pos = $player->getPosition();
            $this->assertNotNull($pos);
            if ($pos->getX() === 12) {
                $losCount++;
            }
        }
        $this->assertSame(3, $losCount);
    }

    public function testSetupPlayerMoveExistingPlayer(): void
    {
        $builder = new GameStateBuilder();
        $builder->withPhase(GamePhase::SETUP);
        $builder->withActiveTeam(TeamSide::HOME);
        $builder->addPlayer(TeamSide::HOME, 5, 5, id: 1);

        $state = $builder->build();
        $resolver = new ActionResolver(new FixedDiceRoller([]));

        $result = $resolver->resolve($state, ActionType::SETUP_PLAYER, [
            'playerId' => 1,
            'x' => 10,
            'y' => 3,
        ]);

        $player = $result->getNewState()->getPlayer(1);
        $this->assertNotNull($player);
        $pos = $player->getPosition();
        $this->assertNotNull($pos);
        $this->assertSame(10, $pos->getX());
        $this->assertSame(3, $pos->getY());
        $this->assertSame(PlayerState::STANDING, $player->getState());
    }
}
