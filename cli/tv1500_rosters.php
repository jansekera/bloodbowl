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
        // uživatel 4.11.: lovec (AG ≥ 4 + Strip Ball + Wrestle) i u elfů ⇒ Wrestle z Linemana na Wardancera, TV stejná
        ['Wardancer', 1, [8, 3, 4, 7], [SkillName::Block, SkillName::Dodge, SkillName::Leap, SkillName::StripBall, SkillName::Pro, SkillName::Wrestle]],
        ['Wardancer', 1, [8, 3, 4, 7], [SkillName::Block, SkillName::Dodge, SkillName::Leap, SkillName::SideStep]],
        ['Catcher',   1, [8, 2, 4, 7], [SkillName::Catch, SkillName::Dodge, SkillName::Sprint, SkillName::Block, SkillName::DivingCatch]],
        // uživatel 16.10.: jeden Catcher Nerves of Steel místo Diving Catch (chytá bez −1 za zóny)
        ['Catcher',   1, [8, 2, 4, 7], [SkillName::Catch, SkillName::Dodge, SkillName::Sprint, SkillName::Block, SkillName::NervesOfSteel]],
        ['Thrower',   1, [7, 3, 4, 7], [SkillName::Pass, SkillName::Block, SkillName::Accurate]],
        ['Treeman',   1, [2, 6, 1, 10], [SkillName::Loner, SkillName::TakeRoot, SkillName::StandFirm, SkillName::MightyBlow, SkillName::ThickSkull, SkillName::Guard]],
        // uživatel 16.10.: jeden Lineman bez Wrestle (jen Dodge), TV 1510
        ['Lineman',   1, [7, 3, 4, 7], [SkillName::Dodge]],
        ['Lineman',   1, [7, 3, 4, 7], [SkillName::Dodge]],
    ],
    'Skaven' => [
        // uživatel 18.10.: lovec +Strip Ball za Wrestle jednoho Linemana; hlavní Blitzer Strip Ball místo Guard
        ['Lineman',       2, [7, 3, 3, 7], [SkillName::Wrestle]],
        ['Lineman',       1, [7, 3, 3, 7], []],
        ['Gutter Runner', 1, [10, 2, 4, 7], [SkillName::Dodge, SkillName::SureFeet, SkillName::Sprint, SkillName::Block, SkillName::SideStep]],
        ['Gutter Runner', 2, [9, 2, 4, 7], [SkillName::Dodge, SkillName::Block, SkillName::SideStep]],
        ['Gutter Runner', 1, [9, 2, 4, 7], [SkillName::Dodge, SkillName::Wrestle, SkillName::Horns, SkillName::SideStep, SkillName::StripBall]],
        ['Blitzer',       1, [7, 3, 3, 8], [SkillName::Block, SkillName::StripBall, SkillName::MightyBlow]],
        ['Blitzer',       1, [7, 3, 3, 8], [SkillName::Block, SkillName::Guard]],
        ['Thrower',       1, [7, 3, 3, 7], [SkillName::SureHands, SkillName::Pass, SkillName::Block, SkillName::Pro]],
        ['Rat Ogre',      1, [6, 5, 2, 8], [SkillName::Loner, SkillName::Frenzy, SkillName::MightyBlow, SkillName::WildAnimal, SkillName::PrehensileTail]],
    ],
    'Orc' => [
        ['Blitzer',   2, [6, 3, 3, 9], [SkillName::Block, SkillName::Guard]],
        ['Blitzer',   1, [6, 3, 3, 9], [SkillName::Block, SkillName::MightyBlow, SkillName::Pro]],
        ['Blitzer',   1, [6, 3, 3, 9], [SkillName::Block, SkillName::StripBall, SkillName::Tackle]],
        ['Black Orc', 1, [4, 4, 2, 9], [SkillName::Guard, SkillName::Block, SkillName::StandFirm, SkillName::MightyBlow]],
        // uživatel 20.10.: Grab jednomu ze silných orků místo Mighty Blow (TV stejná)
        ['Black Orc', 1, [4, 4, 2, 9], [SkillName::Guard, SkillName::Block, SkillName::StandFirm, SkillName::Grab]],
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
    // P288 (audit goblinů 10.11.2026): táž jedenáctka jako C++ getGoblinRoster1500 (P267, „u goblinů jde o ověření
    // pravidel“) — co nejvíc goblinních mechanik. Star playeři Ripper Bolgrot (ř. 7627-7631) a Scrappa Sorehead
    // (ř. 7637-7641). 1270 + 3 rerolly 180 + lékárník 50 = 1500 (P287: 2 rerolly + lékárník + 1 úplatek = 1490, getTv1500TeamExtras).
    'Goblin' => [
        ['Goblin',         2, [6, 2, 3, 7], [SkillName::Dodge, SkillName::RightStuff, SkillName::Stunty, SkillName::SideStep, SkillName::SureFeet]],
        ['Goblin',         1, [6, 2, 3, 7], [SkillName::Dodge, SkillName::RightStuff, SkillName::Stunty, SkillName::SideStep, SkillName::SureHands]],
        ['Bombardier',     1, [6, 2, 3, 7], [SkillName::Bombardier, SkillName::Dodge, SkillName::SecretWeapon, SkillName::Stunty, SkillName::HailMaryPass]],
        ['Looney',         1, [6, 2, 3, 7], [SkillName::Chainsaw, SkillName::SecretWeapon, SkillName::Stunty]],
        ['Fanatic',        1, [3, 7, 3, 7], [SkillName::BallAndChain, SkillName::NoHands, SkillName::SecretWeapon, SkillName::Stunty]],
        ['Pogoer',         1, [7, 2, 3, 7], [SkillName::Dodge, SkillName::Leap, SkillName::VeryLongLegs, SkillName::Stunty, SkillName::SureFeet, SkillName::SideStep]],
        ['Troll',          1, [4, 5, 1, 9], [SkillName::Loner, SkillName::AlwaysHungry, SkillName::MightyBlow, SkillName::ReallyStupid, SkillName::Regeneration, SkillName::ThrowTeamMate, SkillName::Block]],
        ['Troll',          1, [4, 5, 1, 9], [SkillName::Loner, SkillName::AlwaysHungry, SkillName::MightyBlow, SkillName::ReallyStupid, SkillName::Regeneration, SkillName::ThrowTeamMate, SkillName::Block, SkillName::Guard]],
        ['Scrappa Sorehead', 1, [7, 2, 3, 7], [SkillName::Loner, SkillName::DirtyPlayer, SkillName::Dodge, SkillName::Leap, SkillName::RightStuff, SkillName::Sprint, SkillName::Stunty, SkillName::SureFeet, SkillName::VeryLongLegs]],
        ['Ripper Bolgrot', 1, [4, 6, 1, 9], [SkillName::Loner, SkillName::Grab, SkillName::MightyBlow, SkillName::Regeneration, SkillName::ThrowTeamMate]],
    ],
];

/**
 * P287 (10.11.2026): týmové přehozy a úplatky na úrovni TV1500 — tatáž čísla jako C++ TeamRoster `rerolls`/`bribes`.
 * Goblini 2 přehozy + 1 úplatek (uživatel 10.11.: „do týmu goblinů přidej do TV i nákup úplatků“); ostatní 3 a 0.
 *
 * @return array{rerolls: int, bribes: int}
 */
function getTv1500TeamExtras(string $race): array
{
    return $race === 'Goblin' ? ['rerolls' => 2, 'bribes' => 1] : ['rerolls' => 3, 'bribes' => 0];
}

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
