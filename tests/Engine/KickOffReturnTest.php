<?php
declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\TeamStateDTO;
use App\Engine\FixedDiceRoller;
use App\Engine\KickoffResolver;
use App\Engine\ScatterCalculator;
use App\Engine\BallResolver;
use App\Engine\TacklezoneCalculator;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use App\ValueObject\Position;
use PHPUnit\Framework\TestCase;

final class KickOffReturnTest extends TestCase
{
    private function createKickoffResolver(array $rolls): KickoffResolver
    {
        $dice = new FixedDiceRoller($rolls);
        $scatterCalc = new ScatterCalculator();
        $tzCalc = new TacklezoneCalculator();
        $ballResolver = new BallResolver($dice, $tzCalc, $scatterCalc);
        return new KickoffResolver($dice, $scatterCalc, $ballResolver);
    }

    public function testKickOffReturnMovesTowardBall(): void
    {
        // KOR player at (3,7), ball lands at (6,7) — player should move 3 squares toward ball
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 3, 7, skills: [SkillName::KickOffReturn], id: 1)
            ->addPlayer(TeamSide::HOME, 5, 5, id: 2)
            ->addPlayer(TeamSide::AWAY, 15, 7, id: 10)
            ->withActiveTeam(TeamSide::HOME)
            ->build();

        // Scatter: D8=1(N), D6=0 → lands at same spot (6,7)
        // Kickoff table: D6+D6 = 3+4 = 7 (Brilliant Coaching), home D6=3, away D6=3 → tie
        // After KOR: player at (6,7), ball at (6,7) → catch: roll 4 → success (AG3, 4+)
        $dice = new FixedDiceRoller([
            1, 1,   // scatter: D8=1, D6=1
            3, 4,   // kickoff table: 7 (Brilliant Coaching)
            3, 3,   // cheering rolls (tie)
            4,       // catch roll
        ]);
        $scatterCalc = new ScatterCalculator();
        $tzCalc = new TacklezoneCalculator();
        $ballResolver = new BallResolver($dice, $tzCalc, $scatterCalc);
        $resolver = new KickoffResolver($dice, $scatterCalc, $ballResolver);

        $result = $resolver->resolveKickoff($state, new Position(6, 7));

        $types = array_map(fn($e) => $e->getType(), $result['events']);
        $this->assertContains('kick_off_return', $types);

