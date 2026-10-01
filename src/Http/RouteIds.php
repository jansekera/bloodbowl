<?php

declare(strict_types=1);

namespace App\Http;

/**
 * P99, security review 01.10.2026: an id beyond the PostgreSQL `integer` column (2 147 483 647) reached
 * the database and ended as 500 ("value out of range"). Such an id cannot exist, so the route is a 404.
 */
final class RouteIds
{
    private const MAX_DB_INTEGER = 2147483647;

    public static function outOfRange(string $uri): bool
    {
        preg_match_all('#/(\d+)(?=/|$)#', $uri, $m);
        foreach ($m[1] as $digits) {
            if (strlen($digits) > 10 || (int) $digits > self::MAX_DB_INTEGER) {
                return true;
            }
        }
        return false;
    }
}
