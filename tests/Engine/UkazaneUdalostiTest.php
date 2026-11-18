<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Enum\ActionType;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * Review P294 (17.11.2026, nález 1): dvě otázky v jedné akci. Kouč u každé otázky i po odpovědi vidí každou událost
 * právě jednou — co už viděl (`ReplayContext::$shown`), se sčítá přes všechny otázky akce, ne přepisuje.
 */
final class UkazaneUdalostiTest extends TestCase
{
    /** @param list<int> $kostky */
    private function kouc(array $kostky): ActionResolver
    {
        $r = new ActionResolver(new FixedDiceRoller($kostky));
        $r->setHumanSides([TeamSide::HOME]);

        return $r;
    }

    public function testBoneHeadAPakUhybVTemzePohybuKazdaUdalostJednou(): void
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 5, agility: 3, skills: [SkillName::BoneHead, SkillName::Pro], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 4, id: 2)
            ->withBallOffPitch()
            ->build();
        $vse = [];
        // Bone Head 1 ⇒ otázka 1
        $r = $this->kouc([1])->resolve($s, ActionType::MOVE, ['playerId' => 1, 'x' => 5, 'y' => 6]);
        $this->assertNotNull($r->getNewState()->getPendingReroll());
        $vse = [...$vse, ...$r->getEvents()];
        // týmový přehoz 4 projde; úhyb z (5,5) na (5,6) 1 ⇒ otázka 2 (Pro)
        $r = $this->kouc([4, 1])->resolve($r->getNewState(), ActionType::RESOLVE_REROLL, ['choice' => 'team_reroll']);
        $this->assertSame('dodge', $r->getNewState()->getPendingReroll()?->getRollType()?->value);
        $vse = [...$vse, ...$r->getEvents()];
        // Pro 4 projde, úhyb 6
        $r = $this->kouc([4, 6])->resolve($r->getNewState(), ActionType::RESOLVE_REROLL, ['choice' => 'pro']);
        $this->assertNull($r->getNewState()->getPendingReroll());
        $vse = [...$vse, ...$r->getEvents()];

        $typy = array_map(static fn($e) => $e->getType(), $vse);
        $this->assertSame(2, count(array_keys($typy, 'bone_head', true)), 'neúspěch + povedený přehoz, nic dvakrát');
        $this->assertSame(2, count(array_keys($typy, 'dodge', true)), 'nepovedený úhyb + přehoz Pro');
        $this->assertSame(['bone_head', 'reroll', 'bone_head', 'dodge'], array_slice($typy, 0, 4), 'pořadí: kouč vidí průběh, jak šel');
    }
}
