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
 * P280, P281 (audit goblinů 10.11.2026), parita s C++ ball_and_chain_handler.cpp / ttm_handler.cpp.
 *
 * - P280: vstup na pole s míčem = pokus o zvednutí (`rules_bb2016.txt` ř. 436-438); No Hands ⇒ „will bounce, and will
 *   cause a turnover if it is his team's turn“ (ř. 8320-8322).
 * - P281: sražený hráč týmu na tahu = turnover (ř. 366-367) — i spoluhráč sražený blokem Ball & Chain („friend or
 *   foe“, ř. 7820-7822) nebo hozeným hráčem, který na něj dopadl (ř. 8617-8619).
 */
final class OwnKnockdownTurnoverTest extends TestCase
{
    public function testP280BallAndChainVstoupiNaMicTurnover(): void
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 10, 7, movement: 2, strength: 7, id: 1, skills: [SkillName::BallAndChain, SkillName::NoHands])
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 12)
            ->withBallOnGround(11, 7)
            ->build();
        // rovně na míč, odskok D8 3 = východ; druhý krok už ne
        $r = (new ActionResolver(new FixedDiceRoller([3, 3, 3, 3])))->resolve($s, ActionType::BALL_AND_CHAIN, ['playerId' => 1]);

        $this->assertTrue($r->isTurnover());
        $this->assertSame(11, $r->getNewState()->requirePlayer(1)->getPosition()?->getX(), 'pohyb končí');
    }

    public function testP281BallAndChainSraziSpoluhraceTurnover(): void
    {
        // naši ve všech čtyřech směrech ⇒ každé natočení stojí stejně; při shodě k soupeři (východ), rovně = (11,7)
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 10, 7, movement: 1, strength: 7, id: 1, skills: [SkillName::BallAndChain, SkillName::NoHands])
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 12)
            ->addPlayer(TeamSide::HOME, 11, 7, id: 2)
            ->addPlayer(TeamSide::HOME, 9, 7, id: 3)
            ->addPlayer(TeamSide::HOME, 10, 6, id: 4)
            ->addPlayer(TeamSide::HOME, 10, 8, id: 5)
            ->withBallOffPitch()
            ->build();
        // rovně ⇒ blok, všechny kostky DD (vynucené), odtlačení na (12,7), brnění 1+1
        $r = (new ActionResolver(new FixedDiceRoller([3, 6, 6, 6, 1, 1, 1, 1])))->resolve($s, ActionType::BALL_AND_CHAIN, ['playerId' => 1]);

        $this->assertNotSame(PlayerState::STANDING, $r->getNewState()->requirePlayer(2)->getState());
        $this->assertTrue($r->isTurnover());
    }

    public function testP281HozenyDopadneNaSpoluhraceTurnover(): void
    {
        // Troll (5,5) hází goblina (6,5) na (8,5); rozptyly 3, 7, 7 ⇒ (7,5), kde stojí NÁŠ hráč 3.
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 5, skills: [SkillName::ThrowTeamMate], id: 1)
            ->addPlayer(TeamSide::HOME, 6, 5, skills: [SkillName::RightStuff], id: 2)
            ->addPlayer(TeamSide::HOME, 7, 5, id: 3)
            ->withBallOffPitch()
            ->build();
        // přesnost 4, rozptyly 3/7/7, brnění 3 2+2, rozptyl D8 3 ⇒ (8,5), brnění hozeného 2+2
        $r = (new ActionResolver(new FixedDiceRoller([4, 3, 7, 7, 2, 2, 3, 2, 2])))->resolve($s, ActionType::THROW_TEAM_MATE, [
            'playerId' => 1, 'targetId' => 2, 'targetX' => 8, 'targetY' => 5,
        ]);

        $this->assertSame(PlayerState::PRONE, $r->getNewState()->requirePlayer(3)->getState());
        $this->assertTrue($r->isTurnover());
    }

    public function testP284MightyBlowPlatiIProBlokBallAndChain(): void
    {
        // ř. 7820-7821 „normal blocking rules“ + Mighty Blow ř. 8293-8295. ST 7 proti 3 = 3 kostky (útočník volí) DD;
        // odtlačení na (12,7); brnění 4+4 = 8 = AV ⇒ s MB 9 prorazí, zranění 3+3 = omráčen
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 10, 7, movement: 1, strength: 7, id: 1, skills: [SkillName::BallAndChain, SkillName::NoHands, SkillName::MightyBlow])
            ->addPlayer(TeamSide::AWAY, 11, 7, armour: 8, id: 12)
            ->withBallOffPitch()
            ->build();
        $r = (new ActionResolver(new FixedDiceRoller([3, 6, 6, 6, 4, 4, 3, 3])))->resolve($s, ActionType::BALL_AND_CHAIN, ['playerId' => 1]);

        $this->assertSame(PlayerState::STUNNED, $r->getNewState()->requirePlayer(12)->getState());
    }
}
