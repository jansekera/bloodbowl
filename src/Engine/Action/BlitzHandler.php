<?php

declare(strict_types=1);

namespace App\Engine\Action;

use App\DTO\ActionResult;
use App\DTO\GameState;
use App\DTO\MatchPlayerDTO;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\ValueObject\Position;
use App\Engine\Pathfinder;

final class BlitzHandler implements ActionHandlerInterface
{
    public function __construct(
        private readonly MoveHandler $moveHandler,
        private readonly BlockHandler $blockHandler,
        private readonly Pathfinder $pathfinder,
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

        // Mark blitz used for this turn
        $teamState = $state->getTeamState($attacker->getTeamSide());
        $state = $state->withTeamState($attacker->getTeamSide(), $teamState->withBlitzUsed());

        $events = [];

        // If not adjacent, move to adjacent square first
        if ($attackerPos->distanceTo($defenderPos) > 1) {
            // Find best adjacent square to move to
            // ⭐ Pathfinder se pousti JEDNOU. Drive `findClosestApproach`
            //   volala `findValidMoves` znovu nad tymz stavem a hracem --
            //   podle mereni v tomhle souboru slo o 3,58 % rozhodnuti, takze
            //   to nebylo teoreticke.
            $validMoves = $this->pathfinder->findValidMoves($state, $attacker);
            $moveTarget = $this->findBlitzMoveTarget($validMoves, $defenderPos);

            // ⛔⛔⛔ OPRAVA 11.09.2026: TADY SE HÁZELA VÝJIMKA
            //   `throw new \InvalidArgumentException('Cannot reach target for blitz')`
            //   -- a byla to VADA, ne obrana. Naměřeno 19 z 531 rozhodnutí
            //   (3,58 %) u kouče, proti kterému hraje člověk.
            //
            // ⭐ PROČ JE DEKLARACE BLITZU MIMO DOSAH LEGITIMNÍ (uživatel 11.09.):
            //   Wild Animal (`rules_bb2016.txt` r. 8666-8669) hází D6 **+2 za
            //   Block nebo Blitz**, padá na 1-3. Vyhlásit blitz je tedy JEDINÝ
            //   způsob, jak Rat Ogra / Minotaura rozhýbat na přirozenou **2+**
            //   místo 4+. Hráč přitom vůbec nemusí na cíl dosáhnout -- smysl
            //   má sama DEKLARACE. Kontrola běží PŘED tímhle handlerem
            //   (`ActionResolver.php:130`), takže výjimka tu deklaraci zabila
            //   až POTOM, co svou práci odvedla.
            //
            // ⛔ A živá hra ji NECHYTALA: `AITurnService::playTurn` nemá
            //   `try/catch` ani jednou (`GameOrchestrator.php:122`). Spolkl ji
            //   jen simulátor (`cli/simulate.php:157`) -- proto v korpusu
            //   nebyla vidět. Uživatel 11.09.: „vadu opravit, ne schovat."
            //   ⇒ Záplata do `AITurnService` se schválně NEDĚLÁ.
            //
            // ⇒ Když na sousední pole cíle nedosáhneme, blitz se NERUŠÍ:
            //   hráč se posune, jak nejblíž k cíli umí, a blok se prostě
            //   nekoná (BB2016: Blitz = pohyb + NEJVÝŠ jeden blok během něj).
            $blockPossible = true;
            if ($moveTarget === null) {
                $moveTarget = $this->findClosestApproach($validMoves, $attackerPos, $defenderPos);
                $blockPossible = false;
                if ($moveTarget === null) {
                    // Nemá kam šlápnout vůbec. Deklarace platí (blitz je
                    // odečtený výš), akce se vyčerpala, blok se nekoná.
                    return ActionResult::success($state, $events);
                }
            }

            // Resolve movement to adjacent square
            $moveResult = $this->moveHandler->resolve($state, [
                'playerId' => $attackerId,
                'x' => $moveTarget->getX(),
                'y' => $moveTarget->getY(),
            ]);

            if ($moveResult->isTurnover()) {
                return $moveResult;
            }

            // Pending reroll from dodge/GFI during blitz movement
            if ($moveResult->getNewState()->getPendingReroll() !== null) {
                $events = array_merge($events, $moveResult->getEvents());
                return ActionResult::success($moveResult->getNewState(), $events);
            }

            $state = $moveResult->getNewState();
            $events = array_merge($events, $moveResult->getEvents());

            // Na cíl se nedosáhlo -- přiblížili jsme se a končíme. Hráč si
            // NEČISTÍ `hasMoved`/`hasActed`: žádný blok už nepřijde.
            if (!$blockPossible) {
                return ActionResult::success($state, $events);
            }

            // ⛔⛔ OPRAVA 11.09.2026, TŘETÍ VÝSKYT TÉHOŽ PRINCIPU V TOMHLE
            //   SOUBORU: tady se věřilo, že přesun DOŠEL tam, kam měl.
            //   `MoveHandler` ale umí vrátit `success` s hráčem NA PŮVODNÍM
            //   POLI -- `MoveHandler:185-190`, **Tentacles**: „Movement ends,
            //   NOT a turnover". Blok se pak počítal ze staré pozice a
            //   `BlockHandler:73` hodil `Players must be adjacent to block`.
            //   ZMĚŘENO: 2 z 12 135 rozhodnutí (0,02 %) -- vzácné, ale
            //   v živé hře to nikdo nechytá (`AITurnService` `try/catch` nemá).
            //
            // ⭐ NENÍ TO ZÁPLATA, JE TO PRAVIDLO: Blitz je pohyb + NEJVÝŠ
            //   jeden blok BĚHEM NĚJ. Když nás chapadla zastavila dřív, než
            //   jsme k cíli došli, blok se prostě nekoná -- stejně jako když
            //   se na cíl nedosáhne (větev výš).
            $afterMove = $state->getPlayer($attackerId);
            $afterPos = $afterMove?->getPosition();
            $freshDefenderPos = $state->getPlayer($targetId)?->getPosition();
            if ($afterPos === null || $freshDefenderPos === null
                || $afterPos->distanceTo($freshDefenderPos) !== 1) {
                return ActionResult::success($state, $events);
            }

            // Reset hasMoved so block can still mark it
            $movedAttacker = $state->getPlayer($attackerId);
            if ($movedAttacker !== null) {
                $state = $state->withPlayer($movedAttacker->withHasMoved(false)->withHasActed(false));
            }
        }

        // Now resolve the block (with Horns bonus if applicable)
        $blockParams = [
            'playerId' => $attackerId,
            'targetId' => $targetId,
            'isBlitz' => true,
        ];
        $freshAttacker = $state->getPlayer($attackerId);
        if ($freshAttacker !== null && $freshAttacker->hasSkill(SkillName::Horns)) {
            $blockParams['hornsBonus'] = true;
        }
        $blockResult = $this->blockHandler->resolve($state, $blockParams);

        $events = array_merge($events, $blockResult->getEvents());

        if ($blockResult->isTurnover()) {
            return ActionResult::turnover($blockResult->getNewState(), $events);
        }

        return ActionResult::success($blockResult->getNewState(), $events);
    }

