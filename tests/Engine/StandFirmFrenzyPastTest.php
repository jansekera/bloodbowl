<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\GameState;
use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Enum\ActionType;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * P225 (PHP) — AI obránce nepoužije Stand Firm při nachystané pasti na Frenzy (uživatel 21.10.2026).
 *
 * Stand Firm je volba (ř. 8510-8516 "may choose to not be pushed back"). Frenzy (ř. 8134-8145):
 * po Pushed / Defender Stumbles musí následovat a hodit druhý blok, "so long as they are both still
 * standing and adjacent". Se Stand Firm druhý blok ze STEJNÉHO pole, bez něj z NOVÉHO pole po
 * odtlačení a follow-upu. AI Stand Firm nepoužije, když zároveň (a) druhý blok z nového pole je
 * pro útočníka horší (podepsané kostky: + volí útočník, − obránce), (b) odtlačení nevede do davu,
 * (c) obránce nenese míč. Jinak ho použije jako dosud.
 *
 * Past: útočník HOME (5,7) Frenzy, asistence HOME (5,6) ⇒ ze stejného pole 4:3 = 2 kostky volí
 * útočník (+2). Obránce AWAY (6,7) Stand Firm; AWAY (7,6) zabírá jedno pole odtlačení, obránce jde
 * na (7,8) (nejhorší pole: shoda zón, blíž k lajně), útočník na (6,7). Z nového pole: asistence
 * (5,6) už nesousedí s obráncem, AWAY (7,6) sousedí s útočníkem ⇒ 3:4 = 2 kostky volí obránce (−2).
 */
final class StandFirmFrenzyPastTest extends TestCase
{
    /**
     * @param list<SkillName> $utocnik
     * @param list<SkillName> $obrance
     */
    private function past(array $utocnik = [SkillName::Frenzy], array $obrance = [SkillName::StandFirm], bool $pomocnici = true): \App\Tests\Engine\GameStateBuilder
    {
        $b = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, skills: $utocnik, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 7, skills: $obrance, id: 2);
        if ($pomocnici) {
            $b = $b->addPlayer(TeamSide::HOME, 5, 6, id: 3)
                ->addPlayer(TeamSide::AWAY, 7, 6, id: 4);
        }

