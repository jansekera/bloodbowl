#pragma once

#include "bb/player_stats.h"
#include "bb/player.h"
#include <cstdint>
#include <string>

namespace bb {

struct PlayerTemplate {
    PlayerStats stats;
    SkillSet skills;
    int8_t quantity;  // max count on roster
};

struct TeamRoster {
    const char* name;
    PlayerTemplate positionals[8];  // max 8 positional types
    int positionalCount;
    int rerollCost;  // in thousands
    bool hasApothecary;
};

const TeamRoster& getHumanRoster();
const TeamRoster& getOrcRoster();
const TeamRoster& getSkavenRoster();
const TeamRoster& getDwarfRoster();
const TeamRoster& getWoodElfRoster();
const TeamRoster& getChaosRoster();
const TeamRoster& getUndeadRoster();
const TeamRoster& getLizardmenRoster();
const TeamRoster& getDarkElfRoster();
const TeamRoster& getHalflingRoster();
const TeamRoster& getNorseRoster();
const TeamRoster& getHighElfRoster();
const TeamRoster& getVampireRoster();
const TeamRoster& getAmazonRoster();
const TeamRoster& getNecromanticRoster();
const TeamRoster& getBretonianRoster();
const TeamRoster& getKhemriRoster();
const TeamRoster& getGoblinRoster();
const TeamRoster& getChaosDwarfRoster();
const TeamRoster& getOgreRoster();
const TeamRoster& getNurgleRoster();
const TeamRoster& getProElfRoster();
const TeamRoster& getSlannRoster();
const TeamRoster& getUnderworldRoster();
const TeamRoster& getKhorneRoster();
const TeamRoster& getChaosPactRoster();

// Lookup roster by name (case-insensitive)
const TeamRoster* getRosterByName(const std::string& name);

// Lookup developed (skilled) roster for a given team value.
// tv >= 1200 returns a TV~1200 developed variant for supported races
// (orc, human, dwarf, skaven); otherwise falls back to the base roster.
const TeamRoster* getDevelopedRoster(const std::string& name, int tv);

// Classify roster speed based on average MA of an 11-player team
RosterSpeed classifyRosterSpeed(const TeamRoster& roster);

} // namespace bb
