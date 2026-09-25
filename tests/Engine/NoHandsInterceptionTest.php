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
 * `rules_bb2016.txt` r. 8318-8320 (No Hands): "The player is unable to pick up,
 * **intercept** or carry the ball". Zvedani a chytani engine resil, zachyceni ne.
 */
final class NoHandsInterceptionTest extends TestCase
{
    /**
     * @param list<SkillName> $skillyZachytavace
     * @param list<int> $kostky
     * @return list<string>
     */
    private function prihravka(array $skillyZachytavace, array $kostky): array
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 10, 5, agility: 3, id: 2)
            ->addPlayer(TeamSide::AWAY, 7, 5, agility: 3, skills: $skillyZachytavace, id: 3)
            ->withBallCarried(1)
            ->build();

        $r = (new ActionResolver(new FixedDiceRoller($kostky)))->resolve($s, ActionType::PASS, [
            'playerId' => 1, 'targetX' => 10, 'targetY' => 5,
        ]);

        return array_map(static fn($e) => $e->getType(), $r->getEvents());
    }

    public function testNoHandsPlayerDoesNotIntercept(): void
    {
        // presnost 6, chyt 6 -- zadny hod na zachyceni
        $this->assertNotContains('interception', $this->prihravka([SkillName::NoHands], [6, 6]));
    }

    /** Pozitivni kontrola: tentyz hrac BEZ No Hands se o zachyceni pokusi. */
    public function testSamePlayerWithoutNoHandsTriesToIntercept(): void
    {
        // zachyceni 1 (nevyjde), presnost 6, chyt 6
        $this->assertContains('interception', $this->prihravka([], [1, 6, 6]));
    }
}
