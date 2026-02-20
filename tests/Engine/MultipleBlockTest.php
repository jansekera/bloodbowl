<?php
declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Engine\RulesEngine;
use App\Enum\ActionType;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

final class MultipleBlockTest extends TestCase
{
    /**
     * ST7 attacker blocks 2 ST3 defenders with Multiple Block.
     * Block 1: attStr=7 vs defStr=3+1(assist)+2(MB)=6 → 2 dice attacker. POW → push+knockdown.
     * Block 2: attStr=7 vs defStr=3+0+2=5 → 2 dice attacker. POW → push+knockdown.
     */
    public function testMultipleBlockBothDefendersDown(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 7, skills: [SkillName::MultipleBlock, SkillName::Block], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, armour: 7, id: 2)
            ->addPlayer(TeamSide::AWAY, 5, 6, strength: 3, armour: 7, id: 3)
            ->withBallOffPitch()
            ->build();

        // Block 1: 2 dice attacker, [6, 6] → POW. Armor: [3, 3] = 6 ≤ AV7 → holds, PRONE.
        // Block 2: 2 dice attacker (no assist after defender1 pushed), [6, 6] → POW. Armor: [3, 3] → holds, PRONE.
        $dice = new FixedDiceRoller([
            6, 6,  // block 1: 2 dice, both POW → attacker picks POW
            3, 3,  // defender 2 armor: 6 ≤ 7 holds
            6, 6,  // block 2: 2 dice, both POW
            3, 3,  // defender 3 armor: 6 ≤ 7 holds
        ]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::MULTIPLE_BLOCK, [
            'playerId' => 1,
            'targetId' => 2,
            'targetId2' => 3,
        ]);

        $this->assertFalse($result->isTurnover());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('multiple_block', $types);

        // Both defenders should be prone
        $newState = $result->getNewState();
        $this->assertSame(PlayerState::PRONE, $newState->getPlayer(2)->getState());
        $this->assertSame(PlayerState::PRONE, $newState->getPlayer(3)->getState());

