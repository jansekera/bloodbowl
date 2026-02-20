<?php

declare(strict_types=1);

require __DIR__ . '/vendor/autoload.php';

use App\Database;

$pdo = Database::getConnection();

echo "Seeding Blood Bowl database...\n";

// ========== SKILLS ==========
$skills = [
    // General
    ['Block', 'General', 'Adds block choice to block dice results, preventing knockdown'],
    ['Dodge', 'Agility', 'Allows reroll of failed dodge, opponent must choose stumble on block'],
    ['Sure Hands', 'General', 'Allows reroll of failed pickup'],
    ['Sure Feet', 'General', 'Allows reroll of failed GFI'],

    ['Frenzy', 'General', 'Must follow up blocks, second block if pushed back'],
    ['Kick', 'General', 'Allows more accurate kickoff placement'],
    ['Tackle', 'General', 'Opponent cannot use Dodge skill against blocks'],
    ['Pro', 'General', 'Once per turn, reroll any dice roll on 4+'],
    ['Strip Ball', 'General', 'Forces ball carrier to drop ball when pushed back'],
    ['Dauntless', 'General', 'Roll to match stronger opponent\'s strength on block'],
    ['Dirty Player', 'General', '+1 to armor or injury roll when fouling'],
    ['Fend', 'General', 'Opponent cannot follow up after a block'],
    ['Kick-Off Return', 'General', 'Free move for one player after kickoff'],
    ['Leader', 'General', 'Grants an extra team reroll'],
    ['Wrestle', 'General', 'Both players placed prone instead of push on block'],

    // Agility
    ['Catch', 'Agility', 'Allows reroll of failed catch'],
    ['Diving Catch', 'Agility', '+1 to catch on accurate pass in adjacent square'],
    ['Diving Tackle', 'Agility', '+1 TZ penalty for dodging, player goes prone'],
    ['Sprint', 'Agility', 'Allows a third GFI attempt'],
    ['Leap', 'Agility', 'Jump over adjacent squares ignoring tackle zones'],
    ['Side Step', 'Agility', 'Choose pushback square instead of opponent'],
    ['Jump Up', 'Agility', 'Stand up for free and block'],
    ['Sneaky Git', 'Agility', 'Avoid being sent off for fouling on doubles'],

    // Strength
    ['Mighty Blow', 'Strength', '+1 to armor or injury roll when blocking'],
    ['Guard', 'Strength', 'Provide assist even in tackle zones'],
    ['Stand Firm', 'Strength', 'Cannot be pushed back from a block'],
    ['Piling On', 'Strength', 'Reroll armor or injury roll (go prone)'],
    ['Grab', 'Strength', 'Choose pushback direction on successful block'],
    ['Break Tackle', 'Strength', 'Use ST instead of AG for dodge rolls'],
    ['Thick Skull', 'Strength', 'Treat stunned as KO\'d on injury table'],
    ['Juggernaut', 'Strength', 'Cannot be wrestled or fended on blitz'],

    // Passing
    ['Accurate', 'Passing', '+1 to passing roll'],
    ['Strong Arm', 'Passing', '+1 to range category for passing'],
    ['Safe Throw', 'Passing', 'Reroll interception attempts against you'],
    ['Nerves of Steel', 'Passing', 'Ignore tackle zones for pass, catch, intercept'],
    ['Dump-Off', 'Passing', 'Quick pass when blocked'],
    ['Pass', 'Passing', 'Allows reroll of failed pass'],
    ['Hail Mary Pass', 'Passing', 'Throw anywhere on pitch (inaccurate)'],

    // Mutation
    ['Horns', 'Mutation', '+1 ST when making a blitz'],
    ['Claw', 'Mutation', 'Armor breaks on 8+ regardless of AV'],
    ['Prehensile Tail', 'Mutation', '+1 to dodge rolls for opponents leaving tackle zone'],
    ['Two Heads', 'Mutation', '+1 to dodge rolls'],
    ['Big Hand', 'Mutation', 'Ignore tackle zones for pickup'],
    ['Extra Arms', 'Mutation', '+1 to catch and pickup rolls'],
    ['Tentacles', 'Mutation', 'Opponents must beat ST roll to leave tackle zone'],

    // Extraordinary
    ['Loner', 'Extraordinary', 'Team rerolls only work on 4+'],
    ['Regeneration', 'Extraordinary', 'Roll 4+ to avoid casualty (returned to reserves)'],
    ['Bone-head', 'Extraordinary', 'On 1, lose tackle zones and action'],
    ['Really Stupid', 'Extraordinary', 'Need adjacent ally or 4+ to act'],
    ['Wild Animal', 'Extraordinary', 'Need 4+ to act on non-block actions'],
    ['Throw Team-Mate', 'Extraordinary', 'Can throw a team-mate with Right Stuff'],
    ['Right Stuff', 'Extraordinary', 'Can be thrown by a team-mate'],
    ['Stunty', 'Extraordinary', '+1 dodge, -1 pass against, injured easier'],
    ['No Hands', 'Extraordinary', 'Cannot pick up, carry, or catch the ball'],
    ['Secret Weapon', 'Extraordinary', 'Sent off at the end of the drive'],
    ['Take Root', 'Extraordinary', 'On 1, cannot move for the rest of the drive'],
    ['Disturbing Presence', 'Mutation', '-1 to pass, catch, and intercept for nearby opponents'],
    ['Shadowing', 'General', 'Follow opponent who leaves tackle zone on failed roll'],
    ['Stab', 'Extraordinary', 'Use AG instead of ST for block, no block dice'],

    // Phase 19
    ['Bombardier', 'Extraordinary', 'Can throw a bomb instead of normal pass action'],
    ['Bloodlust', 'Extraordinary', 'Must roll 2+ before any action or bite a Thrall'],
    ['Hypnotic Gaze', 'Extraordinary', 'Target adjacent opponent loses tackle zones on 2+'],
    ['Ball & Chain', 'Extraordinary', 'Moves randomly, auto-blocks anyone contacted'],

    // Phase 20
    ['Decay', 'Extraordinary', 'Roll injury twice and take the worse result'],

    // Phase 21
    ['Chainsaw', 'Extraordinary', 'Auto armor roll instead of block dice, kickback on double 1'],
    ['Foul Appearance', 'Extraordinary', 'Opponent must roll 2+ before blocking this player'],
    ['Always Hungry', 'Extraordinary', 'On 1, eat thrown teammate instead of throwing'],
    ['Very Long Legs', 'Mutation', '+1 to Leap and interception rolls'],

    // Phase 22
    ['Animosity', 'Extraordinary', 'Roll D6 when passing/handing off to different race; on 1, action fails'],
    ['Pass Block', 'General', 'Move up to 3 squares toward receiver when opponent declares pass'],

    // Phase 23
    ["Nurgle's Rot", 'Extraordinary', 'Victim infected with Nurgle\'s Rot on casualty kill'],
    ['Titchy', 'Extraordinary', '+1 dodge (like Stunty), opponents easier to dodge away from'],
    ['Stakes', 'Extraordinary', 'Blocks Regeneration when causing a casualty'],

    // Phase 24
    ['Multiple Block', 'Strength', 'Block 2 adjacent opponents (each at +2 ST, no follow-up)'],
];

