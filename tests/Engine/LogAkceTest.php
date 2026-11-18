<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\GameEvent;
use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Enum\ActionType;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * P306 A1 (18.11.2026): otázka kouči uprostřed akce ukáže průběh celé akce (log podle vzniku událostí), i když ji
 * položí hod hluboko v `BallResolver`. OPRAVENO — dřív otázka nesla jen události, které znal kód, jenž ji položil
 * (a ručně přidané obalem `sPredchozimi` na 8 místech); u chytání odrazu po bloku kouč viděl jen „Catch failed“
 * bez bloku, pádu a odrazu.
 */
final class LogAkceTest extends TestCase
{
    /** @param list<int> $kostky */
    private function kouc(array $kostky): ActionResolver
    {
        $r = new ActionResolver(new FixedDiceRoller($kostky));
        $r->setHumanSides([TeamSide::HOME]);

        return $r;
    }

    /** Blok srazí nosiče, míč odskočí ke spoluhráči útočníka a ten ho nechytí (ř. 929-931: přehoz smí na každý hod). */
    private function blokNaNosice(): \App\DTO\GameState
    {
        return (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, id: 2)
            ->addPlayer(TeamSide::HOME, 8, 4, agility: 3, id: 3)
            ->withBallCarried(2)
            ->build();
    }

    /**
     * @param list<GameEvent> $e
     * @return list<string>
     */
    private static function typy(array $e): array
    {
        return array_map(static fn(GameEvent $x): string => $x->getType(), $e);
    }

    public function testOtazkaNaChytaniOdrazuPoBlokuUkazeCelyPrubeh(): void
    {
        // POW, brnění 1+1 drží, odraz na východ (8,4), chytání 1
        $q = $this->kouc([6, 1, 1, 3, 1])->resolve($this->blokNaNosice(), ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $this->assertSame('catch', $q->getNewState()->getPendingReroll()?->getRollType()?->value, 'fixtura: otázka na chytání');
        $this->assertSame(
            ['block', 'push', 'follow_up', 'player_fell', 'armour_roll', 'ball_bounce', 'catch'],
            self::typy($q->getEvents()),
            'kouč u otázky vidí, proč míč letí k jeho hráči',
        );
    }

    public function testPoOdpovediSeNicNeopakujeANicNechybi(): void
    {
        $q = $this->kouc([6, 1, 1, 3, 1])->resolve($this->blokNaNosice(), ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);
        $r = $this->kouc([6])->resolve($q->getNewState(), ActionType::RESOLVE_REROLL, ['choice' => 'team_reroll']);

        $this->assertSame(['reroll', 'catch'], self::typy($r->getEvents()));
        $this->assertSame(3, $r->getNewState()->getBall()->getCarrierId(), 'přehoz chytil');
    }

    public function testLogBereUdalostiVPoradiVznikuABezVypoctuNanecisto(): void
    {
        GameEvent::zacitLog();
        try {
            $a = GameEvent::turnover('a');
            GameEvent::bezLogu(static fn(): GameEvent => GameEvent::turnover('nanečisto'));
            $b = GameEvent::turnover('b');
            $this->assertSame([$a, $b], GameEvent::log());
        } finally {
            GameEvent::ukoncitLog();
        }
    }

    public function testAkceSeDoAkceNevnoruje(): void
    {
        GameEvent::zacitLog();
        try {
            $this->expectException(\LogicException::class);
            GameEvent::zacitLog();
        } finally {
            GameEvent::ukoncitLog();
        }
    }

    /**
     * Review P306 — pojistka: nic, co vzniklo nanečisto nebo v rozpracovaném výpočtu přerušeném vlastní otázkou,
     * nezůstane v logu akce (kouč by to u další otázky viděl). Hlídá `ActionResolver::hlidatLog` (zapnuto pro celou
     * sadu v `tests/bootstrap.php`); tyto testy jen sestaví situace, kde se dnes počítá nanečisto. Místa, kde se dnes počítá nanečisto: otázka na
     * Stand Firm (zahoditPri), past na Frenzy (odtlačení nanečisto, bezLogu), Bone Head po odmítnutém přehozu (bezLogu).
     */
    public function testOtazkaNaStandFirmNenechaVLoguRozpracovanyBlok(): void
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 7, skills: [SkillName::StandFirm], id: 2)
            ->withBallOffPitch()
            ->build();
        $r = new ActionResolver(new FixedDiceRoller([3, 3, 3]));
        $r->setInteractiveBlocks(true);
        $r->setHumanSides([TeamSide::HOME, TeamSide::AWAY]);
        $hod = $r->resolve($s, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2])->getNewState();
        $v = $r->resolve($hod, ActionType::CHOOSE_BLOCK_DIE, ['faceIndex' => 0]);   // zahozená událost ⇒ LogicException

        $this->assertNotNull($v->getNewState()->getPendingStandFirm(), 'fixtura: otázka na Stand Firm');
    }

    public function testPastNaFrenzyNanecistoNeniVLogu(): void
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, skills: [SkillName::Frenzy], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 7, skills: [SkillName::StandFirm, SkillName::Fend], id: 2)
            ->withBallOffPitch()
            ->build();
        $r = new ActionResolver(new FixedDiceRoller([3, 3, 3, 3, 3, 3]));
        $r->setInteractiveBlocks(true);
        $hod = $r->resolve($s, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2])->getNewState();
        $v = $r->resolve($hod, ActionType::CHOOSE_BLOCK_DIE, ['faceIndex' => 0]);   // zahozená událost ⇒ LogicException

        $this->assertContains('push', self::typy($v->getEvents()), 'fixtura: blok doběhl s odtlačením');
    }

    public function testBoneHeadPoOdmitnutemPrehozuNeniVLoguDvakrat(): void
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, skills: [SkillName::BoneHead], id: 1)
            ->withBallOffPitch()
            ->build();
        $q = $this->kouc([1])->resolve($s, ActionType::MOVE, ['playerId' => 1, 'x' => 5, 'y' => 6]);
        $r = $this->kouc([1]);
        $v = $r->resolve($q->getNewState(), ActionType::RESOLVE_REROLL, ['choice' => 'decline']);   // zahozená událost ⇒ LogicException

        $this->assertSame([], self::typy($v->getEvents()), 'neúspěch kouč viděl u otázky; po odmítnutí se nic neopakuje');
    }
}
