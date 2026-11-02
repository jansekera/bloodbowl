<?php

declare(strict_types=1);

namespace App\Engine;

use App\DTO\ActionResult;
use App\DTO\GameEvent;
use App\DTO\GameState;
use App\DTO\MatchPlayerDTO;
use App\DTO\PendingApothecaryDTO;
use App\DTO\PendingDivingCatchDTO;
use App\DTO\BallState;
use App\Engine\Action\BallAndChainHandler;
use App\Engine\Action\BlitzHandler;
use App\Engine\Action\BlockHandler;
use App\Engine\Action\BombThrowHandler;
use App\Engine\Action\EndTurnHandler;
use App\Engine\Action\FoulHandler;
use App\Engine\Action\HandOffHandler;
use App\Engine\Action\HypnoticGazeHandler;
use App\Engine\Action\MoveHandler;
use App\Engine\Action\RerollHandler;
use App\Engine\Action\SetupHandler;
use App\Engine\Action\ThrowTeamMateHandler;
use App\Enum\ActionType;
use App\Enum\CasualtyResult;
use App\Enum\PlayerState;
use App\Enum\TeamSide;
use App\ValueObject\Position;

final class ActionResolver
{
    private bool $interactiveBlocks = false;
    private bool $interactiveRerolls = false;
    private readonly InjuryResolver $injuryResolver;
    private readonly MoveHandler $moveHandler;
    private readonly BlockHandler $blockHandler;
    private readonly BlitzHandler $blitzHandler;
    private readonly FoulHandler $foulHandler;
    private readonly HandOffHandler $handOffHandler;
    private readonly SetupHandler $setupHandler;
    private readonly EndTurnHandler $endTurnHandler;
    private readonly ThrowTeamMateHandler $ttmHandler;
    private readonly BombThrowHandler $bombThrowHandler;
    private readonly HypnoticGazeHandler $hypnoticGazeHandler;
    private readonly BallAndChainHandler $ballAndChainHandler;
    private readonly RerollHandler $rerollHandler;
    private ?PassResolver $passResolver = null;
    private readonly DiceRollerInterface $dice;
    private readonly ReplayDiceRoller $replayDice;
    private readonly TacklezoneCalculator $tzCalc;
    private readonly ScatterCalculator $scatterCalc;
    private readonly BallResolver $ballResolver;
    private readonly GameFlowResolver $gameFlowResolver;
    private readonly BigGuyCheckResolver $bigGuyCheckResolver;

    public function __construct(DiceRollerInterface $dice)
    {
        // 30.10.2026: všechny hody akce jdou přes záznam, aby šla po otázce Diving Catch přehrát (viz `resolve`).
        // OPRAVENO 30.10.2026 (recenze C/1) — konstruktor dřív bral i hotové BallResolver/PassResolver/
        //   KickoffResolver/GameFlowResolver s vlastními kostkami mimo záznam (tak je předával kontejner) ⇒ po
        //   odpovědi se akce přehrála s novými hody. Proto se všechny resolvery staví tady nad `$replayDice`.
        $this->replayDice = new ReplayDiceRoller($dice);
        $dice = $this->replayDice;
        $this->dice = $dice;
        $this->tzCalc = new TacklezoneCalculator();
        $this->scatterCalc = new ScatterCalculator();
        $pathfinder = new Pathfinder($this->tzCalc);
        $strCalc = new StrengthCalculator();
        $injuryResolver = new InjuryResolver();
        $this->injuryResolver = $injuryResolver;
        $this->ballResolver = new BallResolver($dice, $this->tzCalc, $this->scatterCalc);
        $kickoffResolver = new KickoffResolver($dice, $this->scatterCalc, $this->ballResolver, $injuryResolver);
        $this->gameFlowResolver = new GameFlowResolver($dice);
        // ⭐ 11.09.2026 (PHP22): Bloodlust potrebuje hod na zraneni a odraz
        //   mice, takze uz to neni bezzavisla trida.
        $this->bigGuyCheckResolver = new BigGuyCheckResolver($injuryResolver, $this->ballResolver);

        $this->moveHandler = new MoveHandler($dice, $this->tzCalc, $pathfinder, $this->ballResolver, $injuryResolver);
        $this->blockHandler = new BlockHandler($dice, $strCalc, $this->tzCalc, $injuryResolver, $this->ballResolver, $this->moveHandler, $this->getPassResolver());
        $this->blitzHandler = new BlitzHandler($this->moveHandler, $this->blockHandler, $pathfinder);
        $this->foulHandler = new FoulHandler($dice, $injuryResolver, $this->ballResolver);
        $this->handOffHandler = new HandOffHandler($this->ballResolver, $dice);
        $this->ttmHandler = new ThrowTeamMateHandler($dice, $this->tzCalc, $this->scatterCalc, $injuryResolver, $this->ballResolver);
        $this->bombThrowHandler = new BombThrowHandler($dice, $this->tzCalc, $this->scatterCalc, $injuryResolver, $this->ballResolver);
        $this->hypnoticGazeHandler = new HypnoticGazeHandler($dice, $this->tzCalc);
        $this->ballAndChainHandler = new BallAndChainHandler($dice, $injuryResolver, $this->ballResolver, $this->scatterCalc, $this->blockHandler);
        $this->rerollHandler = new RerollHandler($dice, $this->ballResolver, $this->moveHandler);
        $this->setupHandler = new SetupHandler($kickoffResolver);
        $this->endTurnHandler = new EndTurnHandler();
    }

