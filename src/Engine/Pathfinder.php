<?php
declare(strict_types=1);

namespace App\Engine;

use App\DTO\GameState;
use App\DTO\MatchPlayerDTO;
use App\DTO\MovePath;
use App\DTO\MoveStep;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\ValueObject\Position;

final class Pathfinder
{
    private const GFI_SQUARES = 2;
    private const SPRINT_GFI_SQUARES = 3;

    private readonly TacklezoneCalculator $tzCalc;

    public function __construct(?TacklezoneCalculator $tzCalc = null)
    {
        $this->tzCalc = $tzCalc ?? new TacklezoneCalculator();
    }

    /**
     * Find all valid move destinations for a player.
     *
     * @return array<string, MovePath> keyed by "x,y"
     */
    public function findValidMoves(GameState $state, MatchPlayerDTO $player): array
    {
        $pos = $player->getPosition();
        if ($pos === null || !$player->canMove()) {
            return [];
        }

        $ma = $player->getMovementRemaining();

        // PRONE players spend 3 MA to stand up (Jump Up: free)
        if ($player->getState() === PlayerState::PRONE) {
            if ($player->hasSkill(SkillName::JumpUp)) {
                // Free stand-up, no MA cost
            } elseif ($ma < 3) {
                return []; // Must roll to stand; can only stand in place (handled by RulesEngine)
            } else {
                $ma -= 3;
            }
        }

        $gfiSquares = $player->hasSkill(SkillName::Sprint) ? self::SPRINT_GFI_SQUARES : self::GFI_SQUARES;
        $maxRange = $ma + $gfiSquares;
        $isInTz = $this->tzCalc->isInTacklezone($state, $player);

        $hasLeap = $player->hasSkill(SkillName::Leap);

        // BFS
        /** @var array<string, array{cost: int, dodges: int, gfis: int, steps: list<MoveStep>, prev: ?string, hasLeaped: bool}> */
        $visited = [];
        $startKey = $pos->getX() . ',' . $pos->getY();
        $visited[$startKey] = [
            'cost' => 0,
            'dodges' => 0,
            'gfis' => 0,
            'steps' => [],
            'prev' => null,
            'hasLeaped' => false,
        ];

        /** @var list<array{pos: Position, key: string}> */
        $queue = [['pos' => $pos, 'key' => $startKey]];
        $results = [];

        while ($queue !== []) {
            $current = array_shift($queue);
            $currentPos = $current['pos'];
            $currentKey = $current['key'];
            $currentData = $visited[$currentKey];
            $currentCost = $currentData['cost'];

            // Normal adjacent moves
            foreach ($currentPos->getAdjacentPositions() as $neighbor) {
                $neighborKey = $neighbor->getX() . ',' . $neighbor->getY();

                // Can't move to occupied square
                if ($state->getPlayerAtPosition($neighbor) !== null) {
                    continue;
                }

                $newCost = $currentCost + 1;
                if ($newCost > $maxRange) {
                    continue;
                }

                // Dodge required when leaving a tackle zone
                $leavingTz = $this->tzCalc->countTacklezones(
                    $state,
                    $currentPos,
                    $player->getTeamSide()
                ) > 0;

                // Don't count leaving from start position unless player is actually in TZ
                if ($currentKey === $startKey) {
                    $leavingTz = $isInTz;
                }

                $requiresDodge = $leavingTz;
                $dodgeTarget = $requiresDodge
                    ? $this->tzCalc->calculateDodgeTarget($state, $player, $neighbor, $currentPos)
                    : 0;

                $isGfi = $newCost > $ma;
                $newDodges = $currentData['dodges'] + ($requiresDodge ? 1 : 0);
                $newGfis = $currentData['gfis'] + ($isGfi ? 1 : 0);

                $step = new MoveStep(
                    position: $neighbor,
                    movementCost: $newCost,
                    requiresDodge: $requiresDodge,
                    dodgeTarget: $dodgeTarget,
                    isGfi: $isGfi,
                );

                $newSteps = $currentData['steps'];
                $newSteps[] = $step;

                // Only keep the best path (fewest dodges, then fewest GFIs, then lowest cost)
                if (isset($visited[$neighborKey])) {
                    $existing = $visited[$neighborKey];
                    if ($this->isBetterPath($newDodges, $newGfis, $newCost, $existing)) {
                        // Replace with better path
                        $visited[$neighborKey] = [
                            'cost' => $newCost,
                            'dodges' => $newDodges,
                            'gfis' => $newGfis,
                            'steps' => $newSteps,
                            'prev' => $currentKey,
                            'hasLeaped' => $currentData['hasLeaped'],
                        ];
                        // Re-enqueue for exploration from this node
                        $queue[] = ['pos' => $neighbor, 'key' => $neighborKey];
                    }
                    continue;
                }

                $visited[$neighborKey] = [
                    'cost' => $newCost,
                    'dodges' => $newDodges,
                    'gfis' => $newGfis,
                    'steps' => $newSteps,
                    'prev' => $currentKey,
                    'hasLeaped' => $currentData['hasLeaped'],
                ];

                $queue[] = ['pos' => $neighbor, 'key' => $neighborKey];
            }

            // Leap moves: jump 2 squares in any of 8 directions (once per movement)
            if ($hasLeap && !$currentData['hasLeaped']) {
                foreach ($this->getLeapDestinations($currentPos) as $leapDest) {
                    if (!$leapDest->isOnPitch()) {
                        continue;
                    }
                    if ($state->getPlayerAtPosition($leapDest) !== null) {
                        continue;
                    }

                    $leapDestKey = $leapDest->getX() . ',' . $leapDest->getY();
                    $newCost = $currentCost + 2;
                    if ($newCost > $maxRange) {
                        continue;
                    }

                    $isGfi = $newCost > $ma;
                    // Leap requires AG roll (like dodge) at destination with TZ
                    $leapTz = $this->tzCalc->countTacklezones($state, $leapDest, $player->getTeamSide());
                    $leapTarget = max(2, min(6, 7 - $player->getStats()->getAgility() + $leapTz));

                    $newDodges = $currentData['dodges'] + 1; // count leap as a "dodge" for path scoring
                    $newGfis = $currentData['gfis'] + ($isGfi ? 1 : 0);

                    $step = new MoveStep(
                        position: $leapDest,
                        movementCost: $newCost,
                        requiresDodge: false,
                        dodgeTarget: $leapTarget,
                        isGfi: $isGfi,
                        isLeap: true,
                    );

                    $newSteps = $currentData['steps'];
                    $newSteps[] = $step;

                    if (isset($visited[$leapDestKey])) {
                        $existing = $visited[$leapDestKey];
                        if ($this->isBetterPath($newDodges, $newGfis, $newCost, $existing)) {
                            $visited[$leapDestKey] = [
                                'cost' => $newCost,
                                'dodges' => $newDodges,
                                'gfis' => $newGfis,
                                'steps' => $newSteps,
                                'prev' => $currentKey,
                                'hasLeaped' => true,
                            ];
                            $queue[] = ['pos' => $leapDest, 'key' => $leapDestKey];
                        }
                        continue;
                    }

                    $visited[$leapDestKey] = [
                        'cost' => $newCost,
                        'dodges' => $newDodges,
                        'gfis' => $newGfis,
                        'steps' => $newSteps,
                        'prev' => $currentKey,
                        'hasLeaped' => true,
                    ];

                    $queue[] = ['pos' => $leapDest, 'key' => $leapDestKey];
                }
            }
        }

        // Build result paths (skip start position)
        foreach ($visited as $key => $data) {
            if ($key === $startKey) {
                continue;
            }

            $parts = explode(',', $key);
            $destination = new Position((int) $parts[0], (int) $parts[1]);

            $results[$key] = new MovePath(
                destination: $destination,
                steps: $data['steps'],
                totalCost: $data['cost'],
                dodgeCount: $data['dodges'],
                gfiCount: $data['gfis'],
            );
        }

        return $results;
    }