$skillStmt = $pdo->prepare('INSERT INTO skills (name, category, description) VALUES (:name, :category, :description) ON CONFLICT (name) DO NOTHING');

foreach ($skills as [$name, $category, $description]) {
    $skillStmt->execute([
        'name' => $name,
        'category' => $category,
        'description' => $description,
    ]);
}

echo "  Inserted " . count($skills) . " skills\n";

// Helper to get skill ID by name
$skillIds = [];
$stmt = $pdo->query('SELECT id, name FROM skills');
foreach ($stmt->fetchAll() as $row) {
    $skillIds[$row['name']] = (int) $row['id'];
}

// ========== RACES ==========
$races = [
    ['Human', 60000, true],
    ['Orc', 60000, true],
    ['Skaven', 60000, true],
    ['Dwarf', 50000, true],
    ['Wood Elf', 50000, true],
    ['Chaos', 70000, true],
    ['Undead', 70000, false],
    ['Lizardmen', 60000, true],
    ['Dark Elf', 50000, true],
    ['Halfling', 60000, true],
    ['Norse', 60000, true],
    ['High Elf', 50000, true],
    ['Vampire', 70000, true],
    ['Amazon', 50000, true],
    ['Necromantic', 70000, false],
    ['Bretonnian', 60000, true],
    ['Khemri', 70000, false],
    ['Goblin', 60000, true],
    ['Chaos Dwarf', 70000, true],
    ['Ogre', 70000, true],
    // Phase 22
    ['Nurgle', 70000, false],
    ['Pro Elf', 50000, true],
    ['Slann', 50000, true],
    ['Underworld', 70000, true],
    // Phase 23
    ['Khorne', 70000, true],
    ['Chaos Pact', 70000, true],
];

