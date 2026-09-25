<?php
declare(strict_types=1);

namespace App\Engine\Action;

use App\DTO\ActionResult;
use App\DTO\BallState;
use App\DTO\GameEvent;
use App\DTO\GameState;
use App\DTO\MatchPlayerDTO;
use App\Enum\BlockDiceFace;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\ValueObject\Position;
use App\Engine\BallResolver;
use App\Engine\DiceRollerInterface;
use App\Engine\InjuryResolver;
use App\Engine\ScatterCalculator;
use App\Engine\StrengthCalculator;

final class BallAndChainHandler implements ActionHandlerInterface
{
    private readonly StrengthCalculator $strCalc;

    public function __construct(
        private readonly DiceRollerInterface $dice,
        private readonly InjuryResolver $injuryResolver,
        private readonly BallResolver $ballResolver,
        private readonly ScatterCalculator $scatterCalc,
        private readonly BlockHandler $blockHandler,
    ) {
        $this->strCalc = new StrengthCalculator();
    }

    /**
     * @param array<string, mixed> $params {playerId}
     */
    public function resolve(GameState $state, array $params): ActionResult
    {
        $playerId = (int) $params['playerId'];
        $player = $state->getPlayer($playerId);

        if ($player === null) {
            throw new \InvalidArgumentException('Player not found');
        }
        if (!$player->hasSkill(SkillName::BallAndChain)) {
            throw new \InvalidArgumentException('Player must have Ball & Chain skill');
        }

        $events = [];
        $srazenPriBloku = false;
        $ma = $player->getStats()->getMovement();

        for ($step = 0; $step < $ma; $step++) {
            $currentPos = $player->getPosition();
            if ($currentPos === null) {
                break; // Player was KO'd off pitch
            }

            // Sablona vhazovani natocena podle volby kouce + D6 (`rules_bb2016.txt`
            //   r. 7829-7833); volba viz `zvolNatoceni`.
            $smer = $this->zvolNatoceni($state, $player, $currentPos);
            $direction = $this->dice->rollD6();
            [$dx, $dy] = $this->scatterCalc->templateOffset($smer, $direction);
            $newPos = new Position($currentPos->getX() + $dx, $currentPos->getY() + $dy);

            // Mimo hriste: dav ho zbije jako vytlaceneho (r. 7835-7837) -- hod na
            //   zraneni, ne automaticke KO; turnover to neni (r. 369-370).
            if (!$newPos->isOnPitch()) {
                $events[] = GameEvent::ballAndChainMove($playerId, (string) $currentPos, 'off-pitch', $direction);
                $events[] = GameEvent::crowdSurf($playerId);
                [$state, $events] = $this->zranitBnc($state, $player, $events, true);
                $player = $state->getPlayer($playerId);
                break;
            }

            // Check for occupant
            $occupant = $state->getPlayerAtPosition($newPos);
            if ($occupant !== null) {
                // Auto-block the occupant
                $events[] = GameEvent::ballAndChainMove($playerId, (string) $currentPos, (string) $newPos, $direction);
                $events[] = GameEvent::ballAndChainBlock($playerId, $occupant->getId());

                // B&C zustava na svem poli a blokuje odtud (r. 7840-7842); lezici
                //   nebo omraceny v ceste se misto bloku odtlaci a hazi na brneni (r. 7843-7845).
                if ($occupant->getState()->canAct()) {
                    [$state, $events, $player] = $this->resolveAutoBlock($state, $player, $occupant, $events);
                } else {
                    [$state, $events] = $this->odtlacitLeziciho($state, $player, $occupant, $events);
                }

                // If player was knocked down during auto-block, stop movement
                $player = $state->getPlayer($playerId);
                if ($player === null || $player->getState() !== PlayerState::STANDING) {
                    $srazenPriBloku = true;
                    break;
                }

                // Povinny follow-up (r. 7845-7847): uvolnilo-li se pole, B&C postoupi.
                if ($state->getPlayerAtPosition($newPos) === null) {
                    $player = $player->withPosition($newPos);
                    $state = $state->withPlayer($player);
                }
            } else {
                // Move to empty square
                $events[] = GameEvent::ballAndChainMove($playerId, (string) $currentPos, (string) $newPos, $direction);
                $player = $player->withPosition($newPos);
                $state = $state->withPlayer($player);

                // Ball & Chain player cannot pick up the ball — ball bounces
                $ball = $state->getBall();
                if (!$ball->isHeld() && $ball->getPosition() !== null && $ball->getPosition()->equals($newPos)) {
                    $bounceResult = $this->ballResolver->resolveBounce($state, $newPos);
                    $events = array_merge($events, $bounceResult['events']);
                    $state = $bounceResult['state'];
                }
            }
        }

        // Mark as acted
        $player = $state->getPlayer($playerId);
        if ($player !== null) {
            $state = $state->withPlayer($player->withHasActed(true)->withHasMoved(true));
        }

        // Srazeny pri bloku = hrac tymu na tahu Knocked Down = turnover (r. 368);
        //   dav turnover neni (r. 369-370).
        return $srazenPriBloku
            ? ActionResult::turnover($state, $events)
            : ActionResult::success($state, $events);
    }

