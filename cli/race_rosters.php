<?php
declare(strict_types=1);

/**
 * Race roster definitions for CLI simulation.
 *
 * Each race maps to a list of positional entries:
 * [positionalName, count, [MA, ST, AG, AV], [SkillName cases...]]
 *
 * Total players per race = 11.
 */

use App\DTO\MatchPlayerDTO;
use App\Enum\PlayerState;
use App\Enum\SkillName;
use App\Enum\TeamSide;
use App\ValueObject\PlayerStats;
use App\ValueObject\Position;

const RACE_ROSTERS = [
    'Human' => [
        ['Blitzer',  4, [7, 3, 3, 8], [SkillName::Block]],
        ['Catcher',  2, [8, 2, 3, 7], [SkillName::Dodge, SkillName::Catch]],
        ['Thrower',  1, [6, 3, 3, 8], [SkillName::SureHands]],
        ['Lineman',  4, [6, 3, 3, 8], []],
    ],
    'Orc' => [
        ['Blitzer',   4, [6, 3, 3, 9], [SkillName::Block]],
        ['Black Orc', 4, [4, 4, 2, 9], []],
        ['Thrower',   1, [5, 3, 3, 8], [SkillName::SureHands]],
        ['Lineman',   2, [5, 3, 3, 9], []],
    ],
    'Skaven' => [
        ['Blitzer',       2, [7, 3, 3, 8], [SkillName::Block]],
        ['Gutter Runner', 4, [9, 2, 4, 7], [SkillName::Dodge]],
        ['Thrower',       1, [7, 3, 3, 7], [SkillName::SureHands]],
        ['Lineman',       4, [7, 3, 3, 7], []],
    ],
    'Dwarf' => [
        ['Blitzer',      2, [5, 3, 3, 9], [SkillName::Block, SkillName::ThickSkull]],
        ['Troll Slayer',  2, [5, 3, 2, 8], [SkillName::Frenzy, SkillName::Dauntless, SkillName::ThickSkull]],
        ['Runner',       2, [6, 3, 3, 8], [SkillName::SureHands, SkillName::ThickSkull]],
        ['Blocker',      5, [4, 3, 2, 9], [SkillName::Block, SkillName::Tackle, SkillName::ThickSkull]],
    ],
    'Wood Elf' => [
        ['Wardancer', 2, [8, 3, 4, 7], [SkillName::Block, SkillName::Dodge]],
        ['Catcher',   4, [8, 2, 4, 7], [SkillName::Dodge, SkillName::Catch]],
        ['Thrower',   1, [7, 3, 4, 7], [SkillName::Pass]],
        ['Lineman',   4, [7, 3, 4, 7], []],
    ],
    'Chaos' => [
        ['Chaos Warrior', 4, [5, 4, 3, 9], []],
        ['Beastman',      7, [6, 3, 3, 8], [SkillName::Horns]],
    ],
    'Undead' => [
        ['Mummy',    2, [3, 5, 1, 9], [SkillName::MightyBlow, SkillName::Regeneration]],
        ['Wight',    2, [6, 3, 3, 8], [SkillName::Block, SkillName::Regeneration]],
        ['Ghoul',    4, [7, 3, 3, 7], [SkillName::Dodge]],
        ['Zombie',   3, [4, 3, 2, 8], [SkillName::Regeneration]],
    ],
    'Lizardmen' => [
        ['Saurus', 6, [6, 4, 1, 9], []],
        ['Skink',  5, [8, 2, 3, 7], [SkillName::Dodge, SkillName::Stunty]],
    ],
    'Dark Elf' => [
        ['Blitzer',   4, [7, 3, 4, 8], [SkillName::Block]],
        ['Witch Elf', 2, [7, 3, 4, 7], [SkillName::Dodge, SkillName::Frenzy, SkillName::JumpUp]],
        ['Runner',    2, [7, 3, 4, 7], [SkillName::DumpOff]],
        ['Lineman',   3, [6, 3, 4, 8], []],
    ],
    'Halfling' => [
        ['Treeman',  2, [2, 6, 1, 10], [SkillName::Loner, SkillName::MightyBlow, SkillName::StandFirm, SkillName::ThickSkull, SkillName::TakeRoot, SkillName::ThrowTeamMate]],
        ['Halfling', 9, [6, 2, 3, 6],  [SkillName::Dodge, SkillName::Stunty, SkillName::RightStuff]],
    ],
    'Norse' => [
        ['Berserker', 2, [6, 3, 3, 7], [SkillName::Block, SkillName::Frenzy, SkillName::JumpUp]],
        ['Runner',    2, [7, 3, 3, 7], [SkillName::Block, SkillName::Dauntless]],
        ['Thrower',   2, [6, 3, 3, 7], [SkillName::Block, SkillName::Pass]],
        ['Lineman',   5, [6, 3, 3, 7], [SkillName::Block]],
    ],
    'High Elf' => [
        ['Blitzer',  4, [7, 3, 4, 8], [SkillName::Block]],
        ['Catcher',  2, [8, 3, 4, 7], [SkillName::Catch]],
        ['Thrower',  2, [6, 3, 4, 8], [SkillName::Pass, SkillName::SafeThrow]],
        ['Lineman',  3, [6, 3, 4, 8], []],
    ],
    'Vampire' => [
        ['Vampire',  4, [6, 4, 4, 8], [SkillName::HypnoticGaze, SkillName::Regeneration, SkillName::Bloodlust]],
        ['Thrall',   7, [6, 3, 3, 7], []],
    ],
    'Amazon' => [
        ['Blitzer',   4, [6, 3, 3, 7], [SkillName::Block, SkillName::Dodge]],
        ['Catcher',   2, [6, 3, 3, 7], [SkillName::Dodge, SkillName::Catch]],
        ['Thrower',   2, [6, 3, 3, 7], [SkillName::Dodge, SkillName::Pass]],
        ['Linewoman', 3, [6, 3, 3, 7], [SkillName::Dodge]],
    ],
    'Necromantic' => [
        ['Werewolf',    2, [8, 3, 3, 8], [SkillName::Claw, SkillName::Frenzy, SkillName::Regeneration]],
        ['Flesh Golem', 2, [4, 4, 2, 9], [SkillName::Regeneration, SkillName::StandFirm]],
        ['Wight',       2, [6, 3, 3, 8], [SkillName::Block, SkillName::Regeneration]],
        ['Ghoul',       2, [7, 3, 3, 7], [SkillName::Dodge]],
        ['Zombie',      3, [4, 3, 2, 8], [SkillName::Regeneration]],
    ],
    'Bretonnian' => [
        ['Blitzer',          4, [7, 3, 3, 8], [SkillName::Block, SkillName::Catch, SkillName::Dauntless]],
        ['Blocker',          4, [6, 3, 3, 8], [SkillName::Wrestle, SkillName::Fend]],
        ['Peasant Lineman',  3, [6, 3, 3, 7], [SkillName::Fend]],
    ],
    'Khemri' => [
        ['Tomb Guardian', 4, [4, 5, 1, 9], [SkillName::Decay, SkillName::Regeneration]],
        ['Blitz-Ra',      2, [6, 3, 2, 8], [SkillName::Block, SkillName::Regeneration]],
        ['Thro-Ra',       2, [6, 3, 2, 7], [SkillName::Pass, SkillName::Regeneration, SkillName::SureHands]],
        ['Skeleton',      3, [5, 3, 2, 7], [SkillName::Regeneration, SkillName::ThickSkull]],
    ],
    'Goblin' => [
        ['Troll',      2, [4, 5, 1, 9], [SkillName::Loner, SkillName::MightyBlow, SkillName::ReallyStupid, SkillName::Regeneration, SkillName::ThrowTeamMate, SkillName::AlwaysHungry]],
        ['Bombardier', 1, [6, 2, 3, 7], [SkillName::Bombardier, SkillName::Dodge, SkillName::SecretWeapon, SkillName::Stunty]],
        ['Looney',     1, [6, 2, 3, 7], [SkillName::Chainsaw, SkillName::SecretWeapon, SkillName::Stunty]],
        ['Fanatic',    1, [3, 7, 3, 7], [SkillName::BallAndChain, SkillName::NoHands, SkillName::SecretWeapon, SkillName::Stunty]],
        ['Pogoer',     1, [7, 2, 3, 7], [SkillName::Dodge, SkillName::Leap, SkillName::VeryLongLegs, SkillName::Stunty]],
        ['Goblin',     5, [6, 2, 3, 7], [SkillName::Dodge, SkillName::RightStuff, SkillName::Stunty]],
    ],
    'Chaos Dwarf' => [
        ['Minotaur',      1, [5, 5, 2, 8], [SkillName::Loner, SkillName::Frenzy, SkillName::Horns, SkillName::MightyBlow, SkillName::ThickSkull, SkillName::WildAnimal]],
        ['Bull Centaur',  2, [6, 4, 2, 9], [SkillName::Sprint, SkillName::SureFeet, SkillName::ThickSkull]],
        ['Blocker',       4, [4, 3, 2, 9], [SkillName::Block, SkillName::ThickSkull, SkillName::Tackle]],
        ['Hobgoblin',     4, [6, 3, 3, 7], []],
    ],
    'Ogre' => [
        ['Ogre',     6, [5, 5, 2, 9], [SkillName::Loner, SkillName::BoneHead, SkillName::MightyBlow, SkillName::ThickSkull, SkillName::ThrowTeamMate, SkillName::AlwaysHungry]],
        ['Snotling', 5, [5, 1, 3, 5], [SkillName::Dodge, SkillName::RightStuff, SkillName::SideStep, SkillName::Stunty]],
    ],
    'Nurgle' => [
        ['Beast of Nurgle', 1, [4, 5, 1, 9], [SkillName::Loner, SkillName::DisturbingPresence, SkillName::FoulAppearance, SkillName::MightyBlow, SkillName::Regeneration, SkillName::ReallyStupid, SkillName::Tentacles]],
        ['Nurgle Warrior',  4, [4, 4, 2, 9], [SkillName::DisturbingPresence, SkillName::FoulAppearance, SkillName::Regeneration]],
        ['Pestigor',        4, [6, 3, 3, 8], [SkillName::Horns, SkillName::Regeneration, SkillName::DisturbingPresence, SkillName::NurglesRot]],
        ['Rotter',          2, [5, 3, 3, 8], [SkillName::Decay, SkillName::NurglesRot]],
    ],
    'Pro Elf' => [
        ['Blitzer',  2, [7, 3, 4, 8], [SkillName::Block, SkillName::SideStep]],
        ['Catcher',  4, [8, 3, 4, 7], [SkillName::Catch, SkillName::NervesOfSteel]],
        ['Thrower',  2, [6, 3, 4, 7], [SkillName::Pass, SkillName::SafeThrow]],
        ['Lineman',  3, [6, 3, 4, 7], []],
    ],
    'Slann' => [
        ['Kroxigor', 1, [6, 5, 1, 9], [SkillName::Loner, SkillName::BoneHead, SkillName::MightyBlow, SkillName::PrehensileTail, SkillName::ThickSkull]],
        ['Blitzer',  4, [7, 3, 3, 8], [SkillName::Leap, SkillName::VeryLongLegs, SkillName::DivingTackle]],
        ['Catcher',  4, [7, 3, 4, 7], [SkillName::Leap, SkillName::VeryLongLegs, SkillName::DivingCatch]],
        ['Lineman',  2, [6, 3, 3, 8], [SkillName::Leap, SkillName::VeryLongLegs]],
    ],
    'Underworld' => [
        ['Troll',              1, [4, 5, 1, 9], [SkillName::Loner, SkillName::MightyBlow, SkillName::ReallyStupid, SkillName::Regeneration, SkillName::ThrowTeamMate, SkillName::AlwaysHungry], 'Troll'],
        ['Skaven Blitzer',     2, [7, 3, 3, 8], [SkillName::Animosity, SkillName::Block], 'Skaven'],
        ['Skaven Thrower',     2, [7, 3, 3, 7], [SkillName::Animosity, SkillName::Pass, SkillName::SureHands], 'Skaven'],
        ['Underworld Goblin',  2, [6, 2, 3, 7], [SkillName::Dodge, SkillName::RightStuff, SkillName::Stunty, SkillName::Animosity], 'Goblin'],
        ['Skaven Lineman',     4, [7, 3, 3, 7], [SkillName::Animosity], 'Skaven'],
    ],
    'Khorne' => [
        ['Bloodthirster',  1, [6, 5, 1, 9], [SkillName::Loner, SkillName::WildAnimal, SkillName::Claw, SkillName::Frenzy, SkillName::Horns, SkillName::Juggernaut, SkillName::Regeneration]],
        ['Khorne Herald',  2, [6, 3, 3, 8], [SkillName::Frenzy, SkillName::Horns, SkillName::Juggernaut]],
        ['Bloodletter',    4, [6, 3, 3, 8], [SkillName::Horns, SkillName::Juggernaut, SkillName::Regeneration]],
        ['Pit Fighter',    4, [6, 3, 3, 8], [SkillName::Frenzy]],
    ],
    'Chaos Pact' => [
        ['Minotaur',           1, [5, 5, 2, 8], [SkillName::Loner, SkillName::Frenzy, SkillName::Horns, SkillName::MightyBlow, SkillName::ThickSkull, SkillName::WildAnimal], 'Minotaur'],
        ['Ogre',               1, [5, 5, 2, 9], [SkillName::Loner, SkillName::BoneHead, SkillName::MightyBlow, SkillName::ThickSkull, SkillName::ThrowTeamMate], 'Ogre'],
        ['Troll',              1, [4, 5, 1, 9], [SkillName::Loner, SkillName::AlwaysHungry, SkillName::MightyBlow, SkillName::ReallyStupid, SkillName::Regeneration, SkillName::ThrowTeamMate], 'Troll'],
        ['Dark Elf Renegade',  1, [6, 3, 4, 8], [SkillName::Animosity], 'DarkElf'],
        ['Goblin Renegade',    1, [6, 2, 3, 7], [SkillName::Animosity, SkillName::Dodge, SkillName::RightStuff, SkillName::Stunty], 'Goblin'],
        ['Skaven Renegade',    1, [7, 3, 3, 7], [SkillName::Animosity], 'Skaven'],
        ['Marauder',           5, [6, 3, 3, 8], []],
    ],
];

