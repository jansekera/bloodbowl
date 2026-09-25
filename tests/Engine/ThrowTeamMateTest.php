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

final class ThrowTeamMateTest extends TestCase
{
    public function testAccurateThrowWithSuccessfulLanding(): void
    {
        // Thrower AG3, target at (8,5) short range. Projectile AG3.
        // Accuracy: 7-3-1(short)=3+. Roll 4=accurate.
        // Landing: 7-3=4+. Roll 4=success.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 5, skills: [SkillName::ThrowTeamMate], id: 1)
            ->addPlayer(TeamSide::HOME, 6, 5, skills: [SkillName::RightStuff], id: 2)
            ->withBallOffPitch()
            ->build();

        // ⛔ PREPSANO 21.09.2026 (r. 8609-8611): i PRESNY hod se resi jako
        //   nepresny, tedy tri rozptyly. Drive tu bylo `[4, 4]` a ocekavalo se
        //   pristani presne na (8,5) -- to bylo zakotveni vady.
        //   Rozptyly D8=3 (vychod) x3 z (8,5) => (11,5).
        $dice = new FixedDiceRoller([4, 3, 3, 3, 4]); // presnost, 3x rozptyl, pristani
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::THROW_TEAM_MATE, [
            'playerId' => 1, 'targetId' => 2, 'targetX' => 8, 'targetY' => 5,
        ]);

