<?php

declare(strict_types=1);

namespace App\Engine\Action;

use App\DTO\ActionResult;
use App\DTO\BallState;
use App\DTO\GameEvent;
use App\DTO\GameState;
use App\DTO\PendingRerollDTO;
use App\Engine\BallResolver;
use App\Engine\DiceRollerInterface;
use App\Enum\SkillName;
use App\ValueObject\Position;

final class RerollHandler
{
    public function __construct(
        private readonly DiceRollerInterface $dice,
        private readonly BallResolver $ballResolver,
        private readonly MoveHandler $moveHandler,
    ) {}

    /**
     * `$vyhodnotit` (P289): co znamená přehozený hod `int`, nebo konečný neúspěch `null` (platí původní hod).
     * Bez něj úhyb, GFI, zvednutí a Leap; velký hráč si dodá vlastní (kontrola před akcí, pak akce).
     *
     * @param array<string, mixed> $params
     * @param (\Closure(GameState, ?int, list<GameEvent>): ActionResult)|null $vyhodnotit
     */
    public function resolve(GameState $state, array $params, ?\Closure $vyhodnotit = null): ActionResult
    {
        $pending = $state->getPendingReroll();
        if ($pending === null) {
            throw new \InvalidArgumentException('No pending reroll to resolve');
        }

        $choice = (string) ($params['choice'] ?? 'decline');

        // ⛔ OPRAVA 18.09.2026 (review): volba klienta se musi kryt s nabidkou.
        //   Driv sel 'pro' projit i bez skillu, podruhe v tomtez kole a po
        //   neuspesnem hodu Pro (`rules_bb2016.txt` r. 8381, 8385-8386, 926);
        //   'team_reroll' i bez nabidnuteho tymoveho prehozu. Stejne jako
        //   `BlockHandler::resolveBlockReroll`.
        if ($choice === 'pro' && !$pending->isProAvailable()) {
            throw new \InvalidArgumentException('Pro not available');
        }
        if ($choice === 'team_reroll' && !$pending->isTeamRerollAvailable()) {
            throw new \InvalidArgumentException('Team reroll not available');
        }

        $state = $state->withPendingReroll(null);
        $vyhodnotit ??= $this->vychoziVyhodnoceni($pending);

        return match ($choice) {
            'pro' => $this->resolveProReroll($state, $pending, $vyhodnotit),
            'team_reroll' => $this->resolveTeamReroll($state, $pending, $vyhodnotit),
            'decline' => $vyhodnotit($state, null, []),
            default => throw new \InvalidArgumentException("Invalid reroll choice: {$choice}"),
        };
    }

    /**
     * Auto-resolve: always accept the best available reroll.
     */
    public function autoResolve(GameState $state): ActionResult
    {
        $pending = $state->getPendingReroll();
        if ($pending === null) {
            throw new \InvalidArgumentException('No pending reroll');
        }

        $state = $state->withPendingReroll(null);
        $vyhodnotit = $this->vychoziVyhodnoceni($pending);

        // AI: try Pro first, then Team Reroll
        if ($pending->isProAvailable()) {
            return $this->resolveProReroll($state, $pending, $vyhodnotit);
        }
        if ($pending->isTeamRerollAvailable()) {
            return $this->resolveTeamReroll($state, $pending, $vyhodnotit);
        }

        return $vyhodnotit($state, null, []);
    }

    /** @return \Closure(GameState, ?int, list<GameEvent>): ActionResult */
    private function vychoziVyhodnoceni(PendingRerollDTO $pending): \Closure
    {
        return function (GameState $state, ?int $hod, array $events) use ($pending): ActionResult {
            /** @var list<GameEvent> $events */
            if ($hod === null) {
                return $this->applyFailure($state, $pending, $events);
            }
            $success = $hod >= $pending->getTarget();
            $events[] = $this->makeRollEvent($pending, $hod, $success);

            return $success
                ? $this->applySuccess($state, $pending, $events)
                : $this->applyFailure($state, $pending, $events);
        };
    }

