<?php
declare(strict_types=1);

namespace App\Engine;

use App\DTO\GameEvent;
use App\DTO\MatchPlayerDTO;
use App\Enum\CasualtyResult;
use App\Enum\PlayerState;
use App\Enum\SkillName;

final class InjuryResolver
{
    /**
     * Resolve armor roll and potentially injury for a knocked-down player.
     *
     * @return array{player: MatchPlayerDTO, events: list<GameEvent>}
     */
    /**
     * ⭐⭐ `$mightyBlow`: OPRAVA 14.09.2026 -- MB je VOLBA, ne pevny bonus.
     *
     * `rules_bb2016.txt` r. 8291-8297: "Add 1 to ANY Armour OR Injury roll…
     * Note that you only modify ONE of the dice rolls, so if you decide to
     * use Mighty Blow to modify the Armour roll, you may not modify the
     * Injury roll as well."
     *
     * ⛔ Do dneska se MB predaval jako `armourModifier` a `injuryModifier`
     *   byl vzdy 0 ⇒ **utratil se vzdy za brneni**. Kdyz se brneni prolomilo
     *   i bez nej, bonus PROPADL. To neni nelegalni, ale je to trvale
     *   zahozena pulka skillu.
     * ⇒ Ted: nejdriv se posoudi, jestli by brneni prolomil hod SAM. Kdyz ano,
     *   MB jde na ZRANENI; kdyz ne, pouzije se na brneni.
     * ⚠️ PREDPOKLAD, ktery pravidla nerozhoduji: volba se dela PO hodu na
     *   brneni. Text neuvadi okamzik rozhodnuti; takhle to hraje vetsina
     *   implementaci a je to pro majitele skillu optimalni.
     */
    public function resolve(
        MatchPlayerDTO $player,
        DiceRollerInterface $dice,
        int $armourModifier = 0,
        int $injuryModifier = 0,
        bool $hasClaw = false,
        bool $hasStakes = false,
        bool $hasNurglesRot = false,
        bool $mightyBlow = false,
    ): array {
        $events = [];

        // Armor roll: 2D6 > AV = armor broken
        $armourRoll = $dice->roll2D6();
        $armourValue = $player->getStats()->getArmour();

        // Prolomil by hod brneni i BEZ Mighty Blow?
        $bezMB = ($hasClaw && $armourRoll >= 8) || (($armourRoll + $armourModifier) > $armourValue);
        if ($mightyBlow && !$bezMB) {
            $armourModifier++;          // MB se utrati na brneni
        } elseif ($mightyBlow) {
            $injuryModifier++;          // brneni padlo samo => MB jde na zraneni
        }

        $modifiedRoll = $armourRoll + $armourModifier;
        // Claw: armor broken on 8+ regardless of AV
        $armourBroken = ($hasClaw && $armourRoll >= 8) || ($modifiedRoll > $armourValue);

        $events[] = GameEvent::armourRoll(
            $player->getId(),
            $armourRoll,
            $armourModifier,
            $armourValue,
            $armourBroken,
        );

        if (!$armourBroken) {
            return ['player' => $player, 'events' => $events];
        }

        return $this->resolveInjury($player, $dice, $injuryModifier, $events, $hasStakes, $hasNurglesRot);
    }

    /**
     * Resolve with a pre-rolled armor value (used by Chainsaw).
     *
     * @return array{player: MatchPlayerDTO, events: list<GameEvent>}
     */
    public function resolveWithPrerolledArmor(
        MatchPlayerDTO $player,
        DiceRollerInterface $dice,
        int $armourRoll,
        int $armourModifier = 0,
        int $injuryModifier = 0,
        bool $hasClaw = false,
        bool $hasStakes = false,
        bool $hasNurglesRot = false,
    ): array {
        $events = [];
        $modifiedRoll = $armourRoll + $armourModifier;
        $armourValue = $player->getStats()->getArmour();
        $armourBroken = ($hasClaw && $armourRoll >= 8) || ($modifiedRoll > $armourValue);

        $events[] = GameEvent::armourRoll(
            $player->getId(),
            $armourRoll,
            $armourModifier,
            $armourValue,
            $armourBroken,
        );

        if (!$armourBroken) {
            return ['player' => $player, 'events' => $events];
        }

        return $this->resolveInjury($player, $dice, $injuryModifier, $events, $hasStakes, $hasNurglesRot);
    }

