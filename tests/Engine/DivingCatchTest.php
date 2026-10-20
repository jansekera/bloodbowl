<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\ActionResult;
use App\DTO\GameState;
use App\DTO\MatchPlayerDTO;
use App\Engine\ActionResolver;
use App\Engine\BallResolver;
use App\Engine\FixedDiceRoller;
use App\Engine\InjuryResolver;
use App\Engine\KickoffResolver;
use App\Engine\ScatterCalculator;
use App\Engine\TacklezoneCalculator;
use App\Enum\ActionType;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use App\ValueObject\Position;
use PHPUnit\Framework\TestCase;

require_once __DIR__ . '/../../cli/tv1500_rosters.php';

/**
 * Diving Catch — chytání přihrávky, která dopadne VEDLE hráče (P221).
 *
 * `rules_bb2016.txt` ř. 8065-8071: "the player can attempt to catch any pass, kick off or crowd
 *   throw-in, but not bouncing ball, that would land in an EMPTY square IN ONE OF HIS TACKLE ZONES
 *   as if it had landed in his own square WITHOUT LEAVING HIS CURRENT SQUARE. A failed catch will
 *   bounce from the Diving Catch player's square. If there are two or more players attempting to
 *   use this skill then they get in each other's way and neither can use it."
 * ř. 7983-7986 (a obdobně 8186, 8401): hráč bez tackle zón "may not catch".
 *
 * Všechny testy: nepřesná přihrávka z (5,5) na (10,5), přesnost 2, týmový přehoz 2, rozptyl
 * J, V, Z ⇒ míč dopadá na prázdné pole (10,6). Hráč s Diving Catch stojí na (10,7).
 */
final class DivingCatchTest extends TestCase
{
    /** @param list<int> $tail kostky po rozptylu */
    private function inaccuratePassLandingAt10x6(GameState $state, array $tail): ActionResult
    {
        $resolver = new ActionResolver(new FixedDiceRoller(array_merge([2, 2, 5, 3, 7], $tail)));

        return $resolver->resolve($state, ActionType::PASS, ['playerId' => 1, 'targetX' => 10, 'targetY' => 5]);
    }

    private function builderWithThrowerAndDivingCatcher(): GameStateBuilder
    {
        return (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 10, 7, agility: 3, skills: [SkillName::DivingCatch], id: 2)
            ->withBallCarried(1);
    }

    /** @return list<string> */
    private function eventTypes(ActionResult $result): array
    {
        return array_map(static fn($e) => $e->getType(), $result->getEvents());
    }

    private function assertBallOnGroundAt(int $x, int $y, GameState $state, string $message): void
    {
        $ball = $state->getBall();
        $this->assertFalse($ball->isHeld(), $message . ' (míč nemá nikdo držet)');
        $pos = $ball->getPosition();
        $this->assertNotNull($pos, $message);
        $this->assertSame([$x, $y], [$pos->getX(), $pos->getY()], $message);
    }

    private function assertAt(int $x, int $y, MatchPlayerDTO $player, string $message): void
    {
        $pos = $player->getPosition();
        $this->assertNotNull($pos, $message);
        $this->assertSame([$x, $y], [$pos->getX(), $pos->getY()], $message);
    }

    public function testDivingCatchCatchesWithoutLeavingHisSquare(): void
    {
        // 4 = chycení (AG3, bez zón ⇒ 4+).
        $result = $this->inaccuratePassLandingAt10x6($this->builderWithThrowerAndDivingCatcher()->build(), [4]);

        $after = $result->getNewState();
        $this->assertContains('diving_catch', $this->eventTypes($result));
        $this->assertSame(2, $after->getBall()->getCarrierId());
        $this->assertAt(10, 7, $after->requirePlayer(2), '"without leaving his current square" (ř. 8068)');
        $ballPos = $after->getBall()->getPosition();
        $this->assertNotNull($ballPos);
        $this->assertSame([10, 7], [$ballPos->getX(), $ballPos->getY()], 'míč je u nosiče na jeho poli');
    }

    public function testFailedDivingCatchBouncesFromTheDivingCatchPlayersSquare(): void
    {
        // Chycení 1 = neúspěch (týmový přehoz padl na přesnost), odskok 5 = jih: z pole hráče
        // (10,7) na (10,8). Stará mechanika hráče přesunula na (10,6) a odrážela odtud ⇒ (10,7).
        $result = $this->inaccuratePassLandingAt10x6($this->builderWithThrowerAndDivingCatcher()->build(), [1, 5]);

        $after = $result->getNewState();
        $this->assertBallOnGroundAt(10, 8, $after, '"A failed catch will bounce from the Diving Catch player\'s square" (ř. 8068-8069)');
        $this->assertAt(10, 7, $after->requirePlayer(2), 'ani při neúspěchu se hráč nepohne');
    }

