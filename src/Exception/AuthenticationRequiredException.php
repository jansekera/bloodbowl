<?php

declare(strict_types=1);

namespace App\Exception;

/** Nobody is logged in (HTTP 401 for the API, redirect to /login for pages). */
final class AuthenticationRequiredException extends \RuntimeException
{
    public function __construct()
    {
        parent::__construct('Authentication required');
    }
}
