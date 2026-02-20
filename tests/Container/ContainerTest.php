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
        $container->set('service', fn() => new \stdClass());

        $a = $container->get('service');
        $b = $container->get('service');

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
        $container->get('unknown');
    }

    public function testSetOverridesPrevious(): void
    {
        $container = new Container();
        $container->set('svc', fn() => (object) ['v' => 1]);
        $first = $container->get('svc');
        $this->assertSame(1, $first->v);

        $container->set('svc', fn() => (object) ['v' => 2]);
        $second = $container->get('svc');
        $this->assertSame(2, $second->v);
        $this->assertNotSame($first, $second);
    }

    public function testFactoryReceivesContainer(): void
    {
        $container = new Container();
        $container->set('dep', fn() => (object) ['name' => 'dependency']);
        $container->set('svc', function (Container $c) {
            $dep = $c->get('dep');
            return (object) ['dep' => $dep];
        });

        $svc = $container->get('svc');
        $this->assertSame('dependency', $svc->dep->name);
    }

    public function testMultipleServicesAreIndependent(): void
    {
        $container = new Container();
        $container->set('a', fn() => new \stdClass());
        $container->set('b', fn() => new \stdClass());

        $this->assertNotSame($container->get('a'), $container->get('b'));
    }
}