    /**
     * Resolve injury only (no armor roll). Used by foul when armor is already broken.
     *
     * @return array{player: MatchPlayerDTO, events: list<GameEvent>}
     */
    /**
     * @return array{player: MatchPlayerDTO, events: list<GameEvent>, dice: array{int, int}}
     */
    public function resolveInjuryOnly(
        MatchPlayerDTO $player,
        DiceRollerInterface $dice,
        int $modifier = 0,
        bool $hasStakes = false,
        bool $hasNurglesRot = false,
    ): array {
        return $this->resolveInjury($player, $dice, $modifier, [], $hasStakes, $hasNurglesRot);
    }

    /**
     * Resolve crowd surf injury (bez hodu na brneni, rovnou zraneni, BEZ modifikatoru).
     *
     * ⛔⛔ OPRAVENO 16.09.2026 -- tady bylo `+1`. `rules_bb2016.txt` r. 651-654:
     *   "A player pushed off the pitch, even if Knocked Down, is beaten up only by the
     *   crowd and receives one roll on the Injury Table. THE CROWD DOES NOT HAVE ANY
     *   INJURY MODIFYING SKILLS." ⇒ zadny modifikator, ani +1.
     * ⛔⛔ DOPLNENO 24.09.2026 (balik G) -- `rules_bb2016.txt` r. 655-658:
     *   "If a 'Stunned' result is rolled on the Injury table the player should be
     *   placed in the Reserves box of the Dugout, and must remain there until a
     *   touchdown is scored or the half ends."
     *   ⇒ Stunned po vyhozeni z hriste NENI omraceni NA HRISTI -- hrac jde do rezerv.
     *   ⭐ Stav "rezervy" engine MA: `PlayerState::OFF_PITCH`. Pouziva ho uz navrat
     *   z KO (`GameFlowResolver:149`) i Regeneration nize -- hraci v nem cekaji na
     *   rozestaveni pri dalsim drivu, tedy presne "do touchdownu nebo konce polocasu".
     *   ⛔ Starsi komentar tvrdil, ze ten stav neexistuje. Neplatilo to.
     *
     * @return array{player: MatchPlayerDTO, events: list<GameEvent>}
     */
    public function resolveCrowdSurf(
        MatchPlayerDTO $player,
        DiceRollerInterface $dice,
    ): array {
        $result = $this->resolveInjury($player, $dice, 0, []);

        if ($result['player']->getState() === PlayerState::STUNNED) {
            $result['player'] = $result['player']
                ->withState(PlayerState::OFF_PITCH)
                ->withPosition(null);
        }

        return $result;
    }

