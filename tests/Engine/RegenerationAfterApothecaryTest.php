<?php
declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\TeamStateDTO;
use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Enum\ActionType;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * `rules_bb2016.txt` r. 8433-8440 (Regeneration): "roll a D6 for Regeneration after the
 * roll on the Casualty table and after any Apothecary roll if allowed ... Regeneration
 * rolls may not be re-rolled." Engine hazel Regeneration UVNITR hodu na zraneni -- tedy
 * PRED lekarnikem -- a lekarnikuv opakovany hod ji hodil PODRUHE.
 */
final class RegenerationAfterApothecaryTest extends TestCase
{
    public function testApothecaryFirstThenSingleRegenerationRoll(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, armour: 8, skills: [SkillName::Regeneration], id: 2)
            ->withAwayTeam(TeamStateDTO::create(2, 'Away', 'Vampire', TeamSide::AWAY, 2))
            ->withBallOffPitch()
            ->build();

        // blok 6 = Defender Down; brneni 6+6; zraneni 5+5 = CAS + tabulka D68 (1,1 = Badly Hurt);
        // lekarnik: zraneni 5+6 = CAS + D68 (1,1); Regeneration JEDNOU: 4 => rezervy
        $r = (new ActionResolver(new FixedDiceRoller([6, 6, 6, 5, 5, 1, 1, 5, 6, 1, 1, 4])))
            ->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $typy = array_map(static fn($e) => $e->getType(), $r->getEvents());
        $this->assertContains('apothecary', $typy, 'lekarnik se rozhoduje PRVNI');
        $this->assertSame(1, count(array_keys($typy, 'regeneration', true)), 'Regeneration se hazi jednou');
        $this->assertLessThan(array_search('regeneration', $typy, true), array_search('apothecary', $typy, true), 'lekarnik PRED Regeneration');
        $this->assertSame(PlayerState::OFF_PITCH, $r->getNewState()->requirePlayer(2)->getState());
    }

    /** Lekarnik zlepsi CAS na KO => Regeneration se vubec nehazi (neni co regenerovat). */
    public function testApothecaryImprovesToKoNoRegenerationRoll(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, armour: 8, skills: [SkillName::Regeneration], id: 2)
            ->withAwayTeam(TeamStateDTO::create(2, 'Away', 'Vampire', TeamSide::AWAY, 2))
            ->withBallOffPitch()
            ->build();

        // blok 6; brneni 6+6; zraneni 5+5 = CAS + D68 (1,1); lekarnik: zraneni 3+5 = 8 => KO
        $r = (new ActionResolver(new FixedDiceRoller([6, 6, 6, 5, 5, 1, 1, 3, 5])))
            ->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $typy = array_map(static fn($e) => $e->getType(), $r->getEvents());
        $this->assertNotContains('regeneration', $typy);
        $this->assertSame(PlayerState::KO, $r->getNewState()->requirePlayer(2)->getState());
    }
}
