<?php

declare(strict_types=1);

namespace App\Tests\AI;

use App\AI\CoachHeuristics;
use App\AI\GreedyAICoach;
use App\AI\LearningAICoach;
use App\Engine\ActionResolver;
use App\Engine\FixedDiceRoller;
use App\Engine\RulesEngine;
use App\Engine\StrengthCalculator;
use App\Enum\ActionType;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use App\Tests\Engine\GameStateBuilder;
use App\ValueObject\Position;
use PHPUnit\Framework\TestCase;

/**
 * P227 v PHP (C++ sloučeno 26.10., `blitzDiceAllowed`). Uživatel 21.10.2026: „Tři proti nedává smysl ani
 * proti nosiči“; ke dvěma proti volba „3b“ = jen na nosiče míče. Dauntless (uživatel 26.10. „2 ano“):
 * blitz smí, když se Dauntless povede aspoň ve 2 ze 3 případů (rozdíl sil ≤ 2, ř. 8023-8035) a kostky
 * po úspěchu práh projdou. Kostky se počítají z pole, odkud hra po doběhu opravdu blokuje.
 */
final class BlitzDiceThresholdTest extends TestCase
{
    private function povolen(GameStateBuilder $b, int $utocnikId, int $obranceId, ?Position $dopad = null): bool
    {
        $s = $b->build();
        $u = $s->getPlayer($utocnikId);
        $o = $s->getPlayer($obranceId);
        $this->assertNotNull($u);
        $this->assertNotNull($o);

        return CoachHeuristics::blitzKostkamiPovolen(new StrengthCalculator(), $s, $u, $o, $dopad ?? $u->requirePosition());
    }

    public function testTriKostkyProtiNikdyAniNaNosice(): void
    {
        $b = (new GameStateBuilder())->withActiveTeam(TeamSide::HOME)
            ->addPlayer(TeamSide::HOME, 10, 7, strength: 2, id: 1)
            ->addPlayer(TeamSide::AWAY, 11, 7, strength: 5, id: 12)
            ->withBallCarried(12);
        $this->assertFalse($this->povolen($b, 1, 12), 'ST 2 proti nosiči ST 5 = 3 kostky, volí obránce');
    }

    public function testDveKostkyProtiJenNaNosice(): void
    {
        $nosic = (new GameStateBuilder())->withActiveTeam(TeamSide::HOME)
            ->addPlayer(TeamSide::HOME, 10, 7, strength: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 11, 7, strength: 4, id: 12)
            ->withBallCarried(12);
        $this->assertTrue($this->povolen($nosic, 1, 12), '2 proti na nosiče smí (3b)');

        $jiny = (new GameStateBuilder())->withActiveTeam(TeamSide::HOME)
            ->addPlayer(TeamSide::HOME, 10, 7, strength: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 11, 7, strength: 4, id: 12)
            ->addPlayer(TeamSide::AWAY, 20, 3, id: 13)
            ->withBallCarried(13);
        $this->assertFalse($this->povolen($jiny, 1, 12), '2 proti mimo nosiče ne');
    }

    public function testJednaKostkaANasKostkySmi(): void
    {
        $b = (new GameStateBuilder())->withActiveTeam(TeamSide::HOME)
            ->addPlayer(TeamSide::HOME, 10, 7, strength: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 11, 7, strength: 3, id: 12);
        $this->assertTrue($this->povolen($b, 1, 12), '1 kostka');
    }

    /** Horns (+1 k síle při blitzu, ř. 8172-8175) mění 2 proti na 1 kostku. */
    public function testHornsSePocitaJakoVBlitzu(): void
    {
        $b = (new GameStateBuilder())->withActiveTeam(TeamSide::HOME)
            ->addPlayer(TeamSide::HOME, 10, 7, strength: 3, skills: [SkillName::Horns], id: 1)
            ->addPlayer(TeamSide::AWAY, 11, 7, strength: 4, id: 12);
        $this->assertTrue($this->povolen($b, 1, 12));
    }