    /**
     * Nejbližší DOSAŽITELNÉ pole k cíli, když na sousední pole nedosáhneme.
     *
     * ⭐ Používá se jen pro blitz, který se nedá dokončit (viz `resolve`).
     *   Kritérium je totéž jako u `findBlitzMoveTarget` -- nejdřív vzdálenost
     *   k cíli, pak nejméně dodgů a GFI -- aby se „přiblížení" nechovalo jinak
     *   než „doběhnutí" a nevznikly dvě neslučitelné definice téhož.
     */
    private function findClosestApproach(array $validMoves, Position $from, Position $defenderPos): ?Position
    {
        $bestTarget = null;
        $bestKey = null;
        $startDist = $from->distanceTo($defenderPos);

        foreach ($validMoves as $path) {
            $dest = $path->getDestination();
            $dist = $dest->distanceTo($defenderPos);
            // Přiblížení musí být PŘIBLÍŽENÍ -- couvnout od cíle není blitz.
            if ($dist >= $startDist) {
                continue;
            }
            $key = [$dist, $path->getDodgeCount(), $path->getGfiCount(), $path->getTotalCost()];
            if ($bestKey === null || $key < $bestKey) {
                $bestKey = $key;
                $bestTarget = $dest;
            }
        }

        return $bestTarget;
    }

    /**
     * Find best adjacent square to defender for a blitz move.
     */
    private function findBlitzMoveTarget(array $validMoves, Position $defenderPos): ?Position
    {
        $bestTarget = null;
        $bestScore = PHP_INT_MAX;

        foreach ($validMoves as $path) {
            $dest = $path->getDestination();
            if ($dest->distanceTo($defenderPos) !== 1) {
                continue;
            }

            // Prefer fewest dodges, then fewest GFIs
            $score = $path->getDodgeCount() * 100 + $path->getGfiCount() * 10 + $path->getTotalCost();

            // Surfing tiebreaker: prefer approach angle that pushes defender toward sideline
            $dy = $defenderPos->getY() - $dest->getY();
            $ndy = $dy === 0 ? 0 : ($dy > 0 ? 1 : -1);
            $pushY = $defenderPos->getY() + $ndy;

            $surfBonus = 0;
            if ($pushY < 0 || $pushY > 14) {
                $surfBonus = 5; // crowd surf!
            } elseif ($pushY === 0 || $pushY === 14) {
                $surfBonus = 3; // pushed to edge
            } elseif ($pushY === 1 || $pushY === 13) {
                $surfBonus = 1; // pushed toward edge
            }

            // Same for X edges
            $dx = $defenderPos->getX() - $dest->getX();
            $ndx = $dx === 0 ? 0 : ($dx > 0 ? 1 : -1);
            $pushX = $defenderPos->getX() + $ndx;
            if ($pushX < 0 || $pushX > 25) {
                $surfBonus = max($surfBonus, 5);
            }

            $score -= $surfBonus;

            if ($score < $bestScore) {
                $bestScore = $score;
                $bestTarget = $dest;
            }
        }

        return $bestTarget;
    }
}
