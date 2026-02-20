<?php

declare(strict_types=1);

namespace App\Exception;

final class ValidationException extends \RuntimeException
{
    /**
     * @param list<string> $errors
     */
    public function __construct(
        private readonly array $errors,
        string $message = 'Validation failed',
    ) {
        parent::__construct($message);
    }

    /**
     * @return list<string>
     */
    public function getErrors(): array
    {
        return $this->errors;
    }
}
