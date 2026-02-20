<?php
declare(strict_types=1);

namespace App\Engine;

use App\DTO\GameState;
use App\DTO\MatchPlayerDTO;
use App\Enum\ActionType;
use App\Enum\GamePhase;
use App\Enum\PassRange;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use App\ValueObject\Position;

final class RulesEngine
{
    private readonly TacklezoneCalculator $tzCalc;
    private readonly Pathfinder $pathfinder;

    public function __construct(
        ?TacklezoneCalculator $tzCalc = null,
        ?Pathfinder $pathfinder = null,
    ) {
        $this->tzCalc = $tzCalc ?? new TacklezoneCalculator();
        $this->pathfinder = $pathfinder ?? new Pathfinder($this->tzCalc);
    }

    /**
     * Validate whether an action is legal in the current game state.
     *
     * @param array<string, mixed> $params
     * @return list<string> List of validation errors (empty = valid)
     */
    public function validate(GameState $state, ActionType $action, array $params): array
    {
        $errors = [];

        // Check game phase
        if ($action === ActionType::SETUP_PLAYER || $action === ActionType::END_SETUP) {
            if (!$state->getPhase()->isSetup()) {
                $errors[] = 'Can only set up players during setup phase';
            }
        } elseif ($action !== ActionType::END_TURN) {
            if (!$state->getPhase()->isPlayable()) {
                $errors[] = 'Game is not in a playable phase';
            }
        }

        if ($errors !== []) {
            return $errors;
        }

        return match ($action) {
            ActionType::MOVE => $this->validateMove($state, $params),
            ActionType::BLOCK => $this->validateBlock($state, $params),
            ActionType::BLITZ => $this->validateBlitz($state, $params),
            ActionType::PASS => $this->validatePass($state, $params),
            ActionType::HAND_OFF => $this->validateHandOff($state, $params),
            ActionType::FOUL => $this->validateFoul($state, $params),
            ActionType::THROW_TEAM_MATE => $this->validateThrowTeamMate($state, $params),
            ActionType::BOMB_THROW => $this->validateBombThrow($state, $params),
            ActionType::HYPNOTIC_GAZE => $this->validateHypnoticGaze($state, $params),
            ActionType::BALL_AND_CHAIN => $this->validateBallAndChain($state, $params),
            ActionType::MULTIPLE_BLOCK => $this->validateMultipleBlock($state, $params),
            ActionType::END_TURN => [],
            ActionType::SETUP_PLAYER => $this->validateSetupPlayer($state, $params),
            ActionType::END_SETUP => $this->validateEndSetup($state),
        };
    }