$raceStmt = $pdo->prepare('INSERT INTO races (name, reroll_cost, has_apothecary) VALUES (:name, :reroll_cost, :has_apothecary) ON CONFLICT (name) DO NOTHING');

foreach ($races as [$name, $rerollCost, $hasApothecary]) {
    $raceStmt->execute([
        'name' => $name,
        'reroll_cost' => $rerollCost,
        'has_apothecary' => $hasApothecary ? 't' : 'f',
    ]);
}

echo "  Inserted " . count($races) . " races\n";

// Get race IDs
$raceIds = [];
$stmt = $pdo->query('SELECT id, name FROM races');
foreach ($stmt->fetchAll() as $row) {
    $raceIds[$row['name']] = (int) $row['id'];
}

// ========== POSITIONAL TEMPLATES ==========
// Format: [race, name, max, cost, MA, ST, AG, AV, normal_access, double_access, [starting_skills]]
$positionals = [
    // Human
    [$raceIds['Human'], 'Lineman',  16, 50000,  6, 3, 3, 8, 'G', 'ASP',    []],
    [$raceIds['Human'], 'Catcher',   4, 70000,  8, 2, 3, 7, 'GA', 'SP',    ['Dodge', 'Catch']],
    [$raceIds['Human'], 'Thrower',   2, 70000,  6, 3, 3, 8, 'GP', 'AS',    ['Sure Hands']],
    [$raceIds['Human'], 'Blitzer',   4, 90000,  7, 3, 3, 8, 'GS', 'AP',    ['Block']],
    [$raceIds['Human'], 'Ogre',      1, 140000, 5, 5, 2, 9, 'S', 'GAP',    ['Loner', 'Bone-head', 'Mighty Blow', 'Thick Skull', 'Throw Team-Mate']],

    // Orc
    [$raceIds['Orc'], 'Lineman',    16, 50000,  5, 3, 3, 9, 'G', 'ASP',    []],
    [$raceIds['Orc'], 'Goblin',      4, 40000,  6, 2, 3, 7, 'A', 'GSP',    ['Dodge', 'Stunty', 'Right Stuff']],
    [$raceIds['Orc'], 'Thrower',     2, 70000,  5, 3, 3, 8, 'GP', 'AS',    ['Sure Hands']],
    [$raceIds['Orc'], 'Black Orc',   4, 80000,  4, 4, 2, 9, 'GS', 'AP',    []],
    [$raceIds['Orc'], 'Blitzer',     4, 80000,  6, 3, 3, 9, 'GS', 'AP',    ['Block']],
    [$raceIds['Orc'], 'Troll',       1, 110000, 4, 5, 1, 9, 'S', 'GAP',    ['Loner', 'Really Stupid', 'Regeneration', 'Mighty Blow', 'Throw Team-Mate']],

    // Skaven
    [$raceIds['Skaven'], 'Lineman',     16, 50000,  7, 3, 3, 7, 'G', 'ASPM',   []],
    [$raceIds['Skaven'], 'Thrower',      2, 70000,  7, 3, 3, 7, 'GP', 'ASM',   ['Sure Hands']],
    [$raceIds['Skaven'], 'Gutter Runner', 4, 80000, 9, 2, 4, 7, 'GA', 'SPM',   ['Dodge']],
    [$raceIds['Skaven'], 'Blitzer',      2, 90000,  7, 3, 3, 8, 'GS', 'APM',   ['Block']],
    [$raceIds['Skaven'], 'Storm Vermin', 2, 90000,  7, 3, 3, 8, 'GS', 'APM',   ['Block']],
    [$raceIds['Skaven'], 'Rat Ogre',     1, 150000, 6, 5, 1, 8, 'S', 'GAPM',   ['Loner', 'Wild Animal', 'Mighty Blow', 'Prehensile Tail', 'Frenzy']],

    // Dwarf
    [$raceIds['Dwarf'], 'Blocker',      16, 70000,  4, 3, 2, 9, 'GS', 'AP',    ['Block', 'Tackle', 'Thick Skull']],
    [$raceIds['Dwarf'], 'Runner',        2, 80000,  6, 3, 3, 8, 'GP', 'AS',    ['Sure Hands', 'Thick Skull']],
    [$raceIds['Dwarf'], 'Blitzer',       2, 80000,  5, 3, 3, 9, 'GS', 'AP',    ['Block', 'Thick Skull']],
    [$raceIds['Dwarf'], 'Troll Slayer',  2, 90000,  5, 3, 2, 8, 'GS', 'AP',    ['Frenzy', 'Dauntless', 'Thick Skull']],
    [$raceIds['Dwarf'], 'Deathroller',   1, 160000, 4, 7, 1, 10, 'S', 'GAP',   ['Loner', 'Break Tackle', 'Mighty Blow', 'Stand Firm', 'Secret Weapon']],

    // Wood Elf
    [$raceIds['Wood Elf'], 'Lineman',   16, 70000,  7, 3, 4, 7, 'GA', 'SP',    []],
    [$raceIds['Wood Elf'], 'Catcher',    4, 90000,  8, 2, 4, 7, 'GA', 'SP',    ['Dodge', 'Catch']],
    [$raceIds['Wood Elf'], 'Thrower',    2, 90000,  7, 3, 4, 7, 'GAP', 'S',    ['Pass']],
    [$raceIds['Wood Elf'], 'Wardancer',  2, 120000, 8, 3, 4, 7, 'GA', 'SP',    ['Block', 'Dodge']],
    [$raceIds['Wood Elf'], 'Treeman',    1, 120000, 2, 6, 1, 10, 'S', 'GAP',   ['Loner', 'Mighty Blow', 'Stand Firm', 'Thick Skull', 'Throw Team-Mate']],

    // Chaos
    [$raceIds['Chaos'], 'Beastman',       16, 60000,  6, 3, 3, 8, 'GSM', 'AP',   ['Horns']],
    [$raceIds['Chaos'], 'Chaos Warrior',    4, 100000, 5, 4, 3, 9, 'GSM', 'AP',   []],
    [$raceIds['Chaos'], 'Minotaur',         1, 150000, 5, 5, 2, 8, 'SM', 'GAP',   ['Loner', 'Wild Animal', 'Mighty Blow', 'Horns', 'Thick Skull', 'Frenzy']],

    // Undead
    [$raceIds['Undead'], 'Skeleton',       16, 40000,  5, 3, 2, 7, 'G', 'ASP',    ['Regeneration', 'Thick Skull']],
    [$raceIds['Undead'], 'Zombie',         16, 40000,  4, 3, 2, 8, 'G', 'ASP',    ['Regeneration']],
    [$raceIds['Undead'], 'Ghoul',           4, 70000,  7, 3, 3, 7, 'GA', 'SP',    ['Dodge']],
    [$raceIds['Undead'], 'Wight',           2, 90000,  6, 3, 3, 8, 'GS', 'AP',    ['Block', 'Regeneration']],
    [$raceIds['Undead'], 'Mummy',           2, 120000, 3, 5, 1, 9, 'S', 'GAP',    ['Mighty Blow', 'Regeneration']],

    // Lizardmen
    [$raceIds['Lizardmen'], 'Skink',      16, 60000,  8, 2, 3, 7, 'A', 'GSP',    ['Dodge', 'Stunty']],
    [$raceIds['Lizardmen'], 'Saurus',      6, 80000,  6, 4, 1, 9, 'GS', 'AP',    []],
    [$raceIds['Lizardmen'], 'Kroxigor',    1, 140000, 6, 5, 1, 9, 'S', 'GAP',    ['Loner', 'Bone-head', 'Mighty Blow', 'Prehensile Tail', 'Thick Skull']],

    // Dark Elf
    [$raceIds['Dark Elf'], 'Lineman',     16, 70000,  6, 3, 4, 8, 'GA', 'SP',    []],
    [$raceIds['Dark Elf'], 'Runner',       2, 80000,  7, 3, 4, 7, 'GAP', 'S',    ['Dump-Off']],
    [$raceIds['Dark Elf'], 'Assassin',     2, 90000,  6, 3, 4, 8, 'GA', 'SP',    ['Shadowing', 'Stab']],
    [$raceIds['Dark Elf'], 'Blitzer',      4, 100000, 7, 3, 4, 8, 'GA', 'SP',    ['Block']],
    [$raceIds['Dark Elf'], 'Witch Elf',    2, 110000, 7, 3, 4, 7, 'GA', 'SP',    ['Dodge', 'Frenzy', 'Jump Up']],

    // Halfling
    [$raceIds['Halfling'], 'Halfling',   16, 30000,  6, 2, 3, 6, 'A', 'GSP',    ['Dodge', 'Stunty', 'Right Stuff']],
    [$raceIds['Halfling'], 'Treeman',     2, 120000, 2, 6, 1, 10, 'S', 'GAP',   ['Loner', 'Mighty Blow', 'Stand Firm', 'Thick Skull', 'Take Root', 'Throw Team-Mate']],

    // Norse
    [$raceIds['Norse'], 'Lineman',       16, 50000,  6, 3, 3, 7, 'G', 'ASP',    ['Block']],
    [$raceIds['Norse'], 'Thrower',        2, 70000,  6, 3, 3, 7, 'GP', 'AS',    ['Block', 'Pass']],
    [$raceIds['Norse'], 'Runner',         2, 90000,  7, 3, 3, 7, 'GA', 'SP',    ['Block', 'Dauntless']],
    [$raceIds['Norse'], 'Berserker',      2, 90000,  6, 3, 3, 7, 'GS', 'AP',    ['Block', 'Frenzy', 'Jump Up']],
    [$raceIds['Norse'], 'Yhetee',         1, 140000, 5, 5, 1, 8, 'S', 'GAP',    ['Loner', 'Wild Animal', 'Claw', 'Frenzy', 'Disturbing Presence']],

    // High Elf
    [$raceIds['High Elf'], 'Lineman',    16, 70000,  6, 3, 4, 8, 'GA', 'SP',    []],
    [$raceIds['High Elf'], 'Catcher',     4, 90000,  8, 3, 4, 7, 'GA', 'SP',    ['Catch']],
    [$raceIds['High Elf'], 'Thrower',     2, 90000,  6, 3, 4, 8, 'GAP', 'S',    ['Pass', 'Safe Throw']],
    [$raceIds['High Elf'], 'Blitzer',     4, 100000, 7, 3, 4, 8, 'GA', 'SP',    ['Block']],

    // Vampire
    [$raceIds['Vampire'], 'Thrall',     16, 40000,  6, 3, 3, 7, 'G', 'ASP',    []],
    [$raceIds['Vampire'], 'Vampire',     4, 110000, 6, 4, 4, 8, 'GA', 'SP',    ['Hypnotic Gaze', 'Regeneration', 'Bloodlust']],

    // Amazon
    [$raceIds['Amazon'], 'Linewoman',  16, 50000,  6, 3, 3, 7, 'G', 'ASP',    ['Dodge']],
    [$raceIds['Amazon'], 'Thrower',     2, 70000,  6, 3, 3, 7, 'GP', 'AS',    ['Dodge', 'Pass']],
    [$raceIds['Amazon'], 'Catcher',     2, 70000,  6, 3, 3, 7, 'GA', 'SP',    ['Dodge', 'Catch']],
    [$raceIds['Amazon'], 'Blitzer',     4, 90000,  6, 3, 3, 7, 'GS', 'AP',    ['Block', 'Dodge']],

    // Necromantic
    [$raceIds['Necromantic'], 'Zombie',      16, 40000,  4, 3, 2, 8, 'G', 'ASP',    ['Regeneration']],
    [$raceIds['Necromantic'], 'Ghoul',        2, 70000,  7, 3, 3, 7, 'GA', 'SP',    ['Dodge']],
    [$raceIds['Necromantic'], 'Wight',        2, 90000,  6, 3, 3, 8, 'GS', 'AP',    ['Block', 'Regeneration']],
    [$raceIds['Necromantic'], 'Flesh Golem',  2, 110000, 4, 4, 2, 9, 'GS', 'AP',    ['Regeneration', 'Stand Firm']],
    [$raceIds['Necromantic'], 'Werewolf',     2, 120000, 8, 3, 3, 8, 'GA', 'SP',    ['Claw', 'Frenzy', 'Regeneration']],

    // Bretonnian
    [$raceIds['Bretonnian'], 'Peasant Lineman', 16, 40000,  6, 3, 3, 7, 'G', 'ASP',    ['Fend']],
    [$raceIds['Bretonnian'], 'Blocker',          4, 70000,  6, 3, 3, 8, 'GS', 'AP',    ['Wrestle', 'Fend']],
    [$raceIds['Bretonnian'], 'Blitzer',          4, 110000, 7, 3, 3, 8, 'GA', 'SP',    ['Block', 'Catch', 'Dauntless']],

    // Khemri
    [$raceIds['Khemri'], 'Skeleton',       16, 40000,  5, 3, 2, 7, 'G', 'ASP',    ['Regeneration', 'Thick Skull']],
    [$raceIds['Khemri'], 'Thro-Ra',         2, 70000,  6, 3, 2, 7, 'GP', 'AS',    ['Pass', 'Regeneration', 'Sure Hands']],
    [$raceIds['Khemri'], 'Blitz-Ra',        2, 90000,  6, 3, 2, 8, 'GS', 'AP',    ['Block', 'Regeneration']],
    [$raceIds['Khemri'], 'Tomb Guardian',   4, 100000, 4, 5, 1, 9, 'S', 'GAP',    ['Decay', 'Regeneration']],

    // Goblin
    [$raceIds['Goblin'], 'Goblin',      16, 40000,  6, 2, 3, 7, 'A', 'GSP',    ['Dodge', 'Right Stuff', 'Stunty']],
    [$raceIds['Goblin'], 'Bombardier',    2, 40000,  6, 2, 3, 7, 'A', 'GSP',    ['Bombardier', 'Dodge', 'Secret Weapon', 'Stunty']],
    [$raceIds['Goblin'], 'Looney',        1, 40000,  6, 2, 3, 7, 'A', 'GSP',    ['Chainsaw', 'Secret Weapon', 'Stunty']],
    [$raceIds['Goblin'], 'Fanatic',       1, 70000,  3, 7, 3, 7, 'S', 'GAP',    ['Ball & Chain', 'No Hands', 'Secret Weapon', 'Stunty']],
    [$raceIds['Goblin'], 'Pogoer',        1, 70000,  7, 2, 3, 7, 'A', 'GSP',    ['Dodge', 'Leap', 'Very Long Legs', 'Stunty']],
    [$raceIds['Goblin'], 'Troll',         2, 110000, 4, 5, 1, 9, 'S', 'GAP',    ['Loner', 'Mighty Blow', 'Really Stupid', 'Regeneration', 'Throw Team-Mate', 'Always Hungry']],

    // Chaos Dwarf
    [$raceIds['Chaos Dwarf'], 'Hobgoblin',    16, 40000,  6, 3, 3, 7, 'G', 'ASP',    []],
    [$raceIds['Chaos Dwarf'], 'Blocker',        6, 70000,  4, 3, 2, 9, 'GS', 'AP',    ['Block', 'Thick Skull', 'Tackle']],
    [$raceIds['Chaos Dwarf'], 'Bull Centaur',   2, 130000, 6, 4, 2, 9, 'GS', 'AP',    ['Sprint', 'Sure Feet', 'Thick Skull']],
    [$raceIds['Chaos Dwarf'], 'Minotaur',       1, 150000, 5, 5, 2, 8, 'SM', 'GAP',   ['Loner', 'Frenzy', 'Horns', 'Mighty Blow', 'Thick Skull', 'Wild Animal']],

    // Ogre
    [$raceIds['Ogre'], 'Snotling', 16, 20000,  5, 1, 3, 5, 'A', 'GSP',    ['Dodge', 'Right Stuff', 'Side Step', 'Stunty']],
    [$raceIds['Ogre'], 'Ogre',      6, 140000, 5, 5, 2, 9, 'S', 'GAP',    ['Loner', 'Bone-head', 'Mighty Blow', 'Thick Skull', 'Throw Team-Mate', 'Always Hungry']],

    // Nurgle
    [$raceIds['Nurgle'], 'Rotter',            16, 40000,  5, 3, 3, 8, 'G', 'ASPM',   ['Decay', "Nurgle's Rot"]],
    [$raceIds['Nurgle'], 'Pestigor',           4, 80000,  6, 3, 3, 8, 'GSM', 'AP',    ['Horns', 'Regeneration', 'Disturbing Presence', "Nurgle's Rot"]],
    [$raceIds['Nurgle'], 'Nurgle Warrior',     4, 110000, 4, 4, 2, 9, 'GSM', 'AP',    ['Disturbing Presence', 'Foul Appearance', 'Regeneration']],
    [$raceIds['Nurgle'], 'Beast of Nurgle',    1, 140000, 4, 5, 1, 9, 'S', 'GAP',     ['Loner', 'Disturbing Presence', 'Foul Appearance', 'Mighty Blow', 'Regeneration', 'Really Stupid', 'Tentacles']],

    // Pro Elf
    [$raceIds['Pro Elf'], 'Lineman',  16, 60000,  6, 3, 4, 7, 'GA', 'SP',    []],
    [$raceIds['Pro Elf'], 'Catcher',   4, 100000, 8, 3, 4, 7, 'GA', 'SP',    ['Catch', 'Nerves of Steel']],
    [$raceIds['Pro Elf'], 'Thrower',   2, 70000,  6, 3, 4, 7, 'GAP', 'S',    ['Pass', 'Safe Throw']],
    [$raceIds['Pro Elf'], 'Blitzer',   2, 110000, 7, 3, 4, 8, 'GA', 'SP',    ['Block', 'Side Step']],

    // Slann
    [$raceIds['Slann'], 'Lineman',   16, 60000,  6, 3, 3, 8, 'G', 'ASP',    ['Leap', 'Very Long Legs']],
    [$raceIds['Slann'], 'Catcher',    4, 80000,  7, 3, 4, 7, 'GA', 'SP',    ['Leap', 'Very Long Legs', 'Diving Catch']],
    [$raceIds['Slann'], 'Blitzer',    4, 110000, 7, 3, 3, 8, 'GS', 'AP',    ['Leap', 'Very Long Legs', 'Diving Tackle']],
    [$raceIds['Slann'], 'Kroxigor',   1, 140000, 6, 5, 1, 9, 'S', 'GAP',    ['Loner', 'Bone-head', 'Mighty Blow', 'Prehensile Tail', 'Thick Skull']],

    // Underworld
    [$raceIds['Underworld'], 'Underworld Goblin', 12, 40000,  6, 2, 3, 7, 'AM', 'GSP',   ['Dodge', 'Right Stuff', 'Stunty', 'Animosity']],
    [$raceIds['Underworld'], 'Skaven Lineman',    12, 50000,  7, 3, 3, 7, 'GM', 'ASP',    ['Animosity']],
    [$raceIds['Underworld'], 'Skaven Thrower',     2, 70000,  7, 3, 3, 7, 'GMP', 'AS',    ['Animosity', 'Pass', 'Sure Hands']],
    [$raceIds['Underworld'], 'Skaven Blitzer',     2, 90000,  7, 3, 3, 8, 'GSM', 'AP',    ['Animosity', 'Block']],
    [$raceIds['Underworld'], 'Troll',              1, 110000, 4, 5, 1, 9, 'S', 'GAP',     ['Loner', 'Mighty Blow', 'Really Stupid', 'Regeneration', 'Throw Team-Mate', 'Always Hungry']],
    [$raceIds['Underworld'], 'Warpstone Troll',    1, 110000, 4, 5, 1, 9, 'SM', 'GAP',    ['Loner', 'Mighty Blow', 'Really Stupid', 'Regeneration', 'Secret Weapon']],

    // Khorne
    [$raceIds['Khorne'], 'Pit Fighter',     16, 60000,  6, 3, 3, 8, 'GS', 'AP',    ['Frenzy']],
    [$raceIds['Khorne'], 'Bloodletter',      4, 70000,  6, 3, 3, 8, 'GS', 'AP',    ['Horns', 'Juggernaut', 'Regeneration']],
    [$raceIds['Khorne'], 'Khorne Herald',    2, 90000,  6, 3, 3, 8, 'GS', 'AP',    ['Frenzy', 'Horns', 'Juggernaut']],
    [$raceIds['Khorne'], 'Bloodthirster',    1, 180000, 6, 5, 1, 9, 'S', 'GAP',    ['Loner', 'Wild Animal', 'Claw', 'Frenzy', 'Horns', 'Juggernaut', 'Regeneration']],

    // Chaos Pact
    [$raceIds['Chaos Pact'], 'Marauder',            12, 50000,  6, 3, 3, 8, 'GS', 'APM',    []],
    [$raceIds['Chaos Pact'], 'Dark Elf Renegade',    1, 70000,  6, 3, 4, 8, 'GA', 'SPM',    ['Animosity']],
    [$raceIds['Chaos Pact'], 'Goblin Renegade',      1, 40000,  6, 2, 3, 7, 'A', 'GSPM',    ['Animosity', 'Dodge', 'Right Stuff', 'Stunty']],
    [$raceIds['Chaos Pact'], 'Skaven Renegade',      1, 50000,  7, 3, 3, 7, 'GM', 'ASP',    ['Animosity']],
    [$raceIds['Chaos Pact'], 'Troll',                1, 110000, 4, 5, 1, 9, 'S', 'GAP',     ['Loner', 'Always Hungry', 'Mighty Blow', 'Really Stupid', 'Regeneration', 'Throw Team-Mate']],
    [$raceIds['Chaos Pact'], 'Ogre',                 1, 140000, 5, 5, 2, 9, 'S', 'GAP',     ['Loner', 'Bone-head', 'Mighty Blow', 'Thick Skull', 'Throw Team-Mate']],
    [$raceIds['Chaos Pact'], 'Minotaur',             1, 150000, 5, 5, 2, 8, 'S', 'GAP',     ['Loner', 'Frenzy', 'Horns', 'Mighty Blow', 'Thick Skull', 'Wild Animal']],
];

