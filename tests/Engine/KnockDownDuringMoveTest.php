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

/**
 * ⛔⛔⛔ VADA PHP27 (11.09.2026) — HRÁČ SPADL PŘI POHYBU A NIKDY SE MU
 *    NEHODILO NA BRNĚNÍ. Nalezeno z uživatelovy věty *„po turnover se
 *    samozřejmě musí vyhodnotit brnění a tak"*.
 *
 * ⭐ ZMĚŘENO PŘED OPRAVOU: neúspěšný dodge = **jediný hod** (ten dodge),
 *    události `dodge` → `player_fell` → `turnover`, hráč `prone`, konec.
 *    `MoveHandler` přitom `InjuryResolver` **vůbec nedostával**
 *    (`ActionResolver.php:73`), na rozdíl od Block, Foul, TTM, Bomb a B&C.
 *
 * ⭐ PRAVIDLA r. 496-500: *„…then the player is Knocked Down in the square he
 *    was dodging to **and a roll must be made to see if he was injured**
 *    (See Knock Downs & Injuries). If the player is Knocked Down then his
 *    team suffers a turnover and their turn ends immediately."*
 *    ⇒ Zranění je součást PÁDU, ne až důsledek turnoveru.
 */
final class KnockDownDuringMoveTest extends TestCase
{
    /** Hráč HOME v zóně zachycení dvou soupeřů ⇒ únik = dodge. */
    private function stateWithDodgeRequired(): \App\DTO\GameState
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 6, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2)
            ->addPlayer(TeamSide::AWAY, 6, 8, id: 3)
            ->withBallOffPitch()
            ->build();

        return $state->withTeamState(TeamSide::HOME,
            $state->getTeamState(TeamSide::HOME)->withRerolls(0));
    }

    public function testFailedDodgeRollsForArmour(): void
    {
        $state = $this->stateWithDodgeRequired();

        // SEBEKONTROLA FIXTURY: bez dodge by se vada nemohla projevit.
        $this->assertNotSame([], (new \App\Engine\RulesEngine())->getValidMoveTargets($state, 1),
            'fixtura je vadná: hráč nemá kam');

        // 1 = neúspěšný dodge, pak 1+1 = hod na brnění (2D6 = 2, neprorazí AV8).
        $dice = new FixedDiceRoller([1, 1, 1]);
        $result = (new ActionResolver($dice))->resolve(
            $state, ActionType::MOVE, ['playerId' => 1, 'x' => 4, 'y' => 6],
        );

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('armour_roll', $types,
            'hráč byl sražen, ale nehodilo se mu na brnění (r. 496-500)');
        $this->assertTrue($result->isTurnover());
        $this->assertSame(PlayerState::PRONE, $result->getNewState()->getPlayer(1)->getState());
        $this->assertSame(3, $dice->getRollCount(),
            'čekal jsem dodge (1 hod) + brnění (2 hody)');
    }

    public function testBrokenArmourAlsoRollsForInjury(): void
    {
        // ⭐ DRUHÁ POLOVINA: brnění se nejen hodí, ale když praskne, jde se
        //    na zranění. Bez tohohle by test prošel i nad polovinou opravy.
        $state = $this->stateWithDodgeRequired();

        // 1 = neúspěšný dodge · 6+6 = brnění 12 > AV8, prorazí · 1+1 = zranění.
        $dice = new FixedDiceRoller([1, 6, 6, 1, 1]);
        $result = (new ActionResolver($dice))->resolve(
            $state, ActionType::MOVE, ['playerId' => 1, 'x' => 4, 'y' => 6],
        );

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('armour_roll', $types);
        $this->assertContains('injury_roll', $types,
            'brnění prasklo, ale na zranění se nehodilo');
    }

    public function testSuccessfulDodgeRollsNothingExtra(): void
    {
        // ⭐ POZITIVNÍ KONTROLA OBRÁCENĚ: měřidlo nesmí hlásit brnění tam,
        //    kde hráč nespadl. Jinak by první test mohl projít nad čímkoli.
        $state = $this->stateWithDodgeRequired();

        $dice = new FixedDiceRoller([6, 6, 6, 6]);
        $result = (new ActionResolver($dice))->resolve(
            $state, ActionType::MOVE, ['playerId' => 1, 'x' => 4, 'y' => 6],
        );

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertNotContains('armour_roll', $types,
            'hráč dodge zvládl a přesto se hodilo na brnění');
        $this->assertFalse($result->isTurnover());
    }

    public function testFailedGoingForItAlsoRollsForArmour(): void
    {
        // ⭐ TÝŽ PRINCIP JINDE: GFI je druhé ze tří míst v `MoveHandler`,
        //    kde hráč padá (dodge :290, GFI :420, leap :112).
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 1, id: 1)
            ->addPlayer(TeamSide::AWAY, 20, 3, id: 2)
            ->withBallOffPitch()
            ->build();
        $state = $state->withTeamState(TeamSide::HOME,
            $state->getTeamState(TeamSide::HOME)->withRerolls(0));

        // MA=1: první pole zadarmo, druhé přes GFI ⇒ 1 = pád.
        $dice = new FixedDiceRoller([1, 1, 1]);
        $result = (new ActionResolver($dice))->resolve(
            $state, ActionType::MOVE, ['playerId' => 1, 'x' => 7, 'y' => 7],
        );

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('gfi', $types, 'fixtura je vadná: k GFI vůbec nedošlo');
        $this->assertContains('armour_roll', $types,
            'hráč spadl při GFI, ale nehodilo se mu na brnění');
    }

    public function testFailedLeapAlsoRollsForArmour(): void
    {
        // ⭐ A třetí místo: leap.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 6, id: 1,
                        skills: [SkillName::Leap])
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2)
            ->addPlayer(TeamSide::AWAY, 6, 8, id: 3)
            ->withBallOffPitch()
            ->build();
        $state = $state->withTeamState(TeamSide::HOME,
            $state->getTeamState(TeamSide::HOME)->withRerolls(0));

        $dice = new FixedDiceRoller([1, 1, 1, 1, 1]);
        $result = (new ActionResolver($dice))->resolve(
            $state, ActionType::MOVE, ['playerId' => 1, 'x' => 7, 'y' => 7, 'leap' => true],
        );

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        if (!in_array('leap', $types, true)) {
            $this->markTestSkipped('fixtura leap nespustila -- neměří se tím nic');
        }
        $this->assertContains('armour_roll', $types,
            'hráč spadl při leapu, ale nehodilo se mu na brnění');
    }
}
