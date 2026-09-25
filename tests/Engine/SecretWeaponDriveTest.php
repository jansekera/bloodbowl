<?php
declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\ActionResolver;
use App\Engine\GameFlowResolver;
use App\Engine\RandomDiceRoller;
use App\Engine\FixedDiceRoller;
use App\Enum\ActionType;
use App\Enum\GamePhase;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * `rules_bb2016.txt` r. 8451-8454 (Secret Weapon): "Once a drive ends that this player
 * has played in at any point, the referee orders the player to be sent off ...
 * regardless of whether the player is still on the pitch or not."
 * ⛔ Engine do 25.09. vylucoval jen hrace, kteri byli na hristi PRAVE TED --
 *   Looney v KO nebo vytlaceny do davu se vratil dalsi drive.
 */
final class SecretWeaponDriveTest extends TestCase
{
    public function testSecretWeaponEjectedEvenWhenOffPitch(): void
    {
        $s = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->addPlayer(TeamSide::HOME, 6, 6, skills: [SkillName::SecretWeapon], id: 3)
            ->addPlayer(TeamSide::HOME, 6, 7, skills: [SkillName::SecretWeapon], id: 4)
            ->addPlayer(TeamSide::HOME, 6, 8, skills: [SkillName::SecretWeapon], id: 5)
            ->addPlayer(TeamSide::HOME, 6, 9, skills: [SkillName::SecretWeapon], id: 6)
            ->build();
        // 3: sundan do KO behem drivu; 5: vytlacen do davu (rezervy); 6: stoji na hristi;
        // 4: celou dobu na lavicce -- nehral.
        $s = $s->withPlayer($s->requirePlayer(3)->withPlayedThisDrive(true)->withState(PlayerState::KO)->withPosition(null));
        $s = $s->withPlayer($s->requirePlayer(4)->withState(PlayerState::OFF_PITCH)->withPosition(null));
        $s = $s->withPlayer($s->requirePlayer(5)->withPlayedThisDrive(true)->withState(PlayerState::OFF_PITCH)->withPosition(null));

        $r = (new GameFlowResolver(new FixedDiceRoller([5, 5, 5, 5])))->resolvePostTouchdown($s);

        $this->assertSame(PlayerState::EJECTED, $r['state']->requirePlayer(3)->getState(), 'KO hrac v drivu hral');
        $this->assertSame(PlayerState::EJECTED, $r['state']->requirePlayer(5)->getState(), 'vytlaceny do davu v drivu hral');
        $this->assertSame(PlayerState::EJECTED, $r['state']->requirePlayer(6)->getState(), 'na hristi = hral');
        $this->assertSame(PlayerState::OFF_PITCH, $r['state']->requirePlayer(4)->getState(), 'lavicka nehrala');
    }

    /** Priznak se musi opravdu nastavit pri vykopu -- jinak by oprava byla mrtva. */
    public function testKickoffMarksPlayersOnPitchAsPlayed(): void
    {
        $b = (new GameStateBuilder())->withPhase(GamePhase::SETUP)->withActiveTeam(TeamSide::HOME);
        for ($i = 0; $i < 3; $i++) {
            $b->addPlayer(TeamSide::HOME, 12, 4 + $i, id: $i + 1);
            $b->addPlayer(TeamSide::AWAY, 13, 4 + $i, id: $i + 12);
        }
        for ($i = 0; $i < 8; $i++) {
            $b->addPlayer(TeamSide::HOME, 6, $i + 3, id: $i + 4);
            $b->addPlayer(TeamSide::AWAY, 19, $i + 3, id: $i + 15);
        }
        $b->addPlayer(TeamSide::HOME, 1, 1, id: 30);
        $s = $b->build();
        $s = $s->withPlayer($s->requirePlayer(30)->withState(PlayerState::OFF_PITCH)->withPosition(null));

        $r = (new ActionResolver(new RandomDiceRoller()))->resolve($s, ActionType::END_SETUP, []);

        $this->assertTrue($r->getNewState()->requirePlayer(1)->isPlayedThisDrive());
        $this->assertTrue($r->getNewState()->requirePlayer(20)->isPlayedThisDrive());
        $this->assertFalse($r->getNewState()->requirePlayer(30)->isPlayedThisDrive(), 'lavicka nehraje');
    }
}
