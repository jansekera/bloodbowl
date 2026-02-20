<?php
declare(strict_types=1);

namespace App\Engine;

use App\DTO\GameEvent;
use App\DTO\MatchPlayerDTO;
use App\Enum\PlayerState;
use App\Enum\SkillName;

final class InjuryResolver
{
    /**
     * Resolve armor roll and potentially injury for a knocked-down player.
     *
     * @return array{player: MatchPlayerDTO, events: list<GameEvent>}
     */
    public function resolve(
        MatchPlayerDTO $player,
        DiceRollerInterface $dice,
        int $armourModifier = 0,
        int $injuryModifier = 0,
        bool $hasClaw = false,
        bool $hasStakes = false,
        bool $hasNurglesRot = false,
    ): array {
        $events = [];

        // Armor roll: 2D6 > AV = armor broken
        $armourRoll = $dice->roll2D6();
        $modifiedRoll = $armourRoll + $armourModifier;
        $armourValue = $player->getStats()->getArmour();
        // Claw: armor broken on 8+ regardless of AV
        $armourBroken = ($hasClaw && $armourRoll >= 8) || ($modifiedRoll > $armourValue);

        $events[] = GameEvent::armourRoll(
            $player->getId(),
            $armourRoll,
            $armourModifier,
            $armourValue,
            $armourBroken,
        );

        if (!$armourBroken) {
            return ['player' => $player, 'events' => $events];
        }

        return $this->resolveInjury($player, $dice, $injuryModifier, $events, $hasStakes, $hasNurglesRot);
    }

    /**
     * Resolve with a pre-rolled armor value (used by Chainsaw).
     *
     * @return array{player: MatchPlayerDTO, events: list<GameEvent>}
     */
    public function resolveWithPrerolledArmor(
        MatchPlayerDTO $player,
        DiceRollerInterface $dice,
        int $armourRoll,
        int $armourModifier = 0,
        int $injuryModifier = 0,
        bool $hasClaw = false,
        bool $hasStakes = false,
        bool $hasNurglesRot = false,
    ): array {
        $events = [];
        $modifiedRoll = $armourRoll + $armourModifier;
        $armourValue = $player->getStats()->getArmour();
        $armourBroken = ($hasClaw && $armourRoll >= 8) || ($modifiedRoll > $armourValue);

        $events[] = GameEvent::armourRoll(
            $player->getId(),
            $armourRoll,
            $armourModifier,
            $armourValue,
            $armourBroken,
        );

        if (!$armourBroken) {
            return ['player' => $player, 'events' => $events];
        }

        return $this->resolveInjury($player, $dice, $injuryModifier, $events, $hasStakes, $hasNurglesRot);
    }

    /**
     * Resolve injury only (no armor roll). Used by foul when armor is already broken.
     *
     * @return array{player: MatchPlayerDTO, events: list<GameEvent>}
     */
    public function resolveInjuryOnly(
        MatchPlayerDTO $player,
        DiceRollerInterface $dice,
        int $modifier = 0,
        bool $hasStakes = false,
        bool $hasNurglesRot = false,
    ): array {
        return $this->resolveInjury($player, $dice, $modifier, [], $hasStakes, $hasNurglesRot);
    }

    /**
     * Resolve crowd surf injury (no armor roll, straight to injury with +1).
     *
     * @return array{player: MatchPlayerDTO, events: list<GameEvent>}
     */
    public function resolveCrowdSurf(
        MatchPlayerDTO $player,
        DiceRollerInterface $dice,
    ): array {
        return $this->resolveInjury($player, $dice, 1, []);
    }

