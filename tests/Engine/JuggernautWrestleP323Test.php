<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\ActionResult;
use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Enum\ActionType;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * P323 (uživatel 23.11.2026: „sedí, neber v úvahu kdo vstane dříve“) — Juggernaut + Wrestle bez Blocku, Both Down na
 * blitzu: surf ⇒ Pushed; obránce nese míč ⇒ Wrestle; jinak Wrestle jen na rychlejšího obránce. Parita C++
 * `P323JuggernautWrestle.*`. Kostka 2 = Both Down; další kostky jen pro případné brnění/vhazování.
 */
final class JuggernautWrestleP323Test extends TestCase
{
    private function blitz(int $maUtocnika, int $maObrance, int $y = 5, ?int $nosic = null, int $yUtocnika = 5, int $xObrance = 6): ActionResult
    {
        $b = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, $yUtocnika, movement: $maUtocnika, skills: [SkillName::Juggernaut, SkillName::Wrestle], id: 1)
            ->addPlayer(TeamSide::AWAY, $xObrance, $y, movement: $maObrance, id: 2);
        $b = $nosic !== null ? $b->withBallCarried($nosic) : $b->withBallOffPitch();

        return (new ActionResolver(new FixedDiceRoller([2, 1, 1, 1, 1, 1, 1, 1, 1, 1])))
            ->resolve($b->build(), ActionType::BLITZ, ['playerId' => 1, 'targetId' => 2]);
    }

    /** @return list<string> */
    private function typy(ActionResult $r): array
    {
        return array_map(static fn($e): string => $e->getType(), $r->getEvents());
    }

    public function testRychlejsiObranceJePolozenWrestlem(): void
    {
        $r = $this->blitz(5, 7);
        $this->assertContains('wrestle', $this->typy($r));
        $this->assertSame(PlayerState::PRONE, $r->getNewState()->requirePlayer(2)->getState());
    }

    public function testNerychlejsiObranceJeOdtlacen(): void
    {
        $r = $this->blitz(6, 6);
        $this->assertNotContains('wrestle', $this->typy($r));
        $this->assertSame(PlayerState::STANDING, $r->getNewState()->requirePlayer(1)->getState());
    }

    public function testPomalyNosicJePolozenAMicPada(): void
    {
        $r = $this->blitz(6, 4, nosic: 2);
        $this->assertContains('wrestle', $this->typy($r));
        $this->assertNotSame(2, $r->getNewState()->getBall()->getCarrierId());
    }

    public function testSurfMaPrednost(): void
    {
        // obránce na horní lajně (6,0), útočník (6,1) pod ním ⇒ odtlačení vede do davu, i když je rychlejší a nese míč
        $r = $this->blitz(5, 8, y: 0, nosic: 2, yUtocnika: 1, xObrance: 5);
        $this->assertNotContains('wrestle', $this->typy($r));
        $this->assertSame(PlayerState::STANDING, $r->getNewState()->requirePlayer(1)->getState());
    }
}