    public function getPassResolver(): PassResolver
    {
        if ($this->passResolver === null) {
            $this->passResolver = new PassResolver($this->dice, $this->tzCalc, $this->scatterCalc, $this->ballResolver);
        }
        return $this->passResolver;
    }

    public function setInteractiveBlocks(bool $interactive): void
    {
        $this->interactiveBlocks = $interactive;
        // P224: kouč-člověk se při odtlačení ptá na Grab; AI (neinteraktivní blok) se neptá
        $this->blockHandler->setAskGrab($interactive);
    }

    /** @var list<TeamSide> P206 (b): strany kouče-člověka — o lékárníkovi se jich ptá po akci */
    private array $interactiveApothecary = [];

    /** @param list<TeamSide> $sides */
    public function setInteractiveApothecary(array $sides): void
    {
        $this->interactiveApothecary = $sides;
    }

    /**
     * P226: strany kouče-člověka -- jeho hráče se Stand Firm se při odtlačení ptá (i v tahu soupeře);
     * hráče AI rozhoduje engine (P225).
     *
     * @param list<TeamSide> $sides
     */
    public function setInteractiveStandFirm(array $sides): void
    {
        $this->blockHandler->setAskStandFirm($sides);
    }

    /**
     * 30.10.2026: strany kouče-člověka -- jeho hráče s Diving Catch se ptá, jestli skočí (i v tahu
     * soupeře); hráče AI rozhoduje engine jako dosud.
     *
     * @param list<TeamSide> $sides
     */
    public function setInteractiveDivingCatch(array $sides): void
    {
        $this->ballResolver->setInteractiveDivingCatch($sides);
    }

    public function setInteractiveRerolls(bool $interactive): void
    {
        $this->interactiveRerolls = $interactive;
        $this->moveHandler->setInteractiveRerolls($interactive);
    }

    public function getBlockHandler(): Action\BlockHandler
    {
        return $this->blockHandler;
    }

    public function getBallResolver(): BallResolver
    {
        return $this->ballResolver;
    }

    public function getGameFlowResolver(): GameFlowResolver
    {
        return $this->gameFlowResolver;
    }

    /**
     * Odecte TYMOVY limit u akce, ktera propadla pred svym provedenim.
     *
     * ⭐ MOVE a BLOCK zadny tymovy limit nemaji -- ty se neodecitaji.
     *   Seznam je uzavreny, stejne jako v C++ `consumeDeclaredTeamAction`
     *   (`engine/src/action_resolver.cpp:27-37`).
     */
    /**
     * ⭐ PHP25: hrac se vedome neaktivuje. Zadne kostky, zadna udalost na
     *    hristi -- jen se pro tenhle tah odepise, aby kouc mohl pokracovat
     *    dalsim hracem misto toho, aby musel ukoncit kolo celemu tymu.
     *
     * ⛔ Nesmi tu vzniknout turnover ani zadny hod: viz vyjimka z kontroly
     *    pred akci v `resolve()`.
     *
     * @param array<string, mixed> $params
     */
    private function resolveStandPat(GameState $state, array $params): ActionResult
    {
        $playerId = (int) ($params['playerId'] ?? 0);
        $player = $state->getPlayer($playerId);
        if ($player === null) {
            throw new \InvalidArgumentException('Player not found');
        }

        $newState = $state->withPlayer(
            $player->withHasActed(true)->withHasMoved(true),
        );

        return ActionResult::success($newState, [
            GameEvent::standPat($playerId, $player->getName()),
        ]);
    }