        // Attacker should be standing and have acted
        $attacker = $newState->getPlayer(1);
        $this->assertSame(PlayerState::STANDING, $attacker->getState());
        $this->assertTrue($attacker->hasActed());
    }

    /**
     * Verify dice calculation: ST4 attacker vs ST3 defender.
     * Normal block: ST4 vs ST3 = 2 dice attacker chooses.
     * Multiple Block: ST4 vs ST3+1(assist)+2(MB)=ST6 → 2 dice DEFENDER chooses.
     * Defender picks Attacker Down → turnover, no second block.
     */
    public function testMultipleBlockDefenderSTBonus(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 4, skills: [SkillName::MultipleBlock, SkillName::Block], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, armour: 9, id: 2)
            ->addPlayer(TeamSide::AWAY, 5, 6, strength: 3, armour: 9, id: 3)
            ->withBallOffPitch()
            ->build();

        // Block 1: attStr=4 vs defStr=6 → 2 dice defender chooses
        // Dice: [6, 1] → POW and Attacker Down → defender picks Attacker Down
        // Attacker armor: [3, 3] = 6 ≤ AV8 → holds, PRONE
        $dice = new FixedDiceRoller([
            6, 1,  // 2 block dice
            3, 3,  // attacker armor
        ]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::MULTIPLE_BLOCK, [
            'playerId' => 1,
            'targetId' => 2,
            'targetId2' => 3,
        ]);

        $this->assertTrue($result->isTurnover());

        // Verify block event shows 2 dice and defender choosing
        $blockEvents = array_values(array_filter(
            $result->getEvents(),
            fn($e) => $e->getType() === 'block',
        ));
        $this->assertCount(1, $blockEvents); // Only 1 block (no second)
        $this->assertSame(2, $blockEvents[0]->getData()['diceCount']);
        $this->assertFalse($blockEvents[0]->getData()['attackerChooses']);
    }

    /**
     * Verify attacker stays in original position after blocks (no follow-up).
     * Block 1: 1 die (6 vs 6) → PUSHED. Block 2: 2 dice attacker (6 vs 5) → PUSHED.
     */
    public function testMultipleBlockNoFollowUp(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 6, skills: [SkillName::MultipleBlock, SkillName::Block], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, armour: 9, id: 2)
            ->addPlayer(TeamSide::AWAY, 5, 6, strength: 3, armour: 9, id: 3)
            ->withBallOffPitch()
            ->build();

        // Block 1: attStr=6 vs defStr=3+1+2=6 → 1 die. Roll=3 → PUSHED.
        // Block 2: attStr=6 vs defStr=3+0+2=5 → 2 dice attacker. Roll=[3, 4] → both PUSHED.
        $dice = new FixedDiceRoller([
            3,     // block 1: PUSHED
            3, 4,  // block 2: PUSHED, PUSHED
        ]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::MULTIPLE_BLOCK, [
            'playerId' => 1,
            'targetId' => 2,
            'targetId2' => 3,
        ]);

        $this->assertFalse($result->isTurnover());

        // Attacker should NOT have moved (no follow-up)
        $attacker = $result->getNewState()->getPlayer(1);
        $this->assertSame(5, $attacker->getPosition()->getX());
        $this->assertSame(5, $attacker->getPosition()->getY());

        // No follow-up events
        $followUpEvents = array_filter(
            $result->getEvents(),
            fn($e) => $e->getType() === 'follow_up',
        );
        $this->assertEmpty($followUpEvents);

        // 2 push events (one per defender)
        $pushEvents = array_filter(
            $result->getEvents(),
            fn($e) => $e->getType() === 'push',
        );
        $this->assertCount(2, $pushEvents);
    }

    /**
     * Attacker goes down on first block (Both Down, no Block skill) → turnover, no second block.
     * attStr=5 vs defStr=3+1+2=6 → 2 dice defender. Defender picks Both Down.
     */
    public function testMultipleBlockFirstBlockKnocksAttacker(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 5, skills: [SkillName::MultipleBlock], id: 1) // No Block skill
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, armour: 9, id: 2)
            ->addPlayer(TeamSide::AWAY, 5, 6, strength: 3, armour: 9, id: 3)
            ->withBallOffPitch()
            ->build();

        // Block 1: 2 dice defender chooses. [2, 3] → Both Down + PUSHED → defender picks Both Down
        // Both Down: attacker no Block → down. Defender no Block → also down.
        // Defender 2 armor: [3, 3] = 6 ≤ 9 → holds
        // Attacker armor: [3, 3] = 6 ≤ 8 → holds
        $dice = new FixedDiceRoller([
            2, 3,  // 2 block dice: Both Down, PUSHED
            3, 3,  // defender 2 armor
            3, 3,  // attacker armor
        ]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::MULTIPLE_BLOCK, [
            'playerId' => 1,
            'targetId' => 2,
            'targetId2' => 3,
        ]);

        $this->assertTrue($result->isTurnover());

        // Only 1 block event (no second block attempted)
        $blockEvents = array_filter(
            $result->getEvents(),
            fn($e) => $e->getType() === 'block',
        );
        $this->assertCount(1, $blockEvents);

        // Attacker should be prone
        $this->assertSame(PlayerState::PRONE, $result->getNewState()->getPlayer(1)->getState());
    }

    /**
     * Validation fails if player doesn't have MultipleBlock skill.
     */
    public function testMultipleBlockRequiresSkill(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, id: 2)
            ->addPlayer(TeamSide::AWAY, 5, 6, strength: 3, id: 3)
            ->withBallOffPitch()
            ->build();

        $rules = new RulesEngine();
        $errors = $rules->validate($state, ActionType::MULTIPLE_BLOCK, [
            'playerId' => 1,
            'targetId' => 2,
            'targetId2' => 3,
        ]);

        $this->assertNotEmpty($errors);
        $this->assertContains('Player must have Multiple Block skill', $errors);
    }

    /**
     * Validation fails without targetId2 or with same target twice.
     */
    public function testMultipleBlockRequiresTwoTargets(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 5, skills: [SkillName::MultipleBlock], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, id: 2)
            ->addPlayer(TeamSide::AWAY, 5, 6, strength: 3, id: 3)
            ->withBallOffPitch()
            ->build();

        $rules = new RulesEngine();

        // Missing targetId2
        $errors = $rules->validate($state, ActionType::MULTIPLE_BLOCK, [
            'playerId' => 1,
            'targetId' => 2,
        ]);
        $this->assertNotEmpty($errors);
        $this->assertContains('targetId2 is required', $errors);

        // Same target twice
        $errors2 = $rules->validate($state, ActionType::MULTIPLE_BLOCK, [
            'playerId' => 1,
            'targetId' => 2,
            'targetId2' => 2,
        ]);
        $this->assertNotEmpty($errors2);
        $this->assertContains('Cannot block the same player twice', $errors2);
    }

    /**
     * Player with both Frenzy and MultipleBlock: no Frenzy second block occurs.
     * Using StandFirm on defender1 so they stay adjacent — Frenzy would normally trigger.
     */
    public function testMultipleBlockMutuallyExclusiveWithFrenzy(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 6, skills: [SkillName::MultipleBlock, SkillName::Frenzy, SkillName::Block], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, armour: 9, skills: [SkillName::StandFirm], id: 2)
            ->addPlayer(TeamSide::AWAY, 5, 6, strength: 3, armour: 9, id: 3)
            ->withBallOffPitch()
            ->build();

        // Block 1: 1 die (6 vs 6). Roll=3 → PUSHED. StandFirm → not pushed. No knockdown.
        // Block 2: 2 dice attacker (6 vs 5). Roll=[3, 4] → PUSHED. Defender2 pushed.
        $dice = new FixedDiceRoller([
            3,     // block 1: PUSHED (stand firm prevents push)
            3, 4,  // block 2: PUSHED, PUSHED
        ]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::MULTIPLE_BLOCK, [
            'playerId' => 1,
            'targetId' => 2,
            'targetId2' => 3,
        ]);

        $this->assertFalse($result->isTurnover());

        // Should be exactly 2 block events (no frenzy blocks)
        $blockEvents = array_filter(
            $result->getEvents(),
            fn($e) => $e->getType() === 'block',
        );
        $this->assertCount(2, $blockEvents);

        // No frenzy events
        $frenzyEvents = array_filter(
            $result->getEvents(),
            fn($e) => $e->getType() === 'frenzy',
        );
        $this->assertEmpty($frenzyEvents);
    }

    /**
     * getAvailableActions() shows MULTIPLE_BLOCK only when player has skill and 2+ adjacent enemies.
     */
    public function testMultipleBlockAvailableInActions(): void
    {
        // 2 adjacent enemies → should show MULTIPLE_BLOCK
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 4, skills: [SkillName::MultipleBlock], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, id: 2)
            ->addPlayer(TeamSide::AWAY, 5, 6, strength: 3, id: 3)
            ->withBallOffPitch()
            ->build();

        $rules = new RulesEngine();
        $actions = $rules->getAvailableActions($state);
        $actionTypes = array_map(fn($a) => $a['type'], $actions);

        $this->assertContains(ActionType::MULTIPLE_BLOCK->value, $actionTypes);
        $this->assertContains(ActionType::BLOCK->value, $actionTypes);

        // Only 1 adjacent enemy → no MULTIPLE_BLOCK
        $state2 = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 4, skills: [SkillName::MultipleBlock], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, id: 2)
            ->withBallOffPitch()
            ->build();

        $actions2 = $rules->getAvailableActions($state2);
        $actionTypes2 = array_map(fn($a) => $a['type'], $actions2);

        $this->assertNotContains(ActionType::MULTIPLE_BLOCK->value, $actionTypes2);
        $this->assertContains(ActionType::BLOCK->value, $actionTypes2);

        // No MultipleBlock skill → no MULTIPLE_BLOCK action even with 2 enemies
        $state3 = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 4, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, id: 2)
            ->addPlayer(TeamSide::AWAY, 5, 6, strength: 3, id: 3)
            ->withBallOffPitch()
            ->build();

        $actions3 = $rules->getAvailableActions($state3);
        $actionTypes3 = array_map(fn($a) => $a['type'], $actions3);

        $this->assertNotContains(ActionType::MULTIPLE_BLOCK->value, $actionTypes3);
    }

    /**
     * Target must be adjacent — non-adjacent target fails validation.
     */
    public function testMultipleBlockTargetMustBeAdjacent(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 5, skills: [SkillName::MultipleBlock], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, id: 2) // adjacent
            ->addPlayer(TeamSide::AWAY, 8, 5, strength: 3, id: 3) // NOT adjacent
            ->withBallOffPitch()
            ->build();

        $rules = new RulesEngine();
        $errors = $rules->validate($state, ActionType::MULTIPLE_BLOCK, [
            'playerId' => 1,
            'targetId' => 2,
            'targetId2' => 3,
        ]);

        $this->assertNotEmpty($errors);
        $this->assertContains('Target must be adjacent to block', $errors);
    }
}
