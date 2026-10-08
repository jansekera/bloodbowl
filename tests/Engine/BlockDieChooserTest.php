<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\GameState;
use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Engine\RulesEngine;
use App\Enum\ActionType;
use App\Enum\PlayerState;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * KOSTKU BLOKU VYBÍRÁ TRENÉR SILNĚJŠÍHO HRÁČE (audit parity 08.10.2026, nález 5).
 *
 * `rules_bb2016.txt` ř. 628-634: "One Player Stronger -- Two Block Dice* ... * The coach of
 * the **stronger** player picks which block dice is used."
 *
 * Stará mechanika: v interaktivní hře (člověk na tahu) server přijal `faceIndex` od útočníka
 * i tehdy, když vybírá obránce -- člověk proti AI si u bloku 2 kostek "proti" vybral lepší
 * kostku sám.
 *
 * Fixtura: HOME id 1 (ST 3) blokuje AWAY id 2 (ST 4) ⇒ 2 kostky, vybírá obránce.
 * Kostky bloku: 6 = Defender Down (index 0), 1 = Attacker Down (index 1). Obě mechaniky se
 * liší: útočník chce index 0, obránce index 1.
 */
final class BlockDieChooserTest extends TestCase
{
    /** @return array{0: ActionResolver, 1: GameState} resolver a stav s čekajícím blokem */
    private function cekajiciBlok(?TeamSide $aiTeam, int $silaObrance = 4, int $rerolls = 0): array
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: $silaObrance, id: 2)
            ->build();
        $state = $state
            ->withAiTeam($aiTeam)
            ->withTeamState(TeamSide::HOME, $state->getTeamState(TeamSide::HOME)->withRerolls($rerolls));

        // 6, 1 = kostky bloku; 1, 1 = brnění toho, kdo padne (neprorazí AV8).
        $resolver = new ActionResolver(new FixedDiceRoller([6, 1, 1, 1]));
        $resolver->setInteractiveBlocks(true);
        $r = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $pending = $r->getNewState()->getPendingBlock();
        $this->assertNotNull($pending, 'fixtura: blok čeká na volbu kostky');
        $this->assertSame(['defender_down', 'attacker_down'], array_map(fn($f) => $f->value, $pending->getFaces()));

        return [$resolver, $r->getNewState()];
    }

    public function testVolbuUtocnikaServerNeprijmeKdyzVybiraObranceAi(): void
    {
        [, $state] = $this->cekajiciBlok(TeamSide::AWAY);
        $this->assertFalse($state->getPendingBlock()?->isAttackerChooses(), 'fixtura: vybírá obránce');

        $errors = (new RulesEngine())->validate($state, ActionType::CHOOSE_BLOCK_DIE, ['faceIndex' => 0]);

        $this->assertNotSame([], $errors, 'ř. 633-634: kostku vybírá trenér silnějšího hráče, ne útočník');
    }

    public function testObranceAiSiVybereKostkuSam(): void
    {
        [$resolver, $state] = $this->cekajiciBlok(TeamSide::AWAY);

        // Útočník jen potvrdí hod (bez faceIndex); kostku volí kouč obránce = Attacker Down.
        $this->assertSame([], (new RulesEngine())->validate($state, ActionType::CHOOSE_BLOCK_DIE, []));
        $r = $resolver->resolve($state, ActionType::CHOOSE_BLOCK_DIE, []);

        $this->assertTrue($r->isTurnover(), 'obránce zvolil Attacker Down');
        $this->assertSame(PlayerState::PRONE, $r->getNewState()->requirePlayer(1)->getState());
        $this->assertSame(PlayerState::STANDING, $r->getNewState()->requirePlayer(2)->getState());
    }

    public function testFaceIndexOdUtocnikaVolbuObranceAiNezmeni(): void
    {
        // I kdyby volba útočníka validaci obešla, resolver ji nepoužije.
        [$resolver, $state] = $this->cekajiciBlok(TeamSide::AWAY);

        $r = $resolver->resolve($state, ActionType::CHOOSE_BLOCK_DIE, ['faceIndex' => 0]);

        $this->assertSame(PlayerState::STANDING, $r->getNewState()->requirePlayer(2)->getState(), 'ř. 633-634: obránce si Defender Down nevybere');
        $this->assertSame(PlayerState::PRONE, $r->getNewState()->requirePlayer(1)->getState());
    }

    public function testUtocnikSmiPrehoditIKdyzVybiraObrance(): void
    {
        // Přehoz je věc trenéra, který hází (ř. 919-924) -- zákaz se týká jen VÝBĚRU kostky.
        [, $state] = $this->cekajiciBlok(TeamSide::AWAY, rerolls: 1);

        $this->assertSame([], (new RulesEngine())->validate($state, ActionType::REROLL_BLOCK, ['type' => 'team']));
    }

    public function testSilnejsiUtocnikClovekVybiraSam(): void
    {
        // Obránce ST 2 ⇒ 2 kostky, vybírá útočník (člověk): jeho faceIndex platí.
        [$resolver, $state] = $this->cekajiciBlok(TeamSide::AWAY, silaObrance: 2);
        $this->assertTrue($state->getPendingBlock()?->isAttackerChooses(), 'fixtura: vybírá útočník');

        $this->assertSame([], (new RulesEngine())->validate($state, ActionType::CHOOSE_BLOCK_DIE, ['faceIndex' => 0]));
        $this->assertNotSame([], (new RulesEngine())->validate($state, ActionType::CHOOSE_BLOCK_DIE, []), 'člověk musí kostku určit');
        $r = $resolver->resolve($state, ActionType::CHOOSE_BLOCK_DIE, ['faceIndex' => 0]);

        $this->assertSame(PlayerState::PRONE, $r->getNewState()->requirePlayer(2)->getState());
    }

    public function testHraBezAiObranceClovekVybiraVDialogu(): void
    {
        // Bez AI (hot-seat) sedí u dialogu i trenér obránce: volba kostky se přijme.
        [$resolver, $state] = $this->cekajiciBlok(null);

        $this->assertSame([], (new RulesEngine())->validate($state, ActionType::CHOOSE_BLOCK_DIE, ['faceIndex' => 1]));
        $r = $resolver->resolve($state, ActionType::CHOOSE_BLOCK_DIE, ['faceIndex' => 1]);

        $this->assertSame(PlayerState::PRONE, $r->getNewState()->requirePlayer(1)->getState());
    }
}
