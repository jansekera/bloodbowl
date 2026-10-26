<?php

declare(strict_types=1);

namespace App\Engine\Action;

use App\DTO\ActionResult;
use App\DTO\BallState;
use App\DTO\GameEvent;
use App\DTO\GameState;
use App\DTO\MatchPlayerDTO;
use App\DTO\PendingGrabDTO;
use App\DTO\PendingStandFirmDTO;
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

        return $this->kroky($state, $playerId, 0, [], null);
    }

    /**
     * P224: odpověď kouče na otázku o Grabu uprostřed pohybu B&C -- dokončí rozpracovaný blok
     * (odtlačení podle odpovědi) a pokračuje dalšími kroky. `$volba`: zvolené pole, nebo false.
     */
    public function resumeGrab(GameState $state, PendingGrabDTO $pending, Position|false $volba): ActionResult
    {
        $face = $pending->getFace();
        if ($face === null) {
            throw new \InvalidArgumentException('Pending Grab question has no block face');
        }

        return $this->kroky(
            $state->withPendingGrab(null),
            $pending->getAttackerId(),
            $pending->getStep(),
            [],
            ['target' => $pending->getDefenderId(), 'face' => $face, 'volba' => $volba, 'standFirm' => $pending->getStandFirm()],
        );
    }

    /**
     * P226: odpověď kouče-člověka na Stand Firm uprostřed pohybu B&C -- dokončí rozpracovaný blok
     * nebo odtlačení ležícího (`face` null; s odpověďmi `$odpovedi`) a pokračuje dalšími kroky. Drží-li hráč pole, B&C zůstane
     * na svém poli a krok ho stál (oficiální FAQ 05/2020; povinný follow-up jen na uvolněné pole).
     *
     * @param array<int, bool> $odpovedi
     */
    public function resumeStandFirm(GameState $state, PendingStandFirmDTO $pending, array $odpovedi): ActionResult
    {
        return $this->kroky(
            $state->withPendingStandFirm(null),
            $pending->getAttackerId(),
            $pending->getStep(),
            [],
            ['target' => $pending->getDefenderId(), 'face' => $pending->getFace(), 'volba' => $pending->getGrab(), 'standFirm' => $odpovedi],
        );
    }

    /**
     * Kroky pohybu B&C od kroku `$odKroku`. `$obnoveno`: pokračování po otázce na Grab nebo na
     * Stand Firm (P226) -- krok už je hozený a blok vybraný, zbývá odtlačení.
     *
     * @param list<GameEvent> $events
     * @param array{target: int, face: ?BlockDiceFace, volba: Position|false|null, standFirm: array<int, bool>}|null $obnoveno
     */
    private function kroky(GameState $state, int $playerId, int $odKroku, array $events, ?array $obnoveno): ActionResult
    {
        $player = $state->requirePlayer($playerId);
        $srazenPriBloku = false;
        $ma = $player->getStats()->getMovement();

        for ($step = $odKroku; $step < $ma; $step++) {
            $currentPos = $player->getPosition();
            if ($currentPos === null) {
                break; // Player was KO'd off pitch
            }

            $direction = 0; // hozený jen při novém kroku; po otázce na Grab se pohyb už neopakuje
            if ($obnoveno !== null) {
                $occupant = $state->requirePlayer($obnoveno['target']);
                $newPos = $occupant->requirePosition();
            } else {
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
            }
            if ($occupant !== null) {
                if ($obnoveno === null) {
                    // Auto-block the occupant
                    $events[] = GameEvent::ballAndChainMove($playerId, (string) $currentPos, (string) $newPos, $direction);
                    $events[] = GameEvent::ballAndChainBlock($playerId, $occupant->getId());
                }

                // B&C zustava na svem poli a blokuje odtud (r. 7840-7842); lezici
                //   nebo omraceny v ceste se misto bloku odtlaci a hazi na brneni (r. 7843-7845).
                if ($occupant->getState()->canAct()) {
                    [$state, $events, $player] = $this->resolveAutoBlock($state, $player, $occupant, $events, $step, $obnoveno);
                    if ($state->getPendingGrab() !== null || $state->getPendingStandFirm() !== null) {
                        return ActionResult::success($state, $events); // P224/P226: čeká otázka kouči
                    }
                } else {
                    // OPRAVENO 26.10.2026 (review P226, LOW 2) -- odtlačení ležícího běželo bez otázky:
                    //   člověk se Stand Firm v řetězu držel, aniž se ho kdo zeptal (`drziPole` → AI).
                    //   Stand Firm je volba (ř. 8510-8516); ležící sám ho nepoužije (`holdsGround`).
                    //   Do odpovědi se nic nehodí (brnění až po odtlačení), stav je čistý.
                    $obet = $occupant;
                    $odpovedi = $obnoveno['standFirm'] ?? [];
                    [$odtlaceno, $kdo] = $this->blockHandler->sOtazkouNaStandFirm(
                        $odpovedi,
                        fn(): array => $this->odtlacitLeziciho($state, $player, $obet, $events),
                    );
                    if ($kdo !== null) {
                        return ActionResult::success($state->withPendingStandFirm(new PendingStandFirmDTO(
                            $kdo,
                            $playerId,
                            $obet->getId(),
                            null,
                            ...$this->blockHandler->otazkaNaStandFirm($state, $player, $obet, true, $odpovedi, null, false, $kdo),
                            kind: 'bnc',
                            answers: $odpovedi,
                            step: $step,
                        )), $events);
                    }
                    assert($odtlaceno !== null);
                    [$state, $events] = $odtlaceno;
                }
                $obnoveno = null;

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
     * P224: kouč-člověk se před odtlačením ptá na Grab -- vrácený stav pak nese `PendingGrabDTO`
     * a blok se dokončí v `resumeGrab` (`$obnoveno` = už vybraná strana kostky a odpověď).
     * P226: napřed se ptá kouče odtlačovaného hráče (i v řetězu) na Stand Firm -- `PendingStandFirmDTO`,
     * dokončí `resumeStandFirm`. B&C blokuje "following normal blocking rules" (ř. 7829-7831).
     *
     * @param list<GameEvent> $events
     * @param array{target: int, face: ?BlockDiceFace, volba: Position|false|null, standFirm: array<int, bool>}|null $obnoveno
     * @return array{0: GameState, 1: list<GameEvent>, 2: MatchPlayerDTO}
     */
    private function resolveAutoBlock(
        GameState $state,
        MatchPlayerDTO $bncPlayer,
        MatchPlayerDTO $target,
        array $events,
        int $step,
        ?array $obnoveno,
    ): array {
        $bncPos = $bncPlayer->getPosition();
        $targetPos = $target->getPosition();

        if ($bncPos === null || $targetPos === null) {
            return [$state, $events, $bncPlayer];
        }

        $odpovedi = [];
        if ($obnoveno !== null) {
            $face = $obnoveno['face'] ?? throw new \LogicException('Ball & Chain block resumed without a block face');
            $grabVolba = $obnoveno['volba'];
            $odpovedi = $obnoveno['standFirm'];
        } else {
            $grabVolba = null;
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
                $bncPlayer->getId(),
                $target->getId(),
                $info['count'],
                $info['attackerChooses'],
                array_map(static fn(BlockDiceFace $f) => $f->value, $faces),
                $face->value,
            );
        }

        // Odtlačení (Pushed, Stumbles, Down) s otázkami kouči: Stand Firm napřed (bez odtlačení není
        //   co volit), pak Grab (P224). Do odpovědí se nic nehodí ani nepohne.
        $odtlaceno = null;
        if (in_array($face, [BlockDiceFace::PUSHED, BlockDiceFace::DEFENDER_STUMBLES, BlockDiceFace::DEFENDER_DOWN, BlockDiceFace::POW], true)) {
            [$odtlaceno, $kdo] = $this->blockHandler->sOtazkouNaStandFirm($odpovedi, function () use ($state, $bncPlayer, $target, $face, $events, $grabVolba): ?array {
                if ($grabVolba === null && $this->blockHandler->grabQuestionSquares($state, $bncPlayer, $target, $face, false) !== []) {
                    return null;   // čeká otázka na Grab
                }

                return $this->resolvePush($state, $bncPlayer, $target, $events, grabVolba: $grabVolba);
            });
            if ($kdo !== null) {
                return [$state->withPendingStandFirm(new PendingStandFirmDTO(
                    $kdo,
                    $bncPlayer->getId(),
                    $target->getId(),
                    $face,
                    ...$this->blockHandler->otazkaNaStandFirm($state, $bncPlayer, $target, false, $odpovedi, $grabVolba, true, $kdo),
                    kind: 'bnc',
                    answers: $odpovedi,
                    grab: $grabVolba,
                    step: $step,
                )), $events, $bncPlayer];
            }
            if ($odtlaceno === null) {
                return [$state->withPendingGrab(new PendingGrabDTO(
                    'bnc',
                    $bncPlayer->getId(),
                    $target->getId(),
                    $this->blockHandler->grabQuestionSquares($state, $bncPlayer, $target, $face, false),
                    face: $face,
                    step: $step,
                    standFirm: $odpovedi,
                )), $events, $bncPlayer];
            }
        }

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
                assert($odtlaceno !== null);
                [$state, $events] = $odtlaceno;
                break;

            case BlockDiceFace::DEFENDER_STUMBLES:
                // Push + knockdown (unless Dodge without Tackle)
                $knockdown = !$target->hasSkill(SkillName::Dodge) || $bncPlayer->hasSkill(SkillName::Tackle);
                assert($odtlaceno !== null);
                [$state, $events] = $odtlaceno;
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
                assert($odtlaceno !== null);
                [$state, $events] = $odtlaceno;
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
        // OPRAVENO 20.10.2026 (P222, parita s C++) -- i tady platil Grab. Pravidla ř. 7829-7831: Grab
        //   jen "with his blocks"; na ležícího se neblokuje (ř. 7843-7845 "instead of the block").
        [$state, $events] = $this->resolvePush($state, $bnc, $obet, $events, grabNeplati: true);
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
     * retez, dav, Grab i Side Step resi `BlockHandler::resolvePushback`. Grab jen u bloku
     * (r. 7829-7831 "as if a Block Action was being used with his blocks").
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
        bool $grabNeplati = false,
        Position|false|null $grabVolba = null,
    ): array {
        $targetPos = $target->getPosition();
        if ($pusher->getPosition() === null || $targetPos === null) {
            return [$state, $events];
        }
        if ($this->blockHandler->obranceDrziPole($state, $pusher, $target, false, null)) {
            return [$state, $this->blockHandler->udalostStandFirm($pusher, $target, $events)];
        }

        return $this->blockHandler->resolvePushback($state, $pusher, $target, $targetPos, $events, grabNeplati: $grabNeplati, grabVolba: $grabVolba);
    }
}
