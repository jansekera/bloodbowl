<?php
declare(strict_types=1);

namespace App\Tests\AI;

use App\AI\FeatureExtractor;
use App\AI\GameLogger;
use App\DTO\TeamStateDTO;
use App\Enum\TeamSide;
use App\Tests\Engine\GameStateBuilder;
use PHPUnit\Framework\TestCase;

final class GameLoggerTest extends TestCase
{
    private string $tmpDir;

    protected function setUp(): void
    {
        $this->tmpDir = sys_get_temp_dir() . '/bb_logger_test_' . uniqid();
        mkdir($this->tmpDir, 0777, true);
    }

    protected function tearDown(): void
    {
        // Clean up
        $files = glob($this->tmpDir . '/*');
        foreach ($files as $file) {
            unlink($file);
        }
        rmdir($this->tmpDir);
    }

    public function testLogStateWritesValidJsonWithTypeState(): void
    {
        $builder = new GameStateBuilder();
        $builder->addPlayer(TeamSide::HOME, 10, 7, id: 1);
        $builder->addPlayer(TeamSide::AWAY, 15, 7, id: 2);
        $state = $builder->build();

        $path = $this->tmpDir . '/game.jsonl';
        $logger = new GameLogger($path);
        $logger->logState($state, TeamSide::HOME);
        $logger->close();

        $lines = file($path, FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES);
        $this->assertCount(1, $lines);

        $record = json_decode($lines[0], true);
        $this->assertSame('state', $record['type']);
        $this->assertArrayHasKey('features', $record);
        $this->assertSame('home', $record['perspective']);
    }

    public function testLogResultWritesValidJsonWithTypeResult(): void
    {
        $builder = new GameStateBuilder();
        $builder->addPlayer(TeamSide::HOME, 10, 7, id: 1);
        $homeTeam = TeamStateDTO::create(1, 'Home', 'Human', TeamSide::HOME, 3)->withScore(2);
        $awayTeam = TeamStateDTO::create(2, 'Away', 'Orc', TeamSide::AWAY, 2)->withScore(1);
        $builder->withHomeTeam($homeTeam);
        $builder->withAwayTeam($awayTeam);
        $state = $builder->build();

        $path = $this->tmpDir . '/game.jsonl';
        $logger = new GameLogger($path);
        $logger->logResult($state);
        $logger->close();

        $lines = file($path, FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES);
        $record = json_decode($lines[0], true);

        $this->assertSame('result', $record['type']);
        $this->assertSame(2, $record['home_score']);
        $this->assertSame(1, $record['away_score']);
        $this->assertSame('home', $record['winner']);
    }

    public function testFeaturesHaveCorrectCount(): void
    {
        $builder = new GameStateBuilder();
        $builder->addPlayer(TeamSide::HOME, 10, 7, id: 1);
        $builder->addPlayer(TeamSide::AWAY, 15, 7, id: 2);
        $state = $builder->build();

        $path = $this->tmpDir . '/game.jsonl';
        $logger = new GameLogger($path);
        $logger->logState($state, TeamSide::HOME);
        $logger->close();

        $lines = file($path, FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES);
        $record = json_decode($lines[0], true);

        $this->assertCount(FeatureExtractor::NUM_FEATURES, $record['features']);
    }

    public function testOneLinePerLogStateCall(): void
    {
        $builder = new GameStateBuilder();
        $builder->addPlayer(TeamSide::HOME, 10, 7, id: 1);
        $builder->addPlayer(TeamSide::AWAY, 15, 7, id: 2);
        $state = $builder->build();

        $path = $this->tmpDir . '/game.jsonl';
        $logger = new GameLogger($path);
        $logger->logState($state, TeamSide::HOME);
        $logger->logState($state, TeamSide::HOME);
        $logger->logState($state, TeamSide::HOME);
        $logger->close();

        $lines = file($path, FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES);
        $this->assertCount(3, $lines);
    }
}
