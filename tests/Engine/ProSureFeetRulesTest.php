<?php

declare(strict_types=1);

namespace App\Tests\Engine;

use App\DTO\GameState;
use App\DTO\TeamStateDTO;
use App\Engine\ActionResolver;
use App\Engine\BallResolver;
use App\Engine\FixedDiceRoller;
use App\Engine\ScatterCalculator;
use App\Engine\TacklezoneCalculator;
use App\Enum\ActionType;
use App\Enum\BlockDiceFace;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use PHPUnit\Framework\TestCase;

/**
 * ⭐⭐ PRO A SURE FEET PODLE PRAVIDEL (18.09.2026).
 *
 * `rules_bb2016.txt` r. 8381-8387 (Pro): "Once per turn, a Pro is allowed to
 * re-roll any one dice roll he has made other than Armour, Injury or Casualty
 * [...] before the re-roll may be made, his coach must roll a D6. On a roll of
 * 4, 5 or 6 the re-roll may be made. On a roll of 1, 2 or 3 the original
 * result stands and may not be re-rolled with a skill or team re-roll;
 * however you can re-roll the Pro roll with a Team re-roll."
 *
 * r. 925-926: "you may never re-roll a single dice roll more than once."
 * r. 919-924: "a re-roll allows you to re-roll all the dice that produced any
 * one result [...] a three dice block, in which case all three dice would be
 * rolled again".
 *
 * r. 8539-8541 (Sure Feet): "A player may only use the Sure Feet skill once
 * per turn."
 *
 * Kostky jsou volene NA HRANE: kazdy test dava jiny vysledek s vadou a bez ni.
 */
final class ProSureFeetRulesTest extends TestCase
{
    private function bezPrehozu(GameState $s): GameState
    {
        return $s->withTeamState(TeamSide::HOME, $s->getTeamState(TeamSide::HOME)->withRerolls(0));
    }

    /** @param list<\App\DTO\GameEvent> $events @return list<array<string, mixed>> */
    private function proUdalosti(array $events): array
    {
        $out = [];
        foreach ($events as $e) {
            if ($e->getType() === 'pro') {
                $out[] = $e->getData();
            }
        }
        return $out;
    }

    // ================= Pro: 1x za kolo -- pouziti se musi zapsat =================

