<?php
declare(strict_types=1);

namespace App\AI;

use App\DTO\GameState;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use App\Enum\Weather;

final class FeatureExtractor
{
    public const NUM_FEATURES = 70;

    /**
     * Extract normalized features from game state for a given perspective.
     *
     * @return list<float>
     */
    public static function extract(GameState $state, TeamSide $perspective): array
    {
        $opp = $perspective->opponent();
        $myTeam = $state->getTeamState($perspective);
        $oppTeam = $state->getTeamState($opp);

        $myPlayers = $state->getTeamPlayers($perspective);
        $oppPlayers = $state->getTeamPlayers($opp);

        $myOnPitch = $state->getPlayersOnPitch($perspective);
        $oppOnPitch = $state->getPlayersOnPitch($opp);

        // Count by state
        $myStanding = 0;
        $myKo = 0;
        $myInjured = 0;
        foreach ($myPlayers as $p) {
            match ($p->getState()) {
                PlayerState::STANDING => $myStanding++,
                PlayerState::KO => $myKo++,
                PlayerState::INJURED, PlayerState::DEAD => $myInjured++,
                default => null,
            };
        }

        $oppStanding = 0;
        $oppKo = 0;
        $oppInjured = 0;
        foreach ($oppPlayers as $p) {
            match ($p->getState()) {
                PlayerState::STANDING => $oppStanding++,
                PlayerState::KO => $oppKo++,
                PlayerState::INJURED, PlayerState::DEAD => $oppInjured++,
                default => null,
            };
        }

        // Ball info
        $ball = $state->getBall();
        $iHaveBall = 0.0;
        $oppHasBall = 0.0;
        $ballOnGround = 0.0;
        $carrierDistToTd = 0.5; // default: midfield
        $ballInMyHalf = 0.0;

        if ($ball->isHeld() && $ball->getCarrierId() !== null) {
            $carrier = $state->getPlayer($ball->getCarrierId());
            if ($carrier !== null) {
                if ($carrier->getTeamSide() === $perspective) {
                    $iHaveBall = 1.0;
                } else {
                    $oppHasBall = 1.0;
                }
                $pos = $carrier->getPosition();
                if ($pos !== null) {
                    $carrierDistToTd = self::distanceToEndzone($pos->getX(), $perspective) / 26.0;
                    $ballInMyHalf = self::isInMyHalf($pos->getX(), $perspective) ? 1.0 : 0.0;
                }
            }
        } elseif ($ball->isOnPitch()) {
            $ballOnGround = 1.0;
            $ballPos = $ball->getPosition();
            if ($ballPos !== null) {
                $ballInMyHalf = self::isInMyHalf($ballPos->getX(), $perspective) ? 1.0 : 0.0;
            }
        }

        // Average positions and strength (standing players only)
        $myAvgX = 0.0;
        $oppAvgX = 0.0;
        $myAvgStr = 0.0;
        $oppAvgStr = 0.0;
        $myAvgArmour = 0.0;
        $oppAvgArmour = 0.0;
        $myAvgAgility = 0.0;
        $oppAvgAgility = 0.0;

        $myStandingOnPitch = [];
        $oppStandingOnPitch = [];
        foreach ($myOnPitch as $p) {
            if ($p->getState() === PlayerState::STANDING && $p->getPosition() !== null) {
                $myStandingOnPitch[] = $p;
            }
        }
        foreach ($oppOnPitch as $p) {
            if ($p->getState() === PlayerState::STANDING && $p->getPosition() !== null) {
                $oppStandingOnPitch[] = $p;
            }
        }

        if ($myStandingOnPitch !== []) {
            $sumX = 0.0;
            $sumStr = 0.0;
            $sumArmour = 0.0;
            $sumAgility = 0.0;
            foreach ($myStandingOnPitch as $p) {
                $sumX += self::normalizeX($p->getPosition()->getX(), $perspective);
                $sumStr += $p->getStats()->getStrength();
                $sumArmour += $p->getStats()->getArmour();
                $sumAgility += $p->getStats()->getAgility();
            }
            $count = count($myStandingOnPitch);
            $myAvgX = ($sumX / $count) / 26.0;
            $myAvgStr = ($sumStr / $count) / 5.0;
            $myAvgArmour = ($sumArmour / $count) / 10.0;
            $myAvgAgility = ($sumAgility / $count) / 5.0;
        }

        if ($oppStandingOnPitch !== []) {
            $sumX = 0.0;
            $sumStr = 0.0;
            $sumArmour = 0.0;
            $sumAgility = 0.0;
            foreach ($oppStandingOnPitch as $p) {
                $sumX += self::normalizeX($p->getPosition()->getX(), $perspective);
                $sumStr += $p->getStats()->getStrength();
                $sumArmour += $p->getStats()->getArmour();
                $sumAgility += $p->getStats()->getAgility();
            }
            $count = count($oppStandingOnPitch);
            $oppAvgX = ($sumX / $count) / 26.0;
            $oppAvgStr = ($sumStr / $count) / 5.0;
            $oppAvgArmour = ($sumArmour / $count) / 10.0;
            $oppAvgAgility = ($sumAgility / $count) / 5.0;
        }

        // Cage count: my players adjacent to my ball carrier
        $myCageCount = 0.0;
        if ($iHaveBall > 0 && $ball->getCarrierId() !== null) {
            $carrier = $state->getPlayer($ball->getCarrierId());
            if ($carrier !== null && $carrier->getPosition() !== null) {
                $carrierPos = $carrier->getPosition();
                foreach ($myOnPitch as $p) {
                    if ($p->getId() === $carrier->getId()) {
                        continue;
                    }
                    $pPos = $p->getPosition();
                    if ($pPos !== null && $carrierPos->distanceTo($pPos) === 1) {
                        $myCageCount += 1.0;
                    }
                }
            }
        }

        // Receiving team
        $kickingTeam = $state->getKickingTeam();
        $isReceiving = ($kickingTeam !== null && $kickingTeam !== $perspective) ? 1.0 : 0.0;

        // Turn progress
        $turnNumber = $myTeam->getTurnNumber();
        $halfNum = $state->getHalf();
        $turnProgress = ($turnNumber + ($halfNum - 1) * 8) / 16.0;
        $turnProgress = min(1.0, $turnProgress);

        // Weather
        $weather = $state->getWeather();

        // Sideline awareness: fraction of standing players on Y=0 or Y=14
        $mySidelineCount = 0;
        foreach ($myStandingOnPitch as $p) {
            $y = $p->getPosition()->getY();
            if ($y === 0 || $y === 14) {
                $mySidelineCount++;
            }
        }
        $oppSidelineCount = 0;
        foreach ($oppStandingOnPitch as $p) {
            $y = $p->getPosition()->getY();
            if ($y === 0 || $y === 14) {
                $oppSidelineCount++;
            }
        }
        $mySidelineFraction = $myStandingOnPitch !== [] ? $mySidelineCount / count($myStandingOnPitch) : 0.0;
        $oppSidelineFraction = $oppStandingOnPitch !== [] ? $oppSidelineCount / count($oppStandingOnPitch) : 0.0;

        // Turns remaining (normalized)
        $turnsRemaining = max(0, 9 - $turnNumber) / 8.0;

        // Score advantage with ball
        $scoreDiff = $myTeam->getScore() - $oppTeam->getScore();
        $scoreAdvantageWithBall = ($scoreDiff >= 0 && $iHaveBall > 0)
            ? min(($scoreDiff + 1) / 4.0, 1.0)
            : 0.0;

        // Carrier near endzone (within 3 squares of TD)
        $carrierNearEndzone = 0.0;
        if ($iHaveBall > 0 && $ball->getCarrierId() !== null) {
            $carrier = $state->getPlayer($ball->getCarrierId());
            if ($carrier !== null && $carrier->getPosition() !== null) {
                $distToTd = self::distanceToEndzone($carrier->getPosition()->getX(), $perspective);
                if ($distToTd <= 3) {
                    $carrierNearEndzone = 1.0;
                }
            }
        }

        // Stall incentive (compound: scoreAdvantage * turnsRemaining * nearEndzone)
        $stallIncentive = $scoreAdvantageWithBall * $turnsRemaining * $carrierNearEndzone;

        // Carrier tackle zone count: opp standing adjacent to my carrier
        $carrierTzCount = 0.0;
        if ($iHaveBall > 0 && $ball->getCarrierId() !== null) {
            $carrier = $state->getPlayer($ball->getCarrierId());
            if ($carrier !== null && $carrier->getPosition() !== null) {
                $cPos = $carrier->getPosition();
                foreach ($oppStandingOnPitch as $opp_p) {
                    $oppPos = $opp_p->getPosition();
                    if (max(abs($oppPos->getX() - $cPos->getX()), abs($oppPos->getY() - $cPos->getY())) === 1) {
                        $carrierTzCount += 1.0;
                    }
                }
            }
        }

        // Scoring threat: my carrier MA >= distance to endzone
        $scoringThreat = 0.0;
        if ($iHaveBall > 0 && $ball->getCarrierId() !== null) {
            $carrier = $state->getPlayer($ball->getCarrierId());
            if ($carrier !== null && $carrier->getPosition() !== null) {
                $distToTd2 = self::distanceToEndzone($carrier->getPosition()->getX(), $perspective);
                if ($carrier->getStats()->getMovement() >= $distToTd2) {
                    $scoringThreat = 1.0;
                }
            }
        }

        // Opp scoring threat: opp carrier MA >= distance to endzone
        $oppScoringThreat = 0.0;
        if ($oppHasBall > 0 && $ball->getCarrierId() !== null) {
            $carrier = $state->getPlayer($ball->getCarrierId());
            if ($carrier !== null && $carrier->getPosition() !== null) {
                $distToTd2 = self::distanceToEndzone($carrier->getPosition()->getX(), $opp);
                if ($carrier->getStats()->getMovement() >= $distToTd2) {
                    $oppScoringThreat = 1.0;
                }
            }
        }

        // Engaged fractions: standing players in opponent TZ (Chebyshev dist=1)
        $myEngaged = 0;
        foreach ($myStandingOnPitch as $p) {
            $pPos = $p->getPosition();
            foreach ($oppStandingOnPitch as $opp_p) {
                $oppPos = $opp_p->getPosition();
                if (max(abs($oppPos->getX() - $pPos->getX()), abs($oppPos->getY() - $pPos->getY())) === 1) {
                    $myEngaged++;
                    break;
                }
            }
        }
        $oppEngaged = 0;
        foreach ($oppStandingOnPitch as $opp_p) {
            $oppPos = $opp_p->getPosition();
            foreach ($myStandingOnPitch as $p) {
                $pPos = $p->getPosition();
                if (max(abs($pPos->getX() - $oppPos->getX()), abs($pPos->getY() - $oppPos->getY())) === 1) {
                    $oppEngaged++;
                    break;
                }
            }
        }
        $myEngagedFraction = $myStandingOnPitch !== [] ? $myEngaged / count($myStandingOnPitch) : 0.0;
        $oppEngagedFraction = $oppStandingOnPitch !== [] ? $oppEngaged / count($oppStandingOnPitch) : 0.0;

        // Prone/stunned counts
        $myProneStunned = 0;
        foreach ($myOnPitch as $p) {
            if ($p->getState() === PlayerState::PRONE || $p->getState() === PlayerState::STUNNED) {
                $myProneStunned++;
            }
        }
        $oppProneStunned = 0;
        foreach ($oppOnPitch as $p) {
            if ($p->getState() === PlayerState::PRONE || $p->getState() === PlayerState::STUNNED) {
                $oppProneStunned++;
            }
        }

        // Free players: my standing NOT in any opp TZ
        $myFreePlayers = count($myStandingOnPitch) - $myEngaged;

        // Skill fractions for standing players
        $myBlockCount = 0;
        $oppBlockCount = 0;
        $myDodgeCount = 0;
        $oppDodgeCount = 0;
        $myGuardCount = 0;
        $myMightyBlowCount = 0;
        $myClawCount = 0;

        foreach ($myStandingOnPitch as $p) {
            if ($p->hasSkill(SkillName::Block)) { $myBlockCount++; }
            if ($p->hasSkill(SkillName::Dodge)) { $myDodgeCount++; }
            if ($p->hasSkill(SkillName::Guard)) { $myGuardCount++; }
            if ($p->hasSkill(SkillName::MightyBlow)) { $myMightyBlowCount++; }
            if ($p->hasSkill(SkillName::Claw)) { $myClawCount++; }
        }
        foreach ($oppStandingOnPitch as $p) {
            if ($p->hasSkill(SkillName::Block)) { $oppBlockCount++; }
            if ($p->hasSkill(SkillName::Dodge)) { $oppDodgeCount++; }
        }

        // Regen count: fraction of ALL team players (not just standing) with Regeneration
        $myRegenCount = 0;
        foreach ($myPlayers as $p) {
            if ($p->hasSkill(SkillName::Regeneration)) { $myRegenCount++; }
        }

        $myStandingCount = count($myStandingOnPitch);
        $oppStandingCount = count($oppStandingOnPitch);
        $myTotalCount = count($myPlayers);

        // === NEW strategic pattern features [56-69] ===

        // [56] cage_diagonal_quality: 4 diagonal corners around my carrier
        $cageDiagonal = 0;
        if ($iHaveBall > 0 && $ball->getCarrierId() !== null) {
            $carrier = $state->getPlayer($ball->getCarrierId());
            if ($carrier !== null && $carrier->getPosition() !== null) {
                $cx = $carrier->getPosition()->getX();
                $cy = $carrier->getPosition()->getY();
                $diags = [[$cx-1,$cy-1], [$cx+1,$cy-1], [$cx-1,$cy+1], [$cx+1,$cy+1]];
                foreach ($diags as [$dx, $dy]) {
                    if ($dx < 0 || $dx > 25 || $dy < 0 || $dy > 14) continue;
                    foreach ($myStandingOnPitch as $p) {
                        if ($p->getPosition()->getX() === $dx && $p->getPosition()->getY() === $dy) {
                            $cageDiagonal++;
                            break;
                        }
                    }
                }
            }
        }

        // [57] cage_overload_risk: >4 adjacent = chain push risk
        $cageOverloadRisk = 0.0;
        if ($iHaveBall > 0) {
            $cageOverloadRisk = max(0.0, min(1.0, ($myCageCount - 4) / 4.0));
        }

        // [58] opp_cage_diagonal_quality
        $oppCageDiagonal = 0;
        if ($oppHasBall > 0 && $ball->getCarrierId() !== null) {
            $carrier = $state->getPlayer($ball->getCarrierId());
            if ($carrier !== null && $carrier->getPosition() !== null) {
                $cx = $carrier->getPosition()->getX();
                $cy = $carrier->getPosition()->getY();
                $diags = [[$cx-1,$cy-1], [$cx+1,$cy-1], [$cx-1,$cy+1], [$cx+1,$cy+1]];
                foreach ($diags as [$dx, $dy]) {
                    if ($dx < 0 || $dx > 25 || $dy < 0 || $dy > 14) continue;
                    foreach ($oppStandingOnPitch as $p) {
                        if ($p->getPosition()->getX() === $dx && $p->getPosition()->getY() === $dy) {
                            $oppCageDiagonal++;
                            break;
                        }
                    }
                }
            }
        }

        // [59] carrier_can_score: MA+2GFI >= dist to endzone
        $carrierCanScore = 0.0;
        if ($iHaveBall > 0 && $ball->getCarrierId() !== null) {
            $carrier = $state->getPlayer($ball->getCarrierId());
            if ($carrier !== null && $carrier->getPosition() !== null) {
                $dist = self::distanceToEndzone($carrier->getPosition()->getX(), $perspective);
                if ($carrier->getStats()->getMovement() + 2 >= $dist) {
                    $carrierCanScore = 1.0;
                }
            }
        }

        // [60] pass_scoring_threat: teammates within pass range who could score
        $passThreats = 0;
        if ($iHaveBall > 0 && $ball->getCarrierId() !== null) {
            $carrier = $state->getPlayer($ball->getCarrierId());
            if ($carrier !== null && $carrier->getPosition() !== null) {
                $cPos = $carrier->getPosition();
                foreach ($myStandingOnPitch as $p) {
                    if ($p->getId() === $carrier->getId()) continue;
                    $pPos = $p->getPosition();
                    $distToCarrier = max(abs($pPos->getX() - $cPos->getX()), abs($pPos->getY() - $cPos->getY()));
                    if ($distToCarrier <= 10) {
                        $distTD = self::distanceToEndzone($pPos->getX(), $perspective);
                        if ($p->getStats()->getMovement() + 2 >= $distTD) {
                            $passThreats++;
                        }
                    }
                }
            }
        }

        // [61] frenzy_trap_risk: frenzy players adjacent to 2+ opponents
        $frenzyTraps = 0;
        $myFrenzyCount = 0;
        foreach ($myStandingOnPitch as $p) {
            if (!$p->hasSkill(SkillName::Frenzy)) continue;
            $myFrenzyCount++;
            $adjOpp = 0;
            $pPos = $p->getPosition();
            foreach ($oppStandingOnPitch as $opp_p) {
                $oppPos = $opp_p->getPosition();
                if (max(abs($oppPos->getX() - $pPos->getX()), abs($oppPos->getY() - $pPos->getY())) === 1) {
                    $adjOpp++;
                }
            }
            if ($adjOpp >= 2) $frenzyTraps++;
        }

        // [62] screen_between_ball: my players between opp carrier and my endzone
        $screenCount = 0;
        if ($oppHasBall > 0 && $ball->getCarrierId() !== null) {
            $carrier = $state->getPlayer($ball->getCarrierId());
            if ($carrier !== null && $carrier->getPosition() !== null) {
                $ballX = $carrier->getPosition()->getX();
                foreach ($myStandingOnPitch as $p) {
                    $px = $p->getPosition()->getX();
                    if ($perspective === TeamSide::HOME) {
                        if ($px < $ballX) $screenCount++;
                    } else {
                        if ($px > $ballX) $screenCount++;
                    }
                }
            }
        }

        // [63] carrier_blitzable: any opp can reach my carrier (chebyshev <= MA)
        $carrierBlitzable = 0.0;
        if ($iHaveBall > 0 && $ball->getCarrierId() !== null) {
            $carrier = $state->getPlayer($ball->getCarrierId());
            if ($carrier !== null && $carrier->getPosition() !== null) {
                $cPos = $carrier->getPosition();
                foreach ($oppStandingOnPitch as $opp_p) {
                    $oppPos = $opp_p->getPosition();
                    $dist = max(abs($oppPos->getX() - $cPos->getX()), abs($oppPos->getY() - $cPos->getY()));
                    if ($dist <= $opp_p->getStats()->getMovement()) {
                        $carrierBlitzable = 1.0;
                        break;
                    }
                }
            }
        }

        // [64] surfable_opponents: opponents on sideline within blitz range
        $surfableOpps = 0;
        foreach ($oppStandingOnPitch as $opp_p) {
            $oy = $opp_p->getPosition()->getY();
            if ($oy !== 0 && $oy !== 14) continue;
            foreach ($myStandingOnPitch as $p) {
                $dist = max(
                    abs($p->getPosition()->getX() - $opp_p->getPosition()->getX()),
                    abs($p->getPosition()->getY() - $opp_p->getPosition()->getY())
                );
                if ($dist <= $p->getStats()->getMovement()) {
                    $surfableOpps++;
                    break;
                }
            }
        }

        // [65] favorable_blocks: players with 2+ dice block available
        $favorableBlocks = 0;
        foreach ($myStandingOnPitch as $p) {
            $pPos = $p->getPosition();
            foreach ($oppStandingOnPitch as $opp_p) {
                $oppPos = $opp_p->getPosition();
                if (max(abs($oppPos->getX() - $pPos->getX()), abs($oppPos->getY() - $pPos->getY())) !== 1) continue;
                // Simplified: count my adjacent (excl attacker) - opp adjacent (excl defender)
                $myAssist = 0;
                $oppAssist = 0;
                foreach ($myStandingOnPitch as $helper) {
                    if ($helper->getId() === $p->getId()) continue;
                    $hPos = $helper->getPosition();
                    if (max(abs($hPos->getX() - $oppPos->getX()), abs($hPos->getY() - $oppPos->getY())) === 1) {
                        // Check if helper is free from enemy TZ (excluding defender)
                        $helperInOppTz = false;
                        foreach ($oppStandingOnPitch as $check) {
                            if ($check->getId() === $opp_p->getId()) continue;
                            $cPos2 = $check->getPosition();
                            if (max(abs($cPos2->getX() - $hPos->getX()), abs($cPos2->getY() - $hPos->getY())) === 1) {
                                if (!$helper->hasSkill(SkillName::Guard)) {
                                    $helperInOppTz = true;
                                    break;
                                }
                            }
                        }
                        if (!$helperInOppTz) $myAssist++;
                    }
                }
                foreach ($oppStandingOnPitch as $helper) {
                    if ($helper->getId() === $opp_p->getId()) continue;
                    $hPos = $helper->getPosition();
                    if (max(abs($hPos->getX() - $pPos->getX()), abs($hPos->getY() - $pPos->getY())) === 1) {
                        $helperInMyTz = false;
                        foreach ($myStandingOnPitch as $check) {
                            if ($check->getId() === $p->getId()) continue;
                            $cPos2 = $check->getPosition();
                            if (max(abs($cPos2->getX() - $hPos->getX()), abs($cPos2->getY() - $hPos->getY())) === 1) {
                                if (!$helper->hasSkill(SkillName::Guard)) {
                                    $helperInMyTz = true;
                                    break;
                                }
                            }
                        }
                        if (!$helperInMyTz) $oppAssist++;
                    }
                }
                $attST = $p->getStats()->getStrength() + $myAssist;
                $defST = $opp_p->getStats()->getStrength() + $oppAssist;
                if ($attST > $defST) {
                    $favorableBlocks++;
                    break; // count each attacker once
                }
                break; // only check first adjacent
            }
        }

        // [66] one_turn_td_vulnerability: opp can score in one turn (MA+2 >= dist, not in TZ)
        $oneTurnTDVuln = 0.0;
        foreach ($oppStandingOnPitch as $opp_p) {
            $dist = self::distanceToEndzone($opp_p->getPosition()->getX(), $opp);
            if ($opp_p->getStats()->getMovement() + 2 >= $dist) {
                // Check if in my TZ
                $inMyTZ = false;
                foreach ($myStandingOnPitch as $p) {
                    $d = max(
                        abs($p->getPosition()->getX() - $opp_p->getPosition()->getX()),
                        abs($p->getPosition()->getY() - $opp_p->getPosition()->getY())
                    );
                    if ($d === 1) { $inMyTZ = true; break; }
                }
                if (!$inMyTZ) {
                    $oneTurnTDVuln = 1.0;
                    break;
                }
            }
        }

        // [67] loose_ball_proximity: who's closer (1.0 = I'm much closer)
        $looseBallProx = 0.5;
        if ($ballOnGround > 0 && $ball->isOnPitch() && $ball->getPosition() !== null) {
            $bPos = $ball->getPosition();
            $myClosest = 99;
            $oppClosest = 99;
            foreach ($myStandingOnPitch as $p) {
                $d = max(abs($p->getPosition()->getX() - $bPos->getX()), abs($p->getPosition()->getY() - $bPos->getY()));
                if ($d < $myClosest) $myClosest = $d;
            }
            foreach ($oppStandingOnPitch as $p) {
                $d = max(abs($p->getPosition()->getX() - $bPos->getX()), abs($p->getPosition()->getY() - $bPos->getY()));
                if ($d < $oppClosest) $oppClosest = $d;
            }
            $looseBallProx = self::clamp(($oppClosest - $myClosest + 5) / 10.0, 0.0, 1.0);
        }

        // [68] deep_safety_count: my players behind deepest opponent penetration
        $deepSafeties = 0;
        if ($oppStandingOnPitch !== []) {
            $minOppDistToMyEZ = 99;
            foreach ($oppStandingOnPitch as $p) {
                $dToMyEZ = ($perspective === TeamSide::HOME)
                    ? $p->getPosition()->getX()
                    : (25 - $p->getPosition()->getX());
                if ($dToMyEZ < $minOppDistToMyEZ) $minOppDistToMyEZ = $dToMyEZ;
            }
            foreach ($myStandingOnPitch as $p) {
                $dToMyEZ = ($perspective === TeamSide::HOME)
                    ? $p->getPosition()->getX()
                    : (25 - $p->getPosition()->getX());
                if ($dToMyEZ < $minOppDistToMyEZ) $deepSafeties++;
            }
        }

        // [69] isolation_count: players with no friendly within 3 squares
        $isolatedCount = 0;
        foreach ($myStandingOnPitch as $p) {
            $hasNearby = false;
            foreach ($myStandingOnPitch as $other) {
                if ($other->getId() === $p->getId()) continue;
                $d = max(
                    abs($other->getPosition()->getX() - $p->getPosition()->getX()),
                    abs($other->getPosition()->getY() - $p->getPosition()->getY())
                );
                if ($d <= 3) { $hasNearby = true; break; }
            }
            if (!$hasNearby) $isolatedCount++;
        }

        return [
            /* 0  score_diff */          self::clamp(($myTeam->getScore() - $oppTeam->getScore()) / 6.0, -1.0, 1.0),
            /* 1  my_score */            min($myTeam->getScore() / 4.0, 1.0),
            /* 2  opp_score */           min($oppTeam->getScore() / 4.0, 1.0),
            /* 3  turn_progress */       $turnProgress,
            /* 4  my_players_standing */ $myStanding / 11.0,
            /* 5  opp_players_standing */$oppStanding / 11.0,
            /* 6  my_players_ko */       $myKo / 11.0,
            /* 7  opp_players_ko */      $oppKo / 11.0,
            /* 8  my_players_injured */  $myInjured / 11.0,
            /* 9  opp_players_injured */ $oppInjured / 11.0,
            /* 10 my_rerolls */          min($myTeam->getRerolls() / 4.0, 1.0),
            /* 11 opp_rerolls */         min($oppTeam->getRerolls() / 4.0, 1.0),
            /* 12 i_have_ball */         $iHaveBall,
            /* 13 opp_has_ball */        $oppHasBall,
            /* 14 ball_on_ground */      $ballOnGround,
            /* 15 carrier_dist_to_td */  $carrierDistToTd,
            /* 16 ball_in_my_half */     $ballInMyHalf,
            /* 17 my_avg_x */           $myAvgX,
            /* 18 opp_avg_x */          $oppAvgX,
            /* 19 my_avg_strength */     $myAvgStr,
            /* 20 opp_avg_strength */    $oppAvgStr,
            /* 21 my_cage_count */       min($myCageCount / 4.0, 1.0),
            /* 22 is_receiving */        $isReceiving,
            /* 23 is_my_turn */          ($state->getActiveTeam() === $perspective) ? 1.0 : 0.0,
            /* 24 weather_nice */        ($weather === Weather::NICE) ? 1.0 : 0.0,
            /* 25 weather_rain */        ($weather === Weather::POURING_RAIN) ? 1.0 : 0.0,
            /* 26 weather_blizzard */    ($weather === Weather::BLIZZARD) ? 1.0 : 0.0,
            /* 27 my_blitz_available */  (!$myTeam->isBlitzUsedThisTurn()) ? 1.0 : 0.0,
            /* 28 my_pass_available */   (!$myTeam->isPassUsedThisTurn()) ? 1.0 : 0.0,
            /* 29 bias */                1.0,
            /* 30 my_players_on_sideline */ $mySidelineFraction,
            /* 31 opp_players_on_sideline */ $oppSidelineFraction,
            /* 32 my_turns_remaining */  $turnsRemaining,
            /* 33 score_advantage_with_ball */ $scoreAdvantageWithBall,
            /* 34 carrier_near_endzone */ $carrierNearEndzone,
            /* 35 stall_incentive */     $stallIncentive,
            /* 36 my_avg_armour */       $myAvgArmour,
            /* 37 opp_avg_armour */      $oppAvgArmour,
            /* 38 my_avg_agility */      $myAvgAgility,
            /* 39 opp_avg_agility */     $oppAvgAgility,
            /* 40 carrier_tz_count */    min($carrierTzCount / 4.0, 1.0),
            /* 41 scoring_threat */      $scoringThreat,
            /* 42 opp_scoring_threat */  $oppScoringThreat,
            /* 43 my_engaged_fraction */ $myEngagedFraction,
            /* 44 opp_engaged_fraction */$oppEngagedFraction,
            /* 45 my_prone_stunned */    $myProneStunned / 11.0,
            /* 46 opp_prone_stunned */   $oppProneStunned / 11.0,
            /* 47 my_free_players */     $myFreePlayers / 11.0,
            /* 48 my_block_skill_count */  $myStandingCount > 0 ? $myBlockCount / $myStandingCount : 0.0,
            /* 49 opp_block_skill_count */ $oppStandingCount > 0 ? $oppBlockCount / $oppStandingCount : 0.0,
            /* 50 my_dodge_skill_count */  $myStandingCount > 0 ? $myDodgeCount / $myStandingCount : 0.0,
            /* 51 opp_dodge_skill_count */ $oppStandingCount > 0 ? $oppDodgeCount / $oppStandingCount : 0.0,
            /* 52 my_guard_count */        $myStandingCount > 0 ? $myGuardCount / $myStandingCount : 0.0,
            /* 53 my_mighty_blow_count */  $myStandingCount > 0 ? $myMightyBlowCount / $myStandingCount : 0.0,
            /* 54 my_claw_count */         $myStandingCount > 0 ? $myClawCount / $myStandingCount : 0.0,
            /* 55 my_regen_count */        $myTotalCount > 0 ? $myRegenCount / $myTotalCount : 0.0,
            /* 56 cage_diagonal_quality */ $cageDiagonal / 4.0,
            /* 57 cage_overload_risk */    $cageOverloadRisk,
            /* 58 opp_cage_diagonal */     $oppCageDiagonal / 4.0,
            /* 59 carrier_can_score */     $carrierCanScore,
            /* 60 pass_scoring_threat */   min($passThreats / 3.0, 1.0),
            /* 61 frenzy_trap_risk */      $myFrenzyCount > 0 ? $frenzyTraps / $myFrenzyCount : 0.0,
            /* 62 screen_between_ball */   $oppHasBall > 0 ? min($screenCount / 5.0, 1.0) : 0.0,
            /* 63 carrier_blitzable */     $carrierBlitzable,
            /* 64 surfable_opponents */    min($surfableOpps / 3.0, 1.0),
            /* 65 favorable_blocks */      $myStandingCount > 0 ? min($favorableBlocks / $myStandingCount, 1.0) : 0.0,
            /* 66 one_turn_td_vuln */      $oneTurnTDVuln,
            /* 67 loose_ball_proximity */  $looseBallProx,
            /* 68 deep_safety_count */     min($deepSafeties / 3.0, 1.0),
            /* 69 isolation_count */       $myStandingCount > 0 ? $isolatedCount / $myStandingCount : 0.0,
        ];
    }

    /**
     * Normalize x coordinate from perspective: 0 = my endzone, 25 = opponent's endzone.
     */
    private static function normalizeX(int $x, TeamSide $perspective): float
    {
        // HOME endzone is x=0, scores at x=25
        // AWAY endzone is x=25, scores at x=0
        if ($perspective === TeamSide::AWAY) {
            return (float) (25 - $x);
        }
        return (float) $x;
    }

    /**
     * Distance to opponent's endzone (where we score).
     */
    private static function distanceToEndzone(int $x, TeamSide $perspective): int
    {
        if ($perspective === TeamSide::HOME) {
            return 25 - $x; // HOME scores at x=25
        }
        return $x; // AWAY scores at x=0
    }

    /**
     * Is position in my half (closer to my endzone)?
     */
    private static function isInMyHalf(int $x, TeamSide $perspective): bool
    {
        if ($perspective === TeamSide::HOME) {
            return $x <= 12;
        }
        return $x >= 13;
    }

    private static function clamp(float $value, float $min, float $max): float
    {
        return max($min, min($max, $value));
    }
}