    /**
     * Find specific path to a destination.
     */
    public function findPathTo(GameState $state, MatchPlayerDTO $player, Position $destination): ?MovePath
    {
        $key = $destination->getX() . ',' . $destination->getY();
        $moves = $this->findValidMoves($state, $player);
        return $moves[$key] ?? null;
    }

    /**
     * Get leap destinations: 2 squares away in 8 directions.
     *
     * @return list<Position>
     */
    private function getLeapDestinations(Position $pos): array
    {
        $x = $pos->getX();
        $y = $pos->getY();
        $directions = [
            [0, -2], [2, -2], [2, 0], [2, 2],
            [0, 2], [-2, 2], [-2, 0], [-2, -2],
        ];

        $destinations = [];
        foreach ($directions as [$dx, $dy]) {
            $destinations[] = new Position($x + $dx, $y + $dy);
        }
        return $destinations;
    }

    /**
     * @param array{cost: int, dodges: int, gfis: int, steps: list<MoveStep>, prev: ?string} $existing
     */
    private function isBetterPath(int $newDodges, int $newGfis, int $newCost, array $existing): bool
    {
        // Prefer fewer dodges
        if ($newDodges < $existing['dodges']) {
            return true;
        }
        if ($newDodges > $existing['dodges']) {
            return false;
        }

        // Then fewer GFIs
        if ($newGfis < $existing['gfis']) {
            return true;
        }
        if ($newGfis > $existing['gfis']) {
            return false;
        }

        // Then lower cost
        return $newCost < $existing['cost'];
    }
}
