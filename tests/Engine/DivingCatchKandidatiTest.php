<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\ActionResult;
use App\DTO\GameState;
use App\DTO\PendingDivingCatchDTO;
use App\Engine\ActionResolver;
use App\Engine\BallResolver;
use App\Engine\FixedDiceRoller;
use App\Engine\InjuryResolver;
use App\Engine\KickoffResolver;
use App\Engine\ScatterCalculator;
use App\Engine\TacklezoneCalculator;
use App\Enum\ActionType;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use App\ValueObject\Position;
use PHPUnit\Framework\TestCase;

/**
 * Diving Catch — víc kandidátů, varianta B (uživatel 30.10.2026, parita s C++ `DivingCatchCandidates.*`).
 *
 * Dřív: dva a víc hráčů s Diving Catch v dosahu dopadu ⇒ nikdo, bez otázky. Pravidla:
 * - ř. 8069-8071 "If there are two or more players ATTEMPTING to use this skill then they get in each
 *   other's way and neither can use it." — ruší se POKUSY, ne přítomnost;
 * - ř. 985 "you never have to use a skill just because the player's got it";
 * - ř. 986-990 "the coach whose turn is taking place must use his skill first".
 * ⇒ napřed rozhodne strana na tahu (o každém svém kandidátovi), pak druhá, která ví, kdo už skáče.
 * Právě jeden pokus ⇒ chytá; dva a víc ⇒ nikdo, míč dopadne normálně. AI skočí s jedním hráčem
 * (nejlepší cíl hodu).
 *
 * Nepřesná přihrávka HOME z (5,5) na (10,5) dopadá na prázdné (10,6) (jako DivingCatchKoucTest);
 * odraz D8 4 = JV na (11,7); chytání 4 (AG3 bez zón 4+).
 */
final class DivingCatchKandidatiTest extends TestCase
{
    private const HODY_PRIHRAVKY = [2, 2, 5, 3, 7];

    /** Dva kandidáti HOME: 2 na (10,7), 3 na (9,7). */
    private function dvaDomaci(int $agilita3 = 3): GameState
    {
        return (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 10, 7, agility: 3, skills: [SkillName::DivingCatch], id: 2)
            ->addPlayer(TeamSide::HOME, 9, 7, agility: $agilita3, skills: [SkillName::DivingCatch], id: 3)
            ->withBallCarried(1)
            ->build();
    }

    /**
     * Kandidát AWAY 3 na (11,5) (za cílem — nezachytává) a kandidát HOME 13 na (9,7): hráč soupeře má
     * NIŽŠÍ id, aby pořadí „strana na tahu první“ nešlo splést s pořadím podle id.
     */
    private function obeStrany(TeamSide $naTahu = TeamSide::HOME): GameState
    {
        return (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 11, 5, agility: 3, skills: [SkillName::DivingCatch], id: 3)
            ->addPlayer(TeamSide::HOME, 9, 7, agility: 3, skills: [SkillName::DivingCatch], id: 13)
            ->withBallCarried(1)
            ->withActiveTeam($naTahu)
            ->build();
    }

    /**
     * Přihrávka; kouč-člověk hraje `$lide` a odpověděl `$odpovedi` (popořadě, jako po sobě jdoucí
     * RESOLVE_DIVING_CATCH). Vrátí výsledek poslední odpovědi (nebo přihrávky).
     *
     * @param list<TeamSide> $lide
     * @param list<bool> $odpovedi
     */
    private function prihraj(GameState $s, array $lide, array $odpovedi = []): ActionResult
    {
        $r = new ActionResolver(new FixedDiceRoller(array_merge(self::HODY_PRIHRAVKY, [4])));
        $r->setInteractiveDivingCatch($lide);
        $vysledek = $r->resolve($s, ActionType::PASS, ['playerId' => 1, 'targetX' => 10, 'targetY' => 5]);
        foreach ($odpovedi as $ano) {
            $this->assertNotNull($vysledek->getNewState()->getPendingDivingCatch(), 'čeká se otázka');
            $r = new ActionResolver(new FixedDiceRoller([4]));
            $r->setInteractiveDivingCatch($lide);
            $vysledek = $r->resolve($vysledek->getNewState(), ActionType::RESOLVE_DIVING_CATCH, ['use' => $ano]);
        }

        return $vysledek;
    }

