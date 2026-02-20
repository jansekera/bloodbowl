#include "bb/game_simulator.h"
#include "bb/policies.h"
#include "bb/roster.h"
#include "bb/value_function.h"
#include <iostream>
#include <string>
#include <cstring>
#include <chrono>
#include <memory>

using namespace bb;

namespace {

struct Options {
    std::string homeAI = "random";
    std::string awayAI = "random";
    int games = 100;
    int timeBudgetMs = 1000;
    std::string weightsPath;
    double explorationC = 1.41;
    uint32_t seed = 42;
    std::string homeRoster = "human";
    std::string awayRoster = "human";
    bool verbose = false;
};

const TeamRoster& getRoster(const std::string& name) {
    const TeamRoster* r = getRosterByName(name);
    if (r) return *r;
    return getHumanRoster();  // default
}

void printUsage() {
    std::cout << "Usage: mcts_cli [options]\n"
              << "\nOptions:\n"
              << "  --home=POLICY     Home AI: random, greedy, mcts (default: random)\n"
              << "  --away=POLICY     Away AI: random, greedy, mcts (default: random)\n"
              << "  --games=N         Number of games (default: 100)\n"
              << "  --time=MS         MCTS time budget in ms (default: 1000)\n"
              << "  --weights=PATH    Path to weights JSON file\n"
              << "  --exploration=C   UCT exploration constant (default: 1.41)\n"
              << "  --seed=N          RNG seed (default: 42)\n"
              << "  --home-roster=R   Home roster: human, orc, skaven, dwarf, wood-elf, chaos,\n"
              << "                    undead, lizardmen, dark-elf, halfling, norse, high-elf,\n"
              << "                    vampire, amazon, necromantic, bretonnian, khemri, goblin,\n"
              << "                    chaos-dwarf, ogre, nurgle, pro-elf, slann, underworld,\n"
              << "                    khorne, chaos-pact (default: human)\n"
              << "  --away-roster=R   Same roster options as --home-roster (default: human)\n"
              << "  --verbose         Print per-game results\n"
              << "  --help            Show this help\n";
}

Options parseArgs(int argc, char* argv[]) {
    Options opts;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.find("--home=") == 0) opts.homeAI = arg.substr(7);
        else if (arg.find("--away=") == 0) opts.awayAI = arg.substr(7);
        else if (arg.find("--games=") == 0) opts.games = std::stoi(arg.substr(8));
        else if (arg.find("--time=") == 0) opts.timeBudgetMs = std::stoi(arg.substr(7));
        else if (arg.find("--weights=") == 0) opts.weightsPath = arg.substr(10);
        else if (arg.find("--exploration=") == 0) opts.explorationC = std::stod(arg.substr(14));
        else if (arg.find("--seed=") == 0) opts.seed = static_cast<uint32_t>(std::stoul(arg.substr(7)));
        else if (arg.find("--home-roster=") == 0) opts.homeRoster = arg.substr(14);
        else if (arg.find("--away-roster=") == 0) opts.awayRoster = arg.substr(14);
        else if (arg == "--verbose") opts.verbose = true;
        else if (arg == "--help") { printUsage(); exit(0); }
        else { std::cerr << "Unknown option: " << arg << "\n"; printUsage(); exit(1); }
    }
    return opts;
}

} // anonymous namespace

int main(int argc, char* argv[]) {
    Options opts = parseArgs(argc, argv);

    // Load value function if specified
    std::unique_ptr<ValueFunction> valueFn;
    if (!opts.weightsPath.empty()) {
        valueFn = loadValueFunction(opts.weightsPath);
        if (!valueFn) {
            std::cerr << "Failed to load weights from: " << opts.weightsPath << "\n";
            return 1;
        }
        std::cout << "Loaded weights from " << opts.weightsPath << "\n";
    }

    const TeamRoster& homeRoster = getRoster(opts.homeRoster);
    const TeamRoster& awayRoster = getRoster(opts.awayRoster);

    MCTSConfig mctsConfig;
    mctsConfig.timeBudgetMs = opts.timeBudgetMs;
    mctsConfig.explorationC = opts.explorationC;
    mctsConfig.verbose = opts.verbose;

    std::cout << "Match: " << opts.homeAI << " (" << homeRoster.name << ") vs "
              << opts.awayAI << " (" << awayRoster.name << ")\n";
    std::cout << "Games: " << opts.games << "\n";
    if (opts.homeAI == "mcts" || opts.awayAI == "mcts") {
        std::cout << "MCTS time budget: " << opts.timeBudgetMs << "ms\n";
    }

    int homeWins = 0, awayWins = 0, draws = 0;
    int totalHomeScore = 0, totalAwayScore = 0;

    auto benchStart = std::chrono::steady_clock::now();

    for (int g = 0; g < opts.games; ++g) {
        uint32_t gameSeed = opts.seed + g;
        DiceRoller dice(gameSeed);

        // Create policies
        // For MCTS, each game gets its own policy instance with unique seed
        std::unique_ptr<MCTSPolicy> homeMCTS, awayMCTS;

        ActionSelector homePolicy;
        if (opts.homeAI == "mcts") {
            homeMCTS = std::make_unique<MCTSPolicy>(valueFn.get(), mctsConfig, gameSeed * 31);
            homePolicy = [&](const GameState& s) { return (*homeMCTS)(s); };
        } else if (opts.homeAI == "greedy") {
            homePolicy = [&dice](const GameState& s) { return greedyPolicy(s, dice); };
        } else {
            homePolicy = [&dice](const GameState& s) { return randomPolicy(s, dice); };
        }

        ActionSelector awayPolicy;
        if (opts.awayAI == "mcts") {
            awayMCTS = std::make_unique<MCTSPolicy>(valueFn.get(), mctsConfig, gameSeed * 37);
            awayPolicy = [&](const GameState& s) { return (*awayMCTS)(s); };
        } else if (opts.awayAI == "greedy") {
            awayPolicy = [&dice](const GameState& s) { return greedyPolicy(s, dice); };
        } else {
            awayPolicy = [&dice](const GameState& s) { return randomPolicy(s, dice); };
        }

        GameResult result = simulateGame(homeRoster, awayRoster,
                                          homePolicy, awayPolicy, dice);

        totalHomeScore += result.homeScore;
        totalAwayScore += result.awayScore;

        if (result.homeScore > result.awayScore) homeWins++;
        else if (result.awayScore > result.homeScore) awayWins++;
        else draws++;

        if (opts.verbose) {
            std::cout << "Game " << (g + 1) << ": "
                      << result.homeScore << "-" << result.awayScore
                      << " (" << result.totalActions << " actions)\n";
        }
    }

    auto benchEnd = std::chrono::steady_clock::now();
    double totalSec = std::chrono::duration<double>(benchEnd - benchStart).count();

    std::cout << "\n=== Results ===\n";
    std::cout << "Home wins: " << homeWins << " (" << (100.0 * homeWins / opts.games) << "%)\n";
    std::cout << "Away wins: " << awayWins << " (" << (100.0 * awayWins / opts.games) << "%)\n";
    std::cout << "Draws:     " << draws << " (" << (100.0 * draws / opts.games) << "%)\n";
    std::cout << "Avg score: " << (1.0 * totalHomeScore / opts.games)
              << " - " << (1.0 * totalAwayScore / opts.games) << "\n";
    std::cout << "Time:      " << totalSec << "s ("
              << (opts.games / totalSec) << " games/sec)\n";

    return 0;
}
