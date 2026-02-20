<?php

declare(strict_types=1);

namespace App\Engine\Action;

use App\DTO\ActionResult;
use App\DTO\BallState;
use App\DTO\GameEvent;
use App\DTO\GameState;
use App\DTO\MatchPlayerDTO;
use App\Enum\BlockDiceFace;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\ValueObject\Position;
use App\Engine\BallResolver;
use App\Engine\DiceRollerInterface;
use App\Engine\InjuryResolver;
use App\Engine\PassResolver;
use App\Engine\StrengthCalculator;
use App\Engine\TacklezoneCalculator;

final class BlockHandler implements ActionHandlerInterface
{
    private ?PassResolver $passResolver = null;

    public function __construct(
        private readonly DiceRollerInterface $dice,
        private readonly StrengthCalculator $strCalc,
        private readonly TacklezoneCalculator $tzCalc,
        private readonly InjuryResolver $injuryResolver,
        private readonly BallResolver $ballResolver,
    ) {
    }

    public function setPassResolver(PassResolver $passResolver): void
    {
        $this->passResolver = $passResolver;
    }

    /**
     * @param array<string, mixed> $params
     */
    public function resolve(GameState $state, array $params): ActionResult
    {
        $attackerId = (int) $params['playerId'];
        $targetId = (int) $params['targetId'];

        $attacker = $state->getPlayer($attackerId);
        $defender = $state->getPlayer($targetId);

        if ($attacker === null || $defender === null) {
            throw new \InvalidArgumentException('Player not found');
        }

        $events = [];

        // Jump Up: stand up the prone attacker for free before blocking
        if ($attacker->getState() === PlayerState::PRONE && $attacker->hasSkill(SkillName::JumpUp)) {
            $attacker = $attacker->withState(PlayerState::STANDING);
            $state = $state->withPlayer($attacker);
            $events[] = GameEvent::standUp($attackerId);
        }

        $attackerPos = $attacker->getPosition();
        $defenderPos = $defender->getPosition();

        if ($attackerPos === null || $defenderPos === null) {
            throw new \InvalidArgumentException('Players must be on pitch');
        }

        if ($attackerPos->distanceTo($defenderPos) !== 1) {
            throw new \InvalidArgumentException('Players must be adjacent to block');
        }

        // Dump-Off: defender with ball can quick pass before block
        if ($defender->hasSkill(SkillName::DumpOff)
            && $state->getBall()->getCarrierId() === $defender->getId()
            && !$state->getTeamState($defender->getTeamSide())->isPassUsedThisTurn()
            && $this->passResolver !== null
        ) {
            // Find closest friendly teammate adjacent to defender
            $dumpTarget = $this->findDumpOffTarget($state, $defender);
            if ($dumpTarget !== null) {
                $dumpResult = $this->passResolver->resolveDumpOff($state, $defender, $dumpTarget);
                $state = $dumpResult['state'];
                $events = array_merge($events, $dumpResult['events']);
                // Refresh attacker/defender from updated state
                $attacker = $state->getPlayer($attackerId);
                $defender = $state->getPlayer($targetId);
                if ($attacker === null || $defender === null) {
                    throw new \InvalidArgumentException('Player not found after dump-off');
                }
            }
        }

        // Foul Appearance: defender check before any block type proceeds
        if ($defender->hasSkill(SkillName::FoulAppearance)) {
            $faRoll = $this->dice->rollD6();
            $events[] = GameEvent::foulAppearance($defender->getId(), $attacker->getId(), $faRoll, $faRoll >= 2);
            if ($faRoll < 2) {
                // Failed: attacker loses action, NOT a turnover
                $state = $state->withPlayer(
                    $attacker->withHasActed(true)->withHasMoved(true),
                );
                return ActionResult::success($state, $events);
            }
        }

        // Chainsaw: auto armor roll, kickback on double-1, never turnover
        if ($attacker->hasSkill(SkillName::Chainsaw)) {
            return $this->resolveChainsaw($state, $attacker, $defender, $events);
        }

        // Stab: bypass block dice entirely, go straight to armor roll
        if ($attacker->hasSkill(SkillName::Stab)) {
            return $this->resolveStab($state, $attacker, $defender, $events);
        }

        // Calculate effective strengths
        $attStr = $this->strCalc->calculateEffectiveStrength($state, $attacker, $defenderPos);
        $defStr = $this->strCalc->calculateEffectiveStrength($state, $defender, $attackerPos);

        // Horns: +1 ST when blitzing
        if (!empty($params['hornsBonus'])) {
            $attStr++;
        }

        // Dauntless: if attacker ST < defender base ST, roll D6+ST; if >= defender ST, treat as equal
        if ($attacker->hasSkill(SkillName::Dauntless) && $attacker->getStats()->getStrength() < $defender->getStats()->getStrength()) {
            $dauntlessRoll = $this->dice->rollD6();
            $dauntlessTotal = $dauntlessRoll + $attacker->getStats()->getStrength();
            if ($dauntlessTotal >= $defender->getStats()->getStrength()) {
                // Treat as equal ST for dice calculation
                $attStr = max($attStr, $defStr);
            }
        }

        // Determine dice
        $diceInfo = $this->strCalc->getBlockDiceInfo($attStr, $defStr);
        $diceCount = $diceInfo['count'];
        $attackerChooses = $diceInfo['attackerChooses'];

        // Roll block dice
        $faces = [];
        for ($i = 0; $i < $diceCount; $i++) {
            $faces[] = $this->rollBlockDie();
        }

        // Auto-choose best face
        $chosenFace = $this->autoChooseBlockDie($faces, $attackerChooses, $attacker, $defender);

        // Pro reroll on bad block result
        if ($attacker->hasSkill(SkillName::Pro) && !$attacker->isProUsedThisTurn()) {
            $chosenScore = $this->scoreBlockFace($chosenFace, $attacker, $defender);
            if ($chosenScore < 0) {
                $proRoll = $this->dice->rollD6();
                $attacker = $attacker->withProUsedThisTurn(true);
                $state = $state->withPlayer($attacker);
                if ($proRoll >= 4) {
                    // Reroll the worst die
                    $worstIdx = 0;
                    $worstScore = PHP_INT_MAX;
                    foreach ($faces as $idx => $f) {
                        $s = $this->scoreBlockFace($f, $attacker, $defender);
                        if ($s < $worstScore) {
                            $worstScore = $s;
                            $worstIdx = $idx;
                        }
                    }
                    $faces[$worstIdx] = $this->rollBlockDie();
                    $chosenFace = $this->autoChooseBlockDie($faces, $attackerChooses, $attacker, $defender);
                    $events[] = GameEvent::proReroll($attacker->getId(), $proRoll, true, null);
                } else {
                    $events[] = GameEvent::proReroll($attacker->getId(), $proRoll, false, null);
                }
            }
        }

        $faceValues = array_map(fn(BlockDiceFace $f) => $f->value, $faces);
        $events[] = GameEvent::blockAttempt(
            $attackerId, $targetId, $diceCount, $attackerChooses, $faceValues, $chosenFace->value,
        );

        // Apply block result
        $isBlitz = !empty($params['isBlitz']);
        $result = $this->applyBlockResult($state, $attacker, $defender, $chosenFace, $events, $isBlitz);

        // Frenzy: mandatory second block if both still standing and adjacent
        if (!$result->isTurnover() && $attacker->hasSkill(SkillName::Frenzy)) {
            $frenzyState = $result->getNewState();
            $frenzyAttacker = $frenzyState->getPlayer($attackerId);
            $frenzyDefender = $frenzyState->getPlayer($targetId);

            if ($frenzyAttacker !== null && $frenzyDefender !== null
                && $frenzyAttacker->getState() === PlayerState::STANDING
                && $frenzyDefender->getState() === PlayerState::STANDING
                && $frenzyAttacker->getPosition() !== null && $frenzyDefender->getPosition() !== null
                && $frenzyAttacker->getPosition()->distanceTo($frenzyDefender->getPosition()) === 1
            ) {
                $frenzyEvents = $result->getEvents();
                $frenzyEvents[] = GameEvent::frenzyBlock($attackerId, $targetId);

                // Recalculate strengths at new positions
                $attStr2 = $this->strCalc->calculateEffectiveStrength($frenzyState, $frenzyAttacker, $frenzyDefender->getPosition());
                $defStr2 = $this->strCalc->calculateEffectiveStrength($frenzyState, $frenzyDefender, $frenzyAttacker->getPosition());
                $diceInfo2 = $this->strCalc->getBlockDiceInfo($attStr2, $defStr2);

                $faces2 = [];
                for ($i = 0; $i < $diceInfo2['count']; $i++) {
                    $faces2[] = $this->rollBlockDie();
                }

                $chosenFace2 = $this->autoChooseBlockDie($faces2, $diceInfo2['attackerChooses'], $frenzyAttacker, $frenzyDefender);

                $faceValues2 = array_map(fn(BlockDiceFace $f) => $f->value, $faces2);
                $frenzyEvents[] = GameEvent::blockAttempt(
                    $attackerId, $targetId, $diceInfo2['count'], $diceInfo2['attackerChooses'], $faceValues2, $chosenFace2->value,
                );

                $result = $this->applyBlockResult($frenzyState, $frenzyAttacker, $frenzyDefender, $chosenFace2, $frenzyEvents, $isBlitz);
            }
        }

        // Mark attacker as acted (get fresh from result state)
        $finalState = $result->getNewState();
        $updatedAttacker = $finalState->getPlayer($attackerId);
        if ($updatedAttacker !== null) {
            $finalState = $finalState->withPlayer(
                $updatedAttacker->withHasActed(true)->withHasMoved(true),
            );
        }

        if ($result->isTurnover()) {
            return ActionResult::turnover($finalState, $result->getEvents());
        }

        return ActionResult::success($finalState, $result->getEvents());
    }

