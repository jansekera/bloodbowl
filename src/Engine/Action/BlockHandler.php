<?php

declare(strict_types=1);

namespace App\Engine\Action;

use App\DTO\ActionResult;
use App\DTO\BallState;
use App\DTO\GameEvent;
use App\DTO\GameState;
use App\DTO\MatchPlayerDTO;
use App\DTO\PendingBlockDTO;
use App\Enum\BlockDiceFace;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use App\ValueObject\Position;
use App\Engine\BallResolver;
use App\Engine\DiceRollerInterface;
use App\Engine\InjuryResolver;
use App\Engine\PassResolver;
use App\Engine\StrengthCalculator;
use App\Engine\TacklezoneCalculator;

final class BlockHandler implements ActionHandlerInterface
{
    private ?PassResolver $passResolver = null;

    public function __construct(
        private readonly DiceRollerInterface $dice,
        private readonly StrengthCalculator $strCalc,
        private readonly TacklezoneCalculator $tzCalc,
        private readonly InjuryResolver $injuryResolver,
        private readonly BallResolver $ballResolver,
    ) {}

    /**
     * P82 (30.09.2026), port C++ 366fda3e / `block_handler.cpp:553-582`.
     * `rules_bb2016.txt` r. 546-552: rana v blitzu „costs one square of
     * movement". Bez normalniho pohybu se na ni GFI (2+, ve vanici 3+,
     * pocita se do limitu GFI; zakoreneny GFI nesmi, r. 8577-8578); bez GFI
     * se rana nehodi. Neuspesne GFI = pad ve VLASTNIM poli pred ranou, turnover.
     * Prehozy jako `attemptRoll` v C++: Sure Feet (1x za kolo), jinak Pro,
     * jinak tymovy (Loner). ⚠️ Interaktivni volbu prehozu tu clovek zatim nema.
     *
     * @param list<GameEvent> $events
     * @return array{state: GameState, outcome: 'paid'|'unpayable'|'fell'}
     */
    private function payBlitzBlock(GameState $state, MatchPlayerDTO $attacker, array &$events): array
    {
        $gfiFloor = $attacker->isRooted() ? 0 : ($attacker->hasSkill(SkillName::Sprint) ? -3 : -2);
        $remaining = $attacker->getMovementRemaining() - 1;
        if ($remaining < $gfiFloor) {
            $state = $state->withPlayer($attacker->withHasActed(true)->withHasMoved(true));
            return ['state' => $state, 'outcome' => 'unpayable'];
        }
        $attacker = $attacker->withMovementRemaining($remaining);
        $state = $state->withPlayer($attacker);
        if ($remaining >= 0) {
            return ['state' => $state, 'outcome' => 'paid'];
        }

        $id = $attacker->getId();
        $side = $attacker->getTeamSide();
        $threshold = $state->getWeather() === \App\Enum\Weather::BLIZZARD ? 3 : 2;
        $roll = $this->dice->rollD6();
        $ok = $roll >= $threshold;
        $events[] = GameEvent::gfiAttempt($id, $roll, $ok);

        if (!$ok && $attacker->hasSkill(SkillName::SureFeet) && !$attacker->isSureFeetUsedThisTurn()) {
            $attacker = $attacker->withSureFeetUsedThisTurn(true);
            $roll = $this->dice->rollD6();
            $ok = $roll >= $threshold;
            $events[] = GameEvent::rerollUsed($id, 'Sure Feet');
            $events[] = GameEvent::gfiAttempt($id, $roll, $ok);
        } elseif (!$ok && $attacker->hasSkill(SkillName::Pro) && !$attacker->isProUsedThisTurn()) {
            $attacker = $attacker->withProUsedThisTurn(true);
            $pro = \App\Engine\ProCheck::roll($this->dice, $attacker, $state->getTeamState($side)->canUseReroll(), $events);
            if ($pro['teamRerollUsed']) {
                $state = $state->withTeamState($side, $state->getTeamState($side)->withRerollUsed());
            }
            if ($pro['allowed']) {
                $roll = $this->dice->rollD6();
                $ok = $roll >= $threshold;
                $events[] = GameEvent::gfiAttempt($id, $roll, $ok);
            }
        } elseif (!$ok && $state->getTeamState($side)->canUseReroll()) {
            $state = $state->withTeamState($side, $state->getTeamState($side)->withRerollUsed());
            $lonerOk = true;
            if ($attacker->hasSkill(SkillName::Loner)) {
                $lonerRoll = $this->dice->rollD6();
                $lonerOk = $lonerRoll >= 4;
                $events[] = GameEvent::lonerCheck($id, $lonerRoll, $lonerOk);
            }
            if ($lonerOk) {
                $roll = $this->dice->rollD6();
                $ok = $roll >= $threshold;
                $events[] = GameEvent::rerollUsed($id, 'Team Reroll');
                $events[] = GameEvent::gfiAttempt($id, $roll, $ok);
            }
        }

        if ($ok) {
            return ['state' => $state->withPlayer($attacker), 'outcome' => 'paid'];
        }

        $events[] = GameEvent::playerFell($id);
        $events[] = GameEvent::turnover('Failed Going For It');
        $fallen = $attacker->withState(PlayerState::PRONE)->withHasActed(true)->withHasMoved(true);
        $injResult = $this->injuryResolver->resolve($fallen, $this->dice);
        $fallen = $injResult['player'];
        $state = $state->withPlayer($fallen);
        $events = array_merge($events, $injResult['events']);
        [$state, $events] = $this->ballResolver->handleBallOnPlayerDown($state, $fallen, $events);

        return ['state' => $state, 'outcome' => 'fell'];
    }

    /**
     * P81 krok 3 (30.09.2026), port C++ `wantsFollowUp` (`block_handler.cpp:461`).
     * `rules_bb2016.txt` r. 608-611: follow-up je volba utocnika; povinny jen pro
     * Frenzy (r. 8138: „must always follow up if they can").
     * Clovek volbu posila (`$choice`); AI: mimo blitz pole zdarma bere, bez
     * zbytku pohybu taky; v blitzu se zbytkem pohybu nenasleduje na pole
     * s vic tackle zonami, nez kde stoji -- chce jeste odejit.
     */
    private function wantsFollowUp(
        GameState $state,
        MatchPlayerDTO $attacker,
        Position $attackerPos,
        Position $vacated,
        bool $isBlitz,
        ?bool $choice,
    ): bool {
        if ($attacker->hasSkill(SkillName::Frenzy)) {
            return true;
        }
        if ($choice !== null) {
            return $choice;
        }
        if (!$isBlitz || $attacker->getMovementRemaining() <= 0) {
            return true;
        }
        $side = $attacker->getTeamSide();
        $tzStay = $this->tzCalc->countTacklezones($state, $attackerPos, $side, $attacker->getId());
        $tzGo = $this->tzCalc->countTacklezones($state, $vacated, $side, $attacker->getId());

        return $tzGo <= $tzStay;
    }

    public function setPassResolver(PassResolver $passResolver): void
    {
        $this->passResolver = $passResolver;
    }

