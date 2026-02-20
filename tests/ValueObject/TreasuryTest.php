<?php

declare(strict_types=1);

namespace App\Tests\ValueObject;

use App\Exception\InsufficientFundsException;
use App\ValueObject\Treasury;
use PHPUnit\Framework\TestCase;

final class TreasuryTest extends TestCase
{
    public function testCreation(): void
    {
        $treasury = new Treasury(1000000);
        $this->assertSame(1000000, $treasury->getGold());
    }

    public function testNegativeThrows(): void
    {
        $this->expectException(\InvalidArgumentException::class);
        new Treasury(-1);
    }

    public function testCanAfford(): void
    {
        $treasury = new Treasury(100000);

        $this->assertTrue($treasury->canAfford(50000));
        $this->assertTrue($treasury->canAfford(100000));
        $this->assertFalse($treasury->canAfford(100001));
    }

    public function testSpend(): void
    {
        $treasury = new Treasury(100000);
        $after = $treasury->spend(60000);

        $this->assertSame(40000, $after->getGold());
        $this->assertSame(100000, $treasury->getGold()); // immutable
    }

    public function testSpendInsufficientFunds(): void
    {
        $treasury = new Treasury(50000);

        $this->expectException(InsufficientFundsException::class);
        $treasury->spend(50001);
    }

    public function testAdd(): void
    {
        $treasury = new Treasury(100000);
        $after = $treasury->add(50000);

        $this->assertSame(150000, $after->getGold());
        $this->assertSame(100000, $treasury->getGold()); // immutable
    }

    public function testEquals(): void
    {
        $t1 = new Treasury(100000);
        $t2 = new Treasury(100000);
        $t3 = new Treasury(200000);

        $this->assertTrue($t1->equals($t2));
        $this->assertFalse($t1->equals($t3));
    }

    public function testFormat(): void
    {
        $this->assertSame('1,000,000g', (new Treasury(1000000))->format());
        $this->assertSame('50,000g', (new Treasury(50000))->format());
        $this->assertSame('0g', (new Treasury(0))->format());
    }
}
