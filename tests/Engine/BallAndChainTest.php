<?php
declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Enum\ActionType;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use App\Engine\RulesEngine;
use PHPUnit\Framework\TestCase;

final class BallAndChainTest extends TestCase
{
    /**
     * B&C player moves randomly using D8 scatter for each MA point.
     * MA=3, rolls: D8=3(E), D8=3(E), D8=3(E) → moves 3 squares East.
     */
    public function testMovementUsesThrowInTemplate(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 3, id: 1, skills: [SkillName::BallAndChain])
            ->withBallOffPitch()
            ->build();

        // Sablona + D6 (r. 7829-7833): bez soupere vychozi natoceni +x, D6 3 = rovne
        $dice = new FixedDiceRoller([3, 3, 3]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BALL_AND_CHAIN, ['playerId' => 1]);

        $this->assertTrue($result->isSuccess());
        $this->assertFalse($result->isTurnover());

        $newState = $result->getNewState();
        $pos = $newState->requirePlayer(1)->requirePosition();
        $this->assertEquals(8, $pos->getX());
        $this->assertEquals(7, $pos->getY());

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $moveCount = count(array_filter($types, fn($t) => $t === 'ball_and_chain_move'));
        $this->assertEquals(3, $moveCount);
    }

    /**
     * Auto-block when B&C moves into occupied square.
     */
    /** Blokuje ze SVEHO pole a po odtlaceni povinne postoupi (r. 7840-7847). */
    public function testAutoBlockFromOwnSquareThenFollowUp(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 1, id: 1, skills: [SkillName::BallAndChain])
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2)
            ->withBallOffPitch()
            ->build();

        // D6 3 => rovne na (6,7); ST 3 proti 3 = 1 kostka: 6 = Defender Down; brneni 4+4
        $result = (new ActionResolver(new FixedDiceRoller([3, 6, 4, 4])))->resolve($state, ActionType::BALL_AND_CHAIN, ['playerId' => 1]);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('ball_and_chain_block', $types);
        $bnc = $result->getNewState()->requirePlayer(1)->requirePosition();
        $obet = $result->getNewState()->requirePlayer(2)->requirePosition();
        $this->assertSame([6, 7], [$bnc->getX(), $bnc->getY()], 'follow-up na uvolnene pole');
        $this->assertSame([7, 7], [$obet->getX(), $obet->getY()], 'odtlacen od pole B&C, ne z tehoz pole');
    }

    /** Silny B&C hazi kostky podle sily (r. 7840-7842): ST 7 proti 3 = 3 kostky, voli utocnik. */
    public function testBlockUsesStrengthDice(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 1, strength: 7, id: 1, skills: [SkillName::BallAndChain])
            ->addPlayer(TeamSide::AWAY, 6, 7, strength: 3, id: 2)
            ->withBallOffPitch()
            ->build();

        // D6 3; tri kostky 1, 1, 6 => B&C vybere Defender Down; brneni 4+4
        $result = (new ActionResolver(new FixedDiceRoller([3, 1, 1, 6, 4, 4])))->resolve($state, ActionType::BALL_AND_CHAIN, ['playerId' => 1]);

        $this->assertSame(PlayerState::STANDING, $result->getNewState()->requirePlayer(1)->getState(), 's jednou kostkou (1) by lezel');
        $this->assertSame(PlayerState::PRONE, $result->getNewState()->requirePlayer(2)->getState());
    }

    /** Lezici v ceste: odtlacit + brneni misto bloku (r. 7843-7845), pak follow-up. */
    public function testProneOccupantIsPushedAndArmourRolled(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 1, id: 1, skills: [SkillName::BallAndChain])
            ->addPronePlayer(TeamSide::AWAY, 6, 7, id: 2)
            ->addPlayer(TeamSide::AWAY, 9, 7, id: 3) // stojici souper v +x => natoceni +x
            ->withBallOffPitch()
            ->build();

        // D6 3 => rovne na (6,7) s lezicim; brneni 6+6 prolomeno; zraneni 3+3 => stunned
        $result = (new ActionResolver(new FixedDiceRoller([3, 6, 6, 3, 3])))->resolve($state, ActionType::BALL_AND_CHAIN, ['playerId' => 1]);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertNotContains('block', $types, 'na lezici se neblokuje');
        $obet = $result->getNewState()->requirePlayer(2);
        $this->assertSame(PlayerState::STUNNED, $obet->getState());
        $this->assertSame([7, 7], [$obet->requirePosition()->getX(), $obet->requirePosition()->getY()]);
    }

    /**
     * B&C player moving off-pitch is KO'd.
     */
    /** Mimo hriste = dav jako po vytlaceni (r. 7835-7837): hod na zraneni, Stunned = KO (r. 7849-7850). */
    public function testOffPitchIsCrowdInjuryAndStunnedCountsAsKo(): void
    {
        // Roh (0,0): kazde natoceni ma aspon jedno pole mimo; vychozi +x, D6 1 = (1,-1) mimo
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 0, 0, movement: 3, id: 1, skills: [SkillName::BallAndChain])
            ->withBallOffPitch()
            ->build();

        // D6 1 => dav; zraneni 3+3 = 6 => Stunned => u B&C KO
        $result = (new ActionResolver(new FixedDiceRoller([1, 3, 3])))->resolve($state, ActionType::BALL_AND_CHAIN, ['playerId' => 1]);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('crowd_surf', $types);
        $this->assertContains('injury_roll', $types, 'dav hazi na zraneni, ne automaticke KO');
        $player = $result->getNewState()->requirePlayer(1);
        $this->assertSame(PlayerState::KO, $player->getState());
        $this->assertNull($player->getPosition());
        $this->assertFalse($result->isTurnover(), 'zraneni davem neni turnover (r. 369-370)');
    }

    /** Natoceni k nejblizsimu stojicimu souperi (rozhodnuti uzivatele 25.09.). */
    public function testTemplateFacesNearestStandingOpponent(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 1, id: 1, skills: [SkillName::BallAndChain])
            ->addPlayer(TeamSide::AWAY, 5, 11, id: 2)
            ->withBallOffPitch()
            ->build();

        // D6 3 = rovne ve smeru natoceni; soupere je v +y => (5,8)
        $result = (new ActionResolver(new FixedDiceRoller([3])))->resolve($state, ActionType::BALL_AND_CHAIN, ['playerId' => 1]);

        $pos = $result->getNewState()->requirePlayer(1)->requirePosition();
        $this->assertSame([5, 8], [$pos->getX(), $pos->getY()]);
    }

    /** ... a vyhnout se nasim stojicim (rozhodnuti uzivatele 25.09.). */
    public function testTemplateAvoidsOwnStandingPlayers(): void
    {
        // soupere v +x, ale primo pred nim nas hrac => rovne do +x by blokoval vlastniho
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 1, id: 1, skills: [SkillName::BallAndChain])
            ->addPlayer(TeamSide::HOME, 6, 7, id: 2)
            ->addPlayer(TeamSide::AWAY, 9, 7, id: 3)
            ->withBallOffPitch()
            ->build();

        $result = (new ActionResolver(new FixedDiceRoller([3])))->resolve($state, ActionType::BALL_AND_CHAIN, ['playerId' => 1]);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertNotContains('ball_and_chain_block', $types, 'nesmi narazit do vlastniho');
    }

    /** Sraženy pri bloku: rovnou zraneni BEZ brneni (r. 7848-7849) a turnover (r. 368). */
    public function testKnockedDownBallAndChainRollsInjuryWithoutArmourAndTurnsOver(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 1, id: 1, skills: [SkillName::BallAndChain])
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2)
            ->withBallOffPitch()
            ->build();

        // D6 3 => rovne do (6,7) na soupere; blok 1 = Attacker Down; zraneni 4+4 = 8 => KO
        $result = (new ActionResolver(new FixedDiceRoller([3, 1, 4, 4])))->resolve($state, ActionType::BALL_AND_CHAIN, ['playerId' => 1]);

        $brneniBnc = array_filter($result->getEvents(),
            fn($e) => $e->getType() === 'armour_roll' && ($e->getData()['playerId'] ?? null) === 1);
        $this->assertSame([], $brneniBnc, 'u B&C se na brneni nehazi');
        $this->assertSame(PlayerState::KO, $result->getNewState()->requirePlayer(1)->getState());
        $this->assertTrue($result->isTurnover());
    }

    /**
     * B&C players can ONLY use BALL_AND_CHAIN action.
     */
    public function testCannotTakeNormalActions(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, skills: [SkillName::BallAndChain])
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2) // adjacent enemy
            ->withBallOffPitch()
            ->build();

        $rulesEngine = new RulesEngine();
        $actions = $rulesEngine->getAvailableActions($state);

        $player1Actions = array_filter($actions, fn($a) => ($a['playerId'] ?? null) === 1);
        $types = array_map(fn($a) => $a['type'], $player1Actions);

        $this->assertContains('ball_and_chain', $types);
        $this->assertNotContains('move', $types);
        $this->assertNotContains('block', $types);
    }

    /**
     * Ball bounces when B&C player enters ball square (cannot pick up).
     */
    public function testBallBouncesOnBallSquare(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 1, id: 1, skills: [SkillName::BallAndChain])
            ->withBallOnGround(6, 7) // ball on square B&C will move to
            ->build();

        // D8=3 (East) → moves to (6,7) where ball is
        // Ball bounce: D8=1 (North) → (6,6)
        $dice = new FixedDiceRoller([3, 1]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BALL_AND_CHAIN, ['playerId' => 1]);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('ball_bounce', $types);

        $newState = $result->getNewState();
        // Ball should not be carried by B&C player
        $this->assertNotEquals(1, $newState->getBall()->getCarrierId());
    }

    /**
     * B&C player is marked as acted after movement.
     */
    public function testMarkedAsActedAfterMovement(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 1, id: 1, skills: [SkillName::BallAndChain])
            ->withBallOffPitch()
            ->build();

        $dice = new FixedDiceRoller([3]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BALL_AND_CHAIN, ['playerId' => 1]);

        $player = $result->getNewState()->requirePlayer(1);
        $this->assertTrue($player->hasActed());
        $this->assertTrue($player->hasMoved());
    }
}
