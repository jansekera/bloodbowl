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
 * P289 (10.11.2026): kouč-člověk dostane nabídku přehozu i u hodů velkých hráčů a u Leapu.
 * Týmový přehoz smí přehodit jakýkoli hod (`rules_bb2016.txt` ř. 929-931), Pro taky (ř. 8381-8387).
 * Bone Head ř. 7980-7986, Really Stupid ř. 8390-8401, Wild Animal ř. 8666-8669, Leap ř. 8264-8281.
 * AI hody velkých hráčů nepřehazuje — parita s C++ P130.
 */
final class CoachRerollDialogTest extends TestCase
{
    private function boneHead(): \App\DTO\GameState
    {
        return (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 5, skills: [SkillName::BoneHead], id: 1)
            ->addPlayer(TeamSide::AWAY, 15, 5, id: 2)
            ->withBallOffPitch()
            ->build();
    }

    private function interaktivni(FixedDiceRoller $dice): ActionResolver
    {
        $r = new ActionResolver($dice);
        $r->setInteractiveRerolls(true);

        return $r;
    }

    public function testBoneHeadNeuspechNabidnePrehoz(): void
    {
        $r = $this->interaktivni(new FixedDiceRoller([1]))
            ->resolve($this->boneHead(), ActionType::MOVE, ['playerId' => 1, 'x' => 7, 'y' => 5]);

        $pending = $r->getNewState()->getPendingReroll();
        $this->assertNotNull($pending);
        $this->assertSame('bone_head', $pending->getRollType());
        $this->assertSame(2, $pending->getTarget());
        $this->assertTrue($pending->isTeamRerollAvailable());
        $this->assertFalse($r->getNewState()->requirePlayer(1)->isBigGuyStupefied(), 'neúspěch ještě neplatí');
    }

    public function testBoneHeadPrehozPovedenyAkcePokracuje(): void
    {
        $dice = new FixedDiceRoller([1, 4]);
        $resolver = $this->interaktivni($dice);
        $r = $resolver->resolve($this->boneHead(), ActionType::MOVE, ['playerId' => 1, 'x' => 7, 'y' => 5]);
        $r = $resolver->resolve($r->getNewState(), ActionType::RESOLVE_REROLL, ['choice' => 'team_reroll']);

        $s = $r->getNewState();
        $this->assertNull($s->getPendingReroll());
        $this->assertSame(7, $s->requirePlayer(1)->requirePosition()->getX(), 'ohlášený pohyb doběhl');
        $this->assertFalse($s->requirePlayer(1)->isBigGuyStupefied());
        $this->assertFalse($s->getTeamState(TeamSide::HOME)->canUseReroll(), 'týmový přehoz spotřebován');
    }

    public function testBoneHeadPrehozNepovedenyAkcePropadne(): void
    {
        $resolver = $this->interaktivni(new FixedDiceRoller([1, 1]));
        $r = $resolver->resolve($this->boneHead(), ActionType::MOVE, ['playerId' => 1, 'x' => 7, 'y' => 5]);
        $r = $resolver->resolve($r->getNewState(), ActionType::RESOLVE_REROLL, ['choice' => 'team_reroll']);

        $p = $r->getNewState()->requirePlayer(1);
        $this->assertTrue($p->isBigGuyStupefied());
        $this->assertSame(5, $p->requirePosition()->getX());
        $this->assertFalse($r->isTurnover(), 'Bone Head není turnover');
    }

    public function testBoneHeadOdmitnutiPrehozuAkcePropadne(): void
    {
        $resolver = $this->interaktivni(new FixedDiceRoller([1]));
        $r = $resolver->resolve($this->boneHead(), ActionType::MOVE, ['playerId' => 1, 'x' => 7, 'y' => 5]);
        $r = $resolver->resolve($r->getNewState(), ActionType::RESOLVE_REROLL, ['choice' => 'decline']);

        $this->assertTrue($r->getNewState()->requirePlayer(1)->isBigGuyStupefied());
        $this->assertTrue($r->getNewState()->getTeamState(TeamSide::HOME)->canUseReroll(), 'přehoz zůstal');
    }

