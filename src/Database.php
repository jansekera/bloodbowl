<?php

declare(strict_types=1);

namespace App;

use PDO;

final class Database
{
    private static ?PDO $connection = null;
    private static bool $bootstrapped = false;

    public static function getConnection(): PDO
    {
        if (self::$connection === null) {
            self::bootstrap();

            /** @var array{db: array{host: string, port: int, dbname: string, user: string, password: string}} $config */
            $config = require __DIR__ . '/../config.php';
            $db = $config['db'];

            $dsn = "pgsql:host={$db['host']};port={$db['port']};dbname={$db['dbname']}";

            self::$connection = new PDO($dsn, $db['user'], $db['password'], [
                PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
                PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC,
            ]);
        }

        return self::$connection;
    }

    public static function setConnection(PDO $connection): void
    {
        self::$connection = $connection;
    }

    public static function resetConnection(): void
    {
        self::$connection = null;
    }

    private static function bootstrap(): void
    {
        if (self::$bootstrapped) {
            return;
        }

        $envFile = __DIR__ . '/../.env';
        if (file_exists($envFile)) {
            $dotenv = \Dotenv\Dotenv::createUnsafeImmutable(__DIR__ . '/..');
            $dotenv->safeLoad();
        }

        self::$bootstrapped = true;
    }
}