    /**
     * Resolve a Multiple Block action: block two adjacent opponents, each at +2 ST, no follow-up.
     *
     * @param array<string, mixed> $params
     */
    public function resolveMultipleBlock(GameState $state, array $params): ActionResult
    {
        $attackerId = (int) $params['playerId'];
        $targetId1 = (int) $params['targetId'];
        $targetId2 = (int) $params['targetId2'];

        $attacker = $state->getPlayer($attackerId);
        $defender1 = $state->getPlayer($targetId1);
        $defender2 = $state->getPlayer($targetId2);

        if ($attacker === null || $defender1 === null || $defender2 === null) {
            throw new \InvalidArgumentException('Player not found');
        }

        $events = [];

        // Jump Up: stand up the prone attacker for free before blocking
        if ($attacker->getState() === PlayerState::PRONE && $attacker->hasSkill(SkillName::JumpUp)) {
            $attacker = $attacker->withState(PlayerState::STANDING);
            $state = $state->withPlayer($attacker);
            $events[] = GameEvent::standUp($attackerId);
        }

        $events[] = GameEvent::multipleBlock($attackerId, $targetId1, $targetId2);

        // === Block 1: target defender1 ===
        [$state, $events, $attackerDown] = $this->resolveSingleMultipleBlock(
            $state, $attacker, $defender1, $events,
        );

        // If attacker went down on first block, turnover, no second block
        if ($attackerDown) {
            $finalState = $state;
            $updatedAttacker = $finalState->getPlayer($attackerId);
            if ($updatedAttacker !== null) {
                $finalState = $finalState->withPlayer(
                    $updatedAttacker->withHasActed(true)->withHasMoved(true),
                );
            }
            return ActionResult::turnover($finalState, $events);
        }

        // === Block 2: target defender2 (if attacker still standing) ===
        $attacker = $state->getPlayer($attackerId);
        $defender2 = $state->getPlayer($targetId2);

        if ($attacker !== null && $defender2 !== null
            && $attacker->getState() === PlayerState::STANDING
            && $attacker->getPosition() !== null
        ) {
            [$state, $events, $attackerDown2] = $this->resolveSingleMultipleBlock(
                $state, $attacker, $defender2, $events,
            );

            if ($attackerDown2) {
                $finalState = $state;
                $updatedAttacker = $finalState->getPlayer($attackerId);
                if ($updatedAttacker !== null) {
                    $finalState = $finalState->withPlayer(
                        $updatedAttacker->withHasActed(true)->withHasMoved(true),
                    );
                }
                return ActionResult::turnover($finalState, $events);
            }
        }

        // Mark attacker as acted
        $finalState = $state;
        $updatedAttacker = $finalState->getPlayer($attackerId);
        if ($updatedAttacker !== null) {
            $finalState = $finalState->withPlayer(
                $updatedAttacker->withHasActed(true)->withHasMoved(true),
            );
        }

        return ActionResult::success($finalState, $events);
    }