    /**
     * Get available actions for the active team.
     *
     * @return list<array{type: string, playerId?: int}>
     */
    public function getAvailableActions(GameState $state): array
    {
        $actions = [];
        $side = $state->getActiveTeam();

        if ($state->getPhase()->isPlayable()) {
            $teamState = $state->getTeamState($side);

            foreach ($state->getTeamPlayers($side) as $player) {
                // Ball & Chain players can ONLY use BALL_AND_CHAIN action
                if ($player->hasSkill(SkillName::BallAndChain)) {
                    if ($player->canAct()) {
                        $actions[] = ['type' => ActionType::BALL_AND_CHAIN->value, 'playerId' => $player->getId()];
                    }
                    continue;
                }

                if ($player->canMove()) {
                    // Quick check: can the player move at all? (any adjacent empty square)
                    if ($this->canPlayerMoveAnywhere($state, $player)) {
                        $actions[] = ['type' => ActionType::MOVE->value, 'playerId' => $player->getId()];
                    }
                }

                // Block: player can act, adjacent enemy standing
                // Jump Up: prone player with JumpUp can also block
                $canBlock = $player->canAct()
                    || ($player->getState() === PlayerState::PRONE
                        && !$player->hasActed()
                        && $player->hasSkill(SkillName::JumpUp));
                if ($canBlock) {
                    $targets = $this->getBlockTargets($state, $player);
                    if ($targets !== []) {
                        $actions[] = ['type' => ActionType::BLOCK->value, 'playerId' => $player->getId()];
                    }
                    // Multiple Block: player with skill and 2+ adjacent enemies
                    if ($player->hasSkill(SkillName::MultipleBlock) && count($targets) >= 2) {
                        $actions[] = ['type' => ActionType::MULTIPLE_BLOCK->value, 'playerId' => $player->getId()];
                    }
                }

                // Hypnotic Gaze: once per turn, player with HypnoticGaze, adjacent opponent
                if ($player->canAct() && $player->hasSkill(SkillName::HypnoticGaze)) {
                    $gazeTargets = $this->getHypnoticGazeTargets($state, $player);
                    if ($gazeTargets !== []) {
                        $actions[] = ['type' => ActionType::HYPNOTIC_GAZE->value, 'playerId' => $player->getId()];
                    }
                }
            }

            // Blitz: once per turn, any unmoved player (excluding B&C)
            if (!$teamState->isBlitzUsedThisTurn()) {
                foreach ($state->getTeamPlayers($side) as $player) {
                    if ($player->canAct() && !$player->hasSkill(SkillName::BallAndChain)) {
                        $actions[] = ['type' => ActionType::BLITZ->value, 'playerId' => $player->getId()];
                    }
                }
            }

            // Pass: once per turn, ball carrier (excluding B&C)
            if (!$teamState->isPassUsedThisTurn()) {
                $ball = $state->getBall();
                if ($ball->isHeld() && $ball->getCarrierId() !== null) {
                    $carrier = $state->getPlayer($ball->getCarrierId());
                    if ($carrier !== null && $carrier->getTeamSide() === $side && $carrier->canAct()
                        && !$carrier->hasSkill(SkillName::BallAndChain)) {
                        $actions[] = ['type' => ActionType::PASS->value, 'playerId' => $carrier->getId()];
                    }
                }

                // Bomb throw: player with Bombardier (shares pass slot)
                foreach ($state->getTeamPlayers($side) as $player) {
                    if ($player->canAct() && $player->hasSkill(SkillName::Bombardier)
                        && !$player->hasSkill(SkillName::BallAndChain)) {
                        $actions[] = ['type' => ActionType::BOMB_THROW->value, 'playerId' => $player->getId()];
                    }
                }
            }

            // Hand-off: ball carrier to adjacent teammate (excluding B&C)
            $ball = $state->getBall();
            if ($ball->isHeld() && $ball->getCarrierId() !== null) {
                $carrier = $state->getPlayer($ball->getCarrierId());
                if ($carrier !== null && $carrier->getTeamSide() === $side && $carrier->canAct()
                    && !$carrier->hasSkill(SkillName::BallAndChain)) {
                    $handOffTargets = $this->getHandOffTargets($state, $carrier);
                    if ($handOffTargets !== []) {
                        $actions[] = ['type' => ActionType::HAND_OFF->value, 'playerId' => $carrier->getId()];
                    }
                }
            }

            // Throw Team-Mate: player with TTM skill adjacent to RightStuff teammate
            if (!$teamState->isPassUsedThisTurn()) {
                foreach ($state->getTeamPlayers($side) as $player) {
                    if (!$player->canAct() || !$player->hasSkill(SkillName::ThrowTeamMate)) {
                        continue;
                    }
                    $ttmTargets = $this->getThrowTeamMateTargets($state, $player);
                    if ($ttmTargets !== []) {
                        $actions[] = ['type' => ActionType::THROW_TEAM_MATE->value, 'playerId' => $player->getId()];
                    }
                }
            }

            // Foul: once per turn, standing player with adjacent prone/stunned enemy (excluding B&C)
            if (!$teamState->isFoulUsedThisTurn()) {
                foreach ($state->getTeamPlayers($side) as $player) {
                    if ($player->canAct() && !$player->hasSkill(SkillName::BallAndChain)) {
                        $foulTargets = $this->getFoulTargets($state, $player);
                        if ($foulTargets !== []) {
                            $actions[] = ['type' => ActionType::FOUL->value, 'playerId' => $player->getId()];
                        }
                    }
                }
            }

            $actions[] = ['type' => ActionType::END_TURN->value];
        }

        if ($state->getPhase()->isSetup()) {
            foreach ($state->getTeamPlayers($side) as $player) {
                $actions[] = ['type' => ActionType::SETUP_PLAYER->value, 'playerId' => $player->getId()];
            }
            $actions[] = ['type' => ActionType::END_SETUP->value];
        }

        return $actions;
    }

