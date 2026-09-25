<?php
declare(strict_types=1);

namespace App\Engine;

use App\DTO\ActionResult;
use App\DTO\GameEvent;
use App\DTO\GameState;
use App\DTO\MatchPlayerDTO;
use App\Enum\ActionType;
use App\Enum\PlayerState;
use App\Enum\SkillName;

final class BigGuyCheckResolver
{
    /**
     * ⛔ ZAVISLOSTI JSOU POVINNE (11.09.2026). Drive tu byly nullable
     *   s nahradni konstrukci `new BallResolver(new RandomDiceRoller(), ...)`.
     *   To do tridy protahovalo **DRUHY, NESEEDOVANY generator kostek** vedle
     *   toho, ktery prichazi parametrem do `resolvePreActionCheck` --
     *   a odraz mice po neuspesnem Bloodlustu by pak sel z jine kostky nez
     *   zbytek behu. ⇒ Tiche rozbiti reprodukovatelnosti mereni, tedy presne
     *   to, co se v tomhle projektu nesmi.
     *   Volajici je jediny (`ActionResolver`) a obe zavislosti predava.
     */
    public function __construct(
        private readonly InjuryResolver $injuryResolver,
        private readonly BallResolver $ballResolver,
    ) {
    }

    /**
     * ⛔⛔ `wastesTeamAction` (doplneno 11.09.2026 -- polozka PHP15).
     *   `rules_bb2016.txt` r. 8398-8401: „The player can't do anything for the
     *   turn, and **the player's team loses the declared Action for that turn**
     *   (for example if a Really Stupid player declares a Blitz Action and
     *   fails the Really Stupid roll, then **the team cannot declare another
     *   Blitz Action that turn**)."
     *   A u Wild Animal r. 8668-8669: „**the Action is wasted**."
     *
     *   PHP to nedelalo u ZADNE ze ctyr dovednosti ⇒ Big Guy, ktery sel
     *   k zemi na blitzu, tym o blitz NEPRIPRAVIL a ten si ho zahral znovu
     *   jinym hracem. Cista vyhoda proti pravidlum.
     *
     * ⚠️ C++ engine to ma jako `wastesTeamAction` (`big_guy_handler.h:39`,
     *   nastavovane na ctyrech mistech) a odecita ho
     *   `action_resolver.cpp:280` pres `consumeDeclaredTeamAction`, protoze
     *   do switche, kde se limit jinak nastavuje, uz se nedostane. PHP kopie
     *   to nemela -- CTVRTY drift teze tridy.
     *
     * @return array{state: GameState, events: list<\App\DTO\GameEvent>, proceed?: bool, turnover?: bool, wastesTeamAction?: bool}|null
     */
    public function resolvePreActionCheck(
        GameState $state,
        MatchPlayerDTO $player,
        ActionType $action,
        DiceRollerInterface $dice,
    ): ?array {
        if ($player->hasSkill(SkillName::BoneHead)) {
            return $this->resolveBoneHead($state, $player, $dice);
        }

        if ($player->hasSkill(SkillName::ReallyStupid)) {
            return $this->resolveReallyStupid($state, $player, $dice);
        }

        if ($player->hasSkill(SkillName::WildAnimal)) {
            return $this->resolveWildAnimal($state, $player, $action, $dice);
        }

        // ⛔ OPRAVA 11.09.2026 (PHP15d): hod se hazel JEN NA `MOVE`.
        //   `rules_bb2016.txt` r. 8573: „**Immediately after declaring an
        //   Action** with this player, roll a D6." Tedy i na BLOCK, BLITZ,
        //   PASS, HAND-OFF a FOUL -- Treeman doted blokoval bez rizika.
        //   A hazi se jen dokud nezakorenil (pak uz je stav dany).
        if ($player->hasSkill(SkillName::TakeRoot) && !$player->isRooted()) {
            return $this->resolveTakeRoot($state, $player, $action, $dice);
        }

        if ($player->hasSkill(SkillName::Bloodlust) && !$player->isBloodlustHungry()) {
            return $this->resolveBloodlust($state, $player, $action, $dice);
        }

        return null;
    }