    public function testDauntlessSmiDoRozdiluDvouKdyzPakKostkyProjdou(): void
    {
        // rozdíl 2 (ST 2 proti ST 4): šance 4/6 ≥ 2/3, po úspěchu 4 proti 4 = 1 kostka
        $b = (new GameStateBuilder())->withActiveTeam(TeamSide::HOME)
            ->addPlayer(TeamSide::HOME, 10, 7, strength: 2, skills: [SkillName::Dauntless], id: 1)
            ->addPlayer(TeamSide::AWAY, 11, 7, strength: 4, id: 12);
        $this->assertTrue($this->povolen($b, 1, 12), 'Dauntless rozdíl 2');

        // rozdíl 3 (ST 2 proti ST 5): šance 3/6 < 2/3
        $c = (new GameStateBuilder())->withActiveTeam(TeamSide::HOME)
            ->addPlayer(TeamSide::HOME, 10, 7, strength: 2, skills: [SkillName::Dauntless], id: 1)
            ->addPlayer(TeamSide::AWAY, 11, 7, strength: 5, id: 12);
        $this->assertFalse($this->povolen($c, 1, 12), 'Dauntless rozdíl 3');

        // rozdíl 1, ale obránce má 2 asistence vedle útočníka: po úspěchu 3 proti 3+2 = 2 proti mimo nosiče
        $d = (new GameStateBuilder())->withActiveTeam(TeamSide::HOME)
            ->addPlayer(TeamSide::HOME, 10, 7, strength: 2, skills: [SkillName::Dauntless], id: 1)
            ->addPlayer(TeamSide::AWAY, 11, 7, strength: 3, id: 12)
            ->addPlayer(TeamSide::AWAY, 9, 6, id: 13)
            ->addPlayer(TeamSide::AWAY, 9, 8, id: 14);
        $this->assertFalse($this->povolen($d, 1, 12), 'Dauntless se povede, kostky ale pořád 2 proti');

        // tatáž pozice, ale cíl je nosič: 2 proti po úspěchu na nosiče smí (3b)
        $this->assertTrue($this->povolen($d->withBallCarried(12), 1, 12), 'Dauntless, po úspěchu 2 proti na nosiče');
    }

    /**
     * Horns se do rozdílu sil u Dauntless počítá („after all other modifiers“, ř. 8034-8035): ST 2 + Horns
     * proti ST 5 = rozdíl 2 (šance 4/6) ⇒ smí; bez Horns by rozdíl byl 3 (šance 3/6).
     */
    public function testHornsSeZapocitaDoRozdiluUDauntless(): void
    {
        $b = (new GameStateBuilder())->withActiveTeam(TeamSide::HOME)
            ->addPlayer(TeamSide::HOME, 10, 7, strength: 2, skills: [SkillName::Dauntless, SkillName::Horns], id: 1)
            ->addPlayer(TeamSide::AWAY, 11, 7, strength: 5, id: 12);
        $this->assertTrue($this->povolen($b, 1, 12));
    }

    /**
     * Asistence obránce se počítají kolem pole, ODKUD se blokuje (ř. 1662-1669). Útočník stojí daleko
     * (tam by vyšla 1 kostka), na poli dopadu (9,7) ho obklopují dva soupeři ⇒ 3 proti 3+2 = 2 proti.
     */
    public function testKostkySeCtouZPoleDopadu(): void
    {
        $b = (new GameStateBuilder())->withActiveTeam(TeamSide::HOME)
            ->addPlayer(TeamSide::HOME, 3, 7, strength: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 10, 7, strength: 3, id: 12)
            ->addPlayer(TeamSide::AWAY, 8, 6, id: 13)
            ->addPlayer(TeamSide::AWAY, 8, 8, id: 14);
        $this->assertTrue($this->povolen($b, 1, 12, new Position(3, 7)), 'z místa, kde stojí: 1 kostka');
        $this->assertFalse($this->povolen($b, 1, 12, new Position(9, 7)), 'z pole dopadu: 2 proti');

        // Útočník na výchozím poli (7,6) drží pomocníka (8,6) v zóně — ten by tam neasistoval. Po doběhu
        // na (9,7) je zónou jen sám útočník, tedy cíl asistence (ř. 1662-1669 výjimka) ⇒ asistuje: 3 proti 4.
        $c = (new GameStateBuilder())->withActiveTeam(TeamSide::HOME)
            ->addPlayer(TeamSide::HOME, 7, 6, strength: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 10, 7, strength: 3, id: 12)
            ->addPlayer(TeamSide::AWAY, 8, 6, id: 13);
        $this->assertFalse($this->povolen($c, 1, 12, new Position(9, 7)), 'zóny podle pole dopadu, ne výchozího');
    }

