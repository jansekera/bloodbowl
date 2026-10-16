<?php

declare(strict_types=1);

namespace App\Tests\Engine;

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
 * P210 (1) / P156 v PHP (uživatel 16.10.2026: „ten blitz oprav, ať jde pohyb kamkoliv — co AI vybere, je
 * něco jiného“). `rules_bb2016.txt` ř. 346-349: „Blitz: The player may move a number of squares equal to
 * their MA. He MAY make one block during the move. The block may be made at any point during the move“;
 * ř. 548-552 totéž + „may carry on moving after the effects of the block“. Cíl se při ohlášení neříká.
 * Wild Animal ř. 8666-8668: „immediately after declaring an Action … adding 2 … if taking a Block or Blitz
 * Action“ ⇒ ohlášení blitzu bez cíle je hod na 2+.
 * Dřív: BLITZ chtěl cíl a pohyb vedl k němu; web pouštěl MOVE a potom BLITZ (hráč ohlásil pohyb, ne blitz).
 */
final class BlitzDeclareFreeMoveTest extends TestCase
{
    /** @param list<SkillName> $skills */
    private function state(array $skills = []): GameState
    {
        return (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, skills: $skills, id: 1)
            ->addPlayer(TeamSide::HOME, 2, 2, id: 3)
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->withBallOffPitch()
            ->build();
    }

    /**
     * @param array<string, mixed> $params
     * @param list<int> $dice
     */
    private function act(GameState $s, ActionType $t, array $params, array $dice = []): GameState
    {
        $r = (new ActionResolver(new FixedDiceRoller($dice)))->resolve($s, $t, $params);
        $this->assertFalse($r->isTurnover());
        return $r->getNewState();
    }

    /** @return list<string> */
    private function offered(GameState $s, int $playerId): array
    {
        return array_values(array_map(fn($a) => $a['type'], array_filter(
            (new RulesEngine())->getAvailableActions($s),
            fn($a) => ($a['playerId'] ?? null) === $playerId,
        )));
    }

    public function testBlitzCanBeDeclaredWithoutATargetAndUsesTheTeamBlitz(): void
    {
        $s = $this->act($this->state(), ActionType::BLITZ, ['playerId' => 1]);
        $this->assertTrue($s->getTeamState(TeamSide::HOME)->isBlitzUsedThisTurn(), 'ohlášení spotřebuje týmový blitz');
        $this->assertContains(ActionType::MOVE->value, $this->offered($s, 1), 'po ohlášení smí běžet');
        $this->assertNotContains(ActionType::BLITZ->value, $this->offered($s, 3), 'druhý blitz v tahu ne');
    }

    public function testTheDeclaredBlitzerMovesAnywhere(): void
    {
        $s = $this->act($this->state(), ActionType::BLITZ, ['playerId' => 1]);
        $s = $this->act($s, ActionType::MOVE, ['playerId' => 1, 'x' => 5, 'y' => 2]);   // směrem OD soupeře
        $p = $s->requirePlayer(1)->requirePosition();
        $this->assertSame([5, 2], [$p->getX(), $p->getY()]);
    }

    public function testTheDeclaredBlitzerBlocksOnceAfterMovingAndPaysASquare(): void
    {
        $st = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1)
            ->addPlayer(TeamSide::AWAY, 9, 7, id: 2)
            ->withBallOffPitch()
            ->build();
        $s = $this->act($st, ActionType::BLITZ, ['playerId' => 1]);
        $s = $this->act($s, ActionType::MOVE, ['playerId' => 1, 'x' => 8, 'y' => 7]);   // 3 pole, zbývají 3
        $this->assertContains(ActionType::BLITZ->value, $this->offered($s, 1), 'rána blitzu po pohybu');
        $s = $this->act($s, ActionType::BLITZ, ['playerId' => 1, 'targetId' => 2], [6, 1, 1, 1, 1]);   // POW
        $this->assertSame(PlayerState::PRONE, $s->requirePlayer(2)->getState());
        $this->assertNotContains(ActionType::BLITZ->value, $this->offered($s, 1), 'jen jedna rána');
        $this->assertNotContains(ActionType::BLOCK->value, $this->offered($s, 1));
    }

    public function testWildAnimalDeclaringABlitzRollsTwoPlus(): void
    {
        // hod 2: s +2 za Blitz = 4 ⇒ projde (Move by na 2 propadl)
        $s = $this->act($this->state([SkillName::WildAnimal]), ActionType::BLITZ, ['playerId' => 1], [2]);
        $this->assertContains(ActionType::MOVE->value, $this->offered($s, 1), 'Wild Animal po ohlášení blitzu běží');
        // a pohyb už nehází znovu (táž akce) — prázdné kostky by jinak spadly
        $s = $this->act($s, ActionType::MOVE, ['playerId' => 1, 'x' => 5, 'y' => 3]);
        $this->assertSame(3, $s->requirePlayer(1)->requirePosition()->getY());
    }

    public function testAMoveActionCannotTurnIntoABlitzLater(): void
    {
        $st = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1)
            ->addPlayer(TeamSide::AWAY, 9, 7, id: 2)
            ->withBallOffPitch()
            ->build();
        $s = $this->act($st, ActionType::MOVE, ['playerId' => 1, 'x' => 8, 'y' => 7]);   // ohlásil POHYB
        $this->assertNotContains(ActionType::BLITZ->value, $this->offered($s, 1), 'z pohybu se blitz nestane');
        $errors = (new RulesEngine())->validate($s, ActionType::BLITZ, ['playerId' => 1, 'targetId' => 2]);
        $this->assertNotEmpty($errors);
    }

    public function testTheDeclarationLapsesWhenAnotherPlayerActs(): void
    {
        $s = $this->act($this->state(), ActionType::BLITZ, ['playerId' => 1]);
        $s = $this->act($s, ActionType::MOVE, ['playerId' => 3, 'x' => 2, 'y' => 3]);
        $this->assertEmpty($this->offered($s, 1), 'aktivace blitzujícího skončila, když jednal jiný');
    }
}