    /**
     * @param list<TeamSide> $lide
     * @param list<bool> $odpovedi
     */
    private function otazka(GameState $s, array $lide, array $odpovedi = []): PendingDivingCatchDTO
    {
        $q = $this->prihraj($s, $lide, $odpovedi)->getNewState()->getPendingDivingCatch();
        $this->assertNotNull($q, 'otázka nepřišla');

        return $q;
    }

    private function skoky(ActionResult $r): int
    {
        return count(array_filter($r->getEvents(), static fn($e) => $e->getType() === 'diving_catch'));
    }

    /** @return array{0: int, 1: int}|null */
    private function mic(GameState $s): ?array
    {
        $p = $s->getBall()->getPosition();

        return $p === null ? null : [$p->getX(), $p->getY()];
    }

    public function testJedenKandidatOtazkaBezDalsichKandidatu(): void
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 10, 7, agility: 3, skills: [SkillName::DivingCatch], id: 2)
            ->withBallCarried(1)
            ->build();
        $q = $this->otazka($s, [TeamSide::HOME]);

        $this->assertSame(2, $q->getPlayerId());
        $this->assertSame(0, $q->getOtherCandidates());
        $this->assertNull($q->getAttemptingId());
    }

    public function testKoucSePtaNaKazdehoSvehoKandidata(): void
    {
        $prvni = $this->otazka($this->dvaDomaci(), [TeamSide::HOME]);
        $this->assertSame(2, $prvni->getPlayerId());
        $this->assertSame(1, $prvni->getOtherCandidates(), 'jeden další kandidát');

        $druha = $this->otazka($this->dvaDomaci(), [TeamSide::HOME], [false]);
        $this->assertSame(3, $druha->getPlayerId());
        $this->assertNull($druha->getAttemptingId());
    }

    public function testNeProPrvnihoDruhyChyta(): void
    {
        $r = $this->prihraj($this->dvaDomaci(), [TeamSide::HOME], [false, true]);

        $this->assertSame(1, $this->skoky($r));
        $this->assertSame(3, $r->getNewState()->getBall()->getCarrierId());
    }

    public function testAnoProPrvnihoNeProDruhehoPrvniChyta(): void
    {
        $r = $this->prihraj($this->dvaDomaci(), [TeamSide::HOME], [true, false]);

        $this->assertSame(1, $this->skoky($r));
        $this->assertSame(2, $r->getNewState()->getBall()->getCarrierId());
    }

    public function testDvaPokusyTehozTymuSeRusi(): void
    {
        $druha = $this->otazka($this->dvaDomaci(), [TeamSide::HOME], [true]);
        $this->assertSame(3, $druha->getPlayerId());
        $this->assertSame(2, $druha->getAttemptingId(), 'kouč ví, že hráč 2 už skáče');

        $r = $this->prihraj($this->dvaDomaci(), [TeamSide::HOME], [true, true]);
        $this->assertSame(0, $this->skoky($r), '"neither can use it" (ř. 8070-8071)');
        $this->assertNull($r->getNewState()->getBall()->getCarrierId());
        $this->assertSame([11, 7], $this->mic($r->getNewState()), 'odraz z pole dopadu');
    }

    public function testStranaNaTahuSeRozhodujePrvni(): void
    {
        $lide = [TeamSide::HOME, TeamSide::AWAY];
        $prvni = $this->otazka($this->obeStrany(), $lide);
        $this->assertSame(13, $prvni->getPlayerId(), 'na tahu HOME ⇒ napřed HOME, i když AWAY má nižší id');
        $this->assertSame(1, $prvni->getOtherCandidates());

        $druha = $this->otazka($this->obeStrany(), $lide, [true]);
        $this->assertSame(3, $druha->getPlayerId());
        $this->assertSame(13, $druha->getAttemptingId(), 'druhá strana ví, že HOME skáče');

        $this->assertSame(3, $this->otazka($this->obeStrany(TeamSide::AWAY), $lide)->getPlayerId(), 'na tahu AWAY ⇒ napřed AWAY');
    }

    public function testObeStranyAnoNikdoNechyta(): void
    {
        $r = $this->prihraj($this->obeStrany(), [TeamSide::HOME, TeamSide::AWAY], [true, true]);

        $this->assertSame(0, $this->skoky($r));
        $this->assertNull($r->getNewState()->getBall()->getCarrierId());
        $this->assertSame([11, 7], $this->mic($r->getNewState()));
    }

    public function testJenJednaStranaAnoChyta(): void
    {
        $lide = [TeamSide::HOME, TeamSide::AWAY];
        $home = $this->prihraj($this->obeStrany(), $lide, [true, false]);
        $this->assertSame(1, $this->skoky($home));
        $this->assertSame(13, $home->getNewState()->getBall()->getCarrierId());

        $away = $this->prihraj($this->obeStrany(), $lide, [false, true]);
        $this->assertSame(1, $this->skoky($away));
        $this->assertSame(3, $away->getNewState()->getBall()->getCarrierId());
    }

    public function testAIDvaKandidatiTezeStranySkociJenNejlepsi(): void
    {
        // Hráč 3 AG4 ⇒ 3+, hráč 2 AG3 ⇒ 4+; chytání 3 chytí jen AG4. Dřív: nikdo.
        $r = (new ActionResolver(new FixedDiceRoller(array_merge(self::HODY_PRIHRAVKY, [3]))))
            ->resolve($this->dvaDomaci(agilita3: 4), ActionType::PASS, ['playerId' => 1, 'targetX' => 10, 'targetY' => 5]);

        $this->assertSame(1, $this->skoky($r));
        $this->assertSame(3, $r->getNewState()->getBall()->getCarrierId());
    }

    public function testAIObeStranySkociObeANikdoNechyta(): void
    {
        $r = $this->prihraj($this->obeStrany(), []);

        $this->assertSame(0, $this->skoky($r));
        $this->assertSame([11, 7], $this->mic($r->getNewState()));
    }

    public function testClovekNaTahuPrvniPakAISkoci(): void
    {
        $this->assertSame(13, $this->otazka($this->obeStrany(), [TeamSide::HOME])->getPlayerId());

        $ano = $this->prihraj($this->obeStrany(), [TeamSide::HOME], [true]);
        $this->assertSame(0, $this->skoky($ano), 'AI skočila taky ⇒ nikdo');
        $this->assertNull($ano->getNewState()->getBall()->getCarrierId());

        $ne = $this->prihraj($this->obeStrany(), [TeamSide::HOME], [false]);
        $this->assertSame(1, $this->skoky($ne));
        $this->assertSame(3, $ne->getNewState()->getBall()->getCarrierId());
    }

    public function testClovekMimoTahVidiZeAIUzSkocila(): void
    {
        $q = $this->otazka($this->obeStrany(), [TeamSide::AWAY]);
        $this->assertSame(3, $q->getPlayerId());
        $this->assertSame(13, $q->getAttemptingId());

        $ne = $this->prihraj($this->obeStrany(), [TeamSide::AWAY], [false]);
        $this->assertSame(1, $this->skoky($ne));
        $this->assertSame(13, $ne->getNewState()->getBall()->getCarrierId());
    }

    public function testOtazkaSKandidatyPrezijeUlozeniStavu(): void
    {
        $q = $this->prihraj($this->dvaDomaci(), [TeamSide::HOME], [true])->getNewState();
        $ulozena = GameState::fromArray(json_decode((string) json_encode($q->toArray()), true))->getPendingDivingCatch();

        $this->assertNotNull($ulozena);
        $this->assertSame(1, $ulozena->getOtherCandidates());
        $this->assertSame(2, $ulozena->getAttemptingId());
    }

    public function testVykopAIDvaPrijimajiciSkociJenJeden(): void
    {
        // Jako DivingCatchKoucTest::vykopPrijimajici: výkop na (6,7), rozptyl V o 1 ⇒ prázdné (7,7). Dva
        // přijímající s Diving Catch: 1 na (7,8) AG3, 4 na (6,8) AG4 ⇒ skočí 4, chytá 4. Dřív: nikdo,
        // odraz D8 4 z (7,7) na (8,8).
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 7, 8, agility: 3, skills: [SkillName::DivingCatch], id: 1)
            ->addPlayer(TeamSide::AWAY, 15, 7, id: 2)
            ->addPlayer(TeamSide::HOME, 6, 8, agility: 4, skills: [SkillName::DivingCatch], id: 4)
            ->build();
        $dice = new FixedDiceRoller([3, 1, 3, 4, 3, 3, 4]);
        $scatter = new ScatterCalculator();
        $ball = new BallResolver($dice, new TacklezoneCalculator(), $scatter);
        $r = (new KickoffResolver($dice, $scatter, $ball, new InjuryResolver()))->resolveKickoff($s, new Position(6, 7));

        $this->assertCount(1, array_filter($r['events'], static fn($e) => $e->getType() === 'diving_catch'));
        $this->assertSame(4, $r['state']->getBall()->getCarrierId());
    }
}
