<?php

declare(strict_types=1);

namespace App\DTO;

use App\Enum\GamePhase;
use App\Enum\PlayerState;
use App\Enum\TeamSide;
use App\Enum\Weather;
use App\ValueObject\Position;
use App\DTO\PendingBlockDTO;
use App\DTO\PendingRerollDTO;

final class GameState
{
    /**
     * @param array<int, MatchPlayerDTO> $players keyed by match player id
     */
    public function __construct(
        private readonly int $matchId,
        private int $half,
        private GamePhase $phase,
        private TeamSide $activeTeam,
        private TeamStateDTO $homeTeam,
        private TeamStateDTO $awayTeam,
        private array $players,
        private BallState $ball,
        private bool $turnoverPending,
        private ?TeamSide $kickingTeam,
        private ?TeamSide $aiTeam = null,
        private Weather $weather = Weather::NICE,
        private ?PendingBlockDTO $pendingBlock = null,
        private ?PendingRerollDTO $pendingReroll = null,
        // P81 (30.09.2026): hrac, ktery po rane v blitzu smi jeste dojit
        //   (`rules_bb2016.txt` r. 551-552). Propadne, jakmile jedna jiny hrac
        //   nebo skonci kolo -- viz `ActionResolver::resolve`.
        private ?int $blitzContinuationPlayerId = null,
    ) {}

    /**
     * @param array<int, MatchPlayerDTO> $players
     */
    public static function create(
        int $matchId,
        TeamStateDTO $homeTeam,
        TeamStateDTO $awayTeam,
        array $players,
        TeamSide $receivingTeam,
    ): self {
        return new self(
            matchId: $matchId,
            half: 1,
            phase: GamePhase::SETUP,
            activeTeam: $receivingTeam,
            homeTeam: $homeTeam,
            awayTeam: $awayTeam,
            players: $players,
            ball: BallState::offPitch(),
            turnoverPending: false,
            kickingTeam: $receivingTeam->opponent(),
        );
    }

    public function getMatchId(): int
    {
        return $this->matchId;
    }
    public function getHalf(): int
    {
        return $this->half;
    }
    public function getPhase(): GamePhase
    {
        return $this->phase;
    }
    public function getActiveTeam(): TeamSide
    {
        return $this->activeTeam;
    }
    public function getHomeTeam(): TeamStateDTO
    {
        return $this->homeTeam;
    }
    public function getAwayTeam(): TeamStateDTO
    {
        return $this->awayTeam;
    }
    public function getBall(): BallState
    {
        return $this->ball;
    }
    public function isTurnoverPending(): bool
    {
        return $this->turnoverPending;
    }
    public function getKickingTeam(): ?TeamSide
    {
        return $this->kickingTeam;
    }
    public function getAiTeam(): ?TeamSide
    {
        return $this->aiTeam;
    }
    public function getWeather(): Weather
    {
        return $this->weather;
    }
    public function getPendingBlock(): ?PendingBlockDTO
    {
        return $this->pendingBlock;
    }
    public function getPendingReroll(): ?PendingRerollDTO
    {
        return $this->pendingReroll;
    }

    /**
     * Strana, jejíž trenér vybírá kostku čekajícího bloku. `rules_bb2016.txt` ř. 633-634:
     * "The coach of the stronger player picks which block dice is used."
     */
    public function getPendingBlockChooserSide(): ?TeamSide
    {
        if ($this->pendingBlock === null) {
            return null;
        }
        $chooserId = $this->pendingBlock->isAttackerChooses()
            ? $this->pendingBlock->getAttackerId()
            : $this->pendingBlock->getDefenderId();

        return $this->requirePlayer($chooserId)->getTeamSide();
    }

    public function getTeamState(TeamSide $side): TeamStateDTO
    {
        return $side === TeamSide::HOME ? $this->homeTeam : $this->awayTeam;
    }

    /**
     * @return array<int, MatchPlayerDTO>
     */
    public function getPlayers(): array
    {
        return $this->players;
    }