    private function consumeDeclaredTeamAction(
        GameState $state,
        \App\Enum\TeamSide $side,
        ActionType $type,
    ): GameState {
        $team = $state->getTeamState($side);

        $team = match ($type) {
            ActionType::BLITZ => $team->withBlitzUsed(),
            // ⛔ PHP23: `BOMB_THROW` sdili pass slot -- `BombThrowHandler` ho
            //   odecita pres `withPassUsed()` -- ale TADY chybel, takze Big Guy,
            //   ktery propadne na DEKLAROVANEM bomb throwu, tym o pass slot
            //   nepripravil. Tataz trida jako `PHP15`, jen o akci vedle.
            ActionType::PASS,
            ActionType::BOMB_THROW,
            ActionType::THROW_TEAM_MATE => $team->withPassUsed(),
            ActionType::HAND_OFF => $team->withHandOffUsed(),
            ActionType::FOUL => $team->withFoulUsed(),
            default => $team,
        };

        return $state->withTeamState($side, $team);
    }

    /**
     * @param array<string, mixed> $params
     */
    public function resolve(GameState $state, ActionType $action, array $params): ActionResult
    {
        // 30.10.2026: Diving Catch jako otázka kouči (ř. 8065 "can attempt"). Otázka přijde uprostřed
        //   akce, až po hodech; akce se přeruší (`DivingCatchQuestion`), vrátí se stav PŘED ní s otázkou
        //   a po odpovědi se přehraje se zapsanými hody a všemi dosavadními odpověďmi (`ReplayDiceRoller`).
        if ($action === ActionType::RESOLVE_DIVING_CATCH) {
            $pending = $state->getPendingDivingCatch() ?? throw new \InvalidArgumentException('No pending Diving Catch question');

            return $this->resolveSPrehranim(
                $state->withPendingDivingCatch(null),
                $pending->getAction(),
                $pending->getParams(),
                $pending->getDice(),
                [...$pending->getAnswers(), (bool) ($params['use'] ?? false)],
            );
        }

        return $this->resolveSPrehranim($state, $action, $params, [], []);
    }

    /**
     * @param array<string, mixed> $params
     * @param list<int> $hody
     * @param list<bool> $odpovedi
     */
    private function resolveSPrehranim(GameState $state, ActionType $action, array $params, array $hody, array $odpovedi): ActionResult
    {
        $this->replayDice->begin($hody);
        $this->ballResolver->setDivingCatchAnswers($odpovedi);
        try {
            return $this->resolveJednou($state, $action, $params);
        } catch (DivingCatchQuestion $q) {
            return ActionResult::success($state->withPendingDivingCatch(new PendingDivingCatchDTO(
                $q->playerId,
                $q->landing,
                $action,
                $params,
                $this->replayDice->getLog(),
                $odpovedi,
                $q->otherCandidates,
                $q->attemptingId,
            )), []);
        } finally {
            $this->ballResolver->setDivingCatchAnswers([]);
        }
    }