    public function testDivingCatchNeedsHisTacklezones(): void
    {
        $state = $this->builderWithThrowerAndDivingCatcher()->build();
        $state = $state->withPlayer($state->requirePlayer(2)->withLostTacklezones(true));
        $this->assertSame(PlayerState::STANDING, $state->requirePlayer(2)->getState(), 'sebekontrola: stojí');

        // 4 = odskok z (10,6) na JV (11,7). Stará mechanika: 4 je hod na chycení (4+).
        $result = $this->inaccuratePassLandingAt10x6($state, [4]);

        $this->assertNotContains('diving_catch', $this->eventTypes($result), 'bez zón není „square in one of his tackle zones“ (ř. 8067) a „may not catch“ (ř. 7984)');
        $this->assertBallOnGroundAt(11, 7, $result->getNewState(), 'míč odskočí z pole dopadu');
    }

    public function testDivingCatchNeedsAnEmptyLandingSquare(): void
    {
        // Na poli dopadu leží hráč ⇒ pole není "empty square" (ř. 8067); míč od něj odskočí (ř. 893-896).
        $state = $this->builderWithThrowerAndDivingCatcher()
            ->addPronePlayer(TeamSide::AWAY, 10, 6, id: 3)
            ->build();

        $result = $this->inaccuratePassLandingAt10x6($state, [4]);

        $this->assertNotContains('diving_catch', $this->eventTypes($result));
        $this->assertBallOnGroundAt(11, 7, $result->getNewState(), 'míč odskočí od ležícího na JV');
        $this->assertAt(10, 7, $result->getNewState()->requirePlayer(2), 'hráč s Diving Catch se na pole ležícího nepřesune');
    }

    public function testTwoDivingCatchPlayersGetInEachOthersWay(): void
    {
        $state = $this->builderWithThrowerAndDivingCatcher()
            ->addPlayer(TeamSide::AWAY, 9, 7, agility: 3, skills: [SkillName::DivingCatch], id: 3)
            ->build();

        // 4 = odskok z (10,6) na (11,7). Stará mechanika: první nalezený se vrhne a hází.
        $result = $this->inaccuratePassLandingAt10x6($state, [4]);

        $this->assertNotContains('diving_catch', $this->eventTypes($result), '"they get in each other\'s way and neither can use it" (ř. 8069-8071)');
        $this->assertBallOnGroundAt(11, 7, $result->getNewState(), 'míč odskočí z pole dopadu');
    }

    public function testWoodElfTv1500CatcherCatchesWithModifiersOfHisOwnSquare(): void
    {
        // Pozitivní kontrola na skutečné soupisce (cli/tv1500_rosters.php, Wood Elf Catcher AG4,
        // Catch, Dodge, Sprint, Block, Diving Catch). Soupeř na (11,5) dává zónu na pole dopadu
        // (10,6), ne na hráčovo pole (10,7): "as if it had landed in his own square" (ř. 8068)
        // ⇒ cíl 7-4 = 3+. Stará mechanika (hráč na poli dopadu, jedna zóna) by chtěla 4+.
        $roster = getTv1500RaceRoster(TeamSide::HOME, 'Wood Elf');
        $catchers = array_values(array_filter($roster, static fn(MatchPlayerDTO $p): bool => $p->hasSkill(SkillName::DivingCatch)));
        $this->assertNotEmpty($catchers, 'soupiska TV1500 elfů má hráče s Diving Catch');
        $elf = $catchers[0]->withPosition(new Position(10, 7))->withState(PlayerState::STANDING);

        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 11, 5, id: 30)
            ->withBallCarried(1)
            ->build()
            ->withPlayer($elf);

        $result = $this->inaccuratePassLandingAt10x6($state, [3]);