        return $b->withBallOffPitch();
    }

    /**
     * Člověk útočí (interaktivní kostky), AI brání (nikoho se na Stand Firm neptá).
     *
     * @param list<int> $kostky kostky prvního bloku
     */
    private function blok(GameState $s, array $kostky = [3, 3], ActionType $akce = ActionType::BLOCK): GameState
    {
        $r = new ActionResolver(new FixedDiceRoller($kostky));
        $r->setInteractiveBlocks(true);
        $hod = $r->resolve($s, $akce, ['playerId' => 1, 'targetId' => 2])->getNewState();
        $this->assertNotNull($hod->getPendingBlock(), 'fixtura: čeká volba kostky');

        $r2 = new ActionResolver(new FixedDiceRoller(array_fill(0, 12, 3)));
        $r2->setInteractiveBlocks(true);

        return $r2->resolve($hod, ActionType::CHOOSE_BLOCK_DIE, ['faceIndex' => 0])->getNewState();
    }

    /** @return array{0: int, 1: int} */
    private function pole(GameState $s, int $id): array
    {
        $p = $s->requirePlayer($id)->requirePosition();

        return [$p->getX(), $p->getY()];
    }

    public function testFixturaZeStejnehoPoleDveKostkyUtocnika(): void
    {
        $r = new ActionResolver(new FixedDiceRoller([3, 3]));
        $r->setInteractiveBlocks(true);
        $p = $r->resolve($this->past()->build(), ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2])->getNewState()->getPendingBlock();
        $this->assertNotNull($p);
        $this->assertCount(2, $p->getFaces());
        $this->assertTrue($p->isAttackerChooses());
    }

    public function testPastAINepouzijeStandFirm(): void
    {
        $s = $this->blok($this->past()->build());

        $this->assertSame([7, 8], $this->pole($s, 2), 'obránce se nechal odtlačit');
        $this->assertSame([6, 7], $this->pole($s, 1), 'Frenzy následuje');
        $druhy = $s->getPendingBlock();
        $this->assertNotNull($druhy, 'druhý blok Frenzy z nového pole');
        $this->assertTrue($druhy->isFrenzy());
        $this->assertCount(2, $druhy->getFaces());
        $this->assertFalse($druhy->isAttackerChooses(), 'z nového pole volí obránce');
    }

    /**
     * Dauntless (parita s C++ `P225DauntlessBranchMakesTheTrap`, review P226 MEDIUM): útočník ST1 + Dauntless,
     * obránce ST6. Bez Dauntless je druhá rána z obou polí stejná (2:6 i 1:7 = 3 kostky volí obránce), ale
     * když Dauntless uspěje, je ze stejného pole 7:6 (+2) a z nového 6:7 (−2) ⇒ past, Stand Firm nepoužije.
     * Dřív PHP srovnávalo jen větev bez Dauntless a Stand Firm použilo.
     */
    public function testDauntlessVetevDelaPast(): void
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, strength: 1, skills: [SkillName::Frenzy, SkillName::Dauntless], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 7, strength: 6, skills: [SkillName::StandFirm], id: 2)
            ->addPlayer(TeamSide::HOME, 5, 6, id: 3)
            ->addPlayer(TeamSide::AWAY, 7, 6, id: 4)
            ->withBallOffPitch()->build();
        $s = $this->blok($s, [6, 3, 3]);   // Dauntless 6 (1+6 > 6) ⇒ 7:6, dvě kostky útočníka; Pushed
        $this->assertSame([7, 8], $this->pole($s, 2), 'obránce se nechal odtlačit');
        $this->assertSame([6, 7], $this->pole($s, 1), 'Frenzy následuje');
    }

    public function testBezHorsichKostekPouzije(): void
    {
        // bez pomocníků: 3:3 = 1 kostka ze stejného i z nového pole ⇒ není horší
        $s = $this->blok($this->past(pomocnici: false)->build(), [3]);

        $this->assertSame([6, 7], $this->pole($s, 2));
        $this->assertSame([5, 7], $this->pole($s, 1));
    }

    public function testNosicMicePouzije(): void
    {
        $s = $this->blok($this->past()->withBallCarried(2)->build());

        $this->assertSame([6, 7], $this->pole($s, 2));
    }

    public function testUtocnikBezFrenzyPouzije(): void
    {
        $s = $this->blok($this->past(utocnik: [])->build());

        $this->assertSame([6, 7], $this->pole($s, 2));
    }

    public function testDoDavuPouzije(): void
    {
        // Obránce u postranní čáry (y=0), útočník pod ním: všechna pole odtlačení mimo hřiště.
        // Asistence útočníka (4,0) ze stejného pole, obránce bez asistencí ⇒ +2; kdyby se dav
        // nečetl, odtlačený by druhý blok nedostal a past by se počítala z nesmyslu.
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 1, skills: [SkillName::Frenzy], id: 1)
            ->addPlayer(TeamSide::AWAY, 5, 0, skills: [SkillName::StandFirm], id: 2)
            ->addPlayer(TeamSide::HOME, 4, 0, id: 3)
            ->withBallOffPitch()
            ->build();
        $s = $this->blok($s);

        $this->assertSame([5, 0], $this->pole($s, 2), 'do davu ne');
    }

    public function testStumblesBezDodgeObranceSpadnePouzije(): void
    {
        // Defender Stumbles bez Dodge ⇒ obránce leží, druhý blok nebude ani bez Stand Firm
        $s = $this->blok($this->past()->build(), [5, 5]);

        $this->assertSame([6, 7], $this->pole($s, 2));
    }

    public function testStumblesSDodgeJePast(): void
    {
        // s Dodge obránce Stumbles ustojí ⇒ druhý blok z nového pole ⇒ past
        $s = $this->blok($this->past(obrance: [SkillName::StandFirm, SkillName::Dodge])->build(), [5, 5]);

        $this->assertSame([7, 8], $this->pole($s, 2));
    }

    /**
     * NOVÉ PRAVIDLO (uživatel 21.10.2026, „1 ano“): útočník s Frenzy NEMŮŽE následovat ⇒ obránce bez
     * Stand Firm odletí, nesousedí a druhá rána odpadne (Frenzy ř. 8139-8141 "both still standing
     * and adjacent"; Fend ř. 8113-8118; Take Root ř. 8580-8583) ⇒ AI Stand Firm nepoužije.
     * Bez pomocníků: kostky by se nezhoršily, rozhoduje jen to, že druhá rána odpadne.
     */
    public function testFendObranceNepouzije(): void
    {
        $s = $this->blok($this->past(obrance: [SkillName::StandFirm, SkillName::Fend], pomocnici: false)->build(), [3]);

        $this->assertSame(7, $this->pole($s, 2)[0], 'obránce se nechal odtlačit');
        $this->assertSame([5, 7], $this->pole($s, 1), 'Fend: útočník nenásleduje');
        $this->assertNull($s->getPendingBlock(), 'druhá rána odpadla');
    }

    public function testZakorenenyUtocnikNepouzije(): void
    {
        $s = $this->past(pomocnici: false)->build();
        $s = $s->withPlayer($s->requirePlayer(1)->withRooted(true));
        $s = $this->blok($s, [3]);

        $this->assertSame(7, $this->pole($s, 2)[0]);
        $this->assertSame([5, 7], $this->pole($s, 1));
        $this->assertNull($s->getPendingBlock());
    }

    public function testFendADavPouzije(): void
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 1, skills: [SkillName::Frenzy], id: 1)
            ->addPlayer(TeamSide::AWAY, 5, 0, skills: [SkillName::StandFirm, SkillName::Fend], id: 2)
            ->withBallOffPitch()
            ->build();
        $s = $this->blok($s, [3]);

        $this->assertSame([5, 0], $this->pole($s, 2), 'do davu ne');
    }

    public function testFendANosicPouzije(): void
    {
        $s = $this->blok($this->past(obrance: [SkillName::StandFirm, SkillName::Fend], pomocnici: false)->withBallCarried(2)->build(), [3]);

        $this->assertSame([6, 7], $this->pole($s, 2));
    }

    public function testFendAJuggernautVBlitzuStandFirmNeplatiAUtocnikNasleduje(): void
    {
        // Juggernaut v blitzu: soupeř nesmí použít Fend ani Stand Firm (ř. 8193-8196) ⇒ odtlačen,
        // útočník následuje a druhá rána se hází (oba stojí a sousedí)
        $s = $this->past(utocnik: [SkillName::Frenzy, SkillName::Juggernaut], obrance: [SkillName::StandFirm, SkillName::Fend], pomocnici: false)->build();
        $s = $this->blok($s, [3], ActionType::BLITZ);

        $this->assertSame(7, $this->pole($s, 2)[0]);
        $this->assertSame([6, 7], $this->pole($s, 1), 'Fend proti Juggernautovi v blitzu neplatí');
        $this->assertNotNull($s->getPendingBlock(), 'druhá rána Frenzy');
    }

    public function testBlitzSGfiNaDruhouRanuJePast(): void
    {
        // pohyb 0: první rána stojí GFI (zbude -1), druhá jde ještě za GFI (-2) ⇒ druhá rána bude
        $s = $this->past()->build();
        $s = $s->withPlayer($s->requirePlayer(1)->withMovementRemaining(0));
        $s = $this->blok($s, [3, 3, 3], ActionType::BLITZ);   // GFI 3, kostky 3,3

        $this->assertSame([7, 8], $this->pole($s, 2));
    }

    public function testBlitzBezPohybuNaDruhouRanuPouzije(): void
    {
        // pohyb -1: první rána za GFI (zbude -2), na druhou už pole ani GFI nezbývá (ř. 8143-8145) ⇒ past není
        $s = $this->past()->build();
        $s = $s->withPlayer($s->requirePlayer(1)->withMovementRemaining(-1));
        $s = $this->blok($s, [3, 3, 3], ActionType::BLITZ);

        $this->assertSame([6, 7], $this->pole($s, 2));
    }

    public function testMultipleBlockPouzije(): void
    {
        // Multiple Block: bez follow-upu a bez druhé rány Frenzy ⇒ past nevzniká
        $s = $this->past(utocnik: [SkillName::Frenzy, SkillName::MultipleBlock])->addPlayer(TeamSide::AWAY, 4, 7, id: 9)->build();
        $r = new ActionResolver(new FixedDiceRoller(array_fill(0, 30, 3)));
        $s = $r->resolve($s, ActionType::MULTIPLE_BLOCK, ['playerId' => 1, 'targetId' => 2, 'targetId2' => 9])->getNewState();

        $this->assertSame([6, 7], $this->pole($s, 2));
    }

    public function testAIHracSeStandFirmVRetezuDrziIPriPasti(): void
    {
        // Za obráncem (7,6),(7,7) AWAY se Stand Firm a (7,8) AWAY bez něj ⇒ řetěz jde přes (7,8).
        // Ze stejného pole 4:3 (+2), z nového 3:5 (−2) ⇒ past; v řetězu AI Stand Firm drží jako dosud.
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, skills: [SkillName::Frenzy], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 7, skills: [SkillName::StandFirm], id: 2)
            ->addPlayer(TeamSide::HOME, 5, 6, id: 3)
            ->addPlayer(TeamSide::AWAY, 7, 8, id: 4)
            ->addPlayer(TeamSide::AWAY, 7, 6, skills: [SkillName::StandFirm], id: 5)
            ->addPlayer(TeamSide::AWAY, 7, 7, skills: [SkillName::StandFirm], id: 6)
            ->withBallOffPitch()
            ->build();
        $s = $this->blok($s);

        $this->assertSame([7, 8], $this->pole($s, 2), 'obránce Stand Firm nepoužil');
        $this->assertSame(8, $this->pole($s, 4)[0], 'řetěz odtlačil hráče bez Stand Firm');
        $this->assertSame([7, 6], $this->pole($s, 5));
        $this->assertSame([7, 7], $this->pole($s, 6));
    }

    public function testSideStepPrehraniDaTotezPoleJakoOdtlaceni(): void
    {
        // Side Step obránce vybírá pole sám (nejméně zón): (7,6). Běžné odtlačení by šlo na (7,8)
        // (nejvíc zón, HOME (8,8)), odkud by druhá rána nebyla horší (asistence (8,8) ⇒ +2 = +2).
        // Z (7,6) je 3:3 (+1) < +2 ⇒ past jen tehdy, když přehrání vybere pole jako skutečné odtlačení.
        $s = $this->past(obrance: [SkillName::StandFirm, SkillName::SideStep], pomocnici: false)
            ->addPlayer(TeamSide::HOME, 5, 6, id: 3)
            ->addPlayer(TeamSide::HOME, 8, 8, id: 4)
            ->build();
        $s = $this->blok($s);

        $this->assertSame([7, 6], $this->pole($s, 2), 'Side Step, Stand Firm nepoužit');
        $this->assertSame([6, 7], $this->pole($s, 1));
    }

    public function testDruhaRanaFrenzyStandFirmPouzije(): void
    {
        // Po odmítnutí v pasti: druhá rána (Pushed) už další ránu nespouští ⇒ Stand Firm
        $s = $this->blok($this->past()->build());
        $this->assertNotNull($s->getPendingBlock());
        $r = new ActionResolver(new FixedDiceRoller(array_fill(0, 12, 3)));
        $r->setInteractiveBlocks(true);
        $po = $r->resolve($s, ActionType::CHOOSE_BLOCK_DIE, ['faceIndex' => 0])->getNewState();

        $this->assertSame([7, 8], $this->pole($po, 2), 've druhé ráně drží pole');
        $this->assertNull($po->getPendingBlock());
    }
}