function getRaceRoster(TeamSide $side, string $race): array
{
    if (!isset(RACE_ROSTERS[$race])) {
        throw new \InvalidArgumentException("Unknown race: {$race}. Available: " . implode(', ', array_keys(RACE_ROSTERS)));
    }

    $players = [];
    $teamPrefix = $side === TeamSide::HOME ? 'H' : 'A';
    $idOffset = $side === TeamSide::HOME ? 1 : 12;
    $idx = 0;

    foreach (RACE_ROSTERS[$race] as $entry) {
        $positionalName = $entry[0];
        $count = $entry[1];
        $statArr = $entry[2];
        $skills = $entry[3];
        $raceName = $entry[4] ?? null;
        [$ma, $st, $ag, $av] = $statArr;
        for ($i = 0; $i < $count; $i++) {
            $id = $idOffset + $idx;
            $num = $idx + 1;
            $players[$id] = MatchPlayerDTO::create(
                id: $id,
                playerId: $id,
                name: "{$teamPrefix}-{$positionalName} {$num}",
                number: $num,
                positionalName: $positionalName,
                stats: new PlayerStats($ma, $st, $ag, $av),
                skills: $skills,
                teamSide: $side,
                position: new Position(0, 0),
                raceName: $raceName,
            )->withPosition(null)->withState(PlayerState::OFF_PITCH);
            $idx++;
        }
    }

    return $players;
}