    public function testProPriZvedaniSeZapiseJakoPouzity(): void
    {
        $state = $this->bezPrehozu((new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, skills: [SkillName::Pro], id: 1)
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->withBallOnGround(6, 7)
            ->build());

        // zvedani 1 (neuspech), Pro 4 (smi), prehoz 6 (uspech)
        $r = (new ActionResolver(new FixedDiceRoller([1, 4, 6])))->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 6, 'y' => 7,
        ]);

        $this->assertTrue($r->getNewState()->getBall()->isHeld(), 'fixtura: Pro prehoz zvedani probehl');
        $this->assertTrue($r->getNewState()->getPlayer(1)->isProUsedThisTurn(), 'r. 8381: Pro 1x za kolo');
    }

    public function testProPriChytaniSeZapiseJakoPouzity(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, skills: [SkillName::Pro], id: 1)
            ->withBallOnGround(5, 7)
            ->build();
        $dice = new FixedDiceRoller([1, 4, 6]); // chytani 1, Pro 4, prehoz 6
        $ball = new BallResolver($dice, new TacklezoneCalculator(), new ScatterCalculator());

        $r = $ball->resolveCatch($state, $state->getPlayer(1));

        $this->assertTrue($r['success'], 'fixtura: Pro prehoz chytani probehl');
        $this->assertTrue($r['state']->getPlayer(1)->isProUsedThisTurn(), 'r. 8381: Pro 1x za kolo');
    }

    public function testProPriPrihravceSeZapiseJakoPouzity(): void
    {
        $state = $this->bezPrehozu((new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, skills: [SkillName::Pro], id: 1)
            ->addPlayer(TeamSide::HOME, 9, 5, agility: 3, id: 2)
            ->withBallCarried(1)
            ->build());

        // short pass, AG3 => 4+. Hod 3 (nepresne), Pro 4, prehoz 4 (presne), chytani 4
        $r = (new ActionResolver(new FixedDiceRoller(array_merge([3, 4, 4], array_fill(0, 20, 4)))))
            ->resolve($state, ActionType::PASS, ['playerId' => 1, 'targetX' => 9, 'targetY' => 5]);

        $this->assertCount(1, $this->proUdalosti($r->getEvents()), 'fixtura: Pro se pouzil');
        $this->assertTrue($r->getNewState()->getPlayer(1)->isProUsedThisTurn(), 'r. 8381: Pro 1x za kolo');
    }

    // ================= Pro: kostka nejvys 1x prehozena (po skill prehozu uz ne) =================

    public function testProNeniPoPrehozuSureHands(): void
    {
        $state = $this->bezPrehozu((new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, skills: [SkillName::SureHands, SkillName::Pro], id: 1)
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->withBallOnGround(6, 7)
            ->build());

        // zvedani 1, Sure Hands 1 => kostka uz prehozena. S vadou: Pro 4, prehoz 6 = zvednuto.
        $r = (new ActionResolver(new FixedDiceRoller([1, 1, 4, 6, 4, 4, 4])))->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 6, 'y' => 7,
        ]);

        $this->assertSame([], $this->proUdalosti($r->getEvents()), 'r. 926: kostka nejvys 1x prehozena');
        $this->assertTrue($r->isTurnover());
    }

    public function testProNeniPoPrehozuCatch(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, skills: [SkillName::Catch, SkillName::Pro], id: 1)
            ->withBallOnGround(5, 7)
            ->build();
        $dice = new FixedDiceRoller([1, 1, 4, 6, 4, 4, 4]);
        $ball = new BallResolver($dice, new TacklezoneCalculator(), new ScatterCalculator());

        $r = $ball->resolveCatch($state, $state->getPlayer(1));

        $this->assertSame([], $this->proUdalosti($r['events']), 'r. 926: kostka nejvys 1x prehozena');
        $this->assertFalse($r['success']);
    }

    public function testProNeniPoPrehozuPass(): void
    {
        $state = $this->bezPrehozu((new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, agility: 3, skills: [SkillName::Pass, SkillName::Pro], id: 1)
            ->addPlayer(TeamSide::HOME, 9, 5, agility: 3, id: 2)
            ->withBallCarried(1)
            ->build());

        // hod 3, Pass 3 (obe nepresne). S vadou: Pro 4, prehoz 4 = presne.
        $r = (new ActionResolver(new FixedDiceRoller(array_merge([3, 3], array_fill(0, 20, 4)))))
            ->resolve($state, ActionType::PASS, ['playerId' => 1, 'targetX' => 9, 'targetY' => 5]);

        $this->assertSame([], $this->proUdalosti($r->getEvents()), 'r. 926: kostka nejvys 1x prehozena');
    }

    public function testProNeniPoPrehozuDodge(): void
    {
        $state = $this->bezPrehozu((new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, skills: [SkillName::Dodge, SkillName::Pro], id: 1)
            ->addPlayer(TeamSide::AWAY, 5, 6, id: 2)
            ->build());

        // uhyb 1, Dodge 1. S vadou: Pro 4, prehoz 6 = uspech.
        $r = (new ActionResolver(new FixedDiceRoller([1, 1, 4, 6, 1, 1])))->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 5, 'y' => 8,
        ]);

        $this->assertSame([], $this->proUdalosti($r->getEvents()), 'r. 926: kostka nejvys 1x prehozena');
        $this->assertTrue($r->isTurnover());
    }

    public function testProNeniPoPrehozuSureFeet(): void
    {
        $state = $this->bezPrehozu((new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 3, 7, movement: 6, skills: [SkillName::SureFeet, SkillName::Pro], id: 1)
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->build());

        // GFI 1, Sure Feet 1. S vadou: Pro 4, prehoz 6 = uspech.
        $r = (new ActionResolver(new FixedDiceRoller([1, 1, 4, 6, 1, 1])))->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 10, 'y' => 7,
        ]);

        $this->assertSame([], $this->proUdalosti($r->getEvents()), 'r. 926: kostka nejvys 1x prehozena');
        $this->assertTrue($r->isTurnover());
    }

    public function testInteraktivneProNeniNabidnutPoPrehozuDodge(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, skills: [SkillName::Dodge, SkillName::Pro], id: 1)
            ->addPlayer(TeamSide::AWAY, 5, 6, id: 2)
            ->build();

        $resolver = new ActionResolver(new FixedDiceRoller([1, 1, 1, 1]));
        $resolver->setInteractiveRerolls(true);
        $r = $resolver->resolve($state, ActionType::MOVE, ['playerId' => 1, 'x' => 5, 'y' => 8]);

        $this->assertNull($r->getNewState()->getPendingReroll(), 'r. 926: po Dodge uz zadny prehoz');
        $this->assertTrue($r->isTurnover());
    }

    // ================= Pro: po Pro uz tymovy prehoz kostky ne =================

    public function testPoUspesnemProNeniTymovyPrehozKostky(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, skills: [SkillName::Pro], id: 1)
            ->addPlayer(TeamSide::AWAY, 5, 6, id: 2)
            ->build();

        // uhyb 1, Pro 4, prehoz 1 (neuspech). S vadou: tymovy prehoz 6 = uspech.
        $r = (new ActionResolver(new FixedDiceRoller([1, 4, 1, 6, 1, 1])))->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 5, 'y' => 8,
        ]);

        $this->assertTrue($r->isTurnover(), 'r. 926: kostka uz byla prehozena Pro');
        $this->assertFalse($r->getNewState()->getHomeTeam()->isRerollUsedThisTurn());
    }

    public function testPoNeuspesnemProTymovyPrehozPrehazujeHodPro(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, skills: [SkillName::Pro], id: 1)
            ->addPlayer(TeamSide::AWAY, 5, 6, id: 2)
            ->build();

        // uhyb 1, Pro 3 (puvodni vysledek plati), tymovy prehoz HODU PRO 4 => smi,
        // prehoz uhybu 1 => neuspech. S vadou: tymovy prehoz 4 padne na uhyb = uspech.
        $r = (new ActionResolver(new FixedDiceRoller([1, 3, 4, 1, 1, 1])))->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 5, 'y' => 8,
        ]);

        $this->assertTrue($r->isTurnover(), 'r. 8386-8387: tymovy prehoz jde na hod Pro, ne na uhyb');
        $pro = $this->proUdalosti($r->getEvents());
        $this->assertCount(2, $pro);
        $this->assertSame([3, 4], [$pro[0]['proRoll'], $pro[1]['proRoll']]);
        $this->assertTrue($r->getNewState()->getHomeTeam()->isRerollUsedThisTurn());
    }

    public function testPozitivniKontrolaTymovyPrehozHoduProPakUspesnyUhyb(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, skills: [SkillName::Pro], id: 1)
            ->addPlayer(TeamSide::AWAY, 5, 6, id: 2)
            ->build();

        $r = (new ActionResolver(new FixedDiceRoller([1, 3, 4, 6])))->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 5, 'y' => 8,
        ]);

        $this->assertTrue($r->isSuccess());
        $this->assertSame(8, $r->getNewState()->getPlayer(1)->getPosition()->getY());
    }

    public function testInteraktivnePoUspesnemProNeniNabidnutTymovyPrehoz(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, skills: [SkillName::Pro], id: 1)
            ->addPlayer(TeamSide::AWAY, 5, 4)
            ->build();

        $resolver = new ActionResolver(new FixedDiceRoller([2, 4, 1, 1, 1]));
        $resolver->setInteractiveRerolls(true);
        $r = $resolver->resolve($state, ActionType::MOVE, ['playerId' => 1, 'x' => 5, 'y' => 6]);
        $this->assertNotNull($r->getNewState()->getPendingReroll());

        $r2 = $resolver->resolve($r->getNewState(), ActionType::RESOLVE_REROLL, ['choice' => 'pro']);

        $this->assertNull($r2->getNewState()->getPendingReroll(), 'r. 926: kostka uz byla prehozena Pro');
        $this->assertTrue($r2->isTurnover());
    }

    // ================= Pro: 1x za KAZDE kolo, i souperovo =================

    public function testProPouzityVeSvemKoleSeObnoviNaZacatkuSouperova(): void
    {
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 7, skills: [SkillName::Pro], id: 1)
            ->build();
        $state = $state->withPlayer($state->getPlayer(1)->withProUsedThisTurn(true));

        $souperovo = $state->resetPlayersForNewTurn(TeamSide::AWAY);

        $this->assertFalse($souperovo->getPlayer(1)->isProUsedThisTurn(), 'r. 8381: "once per turn" = kazde kolo');
    }

    // ================= Pro na bloku: prehazuji se VSECHNY kostky =================

    private function blokState(): GameState
    {
        return (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 5, strength: 4, skills: [SkillName::Pro], id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 5, strength: 3, id: 2)
            ->build();
    }

    public function testProNaBlokuPrehodiVsechnyKostky(): void
    {
        // 2 kostky: AD, BD. Pro 4, prehoz 6, 6. S vadou se prehodi jen AD => [DD, BD].
        $resolver = new ActionResolver(new FixedDiceRoller([1, 2, 4, 6, 6]));
        $resolver->setInteractiveBlocks(true);
        $r = $resolver->resolve($this->blokState(), ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $r2 = $resolver->resolve($r->getNewState(), ActionType::REROLL_BLOCK, ['type' => 'pro']);

        $this->assertSame(
            [BlockDiceFace::DEFENDER_DOWN, BlockDiceFace::DEFENDER_DOWN],
            $r2->getNewState()->getPendingBlock()->getFaces(),
            'r. 919-924: prehoz bloku = vsechny kostky',
        );
    }

    public function testBlokPoUspesnemProNeniTymovyPrehoz(): void
    {
        $resolver = new ActionResolver(new FixedDiceRoller([1, 2, 4, 1, 2]));
        $resolver->setInteractiveBlocks(true);
        $r = $resolver->resolve($this->blokState(), ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $r2 = $resolver->resolve($r->getNewState(), ActionType::REROLL_BLOCK, ['type' => 'pro']);

        $this->assertFalse($r2->getNewState()->getPendingBlock()->isTeamRerollAvailable(), 'r. 926');
    }

    public function testBlokPoTymovemPrehozuNeniPro(): void
    {
        $resolver = new ActionResolver(new FixedDiceRoller([1, 2, 1, 2]));
        $resolver->setInteractiveBlocks(true);
        $r = $resolver->resolve($this->blokState(), ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);

        $r2 = $resolver->resolve($r->getNewState(), ActionType::REROLL_BLOCK, ['type' => 'team']);

        $this->assertFalse($r2->getNewState()->getPendingBlock()->isProAvailable(), 'r. 926');
    }

    public function testBlokPoNeuspesnemProTymovyPrehozPrehazujeHodPro(): void
    {
        // AD, BD; Pro 3 (puvodni plati); tymovy prehoz hodu Pro 4 => prehoz obou kostek 6, 6.
        // S vadou tymovy prehoz hodi rovnou kostky bloku: 4, 6 => [PUSHED, DD].
        $resolver = new ActionResolver(new FixedDiceRoller([1, 2, 3, 4, 6, 6]));
        $resolver->setInteractiveBlocks(true);
        $r = $resolver->resolve($this->blokState(), ActionType::BLOCK, ['playerId' => 1, 'targetId' => 2]);
        $r2 = $resolver->resolve($r->getNewState(), ActionType::REROLL_BLOCK, ['type' => 'pro']);
        $this->assertTrue($r2->getNewState()->getPendingBlock()->isTeamRerollAvailable(), 'r. 8387: hod Pro jde prehodit');

        $r3 = $resolver->resolve($r2->getNewState(), ActionType::REROLL_BLOCK, ['type' => 'team']);

        $pending = $r3->getNewState()->getPendingBlock();
        $this->assertSame([BlockDiceFace::DEFENDER_DOWN, BlockDiceFace::DEFENDER_DOWN], $pending->getFaces());
        $this->assertFalse($pending->isTeamRerollAvailable());
        $this->assertFalse($pending->isProAvailable());
    }

    // ================= Sure Feet: 1x za kolo =================

    public function testSureFeetJenJednouZaKolo(): void
    {
        $state = $this->bezPrehozu((new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 3, 7, movement: 6, skills: [SkillName::SureFeet], id: 1)
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->build());

        // 2 GFI: GFI1 1, Sure Feet 2 (uspech); GFI2 1 => Sure Feet uz ne. S vadou: 2 = uspech.
        $r = (new ActionResolver(new FixedDiceRoller([1, 2, 1, 2, 1, 1])))->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 11, 'y' => 7,
        ]);

        $this->assertTrue($r->isTurnover(), 'r. 8540-8541: Sure Feet jen 1x za kolo');
    }

    public function testSureFeetPouzitiPretrvaDoDalsiAkceAObnoviSeNaZacatkuKola(): void
    {
        $state = $this->bezPrehozu((new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 3, 7, movement: 6, skills: [SkillName::SureFeet], id: 1)
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 2)
            ->build());

        $r = (new ActionResolver(new FixedDiceRoller([1, 2])))->resolve($state, ActionType::MOVE, [
            'playerId' => 1, 'x' => 10, 'y' => 7,
        ]);
        $this->assertTrue($r->isSuccess(), 'fixtura: Sure Feet zachranil GFI');
        $this->assertTrue($r->getNewState()->getPlayer(1)->isSureFeetUsedThisTurn());

        $noveKolo = $r->getNewState()->resetPlayersForNewTurn(TeamSide::HOME);
        $this->assertFalse($noveKolo->getPlayer(1)->isSureFeetUsedThisTurn());
    }
}