    /**
     * Resolve a single block within a Multiple Block action (+2 defender ST, no follow-up).
     *
     * @param list<GameEvent> $events
     * @return array{0: GameState, 1: list<GameEvent>, 2: bool} Updated state, events, and whether attacker went down
     */
    private function resolveSingleMultipleBlock(
        GameState $state,
        MatchPlayerDTO $attacker,
        MatchPlayerDTO $defender,
        array $events,
    ): array {
        $attackerPos = $attacker->getPosition();
        $defenderPos = $defender->getPosition();

        if ($attackerPos === null || $defenderPos === null) {
            return [$state, $events, false];
        }

        // Dump-Off: defender with ball can quick pass before block
        if ($defender->hasSkill(SkillName::DumpOff)
            && $state->getBall()->getCarrierId() === $defender->getId()
            && !$state->getTeamState($defender->getTeamSide())->isPassUsedThisTurn()
            && $this->passResolver !== null
        ) {
            $dumpTarget = $this->findDumpOffTarget($state, $defender);
            if ($dumpTarget !== null) {
                $dumpResult = $this->passResolver->resolveDumpOff($state, $defender, $dumpTarget);
                $state = $dumpResult['state'];
                $events = array_merge($events, $dumpResult['events']);
                $attacker = $state->getPlayer($attacker->getId());
                $defender = $state->getPlayer($defender->getId());
                if ($attacker === null || $defender === null) {
                    return [$state, $events, false];
                }
            }
        }

        // Foul Appearance check
        if ($defender->hasSkill(SkillName::FoulAppearance)) {
            $faRoll = $this->dice->rollD6();
            $events[] = GameEvent::foulAppearance($defender->getId(), $attacker->getId(), $faRoll, $faRoll >= 2);
            if ($faRoll < 2) {
                // Failed: this block does nothing (but does NOT end the Multiple Block action)
                return [$state, $events, false];
            }
        }

        // Chainsaw: auto armor roll, kickback on double-1, never turnover
        if ($attacker->hasSkill(SkillName::Chainsaw)) {
            $events[] = GameEvent::chainsaw($attacker->getId(), $defender->getId());
            $chainsawRoll = $this->dice->rollD6();
            if ($chainsawRoll === 1) {
                $events[] = GameEvent::chainsawKickback($attacker->getId());
                $injResult = $this->injuryResolver->resolve($attacker, $this->dice);
                $attacker = $injResult['player'];
                $state = $state->withPlayer($attacker);
                $events = array_merge($events, $injResult['events']);
                if ($attacker->getState() !== PlayerState::STANDING) {
                    [$state, $events] = $this->ballResolver->handleBallOnPlayerDown($state, $attacker, $events);
                    return [$state, $events, true];
                }
                return [$state, $events, false];
            }
            // Chainsaw hit on defender
            $mightyBlow = $attacker->hasSkill(SkillName::MightyBlow) ? 1 : 0;
            $hasClaw = $attacker->hasSkill(SkillName::Claw);
            $hasStakes = $attacker->hasSkill(SkillName::Stakes);
            $hasNurglesRot = $attacker->hasSkill(SkillName::NurglesRot);
            $wasBallCarrier = $state->getBall()->getCarrierId() === $defender->getId();
            $injResult = $this->injuryResolver->resolve($defender, $this->dice, $mightyBlow, 0, $hasClaw, $hasStakes, $hasNurglesRot);
            $defender = $injResult['player'];
            $state = $state->withPlayer($defender);
            $events = array_merge($events, $injResult['events']);
            if ($wasBallCarrier && $defender->getState() !== PlayerState::STANDING) {
                [$state, $events] = $this->ballResolver->handleBallOnPlayerDown($state, $defender, $events);
            }
            return [$state, $events, false];
        }

        // Stab: bypass block dice
        if ($attacker->hasSkill(SkillName::Stab)) {
            $events[] = GameEvent::stab($attacker->getId(), $defender->getId());
            $mightyBlow = $attacker->hasSkill(SkillName::MightyBlow) ? 1 : 0;
            $hasClaw = $attacker->hasSkill(SkillName::Claw);
            $hasStakes = $attacker->hasSkill(SkillName::Stakes);
            $hasNurglesRot = $attacker->hasSkill(SkillName::NurglesRot);
            $wasBallCarrier = $state->getBall()->getCarrierId() === $defender->getId();
            $injResult = $this->injuryResolver->resolve($defender, $this->dice, $mightyBlow, 0, $hasClaw, $hasStakes, $hasNurglesRot);
            $defender = $injResult['player'];
            $state = $state->withPlayer($defender);
            $events = array_merge($events, $injResult['events']);
            if ($wasBallCarrier && $defender->getState() !== PlayerState::STANDING && $defenderPos !== null) {
                [$state, $events] = $this->ballResolver->handleBallOnPlayerDown($state, $defender, $events);
            }
            return [$state, $events, false];
        }

        // Calculate effective strengths with +2 to defender
        $attStr = $this->strCalc->calculateEffectiveStrength($state, $attacker, $defenderPos);
        $defStr = $this->strCalc->calculateEffectiveStrength($state, $defender, $attackerPos) + 2;

        // Dauntless: compares base ST (no +2)
        if ($attacker->hasSkill(SkillName::Dauntless) && $attacker->getStats()->getStrength() < $defender->getStats()->getStrength()) {
            $dauntlessRoll = $this->dice->rollD6();
            $dauntlessTotal = $dauntlessRoll + $attacker->getStats()->getStrength();
            if ($dauntlessTotal >= $defender->getStats()->getStrength()) {
                $attStr = max($attStr, $defStr);
            }
        }

        // Determine dice
        $diceInfo = $this->strCalc->getBlockDiceInfo($attStr, $defStr);
        $diceCount = $diceInfo['count'];
        $attackerChooses = $diceInfo['attackerChooses'];

        // Roll block dice
        $faces = [];
        for ($i = 0; $i < $diceCount; $i++) {
            $faces[] = $this->rollBlockDie();
        }

        // Auto-choose best face
        $chosenFace = $this->autoChooseBlockDie($faces, $attackerChooses, $attacker, $defender);

        // Pro reroll on bad block result
        if ($attacker->hasSkill(SkillName::Pro) && !$attacker->isProUsedThisTurn()) {
            $chosenScore = $this->scoreBlockFace($chosenFace, $attacker, $defender);
            if ($chosenScore < 0) {
                $proRoll = $this->dice->rollD6();
                $attacker = $attacker->withProUsedThisTurn(true);
                $state = $state->withPlayer($attacker);
                if ($proRoll >= 4) {
                    $worstIdx = 0;
                    $worstScore = PHP_INT_MAX;
                    foreach ($faces as $idx => $f) {
                        $s = $this->scoreBlockFace($f, $attacker, $defender);
                        if ($s < $worstScore) {
                            $worstScore = $s;
                            $worstIdx = $idx;
                        }
                    }
                    $faces[$worstIdx] = $this->rollBlockDie();
                    $chosenFace = $this->autoChooseBlockDie($faces, $attackerChooses, $attacker, $defender);
                    $events[] = GameEvent::proReroll($attacker->getId(), $proRoll, true, null);
                } else {
                    $events[] = GameEvent::proReroll($attacker->getId(), $proRoll, false, null);
                }
            }
        }

        $faceValues = array_map(fn(BlockDiceFace $f) => $f->value, $faces);
        $events[] = GameEvent::blockAttempt(
            $attacker->getId(), $defender->getId(), $diceCount, $attackerChooses, $faceValues, $chosenFace->value,
        );

        // Apply block result with noFollowUp = true
        $result = $this->applyBlockResult($state, $attacker, $defender, $chosenFace, $events, false, true);

        $attackerDown = $result->isTurnover();
        return [$result->getNewState(), $result->getEvents(), $attackerDown];
    }

