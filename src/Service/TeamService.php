<?php

declare(strict_types=1);

namespace App\Service;

use App\Entity\Player;
use App\Entity\PositionalTemplate;
use App\Entity\Skill;
use App\Entity\Team;
use App\Exception\NotFoundException;
use App\Exception\ValidationException;
use App\Repository\PlayerRepository;
use App\Repository\RaceRepository;
use App\Repository\SkillRepository;
use App\Repository\TeamRepository;
use App\Validation\RosterValidator;

final class TeamService
{
    private const STARTING_TREASURY = 1000000;

    public function __construct(
        private readonly TeamRepository $teamRepository,
        private readonly PlayerRepository $playerRepository,
        private readonly RaceRepository $raceRepository,
        private readonly RosterValidator $rosterValidator,
        private readonly ?SkillRepository $skillRepository = null,
        private readonly ?SPPService $sppService = null,
    ) {
    }

    public function createTeam(int $coachId, int $raceId, string $name): Team
    {
        $name = trim($name);
        $errors = [];

        if ($name === '') {
            $errors[] = 'Team name is required';
        }

        if (strlen($name) > 100) {
            $errors[] = 'Team name must be at most 100 characters';
        }

        $race = $this->raceRepository->findById($raceId);
        if ($race === null) {
            $errors[] = 'Invalid race selected';
        }

        if ($this->teamRepository->nameExistsForCoach($coachId, $name)) {
            $errors[] = 'You already have a team with this name';
        }

        if ($errors !== []) {
            throw new ValidationException($errors);
        }

        return $this->teamRepository->save([
            'coach_id' => $coachId,
            'race_id' => $raceId,
            'name' => $name,
            'treasury' => self::STARTING_TREASURY,
        ]);
    }

    public function hirePlayer(int $teamId, int $templateId, string $playerName): Player
    {
        $team = $this->teamRepository->findById($teamId);
        if ($team === null) {
            throw new NotFoundException('Team not found');
        }

        $race = $this->raceRepository->findByIdWithPositionals($team->getRaceId());
        if ($race === null) {
            throw new NotFoundException('Race not found');
        }

        $template = null;
        foreach ($race->getPositionals() as $pos) {
            if ($pos->getId() === $templateId) {
                $template = $pos;
                break;
            }
        }

        if ($template === null) {
            throw new ValidationException(['Invalid positional for this race']);
        }

        $playerName = trim($playerName);
        if ($playerName === '') {
            throw new ValidationException(['Player name is required']);
        }

        $errors = $this->rosterValidator->validateHirePlayer(
            $teamId,
            $template,
            $team->getTreasury()->getGold(),
        );

        if ($errors !== []) {
            throw new ValidationException($errors);
        }

        $number = $this->playerRepository->getNextNumber($teamId);

        $player = $this->playerRepository->save([
            'team_id' => $teamId,
            'positional_template_id' => $templateId,
            'name' => $playerName,
            'number' => $number,
            'ma' => $template->getStats()->getMovement(),
            'st' => $template->getStats()->getStrength(),
            'ag' => $template->getStats()->getAgility(),
            'av' => $template->getStats()->getArmour(),
        ]);

        // Add starting skills
        $startingSkillIds = array_map(
            fn($s) => $s->getId(),
            $template->getStartingSkills(),
        );
        if ($startingSkillIds !== []) {
            $this->playerRepository->addStartingSkills($player->getId(), $startingSkillIds);
            // Re-fetch to include skills
            $player = $this->playerRepository->findById($player->getId()) ?? $player;
        }

        // Deduct cost from treasury
        $newTreasury = $team->getTreasury()->spend($template->getCost());
        $this->teamRepository->updateTreasury($teamId, $newTreasury->getGold());

        return $player;
    }

    public function firePlayer(int $teamId, int $playerId): void
    {
        $player = $this->playerRepository->findById($playerId);
        if ($player === null || $player->getTeamId() !== $teamId) {
            throw new NotFoundException('Player not found in this team');
        }

        $this->playerRepository->updateStatus($playerId, 'retired');
    }

    public function buyReroll(int $teamId): void
    {
        $team = $this->teamRepository->findById($teamId);
        if ($team === null) {
            throw new NotFoundException('Team not found');
        }

        $race = $this->raceRepository->findById($team->getRaceId());
        if ($race === null) {
            throw new NotFoundException('Race not found');
        }

        $errors = $this->rosterValidator->validateBuyReroll(
            $race->getRerollCost(),
            $team->getTreasury()->getGold(),
        );

        if ($errors !== []) {
            throw new ValidationException($errors);
        }

        $newTreasury = $team->getTreasury()->spend($race->getRerollCost());
        $this->teamRepository->updateTreasury($teamId, $newTreasury->getGold());
        $this->teamRepository->updateRerolls($teamId, $team->getRerolls() + 1);
    }