    /**
     * Re-roll injury with apothecary (take better result).
     * Returns the better of original and re-rolled results.
     *
     * @param list<GameEvent> $events
     * @return array{player: MatchPlayerDTO, events: list<GameEvent>}
     */
    public function resolveApothecary(
        MatchPlayerDTO $player,
        DiceRollerInterface $dice,
        PlayerState $originalState,
        int $originalModifier,
        array $events,
    ): array {
        // Re-roll injury
        $rerollResult = $this->resolveInjury($player, $dice, $originalModifier, []);
        $rerolledState = $rerollResult['player']->getState();

        // Take the better result (stunned > KO > injured)
        $stateOrder = [
            PlayerState::STUNNED->value => 0,
            PlayerState::KO->value => 1,
            PlayerState::INJURED->value => 2,
        ];

        $originalScore = $stateOrder[$originalState->value] ?? 2;
        $rerolledScore = $stateOrder[$rerolledState->value] ?? 2;

        if ($rerolledScore <= $originalScore) {
            // Reroll is same or better
            $events[] = GameEvent::apothecaryUsed(
                $player->getId(),
                $originalState->value,
                $rerolledState->value,
            );
            $events = array_merge($events, $rerollResult['events']);
            return ['player' => $rerollResult['player'], 'events' => $events];
        }

        // Original was better, keep it
        $events[] = GameEvent::apothecaryUsed(
            $player->getId(),
            $originalState->value,
            $originalState->value,
        );

        // Restore original state on the player
        $restoredPlayer = $player->withState($originalState);
        if ($originalState === PlayerState::KO || $originalState === PlayerState::INJURED) {
            $restoredPlayer = $restoredPlayer->withPosition(null);
        }

        return ['player' => $restoredPlayer, 'events' => $events];
    }

    /**
     * ⭐ 21.09.2026: vraci i JEDNOTLIVE KOSTKY (`dice`). Faul potrebuje poznat
     *   dublet i na hodu na zraneni -- `rules_bb2016.txt` r. 1877-1878:
     *   "if the Armour **and/or Injury** roll is a doubles ... the player taking
     *   the Foul Action is sent off". Do ted tahle metoda vracela jen soucet
     *   `roll2D6()` a kostky zahodila, takze dublet na zraneni nesel precist.
     *   ⚠️ Spotreba kostek je stejna (`roll2D6()` je vsude `rollD6()+rollD6()`),
     *   takze fixtury testu zustavaji platne.
     *
     * @param list<GameEvent> $events
     * @return array{player: MatchPlayerDTO, events: list<GameEvent>, dice: array{int, int}}
     */
    /**
     * Tabulka nasledku (D68) -- `rules_bb2016.txt` r. 2405-2423.
     *
     * ⭐ Je to D68: **D6 da desitky, D8 jednotky**, tedy 48 poli.
     *   11-38 Badly Hurt (24/48 = 50 %) · 41-48 Miss Next Game (8/48) ·
     *   51-52 Niggling · 53-54 -1 MA · 55-56 -1 AV · 57 -1 AG · 58 -1 ST ·
     *   61-68 DEAD (8/48 = kazda sesta casualty).
     *
     * ⛔⛔ DOPLNENO 24.09.2026 (balik G) -- do ted byl kazdy hod 10+ na zraneni
     *   plose `INJURED`, takze **smrt nemohla nastat vubec**. Vzor je C++ engine
     *   (`engine/src/injury.cpp:11`, balik G z 10.08.2026).
     *
     * @return array{result: CasualtyResult, tens: int, units: int}
     */
    public function rollCasualty(DiceRollerInterface $dice): array
    {
        $tens = $dice->rollD6();
        $units = $dice->rollD8();

        $result = match (true) {
            $tens <= 3 => CasualtyResult::BADLY_HURT,
            $tens === 4 => CasualtyResult::MISS_NEXT_GAME,
            $tens === 6 => CasualtyResult::DEAD,
            default => match ($units) {          // tens === 5
                1, 2 => CasualtyResult::NIGGLING,
                3, 4 => CasualtyResult::MA_LOSS,
                5, 6 => CasualtyResult::AV_LOSS,
                7 => CasualtyResult::AG_LOSS,
                default => CasualtyResult::ST_LOSS,
            },
        };

        return ['result' => $result, 'tens' => $tens, 'units' => $units];
    }

