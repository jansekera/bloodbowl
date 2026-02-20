<?php
declare(strict_types=1);

namespace App\Tests\AI;

use App\AI\FeatureExtractor;
use App\DTO\TeamStateDTO;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use App\Tests\Engine\GameStateBuilder;
use PHPUnit\Framework\TestCase;

final class FeatureExtractorTest extends TestCase
{
    public function testOutputHasCorrectFeatureCount(): void
    {
        $builder = new GameStateBuilder();
        $builder->addPlayer(TeamSide::HOME, 10, 7, id: 1);
        $builder->addPlayer(TeamSide::AWAY, 15, 7, id: 2);
        $state = $builder->build();

        $features = FeatureExtractor::extract($state, TeamSide::HOME);

        $this->assertCount(FeatureExtractor::NUM_FEATURES, $features);
    }

    public function testSymmetry(): void
    {
        $builder = new GameStateBuilder();
        // Symmetric positions
        $builder->addPlayer(TeamSide::HOME, 10, 7, strength: 3, id: 1);
        $builder->addPlayer(TeamSide::AWAY, 15, 7, strength: 3, id: 2);
        $homeTeam = TeamStateDTO::create(1, 'Home', 'Human', TeamSide::HOME, 2);
        $awayTeam = TeamStateDTO::create(2, 'Away', 'Orc', TeamSide::AWAY, 2);
        $builder->withHomeTeam($homeTeam);
        $builder->withAwayTeam($awayTeam);
        $state = $builder->build();

        $homeFeat = FeatureExtractor::extract($state, TeamSide::HOME);
        $awayFeat = FeatureExtractor::extract($state, TeamSide::AWAY);

        // my_players_standing and opp_players_standing should be swapped
        $this->assertEqualsWithDelta($homeFeat[4], $awayFeat[5], 0.001, 'my_standing vs opp_standing swapped');
        $this->assertEqualsWithDelta($homeFeat[5], $awayFeat[4], 0.001, 'opp_standing vs my_standing swapped');

        // Rerolls should be swapped
        $this->assertEqualsWithDelta($homeFeat[10], $awayFeat[11], 0.001, 'my_rerolls vs opp_rerolls');
        $this->assertEqualsWithDelta($homeFeat[11], $awayFeat[10], 0.001, 'opp_rerolls vs my_rerolls');
    }

    public function testScoreDiff(): void
    {
        $builder = new GameStateBuilder();
        $builder->addPlayer(TeamSide::HOME, 10, 7, id: 1);
        $builder->addPlayer(TeamSide::AWAY, 15, 7, id: 2);
        $homeTeam = TeamStateDTO::create(1, 'Home', 'Human', TeamSide::HOME, 3)->withScore(2);
        $awayTeam = TeamStateDTO::create(2, 'Away', 'Orc', TeamSide::AWAY, 2)->withScore(1);
        $builder->withHomeTeam($homeTeam);
        $builder->withAwayTeam($awayTeam);
        $state = $builder->build();

        $features = FeatureExtractor::extract($state, TeamSide::HOME);

        // score_diff = (2-1)/6 ≈ 0.167
        $this->assertEqualsWithDelta(1.0 / 6.0, $features[0], 0.001);
    }

    public function testBallCarrierDistance(): void
    {
        $builder = new GameStateBuilder();
        // AWAY carrier at x=20 → AWAY scores at x=0 → distance = 20
        $builder->addPlayer(TeamSide::AWAY, 20, 7, id: 1);
        $builder->addPlayer(TeamSide::HOME, 5, 7, id: 2);
        $builder->withBallCarried(1);
        $builder->withActiveTeam(TeamSide::AWAY);
        $state = $builder->build();

        $features = FeatureExtractor::extract($state, TeamSide::AWAY);

        // carrier_dist_to_td = 20/26 for AWAY at x=20
        $this->assertEqualsWithDelta(20.0 / 26.0, $features[15], 0.001);
    }

    public function testBiasIsAlwaysOne(): void
    {
        $builder = new GameStateBuilder();
        $builder->addPlayer(TeamSide::HOME, 10, 7, id: 1);
        $state = $builder->build();

        $homeFeatures = FeatureExtractor::extract($state, TeamSide::HOME);
        $awayFeatures = FeatureExtractor::extract($state, TeamSide::AWAY);

        $this->assertSame(1.0, $homeFeatures[29]);
        $this->assertSame(1.0, $awayFeatures[29]);
    }

