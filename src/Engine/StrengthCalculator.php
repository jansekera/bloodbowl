<?php
declare(strict_types=1);

namespace App\Engine;

use App\DTO\GameState;
use App\DTO\MatchPlayerDTO;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use App\ValueObject\Position;

final class StrengthCalculator
{
    /**
     * Calculate effective blocking strength (base ST + assists).
     */
    public function calculateEffectiveStrength(
        GameState $state,
        MatchPlayerDTO $player,
        Position $targetPosition,
    ): int {
        return $player->getStats()->getStrength()
            + $this->countAssists($state, $player, $targetPosition);
    }

    /**
     * Count friendly assists for a block.
     *
     * An assist is a friendly standing player adjacent to the target position,
     * who is not in any enemy tackle zone OTHER THAN the block target
     * (unless they have the Guard skill).
     */
    public function countAssists(
        GameState $state,
        MatchPlayerDTO $blocker,
        Position $targetPosition,
    ): int {
        $assists = 0;
        $friendlySide = $blocker->getTeamSide();
        $target = $state->getPlayerAtPosition($targetPosition);
        $targetId = $target?->getId();

        foreach ($state->getPlayersOnPitch($friendlySide) as $friend) {
            if ($friend->getId() === $blocker->getId()) {
                continue;
            }
            if (!$friend->getState()->canAct()) {
                continue;
            }

            $friendPos = $friend->getPosition();
            if ($friendPos === null || $friendPos->distanceTo($targetPosition) !== 1) {
                continue;
            }

            // Must not be in an enemy TZ (excluding the block target), unless has Guard
            if (!$friend->hasSkill(SkillName::Guard) && $this->isInTackleZoneExcluding($state, $friend, $targetId)) {
                continue;
            }

            $assists++;
        }

        return $assists;
    }

    /**
     * Cisty pocet asistenci u FAULU: utocne minus obranne.
     *
     * ⭐ 16.09.2026 podle `rules_bb2016.txt` r. 1843-1853:
     *   "Other players that are adjacent to the victim must assist the player making the
     *    foul, and each extra player adds 1 to the Armour roll. Defending players adjacent
     *    to the fouler must also give assists ... -1 per assist. No player from either side
     *    may assist a foul if they are in the tackle zone of an opposing player, do not have
     *    their tackle zones, or are not standing."
     * ⛔ Guard se u faulu pouzit NESMI (r. 8161) -- na rozdil od bloku.
     *   Vyjimka ze zon: u utocne asistence sama OBET, u obranne sam FAULUJICI --
     *   jinak by obranna asistence nemohla vzniknout nikdy.
     */
    public function countFoulAssists(GameState $state, MatchPlayerDTO $fouler, MatchPlayerDTO $victim): int
    {
        $foulerPos = $fouler->getPosition();
        $victimPos = $victim->getPosition();
        if ($foulerPos === null || $victimPos === null) {
            return 0;
        }

        $utocne = $this->spocitejPomocniky($state, $fouler->getTeamSide(), $victimPos, $fouler->getId(), $victim->getId());
        $obranne = $this->spocitejPomocniky($state, $victim->getTeamSide(), $foulerPos, $victim->getId(), $fouler->getId());

        return $utocne - $obranne;
    }

    /**
     * Stojici hraci dane strany sousedici s `$cil`, kteri maji zony a nestoji v zone
     * soupere (krome `$vyjimkaId`). `$kromeId` se preskoci uplne.
     */
    private function spocitejPomocniky(
        GameState $state,
        TeamSide $strana,
        Position $cil,
        int $kromeId,
        int $vyjimkaId,
    ): int {
        $pocet = 0;
        foreach ($state->getPlayersOnPitch($strana) as $hrac) {
            if ($hrac->getId() === $kromeId || !$hrac->getState()->canAct() || $hrac->hasLostTacklezones()) {
                continue;
            }
            $pos = $hrac->getPosition();
            if ($pos === null || $pos->distanceTo($cil) !== 1) {
                continue;
            }
            if ($this->isInTackleZoneExcluding($state, $hrac, $vyjimkaId)) {
                continue;
            }
            $pocet++;
        }

        return $pocet;
    }

    /**
     * Check if player is in any enemy TZ, excluding a specific player.
     */
    private function isInTackleZoneExcluding(GameState $state, MatchPlayerDTO $player, ?int $excludeId): bool
    {
        $pos = $player->getPosition();
        if ($pos === null) {
            return false;
        }

        $enemySide = $player->getTeamSide()->opponent();
        foreach ($state->getPlayersOnPitch($enemySide) as $enemy) {
            if ($enemy->getId() === $excludeId) {
                continue;
            }
            if (!$enemy->getState()->exertsTacklezone()) {
                continue;
            }
            $enemyPos = $enemy->getPosition();
            if ($enemyPos !== null && $pos->distanceTo($enemyPos) === 1) {
                return true;
            }
        }

        return false;
    }

    /**
     * Determine block dice count and who chooses.
     *
     * @return array{count: int, attackerChooses: bool}
     */
    public function getBlockDiceInfo(int $attackerStrength, int $defenderStrength): array
    {
        // ⛔⛔ OPRAVENO 15.09.2026 -- tady bylo `>=`. `rules_bb2016.txt` r. 567-568
        //   a tabulka r. 1731: "MORE THAN TWICE AS STRONG, three dice".
        //   Presne dvojnasobek (casto s asistencemi: 3+3 proti 3) = 2 kostky.
        if ($attackerStrength > 2 * $defenderStrength) {
            return ['count' => 3, 'attackerChooses' => true];
        }
        if ($attackerStrength > $defenderStrength) {
            return ['count' => 2, 'attackerChooses' => true];
        }
        if ($attackerStrength === $defenderStrength) {
            return ['count' => 1, 'attackerChooses' => true];
        }
        if ($defenderStrength > 2 * $attackerStrength) {
            return ['count' => 3, 'attackerChooses' => false];
        }

        return ['count' => 2, 'attackerChooses' => false];
    }
}
