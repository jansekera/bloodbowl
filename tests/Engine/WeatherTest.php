<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\Action\MoveHandler;
use App\Engine\BallResolver;
use App\Engine\FixedDiceRoller;
use App\Engine\KickoffResolver;
use App\Engine\Pathfinder;
use App\Engine\PassResolver;
use App\Engine\ScatterCalculator;
use App\Engine\TacklezoneCalculator;
use App\Enum\PassRange;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use App\Enum\Weather;
use App\ValueObject\Position;
use PHPUnit\Framework\TestCase;

final class WeatherTest extends TestCase
{
    private TacklezoneCalculator $tzCalc;
    private ScatterCalculator $scatterCalc;

    protected function setUp(): void
    {
        $this->tzCalc = new TacklezoneCalculator();
        $this->scatterCalc = new ScatterCalculator();
    }

    // --- Changing Weather Tests ---

    public function testChangingWeatherRollsNewWeather(): void
    {
        $dice = new FixedDiceRoller([4, 4, 1, 1]); // kt roll=8, weather roll=2 -> Sweltering Heat
        $ballResolver = new BallResolver($dice, $this->tzCalc, $this->scatterCalc);
        $resolver = new KickoffResolver($dice, $this->scatterCalc, $ballResolver);

        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 6, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 15, 7, id: 2)
            ->build();

        $result = $resolver->resolveKickoffTable($state, TeamSide::HOME);

