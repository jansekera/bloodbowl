<?php
declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\FixedDiceRoller;
use App\Engine\InjuryResolver;
use App\Enum\PlayerState;
use App\Enum\TeamSide;
use App\ValueObject\PlayerStats;
use App\ValueObject\Position;
use App\DTO\MatchPlayerDTO;
use PHPUnit\Framework\TestCase;

final class InjuryResolverTest extends TestCase
{
    private InjuryResolver $resolver;

    protected function setUp(): void
    {
        $this->resolver = new InjuryResolver();
    }

    private function makePlayer(int $armour = 8, int $id = 1): MatchPlayerDTO
    {
        return MatchPlayerDTO::create(
            id: $id,
            playerId: $id,
            name: "Player {$id}",
            number: $id,
            positionalName: 'Lineman',
            stats: new PlayerStats(6, 3, 3, $armour),
            skills: [],
            teamSide: TeamSide::HOME,
            position: new Position(5, 5),
        );
    }

    public function testArmourHolds(): void
    {
        $player = $this->makePlayer(armour: 8);
        // Roll 4+3=7 vs AV8, does not break (need >8)
        $dice = new FixedDiceRoller([4, 3]);

        $result = $this->resolver->resolve($player, $dice);

        $this->assertSame(PlayerState::STANDING, $result['player']->getState());
        $this->assertCount(1, $result['events']); // just armour roll
    }

    public function testArmourBrokenStunned(): void
    {
        $player = $this->makePlayer(armour: 7);
        // Armour: 4+4=8 > AV7, breaks
        // Injury: 3+3=6 <= 7, stunned
        $dice = new FixedDiceRoller([4, 4, 3, 3]);

        $result = $this->resolver->resolve($player, $dice);

        $this->assertSame(PlayerState::STUNNED, $result['player']->getState());
        $this->assertCount(2, $result['events']); // armour + injury
    }

    public function testArmourBrokenKO(): void
    {
        $player = $this->makePlayer(armour: 7);
        // Armour: 5+4=9 > AV7, breaks
        // Injury: 4+4=8, KO (8-9)
        $dice = new FixedDiceRoller([5, 4, 4, 4]);

        $result = $this->resolver->resolve($player, $dice);

        $this->assertSame(PlayerState::KO, $result['player']->getState());
        $this->assertNull($result['player']->getPosition()); // removed from pitch
    }

    public function testArmourBrokenCasualty(): void
    {
        $player = $this->makePlayer(armour: 7);
        // Armour: 5+4=9 > AV7, breaks
        // Injury: 5+5=10, casualty (10+)
        $dice = new FixedDiceRoller([5, 4, 5, 5]);

        $result = $this->resolver->resolve($player, $dice);

        $this->assertSame(PlayerState::INJURED, $result['player']->getState());
        $this->assertNull($result['player']->getPosition());
    }

    public function testArmourModifierBreaksArmour(): void
    {
        $player = $this->makePlayer(armour: 8);
        // Roll 4+4=8, with +1 modifier = 9 > AV8, breaks
        // Injury: 2+2=4, stunned
        $dice = new FixedDiceRoller([4, 4, 2, 2]);

        $result = $this->resolver->resolve($player, $dice, armourModifier: 1);

        $this->assertSame(PlayerState::STUNNED, $result['player']->getState());
    }

    public function testInjuryModifierUpgradesSeverity(): void
    {
        $player = $this->makePlayer(armour: 7);
        // Armour: 5+4=9 > AV7, breaks
        // Injury: 5+4=9, with +1 = 10, casualty instead of KO
        $dice = new FixedDiceRoller([5, 4, 5, 4]);

        $result = $this->resolver->resolve($player, $dice, injuryModifier: 1);

        $this->assertSame(PlayerState::INJURED, $result['player']->getState());
    }

    // ⛔⛔ OPRAVENO 16.09.2026 -- dav NEMA zadny modifikator.
    //   `rules_bb2016.txt` r. 652-654: "A player pushed off the pitch ... receives one
    //   roll on the Injury Table. THE CROWD DOES NOT HAVE ANY INJURY MODIFYING SKILLS."
    //   Engine posilal `injuryModifier: 1`.
    public function testCrowdSurfSkipsArmour(): void
    {
        $player = $this->makePlayer(armour: 10); // High AV doesn't matter
        // Injury: 3+3=6, bez modifikatoru => stunned
        $dice = new FixedDiceRoller([3, 3]);

        $result = $this->resolver->resolveCrowdSurf($player, $dice);

        // No armour roll event, just injury
        // ⛔ ZMENENO 24.09.2026: "Stunned" po vyhozeni z hriste znamena REZERVY,
        //   ne omraceni na hristi -- `rules_bb2016.txt` r. 655-658.
        $this->assertSame(PlayerState::OFF_PITCH, $result['player']->getState());
        $this->assertCount(1, $result['events']); // only injury roll
    }

