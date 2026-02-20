<?php
declare(strict_types=1);

namespace App\Engine;

use App\DTO\ActionResult;
use App\DTO\GameEvent;
use App\DTO\GameState;
use App\DTO\MatchPlayerDTO;
use App\Enum\ActionType;
use App\Enum\PlayerState;
use App\Enum\SkillName;

final class BigGuyCheckResolver
{
    /**
     * Resolve pre-action check for Big Guy negatraits.
     * Returns null if the action can proceed, or an ActionResult if blocked.
     *
     * @return array{state: GameState, events: list<GameEvent>}|null null = action proceeds
     */
    public function resolvePreActionCheck(
        GameState $state,
        MatchPlayerDTO $player,
        ActionType $action,
        DiceRollerInterface $dice,
    ): ?array {
        if ($player->hasSkill(SkillName::BoneHead)) {
            return $this->resolveBoneHead($state, $player, $dice);
        }

        if ($player->hasSkill(SkillName::ReallyStupid)) {
            return $this->resolveReallyStupid($state, $player, $dice);
        }

        if ($player->hasSkill(SkillName::WildAnimal)) {
            return $this->resolveWildAnimal($state, $player, $action, $dice);
        }

        if ($player->hasSkill(SkillName::TakeRoot) && $action === ActionType::MOVE) {
            return $this->resolveTakeRoot($state, $player, $dice);
        }

        if ($player->hasSkill(SkillName::Bloodlust)) {
            return $this->resolveBloodlust($state, $player, $dice);
        }

        return null;
    }

    /**
     * Bone-head: Roll D6, on 1 = lose action + lose tacklezones.
     *
     * @return array{state: GameState, events: list<GameEvent>}|null
     */
    private function resolveBoneHead(
        GameState $state,
        MatchPlayerDTO $player,
        DiceRollerInterface $dice,
    ): ?array {
        $roll = $dice->rollD6();

        if ($roll === 1) {
            $player = $player
                ->withLostTacklezones(true)
                ->withHasMoved(true)
                ->withHasActed(true);
            $state = $state->withPlayer($player);

            return [
                'state' => $state,
                'events' => [GameEvent::boneHeadFail($player->getId(), $roll)],
            ];
        }

        return null;
    }

    /**
     * Really Stupid: Roll D6, need 2+ (adjacent ally) or 4+ (no ally).
     * On fail = lose action + lose tacklezones.
     *
     * @return array{state: GameState, events: list<GameEvent>}|null
     */
    private function resolveReallyStupid(
        GameState $state,
        MatchPlayerDTO $player,
        DiceRollerInterface $dice,
    ): ?array {
        $hasAdjacentAlly = $this->hasAdjacentTeammate($state, $player);
        $threshold = $hasAdjacentAlly ? 2 : 4;

        $roll = $dice->rollD6();

        if ($roll < $threshold) {
            $player = $player
                ->withLostTacklezones(true)
                ->withHasMoved(true)
                ->withHasActed(true);
            $state = $state->withPlayer($player);

            return [
                'state' => $state,
                'events' => [GameEvent::reallyStupidFail($player->getId(), $roll, $hasAdjacentAlly)],
            ];
        }

        return null;
    }

    /**
     * Wild Animal: Roll D6, on 1-2 = lose action (keeps tacklezones).
     * Block/Blitz = automatic success.
     *
     * @return array{state: GameState, events: list<GameEvent>}|null
     */
    private function resolveWildAnimal(
        GameState $state,
        MatchPlayerDTO $player,
        ActionType $action,
        DiceRollerInterface $dice,
    ): ?array {
        // Block and Blitz auto-pass
        if ($action === ActionType::BLOCK || $action === ActionType::BLITZ) {
            return null;
        }

        $roll = $dice->rollD6();

        if ($roll <= 2) {
            // Wild Animal loses action but keeps tacklezones
            $player = $player
                ->withHasMoved(true)
                ->withHasActed(true);
            $state = $state->withPlayer($player);

            return [
                'state' => $state,
                'events' => [GameEvent::wildAnimalFail($player->getId(), $roll)],
            ];
        }

        return null;
    }