    /** Hrac, ktery ve stavu byt MUSI -- jinak je to chyba volajiciho, ne stav hry. */
    public function requirePlayer(int $id): MatchPlayerDTO
    {
        return $this->getPlayer($id) ?? throw new \LogicException("Player {$id} is not in the game state");
    }

    public function requirePendingReroll(): PendingRerollDTO
    {
        return $this->pendingReroll ?? throw new \LogicException('No pending reroll');
    }

    public function requirePendingBlock(): PendingBlockDTO
    {
        return $this->pendingBlock ?? throw new \LogicException('No pending block');
    }

    public function getPlayer(int $id): ?MatchPlayerDTO
    {
        return $this->players[$id] ?? null;
    }

    /**
     * Drzi mic nekdo z tohoto tymu?
     *
     * ⭐ SLOUCENO 11.09.2026. Tenhle predikat existoval jako
     *   `PassResolver::ballCaughtByTeam` (spravne) a JESTE JEDNOU inline
     *   v `HandOffHandler` (pridano tyz den pri oprave PHP11). Dve kopie
     *   jednoho vzorce uz tenhle projekt kously trikrat -- naposledy
     *   `pathFailProb` v enginu (`496f5a03`). Proto jedno misto hned,
     *   ne az se rozejdou.
     *
     * ⛔ Na tomhle predikatu stoji bod 2 katalogu turnoveru
     *   (`rules_bb2016.txt` r. 371-373): mic, ktery skoncil u NASEHO hrace,
     *   kolo neukoncuje.
     */
    public function isBallHeldBy(TeamSide $side): bool
    {
        $ball = $this->getBall();
        if (!$ball->isHeld()) {
            return false;
        }
        $carrierId = $ball->getCarrierId();
        if ($carrierId === null) {
            return false;
        }
        $carrier = $this->getPlayer($carrierId);

        return $carrier !== null && $carrier->getTeamSide() === $side;
    }

    public function getPlayerAtPosition(Position $pos): ?MatchPlayerDTO
    {
        foreach ($this->players as $player) {
            $playerPos = $player->getPosition();
            if ($playerPos !== null && $playerPos->equals($pos)) {
                return $player;
            }
        }
        return null;
    }

    /**
     * @return list<MatchPlayerDTO>
     */
    public function getTeamPlayers(TeamSide $side): array
    {
        return array_values(array_filter(
            $this->players,
            fn(MatchPlayerDTO $p) => $p->getTeamSide() === $side,
        ));
    }

    /**
     * @return list<MatchPlayerDTO>
     */
    public function getPlayersOnPitch(TeamSide $side): array
    {
        return array_values(array_filter(
            $this->players,
            fn(MatchPlayerDTO $p) => $p->getTeamSide() === $side && $p->getState()->isOnPitch(),
        ));
    }

    // --- Wither methods ---

    public function withPhase(GamePhase $phase): self
    {
        $clone = clone $this;
        $clone->phase = $phase;
        return $clone;
    }

    public function withActiveTeam(TeamSide $side): self
    {
        $clone = clone $this;
        $clone->activeTeam = $side;
        return $clone;
    }

    public function withHalf(int $half): self
    {
        $clone = clone $this;
        $clone->half = $half;
        return $clone;
    }

    public function withBall(BallState $ball): self
    {
        $clone = clone $this;
        $clone->ball = $ball;
        return $clone;
    }

    public function withPlayer(MatchPlayerDTO $player): self
    {
        $clone = clone $this;
        $clone->players[$player->getId()] = $player;
        return $clone;
    }

    public function withHomeTeam(TeamStateDTO $team): self
    {
        $clone = clone $this;
        $clone->homeTeam = $team;
        return $clone;
    }

    public function withAwayTeam(TeamStateDTO $team): self
    {
        $clone = clone $this;
        $clone->awayTeam = $team;
        return $clone;
    }

    public function withTeamState(TeamSide $side, TeamStateDTO $team): self
    {
        return $side === TeamSide::HOME
            ? $this->withHomeTeam($team)
            : $this->withAwayTeam($team);
    }

