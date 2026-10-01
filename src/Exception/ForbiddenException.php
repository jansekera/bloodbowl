<?php

declare(strict_types=1);

namespace App\Exception;

/** The caller is logged in but may not do this (HTTP 403). */
final class ForbiddenException extends \RuntimeException
{
}