    /** Pole dopadu: už sousedí = jeho pole; nedojde = null. */
    public function testPoleDopaduVRulesEngine(): void
    {
        $vedle = (new GameStateBuilder())->withActiveTeam(TeamSide::HOME)
            ->addPlayer(TeamSide::HOME, 10, 7, id: 1)
            ->addPlayer(TeamSide::AWAY, 11, 7, id: 12)->build();
        $this->assertEquals([12 => new Position(10, 7)], (new RulesEngine())->blitzLandingSquares($vedle, 1, [$vedle->requirePlayer(12)]));

        $daleko = (new GameStateBuilder())->withActiveTeam(TeamSide::HOME)
            ->addPlayer(TeamSide::HOME, 2, 7, movement: 4, id: 1)
            ->addPlayer(TeamSide::AWAY, 20, 7, id: 12)->build();
        $this->assertSame([12 => null], (new RulesEngine())->blitzLandingSquares($daleko, 1, [$daleko->requirePlayer(12)]));
    }

    /**
     * Pole dopadu je pole, ze kterého skutečný blitz v enginu blokuje (poslední krok před ranou). Soupeř
     * (8,6) dělá zónu nad cestou, takže pole kolem cíle nejsou rovnocenná.
     */
    public function testPoleDopaduJeTamKamBlitzOpravduDobehne(): void
    {
        $state = (new GameStateBuilder())->withActiveTeam(TeamSide::HOME)
            ->addPlayer(TeamSide::HOME, 5, 7, id: 1)
            ->addPlayer(TeamSide::AWAY, 10, 7, armour: 9, id: 12)
            ->addPlayer(TeamSide::AWAY, 8, 5, id: 13)
            ->withBallOffPitch()->build();
        $dopad = (new RulesEngine())->blitzLandingSquares($state, 1, [$state->requirePlayer(12)])[12];
        $this->assertNotNull($dopad);

        // rána: Defender Down (6), brnění 3+3 drží
        $vysledek = (new ActionResolver(new FixedDiceRoller([6, 3, 3])))
            ->resolve($state, ActionType::BLITZ, ['playerId' => 1, 'targetId' => 12]);
        $kroky = array_values(array_filter($vysledek->getEvents(), fn($e) => $e->getType() === 'player_move'));
        $posledni = end($kroky);
        $this->assertNotFalse($posledni, 'fixtura: blitz musí běžet');
        $this->assertSame(sprintf('(%d,%d)', $dopad->getX(), $dopad->getY()), $posledni->getData()['to']);
    }

    /**
     * Nabídka blitzu u obou koučů (Learning = živá hra s váhami, Greedy bez vah) — vstupní bod nabídky
     * pro hráče a typ akce (`buildScoredAction` / `scoreAction`), ne celé rozhodnutí: to by v těchhle
     * pozicích vyhrál pohyb a test by nic nehlídal. Kouč smí vrátit jiný cíl nebo nic, jen ne zakázaný.
     *
     * @return array<string, ?int> cíl nabídnutého blitzu podle kouče
     */
    private function cilBlitzu(\App\DTO\GameState $state): array
    {
        $rules = new RulesEngine();
        $learning = new \ReflectionMethod(LearningAICoach::class, 'buildScoredAction');
        $greedy = new \ReflectionMethod(GreedyAICoach::class, 'scoreAction');
        $l = $learning->invoke(new LearningAICoach(null, 0.0), $state, $rules, ActionType::BLITZ, 1, TeamSide::HOME, 0.0);
        $g = $greedy->invoke(new GreedyAICoach(), $state, $rules, ActionType::BLITZ, 1, TeamSide::HOME);

        return ['Learning' => $l['params']['targetId'] ?? null, 'Greedy' => $g['params']['targetId'] ?? null];
    }