    /**
     * Bone-head: Roll D6, on 1 = lose action + lose tacklezones.
     *
     * @return array{state: GameState, events: list<GameEvent>}|null
     */
    private function resolveBoneHead(
        GameState $state,
        MatchPlayerDTO $player,
        DiceRollerInterface $dice,
    ): ?array {
        $roll = $dice->rollD6();

        if ($roll >= 2) {
            // r. 7985-7986: „**until he manages to roll a 2 or better** at the
            //   start of a future Action" -- uspesny hod stav UKONCUJE.
            if ($player->isBigGuyStupefied()) {
                $state = $state->withPlayer(
                    $player->withBigGuyStupefied(false)->withLostTacklezones(false),
                );

                return ['state' => $state, 'events' => [], 'proceed' => true];
            }

            return null;
        }

        // Sem se dojde jen pri hodu 1 -- vetev `>= 2` vyse vycerpala zbytek.
        $player = $player
            ->withLostTacklezones(true)
            ->withBigGuyStupefied(true)
            ->withHasMoved(true)
            ->withHasActed(true);
        $state = $state->withPlayer($player);

        return [
            'state' => $state,
            'events' => [GameEvent::boneHeadFail($player->getId(), $roll)],
            // ⭐ viz `wastesTeamAction` niz
            'wastesTeamAction' => true,
        ];
    }

    /**
     * Really Stupid: Roll D6, need 2+ (adjacent ally) or 4+ (no ally).
     * On fail = lose action + lose tacklezones.
     *
     * @return array{state: GameState, events: list<GameEvent>}|null
     */
    private function resolveReallyStupid(
        GameState $state,
        MatchPlayerDTO $player,
        DiceRollerInterface $dice,
    ): ?array {
        $hasAdjacentAlly = $this->hasReallyStupidSupport($state, $player);
        $threshold = $hasAdjacentAlly ? 2 : 4;

        $roll = $dice->rollD6();

        if ($roll >= $threshold && $player->isBigGuyStupefied()) {
            // r. 8403-8405: „until he manages to roll a **successful result**
            //   for a Really Stupid roll at the start of a future Action".
            $state = $state->withPlayer(
                $player->withBigGuyStupefied(false)->withLostTacklezones(false),
            );

            return ['state' => $state, 'events' => [], 'proceed' => true];
        }

        if ($roll < $threshold) {
            $player = $player
                ->withLostTacklezones(true)
                ->withBigGuyStupefied(true)
                ->withHasMoved(true)
                ->withHasActed(true);
            $state = $state->withPlayer($player);

            return [
                'state' => $state,
                'events' => [GameEvent::reallyStupidFail($player->getId(), $roll, $hasAdjacentAlly)],
                'wastesTeamAction' => true,
            ];
        }

        return null;
    }

