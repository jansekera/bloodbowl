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
        $dice = new FixedDiceRoller([5, 1, 6, 6, 4, 4]);   // P264: 1 = hráč 2 v cíli bombu nechytí
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
        $r = $this->hod($s, [5, 1, 4, 4, 4, 4, 4, 3]);   // P264: 1 = hráč 2 v cíli nechytí

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
            ->withHomeTeam(\App\DTO\TeamStateDTO::create(1, 'Home', 'Human', TeamSide::HOME, 0))
            ->withBallOffPitch()
            ->build();

        // fumble 1 (bez prehozu, P251); stred = hazec (5,7): brneni 4+4; (6,7): D6 4 => brneni 4+4
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
        $s = (new ActionResolver(new FixedDiceRoller([])))
            ->resolve($s, ActionType::MOVE, ['playerId' => 1, 'x' => 6, 'y' => 7])
            ->getNewState();

        $this->assertNotSame([], (new \App\Engine\RulesEngine())->validate($s, ActionType::BOMB_THROW, ['playerId' => 1, 'targetX' => 8, 'targetY' => 7]));
    }

    /**
     * Review P186 (H1, týž princip): r. 7953-7954 "the player may not move or stand up
     * before throwing it". Pohyb, který skončil úspěšným přehozem v dialogu, je pohyb --
     * dialog ho jen neukončí (`hasMoved` zůstává false, člověk smí táhnout dál).
     */
    public function testBombardierMayNotThrowAfterMoveRerolledInDialog(): void
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, skills: [SkillName::Bombardier])
            ->addPlayer(TeamSide::AWAY, 4, 7, id: 2)
            ->withBallOffPitch()
            ->build();
        $rules = new \App\Engine\RulesEngine();
        $hod = ['playerId' => 1, 'targetX' => 9, 'targetY' => 7];
        $this->assertSame([], $rules->validate($s, ActionType::BOMB_THROW, $hod), 'pozitivní kontrola: před pohybem házet smí');

        // Úhyb ze zóny AWAY 2: 1 = neúspěch ⇒ dialog, týmový přehoz 6 = úspěch.
        $resolver = new ActionResolver(new FixedDiceRoller([1, 6]));
        $resolver->setInteractiveRerolls(true);
        $r = $resolver->resolve($s, ActionType::MOVE, ['playerId' => 1, 'x' => 6, 'y' => 7]);
        $r = $resolver->resolve($r->getNewState(), ActionType::RESOLVE_REROLL, ['choice' => 'team_reroll']);
        $this->assertSame(6, $r->getNewState()->requirePlayer(1)->requirePosition()->getX(), 'fixtura: hráč se přesunul');

        $this->assertNotSame([], $rules->validate($r->getNewState(), ActionType::BOMB_THROW, $hod));
        $nabidka = array_filter(
            $rules->getAvailableActions($r->getNewState()),
            fn(array $a) => $a['type'] === ActionType::BOMB_THROW->value,
        );
        $this->assertSame([], $nabidka, 'po pohybu se hod bombou nenabízí');
    }

    // ===== P251 (6.11.2026, uzivatel: „bombu jen na fumble“) =====
    // R. 7957-7958 (dovednosti hodu mice platí i pro bombu), 8336-8337 (Pass), 8385-8387 (Pro),
    // 929-936 (tymovy prehoz 1x za kolo), 8287-8290 (Loner), 925-927 (jeden prehoz na hod).
    // Hazec (5,7), cil (8,7) quick, AG3 => cil hodu 3+. Hod 1 = fumble.

    /** @param list<SkillName> $skills */
    private function hazec(array $skills = [], int $rerolls = 2, bool $used = false): \App\DTO\GameState
    {
        $team = \App\DTO\TeamStateDTO::create(1, 'Home', 'Human', TeamSide::HOME, $rerolls);
        if ($used) {
            $team = $team->withRerollUsed();
        }

        return (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, skills: array_merge([SkillName::Bombardier], $skills))
            ->addPlayer(TeamSide::AWAY, 8, 7, id: 2)
            ->withHomeTeam($team)
            ->withBallOffPitch()
            ->build();
    }

    public function testFumbleIsRerolledByTeamReroll(): void
    {
        // fumble 1, tymovy prehoz => 5 presne; stred (8,7): brneni 3+3 neprorazi
        $r = $this->hod($this->hazec(), [1, 5, 1, 3, 3]);   // P264: 1 = hráč 2 v cíli nechytí

        $home = $r->getNewState()->getTeamState(TeamSide::HOME);
        $this->assertSame(1, $home->getRerolls(), 'tymovy prehoz utracen');
        $this->assertTrue($home->isRerollUsedThisTurn());
        $this->assertSame(PlayerState::STANDING, $r->getNewState()->requirePlayer(1)->getState());
        $this->assertSame(PlayerState::PRONE, $r->getNewState()->requirePlayer(2)->getState());
        $this->assertFalse($r->isTurnover());
    }

    public function testInaccurateBombIsNotRerolled(): void
    {
        // Volba uzivatele: nepresna se neprehazuje. (Hlidaci test: projde i na starem kodu.)
        $r = $this->hod($this->hazec(), [2, 3, 3, 3]);

        $this->assertSame(2, $r->getNewState()->getTeamState(TeamSide::HOME)->getRerolls());
        $this->assertFalse($r->getNewState()->getTeamState(TeamSide::HOME)->isRerollUsedThisTurn());
    }

    public function testPassSkillRerollsFumbleWithoutSpendingTeamReroll(): void
    {
        $r = $this->hod($this->hazec([SkillName::Pass]), [1, 5, 1, 3, 3]);   // P264: 1 = nechytí

        $this->assertSame(2, $r->getNewState()->getTeamState(TeamSide::HOME)->getRerolls());
        $this->assertSame(PlayerState::STANDING, $r->getNewState()->requirePlayer(1)->getState());
        $this->assertSame(PlayerState::PRONE, $r->getNewState()->requirePlayer(2)->getState());
        $this->assertFalse($r->isTurnover());
    }

    public function testProRerollsFumbleBeforeTeamReroll(): void
    {
        // fumble, Pro 4 projde, novy hod 5
        $r = $this->hod($this->hazec([SkillName::Pro]), [1, 4, 5, 1, 3, 3]);   // P264: 1 = nechytí

        $this->assertTrue($r->getNewState()->requirePlayer(1)->isProUsedThisTurn());
        $this->assertSame(2, $r->getNewState()->getTeamState(TeamSide::HOME)->getRerolls());
        $this->assertSame(PlayerState::PRONE, $r->getNewState()->requirePlayer(2)->getState());
        $this->assertFalse($r->isTurnover());
    }

    public function testFumbleStandsWhenTeamRerollAlreadyUsed(): void
    {
        // (Hlidaci test.) Fumble: stred = hazec, brneni 2+2.
        $r = $this->hod($this->hazec(used: true), [1, 2, 2]);

        $this->assertSame(PlayerState::PRONE, $r->getNewState()->requirePlayer(1)->getState());
        $this->assertTrue($r->isTurnover());
    }

    public function testLonerBomberDoesNotSpendTeamRerollOnFumble(): void
    {
        // Uzivatel 6.11.: „jestli ma bomber loner - tak se prikloň k neprehozeni ani fumble“.
        $r = $this->hod($this->hazec([SkillName::Loner]), [1, 2, 2]);

        $this->assertSame(2, $r->getNewState()->getTeamState(TeamSide::HOME)->getRerolls());
        $this->assertSame(PlayerState::PRONE, $r->getNewState()->requirePlayer(1)->getState());
        $this->assertTrue($r->isTurnover());
    }

    public function testLonerBomberWithPassSkillStillRerollsFumble(): void
    {
        $r = $this->hod($this->hazec([SkillName::Loner, SkillName::Pass]), [1, 5, 1, 3, 3]);   // P264: 1 = nechytí

        $this->assertSame(2, $r->getNewState()->getTeamState(TeamSide::HOME)->getRerolls());
        $this->assertSame(PlayerState::PRONE, $r->getNewState()->requirePlayer(2)->getState());
        $this->assertFalse($r->isTurnover());
    }

    /**
     * P256 (review dávky 7.11.2026): bomba = hod míčem (r. 7952-7958) ⇒ Strong Arm (r. 8522-8524) i omezení vánice.
     * Dřív PHP bomba Strong Arm nebrala a ve vánici šla i dlouhá.
     */
    public function testP256StrongArmPomahaKratkeBombe(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, skills: [SkillName::Bombardier, SkillName::StrongArm])
            ->addPlayer(TeamSide::AWAY, 10, 7, id: 2)
            ->withBallOffPitch()
            ->build();
        // 5 polí = krátká (cíl 4+); Strong Arm ⇒ jako rychlá (3+): hod 3 = přesná ⇒ výbuch na (10,7)
        $result = (new ActionResolver(new FixedDiceRoller([3, 1, 1, 1, 1])))->resolve($state, ActionType::BOMB_THROW, [
            'playerId' => 1, 'targetX' => 10, 'targetY' => 7,
        ]);
        $this->assertNotSame(\App\Enum\PlayerState::STANDING, $result->getNewState()->requirePlayer(2)->getState());
    }

    public function testP256VeVaniciDlouhaBombaNejde(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, skills: [SkillName::Bombardier])
            ->withWeather(\App\Enum\Weather::BLIZZARD)
            ->withBallOffPitch()
            ->build();
        $this->expectException(\InvalidArgumentException::class);
        (new ActionResolver(new FixedDiceRoller([5, 5, 5, 5])))->resolve($state, ActionType::BOMB_THROW, [
            'playerId' => 1, 'targetX' => 14, 'targetY' => 7,
        ]);
    }

    // ───── P263 (uživatel 7.11.2026: „OK“): Hail Mary u bomby — parita s C++ BombHandler.P263* ─────
    // R. 7952-7954: bomba se hází „using the rules for throwing the ball (including … use of Hail Mary Pass)“.
    // R. 8164-8169: cíl kdekoli, pravítko ne; D6: 1 = fumble (výbuch u házeče, r. 7966-7967), 2-6 projde,
    // ale „never accurate“ ⇒ vždy 3 rozptyly od cíle; bez modifikátorů. Ve vánici ne (r. 8170).
    private function bombaHmp(?\App\Enum\Weather $pocasi = null, int $prehozy = 3): \App\DTO\GameState
    {
        $b = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 3, 7, id: 1, skills: [SkillName::Bombardier, SkillName::HailMaryPass])
            ->addPlayer(TeamSide::AWAY, 23, 7, id: 12)      // 20 polí — mimo pravítko
            ->withHomeTeam(\App\DTO\TeamStateDTO::create(1, 'Home', 'Human', TeamSide::HOME, $prehozy))
            ->withBallOffPitch();
        if ($pocasi !== null) {
            $b = $b->withWeather($pocasi);
        }
        return $b->build();
    }

    public function testP263HailMaryBombaSeRozptyliTrikratOdCile(): void
    {
        // hod 2 (projde); D8=3 (V) ×3 z (20,7) → (23,7); P264: 12 chytá, 1 = nechytí ⇒ výbuch na hráči 12; zbroj 3+3
        $result = (new ActionResolver(new FixedDiceRoller([2, 3, 3, 3, 1, 3, 3])))->resolve($this->bombaHmp(), ActionType::BOMB_THROW, [
            'playerId' => 1, 'targetX' => 20, 'targetY' => 7,
        ]);
        $this->assertFalse($result->isTurnover());
        $this->assertSame(PlayerState::PRONE, $result->getNewState()->requirePlayer(12)->getState());
    }

    public function testP263HailMaryBombaNikdyNeniPresna(): void
    {
        // i šestka se rozptýlí: D8=7 (Z) ×3 z (23,7) → (20,7) ⇒ hráč 12 zůstane stát
        // review 9.11. (M3): kontrola spotřeby kostek — s přesnou šestkou by 12 bombu chytil a hodil jinam
        $kostky = new FixedDiceRoller([6, 7, 7, 7]);
        $result = (new ActionResolver($kostky))->resolve($this->bombaHmp(), ActionType::BOMB_THROW, [
            'playerId' => 1, 'targetX' => 23, 'targetY' => 7,
        ]);
        $this->assertFalse($kostky->hasRemainingRolls());
        $this->assertSame(PlayerState::STANDING, $result->getNewState()->requirePlayer(12)->getState());
    }

    public function testP263HailMaryBombaFumbleNaJednickuVybuchneUHazece(): void
    {
        // 1 = fumble, bez přehozů ⇒ výbuch u házeče; zbroj 3+3
        $result = (new ActionResolver(new FixedDiceRoller([1, 3, 3])))->resolve($this->bombaHmp(prehozy: 0), ActionType::BOMB_THROW, [
            'playerId' => 1, 'targetX' => 20, 'targetY' => 7,
        ]);
        $this->assertTrue($result->isTurnover());
        $this->assertSame(PlayerState::PRONE, $result->getNewState()->requirePlayer(1)->getState());
    }

    public function testP263VeVaniciHailMaryBombaNejde(): void
    {
        $this->expectException(\InvalidArgumentException::class);
        (new ActionResolver(new FixedDiceRoller([5, 5, 5, 5])))->resolve($this->bombaHmp(\App\Enum\Weather::BLIZZARD), ActionType::BOMB_THROW, [
            'playerId' => 1, 'targetX' => 20, 'targetY' => 7,
        ]);
    }

    // ───── P264 (uživatel 9.11.2026: „přidej možnost intercept“): zachycení a chycení bomby; parita s C++ ─────
    // R. 7959-7966: bombu lze zachytit nebo chytit jako míč; kdo ji má, hned ji hodí dál; nepovedené chycení nebo
    // odmítnutí ⇒ výbuch. Zachycení před hodem (r. 1773-1774), nepovedené ⇒ letí dál (uživatel). Hail Mary zachytit
    // nejde (r. 8167). Zachycená bomba není turnover (r. 7955), sražený hráč týmu na tahu ano (r. 7956-7958).
    private function bombaP264(bool $zachycujici = false, int $zx = 8, int $zy = 7, int $cilX = 10, bool $bezRukou = false): \App\DTO\GameState
    {
        // $bezRukou: házeč nechytá ani nezachytává ⇒ hod zpět míří přímo na něj (review M2: jinak vedle něj)
        $b = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, skills: $bezRukou ? [SkillName::Bombardier, SkillName::NoHands] : [SkillName::Bombardier])
            ->addPlayer(TeamSide::AWAY, $cilX, 7, id: 12)
            ->withHomeTeam(\App\DTO\TeamStateDTO::create(1, 'Home', 'Human', TeamSide::HOME, 0))
            ->withBallOffPitch();
        if ($zachycujici) {
            $b = $b->addPlayer(TeamSide::AWAY, $zx, $zy, id: 13);
        }
        return $b->build();
    }

    public function testP264ZachycenaBombaLetiZpetNaNas(): void
    {
        // 6 = 13 zachytil (6+) ⇒ hází na 1 (5,7): rychlá 3+, hod 6 ⇒ výbuch v (5,7); zbroj 3+3
        $kostky = new FixedDiceRoller([6, 6, 3, 3]);
        $r = (new ActionResolver($kostky))->resolve($this->bombaP264(true, bezRukou: true), ActionType::BOMB_THROW, ['playerId' => 1, 'targetX' => 10, 'targetY' => 7]);
        $this->assertSame(PlayerState::STANDING, $r->getNewState()->requirePlayer(12)->getState());
        $this->assertSame(PlayerState::PRONE, $r->getNewState()->requirePlayer(1)->getState());
        $this->assertTrue($r->isTurnover());
        $this->assertFalse($kostky->hasRemainingRolls());
    }

    public function testP264NepovedeneZachyceniBombaLetiDal(): void
    {
        // 1 = zachycení selhalo; hod 6 přesně; 12 chytá (3+), hodí 1 ⇒ výbuch v (10,7); zbroj 3+3
        $r = (new ActionResolver(new FixedDiceRoller([1, 6, 1, 3, 3])))->resolve($this->bombaP264(true), ActionType::BOMB_THROW, ['playerId' => 1, 'targetX' => 10, 'targetY' => 7]);
        $this->assertSame(PlayerState::PRONE, $r->getNewState()->requirePlayer(12)->getState());
        $this->assertSame(PlayerState::STANDING, $r->getNewState()->requirePlayer(13)->getState());
        $this->assertFalse($r->isTurnover());
    }

    public function testP264NezachycujeKomuVybuchNehrozi(): void
    {
        // cíl prázdné (10,10), 12 daleko; 13 v dráze (8,9), od cíle 2 pole ⇒ nezkouší; jen hod 6
        $s = $this->bombaP264(true, 8, 9, 20);
        $kostky = new FixedDiceRoller([6]);
        $r = (new ActionResolver($kostky))->resolve($s, ActionType::BOMB_THROW, ['playerId' => 1, 'targetX' => 10, 'targetY' => 10]);
        $this->assertFalse($r->isTurnover());
        $this->assertFalse($kostky->hasRemainingRolls());
    }

    public function testP264ChycenouBombuChytajiciHodiDal(): void
    {
        // hod 6 přesně; 12 chytá (3+) hodí 6 ⇒ hází na 1 (5,7): krátká 4+, hod 6 ⇒ výbuch; zbroj 3+3
        $kostky = new FixedDiceRoller([6, 6, 6, 3, 3]);
        $r = (new ActionResolver($kostky))->resolve($this->bombaP264(bezRukou: true), ActionType::BOMB_THROW, ['playerId' => 1, 'targetX' => 10, 'targetY' => 7]);
        $this->assertSame(PlayerState::STANDING, $r->getNewState()->requirePlayer(12)->getState());
        $this->assertSame(PlayerState::PRONE, $r->getNewState()->requirePlayer(1)->getState());
        $this->assertTrue($r->isTurnover());
        $this->assertFalse($kostky->hasRemainingRolls());
    }

    public function testP264HailMaryBombuNelzeZachytit(): void
    {
        // jako testP263HailMaryBombaSeRozptyliTrikratOdCile, navíc soupeř 13 v dráze vedle cíle (19,7)
        $s2 = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 3, 7, id: 1, skills: [SkillName::Bombardier, SkillName::HailMaryPass])
            ->addPlayer(TeamSide::AWAY, 23, 7, id: 12)
            ->addPlayer(TeamSide::AWAY, 19, 7, id: 13)
            ->withBallOffPitch()
            ->build();
        $r = (new ActionResolver(new FixedDiceRoller([2, 3, 3, 3, 1, 3, 3])))->resolve($s2, ActionType::BOMB_THROW, ['playerId' => 1, 'targetX' => 20, 'targetY' => 7]);
        $this->assertSame(PlayerState::PRONE, $r->getNewState()->requirePlayer(12)->getState());
        $this->assertSame(PlayerState::STANDING, $r->getNewState()->requirePlayer(13)->getState());
    }
}
