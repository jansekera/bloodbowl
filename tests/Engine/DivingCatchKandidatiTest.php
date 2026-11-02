<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\ActionResult;
use App\DTO\GameState;
use App\DTO\PendingDivingCatchDTO;
use App\DTO\TeamStateDTO;
use App\Engine\ActionResolver;
use App\Engine\BallResolver;
use App\Engine\DivingCatchQuestion;
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

    /**
     * Recenze 30.10. D/1: pořadí stran u výkopu hlídal jen kód (mutace `naTahu: $receivingTeam->opponent()`
     * přežila celé tests/Engine). Ř. 986-990 "the coach whose turn is taking place must use his skill first"
     * — dopad výkopu je už v kole přijímajících (jako C++ `finishKickoff`). Jako DivingCatchKoucTest::vykop:
     * výkop na (10,7), rozptyl V o 2 ⇒ prázdné (12,7); kopající AWAY 2 s Diving Catch na lajně (13,7),
     * přijímající HOME 3 s Diving Catch na (11,8); oba kouči lidé. Aktivní tým při výkopu je schválně
     * KOPAJÍCÍ, aby test spadl i tehdy, kdyby se strana na tahu vzala z aktivního týmu.
     *
     * @param list<bool> $odpovedi
     */
    private function otazkaVykopu(array $odpovedi): DivingCatchQuestion
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 6, 7, agility: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 13, 7, agility: 3, skills: [SkillName::DivingCatch], id: 2)
            ->addPlayer(TeamSide::HOME, 11, 8, agility: 3, skills: [SkillName::DivingCatch], id: 3)
            ->withActiveTeam(TeamSide::AWAY)
            ->build();
        $dice = new FixedDiceRoller([3, 2, 3, 4, 3, 3, 7]);
        $scatter = new ScatterCalculator();
        $ball = new BallResolver($dice, new TacklezoneCalculator(), $scatter);
        $ball->setInteractiveDivingCatch([TeamSide::HOME, TeamSide::AWAY]);
        $ball->setDivingCatchAnswers($odpovedi);
        try {
            (new KickoffResolver($dice, $scatter, $ball, new InjuryResolver()))->resolveKickoff($s, new Position(10, 7));
        } catch (DivingCatchQuestion $q) {
            return $q;
        }
        self::fail('otázka nepřišla');
    }

    public function testVykopNaprvSePtaPrijimajiciPakKopajici(): void
    {
        $prvni = $this->otazkaVykopu([]);
        $this->assertSame(3, $prvni->playerId, 'napřed přijímající (je na tahu)');
        $this->assertNull($prvni->attemptingId);

        $druha = $this->otazkaVykopu([true]);
        $this->assertSame(2, $druha->playerId);
        $this->assertSame(3, $druha->attemptingId, 'kopající ví, že přijímající už skáče');
    }

    /**
     * Recenze 30.10. D/2: počet dalších kandidátů v otázce nepočítá ty, kdo skok už odmítli — od nich
     * riziko zrušení (ř. 8069-8071 "two or more players ATTEMPTING") nehrozí. Dřív pevně kandidáti - 1.
     */
    public function testDalsiKandidatiNepocitajiTyKdoOdmitli(): void
    {
        $this->assertSame(0, $this->otazka($this->dvaDomaci(), [TeamSide::HOME], [false])->getOtherCandidates(), 'hráč 2 odmítl');
        $this->assertSame(1, $this->otazka($this->dvaDomaci(), [TeamSide::HOME], [true])->getOtherCandidates(), 'hráč 2 skáče');

        // AI (HOME, na tahu) má dva kandidáty a skočí jen s jedním ⇒ kouč AWAY má jednoho dalšího, ne dva.
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 10, 7, agility: 3, skills: [SkillName::DivingCatch], id: 2)
            ->addPlayer(TeamSide::HOME, 9, 7, agility: 3, skills: [SkillName::DivingCatch], id: 3)
            ->addPlayer(TeamSide::AWAY, 11, 5, agility: 3, skills: [SkillName::DivingCatch], id: 13)
            ->withBallCarried(1)
            ->build();
        $q = $this->otazka($s, [TeamSide::AWAY]);
        $this->assertSame(13, $q->getPlayerId());
        $this->assertSame(1, $q->getOtherCandidates());
    }

    // ---- P239 (uživatel 2.11.2026), parita s C++ `DivingCatchAiChoice.*` a `KickoffLanding.AiKicker*` ----

    /**
     * Dva kandidáti HOME s vlastními skilly: 2 na (10,7) AG3, 3 na (9,7); HOME má `$prehozy` týmových přehozů.
     * Přihrávající má Pass: nepřesnou přihrávku (hod 2) AI přehodí — skillem, takže týmový přehoz zůstane
     * na chytání (bez Pass by ho spotřebovala přihrávka a test by o přehozu nic neřekl).
     *
     * @param list<SkillName> $skilly2
     * @param list<SkillName> $skilly3
     */
    private function dvaDomaciSkilly(array $skilly2, array $skilly3, int $agilita3, int $prehozy, bool $prehozPouzity = false): GameState
    {
        $tym = TeamStateDTO::create(1, 'Home', 'Human', TeamSide::HOME, $prehozy);

        return (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, skills: [SkillName::Pass], id: 1)
            ->addPlayer(TeamSide::HOME, 10, 7, agility: 3, skills: [SkillName::DivingCatch, ...$skilly2], id: 2)
            ->addPlayer(TeamSide::HOME, 9, 7, agility: $agilita3, skills: [SkillName::DivingCatch, ...$skilly3], id: 3)
            ->withHomeTeam($prehozPouzity ? $tym->withRerollUsed() : $tym)
            ->withBallCarried(1)
            ->build();
    }

    /** AI × AI přihrávka, chytání 4 (chytí AG3 i AG4) ⇒ nosič ukáže, kdo skočil. */
    private function aiSkokan(GameState $s): ?int
    {
        $r = (new ActionResolver(new FixedDiceRoller(array_merge(self::HODY_PRIHRAVKY, [4]))))
            ->resolve($s, ActionType::PASS, ['playerId' => 1, 'targetX' => 10, 'targetY' => 5]);
        $this->assertSame(1, $this->skoky($r));

        return $r->getNewState()->getBall()->getCarrierId();
    }

    /** Šance i s přehozem (Catch ř. 7992-7995 ⇒ p + (1−p)·p): AG3 s Catch 3/4 > AG4 bez přehozu 2/3. Dřív AG4. */
    public function testAIBezPrehozuCatchPredLepsiAgilitou(): void
    {
        $this->assertSame(2, $this->aiSkokan($this->dvaDomaciSkilly([SkillName::Catch], [], 4, 0)));
    }

    /** Týmový přehoz ve vlastním tahu se počítá: AG4 s přehozem 8/9 > AG3 s Catch 3/4. */
    public function testAIPrehozVeVlastnimTahuSePocita(): void
    {
        $this->assertSame(3, $this->aiSkokan($this->dvaDomaciSkilly([SkillName::Catch], [], 4, 3)));
    }

    /** Shoda (AG3 s Catch × AG3 s týmovým přehozem, obojí 3/4) ⇒ hráč s Catch, přehoz se šetří. Dřív první (2). */
    public function testAIPriShodeSkaceHracSCatch(): void
    {
        $this->assertSame(3, $this->aiSkokan($this->dvaDomaciSkilly([], [SkillName::Catch], 3, 3)));
    }

    /**
     * Loner (ř. 8287-8290) tlumí jen týmový přehoz (4+): AG3 Loner 5/8 < AG3 3/4 (bez brány shoda ⇒ hráč 2);
     * Catch neomezuje: AG3 s Catch a Loner 3/4 > AG4 bez přehozu 2/3.
     */
    public function testAILonerTlumiJenTymovyPrehoz(): void
    {
        $this->assertSame(3, $this->aiSkokan($this->dvaDomaciSkilly([SkillName::Loner], [], 3, 3)));
        $this->assertSame(2, $this->aiSkokan($this->dvaDomaciSkilly([SkillName::Catch, SkillName::Loner], [], 4, 0)));
    }

    /** Tah soupeře (FAQ ř. 9253): AWAY přehoz v zásobě nepomůže ⇒ AG3 s Catch 3/4 > AG4 2/3 (s přehozem by měl 8/9). */
    public function testAIVTahuSouperePrehozNeplati(): void
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 11, 5, agility: 4, skills: [SkillName::DivingCatch], id: 13)
            ->addPlayer(TeamSide::AWAY, 11, 7, agility: 3, skills: [SkillName::DivingCatch, SkillName::Catch], id: 14)
            ->withAwayTeam(TeamStateDTO::create(2, 'Away', 'Orc', TeamSide::AWAY, 2))
            ->withBallCarried(1)
            ->build();
        $this->assertSame(14, $this->aiSkokan($s));
    }

    /** Výkop (ř. 1261-1263): přehoz se nepočítá ani přijímajícím ⇒ AG3 s Catch skočí před AG4. Dřív AG4. */
    public function testVykopPrehozSeNepocita(): void
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 7, 8, agility: 3, skills: [SkillName::DivingCatch, SkillName::Catch], id: 1)
            ->addPlayer(TeamSide::AWAY, 15, 7, id: 2)
            ->addPlayer(TeamSide::HOME, 6, 8, agility: 4, skills: [SkillName::DivingCatch], id: 4)
            ->build();
        $dice = new FixedDiceRoller([3, 1, 3, 4, 3, 3, 4]);
        $scatter = new ScatterCalculator();
        $ball = new BallResolver($dice, new TacklezoneCalculator(), $scatter);
        $r = (new KickoffResolver($dice, $scatter, $ball, new InjuryResolver()))->resolveKickoff($s, new Position(6, 7));

        $this->assertSame(1, $r['state']->getBall()->getCarrierId());
    }

    /**
     * Kopající AI na lajně skočí, PRÁVĚ KDYŽ už skáče přijímající ⇒ dva pokusy, nikdo nechytá (ř. 8069-8071),
     * touchback nevznikne. Jako `otazkaVykopu`: dopad na prázdné (12,7), kopající AWAY 2 na (13,7), přijímající
     * HOME 3 na (11,8). `$lide` = kdo hraje jako člověk, `$odpovedi` = jeho odpovědi.
     *
     * @param list<TeamSide> $lide
     * @param list<bool> $odpovedi
     * @return array{state: GameState, events: list<\App\DTO\GameEvent>}
     */
    private function vykopKopajiciNaLajne(array $lide, array $odpovedi): array
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 6, 7, agility: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 13, 7, agility: 3, skills: [SkillName::DivingCatch], id: 2)
            ->addPlayer(TeamSide::HOME, 11, 8, agility: 3, skills: [SkillName::DivingCatch], id: 3)
            ->withActiveTeam(TeamSide::AWAY)
            ->build();
        $dice = new FixedDiceRoller([3, 2, 3, 4, 3, 3, 7, 1, 1, 1]);
        $scatter = new ScatterCalculator();
        $ball = new BallResolver($dice, new TacklezoneCalculator(), $scatter);
        $ball->setInteractiveDivingCatch($lide);
        $ball->setDivingCatchAnswers($odpovedi);

        return (new KickoffResolver($dice, $scatter, $ball, new InjuryResolver()))->resolveKickoff($s, new Position(10, 7));
    }

    public function testVykopKopajiciAISkociKdyzUzSkacePrijimajici(): void
    {
        foreach ([[[], []], [[TeamSide::HOME], [true]]] as [$lide, $odpovedi]) {
            $r = $this->vykopKopajiciNaLajne($lide, $odpovedi);
            $this->assertCount(0, array_filter($r['events'], static fn($e) => $e->getType() === 'diving_catch'), 'dva pokusy ⇒ nikdo');
            $this->assertNull($r['state']->getBall()->getCarrierId(), 'nikdo nechytá, žádný touchback');
            $this->assertSame([11, 7], $this->mic($r['state']), 'dopad na (12,7), odraz D8 7');
        }
    }

    /** Přijímající odmítne ⇒ kopající AI neskočí (sama by dala touchback); míč dopadne a odrazí se. */
    public function testVykopKopajiciAINeskociKdyzPrijimajiciOdmitne(): void
    {
        $r = $this->vykopKopajiciNaLajne([TeamSide::HOME], [false]);
        $this->assertCount(0, array_filter($r['events'], static fn($e) => $e->getType() === 'diving_catch'));
        $this->assertNull($r['state']->getBall()->getCarrierId());
        $this->assertSame([11, 7], $this->mic($r['state']));
    }

    /** Recenze P239 LOW-5: přehoz v tahu už použitý (ř. 934-935) se nepočítá ⇒ AG3 s Catch 3/4 > AG4 2/3. */
    public function testAIPrehozUzPouzityVTomtoTahuSeNepocita(): void
    {
        $this->assertSame(2, $this->aiSkokan($this->dvaDomaciSkilly([SkillName::Catch], [], 4, 3, prehozPouzity: true)));
    }

    /**
     * Recenze P239 LOW-3: šance s +1 za přesnou přihrávku jako skutečné chytání (ř. 891). Přesná přihrávka (6)
     * na prázdné (10,5); 2 na (10,6) AG5 bez Catch s přehozem, 3 na (11,6) AG4 s Catch. Bez +1: 35/36 > 8/9 ⇒ 2;
     * s +1 oba 2+ ⇒ shoda ⇒ hráč s Catch.
     */
    public function testAIPresnaPrihravkaPlusJednaVeSanci(): void
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 10, 6, agility: 5, skills: [SkillName::DivingCatch], id: 2)
            ->addPlayer(TeamSide::HOME, 11, 6, agility: 4, skills: [SkillName::DivingCatch, SkillName::Catch], id: 3)
            ->withBallCarried(1)
            ->build();
        $r = (new ActionResolver(new FixedDiceRoller([6, 3])))
            ->resolve($s, ActionType::PASS, ['playerId' => 1, 'targetX' => 10, 'targetY' => 5]);

        $this->assertSame(1, $this->skoky($r));
        $this->assertSame(3, $r->getNewState()->getBall()->getCarrierId());
    }

    /**
     * Recenze P239 LOW-1: Dump-Off se hází v tahu blokujícího; jeho hráč, který rozptýlený míč chytá skokem, je ve
     * SVÉM tahu ⇒ týmový přehoz smí (ř. 929-933). Jako DivingCatchTest::testDumpOffLandingNextToDivingCatchPlayer,
     * ale s Diving Catch na (8,7) hráč HOME: chytání 1, přehoz 6 ⇒ chytí. Dřív bez přehozu ⇒ odraz.
     */
    public function testDumpOffChytaBlokujiciSTymovymPrehozem(): void
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, agility: 3, skills: [SkillName::DumpOff], id: 2)
            ->addPlayer(TeamSide::AWAY, 8, 5, agility: 3, id: 3)
            ->addPlayer(TeamSide::HOME, 8, 7, agility: 3, skills: [SkillName::DivingCatch], id: 4)
            ->withBallCarried(2)
            ->build();
        $r = (new ActionResolver(new FixedDiceRoller([2, 3, 5, 7, 1, 6, 3, 3, 3])))
            ->resolve($s, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $this->assertSame(4, $r->getNewState()->getBall()->getCarrierId());
    }
}