    /**
     * Get valid move targets for a specific player.
     *
     * @return list<array{x: int, y: int, dodges: int, gfis: int}>
     */
    public function getValidMoveTargets(GameState $state, int $playerId): array
    {
        $player = $state->getPlayer($playerId);
        if ($player === null || !$player->canMove()) {
            return [];
        }

        $moves = $this->pathfinder->findValidMoves($state, $player);
        $targets = [];

        // PRONE players can always stand in place
        if ($player->getState() === PlayerState::PRONE && $player->getPosition() !== null) {
            $pos = $player->getPosition();
            $targets[] = [
                'x' => $pos->getX(),
                'y' => $pos->getY(),
                'dodges' => 0,
                'gfis' => 0,
            ];
        }

        foreach ($moves as $path) {
            $dest = $path->getDestination();
            $targets[] = [
                'x' => $dest->getX(),
                'y' => $dest->getY(),
                'dodges' => $path->getDodgeCount(),
                'gfis' => $path->getGfiCount(),
            ];
        }

        return $targets;
    }

    /**
     * @param array<string, mixed> $params
     * @return list<string>
     */
    private function validateMove(GameState $state, array $params): array
    {
        $errors = [];

        if (!isset($params['playerId'])) {
            $errors[] = 'playerId is required';
            return $errors;
        }

        $playerId = (int) $params['playerId'];
        $player = $state->getPlayer($playerId);

        if ($player === null) {
            $errors[] = "Player {$playerId} not found";
            return $errors;
        }

        if ($player->getTeamSide() !== $state->getActiveTeam()) {
            $errors[] = 'Can only move players from the active team';
            return $errors;
        }

        if (!$player->canMove()) {
            $errors[] = 'Player cannot move (already moved or not standing)';
            return $errors;
        }

        if (!isset($params['x']) || !isset($params['y'])) {
            $errors[] = 'Target position (x, y) is required';
            return $errors;
        }

        $destination = new Position((int) $params['x'], (int) $params['y']);
        if (!$destination->isOnPitch()) {
            $errors[] = 'Target position is off the pitch';
            return $errors;
        }

        // PRONE player standing in place: no pathfinding needed
        $isProne = $player->getState() === PlayerState::PRONE;
        $isStandInPlace = $isProne && $player->getPosition() !== null && $player->getPosition()->equals($destination);

        if (!$isStandInPlace) {
            $path = $this->pathfinder->findPathTo($state, $player, $destination);
            if ($path === null) {
                $errors[] = 'No valid path to target position';
            }
        }

        return $errors;
    }

    /**
     * @param array<string, mixed> $params
     * @return list<string>
     */
    private function validateSetupPlayer(GameState $state, array $params): array
    {
        $errors = [];

        if (!isset($params['playerId'])) {
            $errors[] = 'playerId is required';
            return $errors;
        }

        $playerId = (int) $params['playerId'];
        $player = $state->getPlayer($playerId);

        if ($player === null) {
            $errors[] = "Player {$playerId} not found";
            return $errors;
        }

        if ($player->getTeamSide() !== $state->getActiveTeam()) {
            $errors[] = 'Can only set up players from the active team';
            return $errors;
        }

        if (!isset($params['x']) || !isset($params['y'])) {
            $errors[] = 'Position (x, y) is required';
            return $errors;
        }

        $x = (int) $params['x'];
        $y = (int) $params['y'];
        $position = new Position($x, $y);

        if (!$position->isOnPitch()) {
            $errors[] = 'Position is off the pitch';
            return $errors;
        }

        $side = $player->getTeamSide();
        if ($side === TeamSide::HOME && $x > 12) {
            $errors[] = 'Home team must set up on left half (x <= 12)';
        }
        if ($side === TeamSide::AWAY && $x < 13) {
            $errors[] = 'Away team must set up on right half (x >= 13)';
        }

        $occupant = $state->getPlayerAtPosition($position);
        if ($occupant !== null && $occupant->getId() !== $playerId) {
            $errors[] = 'Position already occupied by another player';
        }

        return $errors;
    }

    /**
     * @param array<string, mixed> $params
     * @return list<string>
     */
    private function validateBlock(GameState $state, array $params): array
    {
        $errors = [];

        if (!isset($params['playerId'])) {
            return ['playerId is required'];
        }
        if (!isset($params['targetId'])) {
            return ['targetId is required'];
        }

        $playerId = (int) $params['playerId'];
        $targetId = (int) $params['targetId'];

        $player = $state->getPlayer($playerId);
        if ($player === null) {
            return ["Player {$playerId} not found"];
        }

        if ($player->getTeamSide() !== $state->getActiveTeam()) {
            return ['Can only block with players from the active team'];
        }

        $canBlock = $player->canAct()
            || ($player->getState() === PlayerState::PRONE
                && !$player->hasActed()
                && $player->hasSkill(SkillName::JumpUp));
        if (!$canBlock) {
            return ['Player cannot act (already acted or not standing)'];
        }

        $target = $state->getPlayer($targetId);
        if ($target === null) {
            return ["Target player {$targetId} not found"];
        }

        if ($target->getTeamSide() === $player->getTeamSide()) {
            return ['Cannot block a friendly player'];
        }

        if (!$target->getState()->isOnPitch()) {
            return ['Target is not on the pitch'];
        }

        $playerPos = $player->getPosition();
        $targetPos = $target->getPosition();

        if ($playerPos === null || $targetPos === null) {
            return ['Players must be on the pitch'];
        }

        if ($playerPos->distanceTo($targetPos) !== 1) {
            $errors[] = 'Target must be adjacent to block';
        }

        return $errors;
    }

