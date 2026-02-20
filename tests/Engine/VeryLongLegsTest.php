<?php
declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Enum\ActionType;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

final class VeryLongLegsTest extends TestCase
{
    public function testVeryLongLegsReducesLeapTarget(): void
    {
        // Surround player so only leap can reach destination
        // Player at 5,5 surrounded by enemies on all adjacent squares except where they came from
        // Force leap by blocking all normal paths to (7,5)
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, skills: [SkillName::Leap, SkillName::VeryLongLegs], id: 1)
            ->addPlayer(TeamSide::AWAY, 4, 4, id: 10) // block NW
            ->addPlayer(TeamSide::AWAY, 5, 4, id: 11) // block N
            ->addPlayer(TeamSide::AWAY, 6, 4, id: 12) // block NE
            ->addPlayer(TeamSide::AWAY, 4, 5, id: 13) // block W
            ->addPlayer(TeamSide::AWAY, 6, 5, id: 14) // block E
            ->addPlayer(TeamSide::AWAY, 4, 6, id: 15) // block SW
            ->addPlayer(TeamSide::AWAY, 5, 6, id: 16) // block S
            ->addPlayer(TeamSide::AWAY, 6, 6, id: 17) // block SE
            ->withBallOffPitch()
            ->build();

        // Leap to (7,7) — 2 squares SE
        // Only enemy at (6,6) is adjacent to (7,7) → TZ=1 at landing
        // leapTarget = max(2, 7-3+1) = 5+, with VLL → max(2, 5-1) = 4+
        // Roll = 4 → success with VLL
        $dice = new FixedDiceRoller([4]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::MOVE, ['playerId' => 1, 'x' => 7, 'y' => 7]);

        $this->assertFalse($result->isTurnover());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('leap', $types);

        $player = $result->getNewState()->getPlayer(1);
        $this->assertNotNull($player->getPosition());
        $this->assertEquals(7, $player->getPosition()->getX());
        $this->assertEquals(7, $player->getPosition()->getY());
    }

    public function testLeapWithoutVeryLongLegsHigherTarget(): void
    {
        // Same setup but without VLL
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, skills: [SkillName::Leap], id: 1)
            ->addPlayer(TeamSide::AWAY, 4, 4, id: 10)
            ->addPlayer(TeamSide::AWAY, 5, 4, id: 11)
            ->addPlayer(TeamSide::AWAY, 6, 4, id: 12)
            ->addPlayer(TeamSide::AWAY, 4, 5, id: 13)
            ->addPlayer(TeamSide::AWAY, 6, 5, id: 14)
            ->addPlayer(TeamSide::AWAY, 4, 6, id: 15)
            ->addPlayer(TeamSide::AWAY, 5, 6, id: 16)
            ->addPlayer(TeamSide::AWAY, 6, 6, id: 17)
            ->withBallOffPitch()
            ->build();

        // Leap to (7,7) — TZ=1 from enemy at (6,6)
        // Without VLL: leapTarget = max(2, 7-3+1) = 5+
        // Roll = 4 → fails (4 < 5)
        $dice = new FixedDiceRoller([4]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::MOVE, ['playerId' => 1, 'x' => 7, 'y' => 7]);

        $this->assertTrue($result->isTurnover());
    }

    public function testVeryLongLegsReducesInterceptionTarget(): void
    {
        // Interceptor with VLL and AG4: base target = 7-4+2 = 5+, VLL reduces to 4+
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 3, 5, agility: 3, skills: [SkillName::SureHands], id: 1)
            ->addPlayer(TeamSide::AWAY, 5, 5, agility: 4, skills: [SkillName::VeryLongLegs], id: 2)
            ->addPlayer(TeamSide::HOME, 8, 5, id: 3)
            ->withBallCarried(1)
            ->build();

        // Interception roll: 4 → with VLL target = 4+ → success → intercepted
        $dice = new FixedDiceRoller([4]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::PASS, ['playerId' => 1, 'targetX' => 8, 'targetY' => 5]);

        $this->assertTrue($result->isTurnover());
        foreach ($result->getEvents() as $event) {
            if ($event->getType() === 'interception') {
                $this->assertTrue($event->getData()['success']);
            }
        }
    }

    public function testInterceptionWithoutVeryLongLegsHigherTarget(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 3, 5, agility: 3, skills: [SkillName::SureHands], id: 1)
            ->addPlayer(TeamSide::AWAY, 5, 5, agility: 4, id: 2)
            ->addPlayer(TeamSide::HOME, 8, 5, id: 3)
            ->withBallCarried(1)
            ->build();

        // Interception roll: 4 → target 5+ without VLL → fails
        // Pass accuracy: 5 → accurate
        // Catch: 5 → success
        $dice = new FixedDiceRoller([4, 5, 5]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::PASS, ['playerId' => 1, 'targetX' => 8, 'targetY' => 5]);

        foreach ($result->getEvents() as $event) {
            if ($event->getType() === 'interception') {
                $this->assertFalse($event->getData()['success']);
                break;
            }
        }
    }

    public function testVeryLongLegsLeapNoTZClampedAt2(): void
    {
        // AG5 + VLL + TZ=1 at (7,7) from enemy at (6,6)
        // leapTarget = max(2, min(6, 7-5+1)) = max(2, 3) = 3
        // With VLL: max(2, 3-1) = max(2, 2) = 2+
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 5, skills: [SkillName::Leap, SkillName::VeryLongLegs], id: 1)
            ->addPlayer(TeamSide::AWAY, 4, 4, id: 10)
            ->addPlayer(TeamSide::AWAY, 5, 4, id: 11)
            ->addPlayer(TeamSide::AWAY, 6, 4, id: 12)
            ->addPlayer(TeamSide::AWAY, 4, 5, id: 13)
            ->addPlayer(TeamSide::AWAY, 6, 5, id: 14)
            ->addPlayer(TeamSide::AWAY, 4, 6, id: 15)
            ->addPlayer(TeamSide::AWAY, 5, 6, id: 16)
            ->addPlayer(TeamSide::AWAY, 6, 6, id: 17)
            ->withBallOffPitch()
            ->build();

        // Target = 2+ (clamped), roll = 2 → success
        $dice = new FixedDiceRoller([2]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::MOVE, ['playerId' => 1, 'x' => 7, 'y' => 7]);

        $this->assertFalse($result->isTurnover());
    }
}
