<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\ActionResult;
use App\DTO\GameState;
use App\DTO\MatchPlayerDTO;
use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
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
}
