<?php

declare(strict_types=1);

namespace App\Engine\Action;

/**
 * P226: odtlačení došlo k hráči kouče-člověka se Stand Firm, na kterého kouč ještě neodpověděl.
 * Vyhazuje ho a chytá jen `BlockHandler` -- vyhodnocení bloku se přeruší (do té chvíle se nic
 * nehodilo ani nepohnulo, odtlačení je až do rozhodnutí čisté) a vznikne `PendingStandFirmDTO`.
 */
final class StandFirmQuestion extends \RuntimeException
{
    public function __construct(public readonly int $playerId)
    {
        parent::__construct("Stand Firm question for player {$playerId}");
    }
}