    /**
     * @param array<string, mixed> $params
     * @return list<string>
     */
    private function validateBlitz(GameState $state, array $params): array
    {
        $errors = [];

        if (!isset($params['playerId'])) {
            return ['playerId is required'];
        }
        if (!isset($params['targetId'])) {
            return ['targetId is required'];
        }

        $playerId = (int) $params['playerId'];
        $targetId = (int) $params['targetId'];

        $player = $state->getPlayer($playerId);
        if ($player === null) {
            return ["Player {$playerId} not found"];
        }

        if ($player->getTeamSide() !== $state->getActiveTeam()) {
            return ['Can only blitz with players from the active team'];
        }

        if (!$player->canAct()) {
            return ['Player cannot act'];
        }

        $teamState = $state->getTeamState($player->getTeamSide());
        if ($teamState->isBlitzUsedThisTurn()) {
            return ['Blitz already used this turn'];
        }

        $target = $state->getPlayer($targetId);
        if ($target === null) {
            return ["Target player {$targetId} not found"];
        }

        if ($target->getTeamSide() === $player->getTeamSide()) {
            return ['Cannot blitz a friendly player'];
        }

        if (!$target->getState()->isOnPitch()) {
            return ['Target is not on the pitch'];
        }

        return $errors;
    }

    /**
     * Get valid block targets for a player (adjacent standing enemies).
     *
     * @return list<MatchPlayerDTO>
     */
    public function getBlockTargets(GameState $state, MatchPlayerDTO $player): array
    {
        $pos = $player->getPosition();
        $canBlock = $player->canAct()
            || ($player->getState() === PlayerState::PRONE
                && !$player->hasActed()
                && $player->hasSkill(SkillName::JumpUp));
        if ($pos === null || !$canBlock) {
            return [];
        }

        $enemySide = $player->getTeamSide()->opponent();
        $targets = [];

        foreach ($state->getPlayersOnPitch($enemySide) as $enemy) {
            $enemyPos = $enemy->getPosition();
            if ($enemyPos !== null && $pos->distanceTo($enemyPos) === 1 && $enemy->getState()->canAct()) {
                $targets[] = $enemy;
            }
        }

        return $targets;
    }

    /**
     * @param array<string, mixed> $params
     * @return list<string>
     */
    private function validatePass(GameState $state, array $params): array
    {
        if (!isset($params['playerId'])) {
            return ['playerId is required'];
        }
        if (!isset($params['targetX']) || !isset($params['targetY'])) {
            return ['Target position (targetX, targetY) is required'];
        }

        $playerId = (int) $params['playerId'];
        $player = $state->getPlayer($playerId);

        if ($player === null) {
            return ["Player {$playerId} not found"];
        }

        if ($player->getTeamSide() !== $state->getActiveTeam()) {
            return ['Can only pass with players from the active team'];
        }

        if (!$player->canAct()) {
            return ['Player cannot act'];
        }

        // Must be carrying the ball
        $ball = $state->getBall();
        if (!$ball->isHeld() || $ball->getCarrierId() !== $playerId) {
            return ['Player must be carrying the ball to pass'];
        }

        $teamState = $state->getTeamState($player->getTeamSide());
        if ($teamState->isPassUsedThisTurn()) {
            return ['Pass already used this turn'];
        }

        $from = $player->getPosition();
        $target = new Position((int) $params['targetX'], (int) $params['targetY']);

        if ($from === null) {
            return ['Player must be on the pitch'];
        }

        if (!$target->isOnPitch()) {
            return ['Target position is off the pitch'];
        }

        $range = PassRange::fromDistance($from->distanceTo($target));
        if ($range === null) {
            return ['Target is out of pass range'];
        }

        return [];
    }