    /**
     * Re-roll injury with apothecary (take better result).
     * Returns the better of original and re-rolled results.
     *
     * @param list<GameEvent> $events
     * @return array{player: MatchPlayerDTO, events: list<GameEvent>}
     */
    public function resolveApothecary(
        MatchPlayerDTO $player,
        DiceRollerInterface $dice,
        PlayerState $originalState,
        int $originalModifier,
        array $events,
    ): array {
        // Re-roll injury
        $rerollResult = $this->resolveInjury($player, $dice, $originalModifier, []);
        $rerolledState = $rerollResult['player']->getState();

        // Take the better result (stunned > KO > injured)
        $stateOrder = [
            PlayerState::STUNNED->value => 0,
            PlayerState::KO->value => 1,
            PlayerState::INJURED->value => 2,
        ];

        $originalScore = $stateOrder[$originalState->value] ?? 2;
        $rerolledScore = $stateOrder[$rerolledState->value] ?? 2;

        if ($rerolledScore <= $originalScore) {
            // Reroll is same or better
            $events[] = GameEvent::apothecaryUsed(
                $player->getId(),
                $originalState->value,
                $rerolledState->value,
            );
            $events = array_merge($events, $rerollResult['events']);
            return ['player' => $rerollResult['player'], 'events' => $events];
        }

        // Original was better, keep it
        $events[] = GameEvent::apothecaryUsed(
            $player->getId(),
            $originalState->value,
            $originalState->value,
        );

        // Restore original state on the player
        $restoredPlayer = $player->withState($originalState);
        if ($originalState === PlayerState::KO || $originalState === PlayerState::INJURED) {
            $restoredPlayer = $restoredPlayer->withPosition(null);
        }

        return ['player' => $restoredPlayer, 'events' => $events];
    }

    /**
     * @param list<GameEvent> $events
     * @return array{player: MatchPlayerDTO, events: list<GameEvent>}
     */
    private function resolveInjury(
        MatchPlayerDTO $player,
        DiceRollerInterface $dice,
        int $modifier,
        array $events,
        bool $hasStakes = false,
        bool $hasNurglesRot = false,
    ): array {
        $roll = $dice->roll2D6();
        // Decay: roll injury twice, take worse (higher) result
        if ($player->hasSkill(SkillName::Decay)) {
            $roll2 = $dice->roll2D6();
            $roll = max($roll, $roll2);
        }
        // Stunty: +1 to injury roll (more vulnerable)
        if ($player->hasSkill(SkillName::Stunty)) {
            $modifier++;
        }
        $modified = $roll + $modifier;

        if ($modified <= 7) {
            $player = $player->withState(PlayerState::STUNNED);
            $events[] = GameEvent::injuryRoll($player->getId(), $roll, $modifier, 'stunned');
        } elseif ($modified <= 9) {
            // Thick Skull: on KO result, roll D6; on 4+ → stunned instead
            if ($player->hasSkill(SkillName::ThickSkull)) {
                $thickSkullRoll = $dice->rollD6();
                if ($thickSkullRoll >= 4) {
                    $player = $player->withState(PlayerState::STUNNED);
                    $events[] = GameEvent::injuryRoll($player->getId(), $roll, $modifier, 'ko');
                    $events[] = GameEvent::rerollUsed($player->getId(), 'Thick Skull');
                    $events[] = GameEvent::injuryRoll($player->getId(), $thickSkullRoll, 0, 'stunned');
                } else {
                    $player = $player->withState(PlayerState::KO)->withPosition(null);
                    $events[] = GameEvent::injuryRoll($player->getId(), $roll, $modifier, 'ko');
                    $events[] = GameEvent::rerollUsed($player->getId(), 'Thick Skull');
                    $events[] = GameEvent::injuryRoll($player->getId(), $thickSkullRoll, 0, 'ko');
                }
            } else {
                $player = $player->withState(PlayerState::KO)->withPosition(null);
                $events[] = GameEvent::injuryRoll($player->getId(), $roll, $modifier, 'ko');
            }
        } else {
            $player = $player->withState(PlayerState::INJURED)->withPosition(null);
            $events[] = GameEvent::injuryRoll($player->getId(), $roll, $modifier, 'casualty');

            // Nurgle's Rot: flavor event when attacker has the skill
            if ($hasNurglesRot) {
                $events[] = GameEvent::nurglesRot(0, $player->getId());
            }

            // Regeneration: after casualty, roll D6; on 4+ player goes to reserves
            // Stakes: blocks Regeneration entirely
            if ($player->hasSkill(SkillName::Regeneration) && !$hasStakes) {
                $regenRoll = $dice->rollD6();
                if ($regenRoll >= 4) {
                    $player = $player->withState(PlayerState::OFF_PITCH);
                    $events[] = GameEvent::regeneration($player->getId(), $regenRoll, true);
                } else {
                    $events[] = GameEvent::regeneration($player->getId(), $regenRoll, false);
                }
            } elseif ($player->hasSkill(SkillName::Regeneration) && $hasStakes) {
                $events[] = GameEvent::stakesBlockRegen(0, $player->getId());
            }
        }

        return ['player' => $player, 'events' => $events];
    }
}