    /**
     * @param array<string, mixed> $params
     */
    private function resolveJednou(GameState $state, ActionType $action, array $params): ActionResult
    {
        // P206 (14.10.2026): lékárník pro všechna zranění akce (i výkopu přes SetupHandler) na jednom
        //   místě. Rozhodnutí AI = C++ `injury.cpp`: jen na Badly Hurt nebo smrt a jen hráče, kterého
        //   nenahradí stejný hráč z lavičky. ⏰ Volba kouče ve webové hře zatím není -- P206 (b).
        $pouzit = [];
        $this->injuryResolver->setLekarnik(function (MatchPlayerDTO $hrac, CasualtyResult $prvni) use ($state, &$pouzit): bool {
            $strana = $hrac->getTeamSide();
            if (isset($pouzit[$strana->value]) || !$state->getTeamState($strana)->canUseApothecary()) {
                return false;
            }
            if (!in_array($prvni, [CasualtyResult::BADLY_HURT, CasualtyResult::DEAD], true)) {
                return false;   // střední pásmo: lékárník se šetří
            }
            foreach ($state->getPlayers() as $p) {
                if ($p->getId() !== $hrac->getId() && $p->getTeamSide() === $strana
                    && $p->getState() === PlayerState::OFF_PITCH && $p->getPositionalName() === $hrac->getPositionalName()) {
                    return false;   // lavička ho nahradí
                }
            }
            $pouzit[$strana->value] = true;

            return true;
        });
        // P206 (b), 16.10.2026 (uživatel: „lékárník se má ptát hráče na vše“): zranění hráče kouče-člověka
        //   (KO i Casualty) se zapíše jako nabídka; akce doběhne celá a pak se kouč zeptá (PendingApothecary).
        /** @var list<array{playerId: int, kind: string, result: ?string, x: ?int, y: ?int, regeneration: bool, stakes: bool}> $nabidky */
        $nabidky = [];
        $lide = $this->interactiveApothecary;
        $this->injuryResolver->setOdlozitLekarnika($lide === [] ? null : function (MatchPlayerDTO $hrac, string $druh, ?CasualtyResult $vysledek, bool $stakes = false) use ($state, $lide, &$nabidky): bool {
            $strana = $hrac->getTeamSide();
            if (!in_array($strana, $lide, true) || !$state->getTeamState($strana)->canUseApothecary()) {
                return false;
            }
            $pos = $hrac->getPosition();
            $nabidky[] = [
                'playerId' => $hrac->getId(),
                'kind' => $druh,
                'result' => $vysledek?->value,
                'x' => $pos?->getX(),
                'y' => $pos?->getY(),
                'regeneration' => $hrac->hasSkill(\App\Enum\SkillName::Regeneration),
                'stakes' => $stakes,   // review 16.10.: Stakes ruší Regeneraci i po odložené volbě
            ];

            return true;
        });
        try {
            $r = $this->resolveAkce($state, $action, $params);
        } finally {
            $this->injuryResolver->setLekarnik(null);
            $this->injuryResolver->setOdlozitLekarnika(null);
        }
        $st = $r->getNewState();
        foreach (array_keys($pouzit) as $strana) {
            $side = TeamSide::from($strana);
            $st = $st->withTeamState($side, $st->getTeamState($side)->withApothecaryUsed());
        }
        // Review 16.10.: až PO zápisu lékárníka AI — dřív návrat napřed a AI mohla lékárníka použít znovu
        if ($nabidky !== []) {
            $st = $st->withPendingApothecary(new PendingApothecaryDTO($nabidky, $r->isTurnover()));

            return new ActionResult($st, $r->isSuccess(), false, $r->getEvents());
        }

        return new ActionResult($st, $r->isSuccess(), $r->isTurnover(), $r->getEvents());
    }