    /**
     * Natoceni sablony pro dalsi krok -- rozhodnuti uzivatele 25.09.: k nejblizsimu
     * stojicimu SOUPERI a vyhnout se VSEM NASIM (i lezicim). Kazde natoceni ma tri
     * mozna pole (po 1/3); skore = stojici soupere - 2 x nasi - dav.
     * Pri shode rozhodne blizkost rovneho pole k nejblizsimu stojicimu souperi.
     *
     * @return array{int, int}
     */
    private function zvolNatoceni(GameState $state, MatchPlayerDTO $bnc, Position $odkud): array
    {
        $souperi = array_values(array_filter(
            $state->getPlayersOnPitch($bnc->getTeamSide()->opponent()),
            static fn(MatchPlayerDTO $p) => $p->getState() === PlayerState::STANDING,
        ));

        $nejlepsi = [1, 0];
        $nejlepsiSkore = null;
        foreach ([[1, 0], [-1, 0], [0, 1], [0, -1]] as $smer) {
            $skore = 0.0;
            foreach ([1, 3, 5] as $d6) {
                [$dx, $dy] = $this->scatterCalc->templateOffset($smer, $d6);
                $pole = new Position($odkud->getX() + $dx, $odkud->getY() + $dy);
                if (!$pole->isOnPitch()) {
                    $skore -= 1.0;
                    continue;
                }
                $kdo = $state->getPlayerAtPosition($pole);
                if ($kdo !== null && $kdo->getTeamSide() === $bnc->getTeamSide()) {
                    $skore -= 2.0; // i lezici a omraceny nas (uzivatel 25.09.)
                } elseif ($kdo !== null && $kdo->getState() === PlayerState::STANDING) {
                    $skore += 1.0;
                }
            }
            $rovne = new Position($odkud->getX() + $smer[0], $odkud->getY() + $smer[1]);
            $vzdalenost = PHP_INT_MAX;
            foreach ($souperi as $souper) {
                $vzdalenost = min($vzdalenost, $rovne->distanceTo($souper->requirePosition()));
            }
            // Blizkost je jen rozhodovani shody -- mensi nez rozdil jednoho hrace.
            $skore -= $vzdalenost === PHP_INT_MAX ? 0.0 : $vzdalenost / 100.0;

            if ($nejlepsiSkore === null || $skore > $nejlepsiSkore) {
                $nejlepsiSkore = $skore;
                $nejlepsi = $smer;
            }
        }

        return $nejlepsi;
    }

    /**
     * Sraženy nebo vyleteny Ball & Chain: rovnou hod na ZRANENI, bez brneni;
     * Stunned se pocita jako KO (`rules_bb2016.txt` r. 7848-7850).
     *
     * @param list<GameEvent> $events
     * @return array{0: GameState, 1: list<GameEvent>}
     */
    private function zranitBnc(GameState $state, MatchPlayerDTO $bnc, array $events, bool $dav): array
    {
        $vysledek = $dav
            ? $this->injuryResolver->resolveCrowdSurf($bnc, $this->dice)
            : $this->injuryResolver->resolveInjuryOnly($bnc->withState(PlayerState::PRONE), $this->dice);
        $hrac = $vysledek['player'];
        $events = array_merge($events, $vysledek['events']);

        // Stunned = KO; u davu resolveCrowdSurf z Stunned udela rezervy -- u B&C taky KO.
        if ($hrac->getState() === PlayerState::STUNNED || ($dav && $hrac->getState() === PlayerState::OFF_PITCH)) {
            $hrac = $hrac->withState(PlayerState::KO)->withPosition(null);
        }
        if ($dav) {
            $hrac = $hrac->withPosition(null);
        }

        return [$state->withPlayer($hrac), $events];
    }

