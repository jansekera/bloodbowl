<?php
declare(strict_types=1);

/**
 * Úroveň TV1500 pro pět měřených ras (kniha P165, uživatel 07.10.2026) — TATÁŽ jedenáctka jako
 * v C++ (`engine/src/roster.cpp`, `get*Roster1500`). Dva enginy, jedna sestava: kdo změní jednu
 * stranu, změní i druhou.
 *
 * Doktrína rohů klece: Stand Firm na rozích a Mighty Blow na dvou protilehlých rozích (mlátící tým),
 * agilní tým Side Step. Trpaslíci: rohy = čtyři Longbeardi („blitzeři nejsou vhodní na rohy, ale na
 * blitz a prořezávání cesty vpřed“); Stand Firm hlavně prorážečům (Blitzeři, Troll Slayeři), ne hráči
 * s Wrestle. Skaven: Rat Ogre i rychlý Gutter Runner v jedné sestavě. Výsledná hodnota týmu (ceny BB2016, 11 hráčů + 3 týmové
 * rerolly + lékárník): trpaslíci 1490 · wood-elf 1520 · skaven 1520 · ork 1500 · člověk 1490.
 *
 * Řádek: [pozice, počet, [MA, ST, AG, AV], skilly]. Skilly jsou ÚPLNÉ (vrozené i dokoupené).
 */

use App\Enum\SkillName;
use App\Enum\TeamSide;

require_once __DIR__ . '/race_rosters.php';

