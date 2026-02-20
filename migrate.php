<?php

declare(strict_types=1);

require __DIR__ . '/vendor/autoload.php';

use App\Database;

$pdo = Database::getConnection();

echo "Running migrations...\n";

$files = glob(__DIR__ . '/migrations/*.sql');
sort($files);

foreach ($files as $file) {
    $name = basename($file);
    echo "  Running {$name}... ";

    $sql = file_get_contents($file);
    if ($sql === false) {
        echo "ERROR: could not read file\n";
        continue;
    }

    try {
        $pdo->exec($sql);
        echo "OK\n";
    } catch (\PDOException $e) {
        echo "ERROR: " . $e->getMessage() . "\n";
    }
}

echo "Done!\n";
