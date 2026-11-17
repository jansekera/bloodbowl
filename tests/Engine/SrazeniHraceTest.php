<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Enum\ActionType;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * P294 krok 7: hráč sražený výbuchem bomby nebo hozeným hráčem (`BallResolver::srazit`, dřív tři kopie).
 * - „treated as Knocked Down even if already Prone or Stunned“ (bomba ř. 7957-7958, dopad ř. 8617-8619) ⇒ omráčený
 *   s drženým brněním zůstává omráčený, nevstává do „leží“;
 * - sražený nosič pustí míč a ten odskočí (ř. 678-679).
 * Testy přes akce (požadavek), ne přes pomocníka. Sondy v pomocníkovi 17.11. tyhle dvě vlastnosti testy nechytily.
 */
final class SrazeniHraceTest extends TestCase
{
    /** @param list<int> $kostky */
    private function bomba(\App\DTO\GameState $s, array $kostky): \App\DTO\ActionResult
    {
        return (new ActionResolver(new FixedDiceRoller($kostky)))->resolve($s, ActionType::BOMB_THROW, ['playerId' => 1, 'targetX' => 8, 'targetY' => 7]);
    }

    public function testOmracenyVeVybuchuSDrzenymBrnenimZustavaOmraceny(): void
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, skills: [SkillName::Bombardier])
            ->addPronePlayer(TeamSide::AWAY, 8, 7, id: 2)
            ->withBallOffPitch()
            ->build();
        $s = $s->withPlayer($s->requirePlayer(2)->withState(PlayerState::STUNNED));
        // přesnost 5; střed (8,7): brnění 2+2 drží
        $r = $this->bomba($s, [5, 2, 2, 1, 1, 1]);

        $this->assertSame(PlayerState::STUNNED, $r->getNewState()->requirePlayer(2)->getState());
    }

    public function testNosicSrazenyVybuchemPustiMic(): void
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, skills: [SkillName::Bombardier])
            ->addPlayer(TeamSide::AWAY, 9, 7, id: 2)
            ->withBallCarried(2)
            ->build();
        // přesnost 5; střed (8,7) prázdný; (9,7): D6 4 ⇒ zasažen, brnění 2+2 drží; míč odskočí D8 3
        $r = $this->bomba($s, [5, 4, 2, 2, 3, 1, 1, 1, 1]);

        $this->assertSame(PlayerState::PRONE, $r->getNewState()->requirePlayer(2)->getState());
        $this->assertNotSame(2, $r->getNewState()->getBall()->getCarrierId(), 'sražený nosič míč nedrží');
    }

    public function testNosicNaKterehoDopadneHozenyHracPustiMic(): void
    {
        // jako TtmLandingRulesTest::testP278…: dopad na (7,5), kde stojí soupeř 3 — teď s míčem
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 5, skills: [SkillName::ThrowTeamMate], id: 1)
            ->addPlayer(TeamSide::HOME, 6, 5, skills: [SkillName::RightStuff], id: 2)
            ->addPlayer(TeamSide::AWAY, 7, 5, id: 3)
            ->withBallCarried(3)
            ->build();
        // přesnost 4, rozptyly 3/7/7 ⇒ (7,5); brnění soupeře 2+2; odraz míče D8 1; rozptyl hozeného D8 3; brnění 2+2
        $r = (new ActionResolver(new FixedDiceRoller([4, 3, 7, 7, 2, 2, 1, 3, 2, 2, 1, 1, 1, 1])))
            ->resolve($s, ActionType::THROW_TEAM_MATE, ['playerId' => 1, 'targetId' => 2, 'targetX' => 8, 'targetY' => 5]);

        $this->assertSame(PlayerState::PRONE, $r->getNewState()->requirePlayer(3)->getState());
        $this->assertNotSame(3, $r->getNewState()->getBall()->getCarrierId(), 'sražený nosič míč nedrží');
    }
}
