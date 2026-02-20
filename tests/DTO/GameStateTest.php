<?php
declare(strict_types=1);

namespace App\Tests\DTO;

use App\DTO\BallState;
use App\DTO\GameState;
use App\DTO\MatchPlayerDTO;
use App\DTO\TeamStateDTO;
use App\Enum\GamePhase;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use App\ValueObject\PlayerStats;
use App\ValueObject\Position;
use PHPUnit\Framework\TestCase;

final class GameStateTest extends TestCase
{
    public function testCreateInitializesCorrectly(): void
    {
        $home = TeamStateDTO::create(1, 'Home', 'Human', TeamSide::HOME, 3);
        $away = TeamStateDTO::create(2, 'Away', 'Orc', TeamSide::AWAY, 2);

        $state = GameState::create(1, $home, $away, [], TeamSide::HOME);

        $this->assertSame(1, $state->getMatchId());
        $this->assertSame(1, $state->getHalf());
        $this->assertSame(GamePhase::SETUP, $state->getPhase());
        $this->assertSame(TeamSide::HOME, $state->getActiveTeam());
    }

    public function testWitherMethods(): void
    {
        $home = TeamStateDTO::create(1, 'Home', 'Human', TeamSide::HOME, 3);
        $away = TeamStateDTO::create(2, 'Away', 'Orc', TeamSide::AWAY, 2);
        $state = GameState::create(1, $home, $away, [], TeamSide::HOME);

        $newState = $state->withPhase(GamePhase::PLAY);
        $this->assertSame(GamePhase::PLAY, $newState->getPhase());
        $this->assertSame(GamePhase::SETUP, $state->getPhase()); // original unchanged
    }

    public function testGetPlayerAtPosition(): void
    {
        $player = MatchPlayerDTO::create(
            1, 1, 'Test', 1, 'Lineman',
            new PlayerStats(6, 3, 3, 8), [], TeamSide::HOME,
            new Position(5, 5),
        );

        $home = TeamStateDTO::create(1, 'Home', 'Human', TeamSide::HOME, 3);
        $away = TeamStateDTO::create(2, 'Away', 'Orc', TeamSide::AWAY, 2);
        $state = new GameState(1, 1, GamePhase::PLAY, TeamSide::HOME, $home, $away, [1 => $player], BallState::offPitch(), false, null);

        $found = $state->getPlayerAtPosition(new Position(5, 5));
        $this->assertNotNull($found);
        $this->assertSame(1, $found->getId());

        $notFound = $state->getPlayerAtPosition(new Position(10, 10));
        $this->assertNull($notFound);
    }

    public function testGetTeamPlayers(): void
    {
        $p1 = MatchPlayerDTO::create(1, 1, 'P1', 1, 'Lineman', new PlayerStats(6, 3, 3, 8), [], TeamSide::HOME, new Position(5, 5));
        $p2 = MatchPlayerDTO::create(2, 2, 'P2', 2, 'Lineman', new PlayerStats(6, 3, 3, 8), [], TeamSide::HOME, new Position(6, 5));
        $p3 = MatchPlayerDTO::create(3, 3, 'P3', 3, 'Lineman', new PlayerStats(5, 3, 3, 9), [], TeamSide::AWAY, new Position(15, 5));

        $home = TeamStateDTO::create(1, 'Home', 'Human', TeamSide::HOME, 3);
        $away = TeamStateDTO::create(2, 'Away', 'Orc', TeamSide::AWAY, 2);
        $state = new GameState(1, 1, GamePhase::PLAY, TeamSide::HOME, $home, $away, [1 => $p1, 2 => $p2, 3 => $p3], BallState::offPitch(), false, null);

        $this->assertCount(2, $state->getTeamPlayers(TeamSide::HOME));
        $this->assertCount(1, $state->getTeamPlayers(TeamSide::AWAY));
    }

    public function testResetPlayersForNewTurn(): void
    {
        $player = MatchPlayerDTO::create(1, 1, 'Test', 1, 'Lineman', new PlayerStats(6, 3, 3, 8), [], TeamSide::HOME, new Position(5, 5));
        $movedPlayer = $player->withHasMoved(true)->withHasActed(true)->withMovementRemaining(0);

        $home = TeamStateDTO::create(1, 'Home', 'Human', TeamSide::HOME, 3);
        $away = TeamStateDTO::create(2, 'Away', 'Orc', TeamSide::AWAY, 2);
        $state = new GameState(1, 1, GamePhase::PLAY, TeamSide::HOME, $home, $away, [1 => $movedPlayer], BallState::offPitch(), false, null);

        $resetState = $state->resetPlayersForNewTurn(TeamSide::HOME);
        $resetPlayer = $resetState->getPlayer(1);

        $this->assertNotNull($resetPlayer);
        $this->assertFalse($resetPlayer->hasMoved());
        $this->assertFalse($resetPlayer->hasActed());
        $this->assertSame(6, $resetPlayer->getMovementRemaining());
    }

    public function testStunnedPlayerRecoveryOnNewTurn(): void
    {
        $player = MatchPlayerDTO::create(1, 1, 'Test', 1, 'Lineman', new PlayerStats(6, 3, 3, 8), [], TeamSide::HOME, new Position(5, 5));
        $stunnedPlayer = $player->withState(PlayerState::STUNNED);

        $home = TeamStateDTO::create(1, 'Home', 'Human', TeamSide::HOME, 3);
        $away = TeamStateDTO::create(2, 'Away', 'Orc', TeamSide::AWAY, 2);
        $state = new GameState(1, 1, GamePhase::PLAY, TeamSide::HOME, $home, $away, [1 => $stunnedPlayer], BallState::offPitch(), false, null);

        $resetState = $state->resetPlayersForNewTurn(TeamSide::HOME);
        $resetPlayer = $resetState->getPlayer(1);

        $this->assertNotNull($resetPlayer);
        $this->assertSame(PlayerState::PRONE, $resetPlayer->getState());
    }

    public function testSerializationRoundTrip(): void
    {
        $player = MatchPlayerDTO::create(1, 1, 'Test', 1, 'Lineman', new PlayerStats(6, 3, 3, 8), [SkillName::Block], TeamSide::HOME, new Position(5, 5));
        $home = TeamStateDTO::create(1, 'Home', 'Human', TeamSide::HOME, 3);
        $away = TeamStateDTO::create(2, 'Away', 'Orc', TeamSide::AWAY, 2);
        $ball = BallState::carried(new Position(5, 5), 1);
        $state = new GameState(1, 1, GamePhase::PLAY, TeamSide::HOME, $home, $away, [1 => $player], $ball, false, TeamSide::AWAY);

        $array = $state->toArray();
        $json = json_encode($array);
        $this->assertNotFalse($json);

        /** @var array<string, mixed> */
        $decoded = json_decode($json, true);
        $restored = GameState::fromArray($decoded);

        $this->assertSame($state->getMatchId(), $restored->getMatchId());
        $this->assertSame($state->getPhase(), $restored->getPhase());
        $this->assertSame($state->getActiveTeam(), $restored->getActiveTeam());
        $this->assertSame(1, $restored->getPlayer(1)?->getId());
        $this->assertTrue($restored->getBall()->isHeld());
        $this->assertSame(1, $restored->getBall()->getCarrierId());
    }
}