    /**
     * @param array<string, mixed> $params
     */
    public function resolve(GameState $state, array $params): ActionResult
    {
        $attackerId = (int) $params['playerId'];
        $targetId = (int) $params['targetId'];

        $attacker = $state->getPlayer($attackerId);
        $defender = $state->getPlayer($targetId);

        if ($attacker === null || $defender === null) {
            throw new \InvalidArgumentException('Player not found');
        }

        $events = [];

        // Jump Up: stand up the prone attacker for free before blocking
        if ($attacker->getState() === PlayerState::PRONE && $attacker->hasSkill(SkillName::JumpUp)) {
            $attacker = $attacker->withState(PlayerState::STANDING);
            $state = $state->withPlayer($attacker);
            $events[] = GameEvent::standUp($attackerId);
        }

        $attackerPos = $attacker->getPosition();
        $defenderPos = $defender->getPosition();

        if ($attackerPos === null || $defenderPos === null) {
            throw new \InvalidArgumentException('Players must be on pitch');
        }

        if ($attackerPos->distanceTo($defenderPos) !== 1) {
            throw new \InvalidArgumentException('Players must be adjacent to block');
        }

        // P82: rana v blitzu stoji pole pohybu (pripadne GFI).
        if (!empty($params['isBlitz'])) {
            $paid = $this->payBlitzBlock($state, $attacker, $events);
            $state = $paid['state'];
            if ($paid['outcome'] === 'fell') {
                return ActionResult::turnover($state->withTurnoverPending(true), $events);
            }
            if ($paid['outcome'] === 'unpayable') {
                return ActionResult::success($state, $events);
            }
            $attacker = $state->requirePlayer($attackerId);
        }

        // Dump-Off: defender with ball can quick pass before block
        if ($defender->hasSkill(SkillName::DumpOff)
            && $state->getBall()->getCarrierId() === $defender->getId()
            && !$state->getTeamState($defender->getTeamSide())->isPassUsedThisTurn()
            && $this->passResolver !== null
        ) {
            // Find closest friendly teammate adjacent to defender
            $dumpTarget = $this->findDumpOffTarget($state, $defender);
            if ($dumpTarget !== null) {
                $dumpResult = $this->passResolver->resolveDumpOff($state, $defender, $dumpTarget);
                $state = $dumpResult['state'];
                $events = array_merge($events, $dumpResult['events']);
                // Refresh attacker/defender from updated state
                $attacker = $state->getPlayer($attackerId);
                $defender = $state->getPlayer($targetId);
                if ($attacker === null || $defender === null) {
                    throw new \InvalidArgumentException('Player not found after dump-off');
                }
            }
        }

        // Foul Appearance: defender check before any block type proceeds
        if ($defender->hasSkill(SkillName::FoulAppearance)) {
            $faRoll = $this->dice->rollD6();
            $events[] = GameEvent::foulAppearance($defender->getId(), $attacker->getId(), $faRoll, $faRoll >= 2);
            if ($faRoll < 2) {
                // Failed: attacker loses action, NOT a turnover
                $state = $state->withPlayer(
                    $attacker->withHasActed(true)->withHasMoved(true),
                );
                return ActionResult::success($state, $events);
            }
        }

        // Chainsaw: D6 misto kostek bloku; 1 = zpetny raz. `rules_bb2016.txt` r. 7996-8017:
        //   brneni zasazeneho +3; srazi-li zpetny raz nositele, je to turnover (r. 366-368).
        if ($attacker->hasSkill(SkillName::Chainsaw)) {
            return $this->resolveChainsaw($state, $attacker, $defender, $events);
        }

        // Stab: bypass block dice entirely, go straight to armor roll
        if ($attacker->hasSkill(SkillName::Stab)) {
            return $this->resolveStab($state, $attacker, $defender, $events);
        }

        // Sily pro blok: Horns (+1 v blitzu), Dauntless, asistence -- viz `blockStrengths`
        [$attStr, $defStr] = $this->blockStrengths($state, $attacker, $defender, !empty($params['hornsBonus']) ? 1 : 0, 0);

        // Determine dice
        $diceInfo = $this->strCalc->getBlockDiceInfo($attStr, $defStr);
        $diceCount = $diceInfo['count'];
        $attackerChooses = $diceInfo['attackerChooses'];

        // Roll block dice
        $faces = [];
        for ($i = 0; $i < $diceCount; $i++) {
            $faces[] = $this->rollBlockDie();
        }

        $isBlitz = !empty($params['isBlitz']);
        $activeSide = $state->getActiveTeam();

        // Create pending block for interactive resolution
        $pending = new PendingBlockDTO(
            attackerId: $attackerId,
            defenderId: $targetId,
            faces: $faces,
            attackerChooses: $attackerChooses,
            isBlitz: $isBlitz,
            isFrenzy: false,
            proAvailable: $attacker->hasSkill(SkillName::Pro) && !$attacker->isProUsedThisTurn(),
            teamRerollAvailable: $state->getTeamState($activeSide)->canUseReroll(),
        );

        $state = $state->withPendingBlock($pending);

        return ActionResult::success($state, $events);
    }

    /**
     * Resolve block choice: apply chosen die face from pending block.
     *
     * @param array<string, mixed> $params
     */
    public function resolveBlockChoice(GameState $state, array $params): ActionResult
    {
        $pending = $state->getPendingBlock();
        if ($pending === null) {
            throw new \InvalidArgumentException('No pending block');
        }

        $faces = $pending->getFaces();

        $attacker = $state->getPlayer($pending->getAttackerId());
        $defender = $state->getPlayer($pending->getDefenderId());

        if ($attacker === null || $defender === null) {
            throw new \InvalidArgumentException('Player not found');
        }

        // OPRAVENO 08.10.2026 (audit parity, nález 5) -- tady stála prázdná "validace"
        //   (`if attackerChooses {} else {}`) a kostka se vždy vzala z `faceIndex` klienta:
        //   člověk-útočník si vybíral i tehdy, když vybírá obránce. Pravidla ř. 633-634:
        //   "The coach of the stronger player picks which block dice is used."
        //   Vybírá-li strana, kterou vede AI, volí její kouč (`autoChooseBlockDie`) a
        //   `faceIndex` se nečte. Jinak vybírá člověk v dialogu.
        if ($state->getPendingBlockChooserSide() === $state->getAiTeam()) {
            $chosenFace = $this->autoChooseBlockDie($faces, $pending->isAttackerChooses(), $attacker, $defender);
        } else {
            $faceIndex = (int) ($params['faceIndex'] ?? -1);
            if ($faceIndex < 0 || $faceIndex >= count($faces)) {
                throw new \InvalidArgumentException('Invalid face index');
            }
            $chosenFace = $faces[$faceIndex];
        }

        $faceValues = array_map(fn(BlockDiceFace $f) => $f->value, $faces);
        $events = [];
        $events[] = GameEvent::blockAttempt(
            $pending->getAttackerId(),
            $pending->getDefenderId(),
            count($faces),
            $pending->isAttackerChooses(),
            $faceValues,
            $chosenFace->value,
        );

        // Clear pending block
        $state = $state->withPendingBlock(null);

        $isBlitz = $pending->isBlitz();
        // P81 krok 3: follow-up je volba -- clovek ji posila spolu s kostkou,
        //   bez parametru rozhodne pravidlo AI (`wantsFollowUp`).
        $followUpChoice = array_key_exists('followUp', $params) ? (bool) $params['followUp'] : null;
        $result = $this->applyBlockResult($state, $attacker, $defender, $chosenFace, $events, $isBlitz, false, $followUpChoice);

        // Frenzy: povinna druha rana, kdyz oba stoji a sousedi -- ale JEN po vysledku
        //   Pushed / Defender Stumbles.
        // OPRAVENO 08.10.2026 (audit parity, nález 7) -- na zvolenou kostku se tu nehledělo:
        //   druhá rána se házela i po Both Down, když oba díky Blocku zůstali stát.
        //   Pravidla ř. 8138-8141: "If a 'Pushed' or 'Defender Stumbles' result was chosen,
        //   the player must immediately throw a second block against the same opponent so
        //   long as they are both still standing and adjacent."
        //   Both Down, který Juggernaut v blitzu bere "as if a 'Pushed' result has been
        //   rolled instead" (ř. 8194-8195), je Pushed.
        $frenzyTrigger = $chosenFace === BlockDiceFace::PUSHED
            || $chosenFace === BlockDiceFace::DEFENDER_STUMBLES
            || ($chosenFace === BlockDiceFace::BOTH_DOWN && $isBlitz && $attacker->hasSkill(SkillName::Juggernaut));
        if ($frenzyTrigger && !$result->isTurnover() && $attacker->hasSkill(SkillName::Frenzy) && !$pending->isFrenzy()) {
            $frenzyState = $result->getNewState();
            $frenzyAttacker = $frenzyState->getPlayer($pending->getAttackerId());
            $frenzyDefender = $frenzyState->getPlayer($pending->getDefenderId());

            if ($frenzyAttacker !== null && $frenzyDefender !== null
                && $frenzyAttacker->getState() === PlayerState::STANDING
                && $frenzyDefender->getState() === PlayerState::STANDING
                && $frenzyAttacker->getPosition() !== null && $frenzyDefender->getPosition() !== null
                && $frenzyAttacker->getPosition()->distanceTo($frenzyDefender->getPosition()) === 1
            ) {
                $frenzyEvents = $result->getEvents();
                // P82: i druha rana Frenzy v blitzu stoji pole (C++ `block_handler.cpp:557`).
                if ($isBlitz) {
                    $paid = $this->payBlitzBlock($frenzyState, $frenzyAttacker, $frenzyEvents);
                    if ($paid['outcome'] === 'fell') {
                        return ActionResult::turnover($paid['state']->withTurnoverPending(true), $frenzyEvents);
                    }
                    if ($paid['outcome'] === 'unpayable') {
                        return ActionResult::success($paid['state'], $frenzyEvents);
                    }
                    $frenzyState = $paid['state'];
                    $frenzyAttacker = $frenzyState->requirePlayer($pending->getAttackerId());
                }
                $frenzyEvents[] = GameEvent::frenzyBlock($pending->getAttackerId(), $pending->getDefenderId());

                // Sily znovu na novych polich -- vcetne Dauntless (pred 08.10.2026 se u druhe
                //   rany nehazel; r. 8026-8027: plati, kdykoli hrac blokuje silnejsiho)
                [$attStr2, $defStr2] = $this->blockStrengths($frenzyState, $frenzyAttacker, $frenzyDefender, 0, 0);
                $diceInfo2 = $this->strCalc->getBlockDiceInfo($attStr2, $defStr2);

                $faces2 = [];
                for ($i = 0; $i < $diceInfo2['count']; $i++) {
                    $faces2[] = $this->rollBlockDie();
                }

                $activeSide = $frenzyState->getActiveTeam();
                $frenzyPending = new PendingBlockDTO(
                    attackerId: $pending->getAttackerId(),
                    defenderId: $pending->getDefenderId(),
                    faces: $faces2,
                    attackerChooses: $diceInfo2['attackerChooses'],
                    isBlitz: $isBlitz,
                    isFrenzy: true,
                    proAvailable: $frenzyAttacker->hasSkill(SkillName::Pro) && !$frenzyAttacker->isProUsedThisTurn(),
                    teamRerollAvailable: $frenzyState->getTeamState($activeSide)->canUseReroll(),
                );

                $frenzyState = $frenzyState->withPendingBlock($frenzyPending);
                return ActionResult::success($frenzyState, $frenzyEvents);
            }
        }

        // Mark attacker as acted
        $finalState = $result->getNewState();
        $updatedAttacker = $finalState->getPlayer($pending->getAttackerId());
        if ($updatedAttacker !== null) {
            // ⛔ P81 (30.09.2026), port C++ 771d1ecc: po rane v BLITZU smi
            //   stojici utocnik dojit zbytkem pohybu (`rules_bb2016.txt`
            //   r. 551-552) -- vcetne GFI, ktere mu zbyly; zakoreneny ne.
            //   Aktivace zustava otevrena: `hasMoved=false` pusti MOVE,
            //   `hasActed=true` zakaze cokoli jineho.
            $gfiFloor = $updatedAttacker->hasSkill(SkillName::Sprint) ? -3 : -2;
            $mayContinue = $isBlitz && !$result->isTurnover()
                && $updatedAttacker->getState() === PlayerState::STANDING
                && !$updatedAttacker->isRooted()
                && $updatedAttacker->getMovementRemaining() > $gfiFloor;
            $finalState = $finalState->withPlayer(
                $updatedAttacker->withHasActed(true)->withHasMoved(!$mayContinue),
            );
            if ($mayContinue) {
                $finalState = $finalState->withBlitzContinuation($updatedAttacker->getId());
            }
        }

        if ($result->isTurnover()) {
            return ActionResult::turnover($finalState, $result->getEvents());
        }

        return ActionResult::success($finalState, $result->getEvents());
    }

