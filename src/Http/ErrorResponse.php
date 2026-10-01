<?php

declare(strict_types=1);

namespace App\Http;

use App\Exception\AuthenticationRequiredException;
use App\Exception\ForbiddenException;
use App\Exception\NotFoundException;
use App\Exception\ValidationException;

/**
 * The one place that turns an exception into an HTTP response.
 *
 * P115, security review 01.10.2026: there was no global handler — a missing login ended as 500
 * instead of 401, and a page could show the raw text of a database exception. Known exceptions keep
 * their status and message; anything else is a generic 500 and its text goes only to the log.
 */
final readonly class ErrorResponse
{
    /** @param array<string, string> $headers */
    private function __construct(
        public int $status,
        public string $body,
        public array $headers = [],
        public string $contentType = 'application/json',
    ) {}

    public static function from(\Throwable $e, bool $isApi): self
    {
        return match (true) {
            $e instanceof AuthenticationRequiredException => $isApi
                ? self::json(401, ['error' => 'Authentication required'])
                : new self(302, '', ['Location' => '/login'], 'text/plain'),
            $e instanceof NotFoundException => self::message(404, $e->getMessage(), $isApi),
            $e instanceof ForbiddenException => self::message(403, $e->getMessage(), $isApi),
            $e instanceof ValidationException => $isApi
                ? self::json(422, ['errors' => $e->getErrors()])
                : self::message(422, implode(' ', $e->getErrors()), false),
            default => self::message(500, 'Internal server error', $isApi),
        };
    }

    /** Sends the response; unexpected errors are logged with their full text. */
    public function send(\Throwable $e): void
    {
        if ($this->status === 500) {
            error_log((string) $e);
        }
        if (!headers_sent()) {
            http_response_code($this->status);
            header('Content-Type: ' . $this->contentType . '; charset=utf-8');
            foreach ($this->headers as $name => $value) {
                header("$name: $value");
            }
        }
        echo $this->body;
    }

    /** @param array<string, mixed> $data */
    private static function json(int $status, array $data): self
    {
        return new self($status, (string) json_encode($data, JSON_UNESCAPED_UNICODE));
    }

    private static function message(int $status, string $message, bool $isApi): self
    {
        return $isApi
            ? self::json($status, ['error' => $message])
            : new self($status, htmlspecialchars("$status $message"), [], 'text/html');
    }
}
