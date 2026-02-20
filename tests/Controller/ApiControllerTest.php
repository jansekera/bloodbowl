<?php

declare(strict_types=1);

namespace App\Tests\Controller;

use App\Controller\ApiController;
use App\Database;
use App\Repository\RaceRepository;
use App\Repository\SkillRepository;
use PHPUnit\Framework\TestCase;

final class ApiControllerTest extends TestCase
{
    private ApiController $controller;

    protected function setUp(): void
    {
        $pdo = Database::getConnection();
        $this->controller = new ApiController(
            new RaceRepository($pdo),
            new SkillRepository($pdo),
        );
    }

    /**
     * @return array<string, mixed>
     */
    private function captureJson(callable $fn): array
    {
        ob_start();
        $fn();
        $output = (string) ob_get_clean();

        /** @var array<string, mixed> */
        return json_decode($output, true);
    }

    public function testGetRaces(): void
    {
        $data = $this->captureJson(fn() => $this->controller->getRaces());

        $this->assertArrayHasKey('data', $data);
        $this->assertArrayHasKey('_links', $data);
        $this->assertCount(26, $data['data']);

        $names = array_column($data['data'], 'name');
        $this->assertContains('Human', $names);
        $this->assertContains('Orc', $names);
        $this->assertContains('Skaven', $names);
        $this->assertContains('Dwarf', $names);
        $this->assertContains('Wood Elf', $names);
        $this->assertContains('Chaos', $names);
        $this->assertContains('Undead', $names);
        $this->assertContains('Lizardmen', $names);
        $this->assertContains('Dark Elf', $names);
    }

    public function testGetRacesIncludesPositionals(): void
    {
        $data = $this->captureJson(fn() => $this->controller->getRaces());

        foreach ($data['data'] as $race) {
            $this->assertArrayHasKey('positionals', $race);
            $this->assertNotEmpty($race['positionals'], "Race {$race['name']} should have positionals");
        }
    }

    public function testGetSkills(): void
    {
        $data = $this->captureJson(fn() => $this->controller->getSkills());

        $this->assertArrayHasKey('data', $data);
        $this->assertArrayHasKey('_links', $data);
        $this->assertGreaterThanOrEqual(30, count($data['data']));

        $first = $data['data'][0];
        $this->assertArrayHasKey('id', $first);
        $this->assertArrayHasKey('name', $first);
        $this->assertArrayHasKey('category', $first);
        $this->assertArrayHasKey('description', $first);
    }
}