    /**
     * Reroll block dice: Pro (4+) or Team Reroll -- oboji prehodi VSECHNY kostky.
     *
     * ⛔ OPRAVA 18.09.2026: `rules_bb2016.txt` r. 919-924 -- "a re-roll allows
     *   you to re-roll all the dice that produced any one result [...] a three
     *   dice block, in which case all three dice would be rolled again"; Pro je
     *   prehoz "any one dice roll" (r. 8381-8382), r. 982 "use my Pro skill to
     *   re-roll that block". Pro driv prehodil jen nejhorsi kostku. Kostka se
     *   prehazuje nejvys jednou (r. 926) => po Pro uz tymovy prehoz jen na
     *   neuspesny HOD PRO (r. 8385-8387), po tymovem uz Pro ne.
     *
     * @param array<string, mixed> $params
     */
    public function resolveBlockReroll(GameState $state, array $params): ActionResult
    {
        $pending = $state->getPendingBlock();
        if ($pending === null) {
            throw new \InvalidArgumentException('No pending block');
        }

        $type = (string) $params['type'];
        $attacker = $state->getPlayer($pending->getAttackerId());
        $defender = $state->getPlayer($pending->getDefenderId());

        if ($attacker === null || $defender === null) {
            throw new \InvalidArgumentException('Player not found');
        }

        $events = [];
        $faces = $pending->getFaces();

        switch ($type) {
            case 'pro':
                if (!$pending->isProAvailable()) {
                    throw new \InvalidArgumentException('Pro not available');
                }
                $proRoll = $this->dice->rollD6();
                $attacker = $attacker->withProUsedThisTurn(true);
                $state = $state->withPlayer($attacker);
                if ($proRoll >= 4) {
                    $events[] = GameEvent::proReroll($attacker->getId(), $proRoll, true, null);
                    $pending = $pending->withRerollUsed()->withFaces($this->rollBlockDice(count($faces)));
                } else {
                    $events[] = GameEvent::proReroll($attacker->getId(), $proRoll, false, null);
                    $pending = $pending->withProFailed();
                }
                $state = $state->withPendingBlock($pending);
                return ActionResult::success($state, $events);

            case 'team':
                if (!$pending->isTeamRerollAvailable()) {
                    throw new \InvalidArgumentException('Team reroll not available');
                }
                $activeSide = $state->getActiveTeam();
                $lonerBlocked = false;
                if ($attacker->hasSkill(SkillName::Loner)) {
                    $lonerRoll = $this->dice->rollD6();
                    $lonerBlocked = $lonerRoll < 4;
                    $events[] = GameEvent::lonerCheck($attacker->getId(), $lonerRoll, !$lonerBlocked);
                }
                // Consume the reroll regardless of Loner
                $state = $state->withTeamState(
                    $activeSide,
                    $state->getTeamState($activeSide)->withRerollUsed(),
                );
                if (!$lonerBlocked) {
                    $events[] = GameEvent::rerollUsed($attacker->getId(), 'Team Reroll');
                    // po neuspesnem Pro se prehazuje HOD PRO (r. 8385-8387)
                    $proAllowed = true;
                    if ($pending->isProFailed()) {
                        $proRoll = $this->dice->rollD6();
                        $proAllowed = $proRoll >= 4;
                        $events[] = GameEvent::proReroll($attacker->getId(), $proRoll, $proAllowed, null);
                    }
                    $pending = $proAllowed
                        ? $pending->withTeamRerollUsed()->withFaces($this->rollBlockDice(count($faces)))
                        : $pending->withTeamRerollUsed();
                } else {
                    $pending = $pending->withTeamRerollUsed();
                }
                $state = $state->withPendingBlock($pending);
                return ActionResult::success($state, $events);

            default:
                throw new \InvalidArgumentException("Unknown reroll type: $type");
        }
    }

    /**
     * Auto-resolve a pending block (for AI): choose best face, use rerolls if beneficial.
     */
    public function autoResolvePendingBlock(GameState $state): ActionResult
    {
        $pending = $state->getPendingBlock();
        if ($pending === null) {
            throw new \InvalidArgumentException('No pending block');
        }

        $attacker = $state->getPlayer($pending->getAttackerId());
        $defender = $state->getPlayer($pending->getDefenderId());
        if ($attacker === null || $defender === null) {
            throw new \InvalidArgumentException('Player not found');
        }

        /** @var list<GameEvent> $accumulatedEvents */
        $accumulatedEvents = [];

        // ⛔ 15.09.2026: tady byl nejdriv pokus o Brawler -- skill z BB2020, v BB2016
        //   neexistuje (`evidence/audit_pravidla_20260915.md`). Odstranen na pokyn uzivatele.
        $bestFace = $this->autoChooseBlockDie($pending->getFaces(), $pending->isAttackerChooses(), $attacker, $defender);

        // Try Pro if result is bad
        $bestScore = $this->scoreBlockFace($bestFace, $attacker, $defender);
        if ($bestScore < 0 && $pending->isProAvailable()) {
            $result = $this->resolveBlockReroll($state, ['type' => 'pro']);
            $accumulatedEvents = array_merge($accumulatedEvents, $result->getEvents());
            $state = $result->getNewState();
            $pending = $state->getPendingBlock();
            if ($pending === null) {
                return $result;
            }
            $attacker = $state->getPlayer($pending->getAttackerId());
            $defender = $state->getPlayer($pending->getDefenderId());
            if ($attacker === null || $defender === null) {
                return $result;
            }
            $bestFace = $this->autoChooseBlockDie($pending->getFaces(), $pending->isAttackerChooses(), $attacker, $defender);
        }

        // Note: team rerolls are NOT auto-used on blocks (available through interactive dialog only)

        // Choose best face
        $bestIdx = 0;
        foreach ($pending->getFaces() as $idx => $f) {
            if ($f === $bestFace) {
                $bestIdx = $idx;
                break;
            }
        }

        $choiceResult = $this->resolveBlockChoice($state, ['faceIndex' => $bestIdx]);

        // Merge accumulated reroll events with choice events
        if ($accumulatedEvents !== []) {
            $allEvents = array_merge($accumulatedEvents, $choiceResult->getEvents());
            if ($choiceResult->isTurnover()) {
                return ActionResult::turnover($choiceResult->getNewState(), $allEvents);
            }
            return ActionResult::success($choiceResult->getNewState(), $allEvents);
        }

        return $choiceResult;
    }

