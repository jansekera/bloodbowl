<?php
declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\PendingBlockDTO;
use App\Engine\ActionResolver;
use App\Engine\DiceRollerInterface;
use App\Engine\FixedDiceRoller;
use App\Enum\ActionType;
use App\Enum\BlockDiceFace;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

final class BlockRerollTest extends TestCase
{
    // ========== Pending Block Creation ==========

    public function testBlockCreatessPendingBlockInInteractiveMode(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, id: 2)
            ->build();

        // Roll 6 = DEFENDER_DOWN
        $dice = new FixedDiceRoller([6]);
        $resolver = new ActionResolver($dice);
        $resolver->setInteractiveBlocks(true);

        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $this->assertNotNull($result->getNewState()->getPendingBlock());
        $pending = $result->getNewState()->getPendingBlock();
        $this->assertSame(1, $pending->getAttackerId());
        $this->assertSame(2, $pending->getDefenderId());
        $this->assertCount(1, $pending->getFaces());
        $this->assertSame(BlockDiceFace::DEFENDER_DOWN, $pending->getFaces()[0]);
        $this->assertTrue($pending->isAttackerChooses());
    }

    public function testChooseBlockDieAppliesResult(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, id: 2)
            ->build();

        // Roll 6 = DD, then armor rolls for defender
        $dice = new FixedDiceRoller([6, 3, 3]); // DD + armor 6 vs 8 (holds)
        $resolver = new ActionResolver($dice);
        $resolver->setInteractiveBlocks(true);

        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);
        $this->assertNotNull($result->getNewState()->getPendingBlock());

        // Choose the die
        $choiceResult = $resolver->resolve($result->getNewState(), ActionType::CHOOSE_BLOCK_DIE, ['faceIndex' => 0]);

        $this->assertNull($choiceResult->getNewState()->getPendingBlock());
        $defender = $choiceResult->getNewState()->getPlayer(2);
        $this->assertSame(PlayerState::PRONE, $defender->getState());
    }

    public function testAttackerDownOnChoiceCausesTurnover(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, id: 2)
            ->build();

        // Roll 1 = AD, armor rolls
        $dice = new FixedDiceRoller([1, 4, 4]); // AD + armor 8 vs 8 (not broken)
        $resolver = new ActionResolver($dice);
        $resolver->setInteractiveBlocks(true);

        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);
        $pending = $result->getNewState()->getPendingBlock();
        $this->assertSame(BlockDiceFace::ATTACKER_DOWN, $pending->getFaces()[0]);

        $choiceResult = $resolver->resolve($result->getNewState(), ActionType::CHOOSE_BLOCK_DIE, ['faceIndex' => 0]);
        $this->assertTrue($choiceResult->isTurnover());
    }

    // ========== Team Reroll ==========

    public function testTeamRerollRerollsAllDice(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, id: 2)
            ->build();

        // Roll 1 = AD, then reroll → 6 = DD, then armor
        $dice = new FixedDiceRoller([1, 6, 3, 3]);
        $resolver = new ActionResolver($dice);
        $resolver->setInteractiveBlocks(true);

        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);
        $this->assertSame(BlockDiceFace::ATTACKER_DOWN, $result->getNewState()->getPendingBlock()->getFaces()[0]);
        $this->assertTrue($result->getNewState()->getPendingBlock()->isTeamRerollAvailable());

        // Use team reroll
        $rerollResult = $resolver->resolve($result->getNewState(), ActionType::REROLL_BLOCK, ['type' => 'team']);
        $pending = $rerollResult->getNewState()->getPendingBlock();
        $this->assertNotNull($pending);
        $this->assertSame(BlockDiceFace::DEFENDER_DOWN, $pending->getFaces()[0]);
        $this->assertFalse($pending->isTeamRerollAvailable());

        // Check reroll was consumed
        $homeTeam = $rerollResult->getNewState()->getHomeTeam();
        $this->assertTrue($homeTeam->isRerollUsedThisTurn());
        $this->assertSame(2, $homeTeam->getRerolls());
    }

    // ========== Brawler Skill ==========

    public function testBrawlerRerollsBothDownDie(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 3, skills: [SkillName::Brawler], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, id: 2)
            ->build();

        // Roll 2 = BD, brawler reroll → 6 = DD, armor
        $dice = new FixedDiceRoller([2, 6, 3, 3]);
        $resolver = new ActionResolver($dice);
        $resolver->setInteractiveBlocks(true);

        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);
        $pending = $result->getNewState()->getPendingBlock();
        $this->assertTrue($pending->isBrawlerAvailable());
        $this->assertSame(BlockDiceFace::BOTH_DOWN, $pending->getFaces()[0]);

        // Use Brawler
        $rerollResult = $resolver->resolve($result->getNewState(), ActionType::REROLL_BLOCK, ['type' => 'brawler']);
        $pending = $rerollResult->getNewState()->getPendingBlock();
        $this->assertNotNull($pending);
        $this->assertSame(BlockDiceFace::DEFENDER_DOWN, $pending->getFaces()[0]);
        $this->assertFalse($pending->isBrawlerAvailable());

        // Brawler events
        $types = array_map(fn($e) => $e->getType(), $rerollResult->getEvents());
        $this->assertContains('reroll', $types);
    }

    public function testBrawlerNotAvailableWithoutBothDown(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 3, skills: [SkillName::Brawler], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, id: 2)
            ->build();

        // Roll 1 = AD (not Both Down)
        $dice = new FixedDiceRoller([1]);
        $resolver = new ActionResolver($dice);
        $resolver->setInteractiveBlocks(true);

        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);
        $pending = $result->getNewState()->getPendingBlock();
        // Brawler is technically available (player has skill), but no BD in roll
        $this->assertTrue($pending->isBrawlerAvailable());

        // Trying to use Brawler without BD should fail
        $this->expectException(\InvalidArgumentException::class);
        $resolver->resolve($result->getNewState(), ActionType::REROLL_BLOCK, ['type' => 'brawler']);
    }

    public function testBrawlerAutoResolvesForAI(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 3, skills: [SkillName::Brawler], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, id: 2)
            ->build();

        // Roll 2 = BD, brawler reroll → 6 = DD, armor
        $dice = new FixedDiceRoller([2, 6, 3, 3]);
        $resolver = new ActionResolver($dice);
        // Not interactive → auto-resolves

        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        // Should be fully resolved
        $this->assertNull($result->getNewState()->getPendingBlock());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('reroll', $types);
    }

    // ========== Pro Reroll on Block ==========

    public function testProRerollOnBlock(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 3, skills: [SkillName::Pro], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, id: 2)
            ->build();

        // Roll 1 = AD, Pro 4+, reroll worst → 3 = Pushed
        $dice = new FixedDiceRoller([1, 4, 3]);
        $resolver = new ActionResolver($dice);
        $resolver->setInteractiveBlocks(true);

        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);
        $this->assertTrue($result->getNewState()->getPendingBlock()->isProAvailable());

        $rerollResult = $resolver->resolve($result->getNewState(), ActionType::REROLL_BLOCK, ['type' => 'pro']);
        $pending = $rerollResult->getNewState()->getPendingBlock();
        $this->assertNotNull($pending);
        $this->assertSame(BlockDiceFace::PUSHED, $pending->getFaces()[0]);
        $this->assertFalse($pending->isProAvailable());

        $types = array_map(fn($e) => $e->getType(), $rerollResult->getEvents());
        $this->assertContains('pro', $types);
    }

    public function testProFailOnBlock(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 3, skills: [SkillName::Pro], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, id: 2)
            ->build();

        // Roll 1 = AD, Pro 3 (fail) → keeps AD
        $dice = new FixedDiceRoller([1, 3]);
        $resolver = new ActionResolver($dice);
        $resolver->setInteractiveBlocks(true);

        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $rerollResult = $resolver->resolve($result->getNewState(), ActionType::REROLL_BLOCK, ['type' => 'pro']);
        $pending = $rerollResult->getNewState()->getPendingBlock();
        $this->assertNotNull($pending);
        $this->assertSame(BlockDiceFace::ATTACKER_DOWN, $pending->getFaces()[0]); // unchanged
        $this->assertFalse($pending->isProAvailable()); // Pro consumed
    }

    // ========== Frenzy + Pending Block ==========

    public function testFrenzyCreatesSecondPendingBlock(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 4, skills: [SkillName::Frenzy], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, id: 2)
            ->build();

        // 2-die block: roll 3(Push), 6(DD) → choose DD (push only), armor, follow-up
        // Then frenzy: roll 3(Push), 5(DS) → pending block for frenzy
        $dice = new FixedDiceRoller([3, 6, 3, 5]);
        $resolver = new ActionResolver($dice);
        $resolver->setInteractiveBlocks(true);

        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);
        $pending = $result->getNewState()->getPendingBlock();
        $this->assertNotNull($pending);
        $this->assertFalse($pending->isFrenzy());

        // Choose Push (index 0, face = PUSHED) to keep both standing
        $choiceResult = $resolver->resolve($result->getNewState(), ActionType::CHOOSE_BLOCK_DIE, ['faceIndex' => 0]);

        // After push + follow-up, frenzy should create a second pending block
        $pending2 = $choiceResult->getNewState()->getPendingBlock();
        $this->assertNotNull($pending2);
        $this->assertTrue($pending2->isFrenzy());
    }

    // ========== Validation ==========

    public function testCannotDoOtherActionsWhilePendingBlock(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 3, 3, id: 3)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, id: 2)
            ->build();

        $dice = new FixedDiceRoller([6]);
        $resolver = new ActionResolver($dice);
        $resolver->setInteractiveBlocks(true);

        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);
        $stateWithPending = $result->getNewState();

        // Try to move another player — should be blocked by validation
        $rulesEngine = new \App\Engine\RulesEngine();
        $errors = $rulesEngine->validate($stateWithPending, ActionType::MOVE, ['playerId' => 3, 'x' => 3, 'y' => 4]);
        $this->assertContains('Must resolve pending block first', $errors);
    }

    public function testGetAvailableActionsWithPendingBlock(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, id: 2)
            ->build();

        $dice = new FixedDiceRoller([1]); // AD = bad result
        $resolver = new ActionResolver($dice);
        $resolver->setInteractiveBlocks(true);

        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);
        $stateWithPending = $result->getNewState();

        $rulesEngine = new \App\Engine\RulesEngine();
        $actions = $rulesEngine->getAvailableActions($stateWithPending);
        $actionTypes = array_map(fn($a) => $a['type'], $actions);

        $this->assertContains('choose_block_die', $actionTypes);
        $this->assertContains('reroll_block', $actionTypes);
        $this->assertNotContains('move', $actionTypes);
        $this->assertNotContains('block', $actionTypes);
    }

    // ========== PendingBlockDTO Serialization ==========

    public function testPendingBlockSerialization(): void
    {
        $pending = new PendingBlockDTO(
            attackerId: 1,
            defenderId: 2,
            faces: [BlockDiceFace::BOTH_DOWN, BlockDiceFace::PUSHED],
            attackerChooses: true,
            isBlitz: false,
            isFrenzy: false,
            brawlerAvailable: true,
            proAvailable: false,
            teamRerollAvailable: true,
        );

        $arr = $pending->toArray();
        $restored = PendingBlockDTO::fromArray($arr);

        $this->assertSame(1, $restored->getAttackerId());
        $this->assertSame(2, $restored->getDefenderId());
        $this->assertCount(2, $restored->getFaces());
        $this->assertSame(BlockDiceFace::BOTH_DOWN, $restored->getFaces()[0]);
        $this->assertSame(BlockDiceFace::PUSHED, $restored->getFaces()[1]);
        $this->assertTrue($restored->isAttackerChooses());
        $this->assertTrue($restored->isBrawlerAvailable());
        $this->assertFalse($restored->isProAvailable());
        $this->assertTrue($restored->isTeamRerollAvailable());
    }

    public function testGameStatePendingBlockSerialization(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, id: 2)
            ->build();

        $pending = new PendingBlockDTO(
            attackerId: 1,
            defenderId: 2,
            faces: [BlockDiceFace::DEFENDER_DOWN],
            attackerChooses: true,
            isBlitz: false,
            isFrenzy: false,
            brawlerAvailable: false,
            proAvailable: false,
            teamRerollAvailable: false,
        );

        $stateWithPending = $state->withPendingBlock($pending);
        $arr = $stateWithPending->toArray();
        $restored = \App\DTO\GameState::fromArray($arr);

        $this->assertNotNull($restored->getPendingBlock());
        $this->assertSame(1, $restored->getPendingBlock()->getAttackerId());
        $this->assertSame(BlockDiceFace::DEFENDER_DOWN, $restored->getPendingBlock()->getFaces()[0]);
    }

    // ========== Loner + Team Reroll ==========

    public function testTeamRerollWithLonerCheck(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 3, skills: [SkillName::Loner], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, id: 2)
            ->build();

        // Roll 1 = AD, Loner 4+ (pass), reroll → 6 = DD, armor
        $dice = new FixedDiceRoller([1, 4, 6, 3, 3]);
        $resolver = new ActionResolver($dice);
        $resolver->setInteractiveBlocks(true);

        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $rerollResult = $resolver->resolve($result->getNewState(), ActionType::REROLL_BLOCK, ['type' => 'team']);
        $pending = $rerollResult->getNewState()->getPendingBlock();
        $this->assertNotNull($pending);
        $this->assertSame(BlockDiceFace::DEFENDER_DOWN, $pending->getFaces()[0]);

        $types = array_map(fn($e) => $e->getType(), $rerollResult->getEvents());
        $this->assertContains('loner', $types);
    }

    public function testTeamRerollBlockedByLoner(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 3, skills: [SkillName::Loner], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, id: 2)
            ->build();

        // Roll 1 = AD, Loner 3 (fail) → dice unchanged
        $dice = new FixedDiceRoller([1, 3]);
        $resolver = new ActionResolver($dice);
        $resolver->setInteractiveBlocks(true);

        $result = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $rerollResult = $resolver->resolve($result->getNewState(), ActionType::REROLL_BLOCK, ['type' => 'team']);
        $pending = $rerollResult->getNewState()->getPendingBlock();
        $this->assertNotNull($pending);
        $this->assertSame(BlockDiceFace::ATTACKER_DOWN, $pending->getFaces()[0]); // unchanged

        // Reroll consumed even though Loner blocked it
        $this->assertTrue($rerollResult->getNewState()->getHomeTeam()->isRerollUsedThisTurn());
    }
}