    /**
     * @param array<string, mixed> $params
     * @return list<string>
     */
    private function validateHandOff(GameState $state, array $params): array
    {
        if (!isset($params['playerId'])) {
            return ['playerId is required'];
        }
        if (!isset($params['targetId'])) {
            return ['targetId is required'];
        }

        $playerId = (int) $params['playerId'];
        $targetId = (int) $params['targetId'];

        $player = $state->getPlayer($playerId);
        if ($player === null) {
            return ["Player {$playerId} not found"];
        }

        if ($player->getTeamSide() !== $state->getActiveTeam()) {
            return ['Can only hand-off with players from the active team'];
        }

        if (!$player->canAct()) {
            return ['Player cannot act'];
        }

        $ball = $state->getBall();
        if (!$ball->isHeld() || $ball->getCarrierId() !== $playerId) {
            return ['Player must be carrying the ball to hand-off'];
        }

        $target = $state->getPlayer($targetId);
        if ($target === null) {
            return ["Target player {$targetId} not found"];
        }

        if ($target->getTeamSide() !== $player->getTeamSide()) {
            return ['Can only hand-off to a teammate'];
        }

        if (!$target->getState()->canAct()) {
            return ['Target must be standing'];
        }

        $playerPos = $player->getPosition();
        $targetPos = $target->getPosition();

        if ($playerPos === null || $targetPos === null) {
            return ['Players must be on the pitch'];
        }

        if ($playerPos->distanceTo($targetPos) !== 1) {
            return ['Target must be adjacent for hand-off'];
        }

        return [];
    }

    /**
     * Get valid pass targets for a player (all squares in range).
     *
     * @return list<array{x: int, y: int, range: string}>
     */
    public function getPassTargets(GameState $state, MatchPlayerDTO $player): array
    {
        $pos = $player->getPosition();
        if ($pos === null) {
            return [];
        }

        $targets = [];
        for ($x = 0; $x < Position::PITCH_WIDTH; $x++) {
            for ($y = 0; $y < Position::PITCH_HEIGHT; $y++) {
                $targetPos = new Position($x, $y);
                $range = PassRange::fromDistance($pos->distanceTo($targetPos));
                if ($range !== null && !$pos->equals($targetPos)) {
                    $targets[] = [
                        'x' => $x,
                        'y' => $y,
                        'range' => $range->value,
                    ];
                }
            }
        }

        return $targets;
    }

    /**
     * Get valid hand-off targets (adjacent standing teammates).
     *
     * @return list<MatchPlayerDTO>
     */
    public function getHandOffTargets(GameState $state, MatchPlayerDTO $player): array
    {
        $pos = $player->getPosition();
        if ($pos === null) {
            return [];
        }

        $targets = [];
        foreach ($state->getTeamPlayers($player->getTeamSide()) as $teammate) {
            if ($teammate->getId() === $player->getId()) {
                continue;
            }
            $teammatePos = $teammate->getPosition();
            if ($teammatePos !== null && $pos->distanceTo($teammatePos) === 1 && $teammate->getState()->canAct()) {
                $targets[] = $teammate;
            }
        }

        return $targets;
    }

    /**
     * @param array<string, mixed> $params
     * @return list<string>
     */
    private function validateThrowTeamMate(GameState $state, array $params): array
    {
        if (!isset($params['playerId'])) {
            return ['playerId is required'];
        }
        if (!isset($params['targetId'])) {
            return ['targetId is required'];
        }
        if (!isset($params['targetX']) || !isset($params['targetY'])) {
            return ['Target position (targetX, targetY) is required'];
        }

        $playerId = (int) $params['playerId'];
        $targetId = (int) $params['targetId'];

        $player = $state->getPlayer($playerId);
        if ($player === null) {
            return ["Player {$playerId} not found"];
        }
        if ($player->getTeamSide() !== $state->getActiveTeam()) {
            return ['Can only throw with players from the active team'];
        }
        if (!$player->canAct()) {
            return ['Player cannot act'];
        }
        if (!$player->hasSkill(SkillName::ThrowTeamMate)) {
            return ['Player must have Throw Team-Mate skill'];
        }

        $target = $state->getPlayer($targetId);
        if ($target === null) {
            return ["Target player {$targetId} not found"];
        }
        if ($target->getTeamSide() !== $player->getTeamSide()) {
            return ['Can only throw a teammate'];
        }
        if (!$target->hasSkill(SkillName::RightStuff)) {
            return ['Target must have Right Stuff skill'];
        }
        if ($target->getState() !== PlayerState::STANDING) {
            return ['Target must be standing'];
        }

        $playerPos = $player->getPosition();
        $targetPos = $target->getPosition();
        if ($playerPos === null || $targetPos === null) {
            return ['Players must be on the pitch'];
        }
        if ($playerPos->distanceTo($targetPos) !== 1) {
            return ['Target must be adjacent'];
        }

        $teamState = $state->getTeamState($player->getTeamSide());
        if ($teamState->isPassUsedThisTurn()) {
            return ['Pass/TTM already used this turn'];
        }

        $landing = new Position((int) $params['targetX'], (int) $params['targetY']);
        $range = PassRange::fromDistance($playerPos->distanceTo($landing));
        if ($range === null) {
            return ['Target landing position is out of range'];
        }

        return [];
    }

