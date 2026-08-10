// Balík G, krok (a2): KOLIK BLOKŮ VLASTNĚ HÁZÍME a kam se attrition ztrácí.
//
// Otázka uživatele (10.08.): "skaven injury 1 je pořád málo". Aritmetika
// říká, že na AV7 je jedno sražení = 6,9 % casualty + 10,4 % KO, tedy na
// jednu casualty za zápas je potřeba skavena srazit ~14x. Naměřených
// ~1/zápas tedy znamená BUD malo bloku, NEBO merke rany -- a to se bez
// dat nerozhodne.
//
// Tenhle harness nesaha na produkci: cte jen eventy ze simulateGameLogged
// a scita trychtyr  blok -> srazeni -> probita zbroj -> odchod ze hry,
// zvlast podle toho, KDO byl obeti.
//
// Build:
//   g++ -O2 -std=c++20 -Iengine/include -Iengine/third_party \
//       diag_block_funnel_20260810.cpp -Lengine/build -lbb_engine \
//       -Wl,-rpath,$PWD/engine/build -o diag_block_funnel
// Spusteni:  ./diag_block_funnel <repoRoot> [nGames=20] [matchup=0]
#include "bb/game_simulator.h"
#include "bb/roster.h"
#include "bb/macro_mcts.h"
#include "bb/value_function.h"
#include "bb/policy_network.h"
#include "bb/dice.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <map>

using namespace bb;

namespace {

struct Funnel {
    long blocks = 0;      // BLOCK eventy (hozene kostky)
    long knockdowns = 0;  // KNOCKED_DOWN
    long armourBreaks = 0;
    long injuries = 0;    // INJURY event (KO i horsi)
    long casualties = 0;  // CASUALTY
};

struct Matchup { const char* home; const char* away; };
const Matchup MATCHUPS[] = {
    {"dwarf", "skaven"}, {"dwarf", "wood-elf"}, {"dwarf", "dwarf"}, {"orc", "skaven"},
};

MCTSConfig makeProdConfig(const PolicyNetwork* pol) {
    MCTSConfig cfg;
    cfg.maxIterations = 100;
    cfg.timeBudgetMs = 0;
    cfg.explorationC = 1.0;
    cfg.dirichletAlpha = 0.0f;
    cfg.vfBlend = 0.15f;
    cfg.nRollouts = 1;
    cfg.policy = pol;
    cfg.policyBlend = 0.0f;
    cfg.cageAdvance = false;
    cfg.cageGrind = false;
    return cfg;
}

void report(const char* label, const Funnel& f, int games) {
    double g = games > 0 ? games : 1;
    printf("  %-22s bloku %6.2f | srazeni %5.2f (%4.1f%% bloku) | "
           "probita zbroj %5.2f (%4.1f%% srazeni) | odchodu %5.2f (%4.1f%% zbroji)\n",
           label,
           f.blocks / g,
           f.knockdowns / g, f.blocks ? 100.0 * f.knockdowns / f.blocks : 0.0,
           f.armourBreaks / g, f.knockdowns ? 100.0 * f.armourBreaks / f.knockdowns : 0.0,
           f.injuries / g, f.armourBreaks ? 100.0 * f.injuries / f.armourBreaks : 0.0);
}

} // namespace

int main(int argc, char** argv) {
    std::string root = (argc > 1) ? argv[1] : ".";
    int nGames = (argc > 2) ? atoi(argv[2]) : 20;
    int mi = (argc > 3) ? atoi(argv[3]) : 0;
    setvbuf(stdout, nullptr, _IOLBF, 0);

    auto vf = loadValueFunction(root + "/weights_best.json");
    auto pol = loadPolicyNetworkFromFile(root + "/weights_policy.json");
    if (!vf || !pol) { fprintf(stderr, "weights required\n"); return 1; }

    const Matchup& mu = MATCHUPS[mi];
    const TeamRoster* homeR = getDevelopedRoster(mu.home, 1200);
    const TeamRoster* awayR = getDevelopedRoster(mu.away, 1200);
    if (!homeR || !awayR) { fprintf(stderr, "roster load failed\n"); return 1; }

    printf("=== BLOCK FUNNEL: %s vs %s, %d her, produkcni konfigurace ===\n",
           mu.home, mu.away, nGames);

    Funnel vsHome, vsAway;   // obet je HOME / AWAY
    long turns = 0;

    for (int i = 0; i < nGames; ++i) {
        uint32_t seed = 71'000'000u + static_cast<uint32_t>(mi) * 1'000'000u
                        + static_cast<uint32_t>(i);
        MCTSConfig cfg = makeProdConfig(pol.get());
        MacroMCTSPolicy homePol(vf.get(), cfg, seed * 2654435761u + 11u);
        MacroMCTSPolicy awayPol(vf.get(), cfg, seed * 2654435761u + 47u);
        DiceRoller dice(seed);
        LoggedGameResult lg = simulateGameLogged(
            *homeR, *awayR,
            [&](const GameState& s) { return homePol(s); },
            [&](const GameState& s) { return awayPol(s); },
            dice);

        for (const auto& tl : lg.turnLogs) {
            turns++;
            for (const auto& e : tl.events) {
                // Obet urcujeme podle ID: HOME = 1..11, AWAY = 12..22.
                auto victimIsHome = [](int id) { return id >= 1 && id <= 11; };
                switch (e.type) {
                    case GameEvent::Type::BLOCK: {
                        // u BLOCK je playerId utocnik, targetId obet
                        Funnel& f = victimIsHome(e.targetId) ? vsHome : vsAway;
                        f.blocks++;
                        break;
                    }
                    case GameEvent::Type::KNOCKED_DOWN:
                        (victimIsHome(e.playerId) ? vsHome : vsAway).knockdowns++;
                        break;
                    case GameEvent::Type::ARMOR_BREAK:
                        (victimIsHome(e.playerId) ? vsHome : vsAway).armourBreaks++;
                        break;
                    case GameEvent::Type::INJURY:
                        (victimIsHome(e.playerId) ? vsHome : vsAway).injuries++;
                        break;
                    case GameEvent::Type::CASUALTY:
                        (victimIsHome(e.playerId) ? vsHome : vsAway).casualties++;
                        break;
                    default: break;
                }
            }
        }
    }

    printf("\nprumerne tahu na hru: %.1f\n\n", turns / (double)nGames);
    printf("Trychtyr na HRU (obet = ta strana):\n");
    report((std::string("obet ") + mu.home).c_str(), vsHome, nGames);
    report((std::string("obet ") + mu.away).c_str(), vsAway, nGames);
    printf("\n  casualty/hru: %s %.2f | %s %.2f\n",
           mu.home, vsHome.casualties / (double)nGames,
           mu.away, vsAway.casualties / (double)nGames);
    printf("\nOcekavani z pravidel pro AV7 (skaven/elf): probiti 41,7%% srazeni,\n"
           "pak 16,7%% casualty a 25%% KO => 17,4%% odchodu na srazeni.\n"
           "Pro AV9 + Thick Skull (trpaslik) je odchod jen ~4,9%% na srazeni.\n");
    return 0;
}