    private function resolveInjury(
        MatchPlayerDTO $player,
        DiceRollerInterface $dice,
        int $modifier,
        array $events,
        bool $hasStakes = false,
        bool $hasNurglesRot = false,
    ): array {
        $die1 = $dice->rollD6();
        $die2 = $dice->rollD6();
        $roll = $die1 + $die2;
        // Decay: roll injury twice, take worse (higher) result
        if ($player->hasSkill(SkillName::Decay)) {
            $decayDie1 = $dice->rollD6();
            $decayDie2 = $dice->rollD6();
            $roll2 = $decayDie1 + $decayDie2;
            if ($roll2 > $roll) {
                // Plati horsi hod, a tedy i JEHO kostky (dublet se cte z toho,
                // ktery se pouzil).
                $roll = $roll2;
                $die1 = $decayDie1;
                $die2 = $decayDie2;
            }
        }
        // Stunty: +1 to injury roll (more vulnerable)
        if ($player->hasSkill(SkillName::Stunty)) {
            $modifier++;
        }
        $modified = $roll + $modifier;

        if ($modified <= 7) {
            $player = $player->withState(PlayerState::STUNNED);
            $events[] = GameEvent::injuryRoll($player->getId(), $roll, $modifier, 'stunned');
        } elseif ($modified <= 9) {
            // ⛔⛔ OPRAVENO 15.09.2026 -- Thick Skull tu byl podle JINE EDICE
            //   (pri KO hod D6, na 4+ omracen). `rules_bb2016.txt` r. 8595-8598:
            //   "treats a roll of 8 on the Injury table, after any modifiers have
            //   been applied, as a Stunned result rather than a KO'd result."
            //   ⇒ Modifikovana 8 = Stunned, 9 = KO, zadna kostka navic.
            //   C++ engine to ma spravne (`engine/src/injury.cpp:69`).
            if ($modified === 8 && $player->hasSkill(SkillName::ThickSkull)) {
                $player = $player->withState(PlayerState::STUNNED);
                $events[] = GameEvent::injuryRoll($player->getId(), $roll, $modifier, 'stunned');
            } else {
                $player = $player->withState(PlayerState::KO)->withPosition(null);
                $events[] = GameEvent::injuryRoll($player->getId(), $roll, $modifier, 'ko');
            }
        } else {
            // ⛔⛔ ZMENENO 24.09.2026 (balik G): do ted tu byl plose `INJURED`,
            //   takze DEAD nemohl nastat vubec. Ted se hazi na tabulce nasledku.
            //   ⚠️ V JEDNOM ZAPASE je mezi BADLY_HURT, MISS_NEXT_GAME, NIGGLING
            //   a ztratami vlastnosti rozdil nulovy -- hrac je venku tak jako tak.
            //   Lisi se az v LIZE. Vysledek se proto ZAZNAMENAVA do udalosti,
            //   i kdyz ho zatim nic necte.
            $cas = $this->rollCasualty($dice);
            $player = $player
                ->withState($cas['result'] === CasualtyResult::DEAD ? PlayerState::DEAD : PlayerState::INJURED)
                ->withPosition(null);
            $events[] = GameEvent::injuryRoll($player->getId(), $roll, $modifier, 'casualty');
            $events[] = GameEvent::casualty(
                $player->getId(),
                $cas['tens'],
                $cas['units'],
                $cas['result']->value,
            );

            // Nurgle's Rot: flavor event when attacker has the skill
            if ($hasNurglesRot) {
                $events[] = GameEvent::nurglesRot(0, $player->getId());
            }

            // Regeneration: after casualty, roll D6; on 4+ player goes to reserves
            // Stakes: blocks Regeneration entirely
            if ($player->hasSkill(SkillName::Regeneration) && !$hasStakes) {
                $regenRoll = $dice->rollD6();
                if ($regenRoll >= 4) {
                    $player = $player->withState(PlayerState::OFF_PITCH);
                    $events[] = GameEvent::regeneration($player->getId(), $regenRoll, true);
                } else {
                    $events[] = GameEvent::regeneration($player->getId(), $regenRoll, false);
                }
            } elseif ($player->hasSkill(SkillName::Regeneration) && $hasStakes) {
                $events[] = GameEvent::stakesBlockRegen(0, $player->getId());
            }
        }

        return ['player' => $player, 'events' => $events, 'dice' => [$die1, $die2]];
    }
}
