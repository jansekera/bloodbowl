<?php

declare(strict_types=1);

namespace App\Engine\Action;

use App\DTO\ActionResult;
use App\DTO\BallState;
use App\DTO\GameEvent;
use App\DTO\GameState;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Engine\BallResolver;
use App\Engine\DiceRollerInterface;
use App\Engine\InjuryResolver;

final class FoulHandler implements ActionHandlerInterface
{
    public function __construct(
        private readonly DiceRollerInterface $dice,
        private readonly InjuryResolver $injuryResolver,
        private readonly BallResolver $ballResolver,
    ) {
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

        $attackerPos = $attacker->getPosition();
        $defenderPos = $defender->getPosition();

        if ($attackerPos === null || $defenderPos === null) {
            throw new \InvalidArgumentException('Players must be on pitch');
        }

        if ($attackerPos->distanceTo($defenderPos) !== 1) {
            throw new \InvalidArgumentException('Players must be adjacent to foul');
        }

        // Mark foul used for this turn
        $activeSide = $attacker->getTeamSide();
        $teamState = $state->getTeamState($activeSide);
        $state = $state->withTeamState($activeSide, $teamState->withFoulUsed());

        // Mark attacker as acted
        $state = $state->withPlayer($attacker->withHasActed(true)->withHasMoved(true));

        $events = [];

        // Roll 2xD6 individually for doubles detection
        $die1 = $this->dice->rollD6();
        $die2 = $this->dice->rollD6();
        $total = $die1 + $die2 + 1; // +1 prone bonus

        // Dirty Player: +1 to foul armor roll
        if ($attacker->hasSkill(SkillName::DirtyPlayer)) {
            $total++;
        }

        $armourValue = $defender->getStats()->getArmour();
        $armourBroken = $total > $armourValue;

        $events[] = GameEvent::foulAttempt($attackerId, $targetId, $die1, $die2, $armourValue, $armourBroken);

        // Handle armor broken -> injury roll (no Mighty Blow for fouls)
        if ($armourBroken) {
            $hasStakes = $attacker->hasSkill(SkillName::Stakes);
            $hasNurglesRot = $attacker->hasSkill(SkillName::NurglesRot);
            $injResult = $this->injuryResolver->resolveInjuryOnly($defender, $this->dice, 0, $hasStakes, $hasNurglesRot);
            $defender = $injResult['player'];
            $state = $state->withPlayer($defender);
            $events = array_merge($events, $injResult['events']);
        }

        // Check for ejection (doubles) — Sneaky Git avoids ejection
        if ($die1 === $die2 && !$attacker->hasSkill(SkillName::SneakyGit)) {
            $events[] = GameEvent::playerEjected($attackerId);

            // Get fresh attacker from state
            $freshAttacker = $state->getPlayer($attackerId);
            if ($freshAttacker !== null) {
                $ejectedPos = $freshAttacker->getPosition();

                // Handle ball if fouler was carrying it
                if ($state->getBall()->getCarrierId() === $attackerId && $ejectedPos !== null) {
                    $state = $state->withBall(BallState::onGround($ejectedPos));
                    $bounceResult = $this->ballResolver->resolveBounce($state, $ejectedPos);
                    $events = array_merge($events, $bounceResult['events']);
                    $state = $bounceResult['state'];
                }

                $state = $state->withPlayer(
                    $freshAttacker->withState(PlayerState::EJECTED)->withPosition(null),
                );
            }
        }

        // Foul is NEVER a turnover (even with ejection)
        return ActionResult::success($state, $events);
    }
}