        $this->assertFalse($result->isTurnover());
        $newState = $result->getNewState();
        $landed = $newState->requirePlayer(2);
        $this->assertSame(11, $landed->requirePosition()->getX());
        $this->assertSame(5, $landed->requirePosition()->getY());
        $this->assertSame(PlayerState::STANDING, $landed->getState());
    }

    public function testInaccurateThrowScattersAndLands(): void
    {
        // Inaccurate: scatters 1 square from target
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 5, skills: [SkillName::ThrowTeamMate], id: 1)
            ->addPlayer(TeamSide::HOME, 6, 5, skills: [SkillName::RightStuff], id: 2)
            ->withBallOffPitch()
            ->build();

        // Accuracy: 7-3-1=3+. Roll 2=inaccurate.
        // ⛔ PREPSANO 21.09.2026 (r. 8610-8611): rozptyl je TRIKRAT, ne jednou.
        // 3x D8=3 (vychod) z (8,5) => (11,5). Landing: 7-3=4+. Roll 4=success.
        $dice = new FixedDiceRoller([2, 3, 3, 3, 4]); // presnost, 3x rozptyl, pristani
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::THROW_TEAM_MATE, [
            'playerId' => 1, 'targetId' => 2, 'targetX' => 8, 'targetY' => 5,
        ]);

        $this->assertFalse($result->isTurnover());
        $landed = $result->getNewState()->requirePlayer(2);
        // Tri pole na vychod od (8,5) → (11,5)
        $this->assertSame(11, $landed->requirePosition()->getX());
    }

    public function testFumbleNechaHraceNaJehoPoli(): void
    {
        // ⛔ PREJMENOVANO 21.09.2026 (bylo `testFumbleScattersFromThrower`):
        //   r. 8613 rika, ze fumblovany hrac zustava na svem PUVODNIM poli,
        //   nerozptyluje se od hazece. Test sam mereni nezmenil -- kontroluje
        //   jen, ze akce probehne a vyda udalosti.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 5, skills: [SkillName::ThrowTeamMate], id: 1)
            ->addPlayer(TeamSide::HOME, 6, 5, skills: [SkillName::RightStuff], id: 2)
            ->withBallOffPitch()
            ->build();

        // Fumble roll=1 => hrac zustava na (6,5); landing: 7-3=4+, roll=1=fail, armor
        $dice = new FixedDiceRoller([1, 1, 2, 1]); // fumble, pristani (neuspech), brneni d1, d2
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::THROW_TEAM_MATE, [
            'playerId' => 1, 'targetId' => 2, 'targetX' => 8, 'targetY' => 5,
        ]);

        $this->assertFalse($result->isTurnover()); // TTM itself is not a turnover

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('throw_team_mate', $types);
        $this->assertContains('ttm_landing', $types);
    }

    public function testFailedLandingCausesProneAndArmor(): void
    {
        // Failed landing roll → prone + armor check
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 5, skills: [SkillName::ThrowTeamMate], id: 1)
            ->addPlayer(TeamSide::HOME, 6, 5, skills: [SkillName::RightStuff], id: 2)
            ->withBallOffPitch()
            ->build();

        // Accurate roll=5 (ale r. 8609-8611: i tak 3x rozptyl), 3x D8=3 => (11,5),
        // landing: 7-3=4+, roll=2=fail, armor 3+3=6 not > 8
        $dice = new FixedDiceRoller([5, 3, 3, 3, 2, 3, 3]); // presnost, 3x rozptyl, pristani, brneni
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::THROW_TEAM_MATE, [
            'playerId' => 1, 'targetId' => 2, 'targetX' => 8, 'targetY' => 5,
        ]);

        $landed = $result->getNewState()->requirePlayer(2);
        $this->assertSame(PlayerState::PRONE, $landed->getState());

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('armour_roll', $types);
    }

    public function testThrownPlayerWithBallSuccessful(): void
    {
        // Thrown player carries ball, lands successfully
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 5, skills: [SkillName::ThrowTeamMate], id: 1)
            ->addPlayer(TeamSide::HOME, 6, 5, skills: [SkillName::RightStuff], id: 2)
            ->withBallCarried(2)
            ->build();

        // Accurate roll=4 (r. 8609-8611: i presny = 3x rozptyl), 3x D8=3 => (11,5),
        // landing 4+, roll=4
        $dice = new FixedDiceRoller([4, 3, 3, 3, 4]); // presnost, 3x rozptyl, pristani
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::THROW_TEAM_MATE, [
            'playerId' => 1, 'targetId' => 2, 'targetX' => 8, 'targetY' => 5,
        ]);

        $this->assertFalse($result->isTurnover());
        $ball = $result->getNewState()->getBall();
        $this->assertTrue($ball->isHeld());
        $this->assertSame(2, $ball->getCarrierId());
    }

    public function testFailLandingWithBallBounce(): void
    {
        // Fail landing with ball → ball bounces
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 5, skills: [SkillName::ThrowTeamMate], id: 1)
            ->addPlayer(TeamSide::HOME, 6, 5, skills: [SkillName::RightStuff], id: 2)
            ->withBallCarried(2)
            ->build();

        // Accurate roll=4 (i tak 3x rozptyl D8=3 => (11,5)), landing fail roll=1,
        // armor 2+2=4 not > 8, bounce D8=3
        $dice = new FixedDiceRoller([4, 3, 3, 3, 1, 2, 2, 3]); // presnost, 3x rozptyl, pristani, brneni, odskok
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::THROW_TEAM_MATE, [
            'playerId' => 1, 'targetId' => 2, 'targetX' => 8, 'targetY' => 5,
        ]);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('ball_bounce', $types);
    }

    public function testLandingOnOccupiedSquareScatters(): void
    {
        // Landing on occupied square → scatter to next
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 5, skills: [SkillName::ThrowTeamMate], id: 1)
            ->addPlayer(TeamSide::HOME, 6, 5, skills: [SkillName::RightStuff], id: 2)
            ->addPlayer(TeamSide::AWAY, 7, 5, id: 3) // stoji na poli, kam hrac dorozptyluje
            ->withBallOffPitch()
            ->build();

        // ⛔ PREPSANO 21.09.2026: hod se ted vzdy rozptyluje 3x (r. 8609-8611).
        //   Aby test poradi dal meril to sve -- pristani na OBSAZENE pole --
        //   jsou rozptyly voleny tak, aby hrac skoncil zpatky na (8,5):
        //   D8=3 (vychod) na (9,5), D8=7 (zapad) zpet na (8,5), D8=3 na (9,5)? ne --
        //   volime 3 (vychod) -> (9,5), 7 (zapad) -> (8,5), 7 (zapad) -> (7,5)
        //   a cilem je obsazene pole (7,5), kde stoji hrac 3.
        // Pak rozptyl z obsazeneho pole D8=3 (vychod) -> (8,5), pristani 5.
        $dice = new FixedDiceRoller([4, 3, 7, 7, 3, 5]); // presnost, 3x rozptyl, rozptyl z obsazeneho, pristani
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::THROW_TEAM_MATE, [
            'playerId' => 1, 'targetId' => 2, 'targetX' => 8, 'targetY' => 5,
        ]);

        $landed = $result->getNewState()->requirePlayer(2);
        $this->assertSame(8, $landed->requirePosition()->getX(), 'z obsazeneho (7,5) rozptyl na vychod');
    }

    public function testOffPitchCrowdSurf(): void
    {
        // Inaccurate scatter goes off pitch → crowd surf
        // Thrower near edge, target near edge
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 1, 0, strength: 5, skills: [SkillName::ThrowTeamMate], id: 1)
            ->addPlayer(TeamSide::HOME, 2, 0, skills: [SkillName::RightStuff], id: 2)
            ->withBallOffPitch()
            ->build();

        // Inaccurate: roll=2, scatter D8=1 (North) → off pitch
        // Crowd surf injury dice
        $dice = new FixedDiceRoller([2, 1, 3, 3]); // accuracy, scatter D8=N, injury die1, die2
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::THROW_TEAM_MATE, [
            'playerId' => 1, 'targetId' => 2, 'targetX' => 4, 'targetY' => 0,
        ]);

        // ⛔ PREPSANO 11.09.2026. Puvodne se tu tvrdil TURNOVER -- a byla to
        //   vada, kterou test zakotvoval. Fixtura ma `withBallOffPitch()`,
        //   takze hozeny hrac MIC NEMA.
        //   `rules_bb2016.txt` r. 368-370 (bod 1): „being injured by the crowd
        //   ... **is not a turnover unless it is a player from the active team
        //   holding the ball**." A bod 6 (r. 381-384) mluvi taky jen o hraci
        //   **s micem**.
        $this->assertFalse($result->isTurnover(),
            'hozeny hrac BEZ mice u davu kolo nekonci (r. 368-370)');
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('crowd_surf', $types, 'k davu se dostat MEL');
    }

    /**
     * ⭐ DRUHA PULKA PARU: s micem to turnover JE (bod 6, r. 381-384).
     *    Bez tohohle by test vys prosel i s "nikdy neni turnover".
     */
    public function testOffPitchCrowdSurfWithTheBallIsATurnover(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 1, 0, strength: 5, skills: [SkillName::ThrowTeamMate], id: 1)
            ->addPlayer(TeamSide::HOME, 2, 0, skills: [SkillName::RightStuff], id: 2)
            ->withBallCarried(2)
            ->build();

        // SEBEKONTROLA FIXTURY: hozeny hrac mic OPRAVDU ma.
        $this->assertTrue($state->getBall()->isHeld());
        $this->assertSame(2, $state->getBall()->getCarrierId(),
            'fixtura je vadna: mic nenese hozeny hrac, par nic nemeri');

        // 21.09.: kostek je vic, protoze po opravě nasleduje VHAZENI
        //   (sablona D6 + 2D6 poli + pripadny odskok) -- viz
        //   `testHozenyNosicVDavuVratiMicVhazenim`.
        $dice = new FixedDiceRoller([2, 1, 3, 3, 4, 2, 2, 5, 5, 5, 5, 5]);
        $resolver = new ActionResolver($dice);
        $result = $resolver->resolve($state, ActionType::THROW_TEAM_MATE, [
            'playerId' => 1, 'targetId' => 2, 'targetX' => 4, 'targetY' => 0,
        ]);

        $this->assertTrue($result->isTurnover(),
            'hozeny hrac S MICEM u davu kolo koncí (bod 6)');
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('crowd_surf', $types);
    }

    // ===== Tri nalezy z radku TTM (21.09.2026) =====
    // r. 8606-8610: "The pass is worked out exactly the same as the player with
    //   Throw Team-Mate passing a ball, except the player **must subtract 1 from
    //   the D6 roll** when he passes the player, fumbles are not automatically
    //   turnovers, and **Long Pass or Long Bomb range passes are not possible**."
    // r. 8613: "**A fumbled team-mate will land in the square he originally
    //   occupied.**"

    public function testHodNaPresnostMaMinusJedna(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, strength: 5, skills: [SkillName::ThrowTeamMate], id: 1)
            ->addPlayer(TeamSide::HOME, 6, 7, skills: [SkillName::RightStuff], id: 2)
            ->withBallOffPitch()
            ->build();

        // (5,7) -> (11,7) je 6 poli = SHORT pass (modifikator 0), AG3, bez zon:
        // cil byl 4+, s -1 je 5+. Hod 4 tedy PRED opravou vysel jako "accurate",
        // po oprave je "inaccurate".
        // ⚠️ Quick pass (do 3 poli) ma +1, ktery tu -1 vyrusi -- proto short.
        $dice = new FixedDiceRoller([4, 3, 3, 3, 6]);
        $result = (new ActionResolver($dice))->resolve($state, ActionType::THROW_TEAM_MATE, [
            'playerId' => 1, 'targetId' => 2, 'targetX' => 11, 'targetY' => 7,
        ]);

        $vysledek = null;
        foreach ($result->getEvents() as $e) {
            if ($e->getType() === 'throw_team_mate') { $vysledek = $e->getData()['result'] ?? null; }
        }
        $this->assertSame('inaccurate', $vysledek, 'r. 8608: -1 k hodu na presnost');
    }

    public function testModifikovanaJednickaJeFumble(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, strength: 5, skills: [SkillName::ThrowTeamMate], id: 1)
            ->addPlayer(TeamSide::HOME, 6, 7, skills: [SkillName::RightStuff], id: 2)
            ->withBallOffPitch()
            ->build();

        // Short pass (6 poli), hod 2 a modifikator -1 => 1 => fumble
        // (r. 1742-1745 plati i pro TTM, protoze "the pass is worked out
        // exactly the same"). Pak pristani na puvodnim poli (6 = uspech).
        $dice = new FixedDiceRoller([2, 6]);
        $result = (new ActionResolver($dice))->resolve($state, ActionType::THROW_TEAM_MATE, [
            'playerId' => 1, 'targetId' => 2, 'targetX' => 11, 'targetY' => 7,
        ]);

        $vysledek = null;
        foreach ($result->getEvents() as $e) {
            if ($e->getType() === 'throw_team_mate') { $vysledek = $e->getData()['result'] ?? null; }
        }
        $this->assertSame('fumble', $vysledek);
    }

    public function testFumblovanyHracZustaneNaSvemPuvodnimPoli(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, strength: 5, skills: [SkillName::ThrowTeamMate], id: 1)
            ->addPlayer(TeamSide::HOME, 6, 7, skills: [SkillName::RightStuff], id: 2)
            ->withBallOffPitch()
            ->build();

        // Fumble (prirozena 1). r. 8613: hrac zustava na svem PUVODNIM poli (6,7).
        // Pred opravou se rozptyloval o jedno pole od hazece.
        // Pak uz jen hod na pristani (6 = uspech).
        $dice = new FixedDiceRoller([1, 6]);
        $result = (new ActionResolver($dice))->resolve($state, ActionType::THROW_TEAM_MATE, [
            'playerId' => 1, 'targetId' => 2, 'targetX' => 8, 'targetY' => 7,
        ]);

        $hozeny = $result->getNewState()->requirePlayer(2);
        $this->assertSame(6, $hozeny->getPosition()?->getX(), 'r. 8613: fumble = zustava na svem poli');
        $this->assertSame(7, $hozeny->requirePosition()->getY());
    }

    public function testNaLongPassSeHazetNESMI(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, strength: 5, skills: [SkillName::ThrowTeamMate], id: 1)
            ->addPlayer(TeamSide::HOME, 6, 7, skills: [SkillName::RightStuff], id: 2)
            ->withBallOffPitch()
            ->build();

        // (5,7) -> (13,7) je 8 poli = Long Pass. r. 8609-8610: nejde.
        $this->expectException(\InvalidArgumentException::class);
        (new ActionResolver(new FixedDiceRoller([4, 4])))->resolve($state, ActionType::THROW_TEAM_MATE, [
            'playerId' => 1, 'targetId' => 2, 'targetX' => 13, 'targetY' => 7,
        ]);
    }

    public function testShortPassProjde(): void
    {
        // Pozitivni kontrola k testu vyse: 6 poli = Short Pass, ten povoleny je.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, strength: 5, skills: [SkillName::ThrowTeamMate], id: 1)
            ->addPlayer(TeamSide::HOME, 6, 7, skills: [SkillName::RightStuff], id: 2)
            ->withBallOffPitch()
            ->build();

        $result = (new ActionResolver(new FixedDiceRoller([5, 3, 3, 3, 6])))->resolve($state, ActionType::THROW_TEAM_MATE, [
            'playerId' => 1, 'targetId' => 2, 'targetX' => 11, 'targetY' => 7,
        ]);
        $this->assertTrue($result->isSuccess());
    }

    public function testPresnyHodSeResiJakoNEPRESNY_TRIKRAT_rozptyl(): void
    {
        // ⭐ 21.09.2026: `rules_bb2016.txt` r. 8609-8611: "In addition, **accurate
        //   passes are treated instead as inaccurate passes thus scattering the
        //   player three times** as players are heavier and harder to pass than
        //   a ball." Do ted presny hod polozil hrace PRESNE na cil (0 rozptylu)
        //   a nepresny rozptyloval jen 1x.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, strength: 5, skills: [SkillName::ThrowTeamMate], id: 1)
            ->addPlayer(TeamSide::HOME, 6, 7, skills: [SkillName::RightStuff], id: 2)
            ->withBallOffPitch()
            ->build();

        // presnost 6 = presny hod; 3x rozptyl D8=3 (vychod) z (8,7) => (11,7);
        // pristani 6 = uspech.
        $dice = new FixedDiceRoller([6, 3, 3, 3, 6]);
        $result = (new ActionResolver($dice))->resolve($state, ActionType::THROW_TEAM_MATE, [
            'playerId' => 1, 'targetId' => 2, 'targetX' => 8, 'targetY' => 7,
        ]);

        $hozeny = $result->getNewState()->requirePlayer(2);
        $this->assertSame(11, $hozeny->getPosition()?->getX(), 'tri pole na vychod od ciloveho pole');
        $this->assertSame(7, $hozeny->requirePosition()->getY());
    }

    public function testNepresnyHodRozptylujeTakyTrikrat(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, strength: 5, skills: [SkillName::ThrowTeamMate], id: 1)
            ->addPlayer(TeamSide::HOME, 6, 7, skills: [SkillName::RightStuff], id: 2)
            ->withBallOffPitch()
            ->build();

        // presnost 2 = nepresny; 3x rozptyl D8=3 => (11,7); pristani 6.
        $dice = new FixedDiceRoller([2, 3, 3, 3, 6]);
        $result = (new ActionResolver($dice))->resolve($state, ActionType::THROW_TEAM_MATE, [
            'playerId' => 1, 'targetId' => 2, 'targetX' => 8, 'targetY' => 7,
        ]);

        $hozeny = $result->getNewState()->requirePlayer(2);
        $this->assertSame(11, $hozeny->getPosition()?->getX());
    }

    public function testRozptylSeZastaviHnedJakHracOpustiHriste(): void
    {
        // Pozitivni kontrola: kdyz hrac vyleti ze hriste uz pri prvnim rozptylu,
        // dalsi dva se nehazi (jinak by se kostky rozjely o dve dal).
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 1, 0, strength: 5, skills: [SkillName::ThrowTeamMate], id: 1)
            ->addPlayer(TeamSide::HOME, 2, 0, skills: [SkillName::RightStuff], id: 2)
            ->withBallOffPitch()
            ->build();

        // presnost 2; rozptyl D8=1 (sever) z (4,0) => (4,-1) mimo hriste;
        // pak uz jen zraneni od davu 3+3.
        $dice = new FixedDiceRoller([2, 1, 3, 3]);
        $result = (new ActionResolver($dice))->resolve($state, ActionType::THROW_TEAM_MATE, [
            'playerId' => 1, 'targetId' => 2, 'targetX' => 4, 'targetY' => 0,
        ]);

        $typy = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('crowd_surf', $typy, 'po vyletu se dalsi rozptyly nehazi');
    }

    public function testHozenyNosicVDavuVratiMicVhazenim(): void
    {
        // ⭐ 21.09.2026: `rules_bb2016.txt` r. 8614-8616 -- hozeny hrac, ktery
        //   odletí ze hriste, „is beaten up by the crowd **in the same manner as
        //   a player who has been pushed off the pitch**". A r. 659-663 pro
        //   vytlaceneho nosice: „the fans ... are more than happy to throw the
        //   ball back into play! The throw-in is centred on the last square the
        //   player was in before he was pushed off the pitch."
        //   Do ted mic proste ZMIZEL (`BallState::offPitch()`).
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 1, 0, strength: 5, skills: [SkillName::ThrowTeamMate], id: 1)
            ->addPlayer(TeamSide::HOME, 2, 0, skills: [SkillName::RightStuff], id: 2)
            ->withBallCarried(2)
            ->build();

        // SEBEKONTROLA FIXTURY: hozeny hrac mic opravdu nese.
        $this->assertSame(2, $state->getBall()->getCarrierId(), 'fixtura je vadna: mic nenese hozeny hrac');

        // nepresny hod 2, rozptyl D8=1 (na sever) => (4,-1) mimo hriste;
        // zraneni 3+3; pak vhazeni: sablona D6 a 2D6 poli, pripadny odskok.
        $dice = new FixedDiceRoller([2, 1, 3, 3, 4, 2, 2, 5, 5, 5, 5, 5]);
        $result = (new ActionResolver($dice))->resolve($state, ActionType::THROW_TEAM_MATE, [
            'playerId' => 1, 'targetId' => 2, 'targetX' => 4, 'targetY' => 0,
        ]);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('crowd_surf', $types, 'fixtura: hrac se mel dostat k davu');
        $this->assertContains('throw_in', $types, 'r. 659-663: fanousci hazi mic zpatky do hry');

        $ball = $result->getNewState()->getBall();
        $this->assertTrue($ball->isOnPitch(), 'mic nesmi zmizet ze hry');
        $this->assertNotNull($ball->getPosition());
        $this->assertTrue($result->isTurnover(), 'nosic u davu = turnover (bod 6) -- to plati dal');
    }

    public function testValidationTargetWithoutRightStuff(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 5, skills: [SkillName::ThrowTeamMate], id: 1)
            ->addPlayer(TeamSide::HOME, 6, 5, id: 2) // no RightStuff
            ->withBallOffPitch()
            ->build();

        $this->expectException(\InvalidArgumentException::class);

        $dice = new FixedDiceRoller([4, 4]);
        $resolver = new ActionResolver($dice);
        $resolver->resolve($state, ActionType::THROW_TEAM_MATE, [
            'playerId' => 1, 'targetId' => 2, 'targetX' => 8, 'targetY' => 5,
        ]);
    }

    public function testValidationNotAdjacent(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 5, skills: [SkillName::ThrowTeamMate], id: 1)
            ->addPlayer(TeamSide::HOME, 8, 5, skills: [SkillName::RightStuff], id: 2) // not adjacent
            ->withBallOffPitch()
            ->build();

        $this->expectException(\InvalidArgumentException::class);

        $dice = new FixedDiceRoller([4, 4]);
        $resolver = new ActionResolver($dice);
        $resolver->resolve($state, ActionType::THROW_TEAM_MATE, [
            'playerId' => 1, 'targetId' => 2, 'targetX' => 10, 'targetY' => 5,
        ]);
    }
}
