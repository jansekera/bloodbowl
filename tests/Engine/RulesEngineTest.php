<?php
declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\RulesEngine;
use App\Enum\ActionType;
use App\Enum\GamePhase;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

final class RulesEngineTest extends TestCase
{
    private RulesEngine $rules;

    protected function setUp(): void
    {
        $this->rules = new RulesEngine();
    }

    public function testValidateMoveDuringPlayPhase(): void
    {
        $state = (new GameStateBuilder())
            ->withPhase(GamePhase::PLAY)
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 6, id: 1)
            ->build();

        $errors = $this->rules->validate($state, ActionType::MOVE, [
            'playerId' => 1,
            'x' => 6,
            'y' => 5,
        ]);

        $this->assertEmpty($errors);
    }

    public function testValidateMoveRejectsInSetupPhase(): void
    {
        $state = (new GameStateBuilder())
            ->withPhase(GamePhase::SETUP)
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->build();

        $errors = $this->rules->validate($state, ActionType::MOVE, [
            'playerId' => 1,
            'x' => 6,
            'y' => 5,
        ]);

        $this->assertNotEmpty($errors);
    }

    public function testValidateMoveRejectsWrongTeam(): void
    {
        $state = (new GameStateBuilder())
            ->withActiveTeam(TeamSide::HOME)
            ->addPlayer(TeamSide::AWAY, 15, 5, id: 1)
            ->build();

        $errors = $this->rules->validate($state, ActionType::MOVE, [
            'playerId' => 1,
            'x' => 16,
            'y' => 5,
        ]);

        $this->assertNotEmpty($errors);
        $this->assertStringContainsString('active team', $errors[0]);
    }

    public function testValidateMoveRejectsAlreadyMoved(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->build();

        $player = $state->getPlayer(1);
        $this->assertNotNull($player);
        $player = $player->withHasMoved(true);
        $state = $state->withPlayer($player);

        $errors = $this->rules->validate($state, ActionType::MOVE, [
            'playerId' => 1,
            'x' => 6,
            'y' => 5,
        ]);

        $this->assertNotEmpty($errors);
    }

    public function testValidateMoveRequiresPlayerId(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->build();

        $errors = $this->rules->validate($state, ActionType::MOVE, []);
        $this->assertNotEmpty($errors);
        $this->assertStringContainsString('playerId', $errors[0]);
    }

    public function testValidateMoveRejectsInvalidPath(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 1, id: 1)
            ->build();

        $errors = $this->rules->validate($state, ActionType::MOVE, [
            'playerId' => 1,
            'x' => 20,
            'y' => 10,
        ]);

        $this->assertNotEmpty($errors);
    }

    public function testValidateMoveRejectsOffPitch(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->build();

        $errors = $this->rules->validate($state, ActionType::MOVE, [
            'playerId' => 1,
            'x' => -1,
            'y' => 5,
        ]);

        $this->assertNotEmpty($errors);
    }

    public function testEndTurnAlwaysValid(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->build();

        $errors = $this->rules->validate($state, ActionType::END_TURN, []);
        $this->assertEmpty($errors);
    }

    public function testGetAvailableActionsInPlayPhase(): void
    {
        $state = (new GameStateBuilder())
            ->withPhase(GamePhase::PLAY)
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 6, id: 1)
            ->build();

        $actions = $this->rules->getAvailableActions($state);

        $types = array_column($actions, 'type');
        $this->assertContains('move', $types);
        $this->assertContains('end_turn', $types);
    }

    public function testGetAvailableActionsInSetupPhase(): void
    {
        $state = (new GameStateBuilder())
            ->withPhase(GamePhase::SETUP)
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->build();

        $actions = $this->rules->getAvailableActions($state);

        $types = array_column($actions, 'type');
        $this->assertContains('setup_player', $types);
        $this->assertContains('end_setup', $types);
    }

    public function testGetValidMoveTargets(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, movement: 2, id: 1)
            ->build();

        $targets = $this->rules->getValidMoveTargets($state, 1);
        $this->assertNotEmpty($targets);

        // Check structure
        $first = $targets[0];
        $this->assertArrayHasKey('x', $first);
        $this->assertArrayHasKey('y', $first);
        $this->assertArrayHasKey('dodges', $first);
        $this->assertArrayHasKey('gfis', $first);
    }

    public function testGetValidMoveTargetsForMovedPlayer(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->build();

        $player = $state->getPlayer(1);
        $this->assertNotNull($player);
        $player = $player->withHasMoved(true);
        $state = $state->withPlayer($player);

        $targets = $this->rules->getValidMoveTargets($state, 1);
        $this->assertEmpty($targets);
    }

    public function testValidateSetupPlayerInSetupPhase(): void
    {
        $state = (new GameStateBuilder())
            ->withPhase(GamePhase::SETUP)
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->build();

        $errors = $this->rules->validate($state, ActionType::SETUP_PLAYER, [
            'playerId' => 1,
            'x' => 10,
            'y' => 7,
        ]);

        $this->assertEmpty($errors);
    }

    public function testValidateSetupRejectsWrongHalf(): void
    {
        $state = (new GameStateBuilder())
            ->withPhase(GamePhase::SETUP)
            ->withActiveTeam(TeamSide::HOME)
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->build();

        $errors = $this->rules->validate($state, ActionType::SETUP_PLAYER, [
            'playerId' => 1,
            'x' => 15, // away side
            'y' => 7,
        ]);

        $this->assertNotEmpty($errors);
    }

    // === Block Validation ===

    public function testValidateBlockAdjacentEnemy(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, id: 2)
            ->build();

        $errors = $this->rules->validate($state, ActionType::BLOCK, [
            'playerId' => 1,
            'targetId' => 2,
        ]);

        $this->assertEmpty($errors);
    }

    public function testValidateBlockRejectsNonAdjacent(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 8, 5, id: 2)
            ->build();

        $errors = $this->rules->validate($state, ActionType::BLOCK, [
            'playerId' => 1,
            'targetId' => 2,
        ]);

        $this->assertNotEmpty($errors);
    }

    public function testValidateBlockRejectsFriendlyTarget(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->addPlayer(TeamSide::HOME, 6, 5, id: 2)
            ->build();

        $errors = $this->rules->validate($state, ActionType::BLOCK, [
            'playerId' => 1,
            'targetId' => 2,
        ]);

        $this->assertNotEmpty($errors);
    }

    public function testValidateBlockRejectsAlreadyActed(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, id: 2)
            ->build();

        $player = $state->getPlayer(1);
        $this->assertNotNull($player);
        $state = $state->withPlayer($player->withHasActed(true));

        $errors = $this->rules->validate($state, ActionType::BLOCK, [
            'playerId' => 1,
            'targetId' => 2,
        ]);

        $this->assertNotEmpty($errors);
    }

    // === Blitz Validation ===

    public function testValidateBlitzValid(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 8, 5, id: 2)
            ->build();

        $errors = $this->rules->validate($state, ActionType::BLITZ, [
            'playerId' => 1,
            'targetId' => 2,
        ]);

        $this->assertEmpty($errors);
    }

    public function testValidateBlitzRejectsDoubleBlitz(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 8, 5, id: 2)
            ->build();

        $teamState = $state->getTeamState(TeamSide::HOME)->withBlitzUsed();
        $state = $state->withTeamState(TeamSide::HOME, $teamState);

        $errors = $this->rules->validate($state, ActionType::BLITZ, [
            'playerId' => 1,
            'targetId' => 2,
        ]);

        $this->assertNotEmpty($errors);
        $this->assertStringContainsString('already used', $errors[0]);
    }

    // === Available Actions with Block/Blitz ===

    public function testGetAvailableActionsIncludesBlock(): void
    {
        $state = (new GameStateBuilder())
            ->withPhase(GamePhase::PLAY)
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, id: 2)
            ->build();

        $actions = $this->rules->getAvailableActions($state);
        $types = array_column($actions, 'type');

        $this->assertContains('block', $types);
        $this->assertContains('blitz', $types);
    }

    public function testGetBlockTargets(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, id: 2) // adjacent
            ->addPlayer(TeamSide::AWAY, 8, 5, id: 3) // not adjacent
            ->build();

        $player = $state->getPlayer(1);
        $this->assertNotNull($player);

        $targets = $this->rules->getBlockTargets($state, $player);

        $this->assertCount(1, $targets);
        $this->assertSame(2, $targets[0]->getId());
    }
}