    public function testCrowdSurfCanCauseKO(): void
    {
        $player = $this->makePlayer(armour: 10);
        // Injury: 4+4=8 => KO (bez modifikatoru; 7 by uz byl jen stunned)
        $dice = new FixedDiceRoller([4, 4]);

        $result = $this->resolver->resolveCrowdSurf($player, $dice);

        $this->assertSame(PlayerState::KO, $result['player']->getState());
    }

    public function testCrowdSurfCanCauseCasualty(): void
    {
        $player = $this->makePlayer(armour: 10);
        // Injury: 5+5=10 => casualty (bez modifikatoru; 9 by byl KO)
        $dice = new FixedDiceRoller([5, 5]);

        $result = $this->resolver->resolveCrowdSurf($player, $dice);

        $this->assertSame(PlayerState::INJURED, $result['player']->getState());
    }

    // ⭐ ROZLISUJICI PRIPADY -- tady se pozna, jestli se pricita +1:
    //   hod 7: bez modifikatoru stunned, s +1 uz KO
    //   hod 9: bez modifikatoru KO, s +1 uz casualty
    // ⛔ ZMENENO 24.09.2026: hod 7 je na tabulce zraneni porad "Stunned", ale po
    //   vyhozeni z hriste to znamena REZERVY. Rozlisovaci sila testu zustava:
    //   s chybnym `+1` by z toho byl KO, takze OFF_PITCH vs KO tu vadu pozna dal.
    public function testCrowdSurfHod7JdeDoRezervNeKo(): void
    {
        $result = $this->resolver->resolveCrowdSurf($this->makePlayer(armour: 10), new FixedDiceRoller([4, 3]));
        $this->assertSame(PlayerState::OFF_PITCH, $result['player']->getState());
    }

    public function testCrowdSurfHod9JeKoNeCasualty(): void
    {
        $result = $this->resolver->resolveCrowdSurf($this->makePlayer(armour: 10), new FixedDiceRoller([5, 4]));
        $this->assertSame(PlayerState::KO, $result['player']->getState());
    }

    // ⭐ BALIK G, 24.09.2026 -- `rules_bb2016.txt` r. 655-658: "If a 'Stunned' result
    //   is rolled on the Injury table the player should be placed in the Reserves box
    //   of the Dugout, and must remain there until a touchdown is scored or the half ends."
    //   Stav rezerv je `OFF_PITCH` -- hraci v nem cekaji na rozestaveni pri dalsim drivu.
    public function testCrowdSurfStunnedJdeDoRezervABezPozice(): void
    {
        $player = $this->makePlayer(armour: 10);
        $this->assertNotNull($player->getPosition(), 'kontrola vychoziho stavu: hrac na hristi pozici MA');

        // 3+3=6 => na tabulce zraneni "Stunned"
        $result = $this->resolver->resolveCrowdSurf($player, new FixedDiceRoller([3, 3]));

        $this->assertSame(PlayerState::OFF_PITCH, $result['player']->getState());
        $this->assertNull($result['player']->getPosition(), 'v rezervach hrac na hristi nestoji');
    }

    // ⭐ POZITIVNI KONTROLA k testu vyse: tytez kostky, ale BEZNE zraneni (ne surf)
    //   musi dat STUNNED NA HRISTI, vcetne pozice. Bez tohohle testu by se nepoznalo,
    //   jestli se "stunned => rezervy" neaplikuje omylem vsude.
    public function testBezneZraneniStunnedZustavaNaHristi(): void
    {
        $player = $this->makePlayer(armour: 10);

        // 3+3=6 na zraneni; brneni prorazime samostatne (10+10 > AV10)
        $result = $this->resolver->resolve($player, new FixedDiceRoller([5, 6, 3, 3]));

        $this->assertSame(PlayerState::STUNNED, $result['player']->getState());
        $this->assertNotNull($result['player']->getPosition(), 'omraceny hrac lezi NA HRISTI');
    }
}