    /**
     * Resolve Stab: armor roll (no block dice, no pushback, never turnover).
     *
     * @param list<GameEvent> $events
     */
    private function resolveStab(
        GameState $state,
        MatchPlayerDTO $attacker,
        MatchPlayerDTO $defender,
        array $events,
    ): ActionResult {
        $events[] = GameEvent::stab($attacker->getId(), $defender->getId());

        $defenderPos = $defender->getPosition();
        $wasBallCarrier = $state->getBall()->getCarrierId() === $defender->getId();
        $mightyBlow = $attacker->hasSkill(SkillName::MightyBlow) ? 1 : 0;
        $hasClaw = $attacker->hasSkill(SkillName::Claw);
        $hasStakes = $attacker->hasSkill(SkillName::Stakes);
        $hasNurglesRot = $attacker->hasSkill(SkillName::NurglesRot);
        $injResult = $this->injuryResolver->resolve($defender, $this->dice, $mightyBlow, 0, $hasClaw, $hasStakes, $hasNurglesRot);
        $defender = $injResult['player'];
        $state = $state->withPlayer($defender);
        $events = array_merge($events, $injResult['events']);

        // Drop ball if defender was carrying and got knocked down/out
        if ($wasBallCarrier && $defender->getState() !== PlayerState::STANDING && $defenderPos !== null) {
            // If defender removed from pitch (KO/injured), ball drops at their original position
            if ($defender->getPosition() === null) {
                $state = $state->withBall(BallState::onGround($defenderPos));
                $bounceResult = $this->ballResolver->resolveBounce($state, $defenderPos);
                $state = $bounceResult['state'];
                $events = array_merge($events, $bounceResult['events']);
            } else {
                [$state, $events] = $this->ballResolver->handleBallOnPlayerDown($state, $defender, $events);
            }
        }

        // Mark attacker as acted
        $state = $state->withPlayer(
            $attacker->withHasActed(true)->withHasMoved(true),
        );

        return ActionResult::success($state, $events);
    }

    /**
     * Resolve Chainsaw: roll D6 first — on 1 kickback (armor roll on attacker),
     * on 2+ armor roll on defender. No block dice, no pushback, never turnover.
     *
     * @param list<GameEvent> $events
     */
    private function resolveChainsaw(
        GameState $state,
        MatchPlayerDTO $attacker,
        MatchPlayerDTO $defender,
        array $events,
    ): ActionResult {
        $events[] = GameEvent::chainsaw($attacker->getId(), $defender->getId());

        // Roll D6: 1 = kickback, 2+ = attack defender
        $chainsawRoll = $this->dice->rollD6();

        if ($chainsawRoll === 1) {
            // Kickback: armor roll on attacker
            $events[] = GameEvent::chainsawKickback($attacker->getId());
            $injResult = $this->injuryResolver->resolve($attacker, $this->dice);
            $attacker = $injResult['player'];
            $state = $state->withPlayer($attacker);
            $events = array_merge($events, $injResult['events']);

            // Drop ball if attacker was carrier and knocked down
            if ($attacker->getState() !== PlayerState::STANDING) {
                [$state, $events] = $this->ballResolver->handleBallOnPlayerDown($state, $attacker, $events);
            }

            // Mark as acted
            $freshAttacker = $state->getPlayer($attacker->getId());
            $state = $state->withPlayer(
                $freshAttacker->withHasActed(true)->withHasMoved(true),
            );
            return ActionResult::success($state, $events);
        }

        // Normal chainsaw attack on defender: armor roll (2D6)
        $defenderPos = $defender->getPosition();
        $wasBallCarrier = $state->getBall()->getCarrierId() === $defender->getId();
        $mightyBlow = $attacker->hasSkill(SkillName::MightyBlow) ? 1 : 0;
        $hasClaw = $attacker->hasSkill(SkillName::Claw);
        $hasStakes = $attacker->hasSkill(SkillName::Stakes);
        $hasNurglesRot = $attacker->hasSkill(SkillName::NurglesRot);
        $injResult = $this->injuryResolver->resolve($defender, $this->dice, $mightyBlow, 0, $hasClaw, $hasStakes, $hasNurglesRot);
        $defender = $injResult['player'];
        $state = $state->withPlayer($defender);
        $events = array_merge($events, $injResult['events']);

        // Drop ball if defender was carrying and got knocked down/out
        if ($wasBallCarrier && $defender->getState() !== PlayerState::STANDING && $defenderPos !== null) {
            if ($defender->getPosition() === null) {
                $state = $state->withBall(BallState::onGround($defenderPos));
                $bounceResult = $this->ballResolver->resolveBounce($state, $defenderPos);
                $state = $bounceResult['state'];
                $events = array_merge($events, $bounceResult['events']);
            } else {
                [$state, $events] = $this->ballResolver->handleBallOnPlayerDown($state, $defender, $events);
            }
        }

        // Mark attacker as acted
        $state = $state->withPlayer(
            $state->getPlayer($attacker->getId())->withHasActed(true)->withHasMoved(true),
        );

        return ActionResult::success($state, $events);
    }

