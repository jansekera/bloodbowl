<?php

declare(strict_types=1);

namespace App\Tests\Container;

use App\Container\Container;
use PHPUnit\Framework\TestCase;

final class ContainerTest extends TestCase
{
    public function testGetReturnsSingleton(): void
    {
        $container = new Container();
        $container->set(\stdClass::class, fn() => new \stdClass());

        $a = $container->get(\stdClass::class);
        $b = $container->get(\stdClass::class);

        $this->assertSame($a, $b);
    }

    public function testHasReturnsTrueForRegistered(): void
    {
        $container = new Container();
        $container->set('service', fn() => new \stdClass());

        $this->assertTrue($container->has('service'));
        $this->assertFalse($container->has('nonexistent'));
    }

    public function testGetThrowsForUnknownService(): void
    {
        $container = new Container();

        $this->expectException(\RuntimeException::class);
        $this->expectExceptionMessage('Service not found: unknown');
        $container->get('unknown'); // @phpstan-ignore argument.type
    }

    public function testSetOverridesPrevious(): void
    {
        $container = new Container();
        $container->set(\stdClass::class, fn() => (object) ['v' => 1]);
        $first = $container->get(\stdClass::class);
        $this->assertSame(1, $first->v);

        $container->set(\stdClass::class, fn() => (object) ['v' => 2]);
        $second = $container->get(\stdClass::class);
        $this->assertSame(2, $second->v);
        $this->assertNotSame($first, $second);
    }

    public function testFactoryReceivesContainer(): void
    {
        $container = new Container();
        $container->set(\stdClass::class, fn() => (object) ['name' => 'dependency']);
        $container->set('svc', function (Container $c) {
            $dep = $c->get(\stdClass::class);
            return (object) ['dep' => $dep];
        });

        $svc = $container->get('svc'); // @phpstan-ignore argument.type
        /** @var object{dep: object{name: string}} $svc */
        $this->assertSame('dependency', $svc->dep->name);
    }

    public function testMultipleServicesAreIndependent(): void
    {
        $container = new Container();
        $container->set('a', fn() => new \stdClass());
        $container->set('b', fn() => new \stdClass());

        // @phpstan-ignore argument.type, argument.type
        $this->assertNotSame($container->get('a'), $container->get('b'));
    }
}