    /**
     * Wild Animal: Roll D6, on 1-2 = lose action (keeps tacklezones).
     * Block/Blitz = automatic success.
     *
     * @return array{state: GameState, events: list<GameEvent>}|null
     */
    private function resolveWildAnimal(
        GameState $state,
        MatchPlayerDTO $player,
        ActionType $action,
        DiceRollerInterface $dice,
    ): ?array {
        // ⛔⛔⛔ OPRAVA 11.09.2026 -- DVĚ VADY NARÁZ, OBĚ PROTI TEXTU PRAVIDEL.
        //   `rules_bb2016.txt` r. 8666-8669: „immediately after declaring an
        //   Action with a Wild Animal, roll a D6, **adding 2 to the roll if
        //   taking a Block or Blitz Action**. On a roll of **1-3**, the Wild
        //   Animal does not move ... and the Action is wasted."
        //
        //   (1) Block/Blitz tu měly `return null`, tedy AUTO-PASS BEZ HODU.
        //       Pravidla dávají bonus +2, ne imunitu: s +2 projde přirozená
        //       2+, ale přirozená 1 PADÁ. ⇒ Rat Ogre / Minotaur byl o šestinu
        //       spolehlivější, než pravidla dovolují.
        //   (2) Ostatní akce padaly na `<= 2`, tedy 1-2. Pravidla říkají
        //       1-3. ⇒ Trojka procházela, ač neměla.
        //
        // ⚠️ TATÁŽ VADA UŽ BYLA NAJITA A OPRAVENA V C++ ENGINU 07.08.2026
        //   (`engine/src/big_guy_handler.cpp:87-93`, vlastní komentář:
        //   „before this the block/blitz branch skipped the roll entirely,
        //   making a Rat Ogre / Minotaur a sixth more reliable at hitting
        //   than the rules allow"). PHP kopie se neopravila. ⇒ Je to týž
        //   vzorec ve dvou kopiích -- třída, která tenhle repozitář kousla
        //   už dvakrát (`131a1779` a `pathFailProb` v `496f5a03`).
        //
        // ⭐ DEKLARACE BLITZU JE PROTO LEGITIMNÍ TAH, i když se na cíl
        //   nedosáhne: je to způsob, jak Wild Animal rozhýbat na 2+ místo
        //   4+ (uživatel 11.09.). Na tom stojí oprava v `BlitzHandler`.
        $hitting = ($action === ActionType::BLOCK || $action === ActionType::BLITZ);
        $target = $hitting ? 2 : 4;

        $roll = $dice->rollD6();

        if ($roll < $target) {
            // Wild Animal loses action but keeps tacklezones
            $player = $player
                ->withHasMoved(true)
                ->withHasActed(true);
            $state = $state->withPlayer($player);

            return [
                'state' => $state,
                'events' => [GameEvent::wildAnimalFail($player->getId(), $roll)],
                'wastesTeamAction' => true,
            ];
        }

        return null;
    }

    /**
     * Take Root: Roll D6 on move, 1 = cannot move.
     * Block/Blitz not affected.
     *
     * @return array{state: GameState, events: list<GameEvent>}|null
     */
    private function resolveTakeRoot(
        GameState $state,
        MatchPlayerDTO $player,
        ActionType $action,
        DiceRollerInterface $dice,
    ): ?array {
        $roll = $dice->rollD6();

        if ($roll !== 1) {
            return null;   // r. 8574: „On a 2 or more ... Action as normal"
        }

        // ⛔⛔ PREPSANO 11.09.2026 (PHP15 b/d) podle `rules_bb2016.txt`
        //   r. 8572-8584. Puvodni PHP verze byla spatne TREMI zpusoby --
        //   presne tymiz, ktere C++ engine opravil 24.08. jako `TA2`
        //   (`engine/src/big_guy_handler.cpp:112-158`):
        //   (1) hod se hazel jen na MOVE (opraveno v dispatchi vys);
        //   (2) zakorenení NEPERZISTOVALO, takze priste zase normalne chodil;
        //   (3) na 1 se blokovala KAZDA akce -- pravidlo ale blok vyslovne
        //       DOVOLUJE.
        //
        // r. 8575-8576: „his MA is considered 0 **until a drive ends, or he is
        //   Knocked Down or Placed Prone**" ⇒ stav pretrvava pres kola.
        $player = $player
            ->withRooted(true)
            ->withMovementRemaining(0);

        // r. 8581-8584: „The player **may block adjacent players** without
        //   following-up as part of a Block Action **however if a player
        //   fails his Take Root roll as part of a Blitz Action he may not
        //   block that turn** (he can still roll to stand up if he is Prone)."
        //   ⇒ BLOCK, PASS, HAND-OFF a FOUL zakorenení nebrani; BLITZ ano,
        //   protoze ten je pohyb + blok.
        $mayStillAct = in_array($action, [
            ActionType::BLOCK,
            ActionType::PASS,
            ActionType::HAND_OFF,
            ActionType::FOUL,
        ], true);

        $state = $state->withPlayer($player);
        $events = [GameEvent::takeRoot($player->getId(), $roll, true)];

        if ($mayStillAct) {
            // Zakorenil, ale akci smi dokoncit -- jen se nehne.
            return [
                'state' => $state,
                'events' => $events,
                'proceed' => true,
            ];
        }

        $state = $state->withPlayer(
            $player->withHasMoved(true)->withHasActed(true),
        );

        return [
            'state' => $state,
            'events' => $events,
            // M2 / r. 351-352: limit visi na DEKLARACI, ne na dokonceni.
            'wastesTeamAction' => true,
        ];
    }

