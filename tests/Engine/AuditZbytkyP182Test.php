<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\ActionResolver;
use App\Engine\BallResolver;
use App\Engine\FixedDiceRoller;
use App\Engine\InjuryResolver;
use App\Engine\KickoffResolver;
use App\Engine\RulesEngine;
use App\Engine\ScatterCalculator;
use App\Engine\TacklezoneCalculator;
use App\Enum\ActionType;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use App\ValueObject\Position;
use PHPUnit\Framework\TestCase;

/**
 * Zbytky auditu parity 08.10.2026 (P182) a review oprav (P186), dotažené 19.11.2026.
 * Každý test cituje řádek `rules_bb2016.txt` a kostky volí tak, aby stará mechanika dala jiný výsledek.
 */
final class AuditZbytkyP182Test extends TestCase
{
    /**
     * Sneaky Git, ř. 8484-8486: „not ejected for rolling doubles on the Armour roll UNLESS THE ARMOUR ROLL
     * WAS SUCCESSFUL“. Dublet na brnění, které padlo ⇒ vyloučen (dřív Sneaky Git chránil vždy).
     */
    public function testSneakyGitJeVyloucenKdyzDubletNaBrneniProrazi(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, skills: [SkillName::SneakyGit], id: 1)
            ->addPronePlayer(TeamSide::AWAY, 6, 7, armour: 8, id: 2)
            ->withBallOffPitch()
            ->build();

        // brnění 5+5 = 10 > 8 (dublet), zranění 3+4 = 7 omráčen, protest rozhodčímu 3 = nic
        $dice = new FixedDiceRoller([5, 5, 3, 4, 3]);
        $result = (new ActionResolver($dice))->resolve($state, ActionType::FOUL, ['playerId' => 1, 'targetId' => 2]);

