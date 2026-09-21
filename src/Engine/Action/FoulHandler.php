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
use App\Engine\StrengthCalculator;

final class FoulHandler implements ActionHandlerInterface
{
    public function __construct(
        private readonly DiceRollerInterface $dice,
        private readonly InjuryResolver $injuryResolver,
        private readonly BallResolver $ballResolver,
        private readonly StrengthCalculator $strengthCalculator = new StrengthCalculator(),
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
        // ⛔⛔ OPRAVENO 16.09.2026 -- tady byl pausalni `+1` ("prone bonus"), ktery
        //   `rules_bb2016.txt` nezna. Modifikatorem hodu na brneni jsou ASISTENCE
        //   (r. 1843-1850): +1 za kazdeho spoluhrace vedle OBETI, -1 za kazdeho soupere
        //   vedle FAULUJICIHO; nesmi stat v zone soupere, musi mit zony a stat.
        //   Guard se u faulu pouzit nesmi (r. 8161).
        $asistence = $this->strengthCalculator->countFoulAssists($state, $attacker, $defender);
        $modifier = $asistence;

        // ⛔ OPRAVA 21.09.2026: `rules_bb2016.txt` r. 8045-8049 -- "Add 1 to any
        //   Armour roll **or** Injury roll made by a player with this skill when
        //   they make a Foul ... Note that you may **only modify one** of the
        //   dice rolls." Do ted se +1 davalo VZDY na brneni, i kdyz brneni
        //   prorazilo samo -- tim se bonus zahodil.
        //   ⭐ Doktrina uzivatele "vol nejlepsi vysledek" (15.09.): bonus jde na
        //   brneni jen tehdy, kdyz o nem rozhodne; jinak si ho schovame na
        //   zraneni. Rozhodnout se smi az po hodu na brneni -- pravidla nerikaji,
        //   ze se volba hlasi predem.
        $armourValue = $defender->getStats()->getArmour();
        $maDirtyPlayer = $attacker->hasSkill(SkillName::DirtyPlayer);

        $bezBonusu = $die1 + $die2 + $modifier;
        $bonusNaBrneni = $maDirtyPlayer && $bezBonusu <= $armourValue && ($bezBonusu + 1) > $armourValue;
        if ($bonusNaBrneni) {
            $modifier++;
        }

        $total = $die1 + $die2 + $modifier;
        $armourBroken = $total > $armourValue;

        // Kdyz se bonus na brneni nepouzil a hod na zraneni bude, patri tam.
        $bonusNaZraneni = ($maDirtyPlayer && !$bonusNaBrneni) ? 1 : 0;

        $events[] = GameEvent::foulAttempt($attackerId, $targetId, $die1, $die2, $armourValue, $armourBroken, $modifier);

        // Handle armor broken -> injury roll (no Mighty Blow for fouls)
        $dubletNaZraneni = false;
        if ($armourBroken) {
            $hasStakes = $attacker->hasSkill(SkillName::Stakes);
            $hasNurglesRot = $attacker->hasSkill(SkillName::NurglesRot);
            $injResult = $this->injuryResolver->resolveInjuryOnly($defender, $this->dice, $bonusNaZraneni, $hasStakes, $hasNurglesRot);
            $defender = $injResult['player'];
            $state = $state->withPlayer($defender);
            $events = array_merge($events, $injResult['events']);
            $dubletNaZraneni = $injResult['dice'][0] === $injResult['dice'][1];
        }

        // ⛔ OPRAVA 21.09.2026: `rules_bb2016.txt` r. 1877-1878 -- "if the Armour
        //   **and/or Injury** roll is a doubles (i.e., two 1s, or two 2s, etc),
        //   the referee has spotted the foul". Do ted se dublet cetl JEN na
        //   brneni, protoze `resolveInjury` vracel pouhy soucet 2D6.
        // Check for ejection (doubles) — Sneaky Git avoids ejection
        $vyloucen = false;
        if (($die1 === $die2 || $dubletNaZraneni) && !$attacker->hasSkill(SkillName::SneakyGit)) {
            $vyloucen = true;
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

        // ⛔ OPRAVENO 16.09.2026 -- vylouceni faulujiciho JE turnover.
        //   r. 1879-1881: "the player taking the Foul Action is sent off ... In addition,
        //   his team suffers a turnover and their turn ends immediately."
        if ($vyloucen) {
            return ActionResult::turnover($state->withTurnoverPending(true), $events);
        }

        return ActionResult::success($state, $events);
    }
}