    /**
     * Blood Lust (`rules_bb2016.txt` r. 7922-7947): D6 hned po ohlaseni akce; na 1 je
     * upir HLADOVY -- akci dokonci (ohlaseny BLOCK smi zmenit na Move) a nakrmi se az
     * na jejim KONCI (viz `nakrmitUpira`, vola ActionResolver po akci).
     * Politika pro zmenu Block -> Move: kdyz uz vedle stoji Thrall, blokuje; jinak se
     * blok neprovede a upir zustava neaktivovany, aby mohl dobehnout k Thrallovi.
     *
     * @return array{state: GameState, events: list<GameEvent>, proceed: bool}|null
     */
    private function resolveBloodlust(
        GameState $state,
        MatchPlayerDTO $player,
        ActionType $action,
        DiceRollerInterface $dice,
    ): ?array {
        $roll = $dice->rollD6();
        if ($roll >= 2) {
            return null; // Pass — action proceeds normally
        }

        $state = $state->withPlayer($player->withBloodlustHungry(true));
        $events = [GameEvent::bloodlustHungry($player->getId(), $roll)];
        $blokBezThralla = $action === ActionType::BLOCK && $this->findAdjacentThrall($state, $player) === null;

        return ['state' => $state, 'events' => $events, 'proceed' => !$blokBezThralla];
    }

    /**
     * Krmeni hladoveho upira na konci akce -- pred prihravkou, predanim i TD.
     * Thrall vedle (stoji, lezi, omracen): hod na zraneni, CAS = Badly Hurt, turnover jen
     * kdyz Thrall drzel mic. Bez Thralla: upir do rezerv, mic odskoci z jeho pole, TURNOVER.
     *
     * @return array{state: GameState, events: list<GameEvent>, turnover: bool}
     */
    public function nakrmitUpira(GameState $state, MatchPlayerDTO $vampire, DiceRollerInterface $dice): array
    {
        $vampire = $vampire->withBloodlustHungry(false);
        $state = $state->withPlayer($vampire);
        if ($vampire->getPosition() === null) {
            return ['state' => $state, 'events' => [], 'turnover' => false];
        }

        $thrall = $this->findAdjacentThrall($state, $vampire);
        if ($thrall !== null) {
            $thrallHadBall = $state->getBall()->isHeld() && $state->getBall()->getCarrierId() === $thrall->getId();
            $inj = $this->injuryResolver->resolveInjuryOnly($thrall, $dice);
            $bitten = $inj['player'];
            if ($bitten->getState() === PlayerState::DEAD) {
                $bitten = $bitten->withState(PlayerState::INJURED); // "treating any casualty roll as Badly Hurt"
            }
            $state = $state->withPlayer($bitten);
            $events = array_merge([GameEvent::bloodlustBite($vampire->getId(), $thrall->getId(), 1)], $inj['events']);
            if ($thrallHadBall) {
                [$state, $events] = $this->ballResolver->handleBallOnPlayerDown($state, $bitten, $events);
            }

            return ['state' => $state, 'events' => $events, 'turnover' => $thrallHadBall];
        }

        $events = [GameEvent::bloodlustFail($vampire->getId(), 1)];
        [$state, $events] = $this->ballResolver->handleBallOnPlayerDown($state, $vampire, $events);
        $state = $state->withPlayer(
            $vampire->withPosition(null)->withState(PlayerState::OFF_PITCH)->withHasMoved(true)->withHasActed(true),
        );

        return ['state' => $state, 'events' => $events, 'turnover' => true];
    }

