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
        $pos = $newState->getPlayer(1)->getPosition();
        $this->assertNotNull($pos);
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
        // Bloodlust: 1 (fail) → kousnuti → zraneni 2+2=4 (Stunned) → move
        $dice = new FixedDiceRoller([1, 2, 2]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1,
            'x' => 4,
            'y' => 7,
        ]);

        $this->assertTrue($result->isSuccess());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('bloodlust_bite', $types);

        $newState = $result->getNewState();
        // ⛔ Drive se tu tvrdilo KO a `getPosition() === null` -- to bylo
        //   z auto-KO. Hod na zraneni 2+2=4 dava STUNNED, tedy hrace, ktery
        //   NA HRISTI ZUSTAVA. Presne v tom je ta oprava: kousnuti neposila
        //   Thralla pryc, jen ho zrani.
        $thrall = $newState->getPlayer(2);
        $this->assertSame(PlayerState::STUNNED, $thrall->getState(),
            'hod 2+2=4 je Stunned (r. 7939-7941)');
        $this->assertNotNull($thrall->getPosition(),
            'omraceny Thrall zustava na hristi');

        // Vampire still moved
        $vampirePos = $newState->getPlayer(1)->getPosition();
        $this->assertNotNull($vampirePos);
        $this->assertEquals(4, $vampirePos->getX());
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
        $vampire = $newState->getPlayer(1);
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
        $dice = new FixedDiceRoller([1, 6, 6, 6, 4, 4]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BLOCK, [
            'playerId' => 1,
            'targetId' => 3,
        ]);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('bloodlust_bite', $types);
        $this->assertContains('block', $types);

        // Thrall dostal hod na zraneni -- a z kousnuti se NEUMIRA.
        $thrall = $result->getNewState()->getPlayer(2);
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
            $state->getPlayer(2)->withState(PlayerState::PRONE),
        );

        // SEBEKONTROLA: Thrall OPRAVDU lezi -- jinak by test nemeril vyjimku.
        $this->assertSame(PlayerState::PRONE, $state->getPlayer(2)->getState(),
            'fixtura je vadna: Thrall stoji');

        $dice = new FixedDiceRoller([1, 2, 2]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 4, 'y' => 7,
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
        $this->assertSame(PlayerState::OFF_PITCH, $after->getPlayer(1)->getState(),
            'upir jde do rezerv');
        $this->assertFalse($after->getBall()->isHeld(),
            'mic se ma odrazit, ne odejit s upirem');
        $this->assertTrue($after->getBall()->isOnPitch(),
            'mic zustava na hristi');
    }
}