    /**
     * Resolve a Multiple Block action: block two adjacent opponents, each at +2 ST, no follow-up.
     *
     * @param array<string, mixed> $params
     */
    public function resolveMultipleBlock(GameState $state, array $params): ActionResult
    {
        $attackerId = (int) $params['playerId'];
        $targetId1 = (int) $params['targetId'];
        $targetId2 = (int) $params['targetId2'];

        $attacker = $state->getPlayer($attackerId);
        $defender1 = $state->getPlayer($targetId1);
        $defender2 = $state->getPlayer($targetId2);

        if ($attacker === null || $defender1 === null || $defender2 === null) {
            throw new \InvalidArgumentException('Player not found');
        }

        $events = [];

        // Jump Up: stand up the prone attacker for free before blocking
        if ($attacker->getState() === PlayerState::PRONE && $attacker->hasSkill(SkillName::JumpUp)) {
            $attacker = $attacker->withState(PlayerState::STANDING);
            $state = $state->withPlayer($attacker);
            $events[] = GameEvent::standUp($attackerId);
        }

        $events[] = GameEvent::multipleBlock($attackerId, $targetId1, $targetId2);

        // === Block 1: target defender1 ===
        [$state, $events, $attackerDown] = $this->resolveSingleMultipleBlock(
            $state,
            $attacker,
            $defender1,
            $events,
        );

        // If attacker went down on first block, turnover, no second block
        if ($attackerDown) {
            $finalState = $state;
            $updatedAttacker = $finalState->getPlayer($attackerId);
            if ($updatedAttacker !== null) {
                $finalState = $finalState->withPlayer(
                    $updatedAttacker->withHasActed(true)->withHasMoved(true),
                );
            }
            return ActionResult::turnover($finalState, $events);
        }

        // === Block 2: target defender2 (if attacker still standing) ===
        $attacker = $state->getPlayer($attackerId);
        $defender2 = $state->getPlayer($targetId2);

        if ($attacker !== null && $defender2 !== null
            && $attacker->getState() === PlayerState::STANDING
            && $attacker->getPosition() !== null
        ) {
            [$state, $events, $attackerDown2] = $this->resolveSingleMultipleBlock(
                $state,
                $attacker,
                $defender2,
                $events,
            );

            if ($attackerDown2) {
                $finalState = $state;
                $updatedAttacker = $finalState->getPlayer($attackerId);
                if ($updatedAttacker !== null) {
                    $finalState = $finalState->withPlayer(
                        $updatedAttacker->withHasActed(true)->withHasMoved(true),
                    );
                }
                return ActionResult::turnover($finalState, $events);
            }
        }

        // Mark attacker as acted
        $finalState = $state;
        $updatedAttacker = $finalState->getPlayer($attackerId);
        if ($updatedAttacker !== null) {
            $finalState = $finalState->withPlayer(
                $updatedAttacker->withHasActed(true)->withHasMoved(true),
            );
        }

        return ActionResult::success($finalState, $events);
    }

    /**
     * Resolve a single block within a Multiple Block action (+2 defender ST, no follow-up).
     *
     * @param list<GameEvent> $events
     * @return array{0: GameState, 1: list<GameEvent>, 2: bool} Updated state, events, and whether attacker went down
     */
    private function resolveSingleMultipleBlock(
        GameState $state,
        MatchPlayerDTO $attacker,
        MatchPlayerDTO $defender,
        array $events,
    ): array {
        $attackerPos = $attacker->getPosition();
        $defenderPos = $defender->getPosition();

        if ($attackerPos === null || $defenderPos === null) {
            return [$state, $events, false];
        }

        // Dump-Off: defender with ball can quick pass before block
        if ($defender->hasSkill(SkillName::DumpOff)
            && $state->getBall()->getCarrierId() === $defender->getId()
            && !$state->getTeamState($defender->getTeamSide())->isPassUsedThisTurn()
            && $this->passResolver !== null
        ) {
            $dumpTarget = $this->findDumpOffTarget($state, $defender);
            if ($dumpTarget !== null) {
                $dumpResult = $this->passResolver->resolveDumpOff($state, $defender, $dumpTarget);
                $state = $dumpResult['state'];
                $events = array_merge($events, $dumpResult['events']);
                $attacker = $state->getPlayer($attacker->getId());
                $defender = $state->getPlayer($defender->getId());
                if ($attacker === null || $defender === null) {
                    return [$state, $events, false];
                }
            }
        }

        // Foul Appearance check
        if ($defender->hasSkill(SkillName::FoulAppearance)) {
            $faRoll = $this->dice->rollD6();
            $events[] = GameEvent::foulAppearance($defender->getId(), $attacker->getId(), $faRoll, $faRoll >= 2);
            if ($faRoll < 2) {
                // Failed: this block does nothing (but does NOT end the Multiple Block action)
                return [$state, $events, false];
            }
        }

        // Pila se v Multiple Block nepouziva (`rules_bb2016.txt` r. 8014-8015) --
        //   `RulesEngine` tuhle akci nositeli pily nenabidne ani nepovoli.

        // Stab: bypass block dice
        if ($attacker->hasSkill(SkillName::Stab)) {
            $events[] = GameEvent::stab($attacker->getId(), $defender->getId());
            // ⛔ OPRAVA 14.09.2026: MIGHTY BLOW SE SE STAB A CHAINSAW POUZIT NESMI.
            //   `rules_bb2016.txt` r. 8291-8297: "Mighty Blow cannot be used
            //   with the Stab or Chainsaw skills." Tohle je prave ta cesta.
            $mightyBlow = 0;
            $hasClaw = $attacker->hasSkill(SkillName::Claw);
            $hasStakes = $attacker->hasSkill(SkillName::Stakes);
            $hasNurglesRot = $attacker->hasSkill(SkillName::NurglesRot);
            $wasBallCarrier = $state->getBall()->getCarrierId() === $defender->getId();
            $injResult = $this->injuryResolver->resolve($defender, $this->dice, 0, 0, $hasClaw, $hasStakes, $hasNurglesRot, (bool) $mightyBlow, chainsawHolderBonus: false);
            $defender = $injResult['player'];
            $state = $state->withPlayer($defender);
            $events = array_merge($events, $injResult['events']);
            if ($wasBallCarrier && $defender->getState() !== PlayerState::STANDING) {
                [$state, $events] = $this->ballResolver->handleBallOnPlayerDown($state, $defender, $events);
            }
            return [$state, $events, false];
        }

        // Sily pro blok; obrance ma v Multiple Block +2 (`rules_bb2016.txt` r. 8300-8301:
        //   "each defender's strength is increased by 2") -- to je modifikator sily, takze
        //   se s nim pocita uz pro Dauntless ("after all other modifiers", r. 8034-8035).
        [$attStr, $defStr] = $this->blockStrengths($state, $attacker, $defender, 0, 2);

        // Determine dice
        $diceInfo = $this->strCalc->getBlockDiceInfo($attStr, $defStr);
        $diceCount = $diceInfo['count'];
        $attackerChooses = $diceInfo['attackerChooses'];

        // Roll block dice
        $faces = [];
        for ($i = 0; $i < $diceCount; $i++) {
            $faces[] = $this->rollBlockDie();
        }

        // Auto-choose best face
        $chosenFace = $this->autoChooseBlockDie($faces, $attackerChooses, $attacker, $defender);

        // Pro reroll on bad block result
        if ($attacker->hasSkill(SkillName::Pro) && !$attacker->isProUsedThisTurn()) {
            $chosenScore = $this->scoreBlockFace($chosenFace, $attacker, $defender);
            if ($chosenScore < 0) {
                $proRoll = $this->dice->rollD6();
                $attacker = $attacker->withProUsedThisTurn(true);
                $state = $state->withPlayer($attacker);
                if ($proRoll >= 4) {
                    // ⛔ OPRAVA 18.09.2026: Pro prehodi VSECHNY kostky bloku (r. 919-924, 982)
                    $faces = $this->rollBlockDice(count($faces));
                    $chosenFace = $this->autoChooseBlockDie($faces, $attackerChooses, $attacker, $defender);
                    $events[] = GameEvent::proReroll($attacker->getId(), $proRoll, true, null);
                } else {
                    $events[] = GameEvent::proReroll($attacker->getId(), $proRoll, false, null);
                }
            }
        }

        $faceValues = array_map(fn(BlockDiceFace $f) => $f->value, $faces);
        $events[] = GameEvent::blockAttempt(
            $attacker->getId(),
            $defender->getId(),
            $diceCount,
            $attackerChooses,
            $faceValues,
            $chosenFace->value,
        );

        // Apply block result with noFollowUp = true
        $result = $this->applyBlockResult($state, $attacker, $defender, $chosenFace, $events, false, true);

        $attackerDown = $result->isTurnover();
        return [$result->getNewState(), $result->getEvents(), $attackerDown];
    }

