<?php
declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\ActionResolver;
use App\Engine\RulesEngine;
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
        $original = $state->requirePlayer(1);
        $this->assertSame(PlayerState::OFF_PITCH, $original->getState());

        $resolver = new ActionResolver(new FixedDiceRoller([]));
        $result = $resolver->resolve($state, ActionType::SETUP_PLAYER, [
            'playerId' => 1,
            'x' => 5,
            'y' => 7,
        ]);

        $player = $result->getNewState()->requirePlayer(1);
        $pos = $player->requirePosition();
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
            $pos = $player->requirePosition();
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
            $pos = $player->requirePosition();
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
            $pos = $player->requirePosition();
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

        $player = $result->getNewState()->requirePlayer(1);
        $pos = $player->requirePosition();
        $this->assertSame(10, $pos->getX());
        $this->assertSame(3, $pos->getY());
        $this->assertSame(PlayerState::STANDING, $player->getState());
    }

    // ============================================================
    // ⭐ BALIK G 24.09.2026 -- soupiska az 16 hracu, na hristi jen 11.
    //   r. 308-309: "Each coach must set up 11 players, or if they can't
    //   field 11 then as many players as they have in Reserves."
    // ============================================================

    /**
     * ⛔ Dvanacty hrac na hriste nesmi. Zbytek soupisky ceka v rezervach.
     */
    public function testCannotSetUpTwelfthPlayer(): void
    {
        $builder = new GameStateBuilder();
        $builder->withPhase(GamePhase::SETUP);
        $builder->withActiveTeam(TeamSide::HOME);

        for ($i = 0; $i < 11; $i++) {
            $builder->addPlayer(TeamSide::HOME, 6, $i + 1, id: $i + 1);
        }
        $builder->addOffPitchPlayer(TeamSide::HOME, id: 12);

        $state = $builder->build();
        $this->assertCount(11, $state->getPlayersOnPitch(TeamSide::HOME));

        $resolver = new ActionResolver(new FixedDiceRoller([]));

        $this->expectException(\InvalidArgumentException::class);
        $this->expectExceptionMessageMatches('/more than 11 players/');
        $resolver->resolve($state, ActionType::SETUP_PLAYER, [
            'playerId' => 12, 'x' => 8, 'y' => 7,
        ]);
    }

    /**
     * ⭐⭐⭐ POZITIVNI KONTROLA k testu vyse: pri DESETI na hristi se
     * jedenacty postavit MUSI. Bez tohohle by "vyhozena vyjimka" mohla
     * znamenat i to, ze rozestavovani nefunguje vubec.
     */
    public function testEleventhPlayerStillGetsSetUp(): void
    {
        $builder = new GameStateBuilder();
        $builder->withPhase(GamePhase::SETUP);
        $builder->withActiveTeam(TeamSide::HOME);

        for ($i = 0; $i < 10; $i++) {
            $builder->addPlayer(TeamSide::HOME, 6, $i + 1, id: $i + 1);
        }
        $builder->addOffPitchPlayer(TeamSide::HOME, id: 11);

        $state = $builder->build();
        $resolver = new ActionResolver(new FixedDiceRoller([]));

        $result = $resolver->resolve($state, ActionType::SETUP_PLAYER, [
            'playerId' => 11, 'x' => 8, 'y' => 7,
        ]);

        $this->assertCount(11, $result->getNewState()->getPlayersOnPitch(TeamSide::HOME));
        $this->assertSame(
            PlayerState::STANDING,
            $result->getNewState()->getPlayer(11)?->getState(),
        );
    }

    /**
     * ⭐ Strop se NEVZTAHUJE na prestaveni hrace, ktery uz na hristi stoji --
     * pocet se tim nezvysi.
     */
    public function testRepositioningIsAllowedAtEleven(): void
    {
        $builder = new GameStateBuilder();
        $builder->withPhase(GamePhase::SETUP);
        $builder->withActiveTeam(TeamSide::HOME);

        for ($i = 0; $i < 11; $i++) {
            $builder->addPlayer(TeamSide::HOME, 6, $i + 1, id: $i + 1);
        }

        $state = $builder->build();
        $resolver = new ActionResolver(new FixedDiceRoller([]));

        $result = $resolver->resolve($state, ActionType::SETUP_PLAYER, [
            'playerId' => 1, 'x' => 9, 'y' => 13,
        ]);

        $this->assertCount(11, $result->getNewState()->getPlayersOnPitch(TeamSide::HOME));
        $pos = $result->getNewState()->getPlayer(1)?->getPosition();
        $this->assertNotNull($pos);
        $this->assertSame(9, $pos->getX());
        $this->assertSame(13, $pos->getY());
    }

    /**
     * ⛔ Ukonceni rozestaveni s dvanacti na hristi se musi odmitnout.
     * (Stav se da vyrobit jen obchvatem validace, proto se stavi primo.)
     */
    public function testEndSetupRejectsTwelveOnPitch(): void
    {
        $builder = new GameStateBuilder();
        $builder->withPhase(GamePhase::SETUP);
        $builder->withActiveTeam(TeamSide::HOME);

        for ($i = 0; $i < 3; $i++) {
            $builder->addPlayer(TeamSide::HOME, 12, 5 + $i, id: $i + 1);
        }
        for ($i = 0; $i < 9; $i++) {
            $builder->addPlayer(TeamSide::HOME, 6, $i + 3, id: $i + 4);
        }
        for ($i = 0; $i < 11; $i++) {
            $builder->addOffPitchPlayer(TeamSide::AWAY, id: 100 + $i);
        }

        $state = $builder->build();
        $this->assertCount(12, $state->getPlayersOnPitch(TeamSide::HOME));

        $resolver = new ActionResolver(new FixedDiceRoller([1, 1, 4, 4, 3, 3, 6]));

        $this->expectException(\InvalidArgumentException::class);
        $this->expectExceptionMessageMatches('/more than 11 players/');
        $resolver->resolve($state, ActionType::END_SETUP, []);
    }

    /**
     * ⭐⭐ DRUHA VADA, nalezena pri tehle oprave: `RulesEngine::validateEndSetup`
     * mel natvrdo `< 11`, zatimco handler uz pocital `min(11, dostupni)`.
     * Tym, ktery po zranenich jedenact hracu NEMA, tak neslo rozestavet.
     * Pravidlo r. 308-309 s tim vyslovne pocita.
     */
    public function testEndSetupAllowsFewerWhenTeamCannotFieldEleven(): void
    {
        $builder = new GameStateBuilder();
        $builder->withPhase(GamePhase::SETUP);
        $builder->withActiveTeam(TeamSide::HOME);

        // Cela soupiska je devet hracu -- jedenact proste nema kde vzit.
        for ($i = 0; $i < 3; $i++) {
            $builder->addPlayer(TeamSide::HOME, 12, 5 + $i, id: $i + 1);
        }
        for ($i = 0; $i < 6; $i++) {
            $builder->addPlayer(TeamSide::HOME, 6, $i + 3, id: $i + 4);
        }
        for ($i = 0; $i < 11; $i++) {
            $builder->addOffPitchPlayer(TeamSide::AWAY, id: 100 + $i);
        }

        $state = $builder->build();
        $this->assertCount(9, $state->getPlayersOnPitch(TeamSide::HOME));

        $resolver = new ActionResolver(new FixedDiceRoller([1, 1, 4, 4, 3, 3, 6]));
        $result = $resolver->resolve($state, ActionType::END_SETUP, []);

        $this->assertSame(GamePhase::PLAY, $result->getNewState()->getPhase());
        $this->assertCount(9, $result->getNewState()->getPlayersOnPitch(TeamSide::HOME));
    }

    // ------------------------------------------------------------
    // ⚠️ 24.09.2026 -- tyhle testy jdou PRIMO na `RulesEngine::validate`.
    //   Duvod: `ActionResolver` vola handler rovnou, takze testy vyse
    //   validacni vrstvu MIJEJI. Na zive ceste ji ale vola
    //   `GameOrchestrator:50`, tedy webova hra -- a tam vada byla.
    //   (Zjisteno tim, ze test "min. pocet" prosel i s vadnym kodem.)
    // ------------------------------------------------------------

    public function testValidateRejectsTwelfthPlayerOnPitch(): void
    {
        $builder = new GameStateBuilder();
        $builder->withPhase(GamePhase::SETUP);
        $builder->withActiveTeam(TeamSide::HOME);
        for ($i = 0; $i < 11; $i++) {
            $builder->addPlayer(TeamSide::HOME, 6, $i + 1, id: $i + 1);
        }
        $builder->addOffPitchPlayer(TeamSide::HOME, id: 12);

        $errors = (new RulesEngine())->validate($builder->build(), ActionType::SETUP_PLAYER, [
            'playerId' => 12, 'x' => 8, 'y' => 7,
        ]);

        $this->assertNotEmpty($errors);
        $this->assertStringContainsString('more than 11 players', implode(' | ', $errors));
    }

    /** ⭐⭐⭐ POZITIVNI KONTROLA: pri desiti musi validace jedenacteho PUSTIT. */
    public function testValidateAllowsEleventhPlayerOnPitch(): void
    {
        $builder = new GameStateBuilder();
        $builder->withPhase(GamePhase::SETUP);
        $builder->withActiveTeam(TeamSide::HOME);
        for ($i = 0; $i < 10; $i++) {
            $builder->addPlayer(TeamSide::HOME, 6, $i + 1, id: $i + 1);
        }
        $builder->addOffPitchPlayer(TeamSide::HOME, id: 11);

        $errors = (new RulesEngine())->validate($builder->build(), ActionType::SETUP_PLAYER, [
            'playerId' => 11, 'x' => 8, 'y' => 7,
        ]);

        $this->assertSame([], $errors);
    }

    public function testValidateEndSetupRejectsTwelveOnPitch(): void
    {
        $builder = new GameStateBuilder();
        $builder->withPhase(GamePhase::SETUP);
        $builder->withActiveTeam(TeamSide::HOME);
        for ($i = 0; $i < 3; $i++) {
            $builder->addPlayer(TeamSide::HOME, 12, 5 + $i, id: $i + 1);
        }
        for ($i = 0; $i < 9; $i++) {
            $builder->addPlayer(TeamSide::HOME, 6, $i + 3, id: $i + 4);
        }

        $errors = (new RulesEngine())->validate($builder->build(), ActionType::END_SETUP, []);

        $this->assertStringContainsString('more than 11 players', implode(' | ', $errors));
    }

    /**
     * ⭐⭐ DRUHA VADA: validace mela minimum natvrdo `< 11`, zatimco handler
     * uz pocital `min(11, dostupni)`. Tym po zranenich tak neprosel validaci
     * a rozestaveni neslo ukoncit. Pravidlo r. 308-309 oba pripady rozlisuje.
     */
    public function testValidateEndSetupAllowsFewerWhenTeamCannotFieldEleven(): void
    {
        $builder = new GameStateBuilder();
        $builder->withPhase(GamePhase::SETUP);
        $builder->withActiveTeam(TeamSide::HOME);
        // Cela soupiska devet hracu, vsichni na hristi.
        for ($i = 0; $i < 3; $i++) {
            $builder->addPlayer(TeamSide::HOME, 12, 5 + $i, id: $i + 1);
        }
        for ($i = 0; $i < 6; $i++) {
            $builder->addPlayer(TeamSide::HOME, 6, $i + 3, id: $i + 4);
        }

        $errors = (new RulesEngine())->validate($builder->build(), ActionType::END_SETUP, []);

        $this->assertSame([], $errors, 'Deviticlenny tym musi rozestaveni ukoncit: ' . implode(' | ', $errors));
    }

    // ------------------------------------------------------------
    // ⭐ BALIK G 24.09.2026, 4/4 -- Sweltering Heat: priznak `outNextSetup`
    //   (r. 1477-1481 "may not be set up for the next kick-off").
    // ------------------------------------------------------------

    public function testCollapsedPlayerCannotBeSetUp(): void
    {
        $builder = new GameStateBuilder();
        $builder->withPhase(GamePhase::SETUP);
        $builder->withActiveTeam(TeamSide::HOME);
        $builder->addOffPitchPlayer(TeamSide::HOME, id: 1);
        $state = $builder->build();
        $state = $state->withPlayer($state->requirePlayer(1)->withOutNextSetup(true));

        $errors = (new RulesEngine())->validate($state, ActionType::SETUP_PLAYER, [
            'playerId' => 1, 'x' => 6, 'y' => 7,
        ]);

        $this->assertStringContainsString('Sweltering Heat', implode(' | ', $errors));
    }

    /** ⭐⭐⭐ POZITIVNI KONTROLA: tentyz hrac BEZ priznaku projit musi. */
    public function testSamePlayerWithoutFlagCanBeSetUp(): void
    {
        $builder = new GameStateBuilder();
        $builder->withPhase(GamePhase::SETUP);
        $builder->withActiveTeam(TeamSide::HOME);
        $builder->addOffPitchPlayer(TeamSide::HOME, id: 1);

        $errors = (new RulesEngine())->validate($builder->build(), ActionType::SETUP_PLAYER, [
            'playerId' => 1, 'x' => 6, 'y' => 7,
        ]);

        $this->assertSame([], $errors);
    }

    /**
     * ⭐ Priznak vydrzi PRESNE JEDNO rozestaveni: automaticke rozestaveni
     * zkolabovaneho vynecha a zaroven mu priznak spotrebuje, takze priste
     * uz nastoupi.
     */
    public function testCollapsedPlayerIsSkippedAndFlagIsConsumed(): void
    {
        $builder = new GameStateBuilder();
        $builder->withPhase(GamePhase::SETUP);
        $builder->withActiveTeam(TeamSide::HOME);
        for ($i = 0; $i < 3; $i++) {
            $builder->addPlayer(TeamSide::HOME, 12, 5 + $i, id: $i + 1);
        }
        for ($i = 0; $i < 8; $i++) {
            $builder->addPlayer(TeamSide::HOME, 6, $i + 3, id: $i + 4);
        }
        // Away ma DVANACT v rezervach, jeden z nich zkolaboval.
        for ($i = 0; $i < 12; $i++) {
            $builder->addOffPitchPlayer(TeamSide::AWAY, id: 100 + $i);
        }

        $state = $builder->build();
        $state = $state->withPlayer($state->requirePlayer(100)->withOutNextSetup(true));

        $resolver = new ActionResolver(new FixedDiceRoller([1, 1, 4, 4, 3, 3, 6]));
        $newState = $resolver->resolve($state, ActionType::END_SETUP, [])->getNewState();

        // Zkolabovany zustal v rezervach...
        $this->assertSame(PlayerState::OFF_PITCH, $newState->requirePlayer(100)->getState());
        // ...ale jedenact ostatnich stoji, takze rozestaveni probehlo.
        $this->assertCount(11, $newState->getPlayersOnPitch(TeamSide::AWAY));
        // ...a priznak je spotrebovany, takze priste uz nastoupi.
        $this->assertFalse(
            $newState->requirePlayer(100)->isOutNextSetup(),
            'Priznak musi vydrzet presne jedno rozestaveni',
        );
    }
}