        $this->assertEquals(Weather::SWELTERING_HEAT, $result['state']->getWeather());
    }

    public function testChangingWeatherToBlizzard(): void
    {
        $dice = new FixedDiceRoller([4, 4, 6, 6]); // kt roll=8, weather roll=12 -> Blizzard
        $ballResolver = new BallResolver($dice, $this->tzCalc, $this->scatterCalc);
        $resolver = new KickoffResolver($dice, $this->scatterCalc, $ballResolver);

        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 6, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 15, 7, id: 2)
            ->build();

        $result = $resolver->resolveKickoffTable($state, TeamSide::HOME);

        $this->assertEquals(Weather::BLIZZARD, $result['state']->getWeather());
    }

    public function testChangingWeatherGeneratesEvents(): void
    {
        $dice = new FixedDiceRoller([4, 4, 3, 3]); // kt=8, weather=6 -> Nice
        $ballResolver = new BallResolver($dice, $this->tzCalc, $this->scatterCalc);
        $resolver = new KickoffResolver($dice, $this->scatterCalc, $ballResolver);

        $state = (new GameStateBuilder())
            ->withWeather(Weather::POURING_RAIN)
            ->addPlayer(TeamSide::HOME, 6, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 15, 7, id: 2)
            ->build();

        $result = $resolver->resolveKickoffTable($state, TeamSide::HOME);

        $types = array_map(fn($e) => $e->getType(), $result['events']);
        $this->assertContains('weather_change', $types);
        $this->assertEquals(Weather::NICE, $result['state']->getWeather());
    }

    // --- Pass Weather Modifier Tests ---

    public function testPassAccuracyUnchangedInNiceWeather(): void
    {
        $dice = new FixedDiceRoller([]);
        $ballResolver = new BallResolver($dice, $this->tzCalc, $this->scatterCalc);
        $passResolver = new PassResolver($dice, $this->tzCalc, $this->scatterCalc, $ballResolver);

        $state = (new GameStateBuilder())
            ->withWeather(Weather::NICE)
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, agility: 3)
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->build();

        $player = $state->getPlayer(1);
        $target = $passResolver->getAccuracyTarget($state, $player, PassRange::SHORT_PASS);

        // 7 - 3 + 0 - 0(short pass modifier) = 4
        $this->assertEquals(4, $target);
    }

    public function testPassAccuracyPlusOneInVerySunny(): void
    {
        $dice = new FixedDiceRoller([]);
        $ballResolver = new BallResolver($dice, $this->tzCalc, $this->scatterCalc);
        $passResolver = new PassResolver($dice, $this->tzCalc, $this->scatterCalc, $ballResolver);

        $state = (new GameStateBuilder())
            ->withWeather(Weather::VERY_SUNNY)
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, agility: 3)
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->build();

        $player = $state->getPlayer(1);
        $target = $passResolver->getAccuracyTarget($state, $player, PassRange::SHORT_PASS);

        // 7 - 3 + 0 - 0(short pass) + 1(weather) = 5
        $this->assertEquals(5, $target);
    }

    public function testPassAccuracyPlusOneInPouringRain(): void
    {
        $dice = new FixedDiceRoller([]);
        $ballResolver = new BallResolver($dice, $this->tzCalc, $this->scatterCalc);
        $passResolver = new PassResolver($dice, $this->tzCalc, $this->scatterCalc, $ballResolver);

        $state = (new GameStateBuilder())
            ->withWeather(Weather::POURING_RAIN)
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, agility: 3)
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->build();

        $player = $state->getPlayer(1);
        $target = $passResolver->getAccuracyTarget($state, $player, PassRange::SHORT_PASS);

        // 7 - 3 + 0 - 0(short pass) + 1(weather) = 5
        $this->assertEquals(5, $target);
    }

    public function testPassAccuracyPlusOneInBlizzard(): void
    {
        $dice = new FixedDiceRoller([]);
        $ballResolver = new BallResolver($dice, $this->tzCalc, $this->scatterCalc);
        $passResolver = new PassResolver($dice, $this->tzCalc, $this->scatterCalc, $ballResolver);

        $state = (new GameStateBuilder())
            ->withWeather(Weather::BLIZZARD)
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, agility: 3)
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->build();

        $player = $state->getPlayer(1);
        $target = $passResolver->getAccuracyTarget($state, $player, PassRange::SHORT_PASS);

        // 7 - 3 + 0 - 0(short pass) + 1(weather) = 5
        $this->assertEquals(5, $target);
    }

    // --- Pickup Weather Modifier Tests ---

    public function testPickupUnchangedInNiceWeather(): void
    {
        $dice = new FixedDiceRoller([]);
        $ballResolver = new BallResolver($dice, $this->tzCalc, $this->scatterCalc);

        $state = (new GameStateBuilder())
            ->withWeather(Weather::NICE)
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, agility: 3)
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->withBallOnGround(5, 7)
            ->build();

        $player = $state->getPlayer(1);
        $target = $ballResolver->getPickupTarget($state, $player);

        // 7 - 3 - 1 + 0 = 3
        $this->assertEquals(3, $target);
    }

    public function testPickupUnchangedInVerySunny(): void
    {
        $dice = new FixedDiceRoller([]);
        $ballResolver = new BallResolver($dice, $this->tzCalc, $this->scatterCalc);

        $state = (new GameStateBuilder())
            ->withWeather(Weather::VERY_SUNNY)
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, agility: 3)
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->withBallOnGround(5, 7)
            ->build();

        $player = $state->getPlayer(1);
        $target = $ballResolver->getPickupTarget($state, $player);

        // 7 - 3 - 1 + 0 = 3 (no change for Very Sunny)
        $this->assertEquals(3, $target);
    }

    public function testPickupPlusOneInPouringRain(): void
    {
        $dice = new FixedDiceRoller([]);
        $ballResolver = new BallResolver($dice, $this->tzCalc, $this->scatterCalc);

        $state = (new GameStateBuilder())
            ->withWeather(Weather::POURING_RAIN)
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, agility: 3)
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->withBallOnGround(5, 7)
            ->build();

        $player = $state->getPlayer(1);
        $target = $ballResolver->getPickupTarget($state, $player);

        // 7 - 3 - 1 + 0 + 1(weather) = 4
        $this->assertEquals(4, $target);
    }

    public function testPickupPlusOneInBlizzard(): void
    {
        $dice = new FixedDiceRoller([]);
        $ballResolver = new BallResolver($dice, $this->tzCalc, $this->scatterCalc);

        $state = (new GameStateBuilder())
            ->withWeather(Weather::BLIZZARD)
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, agility: 3)
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->withBallOnGround(5, 7)
            ->build();

        $player = $state->getPlayer(1);
        $target = $ballResolver->getPickupTarget($state, $player);

        // 7 - 3 - 1 + 0 + 1(weather) = 4
        $this->assertEquals(4, $target);
    }

    // --- Catch Weather Modifier Tests ---

    public function testCatchUnchangedInNiceWeather(): void
    {
        $dice = new FixedDiceRoller([]);
        $ballResolver = new BallResolver($dice, $this->tzCalc, $this->scatterCalc);

        $state = (new GameStateBuilder())
            ->withWeather(Weather::NICE)
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, agility: 3)
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->build();

        $player = $state->getPlayer(1);
        $target = $ballResolver->getCatchTarget($state, $player);

        // 7 - 3 + 0 - 0 = 4
        $this->assertEquals(4, $target);
    }

    public function testCatchPlusOneInPouringRain(): void
    {
        $dice = new FixedDiceRoller([]);
        $ballResolver = new BallResolver($dice, $this->tzCalc, $this->scatterCalc);

        $state = (new GameStateBuilder())
            ->withWeather(Weather::POURING_RAIN)
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, agility: 3)
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->build();

        $player = $state->getPlayer(1);
        $target = $ballResolver->getCatchTarget($state, $player);

        // 7 - 3 + 0 - 0 + 1(weather) = 5
        $this->assertEquals(5, $target);
    }

    public function testCatchPlusOneInBlizzard(): void
    {
        $dice = new FixedDiceRoller([]);
        $ballResolver = new BallResolver($dice, $this->tzCalc, $this->scatterCalc);

        $state = (new GameStateBuilder())
            ->withWeather(Weather::BLIZZARD)
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, agility: 3)
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->build();

        $player = $state->getPlayer(1);
        $target = $ballResolver->getCatchTarget($state, $player);

        // 7 - 3 + 0 - 0 + 1(weather) = 5
        $this->assertEquals(5, $target);
    }

    // --- GFI Weather Modifier Tests ---

    public function testGfiTwoPlusInNiceWeather(): void
    {
        $dice = new FixedDiceRoller([2]); // GFI roll = 2 -> success
        $ballResolver = new BallResolver($dice, $this->tzCalc, $this->scatterCalc);
        $pathfinder = new Pathfinder($this->tzCalc);
        $moveHandler = new MoveHandler($dice, $this->tzCalc, $pathfinder, $ballResolver);

        $state = (new GameStateBuilder())
            ->withWeather(Weather::NICE)
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, movement: 6)
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->build();

        // Move 7 squares (1 GFI)
        $result = $moveHandler->resolve($state, ['playerId' => 1, 'x' => 12, 'y' => 7]);

        $this->assertTrue($result->isSuccess());
    }

    public function testGfiThreePlusInBlizzard(): void
    {
        // Roll 2 -> fails in blizzard (need 3+), team reroll roll 2 -> fail again
        $dice = new FixedDiceRoller([2, 2]); // GFI roll = 2 fail, team reroll = 2 fail
        $ballResolver = new BallResolver($dice, $this->tzCalc, $this->scatterCalc);
        $pathfinder = new Pathfinder($this->tzCalc);
        $moveHandler = new MoveHandler($dice, $this->tzCalc, $pathfinder, $ballResolver);

        $state = (new GameStateBuilder())
            ->withWeather(Weather::BLIZZARD)
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, movement: 6)
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->build();

        // Move 7 squares (1 GFI), roll 2 fails in blizzard, team reroll also fails
        $result = $moveHandler->resolve($state, ['playerId' => 1, 'x' => 12, 'y' => 7]);

        $this->assertTrue($result->isTurnover());
    }

    public function testGfiThreeSucceedsInBlizzard(): void
    {
        $dice = new FixedDiceRoller([3]); // GFI roll = 3 -> success in blizzard
        $ballResolver = new BallResolver($dice, $this->tzCalc, $this->scatterCalc);
        $pathfinder = new Pathfinder($this->tzCalc);
        $moveHandler = new MoveHandler($dice, $this->tzCalc, $pathfinder, $ballResolver);

        $state = (new GameStateBuilder())
            ->withWeather(Weather::BLIZZARD)
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, movement: 6)
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->build();

        $result = $moveHandler->resolve($state, ['playerId' => 1, 'x' => 12, 'y' => 7]);

        $this->assertTrue($result->isSuccess());
    }

    public function testGfiSureFeetInBlizzardUsesThreshold3(): void
    {
        // Roll 2 -> fail, Sure Feet reroll -> 3 -> success
        $dice = new FixedDiceRoller([2, 3]); // GFI=2 fail, Sure Feet reroll=3 success
        $ballResolver = new BallResolver($dice, $this->tzCalc, $this->scatterCalc);
        $pathfinder = new Pathfinder($this->tzCalc);
        $moveHandler = new MoveHandler($dice, $this->tzCalc, $pathfinder, $ballResolver);

        $state = (new GameStateBuilder())
            ->withWeather(Weather::BLIZZARD)
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, movement: 6, skills: [SkillName::SureFeet])
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->build();

        $result = $moveHandler->resolve($state, ['playerId' => 1, 'x' => 12, 'y' => 7]);

        $this->assertTrue($result->isSuccess());
    }

    public function testGfiUnchangedInPouringRain(): void
    {
        $dice = new FixedDiceRoller([2]); // GFI roll = 2 -> success (normal threshold)
        $ballResolver = new BallResolver($dice, $this->tzCalc, $this->scatterCalc);
        $pathfinder = new Pathfinder($this->tzCalc);
        $moveHandler = new MoveHandler($dice, $this->tzCalc, $pathfinder, $ballResolver);

        $state = (new GameStateBuilder())
            ->withWeather(Weather::POURING_RAIN)
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, movement: 6)
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->build();

        $result = $moveHandler->resolve($state, ['playerId' => 1, 'x' => 12, 'y' => 7]);

        $this->assertTrue($result->isSuccess());
    }

    // --- Sweltering Heat Tests ---

    public function testSwelteringHeatRemovesPlayerAtKickoff(): void
    {
        $state = (new GameStateBuilder())
            ->withWeather(Weather::SWELTERING_HEAT)
            ->addPlayer(TeamSide::HOME, 6, 5, id: 1)
            ->addPlayer(TeamSide::HOME, 6, 7, id: 2)
            ->addPlayer(TeamSide::AWAY, 15, 5, id: 3)
            ->addPlayer(TeamSide::AWAY, 15, 7, id: 4)
            ->build();

        // Scatter: D8=1, D6=1 -> (6,4) in home half
        // Sweltering heat: D6=1 (home index 0 -> player 1), D6=1 (away index 0 -> player 3)
        // Kickoff table: 1+4=5 (High Kick)
        // Catch roll: 6 (success)
        $dice = new FixedDiceRoller([1, 1, 1, 1, 1, 4, 6]);
        $ballResolver = new BallResolver($dice, $this->tzCalc, $this->scatterCalc);
        $resolver = new KickoffResolver($dice, $this->scatterCalc, $ballResolver);

        $result = $resolver->resolveKickoff($state, new Position(6, 5));

        // Player 1 should be KO'd
        $p1 = $result['state']->getPlayer(1);
        $this->assertEquals(PlayerState::KO, $p1->getState());

        // Player 3 should be KO'd
        $p3 = $result['state']->getPlayer(3);
        $this->assertEquals(PlayerState::KO, $p3->getState());
    }

    public function testNormalWeatherDoesNotRemovePlayers(): void
    {
        $state = (new GameStateBuilder())
            ->withWeather(Weather::NICE)
            ->addPlayer(TeamSide::HOME, 6, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 15, 5, id: 2)
            ->build();

        // Scatter: D8=1, D6=1
        // No sweltering heat (weather is Nice)
        // Kickoff table: 2+4=6 (Cheering), home=3, away=3 (tie)
        // Ball: lands at (6,4) - empty, bounce D8=3 -> (7,4)
        $dice = new FixedDiceRoller([1, 1, 2, 4, 3, 3, 3]);
        $ballResolver = new BallResolver($dice, $this->tzCalc, $this->scatterCalc);
        $resolver = new KickoffResolver($dice, $this->scatterCalc, $ballResolver);

        $result = $resolver->resolveKickoff($state, new Position(6, 5));

        $this->assertEquals(PlayerState::STANDING, $result['state']->getPlayer(1)->getState());
        $this->assertEquals(PlayerState::STANDING, $result['state']->getPlayer(2)->getState());
    }

    public function testSwelteringHeatEventContainsPlayerName(): void
    {
        $state = (new GameStateBuilder())
            ->withWeather(Weather::SWELTERING_HEAT)
            ->addPlayer(TeamSide::HOME, 6, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 15, 5, id: 2)
            ->build();

        // Scatter: D8=1, D6=1
        // Sweltering heat: D6=1 (home index 0 -> player 1), D6=1 (away index 0 -> player 2)
        // Kickoff table: 1+4=5 (High Kick) - but player 1 is now KO'd, so no one to move
        // Ball at (6,4) empty square, bounce D8=3 -> (7,4)
        $dice = new FixedDiceRoller([1, 1, 1, 1, 1, 4, 3]);
        $ballResolver = new BallResolver($dice, $this->tzCalc, $this->scatterCalc);
        $resolver = new KickoffResolver($dice, $this->scatterCalc, $ballResolver);

        $result = $resolver->resolveKickoff($state, new Position(6, 5));

        $swelterEvents = array_filter(
            $result['events'],
            fn($e) => $e->getType() === 'sweltering_heat',
        );
        $this->assertNotEmpty($swelterEvents);

        $firstEvent = array_values($swelterEvents)[0];
        $this->assertStringContainsString('Player 1', $firstEvent->getDescription());
    }

    // --- GameState Weather Serialization ---

    public function testWeatherSerializationRoundTrip(): void
    {
        $state = (new GameStateBuilder())
            ->withWeather(Weather::BLIZZARD)
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1)
            ->addPlayer(TeamSide::AWAY, 15, 7, id: 2)
            ->build();

        $array = $state->toArray();
        $this->assertEquals('blizzard', $array['weather']);

        $restored = \App\DTO\GameState::fromArray($array);
        $this->assertEquals(Weather::BLIZZARD, $restored->getWeather());
    }

    public function testWeatherDefaultsToNice(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1)
            ->addPlayer(TeamSide::AWAY, 15, 7, id: 2)
            ->build();

        $this->assertEquals(Weather::NICE, $state->getWeather());
    }

    public function testWeatherWitherMethod(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1)
            ->addPlayer(TeamSide::AWAY, 15, 7, id: 2)
            ->build();

        $newState = $state->withWeather(Weather::POURING_RAIN);
        $this->assertEquals(Weather::POURING_RAIN, $newState->getWeather());
        $this->assertEquals(Weather::NICE, $state->getWeather()); // original unchanged
    }
}