    /**
     * Síly obou hráčů pro blok: síla + modifikátory (Horns, +2 v Multiple Block), pak
     * Dauntless, a teprve potom asistence. Jediné místo pro všechny tři rány (běžná,
     * druhá rána Frenzy, Multiple Block).
     *
     * OPRAVENO 08.10.2026 (audit parity, nález 8) -- Dauntless tu byl třikrát špatně:
     *   úspěch už při rovnosti (`>=`); po úspěchu `max(síla útočníka, síla obránce VČETNĚ
     *   jeho asistencí)`, takže obranné asistence zmizely a útočné se nepřičetly; porovnával
     *   holé síly bez Horns; a u druhé rány Frenzy se neházel vůbec.
     *   Pravidla ř. 8026-8035: "The skill only works when the player attempts to block an
     *   opponent who is stronger than himself. ... If the total is equal to or lower than
     *   the opponent's Strength, the player must block using his normal Strength. If the
     *   total is greater, then the player ... counts as having a Strength equal to his
     *   opponent's ... The strength of both players is calculated before any defensive or
     *   offensive assists are added but after all other modifiers."
     *
     * @return array{0: int, 1: int} síla útočníka a obránce včetně asistencí
     */
    private function blockStrengths(
        GameState $state,
        MatchPlayerDTO $attacker,
        MatchPlayerDTO $defender,
        int $attackerModifier,
        int $defenderModifier,
    ): array {
        $attSt = $attacker->getStats()->getStrength() + $attackerModifier;
        $defSt = $defender->getStats()->getStrength() + $defenderModifier;

        if ($attacker->hasSkill(SkillName::Dauntless) && $attSt < $defSt
            && $this->dice->rollD6() + $attSt > $defSt
        ) {
            $attSt = $defSt;
        }

        return [
            $attSt + $this->strCalc->countAssists($state, $attacker, $defender->requirePosition()),
            $defSt + $this->strCalc->countAssists($state, $defender, $attacker->requirePosition()),
        ];
    }

    /**
     * Resolve Stab: armor roll (no block dice, no pushback, never turnover).
     *
     * @param list<GameEvent> $events
     */
    private function resolveStab(
        GameState $state,
        MatchPlayerDTO $attacker,
        MatchPlayerDTO $defender,
        array $events,
    ): ActionResult {
        $events[] = GameEvent::stab($attacker->getId(), $defender->getId());

        $defenderPos = $defender->getPosition();
        $wasBallCarrier = $state->getBall()->getCarrierId() === $defender->getId();
        // ⛔ OPRAVA 14.09.2026: Mighty Blow se se Stab a Chainsaw pouzit nesmi
        //   (`rules_bb2016.txt` r. 8291-8297).
        $mightyBlow = 0;
        $hasClaw = $attacker->hasSkill(SkillName::Claw);
        $hasStakes = $attacker->hasSkill(SkillName::Stakes);
        $hasNurglesRot = $attacker->hasSkill(SkillName::NurglesRot);
        $injResult = $this->injuryResolver->resolve($defender, $this->dice, 0, 0, $hasClaw, $hasStakes, $hasNurglesRot, (bool) $mightyBlow, chainsawHolderBonus: false);
        $defender = $injResult['player'];
        $state = $state->withPlayer($defender);
        $events = array_merge($events, $injResult['events']);

        // Drop ball if defender was carrying and got knocked down/out
        if ($wasBallCarrier && $defender->getState() !== PlayerState::STANDING && $defenderPos !== null) {
            // If defender removed from pitch (KO/injured), ball drops at their original position
            if ($defender->getPosition() === null) {
                $state = $state->withBall(BallState::onGround($defenderPos));
                $bounceResult = $this->ballResolver->resolveBounce($state, $defenderPos);
                $state = $bounceResult['state'];
                $events = array_merge($events, $bounceResult['events']);
            } else {
                [$state, $events] = $this->ballResolver->handleBallOnPlayerDown($state, $defender, $events);
            }
        }

        // Mark attacker as acted
        $state = $state->withPlayer(
            $attacker->withHasActed(true)->withHasMoved(true),
        );

        return ActionResult::success($state, $events);
    }

    /**
     * Resolve Chainsaw: roll D6 first — on 1 kickback (armor roll on attacker),
     * on 2+ armor roll on defender. No block dice, no pushback, never turnover.
     *
     * @param list<GameEvent> $events
     */
    private function resolveChainsaw(
        GameState $state,
        MatchPlayerDTO $attacker,
        MatchPlayerDTO $defender,
        array $events,
    ): ActionResult {
        $events[] = GameEvent::chainsaw($attacker->getId(), $defender->getId());

        // Roll D6: 1 = kickback, 2+ = attack defender
        $chainsawRoll = $this->dice->rollD6();

        if ($chainsawRoll === 1) {
            // Kickback: armor roll on attacker
            $events[] = GameEvent::chainsawKickback($attacker->getId());
            $injResult = $this->injuryResolver->resolve($attacker, $this->dice); // +3 za vlastni pilu prida resolver
            $attacker = $injResult['player'];
            $state = $state->withPlayer($attacker);
            $events = array_merge($events, $injResult['events']);

            // Drop ball if attacker was carrier and knocked down
            if ($attacker->getState() !== PlayerState::STANDING) {
                [$state, $events] = $this->ballResolver->handleBallOnPlayerDown($state, $attacker, $events);
            }

            // Mark as acted
            $freshAttacker = $state->getPlayer($attacker->getId());
            if ($freshAttacker !== null) {
                $state = $state->withPlayer(
                    $freshAttacker->withHasActed(true)->withHasMoved(true),
                );
            }
            // Srazeny hrac tymu na tahu = turnover (`rules_bb2016.txt` r. 366-368).
            if (!$attacker->getState()->canAct()) {
                return ActionResult::turnover($state, $events);
            }
            return ActionResult::success($state, $events);
        }

        // Normal chainsaw attack on defender: armor roll (2D6)
        $defenderPos = $defender->getPosition();
        $wasBallCarrier = $state->getBall()->getCarrierId() === $defender->getId();
        // ⛔ OPRAVA 14.09.2026: Mighty Blow se se Stab a Chainsaw pouzit nesmi
        //   (`rules_bb2016.txt` r. 8291-8297).
        $mightyBlow = 0;
        $hasClaw = $attacker->hasSkill(SkillName::Claw);
        $hasStakes = $attacker->hasSkill(SkillName::Stakes);
        $hasNurglesRot = $attacker->hasSkill(SkillName::NurglesRot);
        $injResult = $this->injuryResolver->resolve($defender, $this->dice, InjuryResolver::CHAINSAW_ARMOUR_BONUS, 0, $hasClaw, $hasStakes, $hasNurglesRot, (bool) $mightyBlow, chainsawHolderBonus: false);
        $defender = $injResult['player'];
        $state = $state->withPlayer($defender);
        $events = array_merge($events, $injResult['events']);

        // Drop ball if defender was carrying and got knocked down/out
        if ($wasBallCarrier && $defender->getState() !== PlayerState::STANDING && $defenderPos !== null) {
            if ($defender->getPosition() === null) {
                $state = $state->withBall(BallState::onGround($defenderPos));
                $bounceResult = $this->ballResolver->resolveBounce($state, $defenderPos);
                $state = $bounceResult['state'];
                $events = array_merge($events, $bounceResult['events']);
            } else {
                [$state, $events] = $this->ballResolver->handleBallOnPlayerDown($state, $defender, $events);
            }
        }

        // Mark attacker as acted
        $freshAttacker = $state->getPlayer($attacker->getId());
        if ($freshAttacker !== null) {
            $state = $state->withPlayer(
                $freshAttacker->withHasActed(true)->withHasMoved(true),
            );
        }

        return ActionResult::success($state, $events);
    }

