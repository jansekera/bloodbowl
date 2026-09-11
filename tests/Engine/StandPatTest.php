<?php
declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Engine\RulesEngine;
use App\Enum\ActionType;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * ⭐ PHP25 (11.09.2026) — „NIC NEDĚLAT" JAKO VOLBA.
 *
 * Uživatel: *„bez balonu můžu stát na TD zóně s předními rohy klece a čekat
 * s celou klecí na poslední tah, kdy nosič provede jeden krok dopředu a dá
 * TD."* ⇒ Pro přední hráče klece je **správný tah žádný tah**, a do dneška
 * to kouč neuměl říct jinak než koncem kola pro CELÝ tým.
 *
 * ⛔⛔ A pro big guye to znamená víc než úsporu pohybu — uživatel:
 * *„pro big guye to navíc znamená se neaktivovat, tak nemusí házet po
 * aktivaci."* Kontrola před akcí *(Bone Head, Really Stupid, Wild Animal,
 * Bloodlust, Take Root)* je důsledek AKTIVACE. Kdo nehraje, nehází.
 */
final class StandPatTest extends TestCase
{
    public function testStandPatCostsNoRollEvenForAReallyStupidBigGuy(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 6, id: 1,
                        skills: [SkillName::ReallyStupid])
            ->addPlayer(TeamSide::AWAY, 20, 3, id: 2)
            ->withBallOffPitch()
            ->build();

        // SEBEKONTROLA: hráč na začátku opravdu ještě může jednat.
        $this->assertTrue($state->getPlayer(1)->canAct(),
            'fixtura je vadná: hráč už jednal, volba nehrát nemá co ušetřit');

        // ⛔ KOSTKA BEZ JEDINÉHO HODU: kdyby se sáhlo na kontrolu před akcí,
        //    `FixedDiceRoller` vyhodí výjimku a test spadne.
        $dice = new FixedDiceRoller([]);
        $result = (new ActionResolver($dice))->resolve(
            $state, ActionType::STAND_PAT, ['playerId' => 1],
        );

        $this->assertSame(0, $dice->getRollCount(),
            'neaktivovaný hráč házel -- to je přesně to, co se nesmí');
        $this->assertFalse($result->isTurnover());

        $after = $result->getNewState()->getPlayer(1);
        $this->assertTrue($after->hasActed(), 'hráč zůstal aktivovatelný');
        $this->assertTrue($after->hasMoved(), 'hráč zůstal pohyblivý');

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertSame(['stand_pat'], $types);
    }

    public function testPositiveControlTheSameBigGuyDoesRollWhenHeActs(): void
    {
        // ⭐ POZITIVNÍ KONTROLA K PŘEDCHOZÍMU TESTU: nula hodů výš má cenu jen
        //    tehdy, když se nad TOUTÉŽ fixturou dá jednička najít. Tentýž hráč,
        //    tentýž stav, jen místo „nic nedělat" opravdový pohyb ⇒ Really
        //    Stupid se hází.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 6, id: 1,
                        skills: [SkillName::ReallyStupid])
            ->addPlayer(TeamSide::AWAY, 20, 3, id: 2)
            ->withBallOffPitch()
            ->build();

        $dice = new FixedDiceRoller([6, 6, 6, 6, 6, 6]);
        (new ActionResolver($dice))->resolve(
            $state, ActionType::MOVE, ['playerId' => 1, 'x' => 6, 'y' => 7],
        );

        $this->assertGreaterThan(0, $dice->getRollCount(),
            'měřidlo je vadné: ani skutečná akce big guye nevyvolala hod');
    }

    public function testStandPatIsOfferedOnlyWhileThePlayerCanStillDoSomething(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 6, id: 1)
            ->addPlayer(TeamSide::AWAY, 20, 3, id: 2)
            ->withBallOffPitch()
            ->build();

        $rules = new RulesEngine();
        $offered = fn($s) => array_values(array_filter(
            $rules->getAvailableActions($s),
            fn(array $a) => $a['type'] === ActionType::STAND_PAT->value,
        ));

        $this->assertCount(1, $offered($state), 'volba nehrát se vůbec nenabídla');

        $done = $state->withPlayer(
            $state->getPlayer(1)->withHasActed(true)->withHasMoved(true),
        );
        $this->assertSame([], $offered($done),
            'volba nehrát se nabízí i hráči, který už jednal');
    }

    public function testBallAndChainPlayerHasNoChoiceButToAct(): void
    {
        // ⛔ Ball & Chain hráč MUSÍ jednat -- pravidla mu volbu nedávají,
        //    takže mu ji nesmí dát ani tahle nová akce.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 3, id: 1,
                        skills: [SkillName::BallAndChain])
            ->addPlayer(TeamSide::AWAY, 20, 3, id: 2)
            ->withBallOffPitch()
            ->build();

        $types = array_column((new RulesEngine())->getAvailableActions($state), 'type');

        $this->assertContains(ActionType::BALL_AND_CHAIN->value, $types,
            'fixtura je vadná: B&C hráči se nenabízí ani jeho vlastní akce');
        $this->assertNotContains(ActionType::STAND_PAT->value, $types,
            'Ball & Chain hráč dostal volbu nehrát, kterou podle pravidel nemá');
    }
}
