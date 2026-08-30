<?php
declare(strict_types=1);

namespace Tests\Enum;

use App\DTO\GameEvent;
use App\Enum\GameEventType;
use App\Enum\SkillCategory;
use PHPUnit\Framework\TestCase;
use ValueError;

/**
 * Typ události žije na TŘECH místech: v enumu, v továrních metodách
 * `GameEvent` a v `CHECK` v migraci 004. Tenhle test hlídá, že se
 * NEROZEJDOU -- protože rozejít se můžou tiše:
 *
 *   · přidá se event v PHP a zapomene se migrace  => INSERT spadne v provozu
 *   · přidá se do migrace a ne do enumu           => `from()` spadne při čtení
 *
 * Obojí se projeví až u zákazníka. Tady se to projeví v CI.
 */
final class GameEventTypeTest extends TestCase
{
    /** @return list<string> */
    private function typesUsedByFactories(): array
    {
        $src = file_get_contents(__DIR__ . '/../../src/DTO/GameEvent.php');
        self::assertIsString($src);
        preg_match_all("/new self\(\s*'([A-Za-z_]+)'/", $src, $m);
        return array_values(array_unique($m[1]));
    }

    /** @return list<string> */
    private function typesInMigration(): array
    {
        $sql = file_get_contents(__DIR__ . '/../../migrations/004_enum_checks.sql');
        self::assertIsString($sql);
        $start = strpos($sql, 'match_events_event_type_check');
        self::assertNotFalse($start);
        $tail = substr($sql, $start);
        preg_match_all("/'([a-z_]+)'/", $tail, $m);
        return array_values(array_unique($m[1]));
    }

    public function testEveryFactoryTypeIsInTheEnum(): void
    {
        $missing = array_diff($this->typesUsedByFactories(),
                              array_column(GameEventType::cases(), 'value'));
        self::assertSame([], array_values($missing),
            'GameEvent vyrábí typ, který enum nezná — doplň ho do GameEventType.');
    }

    public function testEnumAndMigrationCheckAgree(): void
    {
        $enum = array_column(GameEventType::cases(), 'value');
        $sql  = $this->typesInMigration();
        sort($enum); sort($sql);
        self::assertSame($enum, $sql,
            'CHECK v migraci 004 se rozešel s GameEventType — INSERT by v provozu spadl.');
    }

    public function testAnUnknownTypeIsRejectedWhereItIsCreated(): void
    {
        // ⭐ Ne až při INSERTu: překlep má spadnout v místě, kde vznikl.
        $this->expectException(ValueError::class);
        new GameEvent('touchdwon', 'překlep');
    }

    public function testAKnownTypeStillPassesThrough(): void
    {
        $e = new GameEvent('touchdown', 'TD');
        self::assertSame('touchdown', $e->getType());
        self::assertSame(GameEventType::TOUCHDOWN, $e->getTypeEnum());
    }

    public function testSkillCategoryCheckMatchesTheEnum(): void
    {
        $sql = file_get_contents(__DIR__ . '/../../migrations/004_enum_checks.sql');
        self::assertIsString($sql);
        $seg = substr($sql, (int) strpos($sql, 'skills_category_check'));
        $seg = substr($seg, 0, (int) strpos($seg, '));'));
        preg_match_all("/'([A-Za-z]+)'/", $seg, $m);
        $sqlVals  = array_values(array_unique($m[1]));
        $enumVals = array_column(SkillCategory::cases(), 'value');
        sort($sqlVals); sort($enumVals);
        self::assertSame($enumVals, $sqlVals);
    }
}