    public function getBlitzContinuationPlayerId(): ?int
    {
        return $this->blitzContinuationPlayerId;
    }

    public function withBlitzContinuation(?int $playerId): self
    {
        $clone = clone $this;
        $clone->blitzContinuationPlayerId = $playerId;
        return $clone;
    }

    public function withTurnoverPending(bool $pending): self
    {
        $clone = clone $this;
        $clone->turnoverPending = $pending;
        return $clone;
    }

    public function withKickingTeam(?TeamSide $kickingTeam): self
    {
        $clone = clone $this;
        $clone->kickingTeam = $kickingTeam;
        return $clone;
    }

    public function withAiTeam(?TeamSide $aiTeam): self
    {
        $clone = clone $this;
        $clone->aiTeam = $aiTeam;
        return $clone;
    }

    public function withWeather(Weather $weather): self
    {
        $clone = clone $this;
        $clone->weather = $weather;
        return $clone;
    }

    public function withPendingBlock(?PendingBlockDTO $pendingBlock): self
    {
        $clone = clone $this;
        $clone->pendingBlock = $pendingBlock;
        return $clone;
    }

    public function withPendingReroll(?PendingRerollDTO $pendingReroll): self
    {
        $clone = clone $this;
        $clone->pendingReroll = $pendingReroll;
        return $clone;
    }

    /**
     * Reset all players' hasMoved/hasActed for a new turn.
     */
    public function resetPlayersForNewTurn(TeamSide $side): self
    {
        $clone = clone $this;
        $clone->blitzContinuationPlayerId = null;   // P81: pokracovani konci s kolem
        foreach ($clone->players as $id => $player) {
            // ⛔ OPRAVA 18.09.2026: `rules_bb2016.txt` r. 8381 -- Pro "once per
            //   turn" plati pro KAZDE kolo, i souperovo (Pro jde pouzit i mimo
            //   vlastni kolo, napr. pri chytani). Nuloval se jen tym na tahu, takze
            //   Pro pouzity ve vlastnim kole blokoval hrace i cele nasledujici
            //   souperovo kolo.
            if ($player->getTeamSide() !== $side) {
                $clone->players[$id] = $player->withProUsedThisTurn(false);
                continue;
            }
            // ⛔⛔ OPRAVA 11.09.2026 (PHP15b + PHP15d): tenhle reset
            //   MAZAL DVA STAVY, ktere pravidla nechavaji pretrvat.
            //   (1) `lostTacklezones` z BoneHead / Really Stupid --
            //       r. 7985-7986: „until he manages to roll a 2 or better
            //       at the start of a future Action **or the drive ends**".
            //       Otupení tedy vydrzelo presne jedno kolo a zmizelo samo.
            //       ⇒ Drzi se v `bigGuyStupefied` a odtud se kazde kolo
            //       ZNOVU NASAZUJE (tataz konstrukce jako C++
            //       `game_state.cpp:71-72`).
            //   (2) `movementRemaining` u ZAKORENENEHO hrace -- r. 8575:
            //       „his MA is considered 0 until a drive ends". Bez teto
            //       vyjimky by se Treeman po jednom kole zase rozesel.
            //       ⛔ Tuhle druhou vadu jsem si sem PRIVEDL SAM dnesni
            //       opravou Take Root; nasla ji az kontrola tohohle mista.
            $clone->players[$id] = $player
                ->withHasMoved(false)
                ->withMovedThisTurn(false)
                ->withHasActed(false)
                ->withMovementRemaining(
                    $player->isRooted() ? 0 : $player->getStats()->getMovement(),
                )
                ->withLostTacklezones($player->isBigGuyStupefied())
                ->withProUsedThisTurn(false)
                ->withSureFeetUsedThisTurn(false)
                // ⭐ 21.09.2026: r. 960-962 -- Dodge je "once per turn", tedy
                //   za VLASTNI kolo (uhyba se jen ve svem kole).
                ->withDodgeUsedThisTurn(false)
                // ⭐ 21.09.2026: r. 7991 -- Break Tackle "may only be used once per turn"
                ->withBreakTackleUsedThisTurn(false)
                // OPRAVENO 08.10.2026 (audit parity, nález 1) -- tady se omráčený rovnou
                //   otáčel na PRONE, tedy na ZAČÁTKU kola svého týmu: hned v něm vstal a hrál
                //   a omráčení se nelišilo od sražení. Pravidla ř. 703-707: "turned face up
                //   at the END of their team's next turn". Otáčí `turnStunnedFaceUp()` na
                //   konci kola; tady se jen maže příznak, aby hráč omráčený v soupeřově kole
                //   (nebo v dřívějším vlastním) na konci tohohle kola vstal.
                ->withStunnedThisTurn(false);
        }
        return $clone;
    }

