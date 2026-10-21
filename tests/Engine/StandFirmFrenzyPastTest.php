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
    private function blok(GameState $s, array $kostky = [3, 3]): GameState
    {
        $r = new ActionResolver(new FixedDiceRoller($kostky));
        $r->setInteractiveBlocks(true);
        $hod = $r->resolve($s, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2])->getNewState();
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

    public function testFendPouzije(): void
    {
        // Fend: útočník nesmí následovat (ř. 8113-8118) ⇒ nesousedí, druhý blok nebude
        $s = $this->blok($this->past(obrance: [SkillName::StandFirm, SkillName::Fend])->build());

        $this->assertSame([6, 7], $this->pole($s, 2));
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
