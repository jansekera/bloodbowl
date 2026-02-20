<?php
declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Enum\ActionType;
use App\Enum\GamePhase;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use App\ValueObject\Position;
use PHPUnit\Framework\TestCase;

final class ActionResolverTest extends TestCase
{
    public function testMoveToAdjacentSquare(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 6, id: 1)
            ->build();

        $dice = new FixedDiceRoller([]); // no rolls needed
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1,
            'x' => 6,
            'y' => 5,
        ]);

        $this->assertTrue($result->isSuccess());
        $this->assertFalse($result->isTurnover());

        $movedPlayer = $result->getNewState()->getPlayer(1);
        $this->assertNotNull($movedPlayer);
        $this->assertNotNull($movedPlayer->getPosition());
        $this->assertSame(6, $movedPlayer->getPosition()->getX());
        $this->assertSame(5, $movedPlayer->getPosition()->getY());
        $this->assertTrue($movedPlayer->hasMoved());
    }

    public function testMoveMultipleSquares(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 6, id: 1)
            ->build();

        $dice = new FixedDiceRoller([]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1,
            'x' => 8,
            'y' => 5,
        ]);

        $this->assertTrue($result->isSuccess());
        $player = $result->getNewState()->getPlayer(1);
        $this->assertNotNull($player);
        $this->assertNotNull($player->getPosition());
        $this->assertSame(8, $player->getPosition()->getX());
    }

    public function testSuccessfulDodge(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 6, agility: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 5, 4) // creates tackle zone
            ->build();

        // Roll 4 = success for AG3 dodge (need 4+)
        $dice = new FixedDiceRoller([4]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1,
            'x' => 5,
            'y' => 6,
        ]);

        $this->assertTrue($result->isSuccess());
        $this->assertFalse($result->isTurnover());

        $events = $result->getEvents();
        $dodgeEvent = null;
        foreach ($events as $event) {
            if ($event->getType() === 'dodge') {
                $dodgeEvent = $event;
                break;
            }
        }
        $this->assertNotNull($dodgeEvent);
    }

    public function testFailedDodgeCausesTurnover(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 6, agility: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 5, 4)
            ->build();

        // Roll 1 = failure, team reroll also 1 = failure
        $dice = new FixedDiceRoller([1, 1]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1,
            'x' => 5,
            'y' => 6,
        ]);

        $this->assertFalse($result->isSuccess());
        $this->assertTrue($result->isTurnover());

        $player = $result->getNewState()->getPlayer(1);
        $this->assertNotNull($player);
        $this->assertSame(PlayerState::PRONE, $player->getState());
    }

    public function testSuccessfulGFI(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 1, id: 1)
            ->build();

        // Need to move 2 squares but MA is 1, so second square is GFI
        // GFI needs 2+, roll 2 = success
        $dice = new FixedDiceRoller([2]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1,
            'x' => 7,
            'y' => 5,
        ]);

        $this->assertTrue($result->isSuccess());
    }

    public function testFailedGFICausesTurnover(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 1, id: 1)
            ->build();

        // GFI needs 2+, roll 1 = failure, team reroll also 1 = failure
        $dice = new FixedDiceRoller([1, 1]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1,
            'x' => 7,
            'y' => 5,
        ]);

        $this->assertFalse($result->isSuccess());
        $this->assertTrue($result->isTurnover());

        $player = $result->getNewState()->getPlayer(1);
        $this->assertNotNull($player);
        $this->assertSame(PlayerState::PRONE, $player->getState());
    }

    public function testBallMovesWithCarrier(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 6, id: 1)
            ->withBallCarried(1)
            ->build();

        $dice = new FixedDiceRoller([]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1,
            'x' => 7,
            'y' => 5,
        ]);

        $ball = $result->getNewState()->getBall();
        $this->assertTrue($ball->isHeld());
        $this->assertSame(1, $ball->getCarrierId());
        $this->assertNotNull($ball->getPosition());
        $this->assertSame(7, $ball->getPosition()->getX());
    }

    public function testBallDropsOnFailedDodge(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 6, agility: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 5, 4)
            ->withBallCarried(1)
            ->build();

        $dice = new FixedDiceRoller([1, 1, 3]); // fail dodge, team reroll fail, D8=3 (East) for bounce
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1,
            'x' => 5,
            'y' => 6,
        ]);

        $ball = $result->getNewState()->getBall();
        $this->assertFalse($ball->isHeld());
        $this->assertNull($ball->getCarrierId());
    }

    public function testEndTurnSwitchesActiveTeam(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 15, 5, id: 2)
            ->withActiveTeam(TeamSide::HOME)
            ->build();

        $dice = new FixedDiceRoller([]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::END_TURN, []);

        $this->assertTrue($result->isSuccess());
        $this->assertSame(TeamSide::AWAY, $result->getNewState()->getActiveTeam());
    }

    public function testEndTurnResetsPlayerMovement(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 6, id: 1)
            ->addPlayer(TeamSide::AWAY, 15, 5, movement: 5, id: 2)
            ->withActiveTeam(TeamSide::HOME)
            ->build();

        // Mark home player as moved
        $player = $state->getPlayer(1);
        $this->assertNotNull($player);
        $movedPlayer = $player->withHasMoved(true)->withMovementRemaining(0);
        $state = $state->withPlayer($movedPlayer);

        $dice = new FixedDiceRoller([]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::END_TURN, []);

        // Away player should be reset for the new turn
        $awayPlayer = $result->getNewState()->getPlayer(2);
        $this->assertNotNull($awayPlayer);
        $this->assertFalse($awayPlayer->hasMoved());
        $this->assertSame(5, $awayPlayer->getMovementRemaining());
    }

    public function testSetupPlayerPlacesOnPitch(): void
    {
        $state = (new GameStateBuilder())
            ->withPhase(GamePhase::SETUP)
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->build();

        $dice = new FixedDiceRoller([]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::SETUP_PLAYER, [
            'playerId' => 1,
            'x' => 10,
            'y' => 7,
        ]);

        $player = $result->getNewState()->getPlayer(1);
        $this->assertNotNull($player);
        $this->assertNotNull($player->getPosition());
        $this->assertSame(10, $player->getPosition()->getX());
        $this->assertSame(7, $player->getPosition()->getY());
    }

    public function testSetupPlayerRejectsWrongSide(): void
    {
        $state = (new GameStateBuilder())
            ->withPhase(GamePhase::SETUP)
            ->withActiveTeam(TeamSide::HOME)
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->build();

        $dice = new FixedDiceRoller([]);
        $resolver = new ActionResolver($dice);

        $this->expectException(\InvalidArgumentException::class);
        $resolver->resolve($state, ActionType::SETUP_PLAYER, [
            'playerId' => 1,
            'x' => 15, // away side
            'y' => 7,
        ]);
    }

    public function testMoveGeneratesEvents(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 6, id: 1)
            ->build();

        $dice = new FixedDiceRoller([]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1,
            'x' => 7,
            'y' => 5,
        ]);

        $events = $result->getEvents();
        $this->assertNotEmpty($events);

        // Should have player_move events
        $moveEvents = array_filter($events, fn($e) => $e->getType() === 'player_move');
        $this->assertNotEmpty($moveEvents);
    }

    public function testMoveInvalidPlayerThrows(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->build();

        $dice = new FixedDiceRoller([]);
        $resolver = new ActionResolver($dice);

        $this->expectException(\InvalidArgumentException::class);
        $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 999,
            'x' => 6,
            'y' => 5,
        ]);
    }

    public function testStunnedPlayersRecoverOnNewTurn(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 15, 5, id: 2)
            ->withActiveTeam(TeamSide::HOME)
            ->build();

        // Make away player stunned
        $player = $state->getPlayer(2);
        $this->assertNotNull($player);
        $stunned = $player->withState(PlayerState::STUNNED);
        $state = $state->withPlayer($stunned);

        $dice = new FixedDiceRoller([]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::END_TURN, []);

        // Stunned player should become prone
        $player = $result->getNewState()->getPlayer(2);
        $this->assertNotNull($player);
        $this->assertSame(PlayerState::PRONE, $player->getState());
    }

    // === Block Tests ===

    public function testBlockDefenderDown(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, id: 2)
            ->build();

        // Equal strength = 1 die. Roll 6 = DEFENDER_DOWN
        // Armour: 4+4=8 vs AV8 - not broken
        $dice = new FixedDiceRoller([6, 4, 4]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::BLOCK, [
            'playerId' => 1,
            'targetId' => 2,
        ]);

        $this->assertTrue($result->isSuccess());
        $this->assertFalse($result->isTurnover());

        $defender = $result->getNewState()->getPlayer(2);
        $this->assertNotNull($defender);
        $this->assertSame(PlayerState::PRONE, $defender->getState());
    }

    public function testBlockPushNoKnockdown(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, id: 2)
            ->build();

        // Roll 3 = PUSHED (no knockdown)
        $dice = new FixedDiceRoller([3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::BLOCK, [
            'playerId' => 1,
            'targetId' => 2,
        ]);

        $this->assertTrue($result->isSuccess());

        $defender = $result->getNewState()->getPlayer(2);
        $this->assertNotNull($defender);
        $this->assertSame(PlayerState::STANDING, $defender->getState());
        // Defender pushed to (7,4) - smart push picks closest to sideline
        $this->assertNotNull($defender->getPosition());
        $this->assertSame(7, $defender->getPosition()->getX());
        $this->assertSame(4, $defender->getPosition()->getY());
    }

    public function testBlockAttackerFollowsUp(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, id: 2)
            ->build();

        // Roll 3 = PUSHED
        $dice = new FixedDiceRoller([3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::BLOCK, [
            'playerId' => 1,
            'targetId' => 2,
        ]);

        // Attacker should follow up to defender's old position (6,5)
        $attacker = $result->getNewState()->getPlayer(1);
        $this->assertNotNull($attacker);
        $this->assertNotNull($attacker->getPosition());
        $this->assertSame(6, $attacker->getPosition()->getX());
        $this->assertSame(5, $attacker->getPosition()->getY());
    }

    public function testBlockAttackerDownCausesTurnover(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, id: 2)
            ->build();

        // Roll 1 = ATTACKER_DOWN
        // Armour for attacker: 4+4=8 vs AV8 - not broken
        $dice = new FixedDiceRoller([1, 4, 4]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::BLOCK, [
            'playerId' => 1,
            'targetId' => 2,
        ]);

        $this->assertFalse($result->isSuccess());
        $this->assertTrue($result->isTurnover());

        $attacker = $result->getNewState()->getPlayer(1);
        $this->assertNotNull($attacker);
        $this->assertSame(PlayerState::PRONE, $attacker->getState());
    }

    public function testBlockBothDownWithoutBlockSkill(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, id: 2)
            ->build();

        // Roll 2 = BOTH_DOWN, neither has Block skill
        // Defender armour: 3+3=6 vs AV8 - holds
        // Attacker armour: 3+3=6 vs AV8 - holds
        $dice = new FixedDiceRoller([2, 3, 3, 3, 3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::BLOCK, [
            'playerId' => 1,
            'targetId' => 2,
        ]);

        $this->assertTrue($result->isTurnover()); // attacker went down

        $attacker = $result->getNewState()->getPlayer(1);
        $defender = $result->getNewState()->getPlayer(2);
        $this->assertNotNull($attacker);
        $this->assertNotNull($defender);
        $this->assertSame(PlayerState::PRONE, $attacker->getState());
        $this->assertSame(PlayerState::PRONE, $defender->getState());
    }

    public function testBlockBothDownWithBlockSkillAttacker(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 3, skills: [SkillName::Block], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, id: 2)
            ->build();

        // Roll 2 = BOTH_DOWN, attacker has Block
        // Defender falls, armour roll: 3+3=6 vs AV8 - holds
        $dice = new FixedDiceRoller([2, 3, 3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::BLOCK, [
            'playerId' => 1,
            'targetId' => 2,
        ]);

        $this->assertFalse($result->isTurnover()); // attacker has Block, stays up

        $attacker = $result->getNewState()->getPlayer(1);
        $defender = $result->getNewState()->getPlayer(2);
        $this->assertNotNull($attacker);
        $this->assertNotNull($defender);
        $this->assertSame(PlayerState::STANDING, $attacker->getState());
        $this->assertSame(PlayerState::PRONE, $defender->getState());
    }

    public function testBlockDefenderStumblesWithDodge(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, skills: [SkillName::Dodge], id: 2)
            ->build();

        // Roll 5 = DEFENDER_STUMBLES, but defender has Dodge = just push
        $dice = new FixedDiceRoller([5]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::BLOCK, [
            'playerId' => 1,
            'targetId' => 2,
        ]);

        $this->assertTrue($result->isSuccess());

        $defender = $result->getNewState()->getPlayer(2);
        $this->assertNotNull($defender);
        $this->assertSame(PlayerState::STANDING, $defender->getState()); // Dodge prevents knockdown
        $this->assertNotNull($defender->getPosition());
        $this->assertSame(7, $defender->getPosition()->getX()); // pushed
    }

    public function testBlockDefenderStumblesWithoutDodge(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, id: 2)
            ->build();

        // Roll 5 = DEFENDER_STUMBLES, no Dodge = knockdown
        // Armour: 3+3=6 vs AV8 - holds
        $dice = new FixedDiceRoller([5, 3, 3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::BLOCK, [
            'playerId' => 1,
            'targetId' => 2,
        ]);

        $defender = $result->getNewState()->getPlayer(2);
        $this->assertNotNull($defender);
        $this->assertSame(PlayerState::PRONE, $defender->getState());
    }

    public function testBlock2DiceAttackerChoosesBest(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 4, id: 1) // ST4 > ST3
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, id: 2)
            ->build();

        // 2 dice, attacker chooses: roll 1 (ATTACKER_DOWN), 6 (DEFENDER_DOWN)
        // Auto-choose DEFENDER_DOWN (score 100 vs -100)
        // Armour: 3+3=6 vs AV8 - holds
        $dice = new FixedDiceRoller([1, 6, 3, 3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::BLOCK, [
            'playerId' => 1,
            'targetId' => 2,
        ]);

        $this->assertTrue($result->isSuccess());
        $this->assertFalse($result->isTurnover());

        $defender = $result->getNewState()->getPlayer(2);
        $this->assertNotNull($defender);
        $this->assertSame(PlayerState::PRONE, $defender->getState());
    }

    public function testBlock2DiceDefenderChoosesWorst(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 4, id: 2) // ST4 > ST3
            ->build();

        // 2 dice, defender chooses: roll 6 (DEFENDER_DOWN), 1 (ATTACKER_DOWN)
        // Defender picks ATTACKER_DOWN (worst for attacker, score -100)
        // Attacker armour: 3+3=6 vs AV8 - holds
        $dice = new FixedDiceRoller([6, 1, 3, 3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::BLOCK, [
            'playerId' => 1,
            'targetId' => 2,
        ]);

        $this->assertTrue($result->isTurnover());

        $attacker = $result->getNewState()->getPlayer(1);
        $this->assertNotNull($attacker);
        $this->assertSame(PlayerState::PRONE, $attacker->getState());
    }

    public function testBlockWithArmourBreak(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, armour: 7, id: 2)
            ->build();

        // Roll 6 = DEFENDER_DOWN
        // Armour: 5+4=9 > AV7 - broken
        // Injury: 3+3=6 - stunned
        $dice = new FixedDiceRoller([6, 5, 4, 3, 3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::BLOCK, [
            'playerId' => 1,
            'targetId' => 2,
        ]);

        $defender = $result->getNewState()->getPlayer(2);
        $this->assertNotNull($defender);
        $this->assertSame(PlayerState::STUNNED, $defender->getState());

        // Check events contain armour and injury rolls
        $eventTypes = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('armour_roll', $eventTypes);
        $this->assertContains('injury_roll', $eventTypes);
    }

    public function testBlockWithMightyBlow(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 3, skills: [SkillName::MightyBlow], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, armour: 8, id: 2)
            ->build();

        // Roll 6 = DEFENDER_DOWN
        // Armour: 4+4=8 +1(MB)=9 > AV8 - broken!
        // Injury: 3+3=6 - stunned
        $dice = new FixedDiceRoller([6, 4, 4, 3, 3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::BLOCK, [
            'playerId' => 1,
            'targetId' => 2,
        ]);

        $defender = $result->getNewState()->getPlayer(2);
        $this->assertNotNull($defender);
        $this->assertSame(PlayerState::STUNNED, $defender->getState());
    }

    public function testBlockBallDropsOnDefenderDown(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, id: 2)
            ->withBallCarried(2)
            ->build();

        // Roll 6 = DEFENDER_DOWN
        // Armour: 3+3=6 vs AV8 - holds
        // D8=1 (North) for ball bounce
        $dice = new FixedDiceRoller([6, 3, 3, 1]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::BLOCK, [
            'playerId' => 1,
            'targetId' => 2,
        ]);

        $ball = $result->getNewState()->getBall();
        $this->assertFalse($ball->isHeld());
        $this->assertNull($ball->getCarrierId());
    }

    public function testBlockCrowdSurf(): void
    {
        // Defender at edge of pitch
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 24, 5, strength: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 25, 5, strength: 3, id: 2) // at right edge (x=25, max is 25)
            ->build();

        // Roll 3 = PUSHED, pushed off pitch
        // Crowd injury: 3+3=6 +1=7 - stunned
        $dice = new FixedDiceRoller([3, 3, 3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::BLOCK, [
            'playerId' => 1,
            'targetId' => 2,
        ]);

        $defender = $result->getNewState()->getPlayer(2);
        $this->assertNotNull($defender);
        $this->assertNull($defender->getPosition()); // removed from pitch

        $eventTypes = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('crowd_surf', $eventTypes);
    }

    public function testBlockInvalidNotAdjacent(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 8, 5, strength: 3, id: 2)
            ->build();

        $dice = new FixedDiceRoller([]);
        $resolver = new ActionResolver($dice);

        $this->expectException(\InvalidArgumentException::class);
        $resolver->resolve($state, ActionType::BLOCK, [
            'playerId' => 1,
            'targetId' => 2,
        ]);
    }

    public function testBlockMarksPlayerAsActed(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, id: 2)
            ->build();

        // Roll 3 = PUSHED
        $dice = new FixedDiceRoller([3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::BLOCK, [
            'playerId' => 1,
            'targetId' => 2,
        ]);

        $attacker = $result->getNewState()->getPlayer(1);
        $this->assertNotNull($attacker);
        $this->assertTrue($attacker->hasActed());
    }

    // === Blitz Tests ===

    public function testBlitzFromAdjacentSquare(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 3, movement: 6, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, id: 2)
            ->build();

        // Already adjacent, so just block: roll 6 = DEFENDER_DOWN
        // Armour: 3+3=6 vs AV8 - holds
        $dice = new FixedDiceRoller([6, 3, 3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::BLITZ, [
            'playerId' => 1,
            'targetId' => 2,
        ]);

        $this->assertTrue($result->isSuccess());
        $this->assertFalse($result->isTurnover());

        // Check blitz was marked
        $teamState = $result->getNewState()->getTeamState(TeamSide::HOME);
        $this->assertTrue($teamState->isBlitzUsedThisTurn());
    }

    public function testBlitzWithMovement(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 3, 5, strength: 3, movement: 6, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, id: 2)
            ->build();

        // Move from (3,5) to adjacent to (6,5) = (5,5), then block
        // Block roll 3 = PUSHED
        $dice = new FixedDiceRoller([3]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::BLITZ, [
            'playerId' => 1,
            'targetId' => 2,
        ]);

        $this->assertTrue($result->isSuccess());

        // Check there are move events followed by block events
        $eventTypes = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('player_move', $eventTypes);
        $this->assertContains('block', $eventTypes);
    }

    public function testBlitzFailedMoveCausesTurnover(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 3, 5, strength: 3, movement: 6, agility: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, id: 2)
            ->addPlayer(TeamSide::AWAY, 3, 4, strength: 3, id: 3) // marks blitzer
            ->build();

        // Dodge needed to leave TZ, roll 1 = fail, team reroll also 1 = fail
        $dice = new FixedDiceRoller([1, 1]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::BLITZ, [
            'playerId' => 1,
            'targetId' => 2,
        ]);

        $this->assertTrue($result->isTurnover());
    }

    public function testBlitzMarksBlitzUsed(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, id: 2)
            ->build();

        $dice = new FixedDiceRoller([3]); // PUSHED
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::BLITZ, [
            'playerId' => 1,
            'targetId' => 2,
        ]);

        $teamState = $result->getNewState()->getTeamState(TeamSide::HOME);
        $this->assertTrue($teamState->isBlitzUsedThisTurn());
    }

    // --- Pickup on move tests ---

    public function testMoveOntoLooseBallPicksUp(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 6, agility: 3, id: 1)
            ->withBallOnGround(6, 5)
            ->build();

        // Pickup: AG3, target=3+, roll 4 = success
        $dice = new FixedDiceRoller([4]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1,
            'x' => 6,
            'y' => 5,
        ]);

        $this->assertTrue($result->isSuccess());
        $this->assertTrue($result->getNewState()->getBall()->isHeld());
        $this->assertEquals(1, $result->getNewState()->getBall()->getCarrierId());
    }

    public function testMoveOntoLooseBallPickupFailsTurnover(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 6, agility: 3, id: 1)
            ->withBallOnGround(6, 5)
            ->build();

        // Pickup: target=3+, roll 2 = fail, team reroll 2 = fail, D8=1 (North) for bounce
        $dice = new FixedDiceRoller([2, 2, 1]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1,
            'x' => 6,
            'y' => 5,
        ]);

        $this->assertTrue($result->isTurnover());
        $this->assertFalse($result->getNewState()->getBall()->isHeld());
    }

    public function testMoveOntoLooseBallSureHandsReroll(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 6, agility: 3, skills: [SkillName::SureHands], id: 1)
            ->withBallOnGround(6, 5)
            ->build();

        // Pickup: target=3+, roll 2 = fail, Sure Hands reroll 5 = success
        $dice = new FixedDiceRoller([2, 5]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1,
            'x' => 6,
            'y' => 5,
        ]);

        $this->assertTrue($result->isSuccess());
        $this->assertTrue($result->getNewState()->getBall()->isHeld());
    }

    // --- Hand-off tests ---

    public function testHandOffSuccess(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 6, 5, agility: 3, id: 2)
            ->withBallCarried(1)
            ->build();

        // Catch: 7-3 = 4+, +1 hand-off = 3+, roll 4 = success
        $dice = new FixedDiceRoller([4]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::HAND_OFF, [
            'playerId' => 1,
            'targetId' => 2,
        ]);

        $this->assertTrue($result->isSuccess());
        $this->assertTrue($result->getNewState()->getBall()->isHeld());
        $this->assertEquals(2, $result->getNewState()->getBall()->getCarrierId());
    }

    public function testHandOffFailTurnover(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 6, 5, agility: 3, id: 2)
            ->withBallCarried(1)
            ->build();

        // Catch: 3+, roll 2 = fail, team reroll 2 = fail, D8=1 (North) for bounce
        $dice = new FixedDiceRoller([2, 2, 1]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::HAND_OFF, [
            'playerId' => 1,
            'targetId' => 2,
        ]);

        $this->assertTrue($result->isTurnover());
        $this->assertFalse($result->getNewState()->getBall()->isHeld());
    }

    public function testHandOffMarksGiverAsActed(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 6, 5, agility: 3, id: 2)
            ->withBallCarried(1)
            ->build();

        $dice = new FixedDiceRoller([5]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::HAND_OFF, [
            'playerId' => 1,
            'targetId' => 2,
        ]);

        $giver = $result->getNewState()->getPlayer(1);
        $this->assertNotNull($giver);
        $this->assertTrue($giver->hasActed());
    }
}
