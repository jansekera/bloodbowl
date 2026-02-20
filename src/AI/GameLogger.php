<?php
declare(strict_types=1);

namespace App\AI;

use App\DTO\GameState;
use App\Enum\TeamSide;

final class GameLogger
{
    /** @var resource|null */
    private $handle = null;

    public function __construct(private readonly string $outputPath)
    {
    }

    /**
     * Log a decision point: state features from a given perspective.
     */
    public function logState(GameState $state, TeamSide $perspective): void
    {
        $features = FeatureExtractor::extract($state, $perspective);

        $record = [
            'type' => 'state',
            'half' => $state->getHalf(),
            'turn' => $state->getTeamState($perspective)->getTurnNumber(),
            'features' => $features,
            'perspective' => $perspective->value,
        ];

        $this->writeLine($record);
    }

    /**
     * Log final game result.
     */
    public function logResult(GameState $state): void
    {
        $homeScore = $state->getHomeTeam()->getScore();
        $awayScore = $state->getAwayTeam()->getScore();

        $winner = null;
        if ($homeScore > $awayScore) {
            $winner = 'home';
        } elseif ($awayScore > $homeScore) {
            $winner = 'away';
        }

        $record = [
            'type' => 'result',
            'home_score' => $homeScore,
            'away_score' => $awayScore,
            'winner' => $winner,
        ];

        $this->writeLine($record);
    }

    public function close(): void
    {
        if ($this->handle !== null) {
            fclose($this->handle);
            $this->handle = null;
        }
    }

    /**
     * @param array<string, mixed> $record
     */
    private function writeLine(array $record): void
    {
        if ($this->handle === null) {
            $dir = dirname($this->outputPath);
            if (!is_dir($dir)) {
                mkdir($dir, 0777, true);
            }
            $this->handle = fopen($this->outputPath, 'a');
        }

        fwrite($this->handle, json_encode($record, JSON_UNESCAPED_UNICODE) . "\n");
    }
}