    /**
     * Resolve automatic 1-die block for Ball & Chain.
     *
     * @param list<GameEvent> $events
     * @return array{0: GameState, 1: list<GameEvent>, 2: MatchPlayerDTO}
     */
    private function resolveAutoBlock(
        GameState $state,
        MatchPlayerDTO $bncPlayer,
        MatchPlayerDTO $target,
        array $events,
    ): array {
        $bncPos = $bncPlayer->getPosition();
        $targetPos = $target->getPosition();

        if ($bncPos === null || $targetPos === null) {
            return [$state, $events, $bncPlayer];
        }

        // Blok podle beznych pravidel (r. 7840-7842): kostky podle sily vc. asistenci.
        $attStr = $this->strCalc->calculateEffectiveStrength($state, $bncPlayer, $targetPos);
        $defStr = $this->strCalc->calculateEffectiveStrength($state, $target, $bncPos);
        $info = $this->strCalc->getBlockDiceInfo($attStr, $defStr);
        $faces = [];
        for ($i = 0; $i < $info['count']; $i++) {
            $faces[] = match ($this->dice->rollD6()) {
                1 => BlockDiceFace::ATTACKER_DOWN,
                2 => BlockDiceFace::BOTH_DOWN,
                3, 4 => BlockDiceFace::PUSHED,
                5 => BlockDiceFace::DEFENDER_STUMBLES,
                default => BlockDiceFace::DEFENDER_DOWN,
            };
        }
        assert($faces !== []); // getBlockDiceInfo vraci vzdy 1-3 kostky
        $face = $this->vyberStranu($faces, $target->getTeamSide() === $bncPlayer->getTeamSide(), $info['attackerChooses']);

        $events[] = GameEvent::blockAttempt(
            $bncPlayer->getId(), $target->getId(), $info['count'], $info['attackerChooses'],
            array_map(static fn(BlockDiceFace $f) => $f->value, $faces), $face->value,
        );

        // Simplified block resolution for auto-block
        switch ($face) {
            case BlockDiceFace::ATTACKER_DOWN:
                // B&C player knocked down
                $events[] = GameEvent::playerFell($bncPlayer->getId());
                [$state, $events] = $this->zranitBnc($state, $bncPlayer, $events, false);
                $bncPlayer = $state->requirePlayer($bncPlayer->getId());
                break;

            case BlockDiceFace::BOTH_DOWN:
                // Both go down unless they have Block
                if (!$bncPlayer->hasSkill(SkillName::Block)) {
                    $events[] = GameEvent::playerFell($bncPlayer->getId());
                    [$state, $events] = $this->zranitBnc($state, $bncPlayer, $events, false);
                    $bncPlayer = $state->requirePlayer($bncPlayer->getId());
                }
                if (!$target->hasSkill(SkillName::Block)) {
                    $events[] = GameEvent::playerFell($target->getId());
                    $target = $target->withState(PlayerState::PRONE);
                    $state = $state->withPlayer($target);
                    $injResult = $this->injuryResolver->resolve($target, $this->dice);
                    $target = $injResult['player'];
                    $state = $state->withPlayer($target);
                    $events = array_merge($events, $injResult['events']);
                    [$state, $events] = $this->ballResolver->handleBallOnPlayerDown($state, $target, $events);
                }
                break;

            case BlockDiceFace::PUSHED:
                // Push target away (simple: find first empty adjacent or crowd surf)
                [$state, $events] = $this->resolvePush($state, $bncPlayer, $target, $events);
                break;

            case BlockDiceFace::DEFENDER_STUMBLES:
                // Push + knockdown (unless Dodge without Tackle)
                $knockdown = !$target->hasSkill(SkillName::Dodge) || $bncPlayer->hasSkill(SkillName::Tackle);
                [$state, $events] = $this->resolvePush($state, $bncPlayer, $target, $events);
                if ($knockdown) {
                    $target = $state->getPlayer($target->getId());
                    if ($target !== null && $target->getPosition() !== null) {
                        $events[] = GameEvent::playerFell($target->getId());
                        $target = $target->withState(PlayerState::PRONE);
                        $state = $state->withPlayer($target);
                        $injResult = $this->injuryResolver->resolve($target, $this->dice);
                        $target = $injResult['player'];
                        $state = $state->withPlayer($target);
                        $events = array_merge($events, $injResult['events']);
                        [$state, $events] = $this->ballResolver->handleBallOnPlayerDown($state, $target, $events);
                    }
                }
                break;

            case BlockDiceFace::DEFENDER_DOWN:
            case BlockDiceFace::POW:
                // Push + knockdown
                [$state, $events] = $this->resolvePush($state, $bncPlayer, $target, $events);
                $target = $state->getPlayer($target->getId());
                if ($target !== null && $target->getPosition() !== null) {
                    $events[] = GameEvent::playerFell($target->getId());
                    $target = $target->withState(PlayerState::PRONE);
                    $state = $state->withPlayer($target);
                    $injResult = $this->injuryResolver->resolve($target, $this->dice);
                    $target = $injResult['player'];
                    $state = $state->withPlayer($target);
                    $events = array_merge($events, $injResult['events']);
                    [$state, $events] = $this->ballResolver->handleBallOnPlayerDown($state, $target, $events);
                }
                break;
        }

        return [$state, $events, $bncPlayer];
    }

