<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\GameState;
use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Enum\ActionType;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * P81 krok 3 (30.09.2026), port C++ 771d1ecc. `rules_bb2016.txt` r. 608-611:
 *   follow-up je VOLBA utocnika („may choose to follow up"). Povinny jen pro
 *   Frenzy (r. 8138) -- a Ball & Chain ma vlastni handler. PHP nasledoval vzdy.
 *   Clovek voli spolu s kostkou (`CHOOSE_BLOCK_DIE`, parametr `followUp`),
 *   AI podle pravidla z C++ `wantsFollowUp`: v blitzu se zbytkem pohybu
 *   nenasleduje na pole s vic tackle zonami, nez kde stoji.
 */
final class FollowUpChoiceTest extends TestCase
{
    private function plain(): GameState
    {
        return (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2)
            ->withBallOffPitch()
            ->build();
    }

    /** Uvolnene pole (6,7) bude v zone hrace 3 i odtlaceneho obrance; (5,7) v zadne. */
    private function crowded(): GameState
    {
        return (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2)
            ->addPlayer(TeamSide::AWAY, 7, 8, id: 3)
            ->withBallOffPitch()
            ->build();
    }

    private function chooseWith(GameState $state, bool $followUp): GameState
    {
        $resolver = new ActionResolver(new FixedDiceRoller([3, 3, 3, 3]));   // PUSHED
        $resolver->setInteractiveBlocks(true);
        $pending = $resolver->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);
        $this->assertNotNull($pending->getNewState()->getPendingBlock(), 'fixtura: blok neceka na volbu');
        return $resolver->resolve(
            $pending->getNewState(),
            ActionType::CHOOSE_BLOCK_DIE,
            ['faceIndex' => 0, 'followUp' => $followUp],
        )->getNewState();
    }

    public function testTheCoachMayDeclineTheFollowUp(): void
    {
        $after = $this->chooseWith($this->plain(), false);
        $this->assertSame(5, $after->requirePlayer(1)->requirePosition()->getX(), 'utocnik nasledoval, ackoli nechtel');
        $this->assertSame(7, $after->requirePlayer(2)->requirePosition()->getX(), 'fixtura: obrance nebyl odtlacen');
    }

    public function testFollowingUpIsStillTheDefault(): void
    {
        // Bez parametru plati dosavadni chovani (nasleduje).
        $resolver = new ActionResolver(new FixedDiceRoller([3, 3, 3, 3]));
        $resolver->setInteractiveBlocks(true);
        $pending = $resolver->resolve($this->plain(), ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);
        $after = $resolver->resolve($pending->getNewState(), ActionType::CHOOSE_BLOCK_DIE, ['faceIndex' => 0])->getNewState();
        $this->assertSame(6, $after->requirePlayer(1)->requirePosition()->getX());
    }

    public function testFrenzyMustFollowUpWhateverTheCoachSays(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, skills: [SkillName::Frenzy], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2)
            ->withBallOffPitch()
            ->build();
        $after = $this->chooseWith($state, false);
        $this->assertSame(6, $after->requirePlayer(1)->requirePosition()->getX(), 'Frenzy musi nasledovat (r. 8138)');
    }

    public function testTheAiDoesNotFollowIntoMoreTacklezonesWhenTheBlitzCanStillMove(): void
    {
        $resolver = new ActionResolver(new FixedDiceRoller([3, 3, 3, 3]));
        $r = $resolver->resolve($this->crowded(), ActionType::BLITZ, ['playerId' => 1, 'targetId' => 2]);
        $this->assertFalse($r->isTurnover());
        $this->assertSame(
            5,
            $r->getNewState()->requirePlayer(1)->requirePosition()->getX(),
            'AI v blitzu nasledovala na pole se dvema zonami',
        );
    }

    public function testOutsideABlitzTheAiStillFollowsUp(): void
    {
        // Hlidaci test: mimo blitz neni co setrit, pole zdarma se bere.
        $resolver = new ActionResolver(new FixedDiceRoller([3, 3, 3, 3]));
        $r = $resolver->resolve($this->crowded(), ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);
        $this->assertSame(6, $r->getNewState()->requirePlayer(1)->requirePosition()->getX());
    }
}
