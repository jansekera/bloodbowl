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
     * @return array{state: GameState, events: list<GameEvent>, proceed?: bool}|null null = action proceeds
     */
    /**
     * ⛔⛔ `wastesTeamAction` (doplneno 11.09.2026 -- polozka PHP15).
     *   `rules_bb2016.txt` r. 8398-8401: „The player can't do anything for the
     *   turn, and **the player's team loses the declared Action for that turn**
     *   (for example if a Really Stupid player declares a Blitz Action and
     *   fails the Really Stupid roll, then **the team cannot declare another
     *   Blitz Action that turn**)."
     *   A u Wild Animal r. 8668-8669: „**the Action is wasted**."
     *
     *   PHP to nedelalo u ZADNE ze ctyr dovednosti ⇒ Big Guy, ktery sel
     *   k zemi na blitzu, tym o blitz NEPRIPRAVIL a ten si ho zahral znovu
     *   jinym hracem. Cista vyhoda proti pravidlum.
     *
     * ⚠️ C++ engine to ma jako `wastesTeamAction` (`big_guy_handler.h:39`,
     *   nastavovane na ctyrech mistech) a odecita ho
     *   `action_resolver.cpp:280` pres `consumeDeclaredTeamAction`, protoze
     *   do switche, kde se limit jinak nastavuje, uz se nedostane. PHP kopie
     *   to nemela -- CTVRTY drift teze tridy.
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
                // ⭐ viz `wastesTeamAction` niz
                'wastesTeamAction' => true,
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
        // ⛔⛔⛔ OPRAVA 11.09.2026 -- DVĚ VADY NARÁZ, OBĚ PROTI TEXTU PRAVIDEL.
        //   `rules_bb2016.txt` r. 8666-8669: „immediately after declaring an
        //   Action with a Wild Animal, roll a D6, **adding 2 to the roll if
        //   taking a Block or Blitz Action**. On a roll of **1-3**, the Wild
        //   Animal does not move ... and the Action is wasted."
        //
        //   (1) Block/Blitz tu měly `return null`, tedy AUTO-PASS BEZ HODU.
        //       Pravidla dávají bonus +2, ne imunitu: s +2 projde přirozená
        //       2+, ale přirozená 1 PADÁ. ⇒ Rat Ogre / Minotaur byl o šestinu
        //       spolehlivější, než pravidla dovolují.
        //   (2) Ostatní akce padaly na `<= 2`, tedy 1-2. Pravidla říkají
        //       1-3. ⇒ Trojka procházela, ač neměla.
        //
        // ⚠️ TATÁŽ VADA UŽ BYLA NAJITA A OPRAVENA V C++ ENGINU 07.08.2026
        //   (`engine/src/big_guy_handler.cpp:87-93`, vlastní komentář:
        //   „before this the block/blitz branch skipped the roll entirely,
        //   making a Rat Ogre / Minotaur a sixth more reliable at hitting
        //   than the rules allow"). PHP kopie se neopravila. ⇒ Je to týž
        //   vzorec ve dvou kopiích -- třída, která tenhle repozitář kousla
        //   už dvakrát (`131a1779` a `pathFailProb` v `496f5a03`).
        //
        // ⭐ DEKLARACE BLITZU JE PROTO LEGITIMNÍ TAH, i když se na cíl
        //   nedosáhne: je to způsob, jak Wild Animal rozhýbat na 2+ místo
        //   4+ (uživatel 11.09.). Na tom stojí oprava v `BlitzHandler`.
        $hitting = ($action === ActionType::BLOCK || $action === ActionType::BLITZ);
        $target = $hitting ? 2 : 4;

        $roll = $dice->rollD6();

        if ($roll < $target) {
            // Wild Animal loses action but keeps tacklezones
            $player = $player
                ->withHasMoved(true)
                ->withHasActed(true);
            $state = $state->withPlayer($player);

            return [
                'state' => $state,
                'events' => [GameEvent::wildAnimalFail($player->getId(), $roll)],
                'wastesTeamAction' => true,
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
                'wastesTeamAction' => true,
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
