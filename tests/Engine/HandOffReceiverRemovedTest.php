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
 * ⛔ VADA (11.09.2026, PHP17): `HandOffHandler` házel
 *    `Players must be on pitch`, když příjemce mezitím z hřiště zmizel.
 *
 * ⭐ JAK SE TO STANE: nabídka vybírá příjemce přes `getHandOffTargets`, které
 *    vrací **jen hráče na hřišti**. Mezi nabídkou a provedením ale běží
 *    KONTROLA PŘED AKCÍ (`ActionResolver:130`) — a **Bloodlust** v ní kousne
 *    Thralla. Hod na zranění může dát KO nebo zranění a to
 *    (`InjuryResolver:205,211,215`) nastaví `position = null`.
 *    Když je tím Thrallem právě příjemce hand-offu, handler ho nenajde.
 *
 * ⭐ Třída (C) „etapa věří, že předchozí uspěla" — tady přes DVĚ vrstvy.
 * ⭐ Pravidlová kotva r. 7935-7937: vampír se krmí „at the end of the declared
 *    Action, **but before actually passing, handing off, or scoring**".
 */
final class HandOffReceiverRemovedTest extends TestCase
{
    public function testBittenThrallAsReceiverDoesNotThrow(): void
    {
        // Vampír (1) nese míč, vedle něj JEDINÝ Thrall (2) — je zároveň
        // jediný možný příjemce hand-offu i jediný, koho lze kousnout.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1, skills: [SkillName::Bloodlust])
            ->addPlayer(TeamSide::HOME, 6, 7, id: 2)
            ->withBallCarried(1)
            ->build();

        $rules = new RulesEngine();

        // SEBEKONTROLA FIXTURY: hand-off na hráče 2 se OPRAVDU nabízí --
        // jinak by se do opravované větve nedošlo.
        $targets = $rules->getHandOffTargets($state, $state->getPlayer(1));
        $this->assertCount(1, $targets, 'fixtura je vadná: cílů není právě jeden');
        $this->assertSame(2, $targets[0]->getId());

        // Bloodlust: 1 (neúspěch) → kousnutí → zranění 6+6=12 ⇒ Thrall
        // z hřiště zmizí. Pak by následoval hand-off.
        $dice = new FixedDiceRoller([1, 6, 6, 6, 6, 6]);
        $resolver = new ActionResolver($dice);

        $result = $resolver->resolve($state, ActionType::HAND_OFF, [
            'playerId' => 1, 'targetId' => 2,
        ]);

        // SEBEKONTROLA VÝSLEDKU: příjemce OPRAVDU zmizel z hřiště.
        $this->assertNull($result->getNewState()->getPlayer(2)->getPosition(),
            'Thrall na hřišti zůstal — test neměří, co má');

        // Dřív tady letěla výjimka.
        $this->assertTrue($result->isSuccess(),
            'zmizelý příjemce nesmí házet výjimku');
        $this->assertFalse($result->isTurnover(),
            'katalog turnoverů (r. 368-384) tenhle případ nezná');
        // Míč zůstává podávajícímu.
        $this->assertSame(1, $result->getNewState()->getBall()->getCarrierId(),
            'míč nemá komu přejít, zůstává vampírovi');
    }

    public function testNormalHandOffStillWorks(): void
    {
        // ⛔ Druhá polovina páru: oprava nesmí rozbít běžný hand-off.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, agility: 4, id: 1)
            ->addPlayer(TeamSide::HOME, 6, 7, agility: 4, id: 2)
            ->withBallCarried(1)
            ->build();

        $resolver = new ActionResolver(new FixedDiceRoller([6]));
        $result = $resolver->resolve($state, ActionType::HAND_OFF, [
            'playerId' => 1, 'targetId' => 2,
        ]);

        $this->assertTrue($result->isSuccess());
        $this->assertSame(2, $result->getNewState()->getBall()->getCarrierId(),
            'běžný hand-off musí míč předat');
    }

    public function testProneReceiverStillGetsTheHandOffAndCannotCatch(): void
    {
        // ⛔⛔ TOHLE PRVNI VERZE OPRAVY DELALA SPATNE (opraveno 11.09. podruhé
        //    po upozornění uživatele): rušila předání i pro příjemce, který
        //    na hřišti ZŮSTAL, jen leží. To je po opravě Bloodlustu ten
        //    ČASTÝ případ — hod na zranění dá nejčastěji Stunned.
        //
        // ⭐ Pravidla to rozlišují: hráč v sousedním poli JE, takže se předání
        //    koná („it automatically hits the targeted player", r. 1687-1688).
        //    Chytit ale nesmí — r. 857-858: „**Prone and Stunned players may
        //    never attempt to catch the ball.**" ⇒ míč se odrazí, a když
        //    skončí mimo náš tým, je to turnover (r. 1683-1686).
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, agility: 4, id: 1)
            ->addPlayer(TeamSide::HOME, 6, 7, agility: 4, id: 2)
            ->withBallCarried(1)
            ->build();
        $state = $state->withPlayer(
            $state->getPlayer(2)->withState(\App\Enum\PlayerState::PRONE),
        );

        // SEBEKONTROLA FIXTURY: příjemce LEŽÍ, ale JE na hřišti -- jinak by
        // se test trefil do větve (a) a neměřil by, co má.
        $this->assertNotNull($state->getPlayer(2)->getPosition(),
            'fixtura je vadná: příjemce z hřiště zmizel, to je jiný případ');
        $this->assertFalse($state->getPlayer(2)->getState()->canAct(),
            'fixtura je vadná: příjemce stojí');

        // Odraz D8 = 3 (dx +1) na (7,7), tam nikdo nestojí => míč na zemi.
        $resolver = new ActionResolver(new FixedDiceRoller([3]));
        $result = $resolver->resolve($state, ActionType::HAND_OFF, [
            'playerId' => 1, 'targetId' => 2,
        ]);

        $ball = $result->getNewState()->getBall();
        $this->assertFalse($ball->isHeld(),
            'ležící chytit nesmí -- míč se má odrazit (r. 857-858)');
        $this->assertTrue($result->isTurnover(),
            'míč se zastavil nechycený => turnover (r. 1683-1686)');
    }

    public function testProneReceiverBounceCaughtByTeammateIsNotATurnover(): void
    {
        // ⭐ Druhá půlka: turnover visí na tom, kde míč SKONČÍ, ne na tom,
        //    že příjemce nechytil. Odraz do rukou spoluhráče kolo nekončí.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, agility: 4, id: 1)
            ->addPlayer(TeamSide::HOME, 6, 7, agility: 4, id: 2)
            ->addPlayer(TeamSide::HOME, 7, 7, agility: 4, id: 3)
            ->withBallCarried(1)
            ->build();
        $state = $state->withPlayer(
            $state->getPlayer(2)->withState(\App\Enum\PlayerState::PRONE),
        );
        $state = $state->withTeamState(TeamSide::HOME,
            $state->getTeamState(TeamSide::HOME)->withRerollUsed());

        // Odraz D8 = 3 na (7,7), kde STOJÍ hráč 3 => chytá (hod 6).
        $resolver = new ActionResolver(new FixedDiceRoller([3, 6]));
        $result = $resolver->resolve($state, ActionType::HAND_OFF, [
            'playerId' => 1, 'targetId' => 2,
        ]);

        $this->assertSame(3, $result->getNewState()->getBall()->getCarrierId(),
            'odraz měl chytit spoluhráč');
        $this->assertFalse($result->isTurnover(),
            'míč zůstal našemu týmu -- kolo nekončí');
    }
}
