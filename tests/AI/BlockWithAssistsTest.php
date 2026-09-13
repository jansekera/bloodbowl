<?php
declare(strict_types=1);

namespace App\Tests\AI;

use App\AI\LearningAICoach;
use App\Engine\RulesEngine;
use App\Enum\{ActionType, TeamSide};
use App\Tests\Engine\GameStateBuilder;
use PHPUnit\Framework\TestCase;

/**
 * ⭐⭐ Uživatel 12.09.: *„na blitz máme mít lepší kandidáty a asistenty."*
 *
 * Do 13.09. měl každý blok `+0,05` bez ohledu na to, jestli se hází **třemi
 * kostkami ve svůj prospěch**, jednou, nebo dokonce **dvěma proti**. Asistence
 * přitom engine počítá už dávno (`StrengthCalculator`).
 */
final class BlockWithAssistsTest extends TestCase
{
    public function testCoachBlocksWhereItHasTheDice(): void
    {
        // Dva naši mohou blokovat. #1 je sám proti ST 4 ⇒ kostky PROTI němu.
        // #2 má vedle cíle spoluhráče #3 jako asistenci ⇒ kostky pro něj.
        $state = (new GameStateBuilder())
            ->addPlayer(TeamSide::HOME, 5, 3, strength: 3, id: 1)
            ->addPlayer(TeamSide::AWAY, 6, 3, strength: 4, id: 2)
            ->addPlayer(TeamSide::HOME, 5, 10, strength: 3, id: 3)
            ->addPlayer(TeamSide::AWAY, 6, 10, strength: 3, id: 4)
            ->addPlayer(TeamSide::HOME, 6, 11, strength: 3, id: 5)   // asistence k #4
            ->withBallOffPitch()
            ->build();

        $rules = new RulesEngine();

        // SEBEKONTROLA: blok se nabízí OBĚMA, jinak by volba nic neznamenala.
        $blokujici = [];
        foreach ($rules->getAvailableActions($state) as $a) {
            if ($a['type'] === ActionType::BLOCK->value) {
                $blokujici[] = $a['playerId'];
            }
        }
        $this->assertContains(1, $blokujici, 'fixtura: #1 musí mít blok v nabídce');
        $this->assertContains(3, $blokujici, 'fixtura: #3 musí mít blok v nabídce');

        $decision = (new LearningAICoach())->decideAction($state, $rules);

        if ($decision['action'] === ActionType::BLOCK) {
            $this->assertNotSame(1, $decision['params']['playerId'],
                'kouč blokoval slabším hráčem proti ST 4, ačkoli vedle měl blok s asistencí');
        } else {
            // Blok se nezvolil vůbec -- taky správně, ten špatný se hrát nemá.
            $this->assertNotSame(ActionType::BLOCK, $decision['action']);
        }
    }
}
