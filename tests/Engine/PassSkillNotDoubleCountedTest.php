<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\BallResolver;
use App\Engine\PassResolver;
use App\Engine\RandomDiceRoller;
use App\Engine\ScatterCalculator;
use App\Engine\TacklezoneCalculator;
use App\Enum\PassRange;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * ⭐⭐ SKILL `Pass` DAVA RE-ROLL, NE MODIFIKATOR.
 *
 * `rules_bb2016.txt` r. 8335-8337: "A player with the Pass skill is allowed
 * to RE-ROLL the D6 if he throws an inaccurate pass or fumbles."
 *
 * ⛔ Do 14.09.2026 engine daval OBOJI: `-1` k cili (`PassResolver:324`)
 *   A ZAROVEN re-roll (`PassResolver:127`). Tenhle test hlida, ze uz ne.
 *
 * ⚠️ Treti vyskyt tehoz vzorce za jeden den -- po `Dodge` a po dvojici
 *   u `Mighty Blow`. Proto se hlida testem, ne jen komentarem.
 */
final class PassSkillNotDoubleCountedTest extends TestCase
{
    private function resolver(): PassResolver
    {
        $dice = new RandomDiceRoller();
        $tz = new TacklezoneCalculator();

        return new PassResolver($dice, $tz, new ScatterCalculator(), new BallResolver($dice, $tz, new ScatterCalculator()));
    }

    /** @param list<SkillName> $skills */
    private function hazec(array $skills, int $ag)
    {
        return (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 10, 7, agility: $ag, skills: $skills, id: 1)
            ->build();
    }

    public function testPassSkillUzCilHoduNESNIZUJE(): void
    {
        $r = $this->resolver();

        $bez = $this->hazec([], 3);
        $s = $this->hazec([SkillName::Pass], 3);

        $cilBez = $r->getAccuracyTarget($bez, $bez->getPlayer(1), PassRange::SHORT_PASS);
        $cilS = $r->getAccuracyTarget($s, $s->getPlayer(1), PassRange::SHORT_PASS);

        $this->assertSame(
            $cilBez,
            $cilS,
            'Pass dava RE-ROLL, ne modifikator -- cil hodu musi byt stejny jako bez nej',
        );
    }

    public function testAccurateNaopakModifikatorJEAMusiCilSnizit(): void
    {
        // ⭐ POZITIVNI KONTROLA: kdyby merenie nefungovalo, nezmeni se nic
        //   ani u Accurate -- a ten modifikator MA (na rozdil od Pass).
        $r = $this->resolver();

        $bez = $this->hazec([], 3);
        $s = $this->hazec([SkillName::Accurate], 3);

        $this->assertSame(
            $r->getAccuracyTarget($bez, $bez->getPlayer(1), PassRange::SHORT_PASS) - 1,
            $r->getAccuracyTarget($s, $s->getPlayer(1), PassRange::SHORT_PASS),
            'Accurate modifikator je a ma cil snizit o 1',
        );
    }

    public function testCilRosteSDelkouPrihravkyAKlesaSAgilitou(): void
    {
        $r = $this->resolver();

        $ag2 = $this->hazec([], 2);
        $ag4 = $this->hazec([], 4);

        $bomba = $r->getAccuracyTarget($ag2, $ag2->getPlayer(1), PassRange::LONG_BOMB);
        $quick = $r->getAccuracyTarget($ag4, $ag4->getPlayer(1), PassRange::QUICK_PASS);

        $this->assertGreaterThan(
            $quick,
            $bomba,
            'dlouha bomba od AG2 musi byt tezsi nez quick pass od AG4 -- na tomhle stoji PHP37',
        );
    }
}
