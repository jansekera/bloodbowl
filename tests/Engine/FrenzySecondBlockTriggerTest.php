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
 * FRENZY: DRUHÁ RÁNA JEN PO 'PUSHED' / 'DEFENDER STUMBLES' (audit parity 08.10.2026, nález 7).
 *
 * `rules_bb2016.txt` ř. 8138-8141: "If a **'Pushed' or 'Defender Stumbles'** result was
 * chosen, the player must immediately throw a second block against the same opponent so
 * long as they are both still standing and adjacent."
 *
 * Stará mechanika: druhá rána se házela po každém výsledku, po kterém oba stáli a
 * sousedili -- tedy i po Both Down, když mají oba Block a nikdo nepadne.
 *
 * Kostky: první je kostka bloku (stejná síla ⇒ 1 kostka). Další kostky by spotřebovala
 * jen druhá rána: 6 = Defender Down + brnění 1+1 (neprorazí).
 */
final class FrenzySecondBlockTriggerTest extends TestCase
{
    /**
     * @param list<SkillName> $obrance
     * @param list<int> $kostky
     */
    private function blok(array $obrance, array $kostky): ActionResult
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, skills: [SkillName::Block, SkillName::Frenzy], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 7, skills: $obrance, id: 2)
            ->build();

        return (new ActionResolver(new FixedDiceRoller($kostky)))
            ->resolve($state, ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);
    }

    /** @return list<string> */
    private function typy(ActionResult $r): array
    {
        return array_map(fn($e) => $e->getType(), $r->getEvents());
    }

    public function testPoBothDownKdyzObaStojiDikyBlockuSeDruhaRanaNehazi(): void
    {
        // 2 = Both Down; oba mají Block ⇒ nikdo nepadne, oba stojí a sousedí.
        $r = $this->blok([SkillName::Block], [2, 6, 1, 1]);

        $this->assertNotContains('frenzy', $this->typy($r), 'ř. 8138-8141: jen po Pushed / Defender Stumbles');
        $this->assertSame(PlayerState::STANDING, $r->getNewState()->requirePlayer(2)->getState());
        $this->assertSame(6, $r->getNewState()->requirePlayer(2)->requirePosition()->getX());
        $this->assertCount(1, array_filter($this->typy($r), fn(string $t) => $t === 'block'), 'hozena jediná rána');
    }

    public function testPoPushedSeDruhaRanaHazi(): void
    {
        // Kontrola opačným směrem: 3 = Pushed ⇒ follow-up a druhá rána (6 = Defender Down).
        $r = $this->blok([SkillName::Block], [3, 6, 1, 1]);

        $this->assertContains('frenzy', $this->typy($r));
        $this->assertSame(PlayerState::PRONE, $r->getNewState()->requirePlayer(2)->getState());
    }

    public function testPoDefenderStumblesKdyObranceUstalDikyDodgeSeDruhaRanaHazi(): void
    {
        // 5 = Defender Stumbles; Dodge z něj dělá odtlačení ⇒ obránce stojí, druhá rána.
        $r = $this->blok([SkillName::Dodge], [5, 6, 1, 1]);

        $this->assertContains('frenzy', $this->typy($r));
        $this->assertSame(PlayerState::PRONE, $r->getNewState()->requirePlayer(2)->getState());
    }
}
