#include "bb/roster.h"
#include <algorithm>
#include <cctype>

namespace bb {

namespace {

SkillSet makeSkills(std::initializer_list<SkillName> list) {
    SkillSet ss;
    for (auto s : list) ss.add(s);
    return ss;
}

std::string toLower(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    return out;
}

} // anonymous namespace

// Human: Lineman 0-16, Catcher 0-4, Thrower 0-2, Blitzer 0-4, Ogre 0-1
const TeamRoster& getHumanRoster() {
    static const TeamRoster roster = {
        "Human",
        {
            {{6, 3, 3, 8}, {}, 16},
            {{8, 2, 3, 7}, makeSkills({SkillName::Catch, SkillName::Dodge}), 4},
            {{6, 3, 3, 8}, makeSkills({SkillName::SureHands, SkillName::Pass}), 2},
            {{7, 3, 3, 8}, makeSkills({SkillName::Block}), 4},
            {{5, 5, 2, 9}, makeSkills({SkillName::Loner, SkillName::BoneHead,
                SkillName::MightyBlow, SkillName::ThickSkull, SkillName::ThrowTeamMate}), 1},
        },
        5, 50, true
    };
    return roster;
}

// Orc: Lineman 0-16, Goblin 0-4, Thrower 0-2, Black Orc 0-4, Blitzer 0-4, Troll 0-1
const TeamRoster& getOrcRoster() {
    static const TeamRoster roster = {
        "Orc",
        {
            {{5, 3, 3, 9}, {}, 16},
            {{6, 2, 3, 7}, makeSkills({SkillName::Dodge, SkillName::RightStuff, SkillName::Stunty}), 4},
            {{5, 3, 3, 8}, makeSkills({SkillName::SureHands, SkillName::Pass}), 2},
            {{4, 4, 2, 9}, {}, 4},
            {{6, 3, 3, 9}, makeSkills({SkillName::Block}), 4},
            {{4, 5, 1, 9}, makeSkills({SkillName::Loner, SkillName::AlwaysHungry,
                SkillName::MightyBlow, SkillName::ReallyStupid, SkillName::Regeneration,
                SkillName::ThrowTeamMate}), 1},
        },
        6, 60, true
    };
    return roster;
}

// Skaven: Lineman 0-16, Thrower 0-2, Gutter Runner 0-4, Blitzer 0-2, Rat Ogre 0-1
const TeamRoster& getSkavenRoster() {
    static const TeamRoster roster = {
        "Skaven",
        {
            {{7, 3, 3, 7}, {}, 16},
            {{7, 3, 3, 7}, makeSkills({SkillName::SureHands, SkillName::Pass}), 2},
            {{9, 2, 4, 7}, makeSkills({SkillName::Dodge}), 4},
            {{7, 3, 3, 8}, makeSkills({SkillName::Block}), 2},
            {{6, 5, 2, 8}, makeSkills({SkillName::Loner, SkillName::Frenzy,
                SkillName::MightyBlow, SkillName::WildAnimal, SkillName::PrehensileTail}), 1},
        },
        5, 60, true
    };
    return roster;
}

// Dwarf: Blocker 0-16, Runner 0-2, Blitzer 0-2, Troll Slayer 0-2, Deathroller 0-1
const TeamRoster& getDwarfRoster() {
    static const TeamRoster roster = {
        "Dwarf",
        {
            {{4, 3, 2, 9}, makeSkills({SkillName::Block, SkillName::Tackle, SkillName::ThickSkull}), 16},
            {{6, 3, 3, 8}, makeSkills({SkillName::SureHands, SkillName::ThickSkull}), 2},
            {{5, 3, 3, 9}, makeSkills({SkillName::Block, SkillName::ThickSkull}), 2},
            {{5, 3, 2, 8}, makeSkills({SkillName::Block, SkillName::Frenzy,
                SkillName::ThickSkull, SkillName::Dauntless}), 2},
            {{4, 7, 1, 10}, makeSkills({SkillName::Loner, SkillName::BreakTackle,
                SkillName::DirtyPlayer, SkillName::Juggernaut, SkillName::MightyBlow,
                SkillName::NoHands, SkillName::SecretWeapon, SkillName::StandFirm}), 1},
        },
        5, 40, true
    };
    return roster;
}

// Wood Elf: Lineman 0-16, Catcher 0-4, Thrower 0-2, Wardancer 0-2, Treeman 0-1
const TeamRoster& getWoodElfRoster() {
    static const TeamRoster roster = {
        "Wood Elf",
        {
            {{7, 3, 4, 7}, {}, 16},
            {{8, 2, 4, 7}, makeSkills({SkillName::Catch, SkillName::Dodge, SkillName::Sprint}), 4},
            {{7, 3, 4, 7}, makeSkills({SkillName::Pass}), 2},
            {{8, 3, 4, 7}, makeSkills({SkillName::Block, SkillName::Dodge, SkillName::Leap}), 2},
            {{2, 6, 1, 10}, makeSkills({SkillName::Loner, SkillName::TakeRoot,
                SkillName::StandFirm, SkillName::MightyBlow, SkillName::ThickSkull}), 1},
        },
        5, 50, true
    };
    return roster;
}

// Chaos: Beastman 0-16, Chaos Warrior 0-4, Minotaur 0-1
const TeamRoster& getChaosRoster() {
    static const TeamRoster roster = {
        "Chaos",
        {
            {{6, 3, 3, 8}, makeSkills({SkillName::Horns}), 16},
            {{5, 4, 3, 9}, {}, 4},
            {{5, 5, 2, 8}, makeSkills({SkillName::Loner, SkillName::Horns,
                SkillName::Frenzy, SkillName::WildAnimal, SkillName::MightyBlow}), 1},
        },
        3, 70, true
    };
    return roster;
}

// Undead: Skeleton 0-16, Zombie 0-16, Ghoul 0-4, Wight 0-2, Mummy 0-2
const TeamRoster& getUndeadRoster() {
    static const TeamRoster roster = {
        "Undead",
        {
            {{5, 3, 2, 7}, makeSkills({SkillName::Regeneration, SkillName::ThickSkull}), 16},
            {{4, 3, 2, 8}, makeSkills({SkillName::Regeneration}), 16},
            {{7, 3, 3, 7}, makeSkills({SkillName::Dodge}), 4},
            {{6, 3, 3, 8}, makeSkills({SkillName::Block, SkillName::Regeneration}), 2},
            {{3, 5, 1, 9}, makeSkills({SkillName::MightyBlow, SkillName::Regeneration}), 2},
        },
        5, 70, false
    };
    return roster;
}

// Lizardmen: Skink 0-16, Saurus 0-6, Kroxigor 0-1
const TeamRoster& getLizardmenRoster() {
    static const TeamRoster roster = {
        "Lizardmen",
        {
            {{8, 2, 3, 7}, makeSkills({SkillName::Dodge, SkillName::Stunty}), 16},
            {{6, 4, 1, 9}, {}, 6},
            {{6, 5, 1, 9}, makeSkills({SkillName::Loner, SkillName::BoneHead,
                SkillName::MightyBlow, SkillName::PrehensileTail, SkillName::ThickSkull}), 1},
        },
        3, 60, true
    };
    return roster;
}

// Dark Elf: Lineman 0-16, Runner 0-2, Assassin 0-2, Blitzer 0-4, Witch Elf 0-2
const TeamRoster& getDarkElfRoster() {
    static const TeamRoster roster = {
        "Dark Elf",
        {
            {{6, 3, 4, 8}, {}, 16},
            {{7, 3, 4, 7}, makeSkills({SkillName::DumpOff}), 2},
            {{6, 3, 4, 7}, makeSkills({SkillName::Stab, SkillName::Shadowing}), 2},
            {{7, 3, 4, 8}, makeSkills({SkillName::Block}), 4},
            {{7, 3, 4, 7}, makeSkills({SkillName::Dodge, SkillName::Frenzy, SkillName::JumpUp}), 2},
        },
        5, 50, true
    };
    return roster;
}

// Halfling: Halfling 0-16, Treeman 0-2
const TeamRoster& getHalflingRoster() {
    static const TeamRoster roster = {
        "Halfling",
        {
            {{5, 2, 3, 6}, makeSkills({SkillName::Dodge, SkillName::RightStuff, SkillName::Stunty}), 16},
            {{2, 6, 1, 10}, makeSkills({SkillName::Loner, SkillName::TakeRoot,
                SkillName::StandFirm, SkillName::MightyBlow, SkillName::ThickSkull,
                SkillName::ThrowTeamMate}), 2},
        },
        2, 60, true
    };
    return roster;
}

// Norse: Lineman 0-16, Thrower 0-2, Runner 0-2, Berserker 0-2, Ulfwerener 0-2, Yhetee 0-1
const TeamRoster& getNorseRoster() {
    static const TeamRoster roster = {
        "Norse",
        {
            {{6, 3, 3, 7}, makeSkills({SkillName::Block}), 16},
            {{6, 3, 3, 7}, makeSkills({SkillName::Block, SkillName::Pass}), 2},
            {{7, 3, 3, 7}, makeSkills({SkillName::Block, SkillName::Dauntless}), 2},
            {{6, 3, 3, 7}, makeSkills({SkillName::Block, SkillName::Frenzy, SkillName::JumpUp}), 2},
            {{6, 4, 2, 8}, makeSkills({SkillName::Frenzy}), 2},
            {{5, 5, 1, 8}, makeSkills({SkillName::Loner, SkillName::WildAnimal,
                SkillName::Frenzy, SkillName::DisturbingPresence, SkillName::Claw}), 1},
        },
        6, 60, true
    };
    return roster;
}

// High Elf: Lineman 0-16, Catcher 0-4, Thrower 0-2, Blitzer 0-4
const TeamRoster& getHighElfRoster() {
    static const TeamRoster roster = {
        "High Elf",
        {
            {{6, 3, 4, 8}, {}, 16},
            {{8, 3, 4, 7}, makeSkills({SkillName::Catch}), 4},
            {{6, 3, 4, 8}, makeSkills({SkillName::Pass, SkillName::SureHands}), 2},
            {{7, 3, 4, 8}, makeSkills({SkillName::Block}), 4},
        },
        4, 50, true
    };
    return roster;
}

// Vampire: Thrall 0-16, Vampire 0-4
const TeamRoster& getVampireRoster() {
    static const TeamRoster roster = {
        "Vampire",
        {
            {{6, 3, 3, 7}, {}, 16},
            {{6, 4, 4, 8}, makeSkills({SkillName::HypnoticGaze, SkillName::Regeneration,
                SkillName::Bloodlust}), 4},
        },
        2, 70, true
    };
    return roster;
}

// Amazon: Linewoman 0-16, Catcher 0-2, Thrower 0-2, Blitzer 0-4
const TeamRoster& getAmazonRoster() {
    static const TeamRoster roster = {
        "Amazon",
        {
            {{6, 3, 3, 7}, makeSkills({SkillName::Dodge}), 16},
            {{6, 3, 3, 7}, makeSkills({SkillName::Dodge, SkillName::Catch}), 2},
            {{6, 3, 3, 7}, makeSkills({SkillName::Dodge, SkillName::Pass}), 2},
            {{6, 3, 3, 7}, makeSkills({SkillName::Dodge, SkillName::Block}), 4},
        },
        4, 50, true
    };
    return roster;
}

// Necromantic: Zombie 0-16, Ghoul 0-2, Wight 0-2, Flesh Golem 0-2, Werewolf 0-2
const TeamRoster& getNecromanticRoster() {
    static const TeamRoster roster = {
        "Necromantic",
        {
            {{4, 3, 2, 8}, makeSkills({SkillName::Regeneration}), 16},
            {{7, 3, 3, 7}, makeSkills({SkillName::Dodge}), 2},
            {{6, 3, 3, 8}, makeSkills({SkillName::Block, SkillName::Regeneration}), 2},
            {{4, 4, 2, 9}, makeSkills({SkillName::StandFirm, SkillName::Regeneration,
                SkillName::Decay}), 2},
            {{8, 3, 3, 8}, makeSkills({SkillName::Claw, SkillName::Frenzy,
                SkillName::Regeneration}), 2},
        },
        5, 70, false
    };
    return roster;
}

// Bretonnian: Lineman 0-16, Blocker 0-4, Blitzer 0-4
const TeamRoster& getBretonianRoster() {
    static const TeamRoster roster = {
        "Bretonnian",
        {
            {{6, 3, 3, 7}, {}, 16},
            {{6, 3, 3, 8}, makeSkills({SkillName::Wrestle}), 4},
            {{7, 3, 3, 8}, makeSkills({SkillName::Block, SkillName::Fend, SkillName::Catch}), 4},
        },
        3, 60, true
    };
    return roster;
}

// Khemri: Skeleton 0-16, Thro-Ra 0-2, Blitz-Ra 0-2, Tomb Guardian 0-4
const TeamRoster& getKhemriRoster() {
    static const TeamRoster roster = {
        "Khemri",
        {
            {{5, 3, 2, 7}, makeSkills({SkillName::Regeneration, SkillName::ThickSkull}), 16},
            {{6, 3, 2, 7}, makeSkills({SkillName::Pass, SkillName::Regeneration, SkillName::SureHands}), 2},
            {{6, 3, 2, 8}, makeSkills({SkillName::Block, SkillName::Regeneration}), 2},
            {{3, 5, 1, 9}, makeSkills({SkillName::Decay, SkillName::Regeneration}), 4},
        },
        4, 70, false
    };
    return roster;
}

// Goblin: Goblin 0-16, Bombardier 0-1, Looney 0-1, Fanatic 0-1, Pogoer 0-1, Troll 0-2
const TeamRoster& getGoblinRoster() {
    static const TeamRoster roster = {
        "Goblin",
        {
            {{6, 2, 3, 7}, makeSkills({SkillName::Dodge, SkillName::RightStuff, SkillName::Stunty}), 16},
            {{6, 2, 3, 7}, makeSkills({SkillName::Bombardier, SkillName::Dodge, SkillName::SecretWeapon,
                SkillName::Stunty}), 1},
            {{6, 2, 3, 7}, makeSkills({SkillName::Chainsaw, SkillName::SecretWeapon, SkillName::Stunty}), 1},
            {{3, 7, 3, 7}, makeSkills({SkillName::BallAndChain, SkillName::NoHands,
                SkillName::SecretWeapon, SkillName::Stunty}), 1},
            {{7, 2, 3, 7}, makeSkills({SkillName::Dodge, SkillName::Leap,
                SkillName::VeryLongLegs, SkillName::Stunty}), 1},
            {{4, 5, 1, 9}, makeSkills({SkillName::Loner, SkillName::AlwaysHungry,
                SkillName::MightyBlow, SkillName::ReallyStupid, SkillName::Regeneration,
                SkillName::ThrowTeamMate}), 2},
        },
        6, 60, true
    };
    return roster;
}

// Chaos Dwarf: Hobgoblin 0-16, Blocker 0-6, Bull Centaur 0-2, Minotaur 0-1
const TeamRoster& getChaosDwarfRoster() {
    static const TeamRoster roster = {
        "Chaos Dwarf",
        {
            {{6, 3, 3, 7}, {}, 16},
            {{4, 3, 2, 9}, makeSkills({SkillName::Block, SkillName::Tackle, SkillName::ThickSkull}), 6},
            {{6, 4, 2, 9}, makeSkills({SkillName::Sprint, SkillName::SureFeet, SkillName::ThickSkull}), 2},
            {{5, 5, 2, 8}, makeSkills({SkillName::Loner, SkillName::Horns,
                SkillName::Frenzy, SkillName::WildAnimal, SkillName::MightyBlow}), 1},
        },
        4, 70, true
    };
    return roster;
}

// Ogre: Snotling 0-16, Ogre 0-6
const TeamRoster& getOgreRoster() {
    static const TeamRoster roster = {
        "Ogre",
        {
            {{5, 1, 3, 5}, makeSkills({SkillName::Dodge, SkillName::RightStuff,
                SkillName::Stunty, SkillName::Titchy}), 16},
            {{5, 5, 2, 9}, makeSkills({SkillName::Loner, SkillName::BoneHead,
                SkillName::MightyBlow, SkillName::ThickSkull, SkillName::ThrowTeamMate}), 6},
        },
        2, 70, true
    };
    return roster;
}

// Nurgle: Rotter 0-16, Pestigor 0-4, Nurgle Warrior 0-4, Beast of Nurgle 0-1
const TeamRoster& getNurgleRoster() {
    static const TeamRoster roster = {
        "Nurgle",
        {
            {{5, 3, 3, 8}, makeSkills({SkillName::Decay, SkillName::NurglesRot}), 16},
            {{6, 3, 3, 8}, makeSkills({SkillName::Horns, SkillName::Regeneration,
                SkillName::NurglesRot}), 4},
            {{4, 4, 2, 9}, makeSkills({SkillName::FoulAppearance, SkillName::Regeneration,
                SkillName::DisturbingPresence}), 4},
            {{4, 5, 1, 9}, makeSkills({SkillName::Loner, SkillName::FoulAppearance,
                SkillName::MightyBlow, SkillName::NurglesRot, SkillName::Regeneration,
                SkillName::Tentacles, SkillName::DisturbingPresence}), 1},
        },
        4, 70, false
    };
    return roster;
}

// Pro Elf: Lineman 0-16, Catcher 0-4, Thrower 0-2, Blitzer 0-2
const TeamRoster& getProElfRoster() {
    static const TeamRoster roster = {
        "Pro Elf",
        {
            {{6, 3, 4, 7}, {}, 16},
            {{8, 3, 4, 7}, makeSkills({SkillName::NervesOfSteel, SkillName::Catch}), 4},
            {{6, 3, 4, 7}, makeSkills({SkillName::Pass}), 2},
            {{7, 3, 4, 8}, makeSkills({SkillName::Block, SkillName::SideStep}), 2},
        },
        4, 50, true
    };
    return roster;
}

// Slann: Lineman 0-16, Catcher 0-4, Blitzer 0-4, Kroxigor 0-1
const TeamRoster& getSlannRoster() {
    static const TeamRoster roster = {
        "Slann",
        {
            {{6, 3, 3, 8}, makeSkills({SkillName::Leap, SkillName::VeryLongLegs}), 16},
            {{7, 3, 4, 7}, makeSkills({SkillName::Leap, SkillName::VeryLongLegs,
                SkillName::DivingCatch}), 4},
            {{7, 3, 3, 8}, makeSkills({SkillName::Leap, SkillName::VeryLongLegs,
                SkillName::JumpUp}), 4},
            {{6, 5, 1, 9}, makeSkills({SkillName::Loner, SkillName::BoneHead,
                SkillName::MightyBlow, SkillName::PrehensileTail, SkillName::ThickSkull}), 1},
        },
        4, 50, true
    };
    return roster;
}

// Underworld: UW Goblin 0-12, Skaven Lineman 0-12, Skaven Thrower 0-2, Skaven Blitzer 0-2, Troll 0-1, Warpstone Troll 0-1
const TeamRoster& getUnderworldRoster() {
    static const TeamRoster roster = {
        "Underworld",
        {
            {{6, 2, 3, 7}, makeSkills({SkillName::Animosity, SkillName::Dodge,
                SkillName::RightStuff, SkillName::Stunty}), 12},
            {{7, 3, 3, 7}, makeSkills({SkillName::Animosity}), 12},
            {{7, 3, 3, 7}, makeSkills({SkillName::Animosity, SkillName::Pass,
                SkillName::SureHands}), 2},
            {{7, 3, 3, 8}, makeSkills({SkillName::Animosity, SkillName::Block}), 2},
            {{4, 5, 1, 9}, makeSkills({SkillName::Loner, SkillName::AlwaysHungry,
                SkillName::MightyBlow, SkillName::ReallyStupid, SkillName::Regeneration,
                SkillName::ThrowTeamMate}), 1},
            {{4, 5, 1, 9}, makeSkills({SkillName::Loner, SkillName::AlwaysHungry,
                SkillName::MightyBlow, SkillName::ReallyStupid, SkillName::Regeneration,
                SkillName::ThrowTeamMate, SkillName::Tentacles}), 1},
        },
        6, 70, true
    };
    return roster;
}

// Khorne: Pit Fighter 0-16, Bloodletter 0-4, Khorne Herald 0-2, Bloodthirster 0-1
const TeamRoster& getKhorneRoster() {
    static const TeamRoster roster = {
        "Khorne",
        {
            {{6, 3, 3, 8}, makeSkills({SkillName::Frenzy}), 16},
            {{6, 3, 3, 8}, makeSkills({SkillName::Horns, SkillName::Regeneration,
                SkillName::Juggernaut}), 4},
            {{6, 3, 3, 8}, makeSkills({SkillName::Frenzy, SkillName::Juggernaut,
                SkillName::Horns}), 2},
            {{6, 5, 1, 9}, makeSkills({SkillName::Loner, SkillName::Frenzy,
                SkillName::Horns, SkillName::MightyBlow, SkillName::ThickSkull,
                SkillName::WildAnimal, SkillName::Regeneration, SkillName::Claw}), 1},
        },
        4, 70, true
    };
    return roster;
}

// Chaos Pact: Marauder 0-12, DE Renegade 0-1, Goblin Renegade 0-1, Skaven Renegade 0-1, Troll 0-1, Ogre 0-1, Minotaur 0-1
const TeamRoster& getChaosPactRoster() {
    static const TeamRoster roster = {
        "Chaos Pact",
        {
            {{6, 3, 3, 8}, {}, 12},
            {{6, 3, 4, 8}, makeSkills({SkillName::Animosity}), 1},
            {{6, 2, 3, 7}, makeSkills({SkillName::Animosity, SkillName::Stunty,
                SkillName::RightStuff}), 1},
            {{7, 3, 3, 7}, makeSkills({SkillName::Animosity}), 1},
            {{4, 5, 1, 9}, makeSkills({SkillName::Loner, SkillName::Animosity,
                SkillName::AlwaysHungry, SkillName::MightyBlow, SkillName::ReallyStupid,
                SkillName::Regeneration, SkillName::ThrowTeamMate}), 1},
            {{5, 5, 2, 9}, makeSkills({SkillName::Loner, SkillName::Animosity,
                SkillName::BoneHead, SkillName::MightyBlow, SkillName::ThickSkull,
                SkillName::ThrowTeamMate}), 1},
            {{5, 5, 2, 8}, makeSkills({SkillName::Loner, SkillName::Animosity,
                SkillName::Horns, SkillName::Frenzy, SkillName::WildAnimal,
                SkillName::MightyBlow}), 1},
        },
        7, 70, true
    };
    return roster;
}

const TeamRoster* getRosterByName(const std::string& name) {
    std::string lower = toLower(name);

    // Remove spaces/underscores for flexible matching
    std::string normalized;
    for (char c : lower) {
        if (c != ' ' && c != '_' && c != '-') normalized += c;
    }

    if (normalized == "human") return &getHumanRoster();
    if (normalized == "orc") return &getOrcRoster();
    if (normalized == "skaven") return &getSkavenRoster();
    if (normalized == "dwarf") return &getDwarfRoster();
    if (normalized == "woodelf") return &getWoodElfRoster();
    if (normalized == "chaos") return &getChaosRoster();
    if (normalized == "undead") return &getUndeadRoster();
    if (normalized == "lizardmen") return &getLizardmenRoster();
    if (normalized == "darkelf") return &getDarkElfRoster();
    if (normalized == "halfling") return &getHalflingRoster();
    if (normalized == "norse") return &getNorseRoster();
    if (normalized == "highelf") return &getHighElfRoster();
    if (normalized == "vampire") return &getVampireRoster();
    if (normalized == "amazon") return &getAmazonRoster();
    if (normalized == "necromantic") return &getNecromanticRoster();
    if (normalized == "bretonnian") return &getBretonianRoster();
    if (normalized == "khemri") return &getKhemriRoster();
    if (normalized == "goblin") return &getGoblinRoster();
    if (normalized == "chaosdwarf") return &getChaosDwarfRoster();
    if (normalized == "ogre") return &getOgreRoster();
    if (normalized == "nurgle") return &getNurgleRoster();
    if (normalized == "proelf") return &getProElfRoster();
    if (normalized == "slann") return &getSlannRoster();
    if (normalized == "underworld") return &getUnderworldRoster();
    if (normalized == "khorne") return &getKhorneRoster();
    if (normalized == "chaospact") return &getChaosPactRoster();

    return nullptr;
}

} // namespace bb
