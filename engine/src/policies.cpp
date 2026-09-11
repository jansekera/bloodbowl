#include "bb/policies.h"
#include "bb/action_resolver.h"
#include <algorithm>

namespace bb {

Action randomPolicy(const GameState& state, DiceRollerBase& dice) {
    std::vector<Action> actions;
    getAvailableActions(state, actions);

    if (actions.empty()) {
        return Action{ActionType::END_TURN, -1, -1, {-1, -1}};
    }

    // Use dice to pick random index
    int idx = 0;
    if (actions.size() > 1) {
        // Use multiple D6 rolls to get a reasonable uniform distribution
        int r = dice.rollD6() - 1;
        r = r * 6 + (dice.rollD6() - 1);
        idx = r % static_cast<int>(actions.size());
    }

    return actions[idx];
}

Action greedyPolicy(const GameState& state, DiceRollerBase& dice) {
    std::vector<Action> actions;
    getAvailableActions(state, actions);

    if (actions.empty()) {
        return Action{ActionType::END_TURN, -1, -1, {-1, -1}};
    }

    TeamSide mySide = state.activeTeam;

    // Priority 1: Move ball carrier toward endzone (scoring)
    if (state.ball.isHeld && state.ball.carrierId > 0) {
        const Player& carrier = state.getPlayer(state.ball.carrierId);
        if (carrier.teamSide == mySide && carrier.state == PlayerState::STANDING) {
            int targetX = (mySide == TeamSide::HOME) ? 25 : 0;
            int dx = (targetX > carrier.position.x) ? 1 : -1;

            // Find move action toward endzone
            for (auto& a : actions) {
                if (a.type == ActionType::MOVE && a.playerId == carrier.id) {
                    if ((dx > 0 && a.target.x > carrier.position.x) ||
                        (dx < 0 && a.target.x < carrier.position.x)) {
                        return a;
                    }
                }
            }
        }
    }

    // Priority 2: Move a player to the ball (if ball is on ground)
    if (!state.ball.isHeld && state.ball.isOnPitch()) {
        Position ballPos = state.ball.position;

        // Direct pickup: move to the ball square
        for (auto& a : actions) {
            if (a.type == ActionType::MOVE &&
                a.target.x == ballPos.x && a.target.y == ballPos.y) {
                return a;
            }
        }

        // Move closest player toward ball
        Action bestMove{};
        int bestDist = 999;
        bool found = false;
        for (auto& a : actions) {
            if (a.type == ActionType::MOVE) {
                int dist = a.target.distanceTo(ballPos);
                if (dist < bestDist) {
                    bestDist = dist;
                    bestMove = a;
                    found = true;
                }
            }
        }
        if (found) return bestMove;
    }

    // Priority 3: Block actions (build advantage)
    std::vector<Action> blocks;
    for (auto& a : actions) {
        if (a.type == ActionType::BLOCK) blocks.push_back(a);
    }
    if (!blocks.empty()) {
        int r = 0;
        if (blocks.size() > 1) {
            r = (dice.rollD6() - 1) % static_cast<int>(blocks.size());
        }
        return blocks[r];
    }

    // Priority 4: Blitz actions
    for (auto& a : actions) {
        if (a.type == ActionType::BLITZ) return a;
    }

    // Priority 5: Move actions (non-carrier)
    std::vector<Action> moves;
    for (auto& a : actions) {
        if (a.type == ActionType::MOVE) moves.push_back(a);
    }
    if (!moves.empty()) {
        int r = 0;
        if (moves.size() > 1) {
            r = (dice.rollD6() - 1) % static_cast<int>(moves.size());
        }
        return moves[r];
    }

    // ⛔⛔⛔ OPRAVA 10.09.2026: `return actions[0]` BYL `END_TURN`, VZDYCKY.
    //   `rules_engine.cpp:17` vklada `END_TURN` jako **PRVNI** prvek nabidky
    //   ("END_TURN is always available") a nic to nepreradi ⇒ tenhle "fallback"
    //   nekoncil tah nekdy, ale KDYKOLI nesepnulo rameno MOVE/BLOCK/BLITZ.
    //   `greedyPolicy` jina ramena nema (FOUL, PASS, HAND_OFF, LEAP, TTM zadne),
    //   takze staci telo obklopene LEZICIMI soupeři: zadne volne pole (⇒ bez
    //   MOVE), zadny stojici soupeř (⇒ bez BLOCK) -- a legalni je pritom FOUL.
    //   Produkcni cesta: `macro_mcts.cpp:1327`.
    //
    // ⭐ PRAVIDLOVA KOTVA (rules_bb2016.txt r. 363-367): „Normally, a turn only
    //   ends when all of the players in the team have performed an Action.
    //   However, certain events cause the turn to end... These events are
    //   called turnovers" -- a nasleduje UZAVRENY sedmiclenny seznam
    //   (r. 368-384). „Politika nenasla rameno" v nem NENI. ⇒ Ukoncit tah tady
    //   je VADA, ne prisnost.
    //
    // ⚠️ TATAZ TRIDA JAKO `W7` (`evidence/fable_wholeturn_audit_20260902.md:139`,
    //   „PRAZDNA EXPANZE ZAHODI CELY ZBYTEK KOLA"), jen o vrstvu niz -- a `W7`
    //   je zmerene: 1 189 nasich kol (2,5 %) bez jedine nasi akce
    //   (`fable_offer_reach_audit_20260827.md:46`).
    //
    // ⇒ Vrat PRVNI NE-`END_TURN` akci; `END_TURN` teprve kdyz nabidka nema nic
    //   jineho. Zadne skore se tu nevymysli: je to zamerne nejslabsi mozna
    //   oprava, ktera jen prestane zahazovat kolo.
    for (const Action& a : actions) {
        if (a.type != ActionType::END_TURN) return a;
    }
    return actions[0];   // v nabidce opravdu nic jineho neni
}

Action learningPolicy(const GameState& state, DiceRollerBase& dice,
                      const ValueFunction& vf, float epsilon) {
    std::vector<Action> actions;
    getAvailableActions(state, actions);

    if (actions.empty()) {
        return Action{ActionType::END_TURN, -1, -1, {-1, -1}};
    }

    // Epsilon-greedy: with probability epsilon, use greedy heuristic
    // (not random — greedy knows how to score, giving positive reward signals)
    float rand = (dice.rollD6() - 1) * 36.0f + (dice.rollD6() - 1) * 6.0f + (dice.rollD6() - 1);
    rand /= 216.0f; // range [0, 1)

    if (rand < epsilon) {
        return greedyPolicy(state, dice);
    }

    // Value function evaluation for all decisions (no heuristic bootstrap)
    TeamSide perspective = state.activeTeam;
    float bestValue = -1e9f;
    int bestIdx = 0;

    for (int i = 0; i < static_cast<int>(actions.size()); i++) {
        GameState clone = state.clone();
        DiceRoller simDice(static_cast<uint32_t>(i * 31 + 17));
        executeAction(clone, actions[i], simDice, nullptr);

        float features[NUM_FEATURES];
        extractFeatures(clone, perspective, features);
        float value = vf.evaluate(features, NUM_FEATURES);

        if (value > bestValue) {
            bestValue = value;
            bestIdx = i;
        }
    }

    return actions[bestIdx];
}

MCTSPolicy::MCTSPolicy(const ValueFunction* vf, MCTSConfig config, uint32_t seed)
    : search_(vf, config, seed) {}

void MCTSPolicy::setLogDecisions(bool log, int topK) {
    logDecisions_ = log;
    topK_ = topK;
}

Action MCTSPolicy::operator()(const GameState& state) {
    Action result = search_.search(state);

    // Log decision if enabled
    if (logDecisions_) {
        const auto& childVisits = search_.lastChildVisits();
        if (!childVisits.empty()) {
            PolicyDecision decision;

            // Extract state features
            extractFeatures(state, state.activeTeam, decision.stateFeatures);
            decision.perspective = state.activeTeam;
            decision.board = captureBoardSnapshot(state);

            // Compute total visits for fraction calculation
            int totalVisits = 0;
            for (auto& cv : childVisits) {
                totalVisits += cv.visits;
            }

            if (totalVisits > 0) {
                // Sort by visits descending (copy to sort)
                std::vector<ChildVisitInfo> sorted = childVisits;
                std::sort(sorted.begin(), sorted.end(),
                          [](const ChildVisitInfo& a, const ChildVisitInfo& b) {
                              return a.visits > b.visits;
                          });

                // Take top-K
                int k = std::min(topK_, static_cast<int>(sorted.size()));
                for (int i = 0; i < k; ++i) {
                    PolicyDecision::ActionVisit av;
                    extractActionFeatures(state, sorted[i].action, av.actionFeatures);
                    av.visitFraction = static_cast<float>(sorted[i].visits) / totalVisits;
                    decision.visits.push_back(av);
                }

                decisions_.push_back(std::move(decision));
            }
        }
    }

    return result;
}

} // namespace bb