    /**
     * Get valid TTM targets (adjacent teammates with Right Stuff).
     *
     * @return list<MatchPlayerDTO>
     */
    public function getThrowTeamMateTargets(GameState $state, MatchPlayerDTO $player): array
    {
        $pos = $player->getPosition();
        if ($pos === null || !$player->canAct() || !$player->hasSkill(SkillName::ThrowTeamMate)) {
            return [];
        }

        $targets = [];
        foreach ($state->getTeamPlayers($player->getTeamSide()) as $teammate) {
            if ($teammate->getId() === $player->getId()) {
                continue;
            }
            $teammatePos = $teammate->getPosition();
            if ($teammatePos !== null
                && $pos->distanceTo($teammatePos) === 1
                && $teammate->getState() === PlayerState::STANDING
                && $teammate->hasSkill(SkillName::RightStuff)
            ) {
                $targets[] = $teammate;
            }
        }

        return $targets;
    }

    /**
     * @return list<string>
     */
    private function validateEndSetup(GameState $state): array
    {
        $errors = [];
        $side = $state->getActiveTeam();
        $playersOnPitch = $state->getPlayersOnPitch($side);

        if (count($playersOnPitch) < 11) {
            $errors[] = 'Need at least 11 players on the pitch (have ' . count($playersOnPitch) . ')';
        }

        // Check LOS
        $losX = $side === TeamSide::HOME ? 12 : 13;
        $losCount = 0;
        foreach ($playersOnPitch as $player) {
            $pos = $player->getPosition();
            if ($pos !== null && $pos->getX() === $losX) {
                $losCount++;
            }
        }

        if ($losCount < 3) {
            $errors[] = "Need at least 3 players on Line of Scrimmage (have {$losCount})";
        }

        // Check wide zone limits
        $wideTopCount = 0;
        $wideBottomCount = 0;
        foreach ($playersOnPitch as $player) {
            $pos = $player->getPosition();
            if ($pos === null) {
                continue;
            }
            if ($pos->getY() < 4) {
                $wideTopCount++;
            }
            if ($pos->getY() >= 11) {
                $wideBottomCount++;
            }
        }

        if ($wideTopCount > 2) {
            $errors[] = "Maximum 2 players in each wide zone (have {$wideTopCount} in top)";
        }
        if ($wideBottomCount > 2) {
            $errors[] = "Maximum 2 players in each wide zone (have {$wideBottomCount} in bottom)";
        }

        return $errors;
    }

    /**
     * @param array<string, mixed> $params
     * @return list<string>
     */
    private function validateFoul(GameState $state, array $params): array
    {
        if (!isset($params['playerId'])) {
            return ['playerId is required'];
        }
        if (!isset($params['targetId'])) {
            return ['targetId is required'];
        }

        $playerId = (int) $params['playerId'];
        $targetId = (int) $params['targetId'];

        $player = $state->getPlayer($playerId);
        if ($player === null) {
            return ["Player {$playerId} not found"];
        }

        if ($player->getTeamSide() !== $state->getActiveTeam()) {
            return ['Can only foul with players from the active team'];
        }

        if (!$player->canAct()) {
            return ['Player cannot act'];
        }

        $teamState = $state->getTeamState($player->getTeamSide());
        if ($teamState->isFoulUsedThisTurn()) {
            return ['Foul already used this turn'];
        }

        $target = $state->getPlayer($targetId);
        if ($target === null) {
            return ["Target player {$targetId} not found"];
        }

        if ($target->getTeamSide() === $player->getTeamSide()) {
            return ['Cannot foul a friendly player'];
        }

        $targetState = $target->getState();
        if ($targetState !== PlayerState::PRONE && $targetState !== PlayerState::STUNNED) {
            return ['Can only foul prone or stunned players'];
        }

        $playerPos = $player->getPosition();
        $targetPos = $target->getPosition();

        if ($playerPos === null || $targetPos === null) {
            return ['Players must be on the pitch'];
        }

        if ($playerPos->distanceTo($targetPos) !== 1) {
            return ['Target must be adjacent to foul'];
        }

        return [];
    }