    /** Jediný soupeř na dosah je nosič ST 5 proti našemu ST 2 = 3 proti. Dřív ho nabízeli oba kouči. */
    public function testKouciNenabidnouTriProtiNaNosice(): void
    {
        $state = (new GameStateBuilder())->withActiveTeam(TeamSide::HOME)
            ->addPlayer(TeamSide::HOME, 7, 7, strength: 2, id: 1)
            ->addPlayer(TeamSide::AWAY, 10, 7, strength: 5, id: 12)
            ->withBallCarried(12)->build();
        foreach ($this->cilBlitzu($state) as $kouc => $cil) {
            $this->assertNotSame(12, $cil, "{$kouc}: blitz 3 proti na nosiče");
        }
    }

    /** Jediný soupeř na dosah: ST 4 proti našemu ST 3 mimo nosiče (na postranní čáře, bonus za kraj). */
    public function testKouciNenabidnouDveProtiMimoNosice(): void
    {
        $state = (new GameStateBuilder())->withActiveTeam(TeamSide::HOME)
            ->addPlayer(TeamSide::HOME, 10, 2, strength: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 10, 0, strength: 4, id: 12)
            ->addPlayer(TeamSide::AWAY, 24, 14, id: 13)
            ->withBallCarried(13)->build();
        foreach ($this->cilBlitzu($state) as $kouc => $cil) {
            $this->assertNotSame(12, $cil, "{$kouc}: blitz 2 proti mimo nosiče");
        }
    }

    /**
     * Cíl, na jehož sousední pole hráč dojde jen se dvěma GFI: rána stojí pole (P82, `BlitzHandler::poleDopadu`),
     * takže blitz by jen přiblížil a blok se nekoná. Kouči ho nenabídnou (dřív Learning přes `sanceDojitK`
     * a Greedy bez kontroly dosahu nosiči +400).
     */
    public function testKouciNenabidnouBlitzBezRany(): void
    {
        $state = (new GameStateBuilder())->withActiveTeam(TeamSide::HOME)
            ->addPlayer(TeamSide::HOME, 2, 7, movement: 4, strength: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 9, 7, strength: 3, id: 12)
            ->withBallCarried(12)->build();
        $this->assertSame([12 => null], (new RulesEngine())->blitzLandingSquares($state, 1, [$state->requirePlayer(12)]), 'fixtura: bez pole na ránu');
        $this->assertNotSame([], array_filter(
            (new RulesEngine())->getValidMoveTargets($state, 1),
            fn($t) => max(abs($t['x'] - 9), abs($t['y'] - 7)) === 1,
        ), 'fixtura: na sousední pole cíle dojde (přes GFI)');
        foreach ($this->cilBlitzu($state) as $kouc => $cil) {
            $this->assertNotSame(12, $cil, "{$kouc}: blitz bez rány");
        }
    }

    /** Pozitivní kontrola: 2 proti NA NOSIČE kouči nabídnout smějí (a nabídnou — jiný cíl není). */
    public function testKouciNabidnouDveProtiNaNosice(): void
    {
        $state = (new GameStateBuilder())->withActiveTeam(TeamSide::HOME)
            ->addPlayer(TeamSide::HOME, 7, 7, strength: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 10, 7, strength: 4, id: 12)
            ->withBallCarried(12)->build();
        foreach ($this->cilBlitzu($state) as $kouc => $cil) {
            $this->assertSame(12, $cil, "{$kouc}: blitz 2 proti na nosiče");
        }
    }
}
