<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\ActionResult;
use App\DTO\GameState;
use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Engine\RulesEngine;
use App\Enum\ActionType;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * P224 — PARITA S C++ P222: webová hra se kouče-člověka ptá, jestli útočník použije Grab.
 *
 * `rules_bb2016.txt` ř. 8147-8154 (Grab): "only while making a Block Action, if his block results
 * in a push back he **may** choose **any empty square adjacent to his opponent** … When making a
 * Block or Blitz Action, Grab and Side Step will cancel each other out … Grab will not work if
 * there are no empty adjacent squares." Ball & Chain ř. 7829-7831: Grab "as if a Block Action was
 * being used with his blocks". Skill je volba (ř. 1820 "Skill use is not mandatory.").
 * Ptá se jen kouče-člověka (interaktivní kostky); AI se neptá a její chování se nemění.
 */
final class GrabKoucTest extends TestCase
{
    /** @param list<int> $kostky */
    private function resolver(array $kostky, bool $clovek = true): ActionResolver
    {
        $r = new ActionResolver(new FixedDiceRoller($kostky));
        $r->setInteractiveBlocks($clovek);

        return $r;
    }

    /**
     * Útočník (5,7) → obránce (6,7); okolí obránce prázdné.
     *
     * @param list<SkillName> $utocnik
     * @param list<SkillName> $obrance
     */
    private function stav(array $utocnik = [SkillName::Grab], array $obrance = []): GameState
    {
        return (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, skills: $utocnik, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 7, skills: $obrance, id: 2)
            ->withBallOffPitch()
            ->build();
    }

    /**
     * Hodí blok (výchozí 1 kostka = Pushed) a člověk zvolí první kostku.
     *
     * @param list<int> $kostky
     */
    private function zvolKostku(GameState $stav, ActionType $akce = ActionType::BLOCK, array $kostky = [3]): ActionResult
    {
        $hod = $this->resolver($kostky)->resolve($stav, $akce, ['playerId' => 1, 'targetId' => 2]);
        $this->assertNotNull($hod->getNewState()->getPendingBlock(), 'fixtura: čeká volba kostky');

        return $this->resolver([3, 3, 3, 3, 3, 3])->resolve($hod->getNewState(), ActionType::CHOOSE_BLOCK_DIE, ['faceIndex' => 0]);
    }

    /** @return array{0: int, 1: int} */
    private function pole(ActionResult $r, int $id): array
    {
        $p = $r->getNewState()->requirePlayer($id)->requirePosition();

        return [$p->getX(), $p->getY()];
    }

    public function testClovekSeNaGrabZeptaAObranceZatimStojiNaMiste(): void
    {
        $r = $this->zvolKostku($this->stav());

        $otazka = $r->getNewState()->getPendingGrab();
        $this->assertNotNull($otazka, 'Pushed + Grab + volné pole ⇒ otázka');
        $this->assertSame([6, 7], $this->pole($r, 2), 'do odpovědi se neodtlačuje');
        $this->assertSame(
            ['5,6', '5,8', '6,6', '6,8', '7,6', '7,7', '7,8'],
            array_map(static fn($p) => $p->getX() . ',' . $p->getY(), $otazka->getSquares()),
            'ř. 8149-8150: libovolné volné pole vedle obránce',
        );
        $this->assertSame([ActionType::RESOLVE_GRAB->value], array_column((new RulesEngine())->getAvailableActions($r->getNewState()), 'type'));
    }

    public function testNeDaStandardniOdtlaceni(): void
    {
        $q = $this->zvolKostku($this->stav())->getNewState();
        $r = $this->resolver([3, 3, 3])->resolve($q, ActionType::RESOLVE_GRAB, ['use' => false]);

        $this->assertNull($r->getNewState()->getPendingGrab());
        $this->assertNull($r->getNewState()->getPendingBlock());
        $this->assertSame(7, $this->pole($r, 2)[0], 'běžné odtlačení = jedno ze tří polí za obráncem (7,6..8)');
    }

    public function testAnoSVybranymPolemOdtlaciTam(): void
    {
        $q = $this->zvolKostku($this->stav())->getNewState();
        $r = $this->resolver([3, 3, 3])->resolve($q, ActionType::RESOLVE_GRAB, ['use' => true, 'x' => 6, 'y' => 6]);

        $this->assertSame([6, 6], $this->pole($r, 2), 'kouč vybral pole mimo diagram odtlačení');
        $this->assertNull($r->getNewState()->getPendingGrab());
    }

    public function testAnoBezPoleNeboSPolemMimoNabidkuSeOdmitne(): void
    {
        $q = $this->zvolKostku($this->stav())->getNewState();
        $engine = new RulesEngine();

        $this->assertNotSame([], $engine->validate($q, ActionType::RESOLVE_GRAB, ['use' => true]), 'ano bez pole');
        $this->assertNotSame([], $engine->validate($q, ActionType::RESOLVE_GRAB, ['use' => true, 'x' => 5, 'y' => 7]), 'pole útočníka není volné');
        $this->assertNotSame([], $engine->validate($q, ActionType::RESOLVE_GRAB, ['use' => true, 'x' => 9, 'y' => 9]), 'pole daleko od obránce');
        $this->assertSame([], $engine->validate($q, ActionType::RESOLVE_GRAB, ['use' => true, 'x' => 6, 'y' => 8]));
        $this->assertNotSame([], $engine->validate($q, ActionType::END_TURN, []), 'dokud kouč neodpoví, nic jiného nejde');
    }