const TV1500_ROSTERS = [
    'Dwarf' => [
        ['Blocker',      2, [4, 3, 2, 9], [SkillName::Block, SkillName::Tackle, SkillName::ThickSkull, SkillName::Guard, SkillName::StandFirm]],
        ['Blocker',      2, [4, 3, 2, 9], [SkillName::Block, SkillName::Tackle, SkillName::ThickSkull, SkillName::MightyBlow]],
        ['Blitzer',      1, [5, 3, 3, 9], [SkillName::Block, SkillName::ThickSkull, SkillName::Tackle, SkillName::MightyBlow, SkillName::StandFirm]],
        ['Blitzer',      1, [5, 3, 3, 9], [SkillName::Block, SkillName::ThickSkull, SkillName::Tackle, SkillName::MightyBlow, SkillName::StandFirm, SkillName::Pro]],
        ['Troll Slayer', 2, [5, 3, 2, 8], [SkillName::Block, SkillName::Frenzy, SkillName::ThickSkull, SkillName::Dauntless, SkillName::Guard, SkillName::MightyBlow, SkillName::StandFirm]],
        ['Runner',       2, [6, 3, 3, 8], [SkillName::SureHands, SkillName::ThickSkull, SkillName::Block]],
        ['Blocker',      1, [4, 3, 2, 9], [SkillName::Block, SkillName::Tackle, SkillName::ThickSkull, SkillName::Wrestle]],
    ],
    'Wood Elf' => [
        ['Lineman',   3, [7, 3, 4, 7], [SkillName::Block]],
        ['Wardancer', 1, [8, 3, 4, 7], [SkillName::Block, SkillName::Dodge, SkillName::Leap, SkillName::StripBall, SkillName::Pro]],
        ['Wardancer', 1, [8, 3, 4, 7], [SkillName::Block, SkillName::Dodge, SkillName::Leap, SkillName::SideStep]],
        ['Catcher',   2, [8, 2, 4, 7], [SkillName::Catch, SkillName::Dodge, SkillName::Sprint, SkillName::Block, SkillName::DivingCatch]],
        ['Thrower',   1, [7, 3, 4, 7], [SkillName::Pass, SkillName::Block, SkillName::Accurate]],
        ['Treeman',   1, [2, 6, 1, 10], [SkillName::Loner, SkillName::TakeRoot, SkillName::StandFirm, SkillName::MightyBlow, SkillName::ThickSkull, SkillName::Guard]],
        ['Lineman',   2, [7, 3, 4, 7], [SkillName::Wrestle, SkillName::Dodge]],
    ],
    'Skaven' => [
        ['Lineman',       3, [7, 3, 3, 7], [SkillName::Wrestle]],
        ['Gutter Runner', 1, [10, 2, 4, 7], [SkillName::Dodge, SkillName::SureFeet, SkillName::Sprint, SkillName::Block, SkillName::SideStep]],
        ['Gutter Runner', 2, [9, 2, 4, 7], [SkillName::Dodge, SkillName::Block, SkillName::SideStep]],
        ['Gutter Runner', 1, [9, 2, 4, 7], [SkillName::Dodge, SkillName::Wrestle, SkillName::Horns, SkillName::SideStep]],
        ['Blitzer',       1, [7, 3, 3, 8], [SkillName::Block, SkillName::Guard, SkillName::MightyBlow]],
        ['Blitzer',       1, [7, 3, 3, 8], [SkillName::Block, SkillName::Guard]],
        ['Thrower',       1, [7, 3, 3, 7], [SkillName::SureHands, SkillName::Pass, SkillName::Block, SkillName::Pro]],
        ['Rat Ogre',      1, [6, 5, 2, 8], [SkillName::Loner, SkillName::Frenzy, SkillName::MightyBlow, SkillName::WildAnimal, SkillName::PrehensileTail]],
    ],
    'Orc' => [
        ['Blitzer',   2, [6, 3, 3, 9], [SkillName::Block, SkillName::Guard]],
        ['Blitzer',   1, [6, 3, 3, 9], [SkillName::Block, SkillName::MightyBlow, SkillName::Pro]],
        ['Blitzer',   1, [6, 3, 3, 9], [SkillName::Block, SkillName::StripBall, SkillName::Tackle]],
        ['Black Orc', 2, [4, 4, 2, 9], [SkillName::Guard, SkillName::Block, SkillName::StandFirm, SkillName::MightyBlow]],
        ['Black Orc', 2, [4, 4, 2, 9], [SkillName::Guard, SkillName::Block, SkillName::StandFirm]],
        ['Thrower',   1, [5, 3, 3, 8], [SkillName::SureHands, SkillName::Pass, SkillName::Block]],
        ['Lineman',   2, [5, 3, 3, 9], [SkillName::Wrestle]],
    ],
    'Human' => [
        ['Lineman',  1, [6, 3, 3, 8], [SkillName::Block]],
        ['Blitzer',  1, [7, 3, 3, 8], [SkillName::Block, SkillName::Guard, SkillName::StandFirm, SkillName::MightyBlow]],
        // uživatel 07.10.: Dauntless na jednom Blitzerovi (proti orkům), místo Mighty Blow; Stand Firm oběma s Guard
        ['Blitzer',  1, [7, 3, 3, 8], [SkillName::Block, SkillName::Guard, SkillName::StandFirm, SkillName::Dauntless]],
        // uživatel 07.10.: lovec Wrestle místo Stand Firm; Blitzer bez Guard Tackle místo Stand Firm
        ['Blitzer',  1, [7, 3, 3, 8], [SkillName::Block, SkillName::MightyBlow, SkillName::Tackle]],
        ['Blitzer',  1, [7, 3, 3, 8], [SkillName::Block, SkillName::StripBall, SkillName::Tackle, SkillName::Wrestle]],
        ['Thrower',  1, [6, 3, 3, 8], [SkillName::SureHands, SkillName::Pass, SkillName::Block, SkillName::Pro]],
        ['Catcher',  2, [8, 2, 3, 7], [SkillName::Catch, SkillName::Dodge, SkillName::Block, SkillName::SideStep]],
        ['Ogre',     1, [5, 5, 2, 9], [SkillName::Loner, SkillName::BoneHead, SkillName::MightyBlow, SkillName::ThickSkull, SkillName::ThrowTeamMate, SkillName::Block]],
        ['Lineman',  2, [6, 3, 3, 8], [SkillName::Wrestle]],
    ],
];

/**
 * Jedenáctka úrovně TV1500.
 *
 * @return array<int, \App\DTO\MatchPlayerDTO>
 */
function getTv1500RaceRoster(TeamSide $side, string $race): array
{
    if (!isset(TV1500_ROSTERS[$race])) {
        throw new \InvalidArgumentException("TV1500 soupiska není pro rasu: {$race}. Je pro: " . implode(', ', array_keys(TV1500_ROSTERS)));
    }

    return buildRosterFromEntries($side, TV1500_ROSTERS[$race]);
}