    public function testReallyStupidNabidneCilBezSouseda(): void
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 5, skills: [SkillName::ReallyStupid], id: 1)
            ->addPlayer(TeamSide::AWAY, 15, 5, id: 2)
            ->withBallOffPitch()
            ->build();
        $r = $this->interaktivni(new FixedDiceRoller([3]))->resolve($s, ActionType::MOVE, ['playerId' => 1, 'x' => 7, 'y' => 5]);

        $pending = $r->getNewState()->getPendingReroll();
        $this->assertNotNull($pending);
        $this->assertSame('really_stupid', $pending->getRollType());
        $this->assertSame(4, $pending->getTarget());
    }

    public function testWildAnimalBlokPrehozPovedenyBlokPokracuje(): void
    {
        // blok: Wild Animal +2 ⇒ přirozená 2+; 1 padne, přehoz 5 projde, blok 2 kostky (ST 5 vs 3) obě Pow (6), brnění 1+1
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 5, skills: [SkillName::WildAnimal], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, id: 2)
            ->withBallOffPitch()
            ->build();
        $resolver = $this->interaktivni(new FixedDiceRoller([1, 5, 6, 6, 1, 1]));
        $r = $resolver->resolve($s, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);
        $pending = $r->getNewState()->getPendingReroll();
        $this->assertNotNull($pending);
        $this->assertSame('wild_animal', $pending->getRollType());
        $this->assertSame(2, $pending->getTarget());

        $r = $resolver->resolve($r->getNewState(), ActionType::RESOLVE_REROLL, ['choice' => 'team_reroll']);
        $this->assertNotSame(PlayerState::STANDING, $r->getNewState()->requirePlayer(2)->getState(), 'blok proběhl');
    }

    public function testAiBigGuyNepřehazuje(): void
    {
        $r = (new ActionResolver(new FixedDiceRoller([1])))
            ->resolve($this->boneHead(), ActionType::MOVE, ['playerId' => 1, 'x' => 7, 'y' => 5]);

        $this->assertNull($r->getNewState()->getPendingReroll());
        $this->assertTrue($r->getNewState()->requirePlayer(1)->isBigGuyStupefied());
        $this->assertTrue($r->getNewState()->getTeamState(TeamSide::HOME)->canUseReroll());
    }

    public function testBezPrehozuSeNenabizi(): void
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 5, skills: [SkillName::BoneHead], id: 1)
            ->addPlayer(TeamSide::AWAY, 15, 5, id: 2)
            ->withHomeTeam(TeamStateDTO::create(1, 'Home', 'Orc', TeamSide::HOME, 0))
            ->withBallOffPitch()
            ->build();
        $r = $this->interaktivni(new FixedDiceRoller([1]))->resolve($s, ActionType::MOVE, ['playerId' => 1, 'x' => 7, 'y' => 5]);

        $this->assertNull($r->getNewState()->getPendingReroll());
        $this->assertTrue($r->getNewState()->requirePlayer(1)->isBigGuyStupefied());
    }

    private function skok(): \App\DTO\GameState
    {
        return (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, skills: [SkillName::Leap], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, id: 2)
            ->withBallOffPitch()
            ->build();
    }

    public function testLeapNeuspechNabidnePrehoz(): void
    {
        $r = $this->interaktivni(new FixedDiceRoller([2]))
            ->resolve($this->skok(), ActionType::MOVE, ['playerId' => 1, 'x' => 7, 'y' => 5]);

        $pending = $r->getNewState()->getPendingReroll();
        $this->assertNotNull($pending);
        $this->assertSame('leap', $pending->getRollType());
        $this->assertSame(4, $pending->getTarget());
        $this->assertSame(7, $pending->getTargetX());
        $this->assertFalse($r->isTurnover());
        $this->assertSame(5, $r->getNewState()->requirePlayer(1)->requirePosition()->getX());
    }

    public function testLeapPrehozPovedenyHracDopadneASkokStojiDvePole(): void
    {
        $resolver = $this->interaktivni(new FixedDiceRoller([2, 5]));
        $r = $resolver->resolve($this->skok(), ActionType::MOVE, ['playerId' => 1, 'x' => 7, 'y' => 5]);
        $r = $resolver->resolve($r->getNewState(), ActionType::RESOLVE_REROLL, ['choice' => 'team_reroll']);

        $p = $r->getNewState()->requirePlayer(1);
        $this->assertFalse($r->isTurnover());
        $this->assertSame(7, $p->requirePosition()->getX());
        $this->assertSame(4, $p->getMovementRemaining());
    }

    public function testLeapOdmitnutiHracSrazenVCili(): void
    {
        // brnění 1+1
        $resolver = $this->interaktivni(new FixedDiceRoller([2, 1, 1]));
        $r = $resolver->resolve($this->skok(), ActionType::MOVE, ['playerId' => 1, 'x' => 7, 'y' => 5]);
        $r = $resolver->resolve($r->getNewState(), ActionType::RESOLVE_REROLL, ['choice' => 'decline']);

        $p = $r->getNewState()->requirePlayer(1);
        $this->assertTrue($r->isTurnover());
        $this->assertSame(PlayerState::PRONE, $p->getState());
        $this->assertSame(7, $p->requirePosition()->getX());
    }

    public function testLeapGfiNeuspechNabidnePrehozAPovedenyDopadne(): void
    {
        // MA 1 ⇒ skok stojí jeden GFI (ř. 1701): skok 5 projde, GFI 1 padne ⇒ nabídka, přehoz 3 projde
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 1, skills: [SkillName::Leap], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, id: 2)
            ->withBallOffPitch()
            ->build();
        $resolver = $this->interaktivni(new FixedDiceRoller([5, 1, 3]));
        $r = $resolver->resolve($s, ActionType::MOVE, ['playerId' => 1, 'x' => 7, 'y' => 5]);
        $pending = $r->getNewState()->getPendingReroll();
        $this->assertNotNull($pending);
        $this->assertSame('leap_gfi', $pending->getRollType());

        $r = $resolver->resolve($r->getNewState(), ActionType::RESOLVE_REROLL, ['choice' => 'team_reroll']);
        $p = $r->getNewState()->requirePlayer(1);
        $this->assertFalse($r->isTurnover());
        $this->assertSame(7, $p->requirePosition()->getX());
        $this->assertSame(-1, $p->getMovementRemaining());
    }
}