    /**
     * Apply the chosen block die result.
     *
     * @param list<GameEvent> $events
     */
    private function applyBlockResult(
        GameState $state,
        MatchPlayerDTO $attacker,
        MatchPlayerDTO $defender,
        BlockDiceFace $face,
        array $events,
        bool $isBlitz = false,
        bool $noFollowUp = false,
    ): ActionResult {
        $attackerPos = $attacker->getPosition();
        $defenderPos = $defender->getPosition();

        if ($attackerPos === null || $defenderPos === null) {
            return ActionResult::failure($state, $events);
        }

        $attackerDown = false;
        $defenderDown = false;
        $defenderPushed = false;

        switch ($face) {
            case BlockDiceFace::ATTACKER_DOWN:
                $attackerDown = true;
                break;

            case BlockDiceFace::BOTH_DOWN:
                // Juggernaut: on blitz, Both Down becomes push
                if ($isBlitz && $attacker->hasSkill(SkillName::Juggernaut)) {
                    $defenderPushed = true;
                    $events[] = GameEvent::juggernaut($attacker->getId());
                    break;
                }
                // Wrestle: if either has it, both go prone without armor
                if ($attacker->hasSkill(SkillName::Wrestle) || $defender->hasSkill(SkillName::Wrestle)) {
                    $events[] = GameEvent::wrestle($attacker->getId(), $defender->getId());
                    $attacker = $attacker->withState(PlayerState::PRONE);
                    $defender = $defender->withState(PlayerState::PRONE);
                    $wState = $state->withPlayer($attacker)->withPlayer($defender);
                    [$wState, $events] = $this->ballResolver->handleBallOnPlayerDown($wState, $attacker, $events);
                    $defender = $wState->getPlayer($defender->getId());
                    [$wState, $events] = $this->ballResolver->handleBallOnPlayerDown($wState, $defender, $events);
                    return ActionResult::success($wState, $events);
                }
                if (!$attacker->hasSkill(SkillName::Block)) {
                    $attackerDown = true;
                }
                if (!$defender->hasSkill(SkillName::Block)) {
                    $defenderDown = true;
                }
                break;

            case BlockDiceFace::PUSHED:
                $defenderPushed = true;
                break;

            case BlockDiceFace::DEFENDER_STUMBLES:
                $defenderPushed = true;
                if (!$defender->hasSkill(SkillName::Dodge) || $attacker->hasSkill(SkillName::Tackle)) {
                    $defenderDown = true;
                }
                break;

            case BlockDiceFace::DEFENDER_DOWN:
            case BlockDiceFace::POW:
                $defenderDown = true;
                $defenderPushed = true;
                break;
        }

        $currentState = $state;

        // Stand Firm prevents pushback (but not knockdown)
        // Juggernaut on blitz ignores Stand Firm
        if ($defenderPushed && $defender->hasSkill(SkillName::StandFirm)
            && !($isBlitz && $attacker->hasSkill(SkillName::Juggernaut))
        ) {
            $defenderPushed = false;
        }

        // Handle pushback first
        if ($defenderPushed) {
            [$currentState, $events] = $this->resolvePushback(
                $currentState, $attacker, $defender, $defenderPos, $events,
            );
            // Re-fetch defender (position may have changed)
            $defender = $currentState->getPlayer($defender->getId());
        }

        // Follow-up: attacker moves to defender's old position
        // Fend: prevents follow-up when defender is not knocked down
        // Multiple Block: no follow-up allowed
        if ($noFollowUp) {
            // Skip follow-up entirely for Multiple Block
        } elseif ($defenderPushed && !($defender->hasSkill(SkillName::Fend) && !$defenderDown)) {
            $events[] = GameEvent::followUp($attacker->getId(), (string) $attackerPos, (string) $defenderPos);
            $attacker = $attacker->withPosition($defenderPos);
            $currentState = $currentState->withPlayer($attacker);

            // Move ball with attacker if carried
            if ($currentState->getBall()->getCarrierId() === $attacker->getId()) {
                $currentState = $currentState->withBall(BallState::carried($defenderPos, $attacker->getId()));
            }
        } elseif (!$noFollowUp && $defenderPushed && $defender->hasSkill(SkillName::Fend) && !$defenderDown) {
            $events[] = GameEvent::fend($defender->getId());
        }

        // Handle defender knockdown
        if ($defenderDown && $defender !== null && $defender->getPosition() !== null) {
            $events[] = GameEvent::playerFell($defender->getId());
            $defender = $defender->withState(PlayerState::PRONE);
            $currentState = $currentState->withPlayer($defender);

            // Armor/injury roll
            $mightyBlow = $attacker->hasSkill(SkillName::MightyBlow) ? 1 : 0;
            $hasClaw = $attacker->hasSkill(SkillName::Claw);
            $hasStakes = $attacker->hasSkill(SkillName::Stakes);
            $hasNurglesRot = $attacker->hasSkill(SkillName::NurglesRot);
            $injResult = $this->injuryResolver->resolve($defender, $this->dice, $mightyBlow, 0, $hasClaw, $hasStakes, $hasNurglesRot);
            $defender = $injResult['player'];
            $currentState = $currentState->withPlayer($defender);
            $events = array_merge($events, $injResult['events']);

            // Piling On: reroll armor if not broken, attacker goes prone
            $freshAttackerForPO = $currentState->getPlayer($attacker->getId());
            if ($freshAttackerForPO !== null && $freshAttackerForPO->hasSkill(SkillName::PilingOn)
                && $defender->getState() === PlayerState::PRONE
                && $freshAttackerForPO->getState() === PlayerState::STANDING
            ) {
                // Reroll armor/injury
                $poInjResult = $this->injuryResolver->resolve($defender, $this->dice, $mightyBlow, 0, $hasClaw, $hasStakes, $hasNurglesRot);
                $defender = $poInjResult['player'];
                $currentState = $currentState->withPlayer($defender);
                $events = array_merge($events, $poInjResult['events']);
                $events[] = GameEvent::pilingOn($freshAttackerForPO->getId(), 'armour');

                // Attacker goes prone
                $freshAttackerForPO = $freshAttackerForPO->withState(PlayerState::PRONE);
                $currentState = $currentState->withPlayer($freshAttackerForPO);
            }

            // Apothecary: auto-use on casualty
            $defenderSide = $defender->getTeamSide();
            if ($defender->getState() === PlayerState::INJURED && $currentState->getTeamState($defenderSide)->canUseApothecary()) {
                $currentState = $currentState->withTeamState($defenderSide, $currentState->getTeamState($defenderSide)->withApothecaryUsed());
                $apoResult = $this->injuryResolver->resolveApothecary($defender, $this->dice, PlayerState::INJURED, 0, $events);
                $defender = $apoResult['player'];
                $currentState = $currentState->withPlayer($defender);
                $events = $apoResult['events'];
            }

            // Drop ball if carrier
            [$currentState, $events] = $this->ballResolver->handleBallOnPlayerDown($currentState, $defender, $events);
        }

        // Handle attacker knockdown
        if ($attackerDown) {
            $events[] = GameEvent::playerFell($attacker->getId());
            $freshAttacker = $currentState->getPlayer($attacker->getId());
            if ($freshAttacker !== null) {
                $freshAttacker = $freshAttacker->withState(PlayerState::PRONE);
                $currentState = $currentState->withPlayer($freshAttacker);

                // Armor/injury roll for attacker
                $injResult = $this->injuryResolver->resolve($freshAttacker, $this->dice);
                $freshAttacker = $injResult['player'];
                $currentState = $currentState->withPlayer($freshAttacker);
                $events = array_merge($events, $injResult['events']);

                // Apothecary: auto-use on casualty for attacker
                $attackerSide = $freshAttacker->getTeamSide();
                if ($freshAttacker->getState() === PlayerState::INJURED && $currentState->getTeamState($attackerSide)->canUseApothecary()) {
                    $currentState = $currentState->withTeamState($attackerSide, $currentState->getTeamState($attackerSide)->withApothecaryUsed());
                    $apoResult = $this->injuryResolver->resolveApothecary($freshAttacker, $this->dice, PlayerState::INJURED, 0, $events);
                    $freshAttacker = $apoResult['player'];
                    $currentState = $currentState->withPlayer($freshAttacker);
                    $events = $apoResult['events'];
                }

                // Drop ball if carrier
                [$currentState, $events] = $this->ballResolver->handleBallOnPlayerDown($currentState, $freshAttacker, $events);
            }

            // Turnover
            $events[] = GameEvent::turnover('Attacker knocked down');
            return ActionResult::turnover($currentState->withTurnoverPending(true), $events);
        }

        return ActionResult::success($currentState, $events);
    }

