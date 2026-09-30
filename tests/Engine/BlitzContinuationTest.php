<?php
declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\GameState;
use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Engine\RulesEngine;
use App\Enum\ActionType;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * P81 krok 2 (30.09.2026), port C++ 771d1ecc (M1/N10). `rules_bb2016.txt`
 *   r. 551-552: „The player may carry on moving after the effects of the block
 *   have been worked out if he has any squares of movement left."
 *   PHP blitz po rane konci. Pokracovani = bezna akce MOVE tehoz hrace;
 *   propadne, jakmile jedna jiny hrac (aktivace musi byt dokoncena) nebo
 *   skonci kolo. Pila v blitzu pokracovani nema („cannot continue moving
 *   after using it", r. 8016-8017).
 */
final class BlitzContinuationTest extends TestCase
{
    /**
     * Utocnik MA6 v (5,7), obrance v (6,7), spoluhrac daleko.
     *
     * @param list<SkillName> $attackerSkills
     */
    private function state(array $attackerSkills = []): GameState
    {
        return (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, skills: $attackerSkills, id: 1)
            ->addPlayer(TeamSide::HOME, 1, 1, id: 3)
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2)
            ->withBallOffPitch()
            ->build();
    }

    /** @return list<array<string, mixed>> */
    private function movesOf(GameState $state, int $playerId): array
    {
        return array_values(array_filter(
            (new RulesEngine())->getAvailableActions($state),
            fn($a) => $a['type'] === ActionType::MOVE->value && ($a['playerId'] ?? null) === $playerId,
        ));
    }

    /**
     * Blitz: POW (6) na obrance, brneni 1+1; obrance lezi v (7,7), utocnik follow-upem v (6,7).
     *
     * @param list<int> $extraDice
     */
    private function blitzPow(GameState $state, array $extraDice = []): GameState
    {
        $r = (new ActionResolver(new FixedDiceRoller([...$extraDice, 6, 1, 1, 1, 1])))
            ->resolve($state, ActionType::BLITZ, ['playerId' => 1, 'targetId' => 2]);
        $this->assertFalse($r->isTurnover());
        return $r->getNewState();
    }

    public function testTheBlitzerMayCarryOnMovingAfterTheBlock(): void
    {
        $after = $this->blitzPow($this->state());
        $this->assertSame(PlayerState::PRONE, $after->requirePlayer(2)->getState(), 'fixtura: obrance nelezi');
        $this->assertNotEmpty($this->movesOf($after, 1), 'po rane se nenabizi pohyb');

        $r = (new ActionResolver(new FixedDiceRoller([])))
            ->resolve($after, ActionType::MOVE, ['playerId' => 1, 'x' => 6, 'y' => 5]);
        $p = $r->getNewState()->requirePlayer(1);
        $this->assertSame([6, 5], [$p->requirePosition()->getX(), $p->requirePosition()->getY()]);
        $this->assertSame(3, $p->getMovementRemaining(), '6 - 1 za ranu - 2 kroky');
        $this->assertEmpty($this->movesOf($r->getNewState(), 1), 'pokracovani jde jen jednou');
    }

    public function testTheContinuationLapsesWhenAnotherPlayerActs(): void
    {
        $after = $this->blitzPow($this->state());
        $r = (new ActionResolver(new FixedDiceRoller([])))
            ->resolve($after, ActionType::MOVE, ['playerId' => 3, 'x' => 2, 'y' => 1]);
        $this->assertEmpty($this->movesOf($r->getNewState(), 1),
            'blitzujici smi pokracovat i po aktivaci jineho hrace');
    }

    public function testTheContinuationIsTheSameActionSoNoSecondBoneHeadRoll(): void
    {
        $after = $this->blitzPow($this->state([SkillName::BoneHead]), [4]);   // Bone-head 4 prosel
        // Prazdna kostka: kdyby se Bone-head hazel znovu, spadne to na „no more rolls".
        $r = (new ActionResolver(new FixedDiceRoller([])))
            ->resolve($after, ActionType::MOVE, ['playerId' => 1, 'x' => 6, 'y' => 5]);
        $this->assertSame(5, $r->getNewState()->requirePlayer(1)->requirePosition()->getY());
    }

    public function testAChainsawBlitzHasNoContinuation(): void
    {
        // pila 4 = zasah · brneni 1+1 (+3) neprorazi
        $r = (new ActionResolver(new FixedDiceRoller([4, 1, 1, 1, 1])))
            ->resolve($this->state([SkillName::Chainsaw]), ActionType::BLITZ, ['playerId' => 1, 'targetId' => 2]);
        $this->assertEmpty($this->movesOf($r->getNewState(), 1));
    }

    public function testTheContinuationSurvivesSerialisation(): void
    {
        $after = $this->blitzPow($this->state());
        $restored = GameState::fromArray($after->toArray());
        $this->assertSame(1, $restored->getBlitzContinuationPlayerId());
    }
}