    /**
     * Apply the chosen block die result.
     *
     * @param list<GameEvent> $events
     */
    private function applyBlockResult(
        GameState $state,
        MatchPlayerDTO $attacker,
        MatchPlayerDTO $defender,
        BlockDiceFace $face,
        array $events,
        bool $isBlitz = false,
        bool $noFollowUp = false,
        ?bool $followUpChoice = null,
    ): ActionResult {
        $attackerPos = $attacker->getPosition();
        $defenderPos = $defender->getPosition();

        if ($attackerPos === null || $defenderPos === null) {
            return ActionResult::failure($state, $events);
        }

        $attackerDown = false;
        $defenderDown = false;
        $defenderPushed = false;

        switch ($face) {
            case BlockDiceFace::ATTACKER_DOWN:
                $attackerDown = true;
                break;

            case BlockDiceFace::BOTH_DOWN:
                // Juggernaut: on blitz, Both Down becomes push
                if ($isBlitz && $attacker->hasSkill(SkillName::Juggernaut)) {
                    $defenderPushed = true;
                    $events[] = GameEvent::juggernaut($attacker->getId());
                    break;
                }
                // Wrestle: oba jdou na zem bez hodu na brneni -- kdyz ho nektery z nich POUZIJE.
                // OPRAVENO 08.10.2026 (audit parity, nález 6) -- tady stálo "má-li Wrestle
                //   kdokoli z dvojice, použije se vždy". Pravidla ř. 8671-8672: "This player
                //   **may** use Wrestle when he blocks or is blocked"; ř. 1820: "Skill use is
                //   not mandatory." Volí se v `wrestleUsed`.
                if ($this->wrestleUsed($state, $attacker, $defender)) {
                    // ⛔⛔ OPRAVA 11.09.2026: Wrestle NENÍ bezpodmínečně bez
                    //   turnoveru. `rules_bb2016.txt` r. 8677-8678:
                    //   „Use of this skill does not cause a turnover **unless
                    //   the active player was holding the ball**."
                    //   A katalog turnoverů r. 368-372 to říká stejně: „being
                    //   Placed Prone is not a turnover unless it is a player
                    //   from the active team holding the ball … e.g. skills
                    //   like Diving Tackle, Piling On and **Wrestle** count as
                    //   being Placed Prone."
                    //
                    // ⚠️ C++ engine tohle opravil UŽ 24.08.2026 jako `F11`
                    //   (`engine/src/block_handler.cpp:826-848`). PHP kopie
                    //   opravu nedostala -- TŘETÍ drift téhož druhu (vedle
                    //   Wild Animal a gaze).
                    //
                    // ⭐ ČTE SE PŘED PÁDEM, protože `handleBallOnPlayerDown`
                    //   míč upustí -- potom už by se nositel nepoznal.
                    //   (Kdyz je nositel prave utocnik, prislusnost k tymu
                    //   z toho plyne -- druha podminka byla zbytecna.)
                    $activeHeldBall
                        = $state->getBall()->getCarrierId() === $attacker->getId();

                    $events[] = GameEvent::wrestle($attacker->getId(), $defender->getId());
                    $attacker = $attacker->withState(PlayerState::PRONE);
                    $defender = $defender->withState(PlayerState::PRONE);
                    $wState = $state->withPlayer($attacker)->withPlayer($defender);
                    [$wState, $events] = $this->ballResolver->handleBallOnPlayerDown($wState, $attacker, $events);
                    $defender = $wState->getPlayer($defender->getId());
                    if ($defender !== null) {
                        [$wState, $events] = $this->ballResolver->handleBallOnPlayerDown($wState, $defender, $events);
                    }

                    if ($activeHeldBall) {
                        $events[] = GameEvent::turnover('Wrestle with the ball');
                        return ActionResult::turnover($wState->withTurnoverPending(true), $events);
                    }

                    return ActionResult::success($wState, $events);
                }
                if (!$attacker->hasSkill(SkillName::Block)) {
                    $attackerDown = true;
                }
                if (!$defender->hasSkill(SkillName::Block)) {
                    $defenderDown = true;
                }
                break;

            case BlockDiceFace::PUSHED:
                $defenderPushed = true;
                break;

            case BlockDiceFace::DEFENDER_STUMBLES:
                $defenderPushed = true;
                if (!$defender->hasSkill(SkillName::Dodge) || $attacker->hasSkill(SkillName::Tackle)) {
                    $defenderDown = true;
                }
                break;

            case BlockDiceFace::DEFENDER_DOWN:
            case BlockDiceFace::POW:
                $defenderDown = true;
                $defenderPushed = true;
                break;
        }

        $currentState = $state;

        // Stand Firm prevents pushback (but not knockdown)
        // Juggernaut on blitz ignores Stand Firm
        // Zakoreneny se neodtlaci nikdy, ani Juggernautem (r. 8578-8579 "for any reason").
        if ($defenderPushed && ($defender->isRooted() || ($defender->hasSkill(SkillName::StandFirm)
            && !($isBlitz && $attacker->hasSkill(SkillName::Juggernaut))))
        ) {
            $defenderPushed = false;
        }

        // Handle pushback first
        if ($defenderPushed) {
            [$currentState, $events] = $this->resolvePushback(
                $currentState,
                $attacker,
                $defender,
                $defenderPos,
                $events,
            );
            // Re-fetch defender (position may have changed)
            $defender = $currentState->getPlayer($defender->getId());
            if ($defender === null) {
                return ActionResult::turnover($currentState, $events);
            }
        }

        // Follow-up: attacker moves to defender's old position
        // Fend: prevents follow-up when defender is not knocked down
        // Multiple Block: no follow-up allowed
        // ⛔ P83 (30.09.2026): zakoreneny blokuje „without following-up"
        //   (`rules_bb2016.txt` r. 8580-8581). Vzor C++ `block_handler.cpp:548`.
        if ($noFollowUp || $attacker->isRooted()) {
            // Skip follow-up entirely for Multiple Block / rooted attacker
        } elseif ($defenderPushed && !($defender->hasSkill(SkillName::Fend) && !$defenderDown)
            && $this->wantsFollowUp($currentState, $attacker, $attackerPos, $defenderPos, $isBlitz, $followUpChoice)) {
            $events[] = GameEvent::followUp($attacker->getId(), (string) $attackerPos, (string) $defenderPos);
            $attacker = $attacker->withPosition($defenderPos);
            $currentState = $currentState->withPlayer($attacker);

            // Move ball with attacker if carried
            if ($currentState->getBall()->getCarrierId() === $attacker->getId()) {
                $currentState = $currentState->withBall(BallState::carried($defenderPos, $attacker->getId()));
            }
        } elseif ($defenderPushed) {
            // Fend: defender has Fend and is not down (guaranteed by previous branch logic)
            $events[] = GameEvent::fend($defender->getId());
        }

        // Handle defender knockdown
        if ($defenderDown && $defender->getPosition() !== null) {
            $events[] = GameEvent::playerFell($defender->getId());
            $defender = $defender->withState(PlayerState::PRONE);
            $currentState = $currentState->withPlayer($defender);

            // Armor/injury roll
            // ⭐ NORMALNI BLOK -- tady Mighty Blow PLATI. `rules_bb2016.txt`
            //   r. 8291-8293: "when an opponent is Knocked Down by this
            //   player DURING A BLOCK". Zakaz se tyka jen Stab a Chainsaw.
            $mightyBlow = $attacker->hasSkill(SkillName::MightyBlow) ? 1 : 0;
            $hasClaw = $attacker->hasSkill(SkillName::Claw);
            $hasStakes = $attacker->hasSkill(SkillName::Stakes);
            $hasNurglesRot = $attacker->hasSkill(SkillName::NurglesRot);
            $injResult = $this->injuryResolver->resolve($defender, $this->dice, 0, 0, $hasClaw, $hasStakes, $hasNurglesRot, (bool) $mightyBlow, regenerace: false);
            $defender = $injResult['player'];
            $currentState = $currentState->withPlayer($defender);
            $events = array_merge($events, $injResult['events']);

            // Piling On: reroll armor if not broken, attacker goes prone
            $freshAttackerForPO = $currentState->getPlayer($attacker->getId());
            if ($freshAttackerForPO !== null && $freshAttackerForPO->hasSkill(SkillName::PilingOn)
                && $defender->getState() === PlayerState::PRONE
                && $freshAttackerForPO->getState() === PlayerState::STANDING
            ) {
                // Reroll armor/injury
                $poInjResult = $this->injuryResolver->resolve($defender, $this->dice, 0, 0, $hasClaw, $hasStakes, $hasNurglesRot, (bool) $mightyBlow, regenerace: false);
                $defender = $poInjResult['player'];
                $currentState = $currentState->withPlayer($defender);
                $events = array_merge($events, $poInjResult['events']);
                $events[] = GameEvent::pilingOn($freshAttackerForPO->getId(), 'armour');

                // Attacker goes prone
                $freshAttackerForPO = $freshAttackerForPO->withState(PlayerState::PRONE);
                $currentState = $currentState->withPlayer($freshAttackerForPO);
            }

            // Apothecary: auto-use on casualty
            $defenderSide = $defender->getTeamSide();
            if ($defender->getState() === PlayerState::INJURED && $currentState->getTeamState($defenderSide)->canUseApothecary()) {
                $currentState = $currentState->withTeamState($defenderSide, $currentState->getTeamState($defenderSide)->withApothecaryUsed());
                $apoResult = $this->injuryResolver->resolveApothecary($defender, $this->dice, PlayerState::INJURED, 0, $events);
                $defender = $apoResult['player'];
                $currentState = $currentState->withPlayer($defender);
                $events = $apoResult['events'];
            }
            // Regeneration az PO lekarnikovi, jednou (r. 8434-8436)
            $regen = $this->injuryResolver->resolveRegeneration($defender, $this->dice, $hasStakes, $events);
            $defender = $regen['player'];
            $currentState = $currentState->withPlayer($defender);
            $events = $regen['events'];

            // Drop ball if carrier
            [$currentState, $events] = $this->ballResolver->handleBallOnPlayerDown($currentState, $defender, $events);
        }

        // Handle attacker knockdown
        if ($attackerDown) {
            $events[] = GameEvent::playerFell($attacker->getId());
            $freshAttacker = $currentState->getPlayer($attacker->getId());
            if ($freshAttacker !== null) {
                $freshAttacker = $freshAttacker->withState(PlayerState::PRONE);
                $currentState = $currentState->withPlayer($freshAttacker);

                // Armor/injury roll for attacker; kdo se srazi o nositele pily, ma +3
                //   (`rules_bb2016.txt` r. 8011-8013)
                $injResult = $this->injuryResolver->resolve(
                    $freshAttacker,
                    $this->dice,
                    $defender->hasSkill(SkillName::Chainsaw) ? InjuryResolver::CHAINSAW_ARMOUR_BONUS : 0,
                    regenerace: false,
                );
                $freshAttacker = $injResult['player'];
                $currentState = $currentState->withPlayer($freshAttacker);
                $events = array_merge($events, $injResult['events']);

                // Apothecary: auto-use on casualty for attacker
                $attackerSide = $freshAttacker->getTeamSide();
                if ($freshAttacker->getState() === PlayerState::INJURED && $currentState->getTeamState($attackerSide)->canUseApothecary()) {
                    $currentState = $currentState->withTeamState($attackerSide, $currentState->getTeamState($attackerSide)->withApothecaryUsed());
                    $apoResult = $this->injuryResolver->resolveApothecary($freshAttacker, $this->dice, PlayerState::INJURED, 0, $events);
                    $freshAttacker = $apoResult['player'];
                    $currentState = $currentState->withPlayer($freshAttacker);
                    $events = $apoResult['events'];
                }
                // Regeneration az PO lekarnikovi, jednou (r. 8434-8436)
                $regen = $this->injuryResolver->resolveRegeneration($freshAttacker, $this->dice, false, $events);
                $freshAttacker = $regen['player'];
                $currentState = $currentState->withPlayer($freshAttacker);
                $events = $regen['events'];

                // Drop ball if carrier
                [$currentState, $events] = $this->ballResolver->handleBallOnPlayerDown($currentState, $freshAttacker, $events);
            }

            // Turnover
            $events[] = GameEvent::turnover('Attacker knocked down');
            return ActionResult::turnover($currentState->withTurnoverPending(true), $events);
        }

        return ActionResult::success($currentState, $events);
    }