    /**
     * Quick check if player can move to at least one square (avoids full pathfinding).
     */
    private function canPlayerMoveAnywhere(GameState $state, MatchPlayerDTO $player): bool
    {
        $pos = $player->getPosition();
        if ($pos === null) {
            return false;
        }

        // PRONE players can always "move" (stand in place)
        if ($player->getState() === PlayerState::PRONE) {
            return true;
        }

        // Check if any adjacent square is empty and on pitch
        for ($dx = -1; $dx <= 1; $dx++) {
            for ($dy = -1; $dy <= 1; $dy++) {
                if ($dx === 0 && $dy === 0) {
                    continue;
                }
                $nx = $pos->getX() + $dx;
                $ny = $pos->getY() + $dy;
                if ($nx < 0 || $nx >= Position::PITCH_WIDTH || $ny < 0 || $ny >= Position::PITCH_HEIGHT) {
                    continue;
                }
                $neighbor = new Position($nx, $ny);
                if ($state->getPlayerAtPosition($neighbor) === null) {
                    return true;
                }
            }
        }

        return false;
    }

    /**
     * Get valid foul targets (adjacent prone/stunned enemies).
     *
     * @return list<MatchPlayerDTO>
     */
    public function getFoulTargets(GameState $state, MatchPlayerDTO $player): array
    {
        $pos = $player->getPosition();
        if ($pos === null || !$player->canAct()) {
            return [];
        }

        $enemySide = $player->getTeamSide()->opponent();
        $targets = [];

        foreach ($state->getPlayersOnPitch($enemySide) as $enemy) {
            $enemyPos = $enemy->getPosition();
            $enemyState = $enemy->getState();
            if ($enemyPos !== null
                && $pos->distanceTo($enemyPos) === 1
                && ($enemyState === PlayerState::PRONE || $enemyState === PlayerState::STUNNED)
            ) {
                $targets[] = $enemy;
            }
        }

        return $targets;
    }

    /**
     * Get valid Hypnotic Gaze targets (adjacent standing enemies).
     *
     * @return list<MatchPlayerDTO>
     */
    public function getHypnoticGazeTargets(GameState $state, MatchPlayerDTO $player): array
    {
        $pos = $player->getPosition();
        if ($pos === null || !$player->canAct() || !$player->hasSkill(SkillName::HypnoticGaze)) {
            return [];
        }

        $enemySide = $player->getTeamSide()->opponent();
        $targets = [];

        foreach ($state->getPlayersOnPitch($enemySide) as $enemy) {
            $enemyPos = $enemy->getPosition();
            if ($enemyPos !== null && $pos->distanceTo($enemyPos) === 1 && $enemy->getState() === PlayerState::STANDING) {
                $targets[] = $enemy;
            }
        }

        return $targets;
    }

    /**
     * @param array<string, mixed> $params
     * @return list<string>
     */
    private function validateBombThrow(GameState $state, array $params): array
    {
        if (!isset($params['playerId'])) {
            return ['playerId is required'];
        }
        if (!isset($params['targetX']) || !isset($params['targetY'])) {
            return ['Target position (targetX, targetY) is required'];
        }

        $playerId = (int) $params['playerId'];
        $player = $state->getPlayer($playerId);

        if ($player === null) {
            return ["Player {$playerId} not found"];
        }
        if ($player->getTeamSide() !== $state->getActiveTeam()) {
            return ['Can only bomb throw with players from the active team'];
        }
        if (!$player->canAct()) {
            return ['Player cannot act'];
        }
        if (!$player->hasSkill(SkillName::Bombardier)) {
            return ['Player must have Bombardier skill'];
        }

        $teamState = $state->getTeamState($player->getTeamSide());
        if ($teamState->isPassUsedThisTurn()) {
            return ['Pass/bomb already used this turn'];
        }

        $from = $player->getPosition();
        $target = new Position((int) $params['targetX'], (int) $params['targetY']);

        if ($from === null) {
            return ['Player must be on the pitch'];
        }
        if (!$target->isOnPitch()) {
            return ['Target position is off the pitch'];
        }

        $range = PassRange::fromDistance($from->distanceTo($target));
        if ($range === null) {
            return ['Target is out of range'];
        }

        return [];
    }

