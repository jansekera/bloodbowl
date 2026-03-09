<?php

declare(strict_types=1);

namespace App\Container;

final class Container
{
    /** @var array<string, \Closure> */
    private array $factories = [];

    /** @var array<string, object> */
    private array $instances = [];

    public function set(string $id, \Closure $factory): void
    {
        $this->factories[$id] = $factory;
        unset($this->instances[$id]);
    }

    /**
     * @template T of object
     * @param class-string<T> $id
     * @return T
     */
    public function get(string $id): object
    {
        if (isset($this->instances[$id])) {
            /** @var T */
            return $this->instances[$id];
        }

        if (!isset($this->factories[$id])) {
            throw new \RuntimeException("Service not found: {$id}");
        }

        $this->instances[$id] = ($this->factories[$id])($this);
        /** @var T */
        return $this->instances[$id];
    }

    public function has(string $id): bool
    {
        return isset($this->factories[$id]) || isset($this->instances[$id]);
    }
}