    /** @param array<string, mixed> $params */
    private function resolveAkce(GameState $state, ActionType $action, array $params): ActionResult
    {
        // ⛔ P81 (30.09.2026): pokracovani blitzu po rane. Je to TAZ akce (Blitz),
        //   takze se neopakuje kontrola pred akci; a propadne, jakmile jedna jiny
        //   hrac nebo tym ukonci kolo -- aktivace musi byt dokoncena, nez zacne
        //   dalsi. Akce, ktere jen dohravaji odlozenou volbu, ho nerusi.
        $continuationId = $state->getBlitzContinuationPlayerId();
        $isContinuation = false;
        if ($continuationId !== null) {
            $actorId = isset($params['playerId']) ? (int) $params['playerId'] : null;
            if ($action === ActionType::MOVE && $actorId === $continuationId) {
                $isContinuation = true;
                $state = $state->withBlitzContinuation(null);
            } elseif ($action === ActionType::END_TURN || ($action->requiresPlayer() && $actorId !== $continuationId)) {
                $blitzer = $state->getPlayer($continuationId);
                if ($blitzer !== null) {
                    $state = $state->withPlayer($blitzer->withHasMoved(true));
                }
                $state = $state->withBlitzContinuation(null);
            }
        }
        // P210 (16.10.2026): ohlaseny blitz (cil se pri ohlaseni nerika, `rules_bb2016.txt` r. 346-349).
        //   Pohyb a rana blitzujiciho jsou TAZ akce -- kontrola pred akci (Wild Animal s +2, Bone-head …)
        //   probehla pri ohlaseni a neopakuje se. Jakmile jedna jiny hrac nebo tym ukonci kolo, aktivace
        //   blitzujiciho je u konce (jako u pokracovani P81).
        $declaredId = $state->getDeclaredBlitzerId();
        if ($declaredId !== null) {
            $actorId = isset($params['playerId']) ? (int) $params['playerId'] : null;
            if (($action === ActionType::MOVE || $action === ActionType::BLITZ) && $actorId === $declaredId) {
                $isContinuation = true;
            } elseif ($action === ActionType::END_TURN || ($action->requiresPlayer() && $actorId !== $declaredId)) {
                $blitzer = $state->getPlayer($declaredId);
                if ($blitzer !== null) {
                    $state = $state->withPlayer($blitzer->withHasMoved(true)->withHasActed(true));
                }
                $state = $state->withDeclaredBlitzer(null);
            }
        }

        // Big Guy pre-action check for player actions
        /** @var list<\App\DTO\GameEvent> $preEvents */
        $preEvents = [];
        // ⛔⛔⛔ STAND_PAT SE Z KONTROLY PRED AKCI VYJIMA (PHP25, 11.09.2026).
        //   Uzivatel: "pro big guye to navic znamena se neaktivovat -- tak
        //   nemusi hazet po aktivaci." Kontrola pred akci je dusledek
        //   AKTIVACE hrace; kdyz hrac zustava stat, zadna aktivace neprobehla,
        //   takze se NEHAZI Bone Head, Really Stupid, Wild Animal, Bloodlust
        //   ani Take Root. Kdyby se sem STAND_PAT pustil, stalo by "nic
        //   nedelat" tolik co akce -- a big guy by se mohl omracit tim, ze
        //   se rozhodl nehrat.
        if ($action !== ActionType::STAND_PAT && !$isContinuation
            && $action->requiresPlayer() && isset($params['playerId'])) {
            $player = $state->getPlayer((int) $params['playerId']);
            // Ztracene zony (Hypnotic Gaze, Bone Head, Really Stupid) plati "until the
            //   start of his next action" (`rules_bb2016.txt` r. 8187-8188) -- zacatkem
            //   akce se vraci; kontrola pred akci je pripadne vezme znovu.
            if ($player !== null && $player->hasLostTacklezones()) {
                $player = $player->withLostTacklezones(false);
                $state = $state->withPlayer($player);
            }
            if ($player !== null) {
                $checkResult = $this->bigGuyCheckResolver->resolvePreActionCheck(
                    $state,
                    $player,
                    $action,
                    $this->dice,
                );
                if ($checkResult !== null) {
                    // ⛔ PHP22: kontrola pred akci umi skoncit i TURNOVEREM --
                    //   Bloodlust, kdyz upir nema koho kousnout (r. 7942-7943),
                    //   nebo kdyz kousnuty Thrall drzel mic (r. 7941-7942).
                    //   Do 11.09. tahle vetev neexistovala a kolo beželo dal.
                    if (!empty($checkResult['turnover'])) {
                        return ActionResult::turnover(
                            $checkResult['state']->withTurnoverPending(true),
                            $checkResult['events'],
                        );
                    }
                    if (!empty($checkResult['proceed'])) {
                        // Bloodlust bite: state modified but action continues
                        $state = $checkResult['state'];
                        $preEvents = $checkResult['events'];
                    } else {
                        $blockedState = $checkResult['state'];
                        // ⛔ PHP15 (11.09.2026): r. 8398-8401 -- „The player
                        //   can't do anything for the turn, and **the player's
                        //   team loses the declared Action for that turn**
                        //   (for example if a Really Stupid player declares
                        //   a Blitz Action and fails the Really Stupid roll,
                        //   then the team cannot declare another Blitz Action
                        //   that turn)." U Wild Animal r. 8668-8669 stejne:
                        //   „**the Action is wasted**."
                        //   PHP to nedelalo u ZADNE ze ctyr dovednosti ⇒ Big
                        //   Guy, ktery sel k zemi na blitzu, tym o blitz
                        //   NEPRIPRAVIL a ten si ho zahral znovu jinym hracem.
                        //   Odecita se TADY, protoze do `match` niz (kde se
                        //   tymovy limit jinak nastavuje) uz se nedostaneme.
                        // ⚠️ C++ to ma jako `wastesTeamAction`
                        //   (`action_resolver.cpp:276-283`); PHP kopie ne.
                        if (!empty($checkResult['wastesTeamAction'])) {
                            $blockedState = $this->consumeDeclaredTeamAction(
                                $blockedState,
                                $player->getTeamSide(),
                                $action,
                            );
                        }

                        return ActionResult::success($blockedState, $checkResult['events']);
                    }
                }
            }
        }

        // Blood Lust: pred PRIHRAVKOU a PREDANIM se hladovy upir krmi hned
        //   ("before actually passing, handing off", r. 7935-7936).
        if (($action === ActionType::PASS || $action === ActionType::HAND_OFF) && isset($params['playerId'])) {
            $hladovy = $state->getPlayer((int) $params['playerId']);
            if ($hladovy !== null && $hladovy->isBloodlustHungry()) {
                $krmeni = $this->bigGuyCheckResolver->nakrmitUpira($state, $hladovy, $this->dice);
                $state = $krmeni['state'];
                $preEvents = array_merge($preEvents, $krmeni['events']);
                if ($krmeni['turnover']) {
                    return ActionResult::turnover($state->withTurnoverPending(true), $preEvents);
                }
            }
        }

        $result = match ($action) {
            ActionType::MOVE => $this->moveHandler->resolve($state, $params),
            ActionType::BLOCK => $this->blockHandler->resolve($state, $params),
            ActionType::BLITZ => $this->blitzHandler->resolve($state, $params),
            ActionType::PASS => $this->getPassResolver()->resolve($state, $params),
            ActionType::HAND_OFF => $this->handOffHandler->resolve($state, $params),
            ActionType::THROW_TEAM_MATE => $this->ttmHandler->resolve($state, $params),
            ActionType::BOMB_THROW => $this->bombThrowHandler->resolve($state, $params),
            ActionType::HYPNOTIC_GAZE => $this->hypnoticGazeHandler->resolve($state, $params),
            ActionType::BALL_AND_CHAIN => $this->ballAndChainHandler->resolve($state, $params),
            ActionType::MULTIPLE_BLOCK => $this->blockHandler->resolveMultipleBlock($state, $params),
            ActionType::FOUL => $this->foulHandler->resolve($state, $params),
            ActionType::STAND_PAT => $this->resolveStandPat($state, $params),
            ActionType::END_TURN => $this->endTurnHandler->resolve($state, $params),
            ActionType::SETUP_PLAYER => $this->setupHandler->resolve($state, $params),
            ActionType::END_SETUP => $this->setupHandler->resolveEndSetup($state),
            ActionType::AUTO_SETUP => $this->setupHandler->resolveAutoSetup($state, $params),
            ActionType::CHOOSE_BLOCK_DIE => $this->blockHandler->resolveBlockChoice($state, $params),
            ActionType::REROLL_BLOCK => $this->blockHandler->resolveBlockReroll($state, $params),
            ActionType::RESOLVE_REROLL => $this->rerollHandler->resolve($state, $params),
            ActionType::RESOLVE_APOTHECARY => $this->resolveApothecary($state, $params),
            ActionType::RESOLVE_GRAB => $this->resolveGrab($state, $params),
            ActionType::RESOLVE_STAND_FIRM => $this->resolveStandFirm($state, $params),
            ActionType::RESOLVE_DIVING_CATCH => throw new \LogicException('RESOLVE_DIVING_CATCH se vyřizuje v resolve()'),
        };

        // Prepend pre-action events (e.g. Bloodlust bite)
        if ($preEvents !== []) {
            $allEvents = array_merge($preEvents, $result->getEvents());
            if ($result->isTurnover()) {
                $result = ActionResult::turnover($result->getNewState(), $allEvents);
            } else {
                $result = ActionResult::success($result->getNewState(), $allEvents);
            }
        }

        // Auto-resolve pending blocks (unless interactive mode for human players)
        if (!$this->interactiveBlocks) {
            // P226: čeká-li kouč-člověk na otázku o Stand Firm, blok (pendingBlock) dál čeká na odpověď
            while ($result->getNewState()->getPendingBlock() !== null && $result->getNewState()->getPendingStandFirm() === null) {
                $autoResult = $this->blockHandler->autoResolvePendingBlock($result->getNewState());
                $mergedEvents = array_merge($result->getEvents(), $autoResult->getEvents());
                if ($autoResult->isTurnover()) {
                    $result = ActionResult::turnover($autoResult->getNewState(), $mergedEvents);
                    break;
                }
                $result = ActionResult::success($autoResult->getNewState(), $mergedEvents);
            }
        }

        // Auto-resolve pending rerolls (unless interactive mode for human players)
        if (!$this->interactiveRerolls) {
            while ($result->getNewState()->getPendingReroll() !== null) {
                $autoResult = $this->rerollHandler->autoResolve($result->getNewState());
                $mergedEvents = array_merge($result->getEvents(), $autoResult->getEvents());
                if ($autoResult->isTurnover()) {
                    $result = ActionResult::turnover($autoResult->getNewState(), $mergedEvents);
                    break;
                }
                $result = ActionResult::success($autoResult->getNewState(), $mergedEvents);
            }
        }

        // Blood Lust: hladovy upir se krmi na KONCI akce -- pred prihravkou, predanim
        //   i TD (TD se vyhodnocuje az po akci). `rules_bb2016.txt` r. 7934-7947.
        $novy = $result->getNewState();
        if ($novy->getPendingBlock() === null && $novy->getPendingReroll() === null && $novy->getPendingGrab() === null
            && $novy->getPendingStandFirm() === null) {
            foreach ($novy->getPlayers() as $upir) {
                if (!$upir->isBloodlustHungry()
                    || !($upir->hasActed() || $upir->hasMoved() || $action === ActionType::END_TURN)) {
                    continue;
                }
                $krmeni = $this->bigGuyCheckResolver->nakrmitUpira($result->getNewState(), $upir, $this->dice);
                $udalosti = array_merge($result->getEvents(), $krmeni['events']);
                $result = ($result->isTurnover() || $krmeni['turnover'])
                    ? ActionResult::turnover($krmeni['state'], $udalosti)
                    : ActionResult::success($krmeni['state'], $udalosti);
            }
        }

        return $result;
    }

