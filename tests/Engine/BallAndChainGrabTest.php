<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\ActionResult;
use App\DTO\GameState;
use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Enum\ActionType;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * P222 (1) PARITA s C++ — Ball & Chain smí použít Grab.
 *
 * `rules_bb2016.txt` ř. 7829-7831: "A Ball & Chain player may use the Grab skill (as if a Block
 * Action was being used) with his blocks (if he has learned it!)". Grab ř. 8148-8154: "may choose
 * any empty square adjacent to his opponent … Grab and Side Step will cancel each other out …
 * Grab will not work if there are no empty adjacent squares." Na ležícího B&C neblokuje
 * (ř. 7843-7845 "pushed back … instead of the block being thrown at them") ⇒ tam Grab neplatí.
 */
final class BallAndChainGrabTest extends TestCase
{
    /**
     * B&C (5,7) ST 7, jeden krok; obránce (6,7); tři pole odtlačení (7,6..8) drží hráči B&C.
     *
     * @param list<SkillName> $obrance
     */
    private function fanatik(bool $grab, array $obrance = [], bool $obranceLezi = false): GameState
    {
        $skills = [SkillName::BallAndChain, SkillName::NoHands];
        if ($grab) {
            $skills[] = SkillName::Grab;
        }
        $b = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 1, strength: 7, skills: $skills, id: 1);
        $b = $obranceLezi
            ? $b->addPronePlayer(TeamSide::AWAY, 6, 7, skills: $obrance, id: 2)
                ->addPlayer(TeamSide::AWAY, 12, 7, id: 6) // nejbližší stojící ⇒ natočení +x
            : $b->addPlayer(TeamSide::AWAY, 6, 7, skills: $obrance, id: 2);

        return $b->addPlayer(TeamSide::HOME, 7, 6, id: 3)
            ->addPlayer(TeamSide::HOME, 7, 7, id: 4)
            ->addPlayer(TeamSide::HOME, 7, 8, id: 5)
            ->withBallOffPitch()
            ->build();
    }

    private function krok(GameState $state): ActionResult
    {
        // D6 3 = rovně na (6,7); kostky bloku 3 = Pushed; zbytek (brnění / zranění) 3
        return (new ActionResolver(new FixedDiceRoller(array_fill(0, 20, 3))))
            ->resolve($state, ActionType::BALL_AND_CHAIN, ['playerId' => 1]);
    }

    /** @return array{0: int, 1: int}|null */
    private function pole(ActionResult $r, int $id): ?array
    {
        $p = $r->getNewState()->requirePlayer($id)->getPosition();

        return $p === null ? null : [$p->getX(), $p->getY()];
    }

    private function nikdoNeretezen(ActionResult $r): bool
    {
        return $this->pole($r, 3) === [7, 6] && $this->pole($r, 4) === [7, 7] && $this->pole($r, 5) === [7, 8];
    }

    public function testSGrabemTlaciNaVolneSousedniPoleMistoRetezu(): void
    {
        $r = $this->krok($this->fanatik(true));

        $this->assertNotNull($this->pole($r, 2));
        $this->assertNotSame(7, $this->pole($r, 2)[0], 'Grab: volné pole vedle obránce, ne řetěz');
        $this->assertTrue($this->nikdoNeretezen($r));
        $this->assertSame([6, 7], $this->pole($r, 1), 'povinný follow-up');
    }

    public function testBezGrabuTentyzBlokRetezi(): void
    {
        $r = $this->krok($this->fanatik(false));

        $this->assertSame(7, $this->pole($r, 2)[0] ?? null);
        $this->assertFalse($this->nikdoNeretezen($r));
    }

    public function testGrabRusiSideStepObrance(): void
    {
        $r = $this->krok($this->fanatik(true, [SkillName::SideStep]));

        $this->assertSame(7, $this->pole($r, 2)[0] ?? null, 'Grab × Side Step se ruší ⇒ řetěz');
    }

    public function testGrabNepouzijeKdyzBezneOdtlaceniVedeDoDavu(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 6, 1, movement: 1, strength: 7, skills: [SkillName::BallAndChain, SkillName::NoHands, SkillName::Grab], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 0, id: 2) // natočení -y, rovně = blok; za ním dav
            ->withBallOffPitch()
            ->build();

        $this->assertNull($this->pole($this->krok($state), 2), 'Grab nepoužit ⇒ do davu');
    }

    public function testOdtlaceniLezicihoNeniBlokTakzeBezGrabu(): void
    {
        $r = $this->krok($this->fanatik(true, [], true));

        $this->assertSame(7, $this->pole($r, 2)[0] ?? null, 'bez bloku Grab neplatí ⇒ řetěz');
    }
}