    /**
     * @param array<string, mixed> $params
     * @return list<string>
     */
    private function validateHypnoticGaze(GameState $state, array $params): array
    {
        if (!isset($params['playerId'])) {
            return ['playerId is required'];
        }
        if (!isset($params['targetId'])) {
            return ['targetId is required'];
        }

        $playerId = (int) $params['playerId'];
        $targetId = (int) $params['targetId'];

        $player = $state->getPlayer($playerId);
        if ($player === null) {
            return ["Player {$playerId} not found"];
        }
        if ($player->getTeamSide() !== $state->getActiveTeam()) {
            return ['Can only use Hypnotic Gaze with players from the active team'];
        }
        if (!$player->canAct()) {
            return ['Player cannot act'];
        }
        if (!$player->hasSkill(SkillName::HypnoticGaze)) {
            return ['Player must have Hypnotic Gaze skill'];
        }

        $target = $state->getPlayer($targetId);
        if ($target === null) {
            return ["Target player {$targetId} not found"];
        }
        if ($target->getTeamSide() === $player->getTeamSide()) {
            return ['Can only gaze at opponents'];
        }
        if ($target->getState() !== PlayerState::STANDING) {
            return ['Target must be standing'];
        }

        $playerPos = $player->getPosition();
        $targetPos = $target->getPosition();

        if ($playerPos === null || $targetPos === null) {
            return ['Players must be on the pitch'];
        }
        if ($playerPos->distanceTo($targetPos) !== 1) {
            return ['Target must be adjacent'];
        }

        return [];
    }

    /**
     * @param array<string, mixed> $params
     * @return list<string>
     */
    private function validateMultipleBlock(GameState $state, array $params): array
    {
        if (!isset($params['playerId'])) {
            return ['playerId is required'];
        }
        if (!isset($params['targetId'])) {
            return ['targetId is required'];
        }
        if (!isset($params['targetId2'])) {
            return ['targetId2 is required'];
        }

        $playerId = (int) $params['playerId'];
        $targetId = (int) $params['targetId'];
        $targetId2 = (int) $params['targetId2'];

        if ($targetId === $targetId2) {
            return ['Cannot block the same player twice'];
        }

        $player = $state->getPlayer($playerId);
        if ($player === null) {
            return ["Player {$playerId} not found"];
        }

        if ($player->getTeamSide() !== $state->getActiveTeam()) {
            return ['Can only block with players from the active team'];
        }

        $canBlock = $player->canAct()
            || ($player->getState() === PlayerState::PRONE
                && !$player->hasActed()
                && $player->hasSkill(SkillName::JumpUp));
        if (!$canBlock) {
            return ['Player cannot act'];
        }

        if (!$player->hasSkill(SkillName::MultipleBlock)) {
            return ['Player must have Multiple Block skill'];
        }

        $playerPos = $player->getPosition();
        if ($playerPos === null) {
            return ['Player must be on the pitch'];
        }

        // Validate both targets
        foreach ([$targetId, $targetId2] as $tid) {
            $target = $state->getPlayer($tid);
            if ($target === null) {
                return ["Target player {$tid} not found"];
            }
            if ($target->getTeamSide() === $player->getTeamSide()) {
                return ['Cannot block a friendly player'];
            }
            if (!$target->getState()->canAct()) {
                return ['Target must be standing'];
            }
            $targetPos = $target->getPosition();
            if ($targetPos === null) {
                return ['Target must be on the pitch'];
            }
            if ($playerPos->distanceTo($targetPos) !== 1) {
                return ['Target must be adjacent to block'];
            }
        }

        return [];
    }

    /**
     * @param array<string, mixed> $params
     * @return list<string>
     */
    private function validateBallAndChain(GameState $state, array $params): array
    {
        if (!isset($params['playerId'])) {
            return ['playerId is required'];
        }

        $playerId = (int) $params['playerId'];
        $player = $state->getPlayer($playerId);

        if ($player === null) {
            return ["Player {$playerId} not found"];
        }
        if ($player->getTeamSide() !== $state->getActiveTeam()) {
            return ['Can only use Ball & Chain with players from the active team'];
        }
        if (!$player->canAct()) {
            return ['Player cannot act'];
        }
        if (!$player->hasSkill(SkillName::BallAndChain)) {
            return ['Player must have Ball & Chain skill'];
        }

        return [];
    }
}