    public function testMySidelineFeature(): void
    {
        $builder = new GameStateBuilder();
        $builder->addPlayer(TeamSide::HOME, 10, 0, id: 1); // on sideline Y=0
        $builder->addPlayer(TeamSide::HOME, 10, 7, id: 2); // center
        $builder->addPlayer(TeamSide::AWAY, 15, 7, id: 3);
        $state = $builder->build();

        $features = FeatureExtractor::extract($state, TeamSide::HOME);

        // 1 of 2 my players on sideline → 0.5
        $this->assertEqualsWithDelta(0.5, $features[30], 0.001);
    }

    public function testOppSidelineFeature(): void
    {
        $builder = new GameStateBuilder();
        $builder->addPlayer(TeamSide::HOME, 10, 7, id: 1);
        $builder->addPlayer(TeamSide::AWAY, 15, 14, id: 2); // on sideline Y=14
        $state = $builder->build();

        $features = FeatureExtractor::extract($state, TeamSide::HOME);

        // 1 of 1 opponent on sideline → 1.0
        $this->assertEqualsWithDelta(1.0, $features[31], 0.001);
    }

    public function testTurnsRemainingFeature(): void
    {
        $builder = new GameStateBuilder();
        $builder->addPlayer(TeamSide::HOME, 10, 7, id: 1);
        $builder->addPlayer(TeamSide::AWAY, 15, 7, id: 2);
        $homeTeam = TeamStateDTO::create(1, 'Home', 'Human', TeamSide::HOME, 3)->withTurnNumber(1);
        $builder->withHomeTeam($homeTeam);
        $state = $builder->build();

        $features = FeatureExtractor::extract($state, TeamSide::HOME);

        // turnsRemaining = max(0, 9-1)/8 = 1.0
        $this->assertEqualsWithDelta(1.0, $features[32], 0.001);
    }

    public function testScoreAdvantageWithBall(): void
    {
        $builder = new GameStateBuilder();
        $builder->addPlayer(TeamSide::HOME, 10, 7, id: 1);
        $builder->addPlayer(TeamSide::AWAY, 15, 7, id: 2);
        $builder->withBallCarried(1);
        $homeTeam = TeamStateDTO::create(1, 'Home', 'Human', TeamSide::HOME, 3)->withScore(2);
        $awayTeam = TeamStateDTO::create(2, 'Away', 'Orc', TeamSide::AWAY, 2)->withScore(1);
        $builder->withHomeTeam($homeTeam);
        $builder->withAwayTeam($awayTeam);
        $state = $builder->build();

        $features = FeatureExtractor::extract($state, TeamSide::HOME);

        // ahead by 1, have ball → (1+1)/4 = 0.5
        $this->assertEqualsWithDelta(0.5, $features[33], 0.001);
    }

    public function testCarrierNearEndzone(): void
    {
        $builder = new GameStateBuilder();
        // HOME carrier at x=23 → dist to endzone (x=25) = 2 (within 3)
        $builder->addPlayer(TeamSide::HOME, 23, 7, id: 1);
        $builder->addPlayer(TeamSide::AWAY, 5, 7, id: 2);
        $builder->withBallCarried(1);
        $state = $builder->build();

        $features = FeatureExtractor::extract($state, TeamSide::HOME);

        $this->assertEqualsWithDelta(1.0, $features[34], 0.001);
    }

    public function testStallIncentive(): void
    {
        $builder = new GameStateBuilder();
        // HOME carrier at x=23 (near endzone), ahead by 1, turn 1
        $builder->addPlayer(TeamSide::HOME, 23, 7, id: 1);
        $builder->addPlayer(TeamSide::AWAY, 5, 7, id: 2);
        $builder->withBallCarried(1);
        $homeTeam = TeamStateDTO::create(1, 'Home', 'Human', TeamSide::HOME, 3)
            ->withScore(1)->withTurnNumber(1);
        $awayTeam = TeamStateDTO::create(2, 'Away', 'Orc', TeamSide::AWAY, 2)->withScore(0);
        $builder->withHomeTeam($homeTeam);
        $builder->withAwayTeam($awayTeam);
        $state = $builder->build();

        $features = FeatureExtractor::extract($state, TeamSide::HOME);

        // scoreAdvantage = (1+1)/4 = 0.5, turnsRemaining = 8/8 = 1.0, nearEndzone = 1.0
        // stallIncentive = 0.5 * 1.0 * 1.0 = 0.5
        $this->assertEqualsWithDelta(0.5, $features[35], 0.001);
    }