    /**
     * Find an adjacent friendly Thrall (non-Vampire teammate) for Bloodlust bite.
     */
    private function findAdjacentThrall(GameState $state, MatchPlayerDTO $vampire): ?MatchPlayerDTO
    {
        $pos = $vampire->getPosition();
        if ($pos === null) {
            return null;
        }

        foreach ($state->getPlayersOnPitch($vampire->getTeamSide()) as $teammate) {
            if ($teammate->getId() === $vampire->getId()) {
                continue;
            }
            // Thrall = any teammate without Bloodlust (non-Vampire)
            if ($teammate->hasSkill(SkillName::Bloodlust)) {
                continue;
            }
            $teammatePos = $teammate->getPosition();
            // ⛔ OPRAVA 11.09.2026 (PHP22): drive se zadalo `=== STANDING`.
            //   r. 7938-7939: „If he is standing adjacent to one or more
            //   Thrall team-mates (**standing, prone or stunned**)" -- lezici
            //   i omraceny Thrall se kousnout DA.
            if ($teammatePos !== null && $pos->distanceTo($teammatePos) === 1
                && $teammate->getState()->isOnPitch()) {
                return $teammate;
            }
        }

        return null;
    }

    /**
     * Ma Really Stupid hrac vedle sebe souseda, ktery mu da bonus +2?
     *
     * ⭐ NAZEV 11.09.2026: drive `hasAdjacentTeammate`, coz LHALO -- nejde
     *   o „libovolneho spoluhrace vedle", ale o presne kvalifikovaneho
     *   souseda podle r. 8393-8395. Jediny volajici je Really Stupid.
     */
    private function hasReallyStupidSupport(GameState $state, MatchPlayerDTO $player): bool
    {
        $pos = $player->getPosition();
        if ($pos === null) {
            return false;
        }

        foreach ($state->getPlayersOnPitch($player->getTeamSide()) as $teammate) {
            if ($teammate->getId() === $player->getId()) {
                continue;
            }
            // ⛔⛔ OPRAVA 11.09.2026 (PHP15c): TADY SE NEKONTROLOVALO NIC
            //   z toho, co pravidlo zada. `rules_bb2016.txt` r. 8393-8395:
            //   „If there are one or more players from the same team
            //   **STANDING** adjacent to the Really Stupid player's square,
            //   **and who aren't Really Stupid**, then add 2 to the D6 roll."
            //   ⇒ (1) lezici a omraceny soused se NEPOCITA;
            //     (2) soused, ktery je SAM Really Stupid, se nepocita taky --
            //         bez toho se dva Really Stupid navzajem PODPIRAJI a oba
            //         hazi na 2+ misto na 4+.
            // ⚠️ C++ to ma spravne (`big_guy_handler.cpp:44-62`) a je tam
            //   i vlastni poznamka M3b, ze `lostTacklezones` se tu naopak
            //   kontrolovat NEMA -- pravidlo o tacklezonach nic nerika.
            if (!$teammate->getState()->canAct()) {
                continue;
            }
            if ($teammate->hasSkill(SkillName::ReallyStupid)) {
                continue;
            }
            $teammatePos = $teammate->getPosition();
            if ($teammatePos !== null && $pos->distanceTo($teammatePos) === 1) {
                return true;
            }
        }

        return false;
    }
}
