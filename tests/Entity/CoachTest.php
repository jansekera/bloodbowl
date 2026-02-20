<?php

declare(strict_types=1);

namespace App\Tests\Entity;

use App\Entity\Coach;
use PHPUnit\Framework\TestCase;

final class CoachTest extends TestCase
{
    public function testFromRow(): void
    {
        $coach = Coach::fromRow([
            'id' => '1',
            'name' => 'TestCoach',
            'email' => 'test@example.com',
            'password_hash' => 'hashed',
            'created_at' => '2025-01-01 00:00:00',
        ]);

        $this->assertSame(1, $coach->getId());
        $this->assertSame('TestCoach', $coach->getName());
        $this->assertSame('test@example.com', $coach->getEmail());
        $this->assertSame('hashed', $coach->getPasswordHash());
        $this->assertSame('2025-01-01 00:00:00', $coach->getCreatedAt());
    }

    public function testToArrayExcludesPassword(): void
    {
        $coach = Coach::fromRow([
            'id' => '1',
            'name' => 'TestCoach',
            'email' => 'test@example.com',
            'password_hash' => 'secret',
            'created_at' => '2025-01-01 00:00:00',
        ]);

        $array = $coach->toArray();

        $this->assertSame(1, $array['id']);
        $this->assertSame('TestCoach', $array['name']);
        $this->assertSame('test@example.com', $array['email']);
        $this->assertArrayNotHasKey('password_hash', $array);
    }
}
