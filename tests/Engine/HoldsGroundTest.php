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
 * Kdo drzi pole proti odtlaceni (balik E, E18-E20; vzor C++ `holdsGround`):
 * - Take Root: "may not ... be pushed back for any reason" (`rules_bb2016.txt` r. 8578-8579),
 *   zakoreneni konci srazenim nebo polozenim (r. 8575-8576);
 * - Stand Firm (r. 8510-8516) jen STOJICI: "Only Extraordinary skills work when a
 *   player is Prone or Stunned" (r. 1824-1825).
 */
final class HoldsGroundTest extends TestCase
{
    public function testRootedAndStandFirmRules(): void
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::AWAY, 6, 5, skills: [SkillName::StandFirm], id: 1)
            ->addPronePlayer(TeamSide::AWAY, 7, 5, skills: [SkillName::StandFirm], id: 2)
            ->addPlayer(TeamSide::AWAY, 8, 5, id: 3)
            ->build();

        $this->assertTrue($s->requirePlayer(1)->holdsGround(TeamSide::HOME), 'stojici Stand Firm proti souperi');
        $this->assertFalse($s->requirePlayer(1)->holdsGround(TeamSide::AWAY), 'vlastni tym Stand Firm nevyuzije');
        $this->assertFalse($s->requirePlayer(2)->holdsGround(TeamSide::HOME), 'lezici Stand Firm nepouzije');
        $zakoreneny = $s->requirePlayer(3)->withRooted(true);
        $this->assertTrue($zakoreneny->holdsGround(TeamSide::HOME));
        $this->assertTrue($zakoreneny->holdsGround(TeamSide::AWAY), 'zakoreneny i proti vlastnimu tymu');
    }

    public function testKnockdownEndsRoot(): void
    {
        $p = (new GameStateBuilder())->addPlayer(TeamSide::HOME, 5, 5, id: 1)->build()->requirePlayer(1)->withRooted(true);

        $this->assertFalse($p->withState(PlayerState::PRONE)->isRooted());
        $this->assertTrue($p->withState(PlayerState::STANDING)->isRooted());
    }

    /** Bezny blok, vysledek Pushed: zakoreneny zustane stat na miste. */
    public function testRootedDefenderIsNotPushedByBlock(): void
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, id: 2)
            ->withBallOffPitch()
            ->build();
        $s = $s->withPlayer($s->requirePlayer(2)->withRooted(true));

        // 1 kostka: 3 = Pushed
        $r = (new ActionResolver(new FixedDiceRoller([3])))->resolve($s, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $pos = $r->getNewState()->requirePlayer(2)->requirePosition();
        $this->assertSame([6, 5], [$pos->getX(), $pos->getY()]);
    }

    // === Review P186, M3: za odtlačeným stojí jen hráči, kteří drží pole ===
    //
    // `rules_bb2016.txt` ř. 8514-8516: "If a player is pushed back into a player with using
    // Stand Firm then neither player moves." Do davu se jde jen z okraje hřiště (ř. 650-651:
    // "Players must be pushed off the pitch if there are no eligible empty squares on the
    // pitch") -- uprostřed hřiště žádné pole mimo hřiště není.

    /** HOME 1 (5,5) blokuje AWAY 2 (6,5); všechna tři pole odtlačení (7,4) (7,5) (7,6) drží AWAY se Stand Firm. */
    private function obranceMaZaZadyStandFirm(): \App\DTO\GameState
    {
        return (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, id: 2)
            ->addPlayer(TeamSide::AWAY, 7, 4, skills: [SkillName::StandFirm], id: 3)
            ->addPlayer(TeamSide::AWAY, 7, 5, skills: [SkillName::StandFirm], id: 4)
            ->addPlayer(TeamSide::AWAY, 7, 6, skills: [SkillName::StandFirm], id: 5)
            ->withBallOffPitch()
            ->build();
    }

    /** @return list<array{int, int}|null> pole hráčů s danými id (null = mimo hřiště) */
    private function pole(\App\DTO\GameState $s, int ...$ids): array
    {
        return array_map(function (int $id) use ($s) {
            $p = $s->requirePlayer($id)->getPosition();

            return $p === null ? null : [$p->getX(), $p->getY()];
        }, $ids);
    }

    public function testPushedDoHracuSeStandFirmNikymNepohne(): void
    {
        // 1 kostka: 3 = Pushed. Stará mechanika: obránce šel do davu (zranění 2D6) z (6,5).
        $dice = new FixedDiceRoller([3]);
        $r = (new ActionResolver($dice))->resolve($this->obranceMaZaZadyStandFirm(), ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $this->assertSame(
            [[5, 5], [6, 5], [7, 4], [7, 5], [7, 6]],
            $this->pole($r->getNewState(), 1, 2, 3, 4, 5),
            'ř. 8514-8516: nikdo se nehýbe -- obránce nejde do davu, útočník nenásleduje',
        );
        $this->assertSame(PlayerState::STANDING, $r->getNewState()->requirePlayer(2)->getState());
        $this->assertNotContains('crowd_surf', array_map(fn($e) => $e->getType(), $r->getEvents()));
        $this->assertSame(1, $dice->getRollCount(), 'jen kostka bloku -- žádné zranění od davu');
    }

    public function testDefenderDownDoHracuSeStandFirmPadaNaSvemPoli(): void
    {
        // 6 = Defender Down: odtlačit nejde, sražení platí -- padá, kde stál; brnění 1+1 drží.
        $dice = new FixedDiceRoller([6, 1, 1]);
        $r = (new ActionResolver($dice))->resolve($this->obranceMaZaZadyStandFirm(), ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $this->assertSame([[5, 5], [6, 5]], $this->pole($r->getNewState(), 1, 2));
        $this->assertSame(PlayerState::PRONE, $r->getNewState()->requirePlayer(2)->getState());
        $this->assertSame(3, $dice->getRollCount(), 'kostka bloku + hod na brnění (ne zranění od davu bez brnění)');
    }

    public function testRetezDoHracuSeStandFirmNikymNepohne(): void
    {
        // Řetěz (ř. 644-646: sekundární odtlačení "treated exactly like a normal push back"):
        // obránce 2 může jen na (7,5), kde stojí AWAY 4 BEZ Stand Firm; za ním (8,4) (8,5)
        // (8,6) drží Stand Firm. Hráč 4 je "pushed back into a player using Stand Firm" ⇒
        // nehýbe se, a obránce tak nemá kam. Stará mechanika: hráč 4 šel do davu z (7,5).
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, id: 2)
            ->addPlayer(TeamSide::AWAY, 7, 4, skills: [SkillName::StandFirm], id: 3)
            ->addPlayer(TeamSide::AWAY, 7, 5, id: 4)
            ->addPlayer(TeamSide::AWAY, 7, 6, skills: [SkillName::StandFirm], id: 5)
            ->addPlayer(TeamSide::AWAY, 8, 4, skills: [SkillName::StandFirm], id: 6)
            ->addPlayer(TeamSide::AWAY, 8, 5, skills: [SkillName::StandFirm], id: 7)
            ->addPlayer(TeamSide::AWAY, 8, 6, skills: [SkillName::StandFirm], id: 8)
            ->withBallOffPitch()
            ->build();

        $dice = new FixedDiceRoller([3]);
        $r = (new ActionResolver($dice))->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $this->assertSame([[5, 5], [6, 5], [7, 5]], $this->pole($r->getNewState(), 1, 2, 4));
        $this->assertSame(1, $dice->getRollCount());
    }

    public function testULajnyZaStandFirmJdeObranceDoDavu(): void
    {
        // Pozitivní kontrola (ř. 650-651): HOME 1 (5,1) blokuje AWAY 2 (6,0) šikmo k lajně.
        // Pole odtlačení: (7,-1) a (6,-1) mimo hřiště, (7,0) drží Stand Firm ⇒ na hřišti
        // volné pole není a pole mimo hřiště k dispozici JE ⇒ dav. Zranění 1+1 = omráčen
        // ⇒ do rezerv (ř. 655-658).
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 1, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 0, id: 2)
            ->addPlayer(TeamSide::AWAY, 7, 0, skills: [SkillName::StandFirm], id: 3)
            ->withBallOffPitch()
            ->build();

        $r = (new ActionResolver(new FixedDiceRoller([3, 1, 1])))->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $this->assertSame([null, [7, 0]], $this->pole($r->getNewState(), 2, 3));
        $this->assertContains('crowd_surf', array_map(fn($e) => $e->getType(), $r->getEvents()));
    }

    /** Fanatic proti zakorenenemu stromu: Pushed ho nepohne, Fanatic nepostoupi. */
    public function testBallAndChainCannotPushRootedPlayer(): void
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 1, id: 1, skills: [SkillName::BallAndChain])
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2)
            ->withBallOffPitch()
            ->build();
        $s = $s->withPlayer($s->requirePlayer(2)->withRooted(true));

        // D6 3 => rovne na (6,7); 1 kostka: 3 = Pushed
        $r = (new ActionResolver(new FixedDiceRoller([3, 3])))->resolve($s, ActionType::BALL_AND_CHAIN, ['playerId' => 1]);

        $strom = $r->getNewState()->requirePlayer(2)->requirePosition();
        $bnc = $r->getNewState()->requirePlayer(1)->requirePosition();
        $this->assertSame([6, 7], [$strom->getX(), $strom->getY()]);
        $this->assertSame([5, 7], [$bnc->getX(), $bnc->getY()], 'pole se neuvolnilo, follow-up neni');
    }

    /** Fanatic proti LEZICIMU se Stand Firm: odtlaci ho (r. 1824-1825). */
    public function testBallAndChainPushesProneStandFirm(): void
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 1, id: 1, skills: [SkillName::BallAndChain])
            ->addPronePlayer(TeamSide::AWAY, 6, 7, skills: [SkillName::StandFirm], id: 2)
            ->addPlayer(TeamSide::AWAY, 9, 7, id: 3)
            ->withBallOffPitch()
            ->build();

        // D6 3 => (6,7) lezici; odtlaceni + brneni 2+2 drzi
        $r = (new ActionResolver(new FixedDiceRoller([3, 2, 2])))->resolve($s, ActionType::BALL_AND_CHAIN, ['playerId' => 1]);

        $pos = $r->getNewState()->requirePlayer(2)->requirePosition();
        $this->assertSame(7, $pos->getX(), 'odtlacen, Stand Firm vleze neplati');
        $this->assertLessThanOrEqual(1, abs($pos->getY() - 7));
    }
}