    /** @param \Closure(GameState, ?int, list<GameEvent>): ActionResult $vyhodnotit */
    private function resolveProReroll(GameState $state, PendingRerollDTO $pending, \Closure $vyhodnotit): ActionResult
    {
        $player = $state->getPlayer($pending->getPlayerId());
        if ($player === null) {
            return $vyhodnotit($state, null, []);
        }

        $player = $player->withProUsedThisTurn(true);
        $state = $state->withPlayer($player);

        $proRoll = $this->dice->rollD6();
        $events = [];

        if ($proRoll < 4) {
            $events[] = GameEvent::proReroll($pending->getPlayerId(), $proRoll, false, null);

            // r. 8385-8387: puvodni vysledek plati; tymovy prehoz uz jen na hod Pro
            if ($pending->isTeamRerollAvailable()) {
                return ActionResult::success($state->withPendingReroll($pending->withProFailed()), $events);
            }

            return $vyhodnotit($state, null, $events);
        }

        $events[] = GameEvent::proReroll($pending->getPlayerId(), $proRoll, true, null);

        // ⛔ OPRAVA 18.09.2026: po prehozu kostky Pro se uz tymovy prehoz
        //   NENABIZI -- r. 926: "you may never re-roll a single dice roll more
        //   than once." Driv se nabidl a kostka sla prehodit podruhe.
        return $vyhodnotit($state, $this->dice->rollD6(), $events);
    }

    /** @param \Closure(GameState, ?int, list<GameEvent>): ActionResult $vyhodnotit */
    private function resolveTeamReroll(GameState $state, PendingRerollDTO $pending, \Closure $vyhodnotit): ActionResult
    {
        $side = $state->getActiveTeam();
        $state = $state->withTeamState($side, $state->getTeamState($side)->withRerollUsed());

        $player = $state->getPlayer($pending->getPlayerId());
        if ($player === null) {
            return $vyhodnotit($state, null, []);
        }

        $events = [];

        // Loner check
        if ($player->hasSkill(SkillName::Loner)) {
            $lonerRoll = $this->dice->rollD6();
            if ($lonerRoll < 4) {
                $events[] = GameEvent::lonerCheck($pending->getPlayerId(), $lonerRoll, false);
                return $vyhodnotit($state, null, $events);
            }
            $events[] = GameEvent::lonerCheck($pending->getPlayerId(), $lonerRoll, true);
        }

        $events[] = GameEvent::rerollUsed($pending->getPlayerId(), 'Team Reroll');

        // ⛔ OPRAVA 18.09.2026: po neuspesnem Pro prehazuje tymovy prehoz HOD
        //   PRO, ne puvodni kostku (`rules_bb2016.txt` r. 8385-8387).
        if ($pending->isProFailed()) {
            $proRoll = $this->dice->rollD6();
            $events[] = GameEvent::proReroll($pending->getPlayerId(), $proRoll, $proRoll >= 4, null);
            if ($proRoll < 4) {
                return $vyhodnotit($state, null, $events);
            }
        }

        return $vyhodnotit($state, $this->dice->rollD6(), $events);
    }

    /**
     * @param list<GameEvent> $events
     */
    private function applySuccess(GameState $state, PendingRerollDTO $pending, array $events): ActionResult
    {
        $player = $state->getPlayer($pending->getPlayerId());
        if ($player === null) {
            return ActionResult::success($state, $events);
        }

        $target = new Position($pending->getTargetX(), $pending->getTargetY());

        // P289: povedený skok (nebo jeho GFI) -- zbylé GFI skoku a dopad
        if ($pending->getRollType() === 'leap' || $pending->getRollType() === 'leap_gfi') {
            return $this->moveHandler->dokoncitSkokPoPrehozu($state, $player->getId(), $target, $pending->getGfiRemaining(), $events);
        }

        if ($pending->getRollType() === 'pickup') {
            $pos = $player->getPosition();
            if ($pos !== null) {
                $state = $state->withBall(BallState::carried($pos, $player->getId()));
            }
            return ActionResult::success($state, $events);
        }

        // Dodge or GFI: move player to target square
        // OPRAVENO 09.10.2026 (review P186, H1) -- hráč se tu přesune a `hasMoved` se
        //   schválně nenastavuje (člověk smí táhnout dál). Zákaz bloku ale stál právě na
        //   `hasMoved`, takže po přehozeném úhybu / GFI šel BLOCK a týmový blitz zůstal
        //   nepoužitý. Pravidla ř. 675: "you may not move when you take a Block Action".
        $player = $player
            ->withMovedThisTurn(true)
            ->withPosition($target)
            ->withMovementRemaining($player->getMovementRemaining() - 1);
        $state = $state->withPlayer($player);

        // Move ball with carrier
        if ($state->getBall()->getCarrierId() === $player->getId()) {
            $state = $state->withBall(BallState::carried($target, $player->getId()));
        }

        // Check for ball pickup at target
        $ball = $state->getBall();
        if (!$ball->isHeld() && $ball->getPosition() !== null && $ball->getPosition()->equals($target)) {
            $freshPlayer = $state->getPlayer($player->getId()) ?? $player;
            $pickupResult = $this->ballResolver->resolvePickup($state, $freshPlayer, false);
            $events = array_merge($events, $pickupResult['events']);
            $state = $pickupResult['state'];
            if (!$pickupResult['success']) {
                $events[] = GameEvent::turnover('Failed pickup');
                $failPlayer = ($state->getPlayer($pending->getPlayerId()) ?? $freshPlayer)
                    ->withHasMoved(true)
                    ->withHasActed(true);
                $state = $state->withPlayer($failPlayer);
                return ActionResult::turnover($state->withTurnoverPending(true), $events);
            }
        }

        return ActionResult::success($state, $events);
    }

