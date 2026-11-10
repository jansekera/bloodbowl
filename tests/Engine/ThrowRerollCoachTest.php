<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\GameState;
use App\DTO\TeamStateDTO;
use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Enum\ActionType;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * P292 (uživatel 10.11.2026: „oprav ať se ptá“). Týmový přehoz smí přehodit kterýkoli hod (`rules_bb2016.txt`
 * ř. 929-931) a je to volba kouče; Pro taky (ř. 8381-8387, po neúspěšném hodu Pro smí týmový přehoz jen hod Pro).
 * Kouče-člověka se hra zeptá i u přihrávky, bomby a hodu spoluhráčem; akce se přeruší a po odpovědi přehraje se
 * stejnými kostkami. AI přehazuje sama jako dosud. Parita s C++ coachAskReroll.
 *
 * Přihrávka z (5,5) na (10,5): přesnost 2 = nepřesná (jako DivingCatchKoucTest).
 */
final class ThrowRerollCoachTest extends TestCase
{
    /** @param list<int> $kostky */
    private function resolver(array $kostky): ActionResolver
    {
        $r = new ActionResolver(new FixedDiceRoller($kostky));
        $r->setInteractiveThrowRerolls([TeamSide::HOME]);
        $r->setInteractiveDivingCatch([TeamSide::HOME]);

        return $r;
    }

    /** @param list<SkillName> $skills */
    private function prihravka(array $skills = [], bool $divingCatch = false): GameState
    {
        $b = (new GameStateBuilder())->addPlayer(TeamSide::HOME, 5, 5, agility: 3, skills: $skills, id: 1);
        if ($divingCatch) {
            $b->addPlayer(TeamSide::HOME, 10, 7, agility: 3, skills: [SkillName::DivingCatch], id: 2);
        }

        return $b->withBallCarried(1)->build();
    }

    public function testNepovedenaPrihravkaSeZeptaKouce(): void
    {
        $r = $this->resolver([2])->resolve($this->prihravka(), ActionType::PASS, ['playerId' => 1, 'targetX' => 10, 'targetY' => 5]);

        $q = $r->getNewState()->getPendingReroll();
        $this->assertNotNull($q);
        $this->assertSame('pass', $q->getRollType());
        $this->assertSame(2, $q->getRoll());
        $this->assertTrue($q->isTeamRerollAvailable());
        $this->assertFalse($q->isProAvailable());
        $this->assertSame(1, $r->getNewState()->getBall()->getCarrierId(), 'stav před přihrávkou');
    }

    public function testOdmitnutiPrehozPrihravkaZustaneNepresnaAPrehozZustane(): void
    {
        $s = $this->resolver([2])->resolve($this->prihravka(), ActionType::PASS, ['playerId' => 1, 'targetX' => 10, 'targetY' => 5])->getNewState();
        // rozptyl 3× D8, pak odraz
        $r = $this->resolver(array_fill(0, 8, 2))->resolve($s, ActionType::RESOLVE_REROLL, ['choice' => 'decline']);

        $this->assertNull($r->getNewState()->getPendingReroll());
        $this->assertTrue($r->getNewState()->getHomeTeam()->canUseReroll(), 'přehoz nespotřebován');
        $hody = array_values(array_filter($r->getEvents(), static fn($e) => $e->getType() === 'pass'));
        $this->assertCount(1, $hody, 'hází se jen jednou');
    }

    public function testTymovyPrehozPrihravkuPrehodi(): void
    {
        $s = $this->resolver([2])->resolve($this->prihravka(), ActionType::PASS, ['playerId' => 1, 'targetX' => 10, 'targetY' => 5])->getNewState();
        // přehoz 6 = přesná; chytat nikdo nechytá (prázdné pole) ⇒ odraz D8
        $r = $this->resolver([6, 2, 2, 2])->resolve($s, ActionType::RESOLVE_REROLL, ['choice' => 'team_reroll']);

        $this->assertFalse($r->getNewState()->getHomeTeam()->canUseReroll());
        $hody = array_values(array_filter($r->getEvents(), static fn($e) => $e->getType() === 'pass'));
        $this->assertCount(2, $hody);
    }

