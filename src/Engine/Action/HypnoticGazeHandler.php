<?php
declare(strict_types=1);

namespace App\Engine\Action;

use App\DTO\ActionResult;
use App\DTO\GameEvent;
use App\DTO\GameState;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Engine\DiceRollerInterface;
use App\Engine\TacklezoneCalculator;

final class HypnoticGazeHandler implements ActionHandlerInterface
{
    public function __construct(
        private readonly DiceRollerInterface $dice,
        private readonly TacklezoneCalculator $tzCalc,
    ) {
    }

    /**
     * @param array<string, mixed> $params {playerId, targetId}
     */
    public function resolve(GameState $state, array $params): ActionResult
    {
        $gazerId = (int) $params['playerId'];
        $targetId = (int) $params['targetId'];

        $gazer = $state->getPlayer($gazerId);
        $target = $state->getPlayer($targetId);

        if ($gazer === null || $target === null) {
            throw new \InvalidArgumentException('Player not found');
        }
        if (!$gazer->hasSkill(SkillName::HypnoticGaze)) {
            throw new \InvalidArgumentException('Player must have Hypnotic Gaze skill');
        }

        $gazerPos = $gazer->getPosition();
        $targetPos = $target->getPosition();

        if ($gazerPos === null || $targetPos === null) {
            throw new \InvalidArgumentException('Players must be on pitch');
        }
        if ($gazerPos->distanceTo($targetPos) !== 1) {
            throw new \InvalidArgumentException('Target must be adjacent');
        }

        // Mark gazer as acted
        $state = $state->withPlayer($gazer->withHasActed(true)->withHasMoved(true));

        // ⛔⛔ OPRAVA 11.09.2026: OBET SE DO MODIFIKATORU NEPOCITA.
        //   `rules_bb2016.txt` r. 8183-8185: „-1 modifier for each opposing
        //   tackle zone on the player with hypnotic gaze **other than the
        //   victim's**." Obecny `countTacklezones` tu vyjimku nezna a handler
        //   ji neodecital -- a protoze gaze VYZADUJE sousedstvi, obet
        //   prispivala VZDYCKY. Prah byl tedy systematicky o 1 vyssi.
        //
        // ⚠️ C++ to ma spravne: `engine/src/gaze_handler.cpp:28`
        //   `if (opp->id == targetId) continue;   // "other than the victim's"`.
        //   PHP kopie se neopravila -- TRETI drift v tomhle souboru (vedle
        //   turnoveru pri neuspechu) a tataz trida jako Wild Animal.
        //
        // ⭐ NASLO TO druha pulka testovaciho paru (`...SucceedsOneAbove`):
        //   test na modifikator sam o sobe prosel i se spatnym prahem,
        //   protoze tvrdil jen neuspech.
        $tz = 0;
        foreach ($state->getPlayersOnPitch($gazer->getTeamSide()->opponent()) as $opp) {
            if ($opp->getId() === $targetId) {
                continue;   // obet se nepocita
            }
            if (!$opp->getState()->exertsTacklezone() || $opp->hasLostTacklezones()) {
                continue;
            }
            $oppPos = $opp->getPosition();
            if ($oppPos !== null && $gazerPos->distanceTo($oppPos) === 1) {
                $tz++;
            }
        }
        $target_roll = min(6, 2 + $tz);

        $roll = $this->dice->rollD6();
        $success = $roll >= $target_roll;

        $events = [GameEvent::hypnoticGaze($gazerId, $targetId, $roll, $success)];

        if ($success) {
            // Target loses tackle zones
            $target = $target->withLostTacklezones(true);
            $state = $state->withPlayer($target);

            return ActionResult::success($state, $events);
        }

        // ⛔⛔⛔ OPRAVA 11.09.2026: TADY SE VYHLASOVAL TURNOVER A BYLA TO VADA.
        //   `rules_bb2016.txt` r. 8188-8189: „If the roll fails, then the
        //   hypnotic gaze **has no effect**." Zadny turnover.
        //   A uzavreny sedmicленny katalog turnoveru (r. 368-384) gaze
        //   NEOBSAHUJE -- neni to ani sraz, ani ztrata mice, ani fumble.
        //
        // ⚠️ C++ ENGINE TO MA SPRAVNE A VYSLOVNE OKOMENTOVANE:
        //   `engine/src/gaze_handler.cpp:21` („katalog turnoveru (l. 366-382)
        //   gaze vubec nezna") a `:44` („turnover to NENI"). PHP kopie se
        //   neopravila -- TATAZ DRIFT JAKO U Wild Animal (`80852863`).
        //
        // ⇒ Neuspech akci jen vycerpa (`hasActed`/`hasMoved` uz je nastaveno
        //   vys), kolo bezi dal.
        return ActionResult::success($state, $events);
    }
}
