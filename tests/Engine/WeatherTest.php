<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\Action\MoveHandler;
use App\Engine\BallResolver;
use App\Engine\FixedDiceRoller;
use App\Engine\GameFlowResolver;
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
        $dice = new FixedDiceRoller([3, 4, 1, 1]); // kt roll=7, weather roll=2 -> Sweltering Heat
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
        $dice = new FixedDiceRoller([3, 4, 6, 6]); // kt roll=7, weather roll=12 -> Blizzard
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
        $dice = new FixedDiceRoller([3, 4, 3, 3]); // kt=7, weather=6 -> Nice
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

    // ⛔⛔ OPRAVENO 15.09.2026 -- tyhle dva testy tvrdily VADU ENGINU, ne pravidla.
    //   `rules_bb2016.txt` r. 1482-1494: -1 k PRIHRAVCE dava jen Very Sunny.
    //   Pouring Rain dava -1 k chytani, zvedani a intercepci. Blizzard zadny
    //   modifikator nema -- jen povoli pouze quick a short prihravky.
    public function testPassAccuracyUnchangedInPouringRain(): void
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

        // 7 - 3 + 0 - 0(short pass) = 4 -- dest nema vliv na prihravku
        $this->assertEquals(4, $target);
    }

    public function testPassAccuracyUnchangedInBlizzard(): void
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

        // 7 - 3 + 0 - 0(short pass) = 4 -- Blizzard omezuje DOSAH, ne presnost
        $this->assertEquals(4, $target);
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

    // ⭐ PRIDANO 15.09.2026: Big Hand ignoruje pri zvedani i Pouring Rain
    //   (r. 7835-7839: "ignores modifier(s) for enemy tackle zones or Pouring
    //   Rain weather when he attempts to pick up the ball").
    public function testPickupBigHandIgnoresPouringRain(): void
    {
        $dice = new FixedDiceRoller([]);
        $ballResolver = new BallResolver($dice, $this->tzCalc, $this->scatterCalc);

        $state = (new GameStateBuilder())
            ->withWeather(Weather::POURING_RAIN)
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, agility: 3, skills: [\App\Enum\SkillName::BigHand])
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->withBallOnGround(5, 7)
            ->build();

        // 7 - 3 - 1 = 3 -- dest se u Big Hand nepocita
        $this->assertEquals(3, $ballResolver->getPickupTarget($state, $state->getPlayer(1)));
    }

    // ⛔ OPRAVENO 15.09.2026: Blizzard zvedani NEOVLIVNUJE (r. 1490-1494).
    public function testPickupUnchangedInBlizzard(): void
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

        // 7 - 3 - 1 + 0 = 3
        $this->assertEquals(3, $target);
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

    // ⛔ OPRAVENO 15.09.2026: Blizzard chytani NEOVLIVNUJE (r. 1490-1494).
    public function testCatchUnchangedInBlizzard(): void
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

        // 7 - 3 + 0 - 0 = 4
        $this->assertEquals(4, $target);
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

    /**
     * ⛔⛔ PREPSANO 24.09.2026 (balik G, 4/4). Puvodne se tenhle test jmenoval
     *   `testSwelteringHeatRemovesPlayerAtKickoff` a tvrdil, ze se heat resi
     *   PRI VYKOPU a posila hrace do KO. Obojí bylo proti pravidlu -- r. 1477-1481
     *   rika "Roll a D6 for each player on the pitch AT THE END OF A DRIVE. On a
     *   roll of 1 the player collapses and MAY NOT BE SET UP for the next
     *   kick-off." Test tedy kodifikoval vadu; ted meri pravidlo.
     *
     * ⭐ Pri vykopu uz se heat neresi VUBEC -- to overuje test nize.
     */
    public function testSwelteringHeatDoesNothingAtKickoff(): void
    {
        $state = (new GameStateBuilder())
            ->withWeather(Weather::SWELTERING_HEAT)
            ->addPlayer(TeamSide::HOME, 6, 5, id: 1)
            ->addPlayer(TeamSide::HOME, 6, 7, id: 2)
            ->addPlayer(TeamSide::AWAY, 15, 5, id: 3)
            ->addPlayer(TeamSide::AWAY, 15, 7, id: 4)
            ->build();

        // Scatter D8=1, D6=1; kickoff table 1+4=5 (High Kick); catch 6.
        // ⭐ Zadne kostky na heat se uz neodebiraji.
        $dice = new FixedDiceRoller([1, 1, 1, 4, 6]);
        $ballResolver = new BallResolver($dice, $this->tzCalc, $this->scatterCalc);
        $resolver = new KickoffResolver($dice, $this->scatterCalc, $ballResolver);

        $result = $resolver->resolveKickoff($state, new Position(6, 5));

        foreach ([1, 2, 3, 4] as $id) {
            $this->assertSame(
                PlayerState::STANDING,
                $result['state']->getPlayer($id)->getState(),
                "Hrac {$id} nesmi pri vykopu nikam zmizet",
            );
            $this->assertFalse(
                $result['state']->getPlayer($id)->isOutNextSetup(),
                "Hraci {$id} se nesmi pri vykopu nastavit priznak heat",
            );
        }

        $swelter = array_filter($result['events'], fn($e) => $e->getType() === 'sweltering_heat');
        $this->assertSame([], $swelter, 'Pri vykopu nesmi padnout zadna udalost heat');
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

    /**
     * ⭐ BALIK G 24.09.2026 -- heat na KONCI DRIVU: D6 za KAZDEHO hrace na hristi,
     *   na 1 hrac kolabuje a dostane priznak `outNextSetup`.
     *   ⛔ NENI to KO -- KO ma navratovy hod 4+, heat exhaustion zadny nema.
     */
    public function testSwelteringHeatRollsForEveryPlayerAtEndOfDrive(): void
    {
        $state = (new GameStateBuilder())
            ->withWeather(Weather::SWELTERING_HEAT)
            ->addPlayer(TeamSide::HOME, 6, 5, id: 1)
            ->addPlayer(TeamSide::HOME, 6, 7, id: 2)
            ->addPlayer(TeamSide::AWAY, 15, 5, id: 3)
            ->addPlayer(TeamSide::AWAY, 15, 7, id: 4)
            ->build();

        // Ctyri hraci na hristi => ctyri D6. Jen prvnimu padne 1.
        $dice = new FixedDiceRoller([1, 5, 5, 5]);
        $events = [];
        $flow = new GameFlowResolver($dice);
        $newState = $this->callHeat($flow, $state, $events);

        $this->assertTrue($newState->getPlayer(1)->isOutNextSetup(), 'Hodil 1 => kolabuje');
        $this->assertSame(PlayerState::STANDING, $newState->getPlayer(1)->getState(),
            'Kolaps NENI KO -- stav se nemeni, meni se priznak');

        foreach ([2, 3, 4] as $id) {
            $this->assertFalse($newState->getPlayer($id)->isOutNextSetup(),
                "Hrac {$id} hodil 5, nesmi kolabovat");
        }

        $this->assertCount(1, array_filter($events, fn($e) => $e->getType() === 'sweltering_heat'));
    }

    /**
     * ⭐⭐⭐ POZITIVNI KONTROLA: za jineho pocasi se nehazi vubec.
     * Bez ni by "nikdo nezkolaboval" mohlo znamenat i to, ze se metoda nevola.
     */
    public function testNoHeatRollsInNiceWeather(): void
    {
        $state = (new GameStateBuilder())
            ->withWeather(Weather::NICE)
            ->addPlayer(TeamSide::HOME, 6, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 15, 5, id: 2)
            ->build();

        // Samé jednicky: kdyby se hazelo, zkolabovali by oba.
        $dice = new FixedDiceRoller([1, 1, 1, 1]);
        $events = [];
        $newState = $this->callHeat(new GameFlowResolver($dice), $state, $events);

        $this->assertFalse($newState->getPlayer(1)->isOutNextSetup());
        $this->assertFalse($newState->getPlayer(2)->isOutNextSetup());
        $this->assertSame([], $events);
    }

    /**
     * Metoda je privatni (je to krok konce drivu, ne verejna akce), takze
     * se vola pres reflexi -- jinak by test musel projit celym touchdownem
     * a meril by pet veci najednou.
     *
     * @param list<\App\DTO\GameEvent> $events
     */
    private function callHeat(GameFlowResolver $flow, $state, array &$events)
    {
        $m = new \ReflectionMethod($flow, 'hodyNaSwelteringHeat');
        $m->setAccessible(true);
        return $m->invokeArgs($flow, [$state, &$events]);
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