    public function testCarrierTzCount(): void
    {
        $builder = new GameStateBuilder();
        // HOME carrier at (10,7), two AWAY adjacent
        $builder->addPlayer(TeamSide::HOME, 10, 7, id: 1);
        $builder->addPlayer(TeamSide::AWAY, 11, 7, id: 2);
        $builder->addPlayer(TeamSide::AWAY, 10, 8, id: 3);
        $builder->withBallCarried(1);
        $state = $builder->build();

        $features = FeatureExtractor::extract($state, TeamSide::HOME);

        // 2 opp adjacent → 2/4 = 0.5
        $this->assertEqualsWithDelta(0.5, $features[40], 0.001);
    }

    public function testScoringThreat(): void
    {
        $builder = new GameStateBuilder();
        // HOME carrier at x=22, MA=6 → dist=3, MA >= 3 → threat
        $builder->addPlayer(TeamSide::HOME, 22, 7, id: 1, movement: 6);
        $builder->addPlayer(TeamSide::AWAY, 5, 7, id: 2);
        $builder->withBallCarried(1);
        $state = $builder->build();

        $features = FeatureExtractor::extract($state, TeamSide::HOME);

        $this->assertEqualsWithDelta(1.0, $features[41], 0.001);
    }

    public function testScoringThreatNotEnoughMovement(): void
    {
        $builder = new GameStateBuilder();
        // HOME carrier at x=10, MA=6 → dist=15, MA < 15 → no threat
        $builder->addPlayer(TeamSide::HOME, 10, 7, id: 1, movement: 6);
        $builder->addPlayer(TeamSide::AWAY, 15, 7, id: 2);
        $builder->withBallCarried(1);
        $state = $builder->build();

        $features = FeatureExtractor::extract($state, TeamSide::HOME);

        $this->assertEqualsWithDelta(0.0, $features[41], 0.001);
    }

    public function testOppScoringThreat(): void
    {
        $builder = new GameStateBuilder();
        // AWAY carrier at x=3, MA=6 → dist to AWAY endzone (0) = 3, MA >= 3
        $builder->addPlayer(TeamSide::HOME, 20, 7, id: 1);
        $builder->addPlayer(TeamSide::AWAY, 3, 7, id: 2, movement: 6);
        $builder->withBallCarried(2);
        $state = $builder->build();

        $features = FeatureExtractor::extract($state, TeamSide::HOME);

        $this->assertEqualsWithDelta(1.0, $features[42], 0.001);
    }

    public function testEngagedFractions(): void
    {
        $builder = new GameStateBuilder();
        // HOME at (10,7), AWAY at (11,7) → adjacent, both engaged
        $builder->addPlayer(TeamSide::HOME, 10, 7, id: 1);
        $builder->addPlayer(TeamSide::AWAY, 11, 7, id: 2);
        $state = $builder->build();

        $features = FeatureExtractor::extract($state, TeamSide::HOME);

        $this->assertEqualsWithDelta(1.0, $features[43], 0.001); // my_engaged_fraction
        $this->assertEqualsWithDelta(1.0, $features[44], 0.001); // opp_engaged_fraction
    }

    public function testEngagedFractionsNoContact(): void
    {
        $builder = new GameStateBuilder();
        // Far apart → not engaged
        $builder->addPlayer(TeamSide::HOME, 5, 7, id: 1);
        $builder->addPlayer(TeamSide::AWAY, 20, 7, id: 2);
        $state = $builder->build();

        $features = FeatureExtractor::extract($state, TeamSide::HOME);

        $this->assertEqualsWithDelta(0.0, $features[43], 0.001); // my_engaged_fraction
        $this->assertEqualsWithDelta(0.0, $features[44], 0.001); // opp_engaged_fraction
    }

    public function testFreePlayersFeature(): void
    {
        $builder = new GameStateBuilder();
        // Far apart → 1 free home player
        $builder->addPlayer(TeamSide::HOME, 5, 7, id: 1);
        $builder->addPlayer(TeamSide::AWAY, 20, 7, id: 2);
        $state = $builder->build();

        $features = FeatureExtractor::extract($state, TeamSide::HOME);

        // 1 standing, 0 engaged → 1 free / 11
        $this->assertEqualsWithDelta(1.0 / 11.0, $features[47], 0.001);
    }