    public function testVBlitzuSeNeptaGrabNeplati(): void
    {
        // ř. 8148 "only while making a Block Action"
        $r = $this->zvolKostku($this->stav(), ActionType::BLITZ);

        $this->assertNull($r->getNewState()->getPendingGrab());
    }

    public function testBezVolnehoPoleSeNeptaGrabNefunguje(): void
    {
        // ř. 8153-8154 "Grab will not work if there are no empty adjacent squares" -- okolí obránce
        // (kromě útočníka) plné hráčů útočníkova týmu.
        $b = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, skills: [SkillName::Grab], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2)
            ->withBallOffPitch();
        $id = 10;
        foreach ([[5, 6], [5, 8], [6, 6], [6, 8], [7, 6], [7, 7], [7, 8]] as [$x, $y]) {
            $b = $b->addPlayer(TeamSide::HOME, $x, $y, id: $id++);
        }
        $r = $this->zvolKostku($b->build(), kostky: [3, 3, 3]); // 7 asistencí ⇒ 3 kostky

        $this->assertNull($r->getNewState()->getPendingGrab());
    }

    public function testProtiSideStepSeNepta(): void
    {
        // ř. 8151-8153 Grab a Side Step se ruší ⇒ útočník nic nevybírá
        $r = $this->zvolKostku($this->stav(obrance: [SkillName::SideStep]));

        $this->assertNull($r->getNewState()->getPendingGrab());
    }

    public function testBezGrabuSeNepta(): void
    {
        $r = $this->zvolKostku($this->stav(utocnik: []));

        $this->assertNull($r->getNewState()->getPendingGrab());
    }

    public function testKdyzKostkaNeodtlacujeSeNepta(): void
    {
        // Attacker Down (1)
        $r = $this->zvolKostku($this->stav(), kostky: [1]);

        $this->assertNull($r->getNewState()->getPendingGrab());
    }

    public function testAISeNeptaAGrabPlatiAutomaticky(): void
    {
        // Neinteraktivní blok (tah AI): beze změny — otázka nevznikne, blok doběhne celý.
        $r = $this->resolver([3, 3, 3, 3], clovek: false)
            ->resolve($this->stav(), ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $this->assertNull($r->getNewState()->getPendingGrab());
        $this->assertNull($r->getNewState()->getPendingBlock());
        $this->assertNotSame([6, 7], $this->pole($r, 2), 'obránce odtlačen');
    }

    public function testBallAndChainSeKouceZeptaAAnoOdtlaciNaZvolenePole(): void
    {
        $b = fn(): GameState => (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 1, strength: 7, skills: [SkillName::BallAndChain, SkillName::NoHands, SkillName::Grab], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2)
            ->addPlayer(TeamSide::HOME, 7, 6, id: 3)
            ->addPlayer(TeamSide::HOME, 7, 7, id: 4)
            ->addPlayer(TeamSide::HOME, 7, 8, id: 5)
            ->withBallOffPitch()
            ->build();

        // D6 3 = rovně na (6,7); kostky bloku 3 = Pushed
        $q = $this->resolver(array_fill(0, 20, 3))->resolve($b(), ActionType::BALL_AND_CHAIN, ['playerId' => 1]);
        $this->assertNotNull($q->getNewState()->getPendingGrab(), 'B&C blok s Grabem ⇒ otázka');
        $this->assertSame([6, 7], $this->pole($q, 2));
        $this->assertSame([5, 7], $this->pole($q, 1), 'B&C zatím nepostoupil');
        $this->assertFalse($q->getNewState()->requirePlayer(1)->hasActed(), 'akce ještě neskončila');

        $r = $this->resolver(array_fill(0, 20, 3))->resolve($q->getNewState(), ActionType::RESOLVE_GRAB, ['use' => true, 'x' => 6, 'y' => 6]);
        $this->assertSame([6, 6], $this->pole($r, 2));
        $this->assertSame([6, 7], $this->pole($r, 1), 'povinný follow-up');
        $this->assertTrue($r->getNewState()->requirePlayer(1)->hasActed());
        $this->assertNull($r->getNewState()->getPendingGrab());

        $ne = $this->resolver(array_fill(0, 20, 3))->resolve($q->getNewState(), ActionType::RESOLVE_GRAB, ['use' => false]);
        $this->assertSame(7, $this->pole($ne, 2)[0], 'bez Grabu řetěz za obráncem');
    }

    public function testBallAndChainAISeNepta(): void
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 1, strength: 7, skills: [SkillName::BallAndChain, SkillName::NoHands, SkillName::Grab], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2)
            ->withBallOffPitch()
            ->build();
        $r = $this->resolver(array_fill(0, 20, 3), clovek: false)->resolve($s, ActionType::BALL_AND_CHAIN, ['playerId' => 1]);

        $this->assertNull($r->getNewState()->getPendingGrab());
    }
}
