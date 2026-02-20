<?php
declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\Pathfinder;
use App\Enum\TeamSide;
use App\ValueObject\Position;
use PHPUnit\Framework\TestCase;

final class PathfinderTest extends TestCase
{
    private Pathfinder $pathfinder;

    protected function setUp(): void
    {
        $this->pathfinder = new Pathfinder();
    }

    public function testPlayerWithNoMovementCantMove(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 6, id: 1)
            ->build();

        // Set player as already moved
        $player = $state->getPlayer(1);
        $this->assertNotNull($player);
        $player = $player->withHasMoved(true);
        $state = $state->withPlayer($player);

        $moves = $this->pathfinder->findValidMoves($state, $player);
        $this->assertEmpty($moves);
    }

    public function testPlayerCanMoveToAdjacentSquares(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 6, id: 1)
            ->build();

        $player = $state->getPlayer(1);
        $this->assertNotNull($player);
        $moves = $this->pathfinder->findValidMoves($state, $player);

        // Should include all 8 adjacent squares (among many others)
        $this->assertArrayHasKey('4,4', $moves);
        $this->assertArrayHasKey('5,4', $moves);
        $this->assertArrayHasKey('6,6', $moves);
        $this->assertArrayHasKey('4,5', $moves);
        $this->assertArrayHasKey('6,5', $moves);
        $this->assertArrayHasKey('4,6', $moves);
        $this->assertArrayHasKey('5,6', $moves);
        $this->assertArrayHasKey('6,4', $moves);
    }

    public function testPlayerCantMoveToOccupiedSquare(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 6, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5) // occupied
            ->build();

        $player = $state->getPlayer(1);
        $this->assertNotNull($player);
        $moves = $this->pathfinder->findValidMoves($state, $player);

        $this->assertArrayNotHasKey('6,5', $moves);
    }

    public function testPlayerCantMoveToFriendlyOccupiedSquare(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 6, id: 1)
            ->addPlayer(TeamSide::HOME, 6, 5) // friendly occupied
            ->build();

        $player = $state->getPlayer(1);
        $this->assertNotNull($player);
        $moves = $this->pathfinder->findValidMoves($state, $player);

        $this->assertArrayNotHasKey('6,5', $moves);
    }

    public function testMovementRangeRespectsMA(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 2, id: 1)
            ->build();

        $player = $state->getPlayer(1);
        $this->assertNotNull($player);
        $moves = $this->pathfinder->findValidMoves($state, $player);

        // Within MA=2, plus 2 GFI squares
        // Can reach distance 4 (2 MA + 2 GFI)
        $farPosition = '9,5'; // distance 4
        $this->assertArrayHasKey($farPosition, $moves);

        $tooFar = '10,5'; // distance 5
        $this->assertArrayNotHasKey($tooFar, $moves);
    }

    public function testGFISquaresAreMarked(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 2, id: 1)
            ->build();

        $player = $state->getPlayer(1);
        $this->assertNotNull($player);
        $moves = $this->pathfinder->findValidMoves($state, $player);

        // Distance 2 (within MA) - no GFI
        $normalMove = $moves['7,5'] ?? null;
        $this->assertNotNull($normalMove);
        $this->assertSame(0, $normalMove->getGfiCount());

        // Distance 3 (MA+1) - 1 GFI
        $gfiMove = $moves['8,5'] ?? null;
        $this->assertNotNull($gfiMove);
        $this->assertSame(1, $gfiMove->getGfiCount());

        // Distance 4 (MA+2) - 2 GFI
        $gfi2Move = $moves['9,5'] ?? null;
        $this->assertNotNull($gfi2Move);
        $this->assertSame(2, $gfi2Move->getGfiCount());
    }

    public function testDodgeRequiredWhenLeavingTacklezone(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 6, id: 1)
            ->addPlayer(TeamSide::AWAY, 5, 4) // adjacent enemy
            ->build();

        $player = $state->getPlayer(1);
        $this->assertNotNull($player);
        $moves = $this->pathfinder->findValidMoves($state, $player);

        // Moving away from the enemy should require a dodge
        $moveAway = $moves['5,6'] ?? null;
        $this->assertNotNull($moveAway);
        $this->assertSame(1, $moveAway->getDodgeCount());
    }

    public function testNoDodgeWhenNoEnemyTacklezones(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 6, id: 1)
            ->build();

        $player = $state->getPlayer(1);
        $this->assertNotNull($player);
        $moves = $this->pathfinder->findValidMoves($state, $player);

        foreach ($moves as $path) {
            $this->assertSame(0, $path->getDodgeCount());
        }
    }

    public function testFindPathToSpecificDestination(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 6, id: 1)
            ->build();

        $player = $state->getPlayer(1);
        $this->assertNotNull($player);

        $path = $this->pathfinder->findPathTo($state, $player, new Position(7, 5));
        $this->assertNotNull($path);
        $this->assertSame(7, $path->getDestination()->getX());
        $this->assertSame(5, $path->getDestination()->getY());
    }

    public function testFindPathToUnreachableReturnsNull(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 1, id: 1)
            ->build();

        $player = $state->getPlayer(1);
        $this->assertNotNull($player);

        $path = $this->pathfinder->findPathTo($state, $player, new Position(20, 10));
        $this->assertNull($path);
    }

    public function testPlayerAtEdgeHasLimitedMoves(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 0, 0, movement: 6, id: 1)
            ->build();

        $player = $state->getPlayer(1);
        $this->assertNotNull($player);
        $moves = $this->pathfinder->findValidMoves($state, $player);

        // Corner position - (0,0) only has 3 adjacent but player can reach more via GFI
        // Just verify we have valid moves and no positions with negative coordinates
        $this->assertNotEmpty($moves);
        foreach ($moves as $key => $path) {
            $dest = $path->getDestination();
            $this->assertGreaterThanOrEqual(0, $dest->getX());
            $this->assertGreaterThanOrEqual(0, $dest->getY());
        }
    }

    public function testPronePlayersHaveReducedMovement(): void
    {
        // Prone player with MA=6: 6 - 3 (stand-up) = 3 MA + 2 GFI = max 5 squares
        $state = (new GameStateBuilder())
            ->addPronePlayer(TeamSide::HOME, 5, 5, id: 1)
            ->build();

        $player = $state->getPlayer(1);
        $this->assertNotNull($player);
        $moves = $this->pathfinder->findValidMoves($state, $player);

        $this->assertNotEmpty($moves);

        // Max distance should be 5 squares (3 MA + 2 GFI)
        foreach ($moves as $path) {
            $this->assertLessThanOrEqual(5, $path->getTotalCost());
        }
    }

    public function testPronePlayerLowMACantMove(): void
    {
        // Prone player with MA=2 (< 3): pathfinder returns empty
        $state = (new GameStateBuilder())
            ->addPronePlayer(TeamSide::HOME, 5, 5, movement: 2, id: 1)
            ->build();

        $player = $state->getPlayer(1);
        $this->assertNotNull($player);
        $moves = $this->pathfinder->findValidMoves($state, $player);

        $this->assertEmpty($moves);
    }
}
