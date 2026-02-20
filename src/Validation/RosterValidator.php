<?php

declare(strict_types=1);

namespace App\Validation;

use App\Entity\PositionalTemplate;
use App\Repository\PlayerRepository;

final class RosterValidator
{
    public const MIN_PLAYERS = 11;
    public const MAX_PLAYERS = 16;
    private const APOTHECARY_COST = 50000;
    private const ASSISTANT_COACH_COST = 10000;
    private const CHEERLEADER_COST = 10000;

    public function __construct(private readonly PlayerRepository $playerRepository)
    {
    }

    /**
     * @return list<string>
     */
    public function validateHirePlayer(int $teamId, PositionalTemplate $template, int $treasury): array
    {
        $errors = [];

        $activeCount = $this->playerRepository->countActive($teamId);
        if ($activeCount >= self::MAX_PLAYERS) {
            $errors[] = "Team already has maximum of " . self::MAX_PLAYERS . " players";
        }

        $positionalCount = $this->playerRepository->countByPositionalTemplate($teamId, $template->getId());
        if ($positionalCount >= $template->getMaxCount()) {
            $errors[] = "Maximum of {$template->getMaxCount()} {$template->getName()}s allowed";
        }

        if ($treasury < $template->getCost()) {
            $errors[] = "Insufficient funds: need {$template->getCost()}g, have {$treasury}g";
        }

        return $errors;
    }

    /**
     * @return list<string>
     */
    public function validateBuyReroll(int $rerollCost, int $treasury): array
    {
        $errors = [];

        if ($treasury < $rerollCost) {
            $errors[] = "Insufficient funds: need {$rerollCost}g, have {$treasury}g";
        }

        return $errors;
    }

    /**
     * @return list<string>
     */
    public function validateBuyApothecary(bool $raceHasApothecary, bool $teamHasApothecary, int $treasury): array
    {
        $errors = [];

        if (!$raceHasApothecary) {
            $errors[] = "This race cannot hire an apothecary";
        }

        if ($teamHasApothecary) {
            $errors[] = "Team already has an apothecary";
        }

        if ($treasury < self::APOTHECARY_COST) {
            $errors[] = "Insufficient funds: need " . self::APOTHECARY_COST . "g, have {$treasury}g";
        }

        return $errors;
    }

    public static function getApothecaryCost(): int
    {
        return self::APOTHECARY_COST;
    }

    public static function getAssistantCoachCost(): int
    {
        return self::ASSISTANT_COACH_COST;
    }

    public static function getCheerleaderCost(): int
    {
        return self::CHEERLEADER_COST;
    }
}
