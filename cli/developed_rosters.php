<?php
declare(strict_types=1);

/**
 * Developed (TV ~1500) roster definitions for CLI simulation.
 *
 * Each race maps to a list of positional entries:
 * [count, [[extraSkills player1], [extraSkills player2], ...]]
 *
 * Entry order must match RACE_ROSTERS exactly.
 * Each inner array lists the extra skills for one player of that positional.
 */

use App\Enum\SkillName;
use App\Enum\TeamSide;

require_once __DIR__ . '/race_rosters.php';

const DEVELOPED_ROSTERS = [
    // === BASH CATEGORY ===
    'Human' => [
        // Blitzer (4): 2×Guard, 1×MightyBlow, 1×Tackle
        [4, [[SkillName::Guard], [SkillName::Guard], [SkillName::MightyBlow], [SkillName::Tackle]]],
        // Catcher (2): Block
        [2, [[SkillName::Block], [SkillName::Block]]],
        // Thrower (1): Block
        [1, [[SkillName::Block]]],
        // Lineman (4): 1×Kick
        [4, [[SkillName::Kick], [], [], []]],
    ],
    'Orc' => [
        // Blitzer (4): 2×Guard, 2×MightyBlow
        [4, [[SkillName::Guard], [SkillName::Guard], [SkillName::MightyBlow], [SkillName::MightyBlow]]],
        // Black Orc (4): all Block
        [4, [[SkillName::Block], [SkillName::Block], [SkillName::Block], [SkillName::Block]]],
        // Thrower (1): Block
        [1, [[SkillName::Block]]],
        // Lineman (2): —
        [2, [[], []]],
    ],
    'Dwarf' => [
        // Blitzer (2): Guard
        [2, [[SkillName::Guard], [SkillName::Guard]]],
        // Troll Slayer (2): Block
        [2, [[SkillName::Block], [SkillName::Block]]],
        // Runner (2): Block
        [2, [[SkillName::Block], [SkillName::Block]]],
        // Blocker (5): all Guard
        [5, [[SkillName::Guard], [SkillName::Guard], [SkillName::Guard], [SkillName::Guard], [SkillName::Guard]]],
    ],
    'Chaos' => [
        // Chaos Warrior (4): all Block
        [4, [[SkillName::Block], [SkillName::Block], [SkillName::Block], [SkillName::Block]]],
        // Beastman (7): 4×Block, 2×Tackle, 1×Kick
        [7, [[SkillName::Block], [SkillName::Block], [SkillName::Block], [SkillName::Block], [SkillName::Tackle], [SkillName::Tackle], [SkillName::Kick]]],
    ],
    'Undead' => [
        // Mummy (2): Guard
        [2, [[SkillName::Guard], [SkillName::Guard]]],
        // Wight (2): Guard
        [2, [[SkillName::Guard], [SkillName::Guard]]],
        // Ghoul (4): 2×Block, 2×SureHands
        [4, [[SkillName::Block], [SkillName::Block], [SkillName::SureHands], [SkillName::SureHands]]],
        // Zombie (3): —
        [3, [[], [], []]],
    ],
    'Lizardmen' => [
        // Saurus (6): all Block
        [6, [[SkillName::Block], [SkillName::Block], [SkillName::Block], [SkillName::Block], [SkillName::Block], [SkillName::Block]]],
        // Skink (5): 2×SureHands
        [5, [[SkillName::SureHands], [SkillName::SureHands], [], [], []]],
    ],
    'Necromantic' => [
        // Werewolf (2): MightyBlow
        [2, [[SkillName::MightyBlow], [SkillName::MightyBlow]]],
        // Flesh Golem (2): Block
        [2, [[SkillName::Block], [SkillName::Block]]],
        // Wight (2): Guard
        [2, [[SkillName::Guard], [SkillName::Guard]]],
        // Ghoul (2): Block
        [2, [[SkillName::Block], [SkillName::Block]]],
        // Zombie (3): —
        [3, [[], [], []]],
    ],
    'Bretonnian' => [
        // Blitzer (4): 2×Guard, 2×MightyBlow
        [4, [[SkillName::Guard], [SkillName::Guard], [SkillName::MightyBlow], [SkillName::MightyBlow]]],
        // Blocker (4): Guard
        [4, [[SkillName::Guard], [SkillName::Guard], [SkillName::Guard], [SkillName::Guard]]],
        // Peasant Lineman (3): 1×Kick
        [3, [[SkillName::Kick], [], []]],
    ],
    'Khemri' => [
        // Tomb Guardian (4): Block
        [4, [[SkillName::Block], [SkillName::Block], [SkillName::Block], [SkillName::Block]]],
        // Blitz-Ra (2): MightyBlow
        [2, [[SkillName::MightyBlow], [SkillName::MightyBlow]]],
        // Thro-Ra (2): Block
        [2, [[SkillName::Block], [SkillName::Block]]],
        // Skeleton (3): —
        [3, [[], [], []]],
    ],
    'Norse' => [
        // Berserker (2): MightyBlow
        [2, [[SkillName::MightyBlow], [SkillName::MightyBlow]]],
        // Runner (2): MightyBlow
        [2, [[SkillName::MightyBlow], [SkillName::MightyBlow]]],
        // Thrower (2): Accurate
        [2, [[SkillName::Accurate], [SkillName::Accurate]]],
        // Lineman (5): 2×Guard, 1×Kick
        [5, [[SkillName::Guard], [SkillName::Guard], [SkillName::Kick], [], []]],
    ],
    'Khorne' => [
        // Bloodthirster (1): — (big guy, no extra)
        [1, [[]]],
        // Khorne Herald (2): MightyBlow
        [2, [[SkillName::MightyBlow], [SkillName::MightyBlow]]],
        // Bloodletter (4): Block
        [4, [[SkillName::Block], [SkillName::Block], [SkillName::Block], [SkillName::Block]]],
        // Pit Fighter (4): Block
        [4, [[SkillName::Block], [SkillName::Block], [SkillName::Block], [SkillName::Block]]],
    ],
    'Chaos Pact' => [
        // Minotaur (1): —
        [1, [[]]],
        // Ogre (1): —
        [1, [[]]],
        // Troll (1): —
        [1, [[]]],
        // Dark Elf Renegade (1): —
        [1, [[]]],
        // Goblin Renegade (1): —
        [1, [[]]],
        // Skaven Renegade (1): —
        [1, [[]]],
        // Marauder (5): 4×Block, 1×Kick
        [5, [[SkillName::Block], [SkillName::Block], [SkillName::Block], [SkillName::Block], [SkillName::Kick]]],
    ],
    'Vampire' => [
        // Vampire (4): Block
        [4, [[SkillName::Block], [SkillName::Block], [SkillName::Block], [SkillName::Block]]],
        // Thrall (7): 2×Block
        [7, [[SkillName::Block], [SkillName::Block], [], [], [], [], []]],
    ],
    'Chaos Dwarf' => [
        // Minotaur (1): —
        [1, [[]]],
        // Bull Centaur (2): Block
        [2, [[SkillName::Block], [SkillName::Block]]],
        // Blocker (4): Guard
        [4, [[SkillName::Guard], [SkillName::Guard], [SkillName::Guard], [SkillName::Guard]]],
        // Hobgoblin (4): 1×Kick
        [4, [[SkillName::Kick], [], [], []]],
    ],
    'Nurgle' => [
        // Beast of Nurgle (1): —
        [1, [[]]],
        // Nurgle Warrior (4): Block
        [4, [[SkillName::Block], [SkillName::Block], [SkillName::Block], [SkillName::Block]]],
        // Pestigor (4): 2×Block, 2×Guard
        [4, [[SkillName::Block], [SkillName::Block], [SkillName::Guard], [SkillName::Guard]]],
        // Rotter (2): —
        [2, [[], []]],
    ],
    'Ogre' => [
        // Ogre (6): 2×Guard
        [6, [[SkillName::Guard], [SkillName::Guard], [], [], [], []]],
        // Snotling (5): —
        [5, [[], [], [], [], []]],
    ],
    'Goblin' => [
        // Troll (2): —
        [2, [[], []]],
        // Bombardier (1): —
        [1, [[]]],
        // Looney (1): —
        [1, [[]]],
        // Fanatic (1): —
        [1, [[]]],
        // Pogoer (1): —
        [1, [[]]],
        // Goblin (5): 1×DirtyPlayer, 1×SideStep
        [5, [[SkillName::DirtyPlayer], [SkillName::SideStep], [], [], []]],
    ],
    'Halfling' => [
        // Treeman (2): Guard
        [2, [[SkillName::Guard], [SkillName::Guard]]],
        // Halfling (9): 1×DirtyPlayer, 1×SideStep
        [9, [[SkillName::DirtyPlayer], [SkillName::SideStep], [], [], [], [], [], [], []]],
    ],

    // === AGILITY CATEGORY ===
    'Wood Elf' => [
        // Wardancer (2): 1×StripBall, 1×Tackle
        [2, [[SkillName::StripBall], [SkillName::Tackle]]],
        // Catcher (4): 2×Block, 2×Wrestle
        [4, [[SkillName::Block], [SkillName::Block], [SkillName::Wrestle], [SkillName::Wrestle]]],
        // Thrower (1): SureHands
        [1, [[SkillName::SureHands]]],
        // Lineman (4): 2×Dodge, 1×Wrestle, 1×Kick
        [4, [[SkillName::Dodge], [SkillName::Dodge], [SkillName::Wrestle], [SkillName::Kick]]],
    ],
    'Dark Elf' => [
        // Blitzer (4): all Dodge
        [4, [[SkillName::Dodge], [SkillName::Dodge], [SkillName::Dodge], [SkillName::Dodge]]],
        // Witch Elf (2): 1×Wrestle, 1×SideStep
        [2, [[SkillName::Wrestle], [SkillName::SideStep]]],
        // Runner (2): Dodge
        [2, [[SkillName::Dodge], [SkillName::Dodge]]],
        // Lineman (3): 2×Dodge, 1×Kick
        [3, [[SkillName::Dodge], [SkillName::Dodge], [SkillName::Kick]]],
    ],
    'High Elf' => [
        // Blitzer (4): 2×Dodge, 2×Tackle
        [4, [[SkillName::Dodge], [SkillName::Dodge], [SkillName::Tackle], [SkillName::Tackle]]],
        // Catcher (2): Block
        [2, [[SkillName::Block], [SkillName::Block]]],
        // Thrower (2): Accurate
        [2, [[SkillName::Accurate], [SkillName::Accurate]]],
        // Lineman (3): 2×Dodge, 1×Kick
        [3, [[SkillName::Dodge], [SkillName::Dodge], [SkillName::Kick]]],
    ],
    'Pro Elf' => [
        // Blitzer (2): Dodge
        [2, [[SkillName::Dodge], [SkillName::Dodge]]],
        // Catcher (4): 2×Block, 2×Dodge
        [4, [[SkillName::Block], [SkillName::Block], [SkillName::Dodge], [SkillName::Dodge]]],
        // Thrower (2): Accurate
        [2, [[SkillName::Accurate], [SkillName::Accurate]]],
        // Lineman (3): 2×Dodge, 1×Kick
        [3, [[SkillName::Dodge], [SkillName::Dodge], [SkillName::Kick]]],
    ],
    'Skaven' => [
        // Blitzer (2): Guard
        [2, [[SkillName::Guard], [SkillName::Guard]]],
        // Gutter Runner (4): 2×Block, 2×Wrestle
        [4, [[SkillName::Block], [SkillName::Block], [SkillName::Wrestle], [SkillName::Wrestle]]],
        // Thrower (1): Block
        [1, [[SkillName::Block]]],
        // Lineman (4): 1×Kick, 1×DirtyPlayer
        [4, [[SkillName::Kick], [SkillName::DirtyPlayer], [], []]],
    ],
    'Amazon' => [
        // Blitzer (4): 2×MightyBlow, 2×Tackle
        [4, [[SkillName::MightyBlow], [SkillName::MightyBlow], [SkillName::Tackle], [SkillName::Tackle]]],
        // Catcher (2): Block
        [2, [[SkillName::Block], [SkillName::Block]]],
        // Thrower (2): Block
        [2, [[SkillName::Block], [SkillName::Block]]],
        // Linewoman (3): 2×Wrestle, 1×Kick
        [3, [[SkillName::Wrestle], [SkillName::Wrestle], [SkillName::Kick]]],
    ],
    'Slann' => [
        // Kroxigor (1): —
        [1, [[]]],
        // Blitzer (4): 2×Block, 2×Wrestle
        [4, [[SkillName::Block], [SkillName::Block], [SkillName::Wrestle], [SkillName::Wrestle]]],
        // Catcher (4): 2×Block, 2×Dodge
        [4, [[SkillName::Block], [SkillName::Block], [SkillName::Dodge], [SkillName::Dodge]]],
        // Lineman (2): Wrestle
        [2, [[SkillName::Wrestle], [SkillName::Wrestle]]],
    ],
    'Underworld' => [
        // Troll (1): —
        [1, [[]]],
        // Skaven Blitzer (2): Guard
        [2, [[SkillName::Guard], [SkillName::Guard]]],
        // Skaven Thrower (2): Block
        [2, [[SkillName::Block], [SkillName::Block]]],
        // Underworld Goblin (2): SideStep
        [2, [[SkillName::SideStep], [SkillName::SideStep]]],
        // Skaven Lineman (4): 1×Kick
        [4, [[SkillName::Kick], [], [], []]],
    ],
];

/**
 * Get a developed (TV ~1500) race roster with extra skills applied.
 *
 * @return array<int, \App\DTO\MatchPlayerDTO>
 */
function getDevelopedRaceRoster(TeamSide $side, string $race): array
{
    // Get base roster first
    $players = getRaceRoster($side, $race);

    if (!isset(DEVELOPED_ROSTERS[$race])) {
        return $players;
    }

    $developedEntries = DEVELOPED_ROSTERS[$race];
    $baseEntries = RACE_ROSTERS[$race];

    // Iterate through positional entries and apply extra skills
    $playerIdx = 0;
    $playerIds = array_keys($players);

    foreach ($developedEntries as $posIdx => $devEntry) {
        $count = $devEntry[0];
        $skillSets = $devEntry[1];

        for ($i = 0; $i < $count; $i++) {
            $id = $playerIds[$playerIdx];
            $player = $players[$id];
            $extraSkills = $skillSets[$i] ?? [];

            if (!empty($extraSkills)) {
                $mergedSkills = array_merge($player->getSkills(), $extraSkills);
                $players[$id] = $player->withSkills($mergedSkills);
            }

            $playerIdx++;
        }
    }

    return $players;
}
