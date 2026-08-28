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
#include "bb/macro_actions.h"

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
        .value("END_TURN", bb::ActionType::END_TURN)
        .value("LEAP", bb::ActionType::LEAP);

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
            auto playersToList = [](const std::vector<bb::PlayerSnapshot>& players) {
                py::list out;
                for (auto& p : players) {
                    py::dict pd;
                    pd["id"] = p.id;
                    pd["x"] = p.x;
                    pd["y"] = p.y;
                    pd["state"] = p.state;
                    pd["has_ball"] = p.hasBall;
                    pd["name"] = p.name;
                    pd["ma"] = p.ma;
                    pd["st"] = p.st;
                    pd["ag"] = p.ag;
                    pd["av"] = p.av;
                    out.append(pd);
                }
                return out;
            };

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

                // Raw board snapshot at decision time (offline per-player feature research)
                d["home_players"] = playersToList(dec.board.homePlayers);
                d["away_players"] = playersToList(dec.board.awayPlayers);
                d["ball_x"] = dec.board.ballX;
                d["ball_y"] = dec.board.ballY;
                d["ball_held"] = dec.board.ballHeld;
                d["ball_carrier_id"] = dec.board.ballCarrierId;

                result.append(d);
            }
            return result;
        })
        .def("get_turn_logs", [](const bb::LoggedGameResult& lgr) {
            py::list result;
            // GameEvent type names
            static const char* eventNames[] = {
                "MOVE", "DODGE", "GFI", "BLOCK", "PUSH", "INJURY",
                "TOUCHDOWN", "TURNOVER", "BALL_BOUNCE", "PASS", "CATCH",
                "PICKUP", "FOUL", "KICKOFF", "WEATHER", "SKILL",
                "KNOCKED_DOWN", "ARMOR_BREAK", "CASUALTY", "REGENERATION",
                "EJECTED", "HAND_OFF", "STAND_UP", "LEAP", "FOLLOW_UP"
                // Positional map onto GameEvent::Type -- append only.
            };
            for (auto& turn : lgr.turnLogs) {
                py::dict t;
                t["half"] = turn.half;
                t["turn"] = turn.turnNumber;
                t["active_team"] = turn.activeTeam == bb::TeamSide::HOME ? "home" : "away";
                t["home_score"] = turn.homeScore;
                t["away_score"] = turn.awayScore;
                t["ball_x"] = turn.ballX;
                t["ball_y"] = turn.ballY;
                t["ball_held"] = turn.ballHeld;
                t["ball_carrier_id"] = turn.ballCarrierId;
                t["turnover"] = turn.turnover;
                t["touchdown"] = turn.touchdown;

                // What the turn planner decided (bb/turn_plan_record.h).
                // plan_written == false means no planner ran at all -- the
                // whole turn belonged to per-macro search().
                {
                    static const char* goalNames[] = {
                        "NONE", "PICKUP_BALL", "ADVANCE_BALL", "SCORE_BALL"};
                    static const char* verdictNames[] = {
                        "NOT_APPLICABLE", "TEMPO_INSUFFICIENT", "DICEY", "PLAN_READY",
                        "FILL_ONLY"};
                    const auto& pl = turn.plan;
                    py::dict pd;
                    pd["written"] = pl.written;
                    pd["goal"] = (pl.goal < 4) ? goalNames[pl.goal] : "UNKNOWN";
                    pd["verdict"] = (pl.verdict < 5) ? verdictNames[pl.verdict]
                                                     : "NOT_CONSULTED";
                    pd["adopted"] = pl.adopted;
                    pd["dist_to_endzone"] = pl.distToEndzone;
                    pd["turns_left"] = pl.turnsLeft;
                    pd["required_pace"] = pl.requiredPace;
                    pd["achievable_pace"] = pl.achievablePace;
                    pd["raw_achievable_step"] = pl.rawAchievableStep;
                    pd["step"] = pl.step;
                    pd["resistance"] = pl.resistance;
                    pd["filled_corners"] = pl.filledCorners;
                    pd["open_corners"] = pl.openCorners;
                    pd["carrier_gfi"] = pl.carrierGfi;
                    pd["exposure"] = pl.exposure;
                    t["plan"] = pd;
                }
                // K9b (08-18): odpor mimo plánovač -- viz game_simulator.h
                t["corridor_resistance"] = turn.corridorResistance;
                t["corridor_strength"] = turn.corridorStrength;
                t["required_pace"] = turn.requiredPace;
                t["achievable_pace"] = turn.achievablePace;
                t["cage_corners"] = turn.cageCorners;
                t["cage_corners_marked"] = turn.cageCornersMarked;
                t["cage_ortho_occupied"] = turn.cageOrthoOccupied;
                t["cage_ortho_ours"] = turn.cageOrthoOurs;
                t["cage_ahead_occupied"] = turn.cageAheadOccupied;
                t["cage_ahead_ours"] = turn.cageAheadOurs;
                t["carrier_tz"] = turn.carrierTz;
                t["activated"] = turn.activatedCount;
                t["eligible_at_start"] = turn.eligibleAtStart;
                t["moved"] = turn.movedCount;
                t["dist_to_endzone_board"] = turn.distToEndzone;
                switch (turn.weather) {
                    case bb::Weather::SWELTERING_HEAT: t["weather"] = "sweltering_heat"; break;
                    case bb::Weather::VERY_SUNNY:      t["weather"] = "very_sunny"; break;
                    case bb::Weather::NICE:            t["weather"] = "nice"; break;
                    case bb::Weather::POURING_RAIN:    t["weather"] = "pouring_rain"; break;
                    case bb::Weather::BLIZZARD:        t["weather"] = "blizzard"; break;
                }

                // Player snapshots
                py::list home_players, away_players;
                for (auto& p : turn.homePlayers) {
                    py::dict pd;
                    pd["id"] = p.id;
                    pd["x"] = p.x;
                    pd["y"] = p.y;
                    pd["state"] = p.state;
                    pd["has_ball"] = p.hasBall;
                    pd["name"] = p.name;
                    pd["ma"] = p.ma;
                    pd["st"] = p.st;
                    pd["ag"] = p.ag;
                    pd["av"] = p.av;
                    home_players.append(pd);
                }
                for (auto& p : turn.awayPlayers) {
                    py::dict pd;
                    pd["id"] = p.id;
                    pd["x"] = p.x;
                    pd["y"] = p.y;
                    pd["state"] = p.state;
                    pd["has_ball"] = p.hasBall;
                    pd["name"] = p.name;
                    pd["ma"] = p.ma;
                    pd["st"] = p.st;
                    pd["ag"] = p.ag;
                    pd["av"] = p.av;
                    away_players.append(pd);
                }
                t["home_players"] = home_players;
                t["away_players"] = away_players;

                // Events
                py::list events;
                for (auto& ev : turn.events) {
                    py::dict ed;
                    int typeIdx = static_cast<int>(ev.type);
                    // 2026-08-17: byla tu konstanta 21, zatímco eventNames má 22
                    // položek -- HAND_OFF (index 21) se proto exportoval jako
                    // "UNKNOWN". Commit 3b11d33b přidal jméno do tabulky a
                    // nezvedl tuhle stráž, takže log tvrdil ZERO hand-offů ve
                    // 3 000 hrách, ačkoli jich naše strana zahrála 130. Stálo
                    // to jeden nepravdivý doktrinální závěr (P21).
                    // Velikost se bere z pole, aby se to při dalším přidání
                    // typu nemohlo opakovat.
                    constexpr int kNumEventNames =
                        static_cast<int>(sizeof(eventNames) / sizeof(eventNames[0]));
                    ed["type"] = (typeIdx >= 0 && typeIdx < kNumEventNames)
                                     ? eventNames[typeIdx] : "UNKNOWN";
                    ed["player_id"] = ev.playerId;
                    ed["target_id"] = ev.targetId;
                    ed["from_x"] = ev.from.x;
                    ed["from_y"] = ev.from.y;
                    ed["to_x"] = ev.to.x;
                    ed["to_y"] = ev.to.y;
                    ed["roll"] = ev.roll;
                    ed["success"] = ev.success;
                    ed["die1"] = ev.die1;
                    ed["die2"] = ev.die2;
                    events.append(ed);
                }
                t["events"] = events;
                result.append(t);
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
        .def_readwrite("success", &bb::GameEvent::success)
        .def_readwrite("die1", &bb::GameEvent::die1)
        .def_readwrite("die2", &bb::GameEvent::die2);

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

    // KO recovery (package G) needs a dice source; the Python binding keeps
    // its old 4-argument shape and passes nullptr, which simply means KO'd
    // players stay out rather than rolling to return.
    m.def("setup_half",
          [](bb::GameState& state, const bb::TeamRoster& home,
             const bb::TeamRoster& away, bb::TeamSide kickingTeam) {
              bb::setupHalf(state, home, away, kickingTeam, nullptr);
          },
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
                               int mctsIterations,
                               float policyBlend,
                               float vfBlend,
                               bool riskDeferral) {
        bb::DiceRoller dice(seed);

        // Load value function if needed
        std::unique_ptr<bb::ValueFunction> vf;
        if ((homeAI == "learning" || awayAI == "learning" ||
             homeAI == "mcts" || awayAI == "mcts" ||
             homeAI == "macro_mcts" || awayAI == "macro_mcts") && !weightsPath.empty()) {
            vf = bb::loadValueFunction(weightsPath);
        }

        // Load policy network from weights file (if it contains policy data)
        std::unique_ptr<bb::PolicyNetwork> policyNet;
        if (!weightsPath.empty() && (homeAI == "mcts" || awayAI == "mcts" ||
            homeAI == "macro_mcts" || awayAI == "macro_mcts")) {
            policyNet = bb::loadPolicyNetworkFromFile(weightsPath);
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
                cfg.vfBlend = vfBlend;
                cfg.riskDeferral = riskDeferral;
                if (policyNet) {
                    cfg.policy = policyNet.get();
                    cfg.policyBlend = policyBlend;
                }
                macroMctsOut = std::make_shared<bb::MacroMCTSPolicy>(vf.get(), cfg, seed);
                return [m = macroMctsOut](const bb::GameState& s) { return (*m)(s); };
            } else if (ai == "mcts" && vf && mctsIterations > 0) {
                bb::MCTSConfig cfg;
                cfg.maxIterations = mctsIterations;
                cfg.timeBudgetMs = 0;
                cfg.maxChildren = 40;
                if (policyNet) {
                    cfg.policy = policyNet.get();
                    cfg.explorationC = 2.5;
                }
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
       py::arg("mcts_iterations") = 0,
       py::arg("policy_blend") = 0.0f,
       py::arg("vf_blend") = 0.0f,
       py::arg("risk_deferral") = false);  // 2026-07-28 (item 10): Q-guarded risk-sequencing defer (macro_mcts only)

    // simulate_game_logged: returns result + features at turn boundaries + policy decisions
    m.def("simulate_game_logged", [](const bb::TeamRoster& home, const bb::TeamRoster& away,
                                      const std::string& homeAI, const std::string& awayAI,
                                      uint32_t seed,
                                      const std::string& weightsPath,
                                      float epsilon,
                                      int mctsIterations,
                                      const std::string& policyWeightsPath,
                                      float policyBlend,
                                      float vfBlend,
                                      const std::string& awayWeightsPath,
                                      float dirichletAlpha,
                                      float explorationC,
                                      int nRollouts,
                                      bool leafLookahead,
                                      bool riskDeferral,
                                      const std::string& awayPolicyWeightsPath,
                                      float awayPolicyBlend,
                                      bool stagedPickup,
                                      bool awayStagedPickup,
                                      bool cageAdvance,
                                      bool awayCageAdvance,
                                      bool dauntlessInOffer,
                                      bool blitzContinuation) {
        bb::DiceRoller dice(seed);

        // Home VF (training weights)
        std::unique_ptr<bb::ValueFunction> vf;
        if ((homeAI == "learning" || homeAI == "mcts" || homeAI == "macro_mcts") && !weightsPath.empty()) {
            vf = bb::loadValueFunction(weightsPath);
        }

        // Away VF: frozen weights if provided, otherwise same as home
        std::unique_ptr<bb::ValueFunction> awayVf;
        const std::string& awayWPath = awayWeightsPath.empty() ? weightsPath : awayWeightsPath;
        if ((awayAI == "learning" || awayAI == "mcts" || awayAI == "macro_mcts") && !awayWPath.empty()) {
            awayVf = bb::loadValueFunction(awayWPath);
        }

        // Load policy network if provided. A separate away-side net (plus
        // blend) enables a fair policy A/B -- with a single shared net a
        // candidate-policy-vs-champion-policy match was impossible to set up
        // (fable_pipeline_audit_20260730 N1).
        std::unique_ptr<bb::PolicyNetwork> policyNet;
        if (!policyWeightsPath.empty()) {
            policyNet = bb::loadPolicyNetworkFromFile(policyWeightsPath);
        }
        std::unique_ptr<bb::PolicyNetwork> awayPolicyNet;
        if (!awayPolicyWeightsPath.empty()) {
            awayPolicyNet = bb::loadPolicyNetworkFromFile(awayPolicyWeightsPath);
        }

        std::shared_ptr<bb::MCTSPolicy> homeMcts, awayMcts;
        std::shared_ptr<bb::MacroMCTSPolicy> homeMacroMcts, awayMacroMcts;

        auto makePolicy = [&](const std::string& ai,
                              bb::ValueFunction* vfPtr,
                              bb::PolicyNetwork* polPtr,
                              float polBlend,
                              bool stagedPlanner,
                              bool cageOn,
                              std::shared_ptr<bb::MCTSPolicy>& mctsOut,
                              std::shared_ptr<bb::MacroMCTSPolicy>& macroMctsOut) -> bb::ActionSelector {
            if (ai == "greedy") {
                return [&dice](const bb::GameState& s) { return bb::greedyPolicy(s, dice); };
            } else if (ai == "macro_mcts" && mctsIterations > 0) {
                bb::MCTSConfig cfg;
                cfg.maxIterations = mctsIterations;
                cfg.timeBudgetMs = 0;
                cfg.explorationC = explorationC;
                cfg.dirichletAlpha = dirichletAlpha;
                cfg.dirichletWeight = 0.25f;
                cfg.vfBlend = vfBlend;
                cfg.nRollouts = nRollouts;
                cfg.leafLookahead = leafLookahead;
                cfg.riskDeferral = riskDeferral;
                cfg.stagedPickupPlanner = stagedPlanner;
                cfg.cageAdvance = cageOn;
                // 2026-08-14: applies to BOTH sides on purpose. Unlike the cage
                // planner this is not a doctrine we are trying on our dwarves --
                // it is the block offer refusing to see a skill the resolver
                // already honours, so leaving it on one side would compare two
                // different engines rather than two arms.
                cfg.dauntlessInOffer = dauntlessInOffer;
                if (polPtr) {
                    cfg.policy = polPtr;
                    cfg.policyBlend = polBlend;
                }
                macroMctsOut = std::make_shared<bb::MacroMCTSPolicy>(vfPtr, cfg, seed);
                macroMctsOut->setLogDecisions(true, 20);
                return [m = macroMctsOut](const bb::GameState& s) { return (*m)(s); };
            } else if (ai == "mcts" && mctsIterations > 0) {
                bb::MCTSConfig cfg;
                cfg.maxIterations = mctsIterations;
                cfg.timeBudgetMs = 0;
                cfg.maxChildren = 40;
                if (polPtr) {
                    cfg.policy = polPtr;
                    cfg.explorationC = 2.5;
                }
                mctsOut = std::make_shared<bb::MCTSPolicy>(vfPtr, cfg, seed);
                mctsOut->setLogDecisions(true, 20);
                return [mcts = mctsOut](const bb::GameState& s) { return (*mcts)(s); };
            } else if (ai == "learning" && vfPtr) {
                return [&dice, vfPtr, epsilon](const bb::GameState& s) {
                    return bb::learningPolicy(s, dice, *vfPtr, epsilon);
                };
            } else {
                return [&dice](const bb::GameState& s) { return bb::randomPolicy(s, dice); };
            }
        };

        // 2026-08-28: M1/N10 -- blitz nechava aktivaci otevrenou (BB2016 l.
        // 347-350). BOTH sides on purpose, same argument as dauntlessInOffer:
        // it is a rule the planner did not honour, not a doctrine we are
        // trying on our dwarves, so one-sided would compare two engines.
        //
        // ⭐ PROC TO NENI NASAZENI DO PRODUKCE. Default is false. The arm
        // passed its A/B on 2026-08-28 (+0.0177 +- 0.0069, 6/6 predictions),
        // but the pre-registration left two readings unanswerable, because a
        // corpus is collected with the arms OFF: whether M9's ceiling (4.09
        // blitzes/game stuck in contact) got consumed, and how often the
        // follow-up is refused. With the arm off the second one is 0 BY
        // CONSTRUCTION -- which would read as "that half is dead". This switch
        // lets the corpus answer both WITHOUT deciding to ship, keeping
        // "measure what the arm does" apart from "decide to deploy it".
        bb::setBlitzContinuationArm(bb::TeamSide::HOME, blitzContinuation);
        bb::setBlitzContinuationArm(bb::TeamSide::AWAY, blitzContinuation);

        auto logged = bb::simulateGameLogged(
            home, away,
            makePolicy(homeAI, vf.get(), policyNet.get(), policyBlend,
                       stagedPickup, cageAdvance, homeMcts, homeMacroMcts),
            makePolicy(awayAI, awayVf.get(),
                       awayPolicyNet ? awayPolicyNet.get() : policyNet.get(),
                       awayPolicyBlend < 0.0f ? policyBlend : awayPolicyBlend,
                       awayStagedPickup, awayCageAdvance, awayMcts, awayMacroMcts),
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
       py::arg("policy_weights_path") = "",
       py::arg("policy_blend") = 0.0f,
       py::arg("vf_blend") = 0.0f,
       py::arg("away_weights_path") = "",
       py::arg("dirichlet_alpha") = 0.3f,
       py::arg("exploration_c") = 0.5f,   // T2: 2.0 over-explored, flat target; 0.5 sharpens. eval path (simulate_game) uses its own 1.0
       py::arg("n_rollouts") = 1,
       py::arg("leaf_lookahead") = false,  // 2026-07-02 experiment: bounded greedy 1-ply leaf look-ahead (macro_mcts only)
       py::arg("risk_deferral") = false,
       py::arg("away_policy_weights_path") = "",
       py::arg("away_policy_blend") = -1.0f,  // -1 = inherit policy_blend  // 2026-07-28 (item 10): Q-guarded risk-sequencing defer (macro_mcts only)
       py::arg("staged_pickup") = false,       // 2026-08-05 (item 13): staged safe-then-PICKUP whole-turn planner, per side
       py::arg("away_staged_pickup") = false,  // so the gate can run candidate-only while frozen keeps its promoted config
       py::arg("cage_advance") = false,        // 2026-08-11: F1 cage-advance planner, per side. Default off = production.
       py::arg("away_cage_advance") = false,   // Exposed so the verdict distribution can be read at all -- with it off the planner is never consulted.
       py::arg("dauntless_in_offer") = true,
       py::arg("blitz_continuation") = false);  // 2026-08-14: price a block at the strength Dauntless would equalise to. BOTH sides. 2026-08-17: default TRUE = production, after the A/B passed (+3.59 pp vs the null arms). Corpora collected through this binding must match what production plays, or we go back to measuring one engine and shipping another.

    // --- Roster getters ---
    m.def("get_roster", [](const std::string& name) -> const bb::TeamRoster* {
        return bb::getRosterByName(name);
    }, py::return_value_policy::reference);

    // Developed (skilled) roster for a team value; tv >= 1200 returns a TV~1200
    // variant for orc/human/dwarf/skaven, otherwise falls back to the base roster.
    m.def("get_developed_roster", [](const std::string& name, int tv) -> const bb::TeamRoster* {
        return bb::getDevelopedRoster(name, tv);
    }, py::arg("name"), py::arg("tv") = 1000, py::return_value_policy::reference);

    m.def("get_human_roster", &bb::getHumanRoster, py::return_value_policy::reference);
    m.def("get_orc_roster", &bb::getOrcRoster, py::return_value_policy::reference);
    m.def("get_skaven_roster", &bb::getSkavenRoster, py::return_value_policy::reference);
    m.def("get_dwarf_roster", &bb::getDwarfRoster, py::return_value_policy::reference);

    m.attr("NUM_FEATURES") = bb::NUM_FEATURES;
    m.attr("NUM_ACTION_FEATURES") = bb::NUM_ACTION_FEATURES;
}
