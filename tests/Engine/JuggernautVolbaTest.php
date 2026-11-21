<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\ActionResult;
use App\DTO\GameState;
use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Engine\RulesEngine;
use App\Enum\ActionType;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * P309 (uživatel 20.11.2026: „ano“) — Juggernaut převádí Both Down na Pushed jen VOLBOU.
 *
 * `rules_bb2016.txt` ř. 8191-8195 (Juggernaut): "If this player takes a Blitz Action, then opposing players may not
 * use their Fend, Stand Firm or Wrestle skills against blocks, and he **may choose** to treat a 'Both Down' result as
 * if a 'Pushed' result has been rolled instead." Dřív se převádělo vždy. AI: nepřevede, má-li útočník Block a obránce
 * ne (Both Down srazí jen obránce; jeho Wrestle Juggernaut ruší), jinak převede. Kouče-člověka se zeptá
 * (`ActionType::RESOLVE_JUGGERNAUT`, přehrání akce jako u Diving Catch). Parita s C++ `aiJuggernautFace` /
 * `CoachQuestion::USE_JUGGERNAUT`.
 *
 * Kostka bloku: 1 lebka · 2 Both Down · 3-4 Pushed · 5 Defender Stumbles · 6 Defender Down.
 */
final class JuggernautVolbaTest extends TestCase
{
    /**
     * Útočník HOME (5,5) s Juggernautem → obránce AWAY (6,5).
     *
     * @param list<SkillName> $utocnik
     * @param list<SkillName> $obrance
     */
    private function stav(array $utocnik = [SkillName::Juggernaut, SkillName::Block], array $obrance = [], int $silaUtocnika = 3): GameState
    {
        return (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: $silaUtocnika, skills: $utocnik, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, skills: $obrance, id: 2)
            ->withBallOffPitch()
            ->build();
    }

    /** @param list<int> $kostky */
    private function blitzAI(GameState $s, array $kostky): ActionResult
    {
        return (new ActionResolver(new FixedDiceRoller($kostky)))->resolve($s, ActionType::BLITZ, ['playerId' => 1, 'targetId' => 2]);
    }

    /** @return list<string> */
    private function typy(ActionResult $r): array
    {
        return array_map(static fn($e): string => $e->getType(), $r->getEvents());
    }

    /** @param list<int> $kostky */
    private function clovek(array $kostky): ActionResolver
    {
        $r = new ActionResolver(new FixedDiceRoller($kostky));
        $r->setInteractiveBlocks(true);
        $r->setHumanSides([TeamSide::HOME]);

        return $r;
    }

    /** Kouč-člověk zablitzuje, padne Both Down a zvolí ho. */
    private function clovekZvoliBothDown(GameState $s): ActionResult
    {
        $hod = $this->clovek([2])->resolve($s, ActionType::BLITZ, ['playerId' => 1, 'targetId' => 2]);
        $this->assertNotNull($hod->getNewState()->getPendingBlock(), 'fixtura: čeká volba kostky');

        return $this->clovek([1, 1, 1, 1])->resolve($hod->getNewState(), ActionType::CHOOSE_BLOCK_DIE, ['faceIndex' => 0]);
    }

    public function testAiSBlockemNechaBothDownProtiObranciBezBlocku(): void
    {
        $r = $this->blitzAI($this->stav(), [2, 1, 1]);   // Both Down, zbroj obránce 1+1
        $s = $r->getNewState();

        $this->assertFalse($r->isTurnover());
        $this->assertSame(PlayerState::STANDING, $s->requirePlayer(1)->getState(), 'Block útočníka drží');
        $this->assertSame(PlayerState::PRONE, $s->requirePlayer(2)->getState(), 'obránce sražen');
        $this->assertSame(6, $s->requirePlayer(2)->requirePosition()->getX(), 'na svém poli, bez odtlačení');
        $this->assertNotContains('juggernaut', $this->typy($r));
    }

    public function testAiProtiObranciSBlockemDalPrevede(): void
    {
        $r = $this->blitzAI($this->stav(obrance: [SkillName::Block]), [2]);
        $s = $r->getNewState();

        $this->assertSame(PlayerState::STANDING, $s->requirePlayer(2)->getState());
        $this->assertSame(7, $s->requirePlayer(2)->requirePosition()->getX(), 'odtlačen');
        $this->assertContains('juggernaut', $this->typy($r));
    }

