<?php
declare(strict_types=1);

namespace App\Tests\AI;

use App\AI\LearningAICoach;
use App\Engine\RulesEngine;
use App\Enum\ActionType;
use App\Enum\TeamSide;
use App\Tests\Engine\GameStateBuilder;
use PHPUnit\Framework\TestCase;

/**
 * ⭐⭐ KLEC — PHP33, zadání uživatele 12.09.2026:
 *    *„začni správným postavením klece a pak pohybem celé klece dopředu."*
 *
 * Definice, kterou uživatel potvrdil: **klec = nosič + ČTYŘI DIAGONÁLNÍ ROHY.**
 * Ortogonální soused je k ničemu — soupeř na nosiče dosáhne stejně.
 *
 * ⛔ Do 12.09. dával kouč `+1.0` za **jakékoli** sousední pole ⇒ vznikala
 *    hvězda kolem nosiče, ne klec.
 */
final class CageFormationTest extends TestCase
{
    /** Nosič HOME uprostřed, jeden volný spoluhráč, soupeř daleko. */
    private function stateWithCarrierAndOneHelper(int $helperX, int $helperY): \App\DTO\GameState
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 10, 7, movement: 6, id: 1)          // nosič
            ->addPlayer(TeamSide::HOME, $helperX, $helperY, movement: 6, id: 2)
            ->addPlayer(TeamSide::AWAY, 24, 1, id: 3)
            ->withBallCarried(1)
            ->build();

        // Nosič už jednal ⇒ rozhoduje se jen o pomocníkovi.
        return $state->withPlayer(
            $state->getPlayer(1)->withHasActed(true)->withHasMoved(true),
        );
    }

    public function testPlayerAlreadyOnACornerHoldsPosition(): void
    {
        // Pomocník stojí na rohu (9,6) — diagonála od nosiče (10,7).
        $state = $this->stateWithCarrierAndOneHelper(9, 6);
        $rules = new RulesEngine();

        // SEBEKONTROLA FIXTURY: hráč se opravdu MŮŽE hnout, takže „zůstal stát"
        // je rozhodnutí, ne nedostatek možností.
        $this->assertNotSame([], $rules->getValidMoveTargets($state, 2),
            'fixtura je vadná: hráč nemá kam, držení pozice by nic neznamenalo');

        $decision = (new LearningAICoach())->decideAction($state, $rules);

        $this->assertSame(ActionType::STAND_PAT, $decision['action'],
            'hráč stál v rohu klece a přesto se hnul — klec se tím rozsype');
        $this->assertSame(2, $decision['params']['playerId']);
    }

    public function testPlayerOffTheCageMovesToACornerNotToAnEdge(): void
    {
        // Pomocník stojí dál (12,7) a má se zařadit. Roh je diagonála,
        // hrana (např. 11,7 nebo 10,6) je k ničemu.
        $state = $this->stateWithCarrierAndOneHelper(13, 7);
        $rules = new RulesEngine();

        $decision = (new LearningAICoach())->decideAction($state, $rules);

        $this->assertSame(ActionType::MOVE, $decision['action']);
        $dx = abs($decision['params']['x'] - 10);
        $dy = abs($decision['params']['y'] - 7);
        $this->assertTrue($dx === 1 && $dy === 1,
            sprintf('kouč šel na (%d,%d) — to je hrana nebo mimo klec, ne roh',
                $decision['params']['x'], $decision['params']['y']));
    }

    public function testHoldingBeatsShufflingToAnotherCorner(): void
    {
        // ⭐ Držení musí přebít přesun na JINÝ roh, jinak by kouč rohy jen
        //    přehazoval a klec by se pořád rozpadala a skládala.
        $state = $this->stateWithCarrierAndOneHelper(11, 8);   // taky roh
        $rules = new RulesEngine();

        $decision = (new LearningAICoach())->decideAction($state, $rules);

        $this->assertSame(ActionType::STAND_PAT, $decision['action']);
    }

    public function testCarrierIsCappedByTheSlowestCageMember(): void
    {
        // ⭐ PHP33 (B), upřesněno uživatelem 12.09.: *„max pohyb klece podle
        //    nejmenšího MA ze všech pěti."* Klec se posouvá celá, takže dál,
        //    než ujde nejpomalejší z ní, jít nemůže — jinak zůstane roh prázdný.
        //    Tady má jeden roh MA 2, ostatní 6 ⇒ strop jsou DVĚ pole.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 10, 7, movement: 6, id: 1)   // nosič
            ->addPlayer(TeamSide::HOME, 9, 6, movement: 2, id: 2)    // ⭐ nejpomalejší roh
            ->addPlayer(TeamSide::HOME, 9, 8, movement: 6, id: 3)
            ->addPlayer(TeamSide::HOME, 11, 6, movement: 6, id: 4)
            ->addPlayer(TeamSide::HOME, 11, 8, movement: 6, id: 5)
            ->addPlayer(TeamSide::AWAY, 24, 1, id: 6)
            ->withBallCarried(1)
            ->build();
        // Rohy už jednaly ⇒ rozhoduje se jen o nosiči.
        foreach ([2, 3, 4, 5] as $id) {
            $state = $state->withPlayer(
                $state->getPlayer($id)->withHasActed(true)->withHasMoved(true),
            );
        }

        $rules = new RulesEngine();

        // SEBEKONTROLA: nosič se OPRAVDU může dostat dál než o jedno pole.
        $daleko = array_filter($rules->getValidMoveTargets($state, 1),
            static fn(array $t) => max(abs($t['x'] - 10), abs($t['y'] - 7)) > 2);
        $this->assertNotSame([], $daleko,
            'fixtura je vadná: nosič se dál než o dvě pole nedostane, strop by nic neznamenal');

        $decision = (new LearningAICoach())->decideAction($state, $rules);

        $this->assertSame(ActionType::MOVE, $decision['action']);
        $krok = max(abs($decision['params']['x'] - 10), abs($decision['params']['y'] - 7));
        // ⭐ Upřesněno uživatelem 12.09.: nejpomalejší klec nezastaví — dožene
        //    ji přes GFI, jen se aktivuje poslední, protože to je riziko.
        //    Strop je tedy MA nejpomalejšího + 2 pole na GFI.
        $this->assertLessThanOrEqual(4, $krok,
            sprintf('nosič skočil o %d pole; nejpomalejší roh ujde 2 a s GFI 4', $krok));
    }

    public function testCarrierWithoutACageStillSprints(): void
    {
        // ⭐ POZITIVNÍ KONTROLA OBRÁCENĚ: pokuta se smí projevit JEN tehdy,
        //    když klec existuje. Bez rohů se nosič pohybuje jako dřív —
        //    jinak bych zpomalil hru všude, ne jen v kleci.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 10, 7, movement: 6, id: 1)
            ->addPlayer(TeamSide::AWAY, 24, 1, id: 6)
            ->withBallCarried(1)
            ->build();

        $decision = (new LearningAICoach())->decideAction($state, new RulesEngine());

        $this->assertSame(ActionType::MOVE, $decision['action']);
        $krok = max(abs($decision['params']['x'] - 10), abs($decision['params']['y'] - 7));
        $this->assertGreaterThan(1, $krok,
            'bez klece nemá co nosiče brzdit — pokuta se pouští i tam, kde nemá');
    }

    public function testCarrierNeverStepsNextToAnOpponent(): void
    {
        // ⛔⛔ Uživatel 12.09.: „a postavit nosiče vedle soupeře nesmíme už vůbec."
        //    Nosič má před sebou soupeře na (12,7). Postup vpřed k němu je
        //    bodově lákavý (blíž ke koncové zóně), ale skončit v jeho zóně
        //    zachycení znamená blok, sražení a ztrátu míče.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 10, 7, movement: 6, id: 1)
            ->addPlayer(TeamSide::AWAY, 12, 7, id: 2)
            ->withBallCarried(1)
            ->build();

        $rules = new RulesEngine();

        // SEBEKONTROLA: pole vedle soupeře jsou vůbec dosažitelná.
        $vedle = array_filter($rules->getValidMoveTargets($state, 1),
            static fn(array $t) => max(abs($t['x'] - 12), abs($t['y'] - 7)) === 1);
        $this->assertNotSame([], $vedle,
            'fixtura je vadná: vedle soupeře se nedá stoupnout, zákaz by nic neznamenal');

        $decision = (new LearningAICoach())->decideAction($state, $rules);

        $this->assertSame(ActionType::MOVE, $decision['action']);
        $odstup = max(abs($decision['params']['x'] - 12), abs($decision['params']['y'] - 7));
        $this->assertGreaterThan(1, $odstup,
            sprintf('nosič skončil na (%d,%d), tedy vedle soupeře',
                $decision['params']['x'], $decision['params']['y']));
    }

    public function testPickingUpTheBallBeatsAnyRiskFreeMove(): void
    {
        // ⭐ Uživatel 12.09.: „dej prioritu tahu zvednutí míče před ostatními
        //    pohyby bez rizika." Míč leží volně; hráč 2 na něj dosáhne, a
        //    zároveň by mohl bez rizika zaujmout roh u spoluhráče. Zvednutí
        //    musí vyhrát — bez míče není co vozit v kleci.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 10, 7, movement: 6, id: 1)
            ->addPlayer(TeamSide::HOME, 12, 7, movement: 6, id: 2)
            ->addPlayer(TeamSide::AWAY, 24, 1, id: 3)
            ->withBallOnGround(13, 8)
            ->build();
        $state = $state->withPlayer(
            $state->getPlayer(1)->withHasActed(true)->withHasMoved(true),
        );

        $rules = new RulesEngine();
        $decision = (new LearningAICoach())->decideAction($state, $rules);

        $this->assertSame(ActionType::MOVE, $decision['action']);
        $this->assertSame(13, $decision['params']['x'], 'nešel pro míč');
        $this->assertSame(8, $decision['params']['y'], 'nešel pro míč');
    }

    public function testUncontestedBallIsPickedUpByTheSpecialist(): void
    {
        // ⭐⭐ Uživatel 12.09.: „pokud je míč na zemi a nejsou kolem soupeři —
        //    musí runner nebo thrower k míči a zvednout = nejen v prvním kole,
        //    nej priorita." A důvod dodal taky: *„dokud neneseme míč, nemůžeme
        //    dát TD."*
        //    Oba hráči na míč dosáhnou stejně; jeden má Sure Hands, tedy je to
        //    ten, kdo míč udrží. (Positional name builder nastavit neumí,
        //    proto se tu testuje ta větev přes dovednost.)
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 11, 7, movement: 6, id: 1)
            ->addPlayer(TeamSide::HOME, 15, 7, movement: 6, id: 2,
                        skills: [\App\Enum\SkillName::SureHands])
            ->addPlayer(TeamSide::AWAY, 24, 1, id: 3)
            ->withBallOnGround(13, 7)
            ->build();

        $rules = new RulesEngine();

        // SEBEKONTROLA: na míč dosáhnou OBA, jinak by volba nic neznamenala.
        foreach ([1, 2] as $id) {
            $cile = array_filter($rules->getValidMoveTargets($state, $id),
                static fn(array $t) => $t['x'] === 13 && $t['y'] === 7);
            $this->assertNotSame([], $cile, "fixtura je vadná: hráč {$id} na míč nedosáhne");
        }

        $decision = (new LearningAICoach())->decideAction($state, $rules);

        $this->assertSame(ActionType::MOVE, $decision['action']);
        $this->assertSame(2, $decision['params']['playerId'],
            'pro nekrytý míč šel někdo jiný než specialista');
        $this->assertSame(13, $decision['params']['x']);
        $this->assertSame(7, $decision['params']['y']);
    }
}