    public function testProNeuspechDruhaOtazkaNaTymovyPrehozHoduPro(): void
    {
        $s = $this->resolver([2])->resolve($this->prihravka([SkillName::Pro]), ActionType::PASS, ['playerId' => 1, 'targetX' => 10, 'targetY' => 5])->getNewState();
        $this->assertTrue($s->getPendingReroll()?->isProAvailable());

        // Pro 2 = neprošel ⇒ druhá otázka (ř. 8385-8387)
        $s2 = $this->resolver([2])->resolve($s, ActionType::RESOLVE_REROLL, ['choice' => 'pro'])->getNewState();
        $q = $s2->getPendingReroll();
        $this->assertNotNull($q);
        $this->assertTrue($q->isProFailed());
        $this->assertFalse($q->isProAvailable());

        // týmový přehoz hodu Pro: 5 = prošel, přihrávka 6 = přesná, odraz
        $r = $this->resolver([5, 6, 2, 2, 2])->resolve($s2, ActionType::RESOLVE_REROLL, ['choice' => 'team_reroll']);
        $this->assertNull($r->getNewState()->getPendingReroll());
        $this->assertFalse($r->getNewState()->getHomeTeam()->canUseReroll());
    }

    public function testPrehozAPakDivingCatchVJedneAkciDobehne(): void
    {
        // review 10.11. bod 4: dvě různé otázky v jedné akci — odpovědi obou druhů se přehrají spolu
        $s = $this->resolver([2])->resolve($this->prihravka([], true), ActionType::PASS, ['playerId' => 1, 'targetX' => 10, 'targetY' => 5])->getNewState();
        // přehoz 2 = zase nepřesná; rozptyl J, V, Z ⇒ (10,6) vedle hráče 2 s Diving Catch
        $s2 = $this->resolver([2, 5, 3, 7])->resolve($s, ActionType::RESOLVE_REROLL, ['choice' => 'team_reroll'])->getNewState();
        $this->assertNotNull($s2->getPendingDivingCatch());
        $this->assertNull($s2->getPendingReroll());

        $r = $this->resolver([6])->resolve($s2, ActionType::RESOLVE_DIVING_CATCH, ['use' => true]);
        $this->assertNull($r->getNewState()->getPendingReroll(), 'přehoz se znovu neptá');
        $this->assertNull($r->getNewState()->getPendingDivingCatch());
        $this->assertSame(2, $r->getNewState()->getBall()->getCarrierId());
    }

    public function testFumbleHoduSpoluhracemSeZeptaKouce(): void
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 5, skills: [SkillName::ThrowTeamMate], id: 1)
            ->addPlayer(TeamSide::HOME, 6, 5, skills: [SkillName::RightStuff], id: 2)
            ->withBallOffPitch()
            ->build();
        $r = $this->resolver([1])->resolve($s, ActionType::THROW_TEAM_MATE, ['playerId' => 1, 'targetId' => 2, 'targetX' => 8, 'targetY' => 5]);

        $this->assertSame('ttm', $r->getNewState()->getPendingReroll()?->getRollType());
    }

    public function testBezPrehozuSeNepta(): void
    {
        $s = (new GameStateBuilder())
            ->withHomeTeam(TeamStateDTO::create(1, 'Home', 'Human', TeamSide::HOME, 0))
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1)
            ->withBallCarried(1)
            ->build();
        $r = $this->resolver(array_fill(0, 8, 2))->resolve($s, ActionType::PASS, ['playerId' => 1, 'targetX' => 10, 'targetY' => 5]);

        $this->assertNull($r->getNewState()->getPendingReroll());
    }
}