    /**
     * Take Root: Roll D6 on move, 1 = cannot move.
     * Block/Blitz not affected.
     *
     * @return array{state: GameState, events: list<GameEvent>}|null
     */
    private function resolveTakeRoot(
        GameState $state,
        MatchPlayerDTO $player,
        DiceRollerInterface $dice,
    ): ?array {
        $roll = $dice->rollD6();

        if ($roll === 1) {
            $player = $player
                ->withHasMoved(true)
                ->withHasActed(true);
            $state = $state->withPlayer($player);

            return [
                'state' => $state,
                'events' => [GameEvent::takeRoot($player->getId(), $roll, true)],
            ];
        }

        return null;
    }

    /**
     * Bloodlust: Roll D6, need 2+. On fail: bite adjacent Thrall or lose action.
     *
     * @return array{state: GameState, events: list<GameEvent>}|null
     */
    private function resolveBloodlust(
        GameState $state,
        MatchPlayerDTO $player,
        DiceRollerInterface $dice,
    ): ?array {
        $roll = $dice->rollD6();

        if ($roll >= 2) {
            return null; // Pass — action proceeds normally
        }

        // Failed: look for adjacent Thrall (non-Vampire teammate)
        $thrall = $this->findAdjacentThrall($state, $player);

        if ($thrall !== null) {
            // Bite Thrall: Thrall goes to KO box, Vampire proceeds with action
            $thrall = $thrall->withPosition(null)->withState(PlayerState::KO);
            $state = $state->withPlayer($thrall);

            // Emit bite event but return null — action proceeds
            // We need to return a special result that modifies state but allows action to continue
            // Since BigGuyCheckResolver returns null for "proceed", we must update state via the player
            // Trick: we return null and rely on the state being updated... but that doesn't work
            // because state is passed by value. We need to return state modifications.
            // Solution: return the events and state, but mark it as "proceed" in a different way.

            // Actually, looking at the caller: ActionResolver checks if checkResult !== null to block.
            // For Bloodlust bite, we want to modify state AND allow the action to continue.
            // Best approach: return state+events with a special 'proceed' flag.
            // But the current interface is ?array — null means proceed, array means blocked.

            // Simplest solution: use a different return format that the caller can detect.
            // Let's return with a 'proceed' key set to true.
            return [
                'state' => $state,
                'events' => [GameEvent::bloodlustBite($player->getId(), $thrall->getId(), $roll)],
                'proceed' => true,
            ];
        }

        // No Thrall: Vampire loses action and is moved to reserves
        $player = $player
            ->withPosition(null)
            ->withState(PlayerState::OFF_PITCH)
            ->withHasMoved(true)
            ->withHasActed(true);
        $state = $state->withPlayer($player);

        return [
            'state' => $state,
            'events' => [GameEvent::bloodlustFail($player->getId(), $roll)],
        ];
    }

    /**
     * Find an adjacent friendly Thrall (non-Vampire teammate) for Bloodlust bite.
     */
    private function findAdjacentThrall(GameState $state, MatchPlayerDTO $vampire): ?MatchPlayerDTO
    {
        $pos = $vampire->getPosition();
        if ($pos === null) {
            return null;
        }

        foreach ($state->getPlayersOnPitch($vampire->getTeamSide()) as $teammate) {
            if ($teammate->getId() === $vampire->getId()) {
                continue;
            }
            // Thrall = any teammate without Bloodlust (non-Vampire)
            if ($teammate->hasSkill(SkillName::Bloodlust)) {
                continue;
            }
            $teammatePos = $teammate->getPosition();
            if ($teammatePos !== null && $pos->distanceTo($teammatePos) === 1 && $teammate->getState() === PlayerState::STANDING) {
                return $teammate;
            }
        }

        return null;
    }

    private function hasAdjacentTeammate(GameState $state, MatchPlayerDTO $player): bool
    {
        $pos = $player->getPosition();
        if ($pos === null) {
            return false;
        }

        foreach ($state->getPlayersOnPitch($player->getTeamSide()) as $teammate) {
            if ($teammate->getId() === $player->getId()) {
                continue;
            }
            $teammatePos = $teammate->getPosition();
            if ($teammatePos !== null && $pos->distanceTo($teammatePos) === 1) {
                return true;
            }
        }

        return false;
    }
}