    /**
     * Odtlaceni po bloku -- jedine misto pro vsechna odtlaceni vc. retezu
     * (`rules_bb2016.txt` r. 635-653). Verejne i pro Ball & Chain, jehoz
     * odtlaceni jsou bezna odtlaceni (r. 7840-7847).
     *
     * @param list<GameEvent> $events
     * @return array{0: GameState, 1: list<GameEvent>}
     */
    public function resolvePushback(
        GameState $state,
        MatchPlayerDTO $attacker,
        MatchPlayerDTO $defender,
        Position $defenderOriginalPos,
        array $events,
    ): array {
        $attackerPos = $attacker->getPosition();
        if ($attackerPos === null) {
            return [$state, $events];
        }

        return $this->odtlacit($state, $attackerPos, $defender, $defenderOriginalPos, $attacker->getTeamSide(), $attacker, $events);
    }

    /**
     * Jadro odtlaceni. Sekundarni odtlaceni v retezu je "treated exactly like
     * a normal push back as if the second player had been blocked by the first"
     * (r. 644-646), proto se vola rekurzivne tataz funkce.
     * - pole: volne na hristi > dav (vybira tym na tahu) > obsazene = retez (r. 639-651)
     * - smer voli tym na tahu, ledaze ma odtlaceny Side Step -- i v retezu (FAQ);
     * - Grab a Strip Ball patri blokujicimu, plati jen u PRVNIHO odtlaceni
     *   (`$utocnik` je pak null); Grab nesmi zrusit Side Step v retezu (FAQ).
     * - nosic odtlaceny v retezu mic DRZI (neni sraženy).
     *
     * @param list<GameEvent> $events
     * @return array{0: GameState, 1: list<GameEvent>}
     */
    private function odtlacit(
        GameState $state,
        Position $odkud,
        MatchPlayerDTO $kdo,
        Position $kde,
        TeamSide $blockingSide,
        ?MatchPlayerDTO $utocnik,
        array $events,
    ): array {
        $emptySquares = [];
        $occupiedSquares = [];
        $offPitchAvailable = false;
        foreach ($this->getPushbackSquares($odkud, $kde) as $pos) {
            if (!$pos->isOnPitch()) {
                $offPitchAvailable = true;
                continue;
            }
            $occupant = $state->getPlayerAtPosition($pos);
            if ($occupant === null) {
                $emptySquares[] = $pos;
            } elseif (!$occupant->holdsGround($blockingSide)) {
                $occupiedSquares[] = ['pos' => $pos, 'player' => $occupant];
            }
            // Kdo drzi pole (zakoreneny, stojici Stand Firm soupere), neni cil odtlaceni
        }

        $zonySoupere = fn(Position $p): int => $this->tzCalc->countTacklezones($state, $p, $kdo->getTeamSide());
        $pushTo = null;
        $chainPushTarget = null;
        $grab = $utocnik !== null && $utocnik->hasSkill(SkillName::Grab) && !$utocnik->hasSkill(SkillName::Frenzy);
        if ($grab && !$offPitchAvailable) {
            // Grab: utocnik voli nejhorsi volne pole (nejvic zon). Je-li jedno z poli mimo
            //   hriste, rozhoduje bezne poradi niz (volne pole > dav > retez).
            if ($emptySquares !== []) {
                usort($emptySquares, fn(Position $a, Position $b) => $zonySoupere($b) <=> $zonySoupere($a));
                $pushTo = $emptySquares[0];
            } elseif ($occupiedSquares !== []) {
                $pushTo = $occupiedSquares[0]['pos'];
                $chainPushTarget = $occupiedSquares[0]['player'];
            }
        } elseif ($kdo->hasSkill(SkillName::SideStep) && $kdo->getState()->canAct()) {
            // Side Step (i v retezu): odtlaceny voli nejbezpecnejsi pole (nejmin zon)
            if ($emptySquares !== []) {
                usort($emptySquares, fn(Position $a, Position $b) => $zonySoupere($a) <=> $zonySoupere($b));
                $pushTo = $emptySquares[0];
            } elseif ($occupiedSquares !== []) {
                usort($occupiedSquares, fn(array $a, array $b) => $zonySoupere($a['pos']) <=> $zonySoupere($b['pos']));
                $pushTo = $occupiedSquares[0]['pos'];
                $chainPushTarget = $occupiedSquares[0]['player'];
            }
        } elseif ($emptySquares !== []) {
            // OPRAVENO 08.10.2026 (audit parity, nález 3a) -- před touhle větví stálo
            //   `elseif ($offPitchAvailable)` ("dav má přednost"): bylo-li kterékoli ze tří
            //   polí mimo hřiště, šel hráč do davu, i když vedle bylo volné pole. Pravidla
            //   ř. 639: "must be pushed back into an empty square if possible"; ř. 650-651:
            //   do davu jen "if there are no eligible empty squares on the pitch".
            // nejhorsi volne pole pro odtlaceneho: nejvic zon, pak bliz k lajne
            usort($emptySquares, function (Position $a, Position $b) use ($zonySoupere) {
                $tzA = $zonySoupere($a);
                $tzB = $zonySoupere($b);
                if ($tzA !== $tzB) {
                    return $tzB <=> $tzA;
                }
                return min($a->getY(), 14 - $a->getY()) <=> min($b->getY(), 14 - $b->getY());
            });
            $pushTo = $emptySquares[0];
        } elseif ($offPitchAvailable) {
            // Zadne volne pole na hristi a jedno z poli je mimo ⇒ dav (r. 650-651), ne retez
            //   -- pushTo zustava null
        } elseif ($occupiedSquares !== []) {
            $pushTo = $occupiedSquares[0]['pos'];
            $chainPushTarget = $occupiedSquares[0]['player'];
        }

        // Retez: nejdriv uvolnit pole tatazi funkci
        if ($pushTo !== null && $chainPushTarget !== null) {
            [$state, $events] = $this->odtlacit($state, $kde, $chainPushTarget, $pushTo, $blockingSide, null, $events);
        }

        if ($pushTo === null) {
            // Dav (r. 650-663): zraneni bez brneni, nosice vhazuje dav od jeho posledniho pole
            $events[] = $utocnik !== null
                ? GameEvent::crowdSurf($kdo->getId())
                : GameEvent::chainPush($kdo->getId(), (string) $kde, 'off-pitch');
            if ($utocnik === null) {
                $events[] = GameEvent::crowdSurf($kdo->getId());
            }
            $hadBall = $state->getBall()->getCarrierId() === $kdo->getId();
            $kdo = $kdo->withPosition(null);
            $state = $state->withPlayer($kdo);
            if ($hadBall) {
                [$state, $events] = $this->throwInFromCrowd($state, $kde, $odkud, $events);
            }
            $injResult = $this->injuryResolver->resolveCrowdSurf($kdo, $this->dice);
            $events = array_merge($events, $injResult['events']);

            return [$state->withPlayer($injResult['player']), $events];
        }

        $events[] = $utocnik !== null
            ? GameEvent::playerPushed($kdo->getId(), (string) $kde, (string) $pushTo)
            : GameEvent::chainPush($kdo->getId(), (string) $kde, (string) $pushTo);
        $kdo = $kdo->withPosition($pushTo);
        $state = $state->withPlayer($kdo);

        // Odtlaceny nosic mic drzi -- jen Strip Ball blokujiciho mu ho vyrazi
        if ($state->getBall()->getCarrierId() === $kdo->getId()) {
            if ($utocnik !== null && $utocnik->hasSkill(SkillName::StripBall)) {
                $events[] = GameEvent::ballStripped($kdo->getId());
                $state = $state->withBall(BallState::onGround($pushTo));
                $bounceResult = $this->ballResolver->resolveBounce($state, $pushTo);
                $events = array_merge($events, $bounceResult['events']);
                $state = $bounceResult['state'];
            } else {
                $state = $state->withBall(BallState::carried($pushTo, $kdo->getId()));
            }
        }

        return [$state, $events];
    }