        $after = $result->getNewState();
        $this->assertSame($elf->getId(), $after->getBall()->getCarrierId(), 'chytil na 3+ (modifikátory jeho pole)');
        $this->assertAt(10, 7, $after->requirePlayer($elf->getId()), 'a zůstal na svém poli');
        $this->assertFalse($result->isTurnover());
    }

    // --- P223: Diving Catch na VŠECH cestách, kde míč dopadá ze vzduchu ---------------------
    // ř. 8066-8067: "any pass, kick off or crowd throw-in, but not bouncing ball, that would land
    // in an empty square in one of his tackle zones". Dřív jen nepřesná přihrávka.

    public function testAccuratePassToEmptySquareIsCaughtWithAccuratePassModifierOnly(): void
    {
        // Přesná přihrávka (6 na 4+) na prázdné (10,5); hráč s Diving Catch na (10,6).
        // Chytá "as if it had landed in his own square" (ř. 8068) ⇒ "Catching an accurate pass +1"
        // (ř. 891), ale +1 z Diving Catch ne — ten je jen pro "accurate pass TARGETED TO HIS SQUARE"
        // (ř. 8064). AG3: 7-3-1 = 3+. Stará mechanika: odskok D8 3 = V na (11,5).
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, id: 1)
            ->addPlayer(TeamSide::HOME, 10, 6, agility: 3, skills: [SkillName::DivingCatch], id: 2)
            ->withBallCarried(1)
            ->build();

        $result = (new ActionResolver(new FixedDiceRoller([6, 3])))
            ->resolve($state, ActionType::PASS, ['playerId' => 1, 'targetX' => 10, 'targetY' => 5]);

        $after = $result->getNewState();
        $this->assertContains('diving_catch', $this->eventTypes($result));
        $this->assertSame(2, $after->getBall()->getCarrierId());
        $this->assertAt(10, 6, $after->requirePlayer(2), 'chytá ze svého pole');
        $catches = array_values(array_filter($result->getEvents(), static fn($e) => $e->getType() === 'catch'));
        $this->assertSame(3, $catches[0]->getData()['target'], 'jen +1 za přesnou přihrávku (ř. 891), ne +2');
        $this->assertFalse($result->isTurnover());
    }

    public function testHailMaryLandingNextToDivingCatchPlayer(): void
    {
        // Hail Mary (vždy nepřesná, ř. 8165-8171) z (2,5) na (20,5): hod 2 = ne fumble,
        // rozptyl J, V, Z ⇒ (20,6). Hráč s Diving Catch na (20,7) chytá 4 (AG3 4+).
        // Stará mechanika: odskok D8 4 = JV na (21,7), turnover.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 2, 5, agility: 3, skills: [SkillName::HailMaryPass], id: 1)
            ->addPlayer(TeamSide::HOME, 20, 7, agility: 3, skills: [SkillName::DivingCatch], id: 2)
            ->withBallCarried(1)
            ->build();

        $result = (new ActionResolver(new FixedDiceRoller([2, 5, 3, 7, 4])))
            ->resolve($state, ActionType::PASS, ['playerId' => 1, 'targetX' => 20, 'targetY' => 5]);

        $this->assertContains('diving_catch', $this->eventTypes($result));
        $this->assertSame(2, $result->getNewState()->getBall()->getCarrierId());
        $this->assertAt(20, 7, $result->getNewState()->requirePlayer(2), 'chytá ze svého pole');
        $this->assertFalse($result->isTurnover());
    }

    public function testDumpOffLandingNextToDivingCatchPlayer(): void
    {
        // Dump-Off je přihrávka (ř. 8094) ⇒ Diving Catch platí. Cíl = hráč 3 na (8,5);
        // přesnost 2 = nepřesná; rozptyl V (9,5), J (9,6), Z (8,6) = prázdné pole vedle hráče 4
        // s Diving Catch na (8,7); ten chytá 6. Pak blok 3 = odstrčení.
        // Stará mechanika: z (8,6) odskok D8 6 = JZ na (7,7), míč zůstane ležet.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, agility: 3, skills: [SkillName::DumpOff], id: 2)
            ->addPlayer(TeamSide::AWAY, 8, 5, agility: 3, id: 3)
            ->addPlayer(TeamSide::AWAY, 8, 7, agility: 3, skills: [SkillName::DivingCatch], id: 4)
            ->withBallCarried(2)
            ->build();

        $result = (new ActionResolver(new FixedDiceRoller([2, 3, 5, 7, 6, 3])))
            ->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $this->assertContains('diving_catch', $this->eventTypes($result));
        $this->assertSame(4, $result->getNewState()->getBall()->getCarrierId());
        $this->assertAt(8, 7, $result->getNewState()->requirePlayer(4), 'chytá ze svého pole');
    }

    private function kickoffResolver(FixedDiceRoller $dice): KickoffResolver
    {
        $scatter = new ScatterCalculator();

        return new KickoffResolver($dice, $scatter, new BallResolver($dice, new TacklezoneCalculator(), $scatter), new InjuryResolver());
    }

    public function testKickOffLandingNextToDivingCatchPlayer(): void
    {
        // Výkop na (6,7), rozptyl D8 3 = V, D6 1 ⇒ prázdné (7,7). Tabulka 3+4 = Changing Weather
        // (3,3). Hráč přijímajícího týmu s Diving Catch na (7,8) chytá 4 (AG3 4+).
        // Stará mechanika: odskok D8 4 = JV na (8,8).
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 7, 8, agility: 3, skills: [SkillName::DivingCatch], id: 1)
            ->addPlayer(TeamSide::AWAY, 15, 7, id: 2)
            ->build();

        $result = $this->kickoffResolver(new FixedDiceRoller([3, 1, 3, 4, 3, 3, 4]))
            ->resolveKickoff($state, new Position(6, 7));

        $types = array_map(static fn($e) => $e->getType(), $result['events']);
        $this->assertContains('diving_catch', $types);
        $this->assertSame(1, $result['state']->getBall()->getCarrierId());
        $this->assertAt(7, 8, $result['state']->requirePlayer(1), 'chytá ze svého pole');
    }

    public function testKickingDivingCatchPlayerOnTheLineDoesNotDiveIntoATouchback(): void
    {
        // OPRAVENO 20. 10. 2026 (uživatel: „proč by automaticky volil použití diving catch, ať je
        // touchback — tyto skilly nejsou povinné“). Výkop na (10,7), rozptyl V o 2 ⇒ prázdné (12,7).
        // Jediný hráč s Diving Catch je KOPAJÍCÍ na lajně (13,7); skokem by míč „dopadl“ na jeho pole
        // v kopající polovině = touchback pro soupeře (ř. 8068, 281-282) ⇒ skill je volba („can
        // attempt“, ř. 8066), neskočí. Míč odskočí z (12,7) D8 7 = Z na (11,7) a leží. Dřív: touchback.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 6, 7, agility: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 13, 7, agility: 3, skills: [SkillName::DivingCatch], id: 2)
            ->build();

        $result = $this->kickoffResolver(new FixedDiceRoller([3, 2, 3, 4, 3, 3, 7]))
            ->resolveKickoff($state, new Position(10, 7));

        $types = array_map(static fn($e) => $e->getType(), $result['events']);
        $this->assertNotContains('diving_catch', $types);
        $this->assertNotContains('touchback', $types);
        $ball = $result['state']->getBall();
        $this->assertNull($ball->getCarrierId(), 'míč nikdo nedrží');
        $this->assertSame([11, 7], [$ball->getPosition()->getX(), $ball->getPosition()->getY()], 'odraz z pole dopadu');
    }

    public function testCrowdThrowInLandingNextToDivingCatchPlayer(): void
    {
        // Vhazování od (0,7): šablona 3 = kolmo V, 2D6 = 3+3 ⇒ 5 polí na prázdné (5,7).
        // Hráč s Diving Catch na (5,8) chytá 4 (AG3 4+). Stará mechanika: odskok D8 4 = JV na (6,8).
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 8, agility: 3, skills: [SkillName::DivingCatch], id: 1)
            ->withBallOnGround(0, 7)
            ->build();
        $dice = new FixedDiceRoller([3, 3, 3, 4]);

        $result = (new BallResolver($dice, new TacklezoneCalculator(), new ScatterCalculator()))
            ->resolveThrowIn($state, new Position(0, 7), new Position(-1, 7));

        $types = array_map(static fn($e) => $e->getType(), $result['events']);
        $this->assertContains('diving_catch', $types);
        $this->assertSame(1, $result['state']->getBall()->getCarrierId());
        $this->assertAt(5, 8, $result['state']->requirePlayer(1), 'chytá ze svého pole');
    }

    public function testBouncingBallIsNotDivingCaught(): void
    {
        // Strážní test (prošel i před P223): "but not bouncing ball" (ř. 8066). Odskok z (5,6)
        // D8 5 = J ⇒ prázdné (5,7) vedle hráče s Diving Catch na (5,8) — míč zůstane ležet.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 8, agility: 3, skills: [SkillName::DivingCatch], id: 1)
            ->withBallOnGround(5, 6)
            ->build();
        $dice = new FixedDiceRoller([5, 6]);

        $result = (new BallResolver($dice, new TacklezoneCalculator(), new ScatterCalculator()))
            ->resolveBounce($state, new Position(5, 6));

        $types = array_map(static fn($e) => $e->getType(), $result['events']);
        $this->assertNotContains('diving_catch', $types);
        $this->assertBallOnGroundAt(5, 7, $result['state'], 'odskakující míč se chytat nesmí');
    }
}