    public function testFeatureCountIs70(): void
    {
        $this->assertSame(70, FeatureExtractor::NUM_FEATURES);
    }

    public function testBlockSkillFraction(): void
    {
        $builder = new GameStateBuilder();
        // 2 HOME players, 1 with Block
        $builder->addPlayer(TeamSide::HOME, 10, 7, id: 1, skills: [SkillName::Block]);
        $builder->addPlayer(TeamSide::HOME, 10, 6, id: 2);
        $builder->addPlayer(TeamSide::AWAY, 15, 7, id: 3, skills: [SkillName::Block]);
        $state = $builder->build();

        $features = FeatureExtractor::extract($state, TeamSide::HOME);

        // my_block_skill_count: 1/2 = 0.5
        $this->assertEqualsWithDelta(0.5, $features[48], 0.001);
        // opp_block_skill_count: 1/1 = 1.0
        $this->assertEqualsWithDelta(1.0, $features[49], 0.001);
    }

    public function testDodgeSkillFraction(): void
    {
        $builder = new GameStateBuilder();
        // All HOME players with Dodge (Amazon-like)
        $builder->addPlayer(TeamSide::HOME, 10, 7, id: 1, skills: [SkillName::Dodge]);
        $builder->addPlayer(TeamSide::HOME, 10, 6, id: 2, skills: [SkillName::Dodge]);
        $builder->addPlayer(TeamSide::AWAY, 15, 7, id: 3);
        $state = $builder->build();

        $features = FeatureExtractor::extract($state, TeamSide::HOME);

        // my_dodge_skill_count: 2/2 = 1.0
        $this->assertEqualsWithDelta(1.0, $features[50], 0.001);
        // opp_dodge_skill_count: 0/1 = 0.0
        $this->assertEqualsWithDelta(0.0, $features[51], 0.001);
    }

    public function testGuardAndMightyBlowFractions(): void
    {
        $builder = new GameStateBuilder();
        $builder->addPlayer(TeamSide::HOME, 10, 7, id: 1, skills: [SkillName::Guard, SkillName::MightyBlow]);
        $builder->addPlayer(TeamSide::HOME, 10, 6, id: 2);
        $builder->addPlayer(TeamSide::AWAY, 15, 7, id: 3);
        $state = $builder->build();

        $features = FeatureExtractor::extract($state, TeamSide::HOME);

        // my_guard_count: 1/2 = 0.5
        $this->assertEqualsWithDelta(0.5, $features[52], 0.001);
        // my_mighty_blow_count: 1/2 = 0.5
        $this->assertEqualsWithDelta(0.5, $features[53], 0.001);
    }

    public function testClawAndRegenFractions(): void
    {
        $builder = new GameStateBuilder();
        $builder->addPlayer(TeamSide::HOME, 10, 7, id: 1, skills: [SkillName::Claw, SkillName::Regeneration]);
        $builder->addPlayer(TeamSide::HOME, 10, 6, id: 2, skills: [SkillName::Regeneration]);
        $builder->addPlayer(TeamSide::AWAY, 15, 7, id: 3);
        $state = $builder->build();

        $features = FeatureExtractor::extract($state, TeamSide::HOME);

        // my_claw_count: 1/2 = 0.5
        $this->assertEqualsWithDelta(0.5, $features[54], 0.001);
        // my_regen_count: 2/2 = 1.0 (fraction of ALL team players)
        $this->assertEqualsWithDelta(1.0, $features[55], 0.001);
    }

    public function testSkillFeaturesZeroWithNoSkills(): void
    {
        $builder = new GameStateBuilder();
        $builder->addPlayer(TeamSide::HOME, 10, 7, id: 1);
        $builder->addPlayer(TeamSide::AWAY, 15, 7, id: 2);
        $state = $builder->build();

        $features = FeatureExtractor::extract($state, TeamSide::HOME);

        // All skill fractions should be 0
        for ($i = 48; $i <= 55; $i++) {
            $this->assertEqualsWithDelta(0.0, $features[$i], 0.001, "Feature $i should be 0 with no skills");
        }
    }
}