    /**
     * Nosic vytlaceny do davu: dav vhazuje mic od jeho posledniho pole (r. 659-663).
     * Strana sablony = pole za nim ve smeru odstrceni (jako C++ `pushOffPitchExit`).
     *
     * @param list<GameEvent> $events
     * @return array{0: GameState, 1: list<GameEvent>}
     */
    private function throwInFromCrowd(GameState $state, Position $lastSquare, Position $pusherPos, array $events): array
    {
        $exit = new Position(
            $lastSquare->getX() + ($lastSquare->getX() <=> $pusherPos->getX()),
            $lastSquare->getY() + ($lastSquare->getY() <=> $pusherPos->getY()),
        );
        $state = $state->withBall(BallState::onGround($lastSquare));
        $throwIn = $this->ballResolver->resolveThrowIn($state, $lastSquare, $exit);

        return [$throwIn['state'], array_merge($events, $throwIn['events'])];
    }

    /**
     * Get valid pushback squares (away from attacker).
     *
     * @return list<Position>
     */
    private function getPushbackSquares(Position $attackerPos, Position $defenderPos): array
    {
        $dx = $defenderPos->getX() - $attackerPos->getX();
        $dy = $defenderPos->getY() - $attackerPos->getY();

        // Normalize direction
        $ndx = $dx === 0 ? 0 : ($dx > 0 ? 1 : -1);
        $ndy = $dy === 0 ? 0 : ($dy > 0 ? 1 : -1);

        $squares = [];

        // Direct push (primary direction)
        $squares[] = new Position($defenderPos->getX() + $ndx, $defenderPos->getY() + $ndy);

        // Two diagonal alternatives
        if ($ndx === 0) {
            // Vertical push - diagonals are left and right
            $squares[] = new Position($defenderPos->getX() - 1, $defenderPos->getY() + $ndy);
            $squares[] = new Position($defenderPos->getX() + 1, $defenderPos->getY() + $ndy);
        } elseif ($ndy === 0) {
            // Horizontal push - diagonals are up and down
            $squares[] = new Position($defenderPos->getX() + $ndx, $defenderPos->getY() - 1);
            $squares[] = new Position($defenderPos->getX() + $ndx, $defenderPos->getY() + 1);
        } else {
            // Diagonal push - alternatives are the two adjacent directions
            $squares[] = new Position($defenderPos->getX() + $ndx, $defenderPos->getY());
            $squares[] = new Position($defenderPos->getX(), $defenderPos->getY() + $ndy);
        }

        return $squares;
    }

    /**
     * Find the best dump-off target: closest friendly teammate adjacent to the ball carrier.
     */
    private function findDumpOffTarget(GameState $state, MatchPlayerDTO $carrier): ?Position
    {
        $carrierPos = $carrier->getPosition();
        if ($carrierPos === null) {
            return null;
        }

        $best = null;
        foreach ($state->getPlayersOnPitch($carrier->getTeamSide()) as $teammate) {
            if ($teammate->getId() === $carrier->getId()) {
                continue;
            }
            if ($teammate->getState() !== PlayerState::STANDING) {
                continue;
            }
            $tPos = $teammate->getPosition();
            if ($tPos === null) {
                continue;
            }
            // Must be within quick pass range (distance <= 3)
            if ($carrierPos->distanceTo($tPos) <= 3) {
                $best = $tPos;
                break;
            }
        }

        return $best;
    }

    /** @return list<BlockDiceFace> */
    private function rollBlockDice(int $count): array
    {
        $faces = [];
        for ($i = 0; $i < $count; $i++) {
            $faces[] = $this->rollBlockDie();
        }
        return $faces;
    }

    private function rollBlockDie(): BlockDiceFace
    {
        $roll = $this->dice->rollD6();
        return match ($roll) {
            1 => BlockDiceFace::ATTACKER_DOWN,
            2 => BlockDiceFace::BOTH_DOWN,
            3, 4 => BlockDiceFace::PUSHED,
            5 => BlockDiceFace::DEFENDER_STUMBLES,
            default => BlockDiceFace::DEFENDER_DOWN,
        };
    }

    /**
     * Auto-choose the best block die face.
     *
     * @param list<BlockDiceFace> $faces
     */
    private function autoChooseBlockDie(
        array $faces,
        bool $attackerChooses,
        MatchPlayerDTO $attacker,
        MatchPlayerDTO $defender,
    ): BlockDiceFace {
        $scored = [];
        foreach ($faces as $face) {
            $scored[] = [$face, $this->scoreBlockFace($face, $attacker, $defender)];
        }

        // Attacker wants highest score, defender wants lowest
        usort(
            $scored,
            fn(array $a, array $b) => $attackerChooses
            ? $b[1] <=> $a[1]
            : $a[1] <=> $b[1],
        );

        return $scored[0][0];
    }

    /**
     * Použije některý z dvojice Wrestle na výsledek Both Down? (`rules_bb2016.txt`
     * ř. 8671-8678: "may use", ř. 1820: "Skill use is not mandatory".)
     * Volba je automatická pro obě strany (dialog na ni web nemá), vzor C++
     * `block_handler.cpp:931-947`:
     * - útočník bez Blocku by padl sám (turnover) ⇒ Wrestle vždy; s Blockem jen když je co
     *   získat (soupeř má Block, takže by se nestalo nic, nebo drží míč) a sám míč nenese
     *   -- položený nosič týmu na tahu je turnover (ř. 8677-8678);
     * - obránce bez Blocku padá tak jako tak ⇒ Wrestle (padne i útočník a nehází se na
     *   brnění); s Blockem by zůstal stát a útočník bez Blocku padl ⇒ Wrestle jen na
     *   nosiče míče, kterého Block drží na nohou (jeho položení je turnover).
     */
    private function wrestleUsed(GameState $state, MatchPlayerDTO $attacker, MatchPlayerDTO $defender): bool
    {
        $carrierId = $state->getBall()->getCarrierId();
        $attHasBall = $carrierId === $attacker->getId();
        $defHasBall = $carrierId === $defender->getId();
        $attHasBlock = $attacker->hasSkill(SkillName::Block);
        $defHasBlock = $defender->hasSkill(SkillName::Block);

        $attWants = $attacker->hasSkill(SkillName::Wrestle)
            && (!$attHasBlock || (!$attHasBall && ($defHasBlock || $defHasBall)));
        $defWants = $defender->hasSkill(SkillName::Wrestle)
            && (!$defHasBlock || ($attHasBall && $attHasBlock));

        return $attWants || $defWants;
    }

    private function scoreBlockFace(BlockDiceFace $face, MatchPlayerDTO $attacker, MatchPlayerDTO $defender): int
    {
        $attackerHasBlock = $attacker->hasSkill(SkillName::Block);
        $attackerHasWrestle = $attacker->hasSkill(SkillName::Wrestle);
        $defenderHasBlock = $defender->hasSkill(SkillName::Block);
        $defenderHasDodge = $defender->hasSkill(SkillName::Dodge);
        $attackerHasTackle = $attacker->hasSkill(SkillName::Tackle);

        return match ($face) {
            BlockDiceFace::DEFENDER_DOWN, BlockDiceFace::POW => 100,
            BlockDiceFace::DEFENDER_STUMBLES => ($defenderHasDodge && !$attackerHasTackle) ? 30 : 80,
            BlockDiceFace::PUSHED => 30,
            BlockDiceFace::BOTH_DOWN => $attackerHasBlock
                ? ($defenderHasBlock ? 20 : 90)
                : ($attackerHasWrestle ? 25 : -50),
            BlockDiceFace::ATTACKER_DOWN => -100,
        };
    }
}
