<?php

declare(strict_types=1);

namespace App\Tests\Http;

use App\Exception\AuthenticationRequiredException;
use App\Exception\ForbiddenException;
use App\Exception\NotFoundException;
use App\Exception\ValidationException;
use App\Http\ErrorResponse;
use App\Http\RouteIds;
use PHPUnit\Framework\Attributes\DataProvider;
use PHPUnit\Framework\TestCase;

/**
 * P115 + P99 — security review 01.10.2026: nepřihlášený dostal 500 místo 401, obří ID v adrese 500 místo 404,
 * a uživateli mohl uniknout text výjimky z databáze. Jedno místo, které z výjimky udělá odpověď.
 */
final class ErrorResponseTest extends TestCase
{
    public function testApiWithoutLoginIs401(): void
    {
        $r = ErrorResponse::from(new AuthenticationRequiredException(), isApi: true);
        $this->assertSame(401, $r->status);
        $this->assertSame(['error' => 'Authentication required'], json_decode($r->body, true));
    }

    public function testPageWithoutLoginRedirectsToLogin(): void
    {
        $r = ErrorResponse::from(new AuthenticationRequiredException(), isApi: false);
        $this->assertSame(302, $r->status);
        $this->assertSame(['Location' => '/login'], $r->headers);
    }

    /** @return iterable<string, array{\Throwable, int}> */
    public static function knownErrors(): iterable
    {
        yield 'not found' => [new NotFoundException('Team not found'), 404];
        yield 'forbidden' => [new ForbiddenException('Not your turn'), 403];
        yield 'validation' => [new ValidationException(['Name is required']), 422];
    }

    #[DataProvider('knownErrors')]
    public function testKnownErrorsKeepTheirStatusAndMessage(\Throwable $e, int $status): void
    {
        $r = ErrorResponse::from($e, isApi: true);
        $this->assertSame($status, $r->status);
        $this->assertStringContainsString(
            $e instanceof ValidationException ? 'Name is required' : $e->getMessage(),
            $r->body,
        );
    }

    public function testUnexpectedErrorIsGeneric500WithoutTheMessage(): void
    {
        $secret = 'SQLSTATE[22003]: Numeric value out of range: integer, table coaches';
        $r = ErrorResponse::from(new \PDOException($secret), isApi: true);
        $this->assertSame(500, $r->status);
        $this->assertStringNotContainsString('SQLSTATE', $r->body);
        $this->assertSame(['error' => 'Internal server error'], json_decode($r->body, true));
    }

    public function testUnexpectedErrorOnPageIsPlainTextWithoutTheMessage(): void
    {
        $r = ErrorResponse::from(new \RuntimeException('/home/x/secret.php line 3'), isApi: false);
        $this->assertSame(500, $r->status);
        $this->assertStringNotContainsString('secret', $r->body);
    }

    /** @return iterable<string, array{string, bool}> */
    public static function uris(): iterable
    {
        yield 'normal id' => ['/api/v1/teams/42', false];
        yield 'largest int4' => ['/api/v1/teams/2147483647', false];
        yield 'int4 + 1' => ['/api/v1/teams/2147483648', true];
        yield 'huge id' => ['/matches/99999999999', true];
        yield 'second id huge' => ['/api/v1/matches/1/players/99999999999/moves', true];
        yield 'api version is not an id' => ['/api/v1/races', false];
    }

    #[DataProvider('uris')]
    public function testIdsBeyondTheDatabaseIntegerAreRejected(string $uri, bool $outOfRange): void
    {
        $this->assertSame($outOfRange, RouteIds::outOfRange($uri));
    }
}
