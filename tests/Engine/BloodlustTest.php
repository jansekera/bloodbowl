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

final class BloodlustTest extends TestCase
{
    /**
     * Bloodlust roll 2+ passes: action proceeds normally.
     */
    public function testBloodlustPassesOn2Plus(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, skills: [SkillName::Bloodlust])
            ->addPlayer(TeamSide::HOME, 6, 7, id: 2) // Thrall teammate
            ->withBallOnGround(7, 7) // something to move toward
            ->build();

        // Bloodlust roll: 2 (passes)
        // Move — no dodges/GFIs needed
        $dice = new FixedDiceRoller([2]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1,
            'x' => 4,
            'y' => 7,
        ]);

        $this->assertTrue($result->isSuccess());
        $newState = $result->getNewState();
        $pos = $newState->requirePlayer(1)->requirePosition();
        $this->assertEquals(4, $pos->getX());
        $this->assertEquals(7, $pos->getY());
    }

    /**
     * Bloodlust fail: bite adjacent Thrall, action still proceeds.
     */
    public function testBloodlustBiteThrall(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, skills: [SkillName::Bloodlust])
            ->addPlayer(TeamSide::HOME, 6, 7, id: 2) // Thrall teammate (no Bloodlust)
            ->withBallOnGround(3, 7)
            ->build();

        // ⛔ PREPSANO 11.09.2026 (PHP22): kousnuti uz NENI auto-KO, ale
        //   HOD NA ZRANENI (r. 7939-7941: „make an Injury roll on the Thrall
        //   treating any casualty roll as Badly Hurt"), takze se hazi 2 kostky
        //   navic. Drive tu stacila jedna a test spadl na „no more rolls".
        // Bloodlust: 1 => hladovy; tah na (5,8) VEDLE Thralla; na KONCI akce kousnuti: zraneni 2+2 (r. 7934-7941)
        $dice = new FixedDiceRoller([1, 2, 2]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1,
            'x' => 5,
            'y' => 8,
        ]);

        $this->assertTrue($result->isSuccess());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('bloodlust_bite', $types);

        $newState = $result->getNewState();
        // ⛔ Drive se tu tvrdilo KO a `getPosition() === null` -- to bylo
        //   z auto-KO. Hod na zraneni 2+2=4 dava STUNNED, tedy hrace, ktery
        //   NA HRISTI ZUSTAVA. Presne v tom je ta oprava: kousnuti neposila
        //   Thralla pryc, jen ho zrani.
        $thrall = $newState->requirePlayer(2);
        $this->assertSame(PlayerState::STUNNED, $thrall->getState(),
            'hod 2+2=4 je Stunned (r. 7939-7941)');
        $this->assertNotNull($thrall->getPosition(),
            'omraceny Thrall zustava na hristi');

        // Vampire still moved
        $vampirePos = $newState->requirePlayer(1)->requirePosition();
        $this->assertEquals(5, $vampirePos->getX());
    }

    /**
     * Bloodlust fail with no Thrall: vampire loses action, moved to reserves.
     */
    public function testBloodlustFailNoThrall(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, skills: [SkillName::Bloodlust])
            // No adjacent Thralls
            ->withBallOffPitch()
            ->build();

        // Bloodlust roll: 1 (fail), no Thrall → off pitch
        $dice = new FixedDiceRoller([1]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1,
            'x' => 4,
            'y' => 7,
        ]);

        // ⛔ PREPSANO 11.09.2026 (PHP22): drive se tu tvrdil `isSuccess()`
        //   -- tedy ZADNY turnover. r. 7942-7943: „**Failure to bite a Thrall
        //   is a turnover** and requires you to feed on a spectator."
        $this->assertTrue($result->isTurnover(),
            'upir bez Thralla = turnover (r. 7942-7943)');
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('bloodlust_fail', $types);

        $newState = $result->getNewState();
        $vampire = $newState->requirePlayer(1);
        $this->assertEquals(PlayerState::OFF_PITCH, $vampire->getState());
        $this->assertNull($vampire->getPosition());
    }

    /**
     * Another Vampire adjacent doesn't count as Thrall (has Bloodlust).
     */
    public function testBloodlustVampireNotThrall(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, skills: [SkillName::Bloodlust])
            ->addPlayer(TeamSide::HOME, 6, 7, id: 2, skills: [SkillName::Bloodlust]) // Another Vampire
            ->withBallOffPitch()
            ->build();

        // Bloodlust roll: 1 (fail), adjacent player is Vampire (has Bloodlust) → no Thrall → reserves
        $dice = new FixedDiceRoller([1]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1,
            'x' => 4,
            'y' => 7,
        ]);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('bloodlust_fail', $types);
        $this->assertNotContains('bloodlust_bite', $types);
    }

    /**
     * Bloodlust applies to block action too.
     */
    public function testBloodlustOnBlock(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, skills: [SkillName::Bloodlust])
            ->addPlayer(TeamSide::HOME, 5, 6, id: 2) // Thrall
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 3) // block target
            ->withBallOffPitch()
            ->build();

        // ⛔ PREPSANO 11.09.2026 (PHP22): kousnuti je HOD NA ZRANENI, ne KO.
        // Bloodlust: 1 (fail) → zraneni Thralla 6+6=12 (casualty, ale
        //   „treating any casualty roll as Badly Hurt" => INJURED, ne DEAD)
        //   → block: 6 → DEFENDER_DOWN → armor 4+4=8 vs AV8, neprorazi
        $dice = new FixedDiceRoller([1, 6, 6, 4, 4, 6, 6, 1, 1]); // Bloodlust 1; blok 2 kostky (asistence Thralla) 6,6; brneni 4+4; NA KONCI kousnuti 6+6 = CAS + D68 1,1
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, [
            'playerId' => 1,
            'targetId' => 3,
        ]);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('bloodlust_bite', $types);
        $this->assertContains('block', $types);

        // Thrall dostal hod na zraneni -- a z kousnuti se NEUMIRA.
        $thrall = $result->getNewState()->requirePlayer(2);
        $this->assertNotSame(PlayerState::DEAD, $thrall->getState(),
            'z kousnuti se neumira (r. 7940-7941)');
        $this->assertNotSame(PlayerState::STANDING, $thrall->getState(),
            'kousnuti neco udelat MELO');
    }

    /**
     * ⭐ NOVE 11.09.2026 (PHP22). Stavajici sada tyhle tri veci nepokryvala
     *    -- a prave v nich byly vady.
     */
    public function testProneThrallCanBeBitten(): void
    {
        // r. 7938-7939: „adjacent to one or more Thrall team-mates
        //   (**standing, prone or stunned**)". Drive se zadal STANDING.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, skills: [SkillName::Bloodlust])
            ->addPlayer(TeamSide::HOME, 6, 7, id: 2)
            ->withBallOffPitch()
            ->build();
        $state = $state->withPlayer(
            $state->requirePlayer(2)->withState(PlayerState::PRONE),
        );

        // SEBEKONTROLA: Thrall OPRAVDU lezi -- jinak by test nemeril vyjimku.
        $this->assertSame(PlayerState::PRONE, $state->requirePlayer(2)->getState(),
            'fixtura je vadna: Thrall stoji');

        $dice = new FixedDiceRoller([1, 2, 2]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 5, 'y' => 8,
        ]);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('bloodlust_bite', $types,
            'lezici Thrall se kousnout DA (r. 7938-7939)');
        $this->assertFalse($result->isTurnover(),
            'Thrall bez mice turnover nedela (r. 7941-7942)');
    }

    public function testBitingTheBallCarryingThrallIsATurnover(): void
    {
        // r. 7941-7942: „The injury will not cause a turnover **unless the
        //   Thrall was holding the ball**."
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, skills: [SkillName::Bloodlust])
            ->addPlayer(TeamSide::HOME, 6, 7, id: 2)
            ->withBallCarried(2)
            ->build();

        $this->assertSame(2, $state->getBall()->getCarrierId(),
            'fixtura je vadna: mic nenese Thrall');

        $dice = new FixedDiceRoller([1, 2, 2, 3]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 4, 'y' => 7,
        ]);

        $this->assertTrue($result->isTurnover(),
            'kousnuty Thrall drzel mic => turnover (r. 7941-7942)');
    }

    public function testVampireWithoutThrallDropsTheBall(): void
    {
        // r. 7945-7947: „**If he was holding the ball, it bounces** from the
        //   square he occupied when he was removed."
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, skills: [SkillName::Bloodlust])
            ->withBallCarried(1)
            ->build();

        $this->assertSame(1, $state->getBall()->getCarrierId(),
            'fixtura je vadna: mic nenese upir');

        $dice = new FixedDiceRoller([1, 3]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 4, 'y' => 7,
        ]);

        $after = $result->getNewState();
        $this->assertTrue($result->isTurnover());
        $this->assertSame(PlayerState::OFF_PITCH, $after->requirePlayer(1)->getState(),
            'upir jde do rezerv');
        $this->assertFalse($after->getBall()->isHeld(),
            'mic se ma odrazit, ne odejit s upirem');
        $this->assertTrue($after->getBall()->isOnPitch(),
            'mic zustava na hristi');
    }

    /** Krmi se az NA KONCI akce (r. 7934-7936): upir bez Thralla na zacatku dobehne k nemu a nakrmi se. */
    public function testHungryVampireRunsToThrallAndFeedsAtEnd(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, skills: [SkillName::Bloodlust])
            ->addPlayer(TeamSide::HOME, 9, 7, id: 2) // Thrall -- na zacatku NENI vedle
            ->withBallOffPitch()
            ->build();

        // Bloodlust 1; tah na (8,7) vedle Thralla; kousnuti 2+2
        $r = (new ActionResolver(new FixedDiceRoller([1, 2, 2])))->resolve($state, ActionType::MOVE, ['playerId' => 1, 'x' => 8, 'y' => 7]);

        $types = array_map(fn($e) => $e->getType(), $r->getEvents());
        $this->assertContains('bloodlust_bite', $types);
        $this->assertFalse($r->isTurnover());
        $this->assertFalse($r->getNewState()->requirePlayer(1)->isBloodlustHungry());
    }

    /** ... a kdo od Thralla ODEJDE, nakrmit se nema kde: rezervy + turnover. */
    public function testHungryVampireMovingAwayFromThrallFails(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, skills: [SkillName::Bloodlust])
            ->addPlayer(TeamSide::HOME, 6, 7, id: 2) // Thrall vedle na ZACATKU
            ->withBallOffPitch()
            ->build();

        $r = (new ActionResolver(new FixedDiceRoller([1])))->resolve($state, ActionType::MOVE, ['playerId' => 1, 'x' => 2, 'y' => 7]);

        $this->assertTrue($r->isTurnover());
        $this->assertSame(PlayerState::OFF_PITCH, $r->getNewState()->requirePlayer(1)->getState());
    }

    /** Ohlaseny BLOCK bez Thralla vedle: blok se neprovede, upir smi misto nej tahnout (r. 7927-7928). */
    public function testHungryBlockWithoutThrallBecomesMoveOption(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, skills: [SkillName::Bloodlust])
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 3)
            ->addPlayer(TeamSide::HOME, 5, 10, id: 2) // Thrall o kus dal
            ->withBallOffPitch()
            ->build();

        $r = (new ActionResolver(new FixedDiceRoller([1])))->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 3]);

        $types = array_map(fn($e) => $e->getType(), $r->getEvents());
        $this->assertNotContains('block', $types);
        $upir = $r->getNewState()->requirePlayer(1);
        $this->assertTrue($upir->isBloodlustHungry());
        $this->assertFalse($upir->hasActed(), 'muze jeste tahnout k Thrallovi');
    }
}