    /**
     * P224 — odpověď kouče na otázku, jestli útočník použije Grab (`rules_bb2016.txt` ř. 8147-8154).
     * params: `use` (bool), při `use` ano `x`,`y` = zvolené volné pole vedle obránce. Rozpracovaný
     * blok (kostka už zvolena) se dokončí s touto volbou.
     *
     * @param array<string, mixed> $params
     */
    private function resolveGrab(GameState $state, array $params): ActionResult
    {
        $pending = $state->getPendingGrab();
        if ($pending === null) {
            throw new \InvalidArgumentException('No pending Grab question');
        }
        $volba = $params['use'] === true
            ? new Position((int) $params['x'], (int) $params['y'])
            : false;

        if ($pending->getKind() === 'bnc') {
            return $this->ballAndChainHandler->resumeGrab($state, $pending, $volba);
        }

        // follow-up se volí s odpovědí (po odtlačení, `rules_bb2016.txt` ř. 608-611)
        $blockParams = ['faceIndex' => $pending->getFaceIndex()];
        if (isset($params['followUp'])) {
            $blockParams['followUp'] = $params['followUp'];
        }

        return $this->blockHandler->resolveBlockChoice($state->withPendingGrab(null), $blockParams, $volba, $pending->getStandFirm());
    }

    /**
     * P226 — odpověď kouče na otázku, jestli jeho hráč použije Stand Firm (`rules_bb2016.txt`
     * ř. 8510-8516). params: `use` (bool). Rozpracovaný blok se vyhodnotí znovu od téže kostky
     * se všemi dosavadními odpověďmi (followUp z volby kostky a odpověď na Grab zůstávají).
     *
     * @param array<string, mixed> $params
     */
    private function resolveStandFirm(GameState $state, array $params): ActionResult
    {
        $pending = $state->getPendingStandFirm();
        if ($pending === null) {
            throw new \InvalidArgumentException('No pending Stand Firm question');
        }
        $odpovedi = $pending->getAnswers();
        $odpovedi[$pending->getPlayerId()] = $params['use'] === true;
        if ($pending->getKind() === 'bnc') {
            return $this->ballAndChainHandler->resumeStandFirm($state, $pending, $odpovedi);
        }
        if ($pending->getKind() === 'mb') {
            return $this->blockHandler->resumeMultipleBlock($state, $pending, $odpovedi);
        }
        $blockParams = ['faceIndex' => $pending->getFaceIndex()];
        if ($pending->getFollowUp() !== null) {
            $blockParams['followUp'] = $pending->getFollowUp();
        }

        return $this->blockHandler->resolveBlockChoice($state->withPendingStandFirm(null), $blockParams, $pending->getGrab(), $odpovedi);
    }

