<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\MatchPlayerDTO;
use App\Engine\FixedDiceRoller;
use App\Engine\InjuryResolver;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use App\ValueObject\PlayerStats;
use App\ValueObject\Position;
use PHPUnit\Framework\TestCase;

/**
 * P192 PARITA — REGENERACE PO SMRTI A DECAY.
 *
 * `rules_bb2016.txt` ř. 8406-8411 (Regeneration): "If the player suffers a Casualty result on the
 * Injury table, then roll a D6 for Regeneration after the roll on the Casualty table ... On a 4-6,
 * the player will heal the injury" -- Dead je výsledek tabulky Casualty, regeneruje se i on.
 * ř. 8036-8042 (Decay): "When this player suffers a Casualty result on the Injury table, roll twice
 * on the Casualty table and apply both results. ... A successful Regeneration roll will heal both
 * results." Decay hod na zranění NEMĚNÍ (hází se dvakrát tabulka Casualty, ne zranění).
 *
 * Stará mechanika PHP: Regenerace jen u stavu INJURED (mrtvý ne); Decay hodil zranění dvakrát
 * a vzal horší, tabulku Casualty jednou.
 */
final class DecayARegeneraceTest extends TestCase
{
    /** @param list<SkillName> $skills */
    private function hrac(array $skills): MatchPlayerDTO
    {
        return MatchPlayerDTO::create(
            id: 1,
            playerId: 1,
            name: 'Hráč',
            number: 1,
            positionalName: 'Lineman',
            stats: new PlayerStats(6, 3, 3, 8),
            skills: $skills,
            teamSide: TeamSide::HOME,
            position: new Position(5, 5),
        );
    }

    /**
     * @param list<SkillName> $skills
     * @param list<int> $kostky
     */
    private function stav(array $skills, array $kostky): PlayerState
    {
        return (new InjuryResolver())->resolve($this->hrac($skills), new FixedDiceRoller($kostky))['player']->getState();
    }

    public function testRegeneraceSeHaziIPoSmrti(): void
    {
        // brnění 6+6 · zranění 6+6 ⇒ Casualty · D68 6,1 = Dead · Regenerace 4 ⇒ rezervy
        $this->assertSame(PlayerState::OFF_PITCH, $this->stav([SkillName::Regeneration], [6, 6, 6, 6, 6, 1, 4]));
    }

    public function testNeuspesnaRegeneracePoSmrtiNechaMrtveho(): void
    {
        // pozitivní kontrola: Regenerace 3 ⇒ mrtvý zůstává mrtvý
        $this->assertSame(PlayerState::DEAD, $this->stav([SkillName::Regeneration], [6, 6, 6, 6, 6, 1, 3]));
    }
}