    /**
     * Volba strany kostky. Proti VLASTNIMU hraci voli nas kouc za obe strany a bere,
     * co mu nejmene ublizi (rozhodnuti uzivatele 25.09.); proti souperi voli silnejsi
     * -- nejlepsi pro B&C, nebo nejhorsi, kdyz voli souper.
     *
     * @param non-empty-list<BlockDiceFace> $faces
     */
    private function vyberStranu(array $faces, bool $vlastni, bool $attackerChooses): BlockDiceFace
    {
        $poradi = $vlastni
            ? [BlockDiceFace::PUSHED, BlockDiceFace::DEFENDER_STUMBLES, BlockDiceFace::BOTH_DOWN, BlockDiceFace::ATTACKER_DOWN, BlockDiceFace::DEFENDER_DOWN, BlockDiceFace::POW]
            : [BlockDiceFace::POW, BlockDiceFace::DEFENDER_DOWN, BlockDiceFace::DEFENDER_STUMBLES, BlockDiceFace::PUSHED, BlockDiceFace::BOTH_DOWN, BlockDiceFace::ATTACKER_DOWN];
        if (!$vlastni && !$attackerChooses) {
            $poradi = array_reverse($poradi);
        }
        foreach ($poradi as $f) {
            if (in_array($f, $faces, true)) {
                return $f;
            }
        }

        return $faces[0];
    }

    /**
     * Lezici nebo omraceny v ceste B&C: misto bloku odtlaceni a hod na brneni
     * (`rules_bb2016.txt` r. 7843-7845). Omraceny zustava omraceny.
     *
     * @param list<GameEvent> $events
     * @return array{0: GameState, 1: list<GameEvent>}
     */
    private function odtlacitLeziciho(GameState $state, MatchPlayerDTO $bnc, MatchPlayerDTO $obet, array $events): array
    {
        [$state, $events] = $this->resolvePush($state, $bnc, $obet, $events);
        $obet = $state->requirePlayer($obet->getId());
        if ($obet->getPosition() === null) {
            return [$state, $events]; // vytlacen do davu -- tam uz se hazelo
        }
        $injResult = $this->injuryResolver->resolve($obet, $this->dice);
        $events = array_merge($events, $injResult['events']);

        return [$state->withPlayer($injResult['player']), $events];
    }

    /**
     * Odtlaceni od B&C = bezne odtlaceni (`rules_bb2016.txt` r. 7840-7847):
     * retez, dav, Grab i Side Step resi `BlockHandler::resolvePushback`.
     * Kdo drzi pole (zakoreneny; stojici Stand Firm soupere), se neodtlaci;
     * lezici Stand Firm nepouzije (r. 1824-1825).
     *
     * @param list<GameEvent> $events
     * @return array{0: GameState, 1: list<GameEvent>}
     */
    private function resolvePush(
        GameState $state,
        MatchPlayerDTO $pusher,
        MatchPlayerDTO $target,
        array $events,
    ): array {
        $targetPos = $target->getPosition();
        if ($pusher->getPosition() === null || $targetPos === null || $target->holdsGround($pusher->getTeamSide())) {
            return [$state, $events];
        }

        return $this->blockHandler->resolvePushback($state, $pusher, $target, $targetPos, $events);
    }
}