    /**
     * Resolve pushback: move defender one square away from attacker.
     * If all push squares are occupied or off-pitch, chain push occurs.
     *
     * @param list<GameEvent> $events
     * @return array{0: GameState, 1: list<GameEvent>}
     */
    private function resolvePushback(
        GameState $state,
        MatchPlayerDTO $attacker,
        MatchPlayerDTO $defender,
        Position $defenderOriginalPos,
        array $events,
    ): array {
        $attackerPos = $attacker->getPosition();
        if ($attackerPos === null) {
            return [$state, $events];
        }

        $pushSquares = $this->getPushbackSquares($attackerPos, $defenderOriginalPos);

        // Categorize push squares: empty, occupied (chain-pushable), off-pitch
        $emptySquares = [];
        $occupiedSquares = []; // on-pitch, occupied, chain-pushable (no Stand Firm)
        $offPitchAvailable = false;
        foreach ($pushSquares as $pos) {
            if (!$pos->isOnPitch()) {
                $offPitchAvailable = true;
                continue;
            }
            $occupant = $state->getPlayerAtPosition($pos);
            if ($occupant === null) {
                $emptySquares[] = $pos;
            } elseif (!$occupant->hasSkill(SkillName::StandFirm)) {
                $occupiedSquares[] = ['pos' => $pos, 'player' => $occupant];
            }
            // Stand Firm occupants are not valid push targets — skip
        }

        // Find valid push square
        $pushTo = null;
        $chainPushTarget = null;
        if ($attacker->hasSkill(SkillName::Grab) && !$attacker->hasSkill(SkillName::Frenzy) && !$offPitchAvailable) {
            // Grab: optional skill — attacker chooses NOT to use it when crowd surf is available
            // When used: attacker chooses worst square for defender (most enemy TZs)
            if ($emptySquares !== []) {
                usort($emptySquares, function (Position $a, Position $b) use ($state, $defender) {
                    $tzA = $this->tzCalc->countTacklezones($state, $a, $defender->getTeamSide());
                    $tzB = $this->tzCalc->countTacklezones($state, $b, $defender->getTeamSide());
                    return $tzB <=> $tzA; // most TZ first
                });
                $pushTo = $emptySquares[0];
            } elseif ($occupiedSquares !== []) {
                // Chain push fallback: pick first chain-pushable square
                $pushTo = $occupiedSquares[0]['pos'];
                $chainPushTarget = $occupiedSquares[0]['player'];
            }
            // else: no valid squares = crowd surf (pushTo stays null)
        } elseif ($defender->hasSkill(SkillName::SideStep)) {
            // Side Step: defender chooses safest push square (fewest enemy TZs)
            if ($emptySquares !== []) {
                usort($emptySquares, function (Position $a, Position $b) use ($state, $defender) {
                    $tzA = $this->tzCalc->countTacklezones($state, $a, $defender->getTeamSide());
                    $tzB = $this->tzCalc->countTacklezones($state, $b, $defender->getTeamSide());
                    return $tzA <=> $tzB;
                });
                $pushTo = $emptySquares[0];
            } elseif ($occupiedSquares !== []) {
                // Chain push fallback: defender picks safest occupied square
                usort($occupiedSquares, function (array $a, array $b) use ($state, $defender) {
                    $tzA = $this->tzCalc->countTacklezones($state, $a['pos'], $defender->getTeamSide());
                    $tzB = $this->tzCalc->countTacklezones($state, $b['pos'], $defender->getTeamSide());
                    return $tzA <=> $tzB;
                });
                $pushTo = $occupiedSquares[0]['pos'];
                $chainPushTarget = $occupiedSquares[0]['player'];
            }
        } else {
            // Normal: attacker chooses best push (crowd surf > most enemy TZs > sideline)
            if ($offPitchAvailable) {
                // Crowd surf preferred — pushTo stays null
            } elseif ($emptySquares !== []) {
                usort($emptySquares, function (Position $a, Position $b) use ($state, $defender) {
                    $tzA = $this->tzCalc->countTacklezones($state, $a, $defender->getTeamSide());
                    $tzB = $this->tzCalc->countTacklezones($state, $b, $defender->getTeamSide());
                    if ($tzA !== $tzB) {
                        return $tzB <=> $tzA; // most TZs first (worst for defender)
                    }
                    $sideA = min($a->getY(), 14 - $a->getY());
                    $sideB = min($b->getY(), 14 - $b->getY());
                    return $sideA <=> $sideB; // closer to sideline first
                });
                $pushTo = $emptySquares[0];
            } elseif ($occupiedSquares !== []) {
                // Chain push: all on-pitch empty squares taken, pick first chain-pushable
                $pushTo = $occupiedSquares[0]['pos'];
                $chainPushTarget = $occupiedSquares[0]['player'];
            }
            // else: no valid targets → crowd surf
        }

        if ($pushTo !== null && $chainPushTarget !== null) {
            // Chain push: resolve chain first, then push defender into vacated square
                [$state, $events] = $this->resolveChainPush(
                    $state, $defender, $chainPushTarget, $pushTo, $defenderOriginalPos, $events,
                );
                // Now the square should be vacated — push defender there
                $events[] = GameEvent::playerPushed($defender->getId(), (string) $defenderOriginalPos, (string) $pushTo);
                $defender = $defender->withPosition($pushTo);
                $state = $state->withPlayer($defender);

                // Handle ball for pushed player
                if ($state->getBall()->getCarrierId() === $defender->getId()) {
                    if ($attacker->hasSkill(SkillName::StripBall)) {
                        $events[] = GameEvent::ballStripped($defender->getId());
                        $state = $state->withBall(BallState::onGround($pushTo));
                        $bounceResult = $this->ballResolver->resolveBounce($state, $pushTo);
                        $events = array_merge($events, $bounceResult['events']);
                        $state = $bounceResult['state'];
                    } else {
                        $state = $state->withBall(BallState::carried($pushTo, $defender->getId()));
                    }
                }

                return [$state, $events];
        }

        if ($pushTo !== null) {
            // Normal push
            $events[] = GameEvent::playerPushed($defender->getId(), (string) $defenderOriginalPos, (string) $pushTo);
            $defender = $defender->withPosition($pushTo);
            $state = $state->withPlayer($defender);

            // Handle ball for pushed player
            if ($state->getBall()->getCarrierId() === $defender->getId()) {
                if ($attacker->hasSkill(SkillName::StripBall)) {
                    // Strip Ball: ball drops at push destination and bounces
                    $events[] = GameEvent::ballStripped($defender->getId());
                    $state = $state->withBall(BallState::onGround($pushTo));
                    $bounceResult = $this->ballResolver->resolveBounce($state, $pushTo);
                    $events = array_merge($events, $bounceResult['events']);
                    $state = $bounceResult['state'];
                } else {
                    $state = $state->withBall(BallState::carried($pushTo, $defender->getId()));
                }
            }
        } else {
            // Crowd surf - pushed off pitch or no valid square
            $events[] = GameEvent::crowdSurf($defender->getId());

            // Ball drops at original position if carried
            if ($state->getBall()->getCarrierId() === $defender->getId()) {
                $state = $state->withBall(BallState::onGround($defenderOriginalPos));
            }

            $defender = $defender->withPosition(null);
            $state = $state->withPlayer($defender);

            // Crowd injury (skip armor, straight to injury with +1)
            $injResult = $this->injuryResolver->resolveCrowdSurf($defender, $this->dice);
            $defender = $injResult['player'];
            $state = $state->withPlayer($defender);
            $events = array_merge($events, $injResult['events']);
        }

        return [$state, $events];
    }

