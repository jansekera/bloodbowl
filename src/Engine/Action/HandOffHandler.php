<?php

declare(strict_types=1);

namespace App\Engine\Action;

use App\DTO\ActionResult;
use App\DTO\BallState;
use App\DTO\GameEvent;
use App\DTO\GameState;
use App\DTO\MatchPlayerDTO;
use App\Engine\BallResolver;
use App\Engine\DiceRollerInterface;
use App\Enum\SkillName;

final class HandOffHandler implements ActionHandlerInterface
{
    public function __construct(
        private readonly BallResolver $ballResolver,
        private readonly DiceRollerInterface $dice,
    ) {
    }

    /**
     * @param array<string, mixed> $params
     */
    public function resolve(GameState $state, array $params): ActionResult
    {
        $playerId = (int) $params['playerId'];
        $targetId = (int) $params['targetId'];

        $giver = $state->getPlayer($playerId);
        $receiver = $state->getPlayer($targetId);

        if ($giver === null || $receiver === null) {
            throw new \InvalidArgumentException('Player not found');
        }

        $giverPos = $giver->getPosition();
        $receiverPos = $receiver->getPosition();

        if ($giverPos === null) {
            throw new \InvalidArgumentException('Players must be on pitch');
        }

        // ⛔⛔ OPRAVA 11.09.2026 (PHP17): PRIJEMCE UZ NA HRISTI BYT NEMUSI.
        //   Nabidka ho vybirala pres `getHandOffTargets`, ktere vraci jen
        //   hrace NA HRISTI -- jenze mezi nabidkou a provedenim bezi
        //   KONTROLA PRED AKCI (`ActionResolver:130`). A **Bloodlust** v ni
        //   kousne Thralla: hod na zraneni muze dat KO nebo zraneni, a to
        //   `InjuryResolver:205,211,215` nastavi `position = null`.
        //   Kdyz je tim Thrallem prave prijemce hand-offu, handler ho uz
        //   nenajde a hazel `Players must be on pitch`.
        //   ZMERENO: 1 z 12 135 rozhodnuti -- vzacne, ale v zive hre to
        //   `AITurnService` nechyta (`try/catch` tam neni ani jednou).
        //
        // ⭐ TRETI VYSKYT TRIDY (C) „etapa veri, ze predchozi uspela"
        //   (vedle `BlitzHandler` 3x). Tady navic PRES DVE VRSTVY: nabidka
        //   -> kontrola pred akci -> handler.
        //
        // ⭐ PRAVIDLOVA KOTVA: `rules_bb2016.txt` r. 7935-7937 -- vampir se
        //   krmi „at the end of the declared Action, **but before actually
        //   passing, handing off, or scoring**". Kousnuti tedy PRECHAZI
        //   hand-off zamerne. Kdyz prijemce kousnuti neprezil na hristi,
        //   hand-off proste nema komu -- akce je vycerpana, mic zustava
        //   podavajicimu a turnover to NENI (katalog r. 368-384 tenhle
        //   pripad nezna).
        if ($receiverPos === null || !$receiver->getState()->canAct()) {
            $state = $state->withPlayer($giver->withHasActed(true)->withHasMoved(true));

            return ActionResult::success($state, []);
        }

        if ($giverPos->distanceTo($receiverPos) !== 1) {
            throw new \InvalidArgumentException('Players must be adjacent for hand-off');
        }

        $events = [GameEvent::handOff($playerId, $targetId)];

        // Animosity check: if giver has Animosity and receiver has different raceName
        if ($giver->hasSkill(SkillName::Animosity) && $this->isDifferentRace($giver, $receiver)) {
            $animRoll = $this->dice->rollD6();
            $animSuccess = $animRoll >= 2;
            $events[] = GameEvent::animosity($playerId, $targetId, $animRoll, $animSuccess);
            if (!$animSuccess) {
                // Ball stays with giver, mark acted, not a turnover
                $state = $state->withPlayer($giver->withHasActed(true)->withHasMoved(true));
                return ActionResult::success($state, $events);
            }
        }

        // Mark giver as acted
        $state = $state->withPlayer($giver->withHasActed(true)->withHasMoved(true));

        // ⛔ PHP21: odecist TYMOVY limit -- jedna Hand-off Action za kolo.
        $handOffSide = $giver->getTeamSide();
        $state = $state->withTeamState(
            $handOffSide,
            $state->getTeamState($handOffSide)->withHandOffUsed(),
        );

        // Move ball to receiver's position for the catch attempt
        $state = $state->withBall(BallState::onGround($receiverPos));

        // Receiver attempts catch with +1 modifier
        $activeSide = $state->getActiveTeam();
        $teamRerollAvailable = $state->getTeamState($activeSide)->canUseReroll();
        $catchResult = $this->ballResolver->resolveCatch($state, $receiver, modifier: 1, teamRerollAvailable: $teamRerollAvailable);
        $events = array_merge($events, $catchResult['events']);
        $state = $catchResult['state'];

        if ($catchResult['teamRerollUsed']) {
            $state = $state->withTeamState($activeSide, $state->getTeamState($activeSide)->withRerollUsed());
        }

        // ⛔⛔ OPRAVA 11.09.2026: TURNOVER SE VYHLASOVAL PRILIS BRZY.
        //   `rules_bb2016.txt` r. 371-373 (bod 2 uzavreneho katalogu):
        //   „A passed ball, or hand-off, is not caught by any member of the
        //   moving team **before the ball comes to rest**."
        //   A bod 3 k tomu vyslovne dodava: „failing a catch roll, as opposed
        //   to a pick up, **is by itself never a turnover**." (r. 376-378)
        //
        //   `resolveCatch` pri neuspechu mic ODRAZI (`BallResolver:177-180`)
        //   a ten odraz muze skoncit V RUKOU SPOLUHRACE -- `resolveBounce`
        //   na to ma vlastni vetev. Presto se tu vracel turnover, protoze
        //   `success => false` znamena jen "TENHLE hrac nechytil", ne "mic
        //   je pryc". Tym tak prisel o kolo i ve chvili, kdy mic udrzel.
        //
        // ⇒ Rozhoduje STAV MICE PO ODRAZU, ne vysledek jednoho hodu.
        if (!$catchResult['success']) {
            // ⭐ `isBallHeldBy` je TYZ predikat, jaky uz spravne pouzival
            //   `PassResolver` (`ballCaughtByTeam`). Pri oprave 11.09. jsem ho
            //   napsal podruhe inline -- slouceno na `GameState`, at nevzniknou
            //   dve kopie jednoho vzorce (tahle past uz projekt kousla 3x).
            if ($state->isBallHeldBy($activeSide)) {
                // Odraz chytil nekdo nas => mic je porad nas, kolo bezi dal.
                return ActionResult::success($state, $events);
            }

            $events[] = GameEvent::turnover('Failed hand-off');
            return ActionResult::turnover($state->withTurnoverPending(true), $events);
        }

        return ActionResult::success($state, $events);
    }

    private function isDifferentRace(MatchPlayerDTO $a, MatchPlayerDTO $b): bool
    {
        $raceA = $a->getRaceName();
        $raceB = $b->getRaceName();
        if ($raceA === null && $raceB === null) {
            return false;
        }
        return $raceA !== $raceB;
    }
}
