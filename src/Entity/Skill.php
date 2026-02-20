<?php

declare(strict_types=1);

namespace App\Entity;

use App\Enum\SkillCategory;

final class Skill
{
    public function __construct(
        private readonly int $id,
        private readonly string $name,
        private readonly SkillCategory $category,
        private readonly string $description,
    ) {
    }

    /**
     * @param array<string, mixed> $row
     */
    public static function fromRow(array $row): self
    {
        return new self(
            id: (int) $row['id'],
            name: (string) $row['name'],
            category: SkillCategory::from((string) $row['category']),
            description: (string) $row['description'],
        );
    }

    public function getId(): int
    {
        return $this->id;
    }

    public function getName(): string
    {
        return $this->name;
    }

    public function getCategory(): SkillCategory
    {
        return $this->category;
    }

    public function getDescription(): string
    {
        return $this->description;
    }

    /**
     * @return array{id: int, name: string, category: string, description: string}
     */
    public function toArray(): array
    {
        return [
            'id' => $this->id,
            'name' => $this->name,
            'category' => $this->category->value,
            'description' => $this->description,
        ];
    }
}