    /**
     * Resolve a chain push: push the occupant out of the way recursively.
     *
     * @param list<GameEvent> $events
     * @return array{0: GameState, 1: list<GameEvent>}
     */
    private function resolveChainPush(
        GameState $state,
        MatchPlayerDTO $pusher,
        MatchPlayerDTO $chainTarget,
        Position $chainTargetPos,
        Position $pusherOriginalPos,
        array $events,
    ): array {
        // Calculate push direction for chain target (same vector as pusher → target)
        $chainPushSquares = $this->getPushbackSquares($pusherOriginalPos, $chainTargetPos);

        // Categorize chain push squares
        $chainPushTo = null;
        $nextChainTarget = null;
        $offPitchAvailable = false;
        $emptySquares = [];
        $chainableSquares = [];

        foreach ($chainPushSquares as $pos) {
            if (!$pos->isOnPitch()) {
                $offPitchAvailable = true;
                continue;
            }
            $occupant = $state->getPlayerAtPosition($pos);
            if ($occupant === null) {
                $emptySquares[] = $pos;
            } elseif (!$occupant->hasSkill(SkillName::StandFirm)) {
                $chainableSquares[] = ['pos' => $pos, 'player' => $occupant];
            }
            // Stand Firm occupants skipped — can't push into them
        }

        if ($offPitchAvailable) {
            // Crowd surf for chain target — pushTo stays null
        } elseif ($emptySquares !== []) {
            $chainPushTo = $emptySquares[0];
        } elseif ($chainableSquares !== []) {
            // Recursive chain push
            $chainPushTo = $chainableSquares[0]['pos'];
            $nextChainTarget = $chainableSquares[0]['player'];
        }

        if ($chainPushTo !== null && $nextChainTarget !== null) {
            // Recursive chain: push next occupant first
            [$state, $events] = $this->resolveChainPush(
                $state, $chainTarget, $nextChainTarget, $chainPushTo, $chainTargetPos, $events,
            );
        }

        if ($chainPushTo !== null) {
            // Chain push to empty (or now-vacated) square
            $events[] = GameEvent::chainPush($chainTarget->getId(), (string) $chainTargetPos, (string) $chainPushTo);
            $chainTarget = $chainTarget->withPosition($chainPushTo);
            $state = $state->withPlayer($chainTarget);

            // Handle ball for chain-pushed player
            if ($state->getBall()->getCarrierId() === $chainTarget->getId()) {
                $state = $state->withBall(BallState::onGround($chainPushTo));
                $bounceResult = $this->ballResolver->resolveBounce($state, $chainPushTo);
                $events = array_merge($events, $bounceResult['events']);
                $state = $bounceResult['state'];
            }
        } else {
            // Chain push off pitch — crowd surf
            $events[] = GameEvent::chainPush($chainTarget->getId(), (string) $chainTargetPos, 'off-pitch');
            $events[] = GameEvent::crowdSurf($chainTarget->getId());

            if ($state->getBall()->getCarrierId() === $chainTarget->getId()) {
                $state = $state->withBall(BallState::onGround($chainTargetPos));
            }

            $chainTarget = $chainTarget->withPosition(null);
            $state = $state->withPlayer($chainTarget);

            $injResult = $this->injuryResolver->resolveCrowdSurf($chainTarget, $this->dice);
            $chainTarget = $injResult['player'];
            $state = $state->withPlayer($chainTarget);
            $events = array_merge($events, $injResult['events']);
        }

        return [$state, $events];
    }