$posStmt = $pdo->prepare(
    'INSERT INTO positional_templates (race_id, name, max_count, cost, ma, st, ag, av, normal_access, double_access)
     VALUES (:race_id, :name, :max_count, :cost, :ma, :st, :ag, :av, :normal_access, :double_access)
     ON CONFLICT (race_id, name) DO NOTHING
     RETURNING id'
);

$posSkillStmt = $pdo->prepare(
    'INSERT INTO positional_template_skills (positional_template_id, skill_id)
     VALUES (:template_id, :skill_id)
     ON CONFLICT (positional_template_id, skill_id) DO NOTHING'
);

$posCount = 0;
foreach ($positionals as [$raceId, $name, $max, $cost, $ma, $st, $ag, $av, $normal, $double, $startSkills]) {
    $posStmt->execute([
        'race_id' => $raceId,
        'name' => $name,
        'max_count' => $max,
        'cost' => $cost,
        'ma' => $ma,
        'st' => $st,
        'ag' => $ag,
        'av' => $av,
        'normal_access' => $normal,
        'double_access' => $double,
    ]);

    $templateRow = $posStmt->fetch();
    if ($templateRow !== false) {
        $templateId = (int) $templateRow['id'];
        $posCount++;

        foreach ($startSkills as $skillName) {
            if (isset($skillIds[$skillName])) {
                $posSkillStmt->execute([
                    'template_id' => $templateId,
                    'skill_id' => $skillIds[$skillName],
                ]);
            } else {
                echo "  WARNING: Skill '{$skillName}' not found\n";
            }
        }
    }
}

echo "  Inserted {$posCount} positional templates\n";
echo "Done!\n";
