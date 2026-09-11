<?php
declare(strict_types=1);

namespace App\Tests\Engine;

use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Enum\ActionType;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * ⛔ VADA (11.09.2026, PHP15 část a): když Big Guy neuspěl v kontrole před
 *    akcí, tým o **deklarovanou akci nepřišel**.
 *
 * ⭐ `rules_bb2016.txt` r. 8398-8401: „The player can't do anything for the
 *    turn, and **the player's team loses the declared Action for that turn**
 *    (for example if a Really Stupid player declares a Blitz Action and
 *    fails the Really Stupid roll, then **the team cannot declare another
 *    Blitz Action that turn**)." U Wild Animal r. 8668-8669 stejně:
 *    „**the Action is wasted**."
 *
 * ⇒ Bez toho si tým blitz prostě zahrál znovu jiným hráčem. Čistá výhoda
 *    proti pravidlům, a platila pro všechny čtyři dovednosti.
 * ⚠️ C++ to má jako `wastesTeamAction` (`action_resolver.cpp:276-283`).
 */
final class BigGuyWastesTeamActionTest extends TestCase
{
    public function testFailedReallyStupidBlitzCostsTheTeamItsBlitz(): void
    {
        // Hráč 1 je Really Stupid a NEMÁ vedle sebe spoluhráče ⇒ práh 4+.
        // Hráč 3 je běžný a stojí daleko, aby mohl blitzovat potom.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 6, id: 1,
                        skills: [SkillName::ReallyStupid])
            ->addPlayer(TeamSide::AWAY, 6, 7, id: 2)
            ->addPlayer(TeamSide::HOME, 15, 3, movement: 6, id: 3)
            ->withBallOffPitch()
            ->build();

        // SEBEKONTROLA FIXTURY: blitz je na začátku k dispozici.
        $this->assertFalse($state->getTeamState(TeamSide::HOME)->isBlitzUsedThisTurn(),
            'fixtura je vadná: blitz je vyčerpaný už předem');

        // Hod 1 ⇒ pod prahem 4+, kontrola NEUSPĚJE.
        $resolver = new ActionResolver(new FixedDiceRoller([1]));
        $result = $resolver->resolve($state, ActionType::BLITZ, [
            'playerId' => 1, 'targetId' => 2,
        ]);

        $after = $result->getNewState();

        // SEBEKONTROLA VÝSLEDKU: kontrola opravdu selhala.
        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('really_stupid', $types,
            'Really Stupid se neozval -- test neměří, co má');
        $this->assertTrue($after->getPlayer(1)->hasActed());

        $this->assertTrue($after->getTeamState(TeamSide::HOME)->isBlitzUsedThisTurn(),
            'tým měl o deklarovaný blitz přijít (r. 8398-8401)');
    }

    public function testSuccessfulCheckDoesNotWasteAnythingExtra(): void
    {
        // ⛔ Druhá polovina páru: při ÚSPĚCHU se nic navíc nespotřebuje --
        //    blitz se odečte normální cestou v handleru, ne tady.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 6, id: 1,
                        skills: [SkillName::ReallyStupid])
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->withBallOffPitch()
            ->build();

        // Hod 4 ⇒ práh 4+ splněn, kontrola PROJDE.
        $resolver = new ActionResolver(new FixedDiceRoller([4, 6, 6, 6, 6, 6, 6]));
        $result = $resolver->resolve($state, ActionType::BLITZ, [
            'playerId' => 1, 'targetId' => 2,
        ]);

        $this->assertTrue($result->isSuccess());
        $this->assertFalse($result->getNewState()->getPlayer(1)->hasActed()
            && $result->getNewState()->getPlayer(1)->getPosition()->getX() === 5,
            'hráč měl projít kontrolou a jednat, ne zůstat stát');
    }

    public function testFailedWildAnimalMoveCostsNoTeamAction(): void
    {
        // ⭐ MOVE žádný týmový limit nemá -- nesmí se odečíst nic.
        //    Bez tohohle by `consumeDeclaredTeamAction` mohl tiše brát blitz
        //    i za pohyb.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, movement: 6, id: 1,
                        skills: [SkillName::WildAnimal])
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->withBallOffPitch()
            ->build();

        // Hod 1 ⇒ bez bonusu (MOVE) je práh 4+, kontrola NEUSPĚJE.
        $resolver = new ActionResolver(new FixedDiceRoller([1]));
        $result = $resolver->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 6, 'y' => 7,
        ]);

        $types = array_map(fn($e) => $e->getType(), $result->getEvents());
        $this->assertContains('wild_animal', $types, 'kontrola se neozvala');

        $team = $result->getNewState()->getTeamState(TeamSide::HOME);
        $this->assertFalse($team->isBlitzUsedThisTurn(), 'MOVE nesmí brát blitz');
        $this->assertFalse($team->isPassUsedThisTurn(), 'MOVE nesmí brát pass');
        $this->assertFalse($team->isFoulUsedThisTurn(), 'MOVE nesmí brát foul');
    }
}