        $this->assertSame(PlayerState::EJECTED, $result->getNewState()->requirePlayer(1)->getState());
        $this->assertTrue($result->isTurnover(), 'vyloučení za faul je turnover (ř. 1879-1881)');
    }

    /**
     * Sneaky Git chrání jen dublet na brnění; dublet na ZRANĚNÍ skill nezmiňuje ⇒ platí běžné vyloučení
     * (ř. 1877-1878 „Armour and/or Injury roll is a doubles“). Brnění prorazilo, takže výjimka neplatí ani tak.
     */
    public function testSneakyGitJeVyloucenZaDubletNaZraneni(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, skills: [SkillName::SneakyGit], id: 1)
            ->addPronePlayer(TeamSide::AWAY, 6, 7, armour: 8, id: 2)
            ->withBallOffPitch()
            ->build();

        // brnění 6+4 = 10 > 8, zranění 3+3 = 6 (dublet) omráčen, protest 3 = nic
        $dice = new FixedDiceRoller([6, 4, 3, 3, 3]);
        $result = (new ActionResolver($dice))->resolve($state, ActionType::FOUL, ['playerId' => 1, 'targetId' => 2]);

        $this->assertSame(PlayerState::EJECTED, $result->getNewState()->requirePlayer(1)->getState());
    }

    /** Pozitivní kontrola: dublet na brnění, které vydrželo ⇒ Sneaky Git chrání (beze změny). */
    public function testSneakyGitNeniVyloucenKdyzBrneniVydrzi(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, skills: [SkillName::SneakyGit], id: 1)
            ->addPronePlayer(TeamSide::AWAY, 6, 7, armour: 8, id: 2)
            ->withBallOffPitch()
            ->build();

        $dice = new FixedDiceRoller([3, 3]);
        $result = (new ActionResolver($dice))->resolve($state, ActionType::FOUL, ['playerId' => 1, 'targetId' => 2]);

        $this->assertNotSame(PlayerState::EJECTED, $result->getNewState()->requirePlayer(1)->getState());
        $this->assertFalse($result->isTurnover());
    }

    /**
     * Horns, ř. 8173-8175: „adds 1 to the player's Strength for ANY BLOCK(S) he makes during a Blitz Action“
     * ⇒ i druhá rána Frenzy v blitzu (dřív jen první). ST3 + Horns proti ST4: s Horns 1 kostka, bez 2 proti.
     */
    public function testHornsPlatiIProDruhouRanuFrenzyVBlitzu(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 4, 7, strength: 3, skills: [SkillName::Horns, SkillName::Frenzy], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 7, strength: 4, id: 2)
            ->withBallOffPitch()
            ->build();

        // 1. rána: 1 kostka, 3 = Pushed (obránce na (7,7), útočník následuje); 2. rána: 3 = Pushed
        $dice = new FixedDiceRoller([3, 3, 3]);
        $result = (new ActionResolver($dice))->resolve($state, ActionType::BLITZ, ['playerId' => 1, 'targetId' => 2]);

        $bloky = array_values(array_filter($result->getEvents(), fn($e) => $e->getType() === 'block'));
        $this->assertCount(2, $bloky, 'po Pushed přijde druhá rána Frenzy');
        $this->assertSame(1, $bloky[1]->getData()['diceCount'], 'druhá rána v blitzu má Horns: ST4 proti ST4');
        $this->assertTrue($bloky[1]->getData()['attackerChooses']);
    }

    /**
     * Claw, ř. 8020-8022: „any Armour roll of 8 or more AFTER MODIFICATIONS automatically breaks armour“.
     * Hod 4+3 = 7 + Mighty Blow = 8 proti AV 9 ⇒ Claw prorazí (dřív se Claw četl na nemodifikovaném hodu).
     */
    public function testClawPlatiNaModifikovanyHod(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 4, skills: [SkillName::Claw, SkillName::MightyBlow], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, armour: 9, id: 2)
            ->withBallOffPitch()
            ->build();

        // 2 kostky útočníka: 6,6 = POW; brnění 4+3 (+1 MB) = 8; zranění 3+4 = 7 omráčen
        $dice = new FixedDiceRoller([6, 6, 4, 3, 3, 4]);
        $result = (new ActionResolver($dice))->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $brneni = array_values(array_filter($result->getEvents(), fn($e) => $e->getType() === 'armour_roll'));
        $this->assertTrue($brneni[0]->getData()['broken']);
        $this->assertSame(PlayerState::STUNNED, $result->getNewState()->requirePlayer(2)->getState());
    }

    /**
     * Odrazy, ř. 903-906: míč se odráží, „until it is either caught or bounces into an empty square or off the
     * pitch“. Šest ležících hráčů za sebou ⇒ míč skončí na sedmém, volném poli. Dřív strop 5 odrazů nechal
     * míč ležet pod pátým hráčem.
     */
    public function testOdrazPokracujeDokudNedopadneNaVolnePole(): void
    {
        $builder = (new GameStateBuilder())->withBallOnGround(10, 7);
        for ($x = 11; $x <= 16; $x++) {
            $builder->addPronePlayer(TeamSide::AWAY, $x, 7, id: $x);
        }
        $state = $builder->build();

        $dice = new FixedDiceRoller(array_fill(0, 7, 3));   // D8 = 3: o pole doprava
        $scatter = new ScatterCalculator();
        $result = (new BallResolver($dice, new TacklezoneCalculator(), $scatter))->resolveBounce($state, new Position(10, 7));

        $mic = $result['state']->getBall();
        $this->assertFalse($mic->isHeld());
        $this->assertTrue($mic->requirePosition()->equals(new Position(17, 7)));
    }

    /**
     * Pitch Invasion, ř. 1351-1356: D6 „for each opposing player ON THE PITCH“; 6+ ⇒ Stunned, „players with
     * the Ball & Chain skill are KO'd“. Dřív jen za stojící a Ball & Chain jako ostatní. FAME engine nevede (0).
     */
    public function testPitchInvasionZasahneIHraceNaZemiABallAndChainJeKo(): void
    {
        $state = (new GameStateBuilder())
            ->addPronePlayer(TeamSide::HOME, 6, 5, id: 1)
            ->addPlayer(TeamSide::HOME, 6, 7, skills: [SkillName::BallAndChain], id: 2)
            ->addPlayer(TeamSide::AWAY, 15, 5, id: 3)
            ->build();

        // 6+6 = Pitch Invasion; hráč 1: 6, hráč 2: 6, hráč 3: 2
        $dice = new FixedDiceRoller([6, 6, 6, 6, 2]);
        $scatter = new ScatterCalculator();
        $resolver = new KickoffResolver($dice, $scatter, new BallResolver($dice, new TacklezoneCalculator(), $scatter), new InjuryResolver());
        $po = $resolver->resolveKickoffTable($state, TeamSide::HOME)['state'];

        $this->assertSame(PlayerState::STUNNED, $po->requirePlayer(1)->getState(), 'ležící je na hřišti ⇒ hází se i za něj');
        $this->assertSame(PlayerState::KO, $po->requirePlayer(2)->getState());
        $this->assertNull($po->requirePlayer(2)->getPosition());
        $this->assertSame(PlayerState::STANDING, $po->requirePlayer(3)->getState());
    }

    /**
     * Stand Firm × Strip Ball (rozhodl uživatel 10.10.2026, kniha P194 (7); ř. 8510-8521): Stand Firm mění jen
     * pole, ne to, že Strip Ball míč vyrazí ⇒ nosič se Stand Firm stojí na svém poli a míč se odrazí odtud.
     * Dřív PHP odtlačení zrušilo celé a míč zůstal nosiči (C++ ho vyrazí).
     */
    public function testStripBallVyraziMicIHraciKteryDrziPoleStandFirm(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, strength: 4, skills: [SkillName::StripBall], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 7, skills: [SkillName::StandFirm], id: 2)
            ->withBallCarried(2)
            ->build();

        // 2 kostky útočníka: 3,4 = Pushed; odraz míče D8 = 3 ⇒ (7,7)
        $dice = new FixedDiceRoller([3, 4, 3]);
        $result = (new ActionResolver($dice))->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $po = $result->getNewState();
        $obrance = $po->requirePlayer(2);
        $this->assertTrue($obrance->requirePosition()->equals(new Position(6, 7)), 'Stand Firm: obránce zůstal');
        $this->assertSame(PlayerState::STANDING, $obrance->getState());
        $this->assertFalse($po->getBall()->isHeld(), 'Strip Ball míč vyrazil');
        $this->assertTrue($po->getBall()->requirePosition()->equals(new Position(7, 7)));
        $this->assertTrue($po->requirePlayer(1)->requirePosition()->equals(new Position(5, 7)), 'útočník nemá kam následovat');
    }

    /**
     * Vstávání, ř. 690-695: pod 3 MA hod 4+, „if he stands up successfully, he may not move further squares
     * UNLESS HE GOES FOR IT“; týmový přehoz smí na kterýkoli hod hráče (ř. 929-931). Dřív holý hod bez přehozu.
     */
    public function testVstaniPodTriMaSmiPrehoditTymovymPrehozem(): void
    {
        $state = (new GameStateBuilder())
            ->addPronePlayer(TeamSide::HOME, 10, 7, movement: 2, id: 1)
            ->withBallOffPitch()
            ->build();

        // vstání 2 (nevyšlo), týmový přehoz 5 (vyšlo)
        $dice = new FixedDiceRoller([2, 5]);
        $result = (new ActionResolver($dice))->resolve($state, ActionType::MOVE, ['playerId' => 1, 'x' => 10, 'y' => 7]);

        $this->assertSame(PlayerState::STANDING, $result->getNewState()->requirePlayer(1)->getState());
        $this->assertTrue($result->getNewState()->getTeamState(TeamSide::HOME)->isRerollUsedThisTurn());
    }

    /**
     * Totéž pravidlo, druhá půlka: po úspěšném vstání pod 3 MA smí hráč dál přes GFI. Resolver to uměl, ale
     * nabídka i validace (`Pathfinder`) ležícímu pod 3 MA cestu nedaly ⇒ akce nešla zadat.
     */
    public function testPoVstaniPodTriMaSmiDalPresGfi(): void
    {
        $state = (new GameStateBuilder())
            ->addPronePlayer(TeamSide::HOME, 10, 7, movement: 2, id: 1)
            ->withBallOffPitch()
            ->build();

        $pravidla = new RulesEngine();
        $this->assertSame([], $pravidla->validate($state, ActionType::MOVE, ['playerId' => 1, 'x' => 11, 'y' => 7]));
        $cile = array_map(fn(array $c) => [$c['x'], $c['y']], $pravidla->getValidMoveTargets($state, 1));
        $this->assertContains([11, 7], $cile, 'krok přes GFI');
        $this->assertContains([12, 7], $cile, 'dva kroky přes GFI');
        $this->assertNotContains([13, 7], $cile, 'víc než 2 GFI ne');

        // vstání 4, GFI 3
        $dice = new FixedDiceRoller([4, 3]);
        $result = (new ActionResolver($dice))->resolve($state, ActionType::MOVE, ['playerId' => 1, 'x' => 11, 'y' => 7]);

        $hrac = $result->getNewState()->requirePlayer(1);
        $this->assertSame(PlayerState::STANDING, $hrac->getState());
        $this->assertTrue($hrac->requirePosition()->equals(new Position(11, 7)));
        $this->assertFalse($result->isTurnover());
    }

    /**
     * Take Root, ř. 8575-8584: zakořeněný má MA 0 a „may not Go For It“, ležící „can still roll to stand up“.
     * Ležící Treeman, který se při pohybu zakoření, vstane a zůstane na svém poli — akce nespadne na „No valid path“.
     */
    public function testZakorenenyLeziciVstaneAZustaneNaPoli(): void
    {
        $state = (new GameStateBuilder())
            ->addPronePlayer(TeamSide::HOME, 10, 7, movement: 2, strength: 6, agility: 1, armour: 10, skills: [SkillName::TakeRoot], id: 1)
            ->withBallOffPitch()
            ->build();

        // Take Root 1 (zakoření), vstání 4
        $dice = new FixedDiceRoller([1, 4]);
        $result = (new ActionResolver($dice))->resolve($state, ActionType::MOVE, ['playerId' => 1, 'x' => 11, 'y' => 7]);

        $hrac = $result->getNewState()->requirePlayer(1);
        $this->assertTrue($hrac->isRooted());
        $this->assertSame(PlayerState::STANDING, $hrac->getState());
        $this->assertTrue($hrac->requirePosition()->equals(new Position(10, 7)));
    }
}
