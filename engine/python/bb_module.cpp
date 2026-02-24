#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>
#include <pybind11/numpy.h>

#include "bb/enums.h"
#include "bb/position.h"
#include "bb/player.h"
#include "bb/game_state.h"
#include "bb/ball_state.h"
#include "bb/team_state.h"
#include "bb/rules_engine.h"
#include "bb/action_resolver.h"
#include "bb/game_simulator.h"
#include "bb/roster.h"
#include "bb/dice.h"
#include "bb/feature_extractor.h"
#include "bb/action_features.h"
#include "bb/policy_network.h"
#include "bb/policies.h"
#include "bb/game_event.h"
#include "bb/action_result.h"
#include "bb/value_function.h"
#include "bb/mcts.h"
#include "bb/macro_mcts.h"

namespace py = pybind11;

PYBIND11_MODULE(bb_engine, m) {
    m.doc() = "Blood Bowl C++ Engine - Python bindings";

    // --- Enums ---
    py::enum_<bb::TeamSide>(m, "TeamSide")
        .value("HOME", bb::TeamSide::HOME)
        .value("AWAY", bb::TeamSide::AWAY);

    py::enum_<bb::PlayerState>(m, "PlayerState")
        .value("STANDING", bb::PlayerState::STANDING)
        .value("PRONE", bb::PlayerState::PRONE)
        .value("STUNNED", bb::PlayerState::STUNNED)
        .value("KO", bb::PlayerState::KO)
        .value("INJURED", bb::PlayerState::INJURED)
        .value("DEAD", bb::PlayerState::DEAD)
        .value("EJECTED", bb::PlayerState::EJECTED)
        .value("OFF_PITCH", bb::PlayerState::OFF_PITCH);

    py::enum_<bb::GamePhase>(m, "GamePhase")
        .value("COIN_TOSS", bb::GamePhase::COIN_TOSS)
        .value("SETUP", bb::GamePhase::SETUP)
        .value("KICKOFF", bb::GamePhase::KICKOFF)
        .value("PLAY", bb::GamePhase::PLAY)
        .value("TOUCHDOWN", bb::GamePhase::TOUCHDOWN)
        .value("HALF_TIME", bb::GamePhase::HALF_TIME)
        .value("GAME_OVER", bb::GamePhase::GAME_OVER);

    py::enum_<bb::ActionType>(m, "ActionType")
        .value("MOVE", bb::ActionType::MOVE)
        .value("BLOCK", bb::ActionType::BLOCK)
        .value("BLITZ", bb::ActionType::BLITZ)
        .value("PASS", bb::ActionType::PASS)
        .value("HAND_OFF", bb::ActionType::HAND_OFF)
        .value("FOUL", bb::ActionType::FOUL)
        .value("THROW_TEAM_MATE", bb::ActionType::THROW_TEAM_MATE)
        .value("BOMB_THROW", bb::ActionType::BOMB_THROW)
        .value("HYPNOTIC_GAZE", bb::ActionType::HYPNOTIC_GAZE)
        .value("BALL_AND_CHAIN", bb::ActionType::BALL_AND_CHAIN)
        .value("MULTIPLE_BLOCK", bb::ActionType::MULTIPLE_BLOCK)
        .value("END_TURN", bb::ActionType::END_TURN);

    py::enum_<bb::Weather>(m, "Weather")
        .value("SWELTERING_HEAT", bb::Weather::SWELTERING_HEAT)
        .value("VERY_SUNNY", bb::Weather::VERY_SUNNY)
        .value("NICE", bb::Weather::NICE)
        .value("POURING_RAIN", bb::Weather::POURING_RAIN)
        .value("BLIZZARD", bb::Weather::BLIZZARD);

    // --- Position ---
    py::class_<bb::Position>(m, "Position")
        .def(py::init<>())
        .def(py::init<int8_t, int8_t>())
        .def_readwrite("x", &bb::Position::x)
        .def_readwrite("y", &bb::Position::y)
        .def("is_on_pitch", &bb::Position::isOnPitch)
        .def("distance_to", &bb::Position::distanceTo)
        .def("__repr__", [](const bb::Position& p) {
            return "Position(" + std::to_string(p.x) + ", " + std::to_string(p.y) + ")";
        })
        .def("__eq__", [](const bb::Position& a, const bb::Position& b) {
            return a == b;
        });

    // --- BallState ---
    py::class_<bb::BallState>(m, "BallState")
        .def_readwrite("position", &bb::BallState::position)
        .def_readwrite("is_held", &bb::BallState::isHeld)
        .def_readwrite("carrier_id", &bb::BallState::carrierId)
        .def("is_on_pitch", &bb::BallState::isOnPitch);

    // --- PlayerStats ---
    py::class_<bb::PlayerStats>(m, "PlayerStats")
        .def(py::init<>())
        .def_readwrite("movement", &bb::PlayerStats::movement)
        .def_readwrite("strength", &bb::PlayerStats::strength)
        .def_readwrite("agility", &bb::PlayerStats::agility)
        .def_readwrite("armour", &bb::PlayerStats::armour);

    // --- Player ---
    py::class_<bb::Player>(m, "Player")
        .def_readwrite("id", &bb::Player::id)
        .def_readwrite("team_side", &bb::Player::teamSide)
        .def_readwrite("state", &bb::Player::state)
        .def_readwrite("position", &bb::Player::position)
        .def_readwrite("stats", &bb::Player::stats)
        .def_readwrite("movement_remaining", &bb::Player::movementRemaining)
        .def_readwrite("has_moved", &bb::Player::hasMoved)
        .def_readwrite("has_acted", &bb::Player::hasActed)
        .def("has_skill", &bb::Player::hasSkill)
        .def("is_on_pitch", &bb::Player::isOnPitch)
        .def("can_act", &bb::Player::canAct);

    // --- TeamState ---
    py::class_<bb::TeamState>(m, "TeamState")
        .def_readwrite("side", &bb::TeamState::side)
        .def_readwrite("score", &bb::TeamState::score)
        .def_readwrite("rerolls", &bb::TeamState::rerolls)
        .def_readwrite("turn_number", &bb::TeamState::turnNumber);

    // --- GameState ---
    py::class_<bb::GameState>(m, "GameState")
        .def(py::init<>())
        .def_readwrite("half", &bb::GameState::half)
        .def_readwrite("phase", &bb::GameState::phase)
        .def_readwrite("active_team", &bb::GameState::activeTeam)
        .def_readwrite("home_team", &bb::GameState::homeTeam)
        .def_readwrite("away_team", &bb::GameState::awayTeam)
        .def_readwrite("ball", &bb::GameState::ball)
        .def_readwrite("weather", &bb::GameState::weather)
        .def_readwrite("kicking_team", &bb::GameState::kickingTeam)
        .def("get_player", [](bb::GameState& gs, int id) -> bb::Player& {
            return gs.getPlayer(id);
        }, py::return_value_policy::reference_internal)
        .def("clone", &bb::GameState::clone);

    // --- Action ---
    py::class_<bb::Action>(m, "Action")
        .def(py::init<>())
        .def_readwrite("type", &bb::Action::type)
        .def_readwrite("player_id", &bb::Action::playerId)
        .def_readwrite("target_id", &bb::Action::targetId)
        .def_readwrite("target", &bb::Action::target)
        .def("__repr__", [](const bb::Action& a) {
            return "Action(type=" + std::to_string(static_cast<int>(a.type)) +
                   ", player=" + std::to_string(a.playerId) +
                   ", target_id=" + std::to_string(a.targetId) + ")";
        });

    // --- GameResult ---
    py::class_<bb::GameResult>(m, "GameResult")
        .def_readwrite("home_score", &bb::GameResult::homeScore)
        .def_readwrite("away_score", &bb::GameResult::awayScore)
        .def_readwrite("total_actions", &bb::GameResult::totalActions);

    // --- LoggedGameResult ---
    py::class_<bb::LoggedGameResult>(m, "LoggedGameResult")
        .def_readwrite("result", &bb::LoggedGameResult::result)
        .def("get_states", [](const bb::LoggedGameResult& lgr) {
            // Return list of (features_numpy, perspective_str) tuples
            py::list result;
            for (auto& s : lgr.states) {
                py::dict d;
                d["features"] = py::array_t<float>(bb::NUM_FEATURES, s.features);
                d["perspective"] = s.perspective == bb::TeamSide::HOME ? "home" : "away";
                result.append(d);
            }
            return result;
        })
        .def("get_policy_decisions", [](const bb::LoggedGameResult& lgr) {
            py::list result;
            for (auto& dec : lgr.policyDecisions) {
                py::dict d;
                d["state_features"] = py::array_t<float>(bb::NUM_FEATURES, dec.stateFeatures);
                d["perspective"] = dec.perspective == bb::TeamSide::HOME ? "home" : "away";

                py::list visits;
                for (auto& v : dec.visits) {
                    py::dict vd;
                    vd["action_features"] = py::array_t<float>(
                        bb::NUM_ACTION_FEATURES, v.actionFeatures);
                    vd["visit_fraction"] = v.visitFraction;
                    visits.append(vd);
                }
                d["visits"] = visits;
                result.append(d);
            }
            return result;
        });

    // --- DiceRoller ---
    py::class_<bb::DiceRoller>(m, "DiceRoller")
        .def(py::init<uint32_t>())
        .def("roll_d6", &bb::DiceRoller::rollD6)
        .def("roll_d8", &bb::DiceRoller::rollD8)
        .def("roll_2d6", &bb::DiceRoller::roll2D6);

    // --- TeamRoster ---
    py::class_<bb::TeamRoster>(m, "TeamRoster")
        .def_readonly("name", &bb::TeamRoster::name)
        .def_readonly("positional_count", &bb::TeamRoster::positionalCount)
        .def_readonly("reroll_cost", &bb::TeamRoster::rerollCost)
        .def_readonly("has_apothecary", &bb::TeamRoster::hasApothecary);

    // --- ActionResult ---
    py::class_<bb::ActionResult>(m, "ActionResult")
        .def_readwrite("success", &bb::ActionResult::success)
        .def_readwrite("turnover", &bb::ActionResult::turnover);

    // --- GameEvent ---
    py::class_<bb::GameEvent>(m, "GameEvent")
        .def_readwrite("player_id", &bb::GameEvent::playerId)
        .def_readwrite("target_id", &bb::GameEvent::targetId)
        .def_readwrite("from_pos", &bb::GameEvent::from)
        .def_readwrite("to_pos", &bb::GameEvent::to)
        .def_readwrite("roll", &bb::GameEvent::roll)
        .def_readwrite("success", &bb::GameEvent::success);

    // --- Free functions ---
    m.def("get_available_actions", [](const bb::GameState& state) {
        std::vector<bb::Action> actions;
        bb::getAvailableActions(state, actions);
        return actions;
    });

    m.def("execute_action", [](bb::GameState& state, const bb::Action& action, bb::DiceRoller& dice) {
        bb::DiceRollerBase& base = dice;
        return bb::executeAction(state, action, base, nullptr);
    });

    m.def("setup_half", &bb::setupHalf,
          py::arg("state"), py::arg("home"), py::arg("away"),
          py::arg("kicking_team") = bb::TeamSide::AWAY);
    m.def("simple_kickoff", [](bb::GameState& state, bb::DiceRoller& dice) {
        bb::DiceRollerBase& base = dice;
        bb::simpleKickoff(state, base);
    });

    m.def("extract_features", [](const bb::GameState& state, bb::TeamSide perspective) {
        float features[bb::NUM_FEATURES];
        bb::extractFeatures(state, perspective, features);
        return py::array_t<float>(bb::NUM_FEATURES, features);
    });

    // simulate_game: supports "random", "greedy", "learning", and "mcts" AI types
    m.def("simulate_game", [](const bb::TeamRoster& home, const bb::TeamRoster& away,
                               const std::string& homeAI, const std::string& awayAI,
                               uint32_t seed,
                               const std::string& weightsPath,
                               float epsilon,
                               int mctsIterations) {
        bb::DiceRoller dice(seed);

        // Load value function if needed
        std::unique_ptr<bb::ValueFunction> vf;
        if ((homeAI == "learning" || awayAI == "learning" ||
             homeAI == "mcts" || awayAI == "mcts" ||
             homeAI == "macro_mcts" || awayAI == "macro_mcts") && !weightsPath.empty()) {
            vf = bb::loadValueFunction(weightsPath);
        }

        // MCTS/MacroMCTS policies need to persist across calls (they hold state)
        std::shared_ptr<bb::MCTSPolicy> homeMcts, awayMcts;
        std::shared_ptr<bb::MacroMCTSPolicy> homeMacroMcts, awayMacroMcts;

        auto makePolicy = [&](const std::string& ai,
                              std::shared_ptr<bb::MCTSPolicy>& mctsOut,
                              std::shared_ptr<bb::MacroMCTSPolicy>& macroMctsOut) -> bb::ActionSelector {
            if (ai == "greedy") {
                return [&dice](const bb::GameState& s) { return bb::greedyPolicy(s, dice); };
            } else if (ai == "macro_mcts" && mctsIterations > 0) {
                bb::MCTSConfig cfg;
                cfg.maxIterations = mctsIterations;
                cfg.timeBudgetMs = 0;
                cfg.explorationC = 1.0;   // Eval: low C for exploitation
                cfg.dirichletAlpha = 0.0f; // No noise during evaluation
                macroMctsOut = std::make_shared<bb::MacroMCTSPolicy>(vf.get(), cfg, seed);
                return [m = macroMctsOut](const bb::GameState& s) { return (*m)(s); };
            } else if (ai == "mcts" && vf && mctsIterations > 0) {
                bb::MCTSConfig cfg;
                cfg.maxIterations = mctsIterations;
                cfg.timeBudgetMs = 0;
                cfg.maxChildren = 40;
                mctsOut = std::make_shared<bb::MCTSPolicy>(vf.get(), cfg, seed);
                return [mcts = mctsOut](const bb::GameState& s) { return (*mcts)(s); };
            } else if (ai == "learning" && vf) {
                return [&dice, &vf, epsilon](const bb::GameState& s) {
                    return bb::learningPolicy(s, dice, *vf, epsilon);
                };
            } else {
                return [&dice](const bb::GameState& s) { return bb::randomPolicy(s, dice); };
            }
        };

        return bb::simulateGame(home, away,
            makePolicy(homeAI, homeMcts, homeMacroMcts),
            makePolicy(awayAI, awayMcts, awayMacroMcts), dice);
    }, py::arg("home"), py::arg("away"),
       py::arg("home_ai") = "random", py::arg("away_ai") = "random",
       py::arg("seed") = 42,
       py::arg("weights_path") = "",
       py::arg("epsilon") = 0.3f,
       py::arg("mcts_iterations") = 0);

    // simulate_game_logged: returns result + features at turn boundaries + policy decisions
    m.def("simulate_game_logged", [](const bb::TeamRoster& home, const bb::TeamRoster& away,
                                      const std::string& homeAI, const std::string& awayAI,
                                      uint32_t seed,
                                      const std::string& weightsPath,
                                      float epsilon,
                                      int mctsIterations,
                                      const std::string& policyWeightsPath) {
        bb::DiceRoller dice(seed);

        std::unique_ptr<bb::ValueFunction> vf;
        if ((homeAI == "learning" || awayAI == "learning" ||
             homeAI == "mcts" || awayAI == "mcts" ||
             homeAI == "macro_mcts" || awayAI == "macro_mcts") && !weightsPath.empty()) {
            vf = bb::loadValueFunction(weightsPath);
        }

        // Load policy network if provided
        std::unique_ptr<bb::PolicyNetwork> policyNet;
        if (!policyWeightsPath.empty()) {
            policyNet = bb::loadPolicyNetworkFromFile(policyWeightsPath);
        }

        std::shared_ptr<bb::MCTSPolicy> homeMcts, awayMcts;
        std::shared_ptr<bb::MacroMCTSPolicy> homeMacroMcts, awayMacroMcts;

        auto makePolicy = [&](const std::string& ai,
                              std::shared_ptr<bb::MCTSPolicy>& mctsOut,
                              std::shared_ptr<bb::MacroMCTSPolicy>& macroMctsOut) -> bb::ActionSelector {
            if (ai == "greedy") {
                return [&dice](const bb::GameState& s) { return bb::greedyPolicy(s, dice); };
            } else if (ai == "macro_mcts" && mctsIterations > 0) {
                bb::MCTSConfig cfg;
                cfg.maxIterations = mctsIterations;
                cfg.timeBudgetMs = 0;
                cfg.explorationC = 2.0;  // Training: higher C for diverse data
                cfg.dirichletAlpha = 0.3f;   // Dirichlet noise for exploration
                cfg.dirichletWeight = 0.25f; // 75% policy + 25% noise
                if (policyNet) {
                    cfg.policy = policyNet.get();
                }
                macroMctsOut = std::make_shared<bb::MacroMCTSPolicy>(vf.get(), cfg, seed);
                macroMctsOut->setLogDecisions(true, 20);
                return [m = macroMctsOut](const bb::GameState& s) { return (*m)(s); };
            } else if (ai == "mcts" && mctsIterations > 0) {
                bb::MCTSConfig cfg;
                cfg.maxIterations = mctsIterations;
                cfg.timeBudgetMs = 0;
                cfg.maxChildren = 40;
                if (policyNet) {
                    cfg.policy = policyNet.get();
                    cfg.explorationC = 2.5;
                }
                mctsOut = std::make_shared<bb::MCTSPolicy>(vf.get(), cfg, seed);
                mctsOut->setLogDecisions(true, 20);
                return [mcts = mctsOut](const bb::GameState& s) { return (*mcts)(s); };
            } else if (ai == "learning" && vf) {
                return [&dice, &vf, epsilon](const bb::GameState& s) {
                    return bb::learningPolicy(s, dice, *vf, epsilon);
                };
            } else {
                return [&dice](const bb::GameState& s) { return bb::randomPolicy(s, dice); };
            }
        };

        auto logged = bb::simulateGameLogged(
            home, away,
            makePolicy(homeAI, homeMcts, homeMacroMcts),
            makePolicy(awayAI, awayMcts, awayMacroMcts),
            dice);

        // Copy policy decisions from MCTS policies
        if (homeMcts) {
            for (auto& d : homeMcts->decisions()) {
                logged.policyDecisions.push_back(d);
            }
        }
        if (awayMcts) {
            for (auto& d : awayMcts->decisions()) {
                logged.policyDecisions.push_back(d);
            }
        }
        // Copy from MacroMCTS policies
        if (homeMacroMcts) {
            for (auto& d : homeMacroMcts->decisions()) {
                logged.policyDecisions.push_back(d);
            }
        }
        if (awayMacroMcts) {
            for (auto& d : awayMacroMcts->decisions()) {
                logged.policyDecisions.push_back(d);
            }
        }

        return logged;
    }, py::arg("home"), py::arg("away"),
       py::arg("home_ai") = "random", py::arg("away_ai") = "random",
       py::arg("seed") = 42,
       py::arg("weights_path") = "",
       py::arg("epsilon") = 0.3f,
       py::arg("mcts_iterations") = 0,
       py::arg("policy_weights_path") = "");

    // --- Roster getters ---
    m.def("get_roster", [](const std::string& name) -> const bb::TeamRoster* {
        return bb::getRosterByName(name);
    }, py::return_value_policy::reference);

    m.def("get_human_roster", &bb::getHumanRoster, py::return_value_policy::reference);
    m.def("get_orc_roster", &bb::getOrcRoster, py::return_value_policy::reference);
    m.def("get_skaven_roster", &bb::getSkavenRoster, py::return_value_policy::reference);
    m.def("get_dwarf_roster", &bb::getDwarfRoster, py::return_value_policy::reference);

    m.attr("NUM_FEATURES") = bb::NUM_FEATURES;
    m.attr("NUM_ACTION_FEATURES") = bb::NUM_ACTION_FEATURES;
}
