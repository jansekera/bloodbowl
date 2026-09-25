<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\BallResolver;
use App\Engine\FixedDiceRoller;
use App\Engine\ScatterCalculator;
use App\Engine\TacklezoneCalculator;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

final class BallResolverTest extends TestCase
{
    private TacklezoneCalculator $tzCalc;
    private ScatterCalculator $scatterCalc;

    protected function setUp(): void
    {
        $this->tzCalc = new TacklezoneCalculator();
        $this->scatterCalc = new ScatterCalculator();
    }

    private function resolver(FixedDiceRoller $dice): BallResolver
    {
        return new BallResolver($dice, $this->tzCalc, $this->scatterCalc);
    }

    public function testPickupSuccessNoTackleZones(): void
    {
        // AG3 player, no TZ: target = 7 - 3 - 1 = 3+
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 10, 7, agility: 3, id: 1)
            ->withBallOnGround(10, 7)
            ->build();

        $player = $state->requirePlayer(1);

        // Roll 3 = success
        $dice = new FixedDiceRoller([3]);
        $result = $this->resolver($dice)->resolvePickup($state, $player);

        $this->assertTrue($result['success']);
        $this->assertTrue($result['state']->getBall()->isHeld());
        $this->assertEquals(1, $result['state']->getBall()->getCarrierId());
    }

    public function testPickupFailureBouncesball(): void
    {
        // AG3, no TZ: target = 3+
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 10, 7, agility: 3, id: 1)
            ->withBallOnGround(10, 7)
            ->build();

        $player = $state->requirePlayer(1);

        // Roll 2 = fail, then D8=3 (East) for bounce
        $dice = new FixedDiceRoller([2, 3]);
        $result = $this->resolver($dice)->resolvePickup($state, $player);

        $this->assertFalse($result['success']);
        $this->assertFalse($result['state']->getBall()->isHeld());
        // Ball bounced East to (11, 7)
        $ballPos = $result['state']->getBall()->requirePosition();
        $this->assertEquals(11, $ballPos->getX());
        $this->assertEquals(7, $ballPos->getY());
    }

    public function testPickupWithSureHandsReroll(): void
    {
        // AG3, Sure Hands, no TZ: target = 3+
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 10, 7, agility: 3, skills: [SkillName::SureHands], id: 1)
            ->withBallOnGround(10, 7)
            ->build();

        $player = $state->requirePlayer(1);

        // First roll 2 = fail, Sure Hands reroll 4 = success
        $dice = new FixedDiceRoller([2, 4]);
        $result = $this->resolver($dice)->resolvePickup($state, $player);

        $this->assertTrue($result['success']);
        $this->assertTrue($result['state']->getBall()->isHeld());
    }

    public function testPickupInTackleZoneHarder(): void
    {
        // AG3, 1 TZ: target = 7 - 3 - 1 + 1 = 4+
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 10, 7, agility: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 11, 7, id: 10)
            ->withBallOnGround(10, 7)
            ->build();

        $player = $state->requirePlayer(1);

        // Roll 3 = fail (need 4+), then D8=1 bounce N
        $dice = new FixedDiceRoller([3, 1]);
        $result = $this->resolver($dice)->resolvePickup($state, $player);

        $this->assertFalse($result['success']);
    }

    public function testCatchSuccessWithModifier(): void
    {
        // AG3, no TZ, +1 modifier: target = 7 - 3 + 0 - 1 = 3+
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 10, 7, agility: 3, id: 1)
            ->withBallOnGround(10, 7)
            ->build();

        $player = $state->requirePlayer(1);

        $dice = new FixedDiceRoller([3]);
        $result = $this->resolver($dice)->resolveCatch($state, $player, modifier: 1);

        $this->assertTrue($result['success']);
        $this->assertTrue($result['state']->getBall()->isHeld());
    }

    public function testCatchFailureBounces(): void
    {
        // AG3, no TZ, no modifier: target = 7 - 3 = 4+
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 10, 7, agility: 3, id: 1)
            ->withBallOnGround(10, 7)
            ->build();

        $player = $state->requirePlayer(1);

        // Roll 3 = fail, D8=5 bounce S
        $dice = new FixedDiceRoller([3, 5]);
        $result = $this->resolver($dice)->resolveCatch($state, $player);

        $this->assertFalse($result['success']);
        $ballPos = $result['state']->getBall()->requirePosition();
        $this->assertEquals(10, $ballPos->getX());
        $this->assertEquals(8, $ballPos->getY());
    }

    public function testCatchWithCatchSkillReroll(): void
    {
        // AG3, Catch skill, no TZ: target = 4+
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 10, 7, agility: 3, skills: [SkillName::Catch], id: 1)
            ->withBallOnGround(10, 7)
            ->build();

        $player = $state->requirePlayer(1);

        // Roll 3 = fail, Catch reroll 5 = success
        $dice = new FixedDiceRoller([3, 5]);
        $result = $this->resolver($dice)->resolveCatch($state, $player);

        $this->assertTrue($result['success']);
    }

    public function testBounceOntoPlayerWhoCatches(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 10, 7, agility: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 11, 7, agility: 3, id: 2)
            ->withBallOnGround(10, 7)
            ->build();

        // D8=3 (East), ball lands on player 2 at (11,7), catch roll 5 = success (need 4+)
        $dice = new FixedDiceRoller([3, 5]);
        $ballPos = $state->getBall()->requirePosition();
        $result = $this->resolver($dice)->resolveBounce($state, $ballPos);

        $this->assertTrue($result['state']->getBall()->isHeld());
        $this->assertEquals(2, $result['state']->getBall()->getCarrierId());
    }

    public function testBounceOntoPlayerWhoFailsCatchBounceAgain(): void
    {
        // ⛔ 21.09.2026: fixtura dostala `bezPrehozu()`. Od zapojeni tymoveho
        //   prehozu u chytani po odskoku (r. 929-933) by jinak tym prehoz
        //   pouzil a test by meril neco jineho, nez ma.
        $state = $this->bezPrehozu((new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 10, 7, agility: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 11, 7, agility: 3, id: 2)
            ->withBallOnGround(10, 7)
            ->build());

        // D8=3 (East) -> lands on player 2 at (11,7)
        // catch roll 2 = fail (need 4+)
        // bounce again: D8=3 (East) -> lands at (12,7), empty square
        $dice = new FixedDiceRoller([3, 2, 3]);
        $ballPos = $state->getBall()->requirePosition();
        $result = $this->resolver($dice)->resolveBounce($state, $ballPos);

        $this->assertFalse($result['state']->getBall()->isHeld());
        $ballPos = $result['state']->getBall()->requirePosition();
        $this->assertEquals(12, $ballPos->getX());
        $this->assertEquals(7, $ballPos->getY());
    }

    // ================= Pro a Catch u chytani po odskoku (21.09.2026) =================
    //
    // `rules_bb2016.txt` r. 8381: "Once per turn, a Pro is allowed to re-roll any
    // one dice roll he has made other than Armour, Injury or Casualty" -- tedy
    // i hod na chyceni mice po odskoku.
    // r. 1263: "players may use the Catch or Pro skill to try to re-roll the
    // catch roll" (kdyz mic dopadne).
    // r. 925-926: "you may never re-roll a single dice roll more than once."

    /** Fixtura bez tymovych prehozu -- kdyz se meri jen to, co dela odskok. */
    private function bezPrehozu(\App\DTO\GameState $s): \App\DTO\GameState
    {
        return $s->withTeamState(TeamSide::HOME, $s->getTeamState(TeamSide::HOME)->withRerolls(0))
            ->withTeamState(TeamSide::AWAY, $s->getTeamState(TeamSide::AWAY)->withRerolls(0));
    }

    /** @param list<\App\DTO\GameEvent> $events @return list<string>
     *
     * @param list<\App\DTO\GameEvent> $events
     * @return list<string>
     */
    private function typy(array $events): array
    {
        return array_map(static fn($e) => $e->getType(), $events);
    }

    public function testProPrehodiNeuspesneChytaniPoOdskoku(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 10, 7, agility: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 11, 7, agility: 3, skills: [SkillName::Pro], id: 2)
            ->withBallOnGround(10, 7)
            ->build();

        // D8=3 (vychod) -> mic na hrace 2; chyceni 2 = neuspech (treba 4+);
        // hod Pro 5 = smi prehodit; prehozene chyceni 5 = uspech.
        $dice = new FixedDiceRoller([3, 2, 5, 5]);
        $ballPos = $state->getBall()->requirePosition();
        $result = $this->resolver($dice)->resolveBounce($state, $ballPos);

        $this->assertTrue($result['state']->getBall()->isHeld(), 'r. 8381: Pro smi prehodit i chytani po odskoku');
        $this->assertSame(2, $result['state']->getBall()->getCarrierId());
        $this->assertContains('pro', $this->typy($result['events']), 'hod Pro ma byt videt v udalostech');
        $this->assertTrue($result['state']->getPlayer(2)?->isProUsedThisTurn(), 'Pro se zapise jako pouzity');
    }

    public function testNeuspesnyHodProNechaPuvodniVysledek(): void
    {
        // Pozitivni kontrola k testu vyse: kdyz hod Pro nevyjde (1-3), plati
        // puvodni neuspech a mic odskakuje dal.
        // bez tymovych prehozu, at je videt jen to, co dela neuspesny hod Pro
        $state = $this->bezPrehozu((new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 10, 7, agility: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 11, 7, agility: 3, skills: [SkillName::Pro], id: 2)
            ->withBallOnGround(10, 7)
            ->build());

        // D8=3 -> hrac 2; chyceni 2 = neuspech; hod Pro 3 = nesmi; odskok D8=3 na (12,7).
        $dice = new FixedDiceRoller([3, 2, 3, 3]);
        $ballPos = $state->getBall()->requirePosition();
        $result = $this->resolver($dice)->resolveBounce($state, $ballPos);

        $this->assertFalse($result['state']->getBall()->isHeld(), 'neuspesny hod Pro puvodni vysledek nemeni');
        $this->assertSame(12, $result['state']->getBall()->getPosition()?->getX());
    }

    public function testPrehozZCatchJeVidetVUdalostech(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 10, 7, agility: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 11, 7, agility: 3, skills: [SkillName::Catch], id: 2)
            ->withBallOnGround(10, 7)
            ->build();

        // D8=3 -> hrac 2; chyceni 2 = neuspech; prehoz z Catch 5 = uspech.
        $dice = new FixedDiceRoller([3, 2, 5]);
        $ballPos = $state->getBall()->requirePosition();
        $result = $this->resolver($dice)->resolveBounce($state, $ballPos);

        $this->assertTrue($result['state']->getBall()->isHeld());
        $typy = $this->typy($result['events']);
        $this->assertContains('reroll', $typy, 'prehoz z Catch musi vydat udalost');
        $this->assertSame(2, count(array_filter($typy, static fn($t) => $t === 'catch')),
            'videt maji byt OBA pokusy o chyceni, ne jen ten druhy');
    }

    public function testPoPrehozuZCatchUzPronePrichazi(): void
    {
        // r. 925-926: tataz kostka se neprehazuje dvakrat.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 10, 7, agility: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 11, 7, agility: 3, skills: [SkillName::Catch, SkillName::Pro], id: 2)
            ->withBallOnGround(10, 7)
            ->build();

        // D8=3 -> hrac 2; chyceni 2 neuspech; prehoz z Catch 2 taky neuspech;
        // s vadou by ted prislo Pro (5) a chyceni 5 = uspech. Spravne: odskok D8=3.
        $dice = new FixedDiceRoller([3, 2, 2, 5, 5]);
        $ballPos = $state->getBall()->requirePosition();
        $result = $this->resolver($dice)->resolveBounce($state, $ballPos);

        $this->assertFalse($result['state']->getBall()->isHeld(), 'po prehozu z Catch uz Pro na tutez kostku nesmi');
        $this->assertNotContains('pro', $this->typy($result['events']));
    }

    // ===== Tymovy prehoz u chytani po odskoku (21.09.2026) =====
    //
    // r. 929-933: "A coach may use a team re-roll to re-roll any dice roll
    // (other than Armour, Injury or Casualty rolls) made by a player in their
    // own team and who is still on the pitch **during their own turn**."
    // r. 8387 (Pro): "you can re-roll the Pro roll with a Team re-roll."
    // r. 1263: u mice, ktery DOPADA po vykopu, tymovy prehoz na chytani nejde
    //   (to resi `resolveCatch` s `teamRerollAvailable = false`).

    public function testTymovyPrehozZachraniChytaniPoOdskokuVeVlastnimKole(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 10, 7, agility: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 11, 7, agility: 3, id: 2)
            ->withBallOnGround(10, 7)
            ->build();

        // D8=3 -> mic na hrace 2; chyceni 2 = neuspech; tymovy prehoz: chyceni 5 = uspech.
        $dice = new FixedDiceRoller([3, 2, 5]);
        $ballPos = $state->getBall()->requirePosition();
        $result = $this->resolver($dice)->resolveBounce($state, $ballPos);

        $this->assertTrue($result['state']->getBall()->isHeld(), 'r. 929-933: tymovy prehoz plati i na chytani po odskoku');
        $this->assertSame(2, $result['state']->getTeamState(TeamSide::HOME)->getRerolls(), 'prehoz se ma odecist');
        $this->assertContains('reroll', $this->typy($result['events']));
    }

    public function testVSouperovemKoleSeTymovyPrehozNepouzije(): void
    {
        // Pozitivni kontrola: tataz fixtura, jen je na tahu souper.
        $state = (new GameStateBuilder())
            ->withActiveTeam(TeamSide::AWAY)
            ->addPlayer(TeamSide::HOME, 10, 7, agility: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 11, 7, agility: 3, id: 2)
            ->withBallOnGround(10, 7)
            ->build();

        // D8=3 -> hrac 2; chyceni 2 = neuspech; bez prehozu odskok D8=3 na (12,7).
        $dice = new FixedDiceRoller([3, 2, 3]);
        $ballPos = $state->getBall()->requirePosition();
        $result = $this->resolver($dice)->resolveBounce($state, $ballPos);

        $this->assertFalse($result['state']->getBall()->isHeld(), 'mimo vlastni kolo se tymovy prehoz pouzit nesmi');
        $this->assertSame(3, $result['state']->getTeamState(TeamSide::HOME)->getRerolls(), 'prehoz se nesmi odecist');
    }

    public function testTymovyPrehozPrehodiNEUSPESNYHodPro(): void
    {
        // r. 8387: kdyz hod Pro nevyjde, tymovy prehoz jde na HOD PRO, ne na kostku.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 10, 7, agility: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 11, 7, agility: 3, skills: [SkillName::Pro], id: 2)
            ->withBallOnGround(10, 7)
            ->build();

        // D8=3 -> hrac 2; chyceni 2 neuspech; hod Pro 2 = nesmi;
        // tymovy prehoz hodu Pro: 5 = smi; prehozene chyceni 5 = uspech.
        $dice = new FixedDiceRoller([3, 2, 2, 5, 5]);
        $ballPos = $state->getBall()->requirePosition();
        $result = $this->resolver($dice)->resolveBounce($state, $ballPos);

        $this->assertTrue($result['state']->getBall()->isHeld(), 'r. 8387: tymovy prehoz smi prehodit hod Pro');
        $this->assertSame(2, $result['state']->getTeamState(TeamSide::HOME)->getRerolls());
    }

    public function testBounceToEmptySquare(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 10, 7, agility: 3, id: 1)
            ->withBallOnGround(10, 7)
            ->build();

        // D8=1 (North) -> ball lands at (10,6), empty
        $dice = new FixedDiceRoller([1]);
        $ballPos = $state->getBall()->requirePosition();
        $result = $this->resolver($dice)->resolveBounce($state, $ballPos);

        $this->assertFalse($result['state']->getBall()->isHeld());
        $ballPos = $result['state']->getBall()->requirePosition();
        $this->assertEquals(10, $ballPos->getX());
        $this->assertEquals(6, $ballPos->getY());
    }

    public function testBounceOffPitchTriggersThrowIn(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 0, 7, agility: 3, id: 1)
            ->withBallOnGround(0, 7)
            ->build();

        // D8=7 (West) -> off pitch
        // Throw-in (r. 868-871): sablona 3 = kolmo (V), 2D6 = 1+2 -> (3,7) na hristi,
        // (hrac 1 na vychozim poli drahu neovlivni); (3,7) prazdne -> odskok 3 (V) -> (4,7)
        $dice = new FixedDiceRoller([7, 3, 1, 2, 3]);
        $ballPos = $state->getBall()->requirePosition();
        $result = $this->resolver($dice)->resolveBounce($state, $ballPos);

        $ballPos = $result['state']->getBall()->requirePosition();
        $this->assertTrue($ballPos->isOnPitch());
        $this->assertEquals(4, $ballPos->getX());
    }

    public function testThrowInLandsOnPitch(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1)
            ->withBallOnGround(0, 7)
            ->build();

        // sablona 4 = kolmo (V), 2D6 = 2+2 -> (4,7) prazdne -> odskok 1 (S) -> (4,6)
        $dice = new FixedDiceRoller([4, 2, 2, 1]);
        $ballPos = $state->getBall()->requirePosition();
        $result = $this->resolver($dice)->resolveThrowIn($state, $ballPos, new \App\ValueObject\Position(-1, 7));

        $ballPos = $result['state']->getBall()->requirePosition();
        $this->assertEquals(4, $ballPos->getX());
        $this->assertEquals(6, $ballPos->getY());
    }

    public function testPickupTargetCalculation(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 10, 7, agility: 3, id: 1)
            ->build();

        $player = $state->requirePlayer(1);

        $dice = new FixedDiceRoller([]);
        $resolver = $this->resolver($dice);

        // AG3, no TZ: 7 - 3 - 1 = 3
        $this->assertEquals(3, $resolver->getPickupTarget($state, $player));
    }

    public function testCatchTargetCalculation(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 10, 7, agility: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 11, 7, id: 10)
            ->build();

        $player = $state->requirePlayer(1);

        $dice = new FixedDiceRoller([]);
        $resolver = $this->resolver($dice);

        // AG3, 1 TZ, no modifier: 7 - 3 + 1 = 5
        $this->assertEquals(5, $resolver->getCatchTarget($state, $player));

        // With +1 modifier: 7 - 3 + 1 - 1 = 4
        $this->assertEquals(4, $resolver->getCatchTarget($state, $player, 1));
    }
}