    /**
     * Get valid pushback squares (away from attacker).
     *
     * @return list<Position>
     */
    private function getPushbackSquares(Position $attackerPos, Position $defenderPos): array
    {
        $dx = $defenderPos->getX() - $attackerPos->getX();
        $dy = $defenderPos->getY() - $attackerPos->getY();

        // Normalize direction
        $ndx = $dx === 0 ? 0 : ($dx > 0 ? 1 : -1);
        $ndy = $dy === 0 ? 0 : ($dy > 0 ? 1 : -1);

        $squares = [];

        // Direct push (primary direction)
        $squares[] = new Position($defenderPos->getX() + $ndx, $defenderPos->getY() + $ndy);

        // Two diagonal alternatives
        if ($ndx === 0) {
            // Vertical push - diagonals are left and right
            $squares[] = new Position($defenderPos->getX() - 1, $defenderPos->getY() + $ndy);
            $squares[] = new Position($defenderPos->getX() + 1, $defenderPos->getY() + $ndy);
        } elseif ($ndy === 0) {
            // Horizontal push - diagonals are up and down
            $squares[] = new Position($defenderPos->getX() + $ndx, $defenderPos->getY() - 1);
            $squares[] = new Position($defenderPos->getX() + $ndx, $defenderPos->getY() + 1);
        } else {
            // Diagonal push - alternatives are the two adjacent directions
            $squares[] = new Position($defenderPos->getX() + $ndx, $defenderPos->getY());
            $squares[] = new Position($defenderPos->getX(), $defenderPos->getY() + $ndy);
        }

        return $squares;
    }

    /**
     * Find the best dump-off target: closest friendly teammate adjacent to the ball carrier.
     */
    private function findDumpOffTarget(GameState $state, MatchPlayerDTO $carrier): ?Position
    {
        $carrierPos = $carrier->getPosition();
        if ($carrierPos === null) {
            return null;
        }

        $best = null;
        foreach ($state->getPlayersOnPitch($carrier->getTeamSide()) as $teammate) {
            if ($teammate->getId() === $carrier->getId()) {
                continue;
            }
            if ($teammate->getState() !== PlayerState::STANDING) {
                continue;
            }
            $tPos = $teammate->getPosition();
            if ($tPos === null) {
                continue;
            }
            // Must be within quick pass range (distance <= 3)
            if ($carrierPos->distanceTo($tPos) <= 3) {
                $best = $tPos;
                break;
            }
        }

        return $best;
    }

    private function rollBlockDie(): BlockDiceFace
    {
        $roll = $this->dice->rollD6();
        return match ($roll) {
            1 => BlockDiceFace::ATTACKER_DOWN,
            2 => BlockDiceFace::BOTH_DOWN,
            3, 4 => BlockDiceFace::PUSHED,
            5 => BlockDiceFace::DEFENDER_STUMBLES,
            default => BlockDiceFace::DEFENDER_DOWN,
        };
    }

    /**
     * Auto-choose the best block die face.
     *
     * @param list<BlockDiceFace> $faces
     */
    private function autoChooseBlockDie(
        array $faces,
        bool $attackerChooses,
        MatchPlayerDTO $attacker,
        MatchPlayerDTO $defender,
    ): BlockDiceFace {
        $scored = [];
        foreach ($faces as $face) {
            $scored[] = [$face, $this->scoreBlockFace($face, $attacker, $defender)];
        }

        // Attacker wants highest score, defender wants lowest
        usort($scored, fn(array $a, array $b) => $attackerChooses
            ? $b[1] <=> $a[1]
            : $a[1] <=> $b[1],
        );

        return $scored[0][0];
    }

    private function scoreBlockFace(BlockDiceFace $face, MatchPlayerDTO $attacker, MatchPlayerDTO $defender): int
    {
        $attackerHasBlock = $attacker->hasSkill(SkillName::Block);
        $attackerHasWrestle = $attacker->hasSkill(SkillName::Wrestle);
        $defenderHasBlock = $defender->hasSkill(SkillName::Block);
        $defenderHasDodge = $defender->hasSkill(SkillName::Dodge);
        $attackerHasTackle = $attacker->hasSkill(SkillName::Tackle);

        return match ($face) {
            BlockDiceFace::DEFENDER_DOWN, BlockDiceFace::POW => 100,
            BlockDiceFace::DEFENDER_STUMBLES => ($defenderHasDodge && !$attackerHasTackle) ? 30 : 80,
            BlockDiceFace::PUSHED => 30,
            BlockDiceFace::BOTH_DOWN => $attackerHasBlock
                ? ($defenderHasBlock ? 20 : 90)
                : ($attackerHasWrestle ? 25 : -50),
            BlockDiceFace::ATTACKER_DOWN => -100,
        };
    }
}