    /**
     * P206 (b), 16.10.2026 — odpověď kouče na nabídku lékárníka. `rules_bb2016.txt` ř. 1200-1210:
     * KO ⇒ zpět na hřiště omráčený na pole, odkud odešel (byl-li na hřišti), jinak rezervy; Casualty ⇒
     * soupeř hodí znovu na tabulku a kouč vybere jeden z výsledků; Badly Hurt ⇒ rezervy. Jednou za zápas.
     * params: stage 'offer' → `playerId` (0 = nepoužít); stage 'choose' → `result` (hodnota CasualtyResult).
     *
     * @param array<string, mixed> $params
     */
    private function resolveApothecary(GameState $state, array $params): ActionResult
    {
        $pending = $state->getPendingApothecary();
        if ($pending === null) {
            throw new \InvalidArgumentException('No pending apothecary choice');
        }
        $events = [];
        $hotovo = function (GameState $st) use ($pending, &$events): ActionResult {
            $st = $st->withPendingApothecary(null);
            // KO, u nichž se lékárník nepoužil: teprve teď z hřiště (do volby drželi své pole)
            foreach ($pending->getOffers() as $o) {
                $hrac = $st->getPlayer($o['playerId']);
                if ($o['kind'] === 'ko' && $hrac !== null && $hrac->getState() === PlayerState::KO && $hrac->getPosition() !== null) {
                    $st = $st->withPlayer($hrac->withPosition(null));
                }
            }
            // Regenerace hráčů, o kterých se rozhodovalo, až teď (lékárník má přednost, ř. 8406-8411)
            foreach ($pending->getOffers() as $o) {
                $hrac = $st->getPlayer($o['playerId']);
                if ($o['regeneration'] && $hrac !== null
                    && in_array($hrac->getState(), [PlayerState::INJURED, PlayerState::DEAD], true)) {
                    ['player' => $hrac, 'events' => $events] = $this->injuryResolver->resolveRegeneration($hrac, $this->dice, $o['stakes'], $events);
                    $st = $st->withPlayer($hrac);
                }
            }

            return new ActionResult($st, true, $pending->isTurnover(), $events);
        };

        if ($pending->getStage() === 'choose') {
            $volba = CasualtyResult::from((string) ($params['result'] ?? ''));
            if (!in_array($volba->value, $pending->getOptions(), true)) {
                throw new \InvalidArgumentException('Result not offered');
            }
            $id = (int) $pending->getChosenPlayerId();
            $o = $pending->offerFor($id);
            $hrac = $state->requirePlayer($id);
            $events[] = GameEvent::apothecaryUsed($id, (string) ($o['result'] ?? ''), $volba->value);
            $hrac = match ($volba) {
                CasualtyResult::BADLY_HURT => $hrac->withState(PlayerState::OFF_PITCH)->withPosition(null),
                CasualtyResult::DEAD => $hrac->withState(PlayerState::DEAD)->withPosition(null),
                default => $hrac->withState(PlayerState::INJURED)->withPosition(null),
            };

            return $hotovo($state->withPlayer($hrac));
        }

        $id = (int) ($params['playerId'] ?? 0);
        if ($id === 0) {
            return $hotovo($state);   // lékárník se nepoužil
        }
        $o = $pending->offerFor($id);
        if ($o === null) {
            throw new \InvalidArgumentException('Player not offered');
        }
        $hrac = $state->requirePlayer($id);
        $strana = $hrac->getTeamSide();
        $state = $state->withTeamState($strana, $state->getTeamState($strana)->withApothecaryUsed());

        if ($o['kind'] === 'ko') {
            // hráč do volby drží své pole (InjuryResolver) ⇒ omráčený zůstává tam; z davu (bez pole) do rezerv
            $pole = $hrac->getPosition();
            if ($pole !== null) {
                // review 16.10.: withStunned() — omráčený se v tomto kole neotáčí (ř. 703-707)
                $state = $state->withPlayer($hrac->withStunned());
                $events[] = GameEvent::apothecaryUsed($id, 'ko', 'stunned');
                // omráčený položený na volný míč: míč odskočí (ř. 893-898)
                $mic = $state->getBall();
                if (!$mic->isHeld() && $mic->getPosition()?->equals($pole)) {
                    $odraz = $this->ballResolver->resolveBounce($state, $pole);
                    $state = $odraz['state'];
                    $events = array_merge($events, $odraz['events']);
                }
            } else {
                $state = $state->withPlayer($hrac->withState(PlayerState::OFF_PITCH)->withPosition(null));
                $events[] = GameEvent::apothecaryUsed($id, 'ko', 'reserves');
            }

            return $hotovo($state);
        }

        // Casualty: Badly Hurt rovnou do rezerv, jinak druhý hod a volba kouče
        $prvni = CasualtyResult::from((string) $o['result']);
        if ($prvni === CasualtyResult::BADLY_HURT) {
            $events[] = GameEvent::apothecaryUsed($id, $prvni->value, $prvni->value);

            return $hotovo($state->withPlayer($hrac->withState(PlayerState::OFF_PITCH)->withPosition(null)));
        }
        $druhy = $this->injuryResolver->rollCasualty($this->dice)['result'];
        if ($druhy === $prvni) {
            $events[] = GameEvent::apothecaryUsed($id, $prvni->value, $prvni->value);

            return $hotovo($state);
        }

        return new ActionResult(
            $state->withPendingApothecary($pending->withChoice($id, [$prvni->value, $druhy->value])),
            true,
            false,
            $events,
        );
    }
}
