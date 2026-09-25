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

final class BombThrowTest extends TestCase
{

    /**
     * Inaccurate bomb scatters 3 times from target.
     */
    public function testInaccurateBombScatters(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, agility: 2, skills: [SkillName::Bombardier])
            // AG2: accuracy = 7-2+0-1 = 4+ (need 4+)
            ->withBallOffPitch()
            ->build();

        // Accuracy roll: 3 (< 4, inaccurate but not fumble)
        // 3 scatter D8s: 3 (East), 3 (East), 3 (East)
        // Target (8,7) → (9,7) → (10,7) → (11,7)
        // No players at bomb landing — no explosion effects
        $dice = new FixedDiceRoller([3, 3, 3, 3]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BOMB_THROW, [
            'playerId' => 1,
            'targetX' => 8,
            'targetY' => 7,
        ]);

        $this->assertFalse($result->isTurnover());
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('bomb_throw', $types);
        $this->assertContains('bomb_landing', $types);
    }




    /**
     * Armor roll on knocked-down players — armor broken causes injury.
     */
    public function testBombArmorRollCausesInjury(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, skills: [SkillName::Bombardier])
            ->addPlayer(TeamSide::AWAY, 8, 7, id: 2, armour: 6) // low armor
            ->withBallOffPitch()
            ->build();

        // Accuracy: 5 (accurate)
        // Player 2 armor: 6+6=12 vs AV6 → broken
        // Injury: 4+4=8 → KO
        $dice = new FixedDiceRoller([5, 6, 6, 4, 4]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::BOMB_THROW, [
            'playerId' => 1,
            'targetX' => 8,
            'targetY' => 7,
        ]);

        $newState = $result->getNewState();
        $this->assertEquals(PlayerState::KO, $newState->requirePlayer(2)->getState());
    }

    /**
     * `rules_bb2016.txt` r. 7948-7975 (Bombardier). Poradi kostek pri vybuchu:
     * pole 3x3 po sloupcich (x-1..x+1), v kazdem shora dolu; soused hazi D6 (4+),
     * stred ne.
     *
     * @param list<int> $kostky
     */
    private function hod(\App\DTO\GameState $s, array $kostky, int $x = 8, int $y = 7): \App\DTO\ActionResult
    {
        return (new ActionResolver(new FixedDiceRoller($kostky)))->resolve($s, ActionType::BOMB_THROW, [
            'playerId' => 1, 'targetX' => $x, 'targetY' => $y,
        ]);
    }

    public function testAccurateBombKnocksDownCenterAlwaysAdjacentOnFourPlus(): void
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, skills: [SkillName::Bombardier])
            ->addPlayer(TeamSide::AWAY, 8, 7, id: 2)
            ->addPlayer(TeamSide::AWAY, 9, 7, id: 3)
            ->addPlayer(TeamSide::AWAY, 9, 8, id: 4)
            ->withBallOffPitch()
            ->build();

        // presnost 5; stred (8,7): brneni 4+4; (9,7): D6 4 => zasah, brneni 4+4; (9,8): D6 3 => vedle
        $r = $this->hod($s, [5, 4, 4, 4, 4, 4, 3]);

        $this->assertFalse($r->isTurnover(), 'srazeni jen soupere');
        $this->assertSame(PlayerState::PRONE, $r->getNewState()->requirePlayer(2)->getState());
        $this->assertSame(PlayerState::PRONE, $r->getNewState()->requirePlayer(3)->getState());
        $this->assertSame(PlayerState::STANDING, $r->getNewState()->requirePlayer(4)->getState(), 'soused na 3 zustava stat');
    }

    /** Fumble vybuchne v poli HAZECE -- a je to turnover (r. 7956-7957, 7967-7968). */
    public function testFumbleExplodesInThrowersSquareAndTurnsOver(): void
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, skills: [SkillName::Bombardier])
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2)
            ->withBallOffPitch()
            ->build();

        // fumble 1; stred = hazec (5,7): brneni 4+4; (6,7): D6 4 => brneni 4+4
        $r = $this->hod($s, [1, 4, 4, 4, 4, 4]);

        $this->assertTrue($r->isTurnover());
        $this->assertSame(PlayerState::PRONE, $r->getNewState()->requirePlayer(1)->getState());
        $this->assertSame(PlayerState::PRONE, $r->getNewState()->requirePlayer(2)->getState());
    }

    /** Srazeny vlastni hrac = turnover (r. 7956-7957). */
    public function testOwnPlayerKnockedDownIsTurnover(): void
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, skills: [SkillName::Bombardier])
            ->addPlayer(TeamSide::HOME, 9, 7, id: 2)
            ->withBallOffPitch()
            ->build();

        // presnost 5; stred (8,7) prazdny; (9,7): D6 4 => brneni 4+4
        $r = $this->hod($s, [5, 4, 4, 4]);

        $this->assertTrue($r->isTurnover());
    }

    /** Hazec vedle vybuchu neni imunni. */
    public function testThrowerAdjacentToExplosionCanBeHit(): void
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, skills: [SkillName::Bombardier])
            ->withBallOffPitch()
            ->build();

        // cil (6,7) quick; presnost 5; (5,7) hazec: D6 4 => brneni 4+4
        $r = $this->hod($s, [5, 4, 4, 4], 6, 7);

        $this->assertSame(PlayerState::PRONE, $r->getNewState()->requirePlayer(1)->getState());
        $this->assertTrue($r->isTurnover());
    }

    /** "treated as Knocked Down even if already Prone or Stunned" -- lezici dostane hod na brneni. */
    public function testPronePlayerInCenterGetsArmourRoll(): void
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, skills: [SkillName::Bombardier])
            ->addPronePlayer(TeamSide::AWAY, 8, 7, id: 2)
            ->withBallOffPitch()
            ->build();

        // presnost 5; stred: brneni 6+6 prolomeno; zraneni 3+3 => stunned
        $r = $this->hod($s, [5, 6, 6, 3, 3]);

        $this->assertSame(PlayerState::STUNNED, $r->getNewState()->requirePlayer(2)->getState());
    }

    /** Bomba NESPOTREBUJE akci Pass a nebrani ji ani pouzita prihravka (r. 7950-7951). */
    public function testBombDoesNotUseTeamPassAction(): void
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, skills: [SkillName::Bombardier])
            ->withBallOffPitch()
            ->build();
        $rules = new \App\Engine\RulesEngine();
        $poPrihravce = $s->withTeamState(TeamSide::HOME, $s->getTeamState(TeamSide::HOME)->withPassUsed());

        $this->assertSame([], $rules->validate($poPrihravce, ActionType::BOMB_THROW, ['playerId' => 1, 'targetX' => 8, 'targetY' => 7]));

        $r = $this->hod($s, [5]);
        $this->assertFalse($r->getNewState()->getTeamState(TeamSide::HOME)->isPassUsedThisTurn());
    }

    /** Pred hodem se nesmi hnout (r. 7953-7954). */
    public function testBombardierMayNotMoveBeforeThrowing(): void
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, skills: [SkillName::Bombardier])
            ->withBallOffPitch()
            ->build();
        $s = $s->withPlayer($s->requirePlayer(1)->withHasMoved(true));

        $this->assertNotSame([], (new \App\Engine\RulesEngine())->validate($s, ActionType::BOMB_THROW, ['playerId' => 1, 'targetX' => 8, 'targetY' => 7]));
    }
}