    /**
     * @param list<GameEvent> $events
     */
    private function applyFailure(GameState $state, PendingRerollDTO $pending, array $events = []): ActionResult
    {
        $player = $state->getPlayer($pending->getPlayerId());
        if ($player === null) {
            return ActionResult::turnover($state->withTurnoverPending(true), $events);
        }

        if ($pending->getRollType() === 'pickup') {
            $pos = $player->getPosition();
            if ($pos !== null) {
                $bounceResult = $this->ballResolver->resolveBounce($state, $pos);
                $events = array_merge($events, $bounceResult['events']);
                $state = $bounceResult['state'];
            }
            $events[] = GameEvent::turnover('Failed pickup');
            $player = ($state->getPlayer($pending->getPlayerId()) ?? $player)
                ->withHasMoved(true)
                ->withHasActed(true);
            $state = $state->withPlayer($player);
            return ActionResult::turnover($state->withTurnoverPending(true), $events);
        }

        if ($pending->getRollType() === 'leap' || $pending->getRollType() === 'leap_gfi') {
            return $this->moveHandler->skokSelhal($state, $player, new Position($pending->getTargetX(), $pending->getTargetY()), $events);
        }

        // Dodge or GFI: player falls
        // OPRAVENO 08.10.2026 (audit parity, nalezy 4a a 4b) -- tady byla vlastni kopie padu:
        //   u uhybu nechavala hrace na VYCHOZIM poli (jen GFI slo na cilove) a po padu se
        //   NEHAZELO na brneni ani zraneni. Pravidla r. 497-499 (uhyb): "Knocked Down in the
        //   square he was dodging to and a roll must be made to see if he was injured";
        //   r. 1702-1703 (GFI): "Knocked Down in the square that they moved to. Roll to see
        //   if he was injured." Pad je ted jedna funkce spolecna s `MoveHandler`.
        $events[] = GameEvent::playerFell($pending->getPlayerId());
        $events[] = GameEvent::turnover(
            $pending->getRollType() === 'gfi' ? 'Failed Going For It' : 'Failed dodge',
        );

        [$state, $events] = $this->moveHandler->knockDownAt(
            $state,
            $player,
            new Position($pending->getTargetX(), $pending->getTargetY()),
            $events,
        );

        return ActionResult::turnover($state->withTurnoverPending(true), $events);
    }

    private function makeRollEvent(PendingRerollDTO $pending, int $roll, bool $success): GameEvent
    {
        return match ($pending->getRollType()) {
            'dodge' => GameEvent::dodgeAttempt($pending->getPlayerId(), $pending->getTarget(), $roll, $success),
            'gfi', 'leap_gfi' => GameEvent::gfiAttempt($pending->getPlayerId(), $roll, $success),
            'leap' => GameEvent::leap($pending->getPlayerId(), $pending->getTarget(), $roll, $success),
            default => GameEvent::ballPickup($pending->getPlayerId(), $pending->getTarget(), $roll, $success),
        };
    }
}