    public function buyApothecary(int $teamId): void
    {
        $team = $this->teamRepository->findById($teamId);
        if ($team === null) {
            throw new NotFoundException('Team not found');
        }

        $race = $this->raceRepository->findById($team->getRaceId());
        if ($race === null) {
            throw new NotFoundException('Race not found');
        }

        $errors = $this->rosterValidator->validateBuyApothecary(
            $race->hasApothecary(),
            $team->hasApothecary(),
            $team->getTreasury()->getGold(),
        );

        if ($errors !== []) {
            throw new ValidationException($errors);
        }

        $newTreasury = $team->getTreasury()->spend(RosterValidator::getApothecaryCost());
        $this->teamRepository->updateTreasury($teamId, $newTreasury->getGold());
        $this->teamRepository->updateApothecary($teamId, true);
    }

    public function retireTeam(int $teamId): void
    {
        $team = $this->teamRepository->findById($teamId);
        if ($team === null) {
            throw new NotFoundException('Team not found');
        }

        $this->teamRepository->updateStatus($teamId, 'retired');
    }

    /**
     * Get available skills for advancement.
     *
     * @return array{normal: list<array{id: int, name: string, category: string}>, double: list<array{id: int, name: string, category: string}>, can_advance: bool}
     */
    public function getAvailableSkillsForPlayer(int $playerId): array
    {
        $player = $this->playerRepository->findById($playerId);
        if ($player === null) {
            throw new NotFoundException('Player not found');
        }

        $sppService = $this->sppService ?? new SPPService();
        $skillRepo = $this->skillRepository ?? new SkillRepository();

        $nonStarting = $this->playerRepository->countNonStartingSkills($playerId);
        $canAdvance = $sppService->canAdvance($player->getSpp(), $nonStarting);

        if (!$canAdvance) {
            return ['normal' => [], 'double' => [], 'can_advance' => false];
        }

        $template = $this->raceRepository->findPositionalTemplateById($player->getPositionalTemplateId());
        if ($template === null) {
            return ['normal' => [], 'double' => [], 'can_advance' => false];
        }

        $allSkills = $skillRepo->findAll();
        $available = $sppService->getAvailableSkills(
            $player->getSkills(),
            $template->getNormalAccess(),
            $template->getDoubleAccess(),
            $allSkills,
        );

        return [
            'normal' => array_map(fn(Skill $s) => $s->toArray(), $available['normal']),
            'double' => array_map(fn(Skill $s) => $s->toArray(), $available['double']),
            'can_advance' => true,
        ];
    }

    /**
     * Advance a player by learning a new skill.
     */
    public function advancePlayer(int $playerId, int $skillId): Player
    {
        $player = $this->playerRepository->findById($playerId);
        if ($player === null) {
            throw new NotFoundException('Player not found');
        }

        $sppService = $this->sppService ?? new SPPService();
        $skillRepo = $this->skillRepository ?? new SkillRepository();

        // Check can advance
        $nonStarting = $this->playerRepository->countNonStartingSkills($playerId);
        if (!$sppService->canAdvance($player->getSpp(), $nonStarting)) {
            throw new ValidationException(['Player has no pending level-up']);
        }

        // Check skill exists
        $skill = $skillRepo->findById($skillId);
        if ($skill === null) {
            throw new NotFoundException('Skill not found');
        }

        // Check player doesn't already have the skill
        foreach ($player->getSkills() as $owned) {
            if ($owned->getId() === $skillId) {
                throw new ValidationException(['Player already has this skill']);
            }
        }

        // Check skill is in available normal/double
        $template = $this->raceRepository->findPositionalTemplateById($player->getPositionalTemplateId());
        if ($template === null) {
            throw new NotFoundException('Positional template not found');
        }

        $allSkills = $skillRepo->findAll();
        $available = $sppService->getAvailableSkills(
            $player->getSkills(),
            $template->getNormalAccess(),
            $template->getDoubleAccess(),
            $allSkills,
        );

        $isAvailable = false;
        foreach (array_merge($available['normal'], $available['double']) as $avail) {
            if ($avail->getId() === $skillId) {
                $isAvailable = true;
                break;
            }
        }

        if (!$isAvailable) {
            throw new ValidationException(['Skill is not available for this player']);
        }

        // Add skill
        $this->playerRepository->addSkill($playerId, $skillId);

        // Update level
        $newLevel = $sppService->getLevel($player->getSpp());
        $this->playerRepository->updateSPP($playerId, $player->getSpp(), $newLevel);

        return $this->playerRepository->findById($playerId) ?? $player;
    }
}
