<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\ActionResult;
use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Enum\ActionType;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * P275 (audit goblinů 10.11.2026): Stunty „must subtract 1 from the roll when they pass“ (rules_bb2016.txt
 * ř. 8533-8534); bomba se hází „using the rules for throwing the ball“ (ř. 7952-7953). Dřív se Stunty při hodu
 * nečetl. Parita s C++ helpers.cpp throwRollMods.
 */
final class StuntyPassTest extends TestCase
{
    /** @return array<string, mixed>|null */
    private function udalost(ActionResult $r, string $typ): ?array
    {
        foreach ($r->getEvents() as $e) {
            if ($e->getType() === $typ) {
                return $e->getData();
            }
        }
        return null;
    }

    /** @param list<int> $kostky */
    private function prihraj(bool $stunty, int $cilX, array $kostky): ActionResult
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1, skills: $stunty ? [SkillName::Stunty] : [])
            ->addPlayer(TeamSide::HOME, $cilX, 5, agility: 3, id: 2)
            ->withHomeTeam(\App\DTO\TeamStateDTO::create(1, 'Home', 'Goblin', TeamSide::HOME, 0))
            ->withBallCarried(1)
            ->build();

        return (new ActionResolver(new FixedDiceRoller($kostky)))->resolve($s, ActionType::PASS, [
            'playerId' => 1, 'targetX' => $cilX, 'targetY' => 5,
        ]);
    }

    public function testRychlaPrihravkaStuntyPotrebujeOJednuVic(): void
    {
        // rychlá (5,5)→(8,5): bez Stunty 7-3-1 = 3+ ⇒ hod 3 přesná; se Stunty 4+ ⇒ hod 3 nepřesná
        $bez = $this->udalost($this->prihraj(false, 8, [3, 6, 6, 6, 6, 6]), 'pass');
        $se = $this->udalost($this->prihraj(true, 8, [3, 2, 2, 2, 6, 6, 6]), 'pass');
        $this->assertSame([3, 'accurate'], [$bez['target'] ?? null, $bez['result'] ?? null]);
        $this->assertSame([4, 'inaccurate'], [$se['target'] ?? null, $se['result'] ?? null]);
    }

    public function testStuntyFumblujeKratkouNaDvojku(): void
    {
        // krátká (5,5)→(10,5): modifikátor 0 − 1 = −1 ⇒ hod 2 = 1 po úpravě = fumble (ř. 1742-1744)
        $se = $this->udalost($this->prihraj(true, 10, [2, 3, 6, 6, 6]), 'pass');
        $this->assertSame('fumble', $se['result'] ?? null);
    }

    public function testBombaStuntyBombardieraTakeSMinusJednou(): void
    {
        // rychlá bomba (5,7)→(8,7): bez Stunty 3+ ⇒ hod 3 přesně; se Stunty 4+ ⇒ hod 3 nepřesně
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, skills: [SkillName::Bombardier, SkillName::Stunty])
            ->addPlayer(TeamSide::AWAY, 8, 7, id: 12)
            ->withBallOffPitch()
            ->build();
        $r = (new ActionResolver(new FixedDiceRoller([3, 2, 2, 2, 6, 6, 6, 6])))->resolve($s, ActionType::BOMB_THROW, [
            'playerId' => 1, 'targetX' => 8, 'targetY' => 7,
        ]);
        $this->assertSame('inaccurate', $this->udalost($r, 'bomb_throw')['result'] ?? null);
    }
}