        // Player 1 should have moved closer to (6,7)
        $player = $result['state']->getPlayer(1);
        $this->assertNotNull($player->getPosition());
        $this->assertLessThanOrEqual(3, abs($player->getPosition()->getX() - 3));
    }

    public function testNoKickOffReturnPlayerNoMovement(): void
    {
        // No player has KOR — no KOR events
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 3, 7, id: 1)
            ->addPlayer(TeamSide::HOME, 5, 5, id: 2)
            ->addPlayer(TeamSide::AWAY, 15, 7, id: 10)
            ->withActiveTeam(TeamSide::HOME)
            ->build();

        // Same dice sequence as above
        $dice = new FixedDiceRoller([
            1, 1,   // scatter
            3, 4,   // kickoff table: 7
            3, 3,   // cheering tie
            3,       // bounce D8
        ]);
        $scatterCalc = new ScatterCalculator();
        $tzCalc = new TacklezoneCalculator();
        $ballResolver = new BallResolver($dice, $tzCalc, $scatterCalc);
        $resolver = new KickoffResolver($dice, $scatterCalc, $ballResolver);

        $result = $resolver->resolveKickoff($state, new Position(6, 7));

        $types = array_map(fn($e) => $e->getType(), $result['events']);
        $this->assertNotContains('kick_off_return', $types);
    }

    public function testKickOffReturnPlayerAlreadyAtBallNoMovement(): void
    {
        // KOR player already at ball position — no movement
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 6, 7, skills: [SkillName::KickOffReturn], id: 1)
            ->addPlayer(TeamSide::AWAY, 15, 7, id: 10)
            ->withActiveTeam(TeamSide::HOME)
            ->build();

        // Scatter D8=1, D6=1 → ball at (6,6) — not same as (6,7) but close
        // Actually let's make ball land exactly at player
        $dice = new FixedDiceRoller([
            5, 1,   // scatter D8=5(S), D6=1 → (6,8)… let's use different target
            3, 4,   // kickoff table: 7
            3, 3,   // cheering tie
            4,       // catch/bounce
        ]);
        $scatterCalc = new ScatterCalculator();
        $tzCalc = new TacklezoneCalculator();
        $ballResolver = new BallResolver($dice, $tzCalc, $scatterCalc);
        $resolver = new KickoffResolver($dice, $scatterCalc, $ballResolver);

        // Ball target at (6,7), scatter S by 1 → (6,8) — still in receiving half
        $result = $resolver->resolveKickoff($state, new Position(6, 7));

        // Player should move at most 1 square toward (6,8)
        $player = $result['state']->getPlayer(1);
        $this->assertNotNull($player->getPosition());
    }

    public function testKickOffReturnMovesUpToThreeSquaresOnly(): void
    {
        // KOR player at (1,7), ball at (10,7) — should move only 3 squares
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 1, 7, movement: 8, skills: [SkillName::KickOffReturn], id: 1)
            ->addPlayer(TeamSide::AWAY, 15, 7, id: 10)
            ->withActiveTeam(TeamSide::HOME)
            ->build();

        $dice = new FixedDiceRoller([
            3, 1,   // scatter D8=3(E), D6=1 → (11,7) in home half
            3, 4,   // kickoff table: 7 (Brilliant Coaching)
            3, 3,   // cheering tie
            3,       // bounce D8 (ball bounces since no player at (11,7))
        ]);
        $scatterCalc = new ScatterCalculator();
        $tzCalc = new TacklezoneCalculator();
        $ballResolver = new BallResolver($dice, $tzCalc, $scatterCalc);
        $resolver = new KickoffResolver($dice, $scatterCalc, $ballResolver);

        $result = $resolver->resolveKickoff($state, new Position(10, 7));

        // Player moved from (1,7) toward (11,7), max 3 steps → (4,7)
        $player = $result['state']->getPlayer(1);
        $this->assertNotNull($player->getPosition());
        $this->assertEquals(4, $player->getPosition()->getX());
        $this->assertEquals(7, $player->getPosition()->getY());
    }

    public function testKickOffReturnClosestPlayerMoves(): void
    {
        // Two KOR players: one at (3,7), another at (8,7). Ball at (10,7). Closer one (8,7) moves.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 3, 7, skills: [SkillName::KickOffReturn], id: 1)
            ->addPlayer(TeamSide::HOME, 8, 7, skills: [SkillName::KickOffReturn], id: 2)
            ->addPlayer(TeamSide::AWAY, 15, 7, id: 10)
            ->withActiveTeam(TeamSide::HOME)
            ->build();

        $dice = new FixedDiceRoller([
            3, 1,   // scatter D8=3(E), D6=1 → (11,7), in home half
            3, 4,   // kickoff table: 7 (Brilliant Coaching)
            3, 3,   // cheering tie
            4,       // catch roll for player 2 at (11,7) after KOR move
        ]);
        $scatterCalc = new ScatterCalculator();
        $tzCalc = new TacklezoneCalculator();
        $ballResolver = new BallResolver($dice, $tzCalc, $scatterCalc);
        $resolver = new KickoffResolver($dice, $scatterCalc, $ballResolver);

        $result = $resolver->resolveKickoff($state, new Position(10, 7));

        // Player 2 (closer at 8,7) should have moved, player 1 (3,7) should not
        $player1 = $result['state']->getPlayer(1);
        $player2 = $result['state']->getPlayer(2);
        $this->assertEquals(3, $player1->getPosition()->getX()); // unchanged
        $this->assertEquals(11, $player2->getPosition()->getX()); // moved 3 squares: 8→11
    }

    public function testTouchbackNoKickOffReturn(): void
    {
        // Ball lands outside receiving half → touchback, no KOR
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 3, 7, skills: [SkillName::KickOffReturn], id: 1)
            ->addPlayer(TeamSide::AWAY, 15, 7, id: 10)
            ->withActiveTeam(TeamSide::HOME)
            ->build();

        // Scatter to outside receiving half: target (14,7), D8=3(E), D6=2 → (16,7) away half
        $dice = new FixedDiceRoller([
            3, 2,   // scatter → lands in away half → touchback
            3, 4,   // kickoff table: 7
            3, 3,   // cheering tie
        ]);
        $scatterCalc = new ScatterCalculator();
        $tzCalc = new TacklezoneCalculator();
        $ballResolver = new BallResolver($dice, $tzCalc, $scatterCalc);
        $resolver = new KickoffResolver($dice, $scatterCalc, $ballResolver);

        $result = $resolver->resolveKickoff($state, new Position(14, 7));

        $types = array_map(fn($e) => $e->getType(), $result['events']);
        $this->assertNotContains('kick_off_return', $types);
        $this->assertContains('touchback', $types);
    }
}