    /** Ponechaný Both Down nesmí oživit Wrestle obránce — Juggernaut ho v blitzu ruší (ř. 8192-8194). */
    public function testPonechanyBothDownObranceNesmiWrestle(): void
    {
        $r = $this->blitzAI($this->stav(obrance: [SkillName::Wrestle]), [2, 1, 1]);
        $s = $r->getNewState();

        $this->assertFalse($r->isTurnover());
        $this->assertSame(PlayerState::STANDING, $s->requirePlayer(1)->getState(), 'Wrestle by položil i útočníka');
        $this->assertSame(PlayerState::PRONE, $s->requirePlayer(2)->getState());
        $this->assertNotContains('wrestle', $this->typy($r));
    }

    /** Výběr kostky oceňuje Both Down tak, jak se vyhodnotí: proti obránci s Wrestle srazí jen obránce ⇒ lepší než Pushed. */
    public function testVyberKostkyDaPrednostPonechanemuBothDown(): void
    {
        $r = $this->blitzAI($this->stav(obrance: [SkillName::Wrestle], silaUtocnika: 4), [3, 2, 1, 1]);   // Pushed, Both Down
        $s = $r->getNewState();

        $this->assertSame(PlayerState::PRONE, $s->requirePlayer(2)->getState());
        $this->assertSame(6, $s->requirePlayer(2)->requirePosition()->getX());
        $this->assertSame(PlayerState::STANDING, $s->requirePlayer(1)->getState());
    }

    public function testClovekDostaneOtazkuANicSeZatimNestane(): void
    {
        $s = $this->clovekZvoliBothDown($this->stav([SkillName::Juggernaut]))->getNewState();

        $otazka = $s->getPendingJuggernaut();
        $this->assertNotNull($otazka, 'Both Down v blitzu s Juggernautem + kouč-člověk ⇒ otázka');
        $this->assertSame(1, $otazka->getPlayerId());
        $this->assertSame(2, $otazka->getDefenderId());
        $this->assertSame(PlayerState::STANDING, $s->requirePlayer(1)->getState());
        $this->assertSame(PlayerState::STANDING, $s->requirePlayer(2)->getState());
        $this->assertSame([ActionType::RESOLVE_JUGGERNAUT->value], array_column((new RulesEngine())->getAvailableActions($s), 'type'));
        $this->assertNotSame([], (new RulesEngine())->validate($s, ActionType::END_TURN, []), 'bez odpovědi nic jiného');
    }

    public function testClovekNeBothDownPlatiAUtocnikBezBlockuPada(): void
    {
        $q = $this->clovekZvoliBothDown($this->stav([SkillName::Juggernaut]))->getNewState();
        $r = $this->clovek([1, 1, 1, 1])->resolve($q, ActionType::RESOLVE_JUGGERNAUT, ['use' => false]);
        $s = $r->getNewState();

        $this->assertNull($s->getPendingJuggernaut());
        $this->assertTrue($r->isTurnover(), 'útočník bez Blocku padl');
        $this->assertSame(PlayerState::PRONE, $s->requirePlayer(1)->getState());
        $this->assertSame(PlayerState::PRONE, $s->requirePlayer(2)->getState());
    }

    public function testClovekAnoObranceOdtlacen(): void
    {
        $q = $this->clovekZvoliBothDown($this->stav())->getNewState();   // s Blockem by AI nepřevedla
        $r = $this->clovek([1, 1, 1, 1])->resolve($q, ActionType::RESOLVE_JUGGERNAUT, ['use' => true]);
        $s = $r->getNewState();

        $this->assertNull($s->getPendingJuggernaut());
        $this->assertFalse($r->isTurnover());
        $this->assertSame(PlayerState::STANDING, $s->requirePlayer(2)->getState());
        $this->assertSame(7, $s->requirePlayer(2)->requirePosition()->getX(), 'odtlačen');
        $this->assertContains('juggernaut', $this->typy($r));
    }

    public function testBezBlitzuSeNeptaNikoho(): void
    {
        $hod = $this->clovek([2])->resolve($this->stav([SkillName::Juggernaut]), ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);
        $s = $this->clovek([1, 1, 1, 1])->resolve($hod->getNewState(), ActionType::CHOOSE_BLOCK_DIE, ['faceIndex' => 0])->getNewState();

        $this->assertNull($s->getPendingJuggernaut());
        $this->assertSame(PlayerState::PRONE, $s->requirePlayer(1)->getState(), 'obyčejný blok: Both Down platí');
    }
}
