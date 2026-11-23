<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\TeamStateDTO;
use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Engine\TacklezoneCalculator;
use App\Enum\ActionType;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use App\ValueObject\Position;
use PHPUnit\Framework\TestCase;

/**
 * P321 — Swoop (rules_bb2016.txt ř. 8549-8564): hozený hráč se rozptyluje šablonou pro vhazování natočenou koučem
 * (D6, o 1 pole), k hodu na přistání +1; se Stunty při úhybu neignoruje zóny na cílovém poli.
 */
final class SwoopTest extends TestCase
{
    /**
     * @param list<SkillName> $skillyHozeneho
     * @param list<int> $kostky
     */
    private function hod(array $skillyHozeneho, array $kostky): \App\DTO\ActionResult
    {
        $state = (new GameStateBuilder())
            ->withHomeTeam(TeamStateDTO::create(1, 'Home', 'Goblin', TeamSide::HOME, 0))   // bez přehozů
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 5, skills: [SkillName::ThrowTeamMate], id: 1)
            ->addPlayer(TeamSide::HOME, 6, 5, skills: $skillyHozeneho, id: 2)
            ->withBallOffPitch()
            ->build();

        return (new ActionResolver(new FixedDiceRoller($kostky)))->resolve($state, ActionType::THROW_TEAM_MATE, [
            'playerId' => 1, 'targetId' => 2, 'targetX' => 8, 'targetY' => 5,
        ]);
    }

    public function testRozptylSablonouAPristaniSPlusJednou(): void
    {
        // přesnost 4; šablona: z cíle (8,5) shoda ⇒ dopředu, D6 3 ⇒ (9,5); z (9,5) nejblíž cíli natočení dozadu, 3 ⇒ (8,5);
        // znovu dopředu, 3 ⇒ (9,5). Přistání AG3: 4+ − 1 (Swoop) = 3+, hod 3 ⇒ stojí.
        $r = $this->hod([SkillName::RightStuff, SkillName::Stunty, SkillName::Swoop], [4, 3, 3, 3, 3]);
        $p = $r->getNewState()->requirePlayer(2);
        $this->assertSame(PlayerState::STANDING, $p->getState());
        $this->assertTrue($p->requirePosition()->equals(new Position(9, 5)));
    }

    public function testBezSwoopPristaniNaTrojkuNestaci(): void
    {
        // pozitivní kontrola (rozptyly D8 jako TtmLandingRulesTest P283: 3/3/7 ⇒ (9,5)); přistání 3 < 4 ⇒ leží, brnění 2+2
        $r = $this->hod([SkillName::RightStuff, SkillName::Stunty], [4, 3, 3, 7, 3, 2, 2]);
        $this->assertSame(PlayerState::PRONE, $r->getNewState()->requirePlayer(2)->getState());
    }

    public function testSwoopSeStuntyNeignorujeZonyPriUhybu(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 10, 7, skills: [SkillName::Stunty], id: 1)
            ->addPlayer(TeamSide::HOME, 10, 3, skills: [SkillName::Stunty, SkillName::Swoop], id: 2)
            ->addPlayer(TeamSide::AWAY, 12, 7, id: 3)
            ->addPlayer(TeamSide::AWAY, 12, 3, id: 4)
            ->build();
        $tz = new TacklezoneCalculator();
        $stunty = $tz->calculateDodgeTarget($state, $state->requirePlayer(1), new Position(11, 7));
        $swoop = $tz->calculateDodgeTarget($state, $state->requirePlayer(2), new Position(11, 3));
        $this->assertSame($stunty + 1, $swoop, 'jedna zóna na cílovém poli se se Swoop počítá');
    }
}