    /**
     * Konec kola týmu `$side`: jeho omráčení hráči se otáčejí lícem nahoru (`rules_bb2016.txt`
     * ř. 703-707: "All face-down players are turned face up at the end of their team's next
     * turn, even if a turnover takes place. Note that a player may not turn face up on the
     * turn they are Stunned."). Vzor: C++ `turn_handler.cpp:22-26`.
     */
    public function turnStunnedFaceUp(TeamSide $side): self
    {
        $clone = clone $this;
        foreach ($clone->players as $id => $player) {
            if ($player->getTeamSide() === $side
                && $player->getState() === PlayerState::STUNNED
                && !$player->isStunnedThisTurn()) {
                $clone->players[$id] = $player->withState(PlayerState::PRONE);
            }
        }

        return $clone;
    }

    /**
     * @return array<string, mixed>
     */
    public function toArray(): array
    {
        $playersArray = [];
        foreach ($this->players as $player) {
            $playersArray[] = $player->toArray();
        }

        return [
            'matchId' => $this->matchId,
            'half' => $this->half,
            'phase' => $this->phase->value,
            'activeTeam' => $this->activeTeam->value,
            'homeTeam' => $this->homeTeam->toArray(),
            'awayTeam' => $this->awayTeam->toArray(),
            'players' => $playersArray,
            'ball' => $this->ball->toArray(),
            'turnoverPending' => $this->turnoverPending,
            'kickingTeam' => $this->kickingTeam?->value,
            'aiTeam' => $this->aiTeam?->value,
            'weather' => $this->weather->value,
            'pendingBlock' => $this->pendingBlock?->toArray(),
            'pendingReroll' => $this->pendingReroll?->toArray(),
            'blitzContinuationPlayerId' => $this->blitzContinuationPlayerId,
        ];
    }

    /**
     * @param array<string, mixed> $data
     */
    public static function fromArray(array $data): self
    {
        $players = [];
        foreach ((array) $data['players'] as $playerData) {
            $player = MatchPlayerDTO::fromArray($playerData);
            $players[$player->getId()] = $player;
        }

        return new self(
            matchId: (int) $data['matchId'],
            half: (int) $data['half'],
            phase: GamePhase::from((string) $data['phase']),
            activeTeam: TeamSide::from((string) $data['activeTeam']),
            homeTeam: TeamStateDTO::fromArray((array) $data['homeTeam']),
            awayTeam: TeamStateDTO::fromArray((array) $data['awayTeam']),
            players: $players,
            ball: BallState::fromArray((array) $data['ball']),
            turnoverPending: (bool) $data['turnoverPending'],
            kickingTeam: isset($data['kickingTeam']) ? TeamSide::from((string) $data['kickingTeam']) : null,
            aiTeam: isset($data['aiTeam']) ? TeamSide::from((string) $data['aiTeam']) : null,
            weather: Weather::from((string) ($data['weather'] ?? 'nice')),
            pendingBlock: isset($data['pendingBlock']) ? PendingBlockDTO::fromArray((array) $data['pendingBlock']) : null,
            pendingReroll: isset($data['pendingReroll']) ? PendingRerollDTO::fromArray((array) $data['pendingReroll']) : null,
            blitzContinuationPlayerId: isset($data['blitzContinuationPlayerId']) ? (int) $data['blitzContinuationPlayerId'] : null,
        );
    }
}
