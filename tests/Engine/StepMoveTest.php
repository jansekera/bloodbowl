<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\GameState;
use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Engine\RulesEngine;
use App\Enum\ActionType;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * P296 (11.11.2026): kouč-člověk táhne hráčem po krocích a rozhoduje se uprostřed (např. podle výsledku úhybu).
 * Dílčí krok (`partial`) nechá pohyb otevřený — další MOVE téhož hráče je TÁŽ akce, takže se znovu nehází
 * kontrola před akcí (Bone Head ř. 7980-7986). Aktivace se uzavře, jakmile jedná jiný hráč nebo skončí kolo:
 * „a player must finish his Action before another player can take one" (`rules_bb2016.txt` ř. 423-429).
 * Po pohybu už hráč neohlásí Blitz (ř. 420-422: akce se ohlašuje napřed).
 */
final class StepMoveTest extends TestCase
{
    /**
     * Hráč 1 MA6 v (5,5), spoluhráč 3 daleko, soupeř 2 v (6,6) — z (5,5) se uhýbá.
     *
     * @param list<SkillName> $skills
     */
    private function state(array $skills = []): GameState
    {
        return (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, skills: $skills, id: 1)
            ->addPlayer(TeamSide::HOME, 1, 1, id: 3)
            ->addPlayer(TeamSide::AWAY, 6, 6, id: 2)
            ->withBallOffPitch()
            ->build();
    }

    public function testDilciKrokNechaPohybOtevreny(): void
    {
        $resolver = new ActionResolver(new FixedDiceRoller([6]));   // úhyb z (5,5)
        $s = $resolver->resolve($this->state(), ActionType::MOVE, ['playerId' => 1, 'x' => 4, 'y' => 5, 'partial' => true])->getNewState();

        $p = $s->requirePlayer(1);
        $this->assertSame(4, $p->requirePosition()->getX());
        $this->assertFalse($p->hasMoved(), 'pohyb není ukončen');
        $this->assertSame(5, $p->getMovementRemaining());
        $this->assertSame(1, $s->getMoveContinuationPlayerId());
        $this->assertSame([], (new RulesEngine())->validate($s, ActionType::MOVE, ['playerId' => 1, 'x' => 1, 'y' => 5]));

        $s = $resolver->resolve($s, ActionType::MOVE, ['playerId' => 1, 'x' => 1, 'y' => 5])->getNewState();
        $this->assertSame(1, $s->requirePlayer(1)->requirePosition()->getX());
        $this->assertSame(2, $s->requirePlayer(1)->getMovementRemaining());
        $this->assertTrue($s->requirePlayer(1)->hasMoved(), 'celá zbylá cesta pohyb ukončí');
        $this->assertNull($s->getMoveContinuationPlayerId());
    }

    public function testPokracovaniNehaziBoneHeadZnovu(): void
    {
        // jediný hod Bone Head (4) na začátku akce; úhyb 6; druhý krok bez úhybu — další kostka by výjimkou spadla
        $resolver = new ActionResolver(new FixedDiceRoller([4, 6]));
        $s = $resolver->resolve($this->state([SkillName::BoneHead]), ActionType::MOVE, ['playerId' => 1, 'x' => 4, 'y' => 5, 'partial' => true])->getNewState();
        $s = $resolver->resolve($s, ActionType::MOVE, ['playerId' => 1, 'x' => 3, 'y' => 5, 'partial' => true])->getNewState();

        $this->assertSame(3, $s->requirePlayer(1)->requirePosition()->getX());
        $this->assertSame(1, $s->getMoveContinuationPlayerId(), 'i po druhém dílčím kroku pohyb trvá');
    }

    public function testAkceJinehoHraceUzavrePohyb(): void
    {
        $resolver = new ActionResolver(new FixedDiceRoller([6]));
        $s = $resolver->resolve($this->state(), ActionType::MOVE, ['playerId' => 1, 'x' => 4, 'y' => 5, 'partial' => true])->getNewState();
        $s = $resolver->resolve($s, ActionType::MOVE, ['playerId' => 3, 'x' => 2, 'y' => 1])->getNewState();

        $p = $s->requirePlayer(1);
        $this->assertTrue($p->hasMoved());
        $this->assertTrue($p->hasActed(), 'po jiné aktivaci už nepřihraje ani nepředá');
        $this->assertNull($s->getMoveContinuationPlayerId());
        $this->assertNotSame([], (new RulesEngine())->validate($s, ActionType::MOVE, ['playerId' => 1, 'x' => 3, 'y' => 5]));
    }

    public function testPoDilcimKrokuNelzeOhlasitBlitz(): void
    {
        $s = (new ActionResolver(new FixedDiceRoller([6])))
            ->resolve($this->state(), ActionType::MOVE, ['playerId' => 1, 'x' => 4, 'y' => 5, 'partial' => true])->getNewState();

        $this->assertNotSame([], (new RulesEngine())->validate($s, ActionType::BLITZ, ['playerId' => 1]));
        $this->assertNotSame([], (new RulesEngine())->validate($s, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]));
    }

    public function testPrehozenyUhybNechaPohybOtevrenyBezNovehoBoneHead(): void
    {
        // Bone Head 4, úhyb 1 (nabídka přehozu), přehoz 6; pak krok bez úhybu — další kostka by výjimkou spadla
        $resolver = new ActionResolver(new FixedDiceRoller([4, 1, 6]));
        $resolver->setInteractiveRerolls(true);
        $s = $resolver->resolve($this->state([SkillName::BoneHead]), ActionType::MOVE, ['playerId' => 1, 'x' => 4, 'y' => 5])->getNewState();
        $this->assertNotNull($s->getPendingReroll());
        $s = $resolver->resolve($s, ActionType::RESOLVE_REROLL, ['choice' => 'team_reroll'])->getNewState();
        $this->assertSame(1, $s->getMoveContinuationPlayerId());

        $s = $resolver->resolve($s, ActionType::MOVE, ['playerId' => 1, 'x' => 3, 'y' => 5])->getNewState();
        $this->assertSame(3, $s->requirePlayer(1)->requirePosition()->getX());
    }

    public function testKonecKolaUzavrePohyb(): void
    {
        $resolver = new ActionResolver(new FixedDiceRoller([6]));
        $s = $resolver->resolve($this->state(), ActionType::MOVE, ['playerId' => 1, 'x' => 4, 'y' => 5, 'partial' => true])->getNewState();
        $s = $resolver->resolve($s, ActionType::END_TURN, [])->getNewState();

        $this->assertNull($s->getMoveContinuationPlayerId());
    }
}
