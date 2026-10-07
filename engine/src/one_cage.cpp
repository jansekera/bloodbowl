#include "bb/one_cage.h"
#include <cstdio>
#include <cstdlib>
#include "bb/helpers.h"
#include "bb/pathfinder.h"
#include "bb/turn_planner.h"
#include "bb/turn_plan_record.h"
#include <algorithm>
#include <climits>
#include <cstdlib>
#include <tuple>

namespace bb {

namespace {

int distToEndzone(Position pos, TeamSide side) {
    return std::abs(pos.x - ((side == TeamSide::HOME) ? 25 : 0));
}

int ceilDiv(int a, int b) { return (a + b - 1) / b; }

bool ourBall(const GameState& state) {
    if (!state.ball.isHeld || state.ball.carrierId <= 0) return false;
    const Player& c = state.getPlayer(state.ball.carrierId);
    return c.teamSide == state.activeTeam && c.isOnPitch();
}

bool freeToAct(const Player& p) {
    return p.isOnPitch() && p.state == PlayerState::STANDING && p.canAct() &&
           !p.hasMoved && !p.hasActed;
}

// Kolik stojících soupeřů příští tah dosáhne na pole vedle `sq` (MA + 2 GFI).
int threatsTo(const GameState& state, Position sq, TeamSide mySide) {
    int n = 0;
    state.forEachOnPitch(opponent(mySide), [&](const Player& o) {
        if (o.state != PlayerState::STANDING) return;
        if (o.position.distanceTo(sq) - 1 <= o.stats.movement + 2) ++n;
    });
    return n;
}

} // namespace

Position farthestSafeForward(const GameState& state, const Player& carrier, int budget) {
    const TeamSide side = carrier.teamSide;
    // Menší je lepší: vzdálenost k TD zóně, pak kolik soupeřů na pole dosáhne,
    // pak odklon od středu hřiště.
    auto key = [&](Position sq) {
        return std::make_tuple(distToEndzone(sq, side), threatsTo(state, sq, side),
                               std::abs(sq.y - 7));
    };
    Position best = carrier.position;
    auto bestKey = key(best);
    for (int x = carrier.position.x - budget; x <= carrier.position.x + budget; ++x) {
        for (int y = carrier.position.y - budget; y <= carrier.position.y + budget; ++y) {
            const Position sq{static_cast<int8_t>(x), static_cast<int8_t>(y)};
            if (!sq.isOnPitch() || sq == carrier.position) continue;
            if (sq.y < 1 || sq.y > 13) continue;           // rohy klece musí mít kam stát
            if (distToEndzone(sq, side) > std::get<0>(bestKey)) continue;
            if (state.getPlayerAtPosition(sq)) continue;
            if (countTacklezones(state, sq, side, carrier.id) > 0) continue;
            const auto k = key(sq);
            if (!(k < bestKey)) continue;
            // -1 = nedojde, > 0 = cesta vede přes hod (dodge nebo GFI)
            if (pathFailProb(state, carrier, sq, budget, Position{-1, -1}) != 0.0) continue;
            best = sq;
            bestKey = k;
        }
    }
    return best;
}

ReleaseDecision decideRelease(const GameState& state, const Player& carrier,
                              const CageAdvancePlanner& planner) {
    ReleaseDecision r;
    const TeamSide side = carrier.teamSide;
    r.dist = distToEndzone(carrier.position, side);
    r.turnsLeft = std::clamp(9 - state.getTeamState(side).turnNumber, 0, 8);
    const int ma = std::max(1, static_cast<int>(carrier.stats.movement));
    const int reach = ma + 2;   // odsud tah na TD dohraje MCTS (SCORE_BALL)

    for (int s = carrier.movementRemaining; s >= 1; --s) {
        if (planner.tryAssign(state, carrier, s, {}).feasible) { r.cageStep = s; break; }
    }
    const int penalty = std::min(2, (corridorResistance(state, carrier, side) + 1) / 2);
    r.futurePace = std::max(0, r.cageStep - penalty);

    // Klec: tento tah cageStep, pak futurePace, poslední tah sólo na TD.
    if (r.cageStep > 0) {
        const int rest = r.dist - r.cageStep - reach;
        int need = 1;
        if (rest > 0) need = (r.futurePace > 0) ? ceilDiv(rest, r.futurePace) + 1 : INT_MAX;
        r.cageMakesIt = need <= r.turnsLeft - 1;
    }
    // Sólo od tohoto tahu: každý tah MA, poslední tah na TD.
    const int soloRest = r.dist - reach;
    const int soloNeed = (soloRest > 0 ? ceilDiv(soloRest, ma) : 0) + 1;
    r.soloMakesIt = soloNeed <= r.turnsLeft;

    r.release = !r.cageMakesIt && r.soloMakesIt;
    return r;
}

// Roh klece s Frenzy: po ráně musí následovat, takže blokem roh opustí.
static bool frenzyCorner(const Player& p, const Player& carrier) {
    return p.hasSkill(SkillName::Frenzy) && std::abs(p.position.x - carrier.position.x) == 1 &&
           std::abs(p.position.y - carrier.position.y) == 1;
}

// P154 (b) (uživatel 07.10.2026: „pokud je ta situace na konci poločasu — nosič musí vyběhnout,
// aby stihl TD“). Sólo od PŘÍŠTÍHO tahu potřebuje ceil((dist − (MA+2)) / MA) + 1 tahů; nevejde-li
// se to do tahů, které po tomto zbývají, čekat nejde.
static bool waitingCostsTheTouchdown(const GameState& state, const Player& carrier) {
    const int dist = distToEndzone(carrier.position, carrier.teamSide);
    const int ma = std::max(1, static_cast<int>(carrier.stats.movement));
    const int turnsAfterThis = std::clamp(8 - state.getTeamState(carrier.teamSide).turnNumber, 0, 8);
    const int rest = dist - (ma + 2);
    const int soloNeedIfWaiting = (rest > 0 ? (rest + ma - 1) / ma : 0) + 1;
    return soloNeedIfWaiting > turnsAfterThis;
}

CageController::CageController(const ValueFunction* vf, MCTSConfig config, uint32_t seed)
    : planner_(vf, config, seed), config_(config) {}

bool CageController::released(const GameState& state, const Player& carrier) const {
    const int score = state.homeTeam.score + state.awayTeam.score;
    return carrier.id == releasedCarrier_ && state.half == releasedHalf_ &&
           score == releasedScore_;
}

bool CageController::stillValid(const GameState& state, const Macro& m) const {
    if (m.type == MacroType::PICKUP) return stagedMacroStillValid(state, m, false);
    if (!ourBall(state)) return false;    // míč pryč ⇒ zbytek plánu nemá smysl
    if (m.type == MacroType::REPOSITION) return stagedMacroStillValid(state, m, true);
    if (m.type == MacroType::BLOCK) {
        const Player& a = state.getPlayer(m.playerId);
        const Player& d = state.getPlayer(m.targetId);
        return freeToAct(a) && d.isOnPitch() && d.state == PlayerState::STANDING &&
               a.position.distanceTo(d.position) == 1;
    }
    if (m.type == MacroType::BLITZ) {
        const Player& a = state.getPlayer(m.playerId);
        const Player& d = state.getPlayer(m.targetId);
        return freeToAct(a) && d.isOnPitch() && d.state == PlayerState::STANDING &&
               !state.getTeamState(a.teamSide).blitzUsedThisTurn;
    }
    return false;
}

void CageController::planStart(const GameState& state) {
    stage_ = Stage::DONE;
    const TurnGoal goal = classifyTurnGoal(state);

    if (goal == TurnGoal::PICKUP_BALL) {
        // Fáze 1: zvedá nejlepší z kandidátů, které nabízí i MCTS — napřed
        // ten, kdo k míči dojde bez hodu, pak Sure Hands, pak obratnost.
        std::vector<Macro> macros;
        getAvailableMacros(state, macros, config_.dauntlessInOffer);
        const Macro* best = nullptr;
        double bestFail = 2.0;
        auto handler = [&](const Player& p) {
            return (p.hasSkill(SkillName::SureHands) ? 100 : 0) + p.stats.agility;
        };
        for (const Macro& m : macros) {
            if (m.type != MacroType::PICKUP) continue;
            const Player& p = state.getPlayer(m.playerId);
            double fail = pathFailProb(state, p, m.targetPos,
                                       p.movementRemaining + maxGfiSquares(p), Position{-1, -1});
            if (fail < 0.0) fail = 1.0;
            // P154/K4 (06.10.2026): do rizika patří i HOD NA ZVEDNUTÍ, ne jen cesta k míči.
            // Dosud vyhrál ten, kdo k míči došel bez hodu, i když pak zvedal na 4+ a horší
            // (40 poločasů na main: 9 ze 42 zvednutí řadičem na 4+ a horší, tah se zvednutím
            // skončil turnoverem v 65 %). Sure Hands = přehoz zvednutí (bez něj se s přehozem
            // nepočítá — týmový reroll je zdroj celého tahu).
            {
                const int target = calculatePickupTargetAt(state, p, m.targetPos);
                double pick = std::clamp((target - 1) / 6.0, 0.0, 1.0);
                if (p.hasSkill(SkillName::SureHands)) pick *= pick;
                fail = 1.0 - (1.0 - fail) * (1.0 - pick);
            }
            if (!best || fail < bestFail - 1e-9 ||
                (std::abs(fail - bestFail) <= 1e-9 &&
                 handler(p) > handler(state.getPlayer(best->playerId)))) {
                best = &m;
                bestFail = fail;
            }
        }
        if (!best) return;
        Macro pick = *best;
        pick.cageManaged = true;   // expandPickup: po zvednutí co nejdál dopředu
        queue_ = {pick};
        phase_ = CagePhase::PICKUP;
        stage_ = Stage::AFTER_PICKUP;
        return;
    }

    // P150 (06.10.2026): stopa rozhodnutí řadiče pod BB_CAGE_DEBUG — proč tah klece není / je.
    const bool dbg = std::getenv("BB_CAGE_DEBUG") != nullptr;
    // P154/K3 (06.10.2026): tah označený SCORE_BALL (nosič do MA + 2 od TD) řadič dosud celý
    // přenechal hledání — a to ve středních kolech neskórovalo a tah ukončilo (partie 02.10.,
    // tahy 9 a 28: TD 0/20, klec stála). Teď: když nosič do zóny NEDOJDE BEZ HODU a není to
    // poslední kolo poločasu, jde klec dál jako v běžném postupu. Kdo dojde bez hodu, nebo
    // hraje poslední kolo, zůstává hledání (skórování se nebrání).
    scoringRangeCage_ = false;
    if (goal == TurnGoal::SCORE_BALL && state.ball.isHeld && state.ball.carrierId > 0) {
        const Player& c = state.getPlayer(state.ball.carrierId);
        const bool lastTurn = state.getTeamState(c.teamSide).turnNumber >= 8;
        bool walksIn = false;
        const int ezX = (c.teamSide == TeamSide::HOME) ? 25 : 0;
        for (int y = 0; y < 15 && !walksIn; ++y) {
            const Position sq{static_cast<int8_t>(ezX), static_cast<int8_t>(y)};
            if (state.getPlayerAtPosition(sq)) continue;
            walksIn = pathFailProb(state, c, sq, c.movementRemaining, Position{-1, -1}) == 0.0;
        }
        scoringRangeCage_ = !lastTurn && !walksIn && c.teamSide == state.activeTeam;
        if (dbg) std::fprintf(stderr, "[cage ctl] SCORE_BALL: dojde bez hodu %d, poslední kolo %d => %s\n",
                              walksIn, lastTurn, scoringRangeCage_ ? "klec jde dál" : "rozhoduje hledání");
    }
    if (goal != TurnGoal::ADVANCE_BALL && !scoringRangeCage_) {
        if (dbg) std::fprintf(stderr, "[cage ctl] bez plánu: cíl tahu %d není ADVANCE_BALL\n", static_cast<int>(goal));
        return;
    }
    const Player& carrier = state.getPlayer(state.ball.carrierId);
    if (!freeToAct(carrier)) {
        if (dbg) std::fprintf(stderr, "[cage ctl] bez plánu: nosič %d už hrál nebo nestojí\n", carrier.id);
        return;
    }
    if (dbg) {
        const ReleaseDecision r = decideRelease(state, carrier, planner_);
        std::fprintf(stderr, "[cage ctl] nosič %d (%d,%d): do TD %d, tahů %d, krok klece %d, další tempo %d, "
                     "klec doběhne %d, sólo doběhne %d, už vypuštěn %d => %s\n",
                     carrier.id, carrier.position.x, carrier.position.y, r.dist, r.turnsLeft, r.cageStep,
                     r.futurePace, r.cageMakesIt, r.soloMakesIt, released(state, carrier),
                     (released(state, carrier) || r.release) ? "VÝBĚH (fáze 3)" : "KLEC (fáze 2)");
    }

    if (!released(state, carrier) && !decideRelease(state, carrier, planner_).release) {
        // Fáze 2. Soupeř na poli rohu ⇒ napřed ho shodit (uživatel 02.10.).
        phase_ = CagePhase::CAGE;
        std::vector<Macro> macros;
        getAvailableMacros(state, macros, config_.dauntlessInOffer);
        std::vector<int> usedAttackers;
        for (const Position& d : carrier.position.getAdjacent()) {
            if (!d.isOnPitch() || std::abs(d.x - carrier.position.x) != 1 ||
                std::abs(d.y - carrier.position.y) != 1) continue;
            const Player* opp = state.getPlayerAtPosition(d);
            if (!opp || opp->teamSide == carrier.teamSide ||
                opp->state != PlayerState::STANDING) continue;
            const Macro* pickBlock = nullptr;
            for (const Macro& m : macros) {
                if (m.type != MacroType::BLOCK || m.targetId != opp->id) continue;
                if (m.playerId == carrier.id) continue;   // nosič nebojuje
                if (std::find(usedAttackers.begin(), usedAttackers.end(), m.playerId) !=
                    usedAttackers.end()) continue;
                const Player& a = state.getPlayer(m.playerId);
                if (!pickBlock) { pickBlock = &m; continue; }
                const Player& b = state.getPlayer(pickBlock->playerId);
                if (a.hasSkill(SkillName::Block) != b.hasSkill(SkillName::Block)
                        ? a.hasSkill(SkillName::Block)
                        : a.stats.strength > b.stats.strength) {
                    pickBlock = &m;
                }
            }
            if (pickBlock) {
                queue_.push_back(*pickBlock);
                usedAttackers.push_back(pickBlock->playerId);
            }
        }
        // ⭐ P154 (a) (uživatel 07.10.2026: „uvolnit rohového blokem nebo blitzem — nebo jej
        //   nahradit volným“): spoluhráč, kterého drží v zóně jediný soupeř, na roh bez úhybu
        //   nedojde (16 % prázdných rohů, 40 poločasů na main). Soupeře mu z cesty shodí JINÝ
        //   náš hráč — jen bezpečnou ranou: 2+ kostky, které vybíráme my, a útočník má Block
        //   (pád útočníka jen na dvě lebky). Náhradu volným hráčem dělá výběr rohů v plánovači.
        {
            struct Freeing { const Macro* m; int freed; };
            std::vector<Freeing> cands;
            for (const Macro& m : macros) {
                if (m.type != MacroType::BLOCK || m.playerId == carrier.id) continue;
                const Player& a = state.getPlayer(m.playerId);
                const Player& d = state.getPlayer(m.targetId);
                if (!freeToAct(a) || !a.hasSkill(SkillName::Block)) continue;
                if (frenzyCorner(a, carrier)) continue;
                if (blockDiceCount(state, a, d) < 2) continue;
                int freed = 0;
                state.forEachOnPitch(carrier.teamSide, [&](const Player& t) {
                    if (t.id == a.id || t.id == carrier.id || !freeToAct(t)) return;
                    if (t.position.distanceTo(d.position) != 1) return;
                    // drží ho jen tenhle jeden soupeř?
                    if (countTacklezones(state, t.position, t.teamSide, t.id) == 1) ++freed;
                });
                if (freed > 0) cands.push_back({&m, freed});
            }
            std::stable_sort(cands.begin(), cands.end(),
                             [](const Freeing& x, const Freeing& y) { return x.freed > y.freed; });
            std::vector<int> usedTargets;
            for (const Macro& q : queue_) usedTargets.push_back(q.targetId);
            for (const Freeing& f : cands) {
                if (std::find(usedAttackers.begin(), usedAttackers.end(), f.m->playerId) != usedAttackers.end()) continue;
                if (std::find(usedTargets.begin(), usedTargets.end(), f.m->targetId) != usedTargets.end()) continue;
                queue_.push_back(*f.m);
                usedAttackers.push_back(f.m->playerId);
                usedTargets.push_back(f.m->targetId);
            }
        }
        // ⭐ P154 (a) pokračování (uživatel 07.10.2026: „využij i blitz a block pro uvolnění
        //   klece“, „nezapomeň na příchod pro asistenci u blitz a block“). Soupeř, který stojí
        //   na poli rohu nebo jako jediný drží našeho volného hráče, a bezpečná rána na něj
        //   zatím není: (A) volný spoluhráč PŘIJDE PRO ASISTENCI (bez hodu, na pole vedle
        //   soupeře), takže hráč s Block, který u soupeře už stojí, hází 2+ kostky, které
        //   vybíráme my; (B) jinak jeden BLITZ za tah: hráč s Block doběhne bez úhybu a bez GFI
        //   a z pole, kam dojde, má 2+ kostky.
        {
            const TeamSide side = carrier.teamSide;
            auto used = [&](int id) {
                return std::find(usedAttackers.begin(), usedAttackers.end(), id) != usedAttackers.end();
            };
            auto targeted = [&](int id) {
                for (const Macro& q : queue_) if (q.targetId == id) return true;
                return false;
            };
            std::vector<int> blockers;
            auto addBlocker = [&](int id) {
                if (std::find(blockers.begin(), blockers.end(), id) == blockers.end()) blockers.push_back(id);
            };
            for (int cx : {-1, 1}) for (int cy : {-1, 1}) {             // soupeř na poli rohu
                const Player* o = state.getPlayerAtPosition({static_cast<int8_t>(carrier.position.x + cx),
                                                             static_cast<int8_t>(carrier.position.y + cy)});
                if (o && o->teamSide != side && o->state == PlayerState::STANDING) addBlocker(o->id);
            }
            state.forEachOnPitch(side, [&](const Player& t) {           // jediný držitel našeho volného hráče
                if (t.id == carrier.id || !freeToAct(t)) return;
                if (countTacklezones(state, t.position, side, t.id) != 1) return;
                for (const Position& sq : t.position.getAdjacent()) {
                    if (!sq.isOnPitch()) continue;
                    const Player* o = state.getPlayerAtPosition(sq);
                    if (o && o->teamSide != side && o->state == PlayerState::STANDING) addBlocker(o->id);
                }
            });
            bool blitzPlanned = state.getTeamState(side).blitzUsedThisTurn;
            for (int eid : blockers) {
                if (targeted(eid)) continue;
                const Player& e = state.getPlayer(eid);
                bool done = false;
                // (A) příchod pro asistenci
                state.forEachOnPitch(side, [&](const Player& a) {
                    if (done || a.id == carrier.id || !freeToAct(a) || used(a.id)) return;
                    if (!a.hasSkill(SkillName::Block) || a.position.distanceTo(e.position) != 1) return;
                    if (frenzyCorner(a, carrier)) return;
                    state.forEachOnPitch(side, [&](const Player& m) {
                        if (done || m.id == a.id || m.id == carrier.id || !freeToAct(m) || used(m.id)) return;
                        if (countTacklezones(state, m.position, side, m.id) > 0) return;   // pomocník musí být volný
                        if (std::abs(m.position.x - carrier.position.x) == 1 &&
                            std::abs(m.position.y - carrier.position.y) == 1) return;      // roh zůstává rohem
                        for (const Position& sq : e.position.getAdjacent()) {
                            if (done) break;
                            if (!sq.isOnPitch() || state.getPlayerAtPosition(sq)) continue;
                            if (sq.distanceTo(m.position) > m.movementRemaining) continue;
                            if (pathFailProb(state, m, sq, m.movementRemaining, Position{-1, -1}) != 0.0) continue;
                            GameState trial = state.clone();
                            trial.getPlayer(m.id).position = sq;
                            if (blockDiceCount(trial, trial.getPlayer(a.id), trial.getPlayer(eid)) < 2) continue;
                            Macro mv{MacroType::REPOSITION, m.id, -1, sq};
                            mv.cageManaged = true;
                            queue_.push_back(mv);
                            queue_.push_back(Macro{MacroType::BLOCK, a.id, eid, {-1, -1}});
                            usedAttackers.push_back(m.id);
                            usedAttackers.push_back(a.id);
                            done = true;
                        }
                    });
                });
                if (done || blitzPlanned) continue;
                // (B) blitz
                state.forEachOnPitch(side, [&](const Player& b) {
                    if (done || b.id == carrier.id || !freeToAct(b) || used(b.id)) return;
                    if (!b.hasSkill(SkillName::Block) || b.position.distanceTo(e.position) <= 1) return;
                    if (std::abs(b.position.x - carrier.position.x) == 1 &&
                        std::abs(b.position.y - carrier.position.y) == 1) return;          // roh neblitzuje
                    Position landing{-1, -1};
                    int steps = 0;
                    if (!blitzLandingDiceFree(state, b, e.position, landing, steps)) return;
                    if (steps > static_cast<int>(b.movementRemaining) - 1) return;          // bez GFI (rána stojí pole)
                    GameState trial = state.clone();
                    trial.getPlayer(b.id).position = landing;
                    if (blockDiceCount(trial, trial.getPlayer(b.id), trial.getPlayer(eid)) < 2) return;
                    queue_.push_back(Macro{MacroType::BLITZ, b.id, eid, {-1, -1}});
                    usedAttackers.push_back(b.id);
                    blitzPlanned = true;
                    done = true;
                });
            }
        }
        stage_ = Stage::AFTER_BLOCKS;
        return;
    }

    // Fáze 3: nosič co nejdál sám; markeři až podle místa, kam doopravdy došel.
    phase_ = CagePhase::RELEASE;
    releasedCarrier_ = carrier.id;
    releasedHalf_ = state.half;
    releasedScore_ = state.homeTeam.score + state.awayTeam.score;
    const Position dest = farthestSafeForward(state, carrier, carrier.movementRemaining);
    if (dbg) std::fprintf(stderr, "[cage ctl] výběh: nosič (%d,%d) -> (%d,%d)\n", carrier.position.x,
                          carrier.position.y, dest.x, dest.y);
    if (dest != carrier.position) {
        Macro run{MacroType::REPOSITION, carrier.id, -1, dest};
        run.cageManaged = true;
        queue_.push_back(run);
    }
    stage_ = Stage::AFTER_RUN;
}

// P154 (b): smí hledání zahrát tohle makro? Ne, když by rozebralo klec: (1) odvedlo roh klece,
// (2) pohnulo nosičem, kterého řadič v tomto tahu drží
// v kleci (postup nevyšel a čekáním o TD nepřijde) — kromě maker, která skórují nebo míč
// předávají, a kromě nosiče, který sám stojí v zóně soupeře (ústup z kontaktu má přednost:
// „blok na nosiče se nesmí stávat vůbec").
bool CageController::forbidsCarrierMove(const GameState& state, const Macro& m) const {
    if (!ourBall(state)) return false;
    if (state.activeTeam != team_ || state.getTeamState(team_).turnNumber != turn_ || state.half != half_) return false;
    const Player& carrier = state.getPlayer(state.ball.carrierId);

    // Roh klece zůstává rohem: hráče, který po tahu řadiče stojí na úhlopříčce vedle nosiče,
    // hledání nepřesouvá jinam. Změřeno 07.10.: tam, kde nosič po kleci stál, klesly rohy
    // z 3,30 na 2,96 — 18× roh odešel přesunem. (Blok z místa roh smí; blitz hledání hráče
    // nejmenuje, ten se tu ohlídat nedá.)
    if (phase_ == CagePhase::CAGE && m.type == MacroType::REPOSITION && m.playerId > 0 &&
        m.playerId != carrier.id) {
        const Player& p = state.getPlayer(m.playerId);
        const bool isCorner = p.teamSide == carrier.teamSide && p.state == PlayerState::STANDING &&
                              std::abs(p.position.x - carrier.position.x) == 1 &&
                              std::abs(p.position.y - carrier.position.y) == 1;
        if (isCorner && m.targetPos != p.position) return true;
    }

    // Roh s Frenzy po ráně následovat MUSÍ (ř. 8134-8145) ⇒ blokem z rohu odejde. Ukázka 07.10.
    // (hra 7, kolo 2): oba Troll Slayeři stáli na rozích, oba zablokovali, klec spadla ze čtyř
    // rohů na dva a nosič pak vyběhl sám. Roh s Frenzy proto neblokuje.
    if (phase_ == CagePhase::CAGE && m.type == MacroType::BLOCK && m.playerId > 0 && m.playerId != carrier.id) {
        const Player& p = state.getPlayer(m.playerId);
        if (p.teamSide == carrier.teamSide && p.hasSkill(SkillName::Frenzy) &&
            std::abs(p.position.x - carrier.position.x) == 1 &&
            std::abs(p.position.y - carrier.position.y) == 1) {
            return true;
        }
    }

    const bool movesCarrier = m.type == MacroType::ADVANCE ||
                              (m.type == MacroType::REPOSITION && m.playerId == carrier.id);
    if (!movesCarrier) return false;
    // Rozhoduje se ze STAVU DESKY ve chvíli volby, ne z toho, jak dopadl plán: ať už řadič plán
    // nedokončil, nebo ho vůbec nedělal — stojí-li nosič v kleci (aspoň tři rohy), hledání ho
    // z ní samotného nevyvede. (07.10.: příznak nastavený jen po nevydařeném postupu pustil
    // nosiče ven ve 34 ze 188 tahů — plán „vyšel“, ale nedohrál se, nebo se neplánoval.)
    const char* why = nullptr;
    if (phase_ == CagePhase::RELEASE || released(state, carrier)) why = "nosič je vypuštěn (fáze 3)";
    int corners = 0;
    for (int cx : {-1, 1}) for (int cy : {-1, 1}) {
        const Player* q = state.getPlayerAtPosition({static_cast<int8_t>(carrier.position.x + cx),
                                                     static_cast<int8_t>(carrier.position.y + cy)});
        if (q && q->teamSide == carrier.teamSide && q->state == PlayerState::STANDING) ++corners;
    }
    if (!why && corners < 3) why = "klec nemá ani tři rohy";
    if (!why && waitingCostsTheTouchdown(state, carrier)) why = "čekáním by nestihl TD";
    if (!why && countTacklezones(state, carrier.position, carrier.teamSide) > 0) why = "nosič stojí v zóně soupeře";
    if (why && std::getenv("BB_CAGE_DEBUG")) {
        std::fprintf(stderr, "[cage ctl] hledání smí pohnout nosičem: %s\n", why);
    }
    return why == nullptr;
}

// P154/K2+K3 (06.10.2026): NEŽ HLEDÁNÍ UKONČÍ TAH, DOTÁHNOUT KLEC. Volá MacroMCTSPolicy,
// když hledání zvolí END_TURN a míč držíme. Dosud po odmítnutém plánu klece (nebo v tahu
// označeném SCORE_BALL, kde řadič ustoupil a hledání neskórovalo) nebyl žádný náhradní
// postup: nosič popošel sám a 8–9 hráčů zůstalo stát (partie 02.10., tahy 5, 7, 9, 28).
// Nosič ještě nehrál ⇒ postup klece (i v dosahu TD, když se neskórovalo), jinak / když
// postup nevyjde ⇒ aspoň dostavět rohy kolem nosiče tam, kde stojí. Vše bez hodu, jednou za tah.
bool CageController::beforeEndTurn(const GameState& state, Macro& out) {
    if (state.phase != GamePhase::PLAY || !ourBall(state)) return false;
    const int turn = state.getTeamState(state.activeTeam).turnNumber;
    if (endTurnTeam_ == state.activeTeam && endTurnTurn_ == turn && endTurnHalf_ == state.half) {
        return false;                       // v tomto tahu už jednou zkoušeno
    }
    endTurnTeam_ = state.activeTeam;
    endTurnTurn_ = turn;
    endTurnHalf_ = state.half;

    const Player& carrier = state.getPlayer(state.ball.carrierId);
    CageAdvancePlan plan;
    if (freeToAct(carrier) && !released(state, carrier)) {
        plan = planner_.build(state, {}, /*evenInScoringRange=*/true);
    } else {
        plan = planner_.buildFillOnly(state, {});
    }
    if (std::getenv("BB_CAGE_DEBUG")) {
        std::fprintf(stderr, "[cage ctl] před koncem tahu: verdikt %d, platný %d, krok %d, maker %zu\n",
                     static_cast<int>(plan.verdict), plan.valid, plan.step, plan.macros.size());
    }
    if (!plan.valid || plan.macros.empty()) return false;
    // next() pozná nový tah podle těchto tří polí a frontu by zahodil — nesmí záležet na tom,
    // jestli se v tomto tahu už volal
    team_ = state.activeTeam;
    turn_ = turn;
    half_ = state.half;
    queue_ = std::move(plan.macros);
    idx_ = 0;
    stage_ = Stage::DONE;
    phase_ = CagePhase::CAGE;
    ++adopted_;
    out = queue_[idx_++];
    return true;
}

void CageController::planAfterPickup(const GameState& state) {
    stage_ = Stage::AFTER_ADVANCE;   // P154 (c): i po zvednutí jdou volní zaostalci dopředu
    if (!ourBall(state)) return;
    CageAdvancePlan plan = planner_.buildFillOnly(state, {});
    if (plan.valid) queue_ = std::move(plan.macros);
}

void CageController::planAdvance(const GameState& state) {
    stage_ = Stage::AFTER_ADVANCE;   // pak zaostalci (P154 c)
    if (!ourBall(state)) return;
    const Player& carrier = state.getPlayer(state.ball.carrierId);
    if (!freeToAct(carrier)) return;
    CageAdvancePlan plan = planner_.build(state, {}, scoringRangeCage_);
    if (std::getenv("BB_CAGE_DEBUG")) {
        std::fprintf(stderr, "[cage ctl] plán postupu: verdikt %d, platný %d, krok %d, rohy stojí %d, po tahu %d, maker %zu\n",
                     static_cast<int>(plan.verdict), plan.valid, plan.step, plan.builtCorners,
                     plan.filledCorners, plan.macros.size());
    }
    // ⭐ P154 (b) (uživatel 07.10.2026: „nosič dál jen s klecí“; obklíčená klec: „zůstat stát
    //   a uvolňovat ranami“; „pokud je ta situace na konci poločasu — nosič musí vyběhnout,
    //   aby stihl TD“). Postup klece nevyšel ⇒ nosič tento tah z klece nevybíhá — LEDAŽE by
    //   čekáním přišel o TD: sólo od příštího tahu potřebuje ceil((dist − (MA+2)) / MA) + 1
    //   tahů; nevejde-li se to do zbývajících, běží už teď.
    if (plan.valid) queue_ = std::move(plan.macros);
}

// ⭐ P154 (c) (uživatel 07.10.2026: „držet zbytek týmu u klece — respektive PŘED klecí — ať se
//   nosič může přesunout dopředu do nové klece — pak co nejdříve dořešit pohyb zaostalců co
//   nejvíce dopředu“). Po tahu klece: kdo stojí volně a zůstal víc než pole za nosičem, jde bez
//   hodu dopředu — nejvýš tři sloupce před nosiče (z hráčů před nosičem se příští tah staví nová
//   klec), ne do kontaktu, a co nejblíž spoluhráči („vedle sebe nebo o jedno“ = těžší blok).
void CageController::planLaggards(const GameState& state) {
    stage_ = Stage::DONE;
    if (!ourBall(state)) return;
    const Player& carrier = state.getPlayer(state.ball.carrierId);
    const TeamSide side = carrier.teamSide;
    const int dx = (side == TeamSide::HOME) ? 1 : -1;
    const Position cp = carrier.position;
    std::vector<Position> taken;
    state.forEachOnPitch(side, [&](const Player& p) {
        if (p.id == carrier.id || !freeToAct(p)) return;
        if ((cp.x - p.position.x) * dx <= 1) return;                       // není zaostalec
        if (countTacklezones(state, p.position, side, p.id) > 0) return;   // vázaný: řeší rány
        const int budget = p.movementRemaining;
        Position best = p.position;
        long bestKey = LONG_MAX;
        for (int x = 0; x < 26; ++x) {
            if ((x - p.position.x) * dx <= 0) continue;                    // jen dopředu
            if ((x - cp.x) * dx > 3) continue;                             // nejvýš 3 sloupce před nosiče
            for (int y = 0; y < 15; ++y) {
                const Position sq{static_cast<int8_t>(x), static_cast<int8_t>(y)};
                if (sq.distanceTo(p.position) > budget) continue;
                if (state.getPlayerAtPosition(sq)) continue;
                if (std::find(taken.begin(), taken.end(), sq) != taken.end()) continue;
                if (countTacklezones(state, sq, side, p.id) > 0) continue; // ne do kontaktu
                if (pathFailProb(state, p, sq, budget, Position{-1, -1}) != 0.0) continue;
                int nearMate = 99;
                state.forEachOnPitch(side, [&](const Player& m) {
                    if (m.id == p.id || m.state != PlayerState::STANDING) return;
                    nearMate = std::min(nearMate, static_cast<int>(m.position.distanceTo(sq)));
                });
                for (const Position& t : taken) nearMate = std::min(nearMate, static_cast<int>(t.distanceTo(sq)));
                const int ahead = (x - cp.x) * dx;                          // co nejvíc dopředu
                const long key = -1000L * ahead + 100L * std::max(0, nearMate - 2) +
                                 10L * sq.distanceTo(cp) + std::abs(y - 7);
                if (key < bestKey) { bestKey = key; best = sq; }
            }
        }
        if (best == p.position) return;
        Macro m{MacroType::REPOSITION, p.id, -1, best};
        m.cageManaged = true;                                              // bez GFI navrch
        queue_.push_back(m);
        taken.push_back(best);
    });
}



void CageController::planMarkers(const GameState& state) {
    stage_ = Stage::DONE;
    if (!ourBall(state)) return;
    const Player& carrier = state.getPlayer(state.ball.carrierId);
    const TeamSide side = carrier.teamSide;
    const Position cp = carrier.position;

    // Soupeři, kteří příští tah dojdou k nosiči a ještě nikým markovaní nejsou.
    std::vector<const Player*> threats;
    state.forEachOnPitch(opponent(side), [&](const Player& o) {
        if (o.state != PlayerState::STANDING) return;
        if (countTacklezones(state, o.position, o.teamSide) > 0) return;  // už markovaný
        if (o.position.distanceTo(cp) - 1 > o.stats.movement + 2) return;
        threats.push_back(&state.getPlayer(o.id));
    });
    std::sort(threats.begin(), threats.end(), [&](const Player* a, const Player* b) {
        return a->position.distanceTo(cp) < b->position.distanceTo(cp);
    });

    std::vector<int> usedMarkers;
    std::vector<Position> usedSquares;
    for (const Player* o : threats) {
        int bestId = -1;
        Position bestSq{-1, -1};
        int bestSteps = INT_MAX;
        state.forEachOnPitch(side, [&](const Player& m) {
            if (m.id == carrier.id || !freeToAct(m)) return;
            if (std::find(usedMarkers.begin(), usedMarkers.end(), m.id) != usedMarkers.end()) return;
            for (const Position& sq : o->position.getAdjacent()) {
                if (!sq.isOnPitch() || state.getPlayerAtPosition(sq)) continue;
                if (std::find(usedSquares.begin(), usedSquares.end(), sq) != usedSquares.end()) continue;
                if (pathFailProb(state, m, sq, m.movementRemaining, Position{-1, -1}) != 0.0) continue;
                const int steps = pathStepsToward(state, m, sq, m.movementRemaining, Position{-1, -1});
                if (steps >= 0 && steps < bestSteps) {
                    bestSteps = steps;
                    bestId = m.id;
                    bestSq = sq;
                }
            }
        });
        if (bestId < 0) continue;
        Macro mark{MacroType::REPOSITION, bestId, -1, bestSq};
        mark.cageManaged = true;   // markování je bez hodu, žádné GFI navrch
        queue_.push_back(mark);
        usedMarkers.push_back(bestId);
        usedSquares.push_back(bestSq);
    }
}

bool CageController::next(const GameState& state, Macro& out) {
    if (state.phase != GamePhase::PLAY) return false;
    const int turn = state.getTeamState(state.activeTeam).turnNumber;
    if (team_ != state.activeTeam || turn_ != turn || half_ != state.half) {
        team_ = state.activeTeam;
        turn_ = turn;
        half_ = state.half;
        stage_ = Stage::START;
        phase_ = CagePhase::NONE;
        queue_.clear();
        idx_ = 0;
    }

    for (int guard = 0; guard < 6; ++guard) {
        if (idx_ < queue_.size()) {
            const Macro& m = queue_[idx_];
            if (!stillValid(state, m)) {   // plán se rozešel s deskou ⇒ zbytek tahu MCTS
                queue_.clear();
                idx_ = 0;
                stage_ = Stage::DONE;
                return false;
            }
            out = m;
            ++idx_;
            return true;
        }
        queue_.clear();
        idx_ = 0;
        switch (stage_) {
            case Stage::START:        planStart(state); break;
            case Stage::AFTER_PICKUP: planAfterPickup(state); break;
            case Stage::AFTER_BLOCKS: planAdvance(state); break;
            case Stage::AFTER_ADVANCE: planLaggards(state); break;
            case Stage::AFTER_RUN:    planMarkers(state); break;
            case Stage::DONE:         return false;
        }
        if (!queue_.empty()) {
            ++adopted_;
            TurnPlanRecord& rec = currentTurnPlanRecord();
            rec.written = true;
            rec.adopted = true;
        }
    }
    return false;
}

} // namespace bb
