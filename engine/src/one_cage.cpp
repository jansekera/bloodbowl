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

// ⭐ P211b (uživatel 15.10.2026: „Rat Ogre nemá být roh klece — a jako zaostalý se zkusí pohnout až
//   poté, co se provede blitz — jen pokud blitz není spuštěn, může pro pohyb využít akci blitz“;
//   „zaostalci nemusí být v plánu rovnou klece — můžou mít vlastní blok později po kleci“). Přesuny
//   řadiče (připravený hráč, zaostalci, markeři) jsou „bez hodu“ — u Wild Animal ale každá akce začíná
//   hodem (ř. 8666-8669: 1-3 akce propadá, +2 jen u Block/Blitz). Změřeno (evidence/p156b_20261015):
//   41× ho řadič přesunul, když byl blitz ještě volný, a 19× propadl. Řadič ho během tahu nebere
//   (může dostat vlastní blok s +2 v hledání); dojde až před koncem tahu (`wildAnimalLaggards`).
bool cageMayMove(const Player& p) {
    return freeToAct(p) && !p.hasSkill(SkillName::WildAnimal);
}

// Kolik soupeřů příští tah dosáhne ranou na pole `sq` (dosah jako v blitzThreat).
int threatsTo(const GameState& state, Position sq, TeamSide mySide) {
    int n = 0;
    state.forEachOnPitch(opponent(mySide), [&](const Player& o) {
        const int r = blitzReachOf(o);
        if (r >= 0 && o.position.distanceTo(sq) - 1 <= r) ++n;
    });
    return n;
}

// ⭐ P216 (uživatel 18.10.2026: „stejně jako řeší kdo jej blitzne a zkouší utéct z dosahu — tak zkouší
//   utéct i z možnosti obklíčit — rychlejší bude lépe utíkat z dosahu všech, agilnější bude možno přijmout
//   s Nerves of Steel v obklíčení a odskočit do prázdného pole a utéct“). Nejhorší odpověď soupeře: vedle
//   připraveného na poli `sq` se postaví každý, kdo tam příští tah dojde (`threatsTo`). Šance, že pak míč
//   z předávky chytí (zóny podle Nerves of Steel) a — je-li obstoupen — uhne do prázdného pole.
double readyMateChance(const GameState& state, const Player& p, Position sq, TeamSide side) {
    const int k = threatsTo(state, sq, side);
    GameState proj = state.clone();
    proj.getPlayer(p.id).position = sq;
    int ct = calculateCatchTarget(proj, proj.getPlayer(p.id), 1);
    if (!p.hasSkill(SkillName::NervesOfSteel)) ct = std::min(6, ct + k);
    double pc = (7 - ct) / 6.0;
    if (p.hasSkill(SkillName::Catch)) pc = 1.0 - (1.0 - pc) * (1.0 - pc);
    if (k == 0) return pc;
    const int dt = std::clamp(6 - static_cast<int>(p.stats.agility), 2, 6);   // úhyb do pole bez zón
    double pd = (7 - dt) / 6.0;
    if (p.hasSkill(SkillName::Dodge)) pd = 1.0 - (1.0 - pd) * (1.0 - pd);
    return pc * pd;
}

} // namespace

// P176: kolik GFI nosič na cestu k `dest` potřebuje (0, když dojde svým pohybem).
static int carrierGfiFor(const GameState& state, const Player& carrier, Position dest) {
    const int mr = static_cast<int>(carrier.movementRemaining);
    const int steps = pathStepsToward(state, carrier, dest, mr + maxGfiSquares(carrier), Position{-1, -1});
    return std::clamp(steps - mr, 0, 2);
}

double looseBallLossRisk(const GameState& state, TeamSide side, Position ball) {
    int nearest = 99;
    state.forEachOnPitch(opponent(side), [&](const Player& o) {
        if (o.state != PlayerState::STANDING) return;
        nearest = std::min(nearest, static_cast<int>(o.position.distanceTo(ball)));
    });
    if (nearest <= 2) return 0.56;
    if (nearest <= 5) return 0.38;
    return 0.14;
}

double carrierFallCost(const GameState& state, const Player& carrier, Position ball) {
    const double loss = looseBallLossRisk(state, carrier.teamSide, ball);
    const int target = std::clamp(7 - static_cast<int>(carrier.stats.agility) - 1, 2, 6);   // zvednutí: +1
    double pick = (target - 1) / 6.0;
    if (carrier.hasSkill(SkillName::SureHands)) pick *= pick;
    const double tempo = teamHasTimeSlack(state, carrier) ? 0.0 : 0.5;
    return std::min(1.0, loss + (1.0 - loss) * pick * 0.5 + tempo);
}

std::vector<std::pair<int, Position>> cornersComing(const GameState& state, const Player& carrier, Position sq) {
    const TeamSide side = carrier.teamSide;
    std::vector<Position> slots;
    for (int cx : {-1, 1}) for (int cy : {-1, 1}) {
        const Position c{static_cast<int8_t>(sq.x + cx), static_cast<int8_t>(sq.y + cy)};
        if (c.isOnPitch()) slots.push_back(c);
    }
    std::vector<const Player*> mates;
    state.forEachOnPitch(side, [&](const Player& p) {
        if (p.id != carrier.id && p.state == PlayerState::STANDING) mates.push_back(&p);
    });
    const std::vector<Position> slots0 = slots;      // pevné pořadí pro mezipaměť
    auto serves = [&](const Player& p, Position c) {
        if (p.position == c) return true;
        const Player* occ = state.getPlayerAtPosition(c);
        if (occ && occ->id != carrier.id) return false;
        // Review 16.10.: P211b platí i pro výběr rohů — Wild Animal roh klece není („Rat Ogre nemá
        // být roh klece“); přesun do rohu by na 4+ v polovině případů propadl.
        if (!cageMayMove(p) || countTacklezones(state, p.position, side, p.id) > 0) return false;
        if (p.position.distanceTo(c) > static_cast<int>(p.movementRemaining)) return false;
        // P190 (rozbor skavenů 09.10.2026): dosud stačila vzdálenost — odhad hrozby po pohybu nosiče
        // pak počítal s rohy, které přes zeď soupeřů nedojdou (nosič po zvednutí odběhl před tým,
        // „rohy“ uvízly v půli cesty). Teď jen hráč, který na pole dojde bez hodu. Pole, na kterém
        // teď stojí nosič (uvolní ho), se bere podle vzdálenosti.
        if (!cageFeatureOn(kFeatCornerPathReach) || occ) return true;
        return pathFailProb(state, p, c, p.movementRemaining, Position{-1, -1}) == 0.0;
    };
    // „dojde“ se ptá drahý hledač cest ⇒ spočítat jednou pro každou dvojici (hráč, pole), ne při
    // každém pořadí polí znovu
    std::vector<std::vector<char>> can(mates.size(), std::vector<char>(slots.size(), -1));
    auto servesCached = [&](size_t pi, Position c) {
        size_t si = 0;
        while (si < slots0.size() && slots0[si] != c) ++si;
        char& v = can[pi][si];
        if (v < 0) v = serves(*mates[pi], c) ? 1 : 0;
        return v == 1;
    };
    // největší párování (nejvýš 4 pole): zkusit všechna pořadí polí hladově
    std::sort(slots.begin(), slots.end(), [](Position a, Position b) {
        return a.x != b.x ? a.x < b.x : a.y < b.y;
    });
    std::vector<std::pair<int, Position>> best;
    do {
        std::vector<std::pair<int, Position>> got;
        for (const Position& c : slots) {
            for (size_t pi = 0; pi < mates.size(); ++pi) {
                const Player* p = mates[pi];
                if (std::any_of(got.begin(), got.end(), [&](const auto& g) { return g.first == p->id; })) continue;
                if (!servesCached(pi, c)) continue;
                got.push_back({p->id, c});
                break;
            }
        }
        if (got.size() > best.size()) best = std::move(got);
    } while (best.size() < slots.size() &&
             std::next_permutation(slots.begin(), slots.end(), [](Position a, Position b) {
                 return a.x != b.x ? a.x < b.x : a.y < b.y;
             }));
    return best;
}

int cornersWithinReach(const GameState& state, const Player& carrier, Position sq) {
    return static_cast<int>(cornersComing(state, carrier, sq).size());
}

double carrierThreatAt(const GameState& state, const Player& carrier, Position sq, double stopAbove) {
    if (!anyOpponentReaches(state, carrier.teamSide, sq)) return 0.0;
    GameState proj = state.clone();
    for (const auto& [id, slot] : cornersComing(state, carrier, sq)) proj.getPlayer(id).position = slot;
    Player& c = proj.getPlayer(carrier.id);
    c.position = sq;
    c.state = PlayerState::STANDING;
    proj.ball = BallState::carried(sq, carrier.id);
    return blitzThreat(proj, c, stopAbove);
}

double loneCarrierThreatAt(const GameState& state, const Player& carrier, Position sq) {
    if (!anyOpponentReaches(state, carrier.teamSide, sq)) return 0.0;
    GameState proj = state.clone();
    Player& c = proj.getPlayer(carrier.id);
    c.position = sq;
    c.state = PlayerState::STANDING;
    proj.ball = BallState::carried(sq, carrier.id);
    return blitzThreat(proj, c);
}

double handOffTdChance(const GameState& state, const Player& carrier, const Player& receiver,
                       Position* via, Position* ez) {
    if (!carrier.isOnPitch() || !receiver.isOnPitch() || receiver.state != PlayerState::STANDING) return 0.0;
    // review 16.10.2026: předávka je jedna za tah — řadič by ji jinak po přeplánování nabídl podruhé
    if (state.getTeamState(carrier.teamSide).handOffUsedThisTurn) return 0.0;
    if (receiver.hasSkill(SkillName::NoHands) || !receiver.canAct() || receiver.hasMoved || receiver.hasActed) return 0.0;
    const TeamSide side = carrier.teamSide;
    // 1) nosič na pole vedle příjemce (svým pohybem, bez GFI)
    double p1 = 0.0;
    Position bestVia{-1, -1};
    if (carrier.position.distanceTo(receiver.position) == 1) {
        p1 = 1.0;
        bestVia = carrier.position;
    } else {
        for (const Position& v : receiver.position.getAdjacent()) {
            if (!v.isOnPitch() || state.getPlayerAtPosition(v)) continue;
            if (v.distanceTo(carrier.position) > static_cast<int>(carrier.movementRemaining)) continue;
            const double fail = pathFailProb(state, carrier, v, carrier.movementRemaining, Position{-1, -1});
            if (fail < 0.0) continue;
            if (1.0 - fail > p1) { p1 = 1.0 - fail; bestVia = v; }
        }
    }
    if (p1 <= 0.0) return 0.0;
    // 2) zachycení předávky
    const int target = std::clamp(calculateCatchTarget(state, receiver, 1), 2, 6);
    double p2 = (7 - target) / 6.0;
    // přehoz: Catch, jinak týmový přehoz, je-li v tomto tahu k dispozici (uživatel 08.10.2026:
    // „předávka nevyjde — počítej s team rerollem, pokud je k dispozici“)
    if (receiver.hasSkill(SkillName::Catch) || state.getTeamState(side).canUseReroll()) {
        p2 = 1.0 - (1.0 - p2) * (1.0 - p2);
    }
    // 3) příjemce do zóny
    double p3 = 0.0;
    Position bestEz{-1, -1};
    const int ezX = (side == TeamSide::HOME) ? 25 : 0;
    const int budget = static_cast<int>(receiver.movementRemaining) + maxGfiSquares(receiver);
    if (distToEndzone(receiver.position, side) > budget) return 0.0;
    for (int y = 0; y < 15; ++y) {
        const Position sq{static_cast<int8_t>(ezX), static_cast<int8_t>(y)};
        if (state.getPlayerAtPosition(sq)) continue;
        const double fail = pathFailProb(state, receiver, sq, budget, Position{-1, -1});
        if (fail < 0.0) continue;
        if (1.0 - fail > p3) { p3 = 1.0 - fail; bestEz = sq; }
    }
    if (p3 <= 0.0) return 0.0;
    if (via) *via = bestVia;
    if (ez) *ez = bestEz;
    return p1 * p2 * p3;
}

Position farthestSafeForward(const GameState& state, const Player& carrier, int budget, bool forCage) {
    const TeamSide side = carrier.teamSide;
    // Menší je lepší: (pro klec: hrozba rány na nosiče po tahu,) vzdálenost k TD zóně, pak kolik
    // soupeřů na pole dosáhne, pak odklon od středu hřiště.
    // Hrozba: 0 = v bezpečí (nikdo nedosáhne, nebo nejvýš rána „dvě kostky, vybírá nosič“ — čistá
    // klec i útěk z dosahu jsou stejně dobré a rozhodne vzdálenost), jinak po desetinách.
    using Rest = std::tuple<int, int, int>;
    using Key = std::pair<int, Rest>;
    auto restOf = [&](Position sq) {
        return Rest{distToEndzone(sq, side), threatsTo(state, sq, side), std::abs(sq.y - 7)};
    };
    // `cap`: stačí vědět, je-li pásmo hrozby nejvýš `cap` (výpočet pak končí dřív); −1 = celé
    auto riskOf = [&](Position sq, int cap) {
        if (!forCage) return 0;
        const double stop = cap < 0 ? 2.0 : std::max(kSafeBlitzThreat, cap / 10.0 + 0.05);
        const double t = carrierThreatAt(state, carrier, sq, stop);
        return t <= kSafeBlitzThreat ? 0 : static_cast<int>(10.0 * t + 0.5);
    };
    Position best = carrier.position;
    Key bestKey{riskOf(best, -1), restOf(best)};
    const Key startKey = bestKey;        // pole přes GFI musí být lepší než stát
    const int startDist = distToEndzone(carrier.position, side);
    // P176: nosič smí přidat GFI jen jako ÚTĚK Z DOSAHU — když pole bez hodu, které by jinak vzal,
    // v dosahu soupeře zůstává, pole přes GFI už ne, a pád by byl levný (šance pádu × cena pádu
    // nejvýš 5 %, viz carrierFallCost). Pád stojí i tah, a ten pomalý tým nemá: po první
    // verzi, kde GFI přidávalo jen vzdálenost, stouplo u trpaslíků „soupeř skóruje první“ z 28
    // na 38. Jen když `budget` je celý zbývající pohyb nosiče.
    const int extraGfi = (cageFeatureOn(kFeatFallValue) && budget == static_cast<int>(carrier.movementRemaining))
                             ? maxGfiSquares(carrier) : 0;
    Position bestRush{-1, -1};
    Rest bestRushRest{};
    for (int x = carrier.position.x - budget - extraGfi; x <= carrier.position.x + budget + extraGfi; ++x) {
        for (int y = carrier.position.y - budget - extraGfi; y <= carrier.position.y + budget + extraGfi; ++y) {
            const Position sq{static_cast<int8_t>(x), static_cast<int8_t>(y)};
            if (!sq.isOnPitch() || sq == carrier.position) continue;
            if (sq.y < 1 || sq.y > 13) continue;           // rohy klece musí mít kam stát
            // P131 / P169 krok 7: aspoň dvě pole od postranní čáry (surf, s Frenzy i z řádku
            // vedle kraje). Výjimka jen pro nosiče, který u kraje už stojí a jde ke středu.
            if (cageFeatureOn(kFeatSideline) && (sq.y < 2 || sq.y > 12) &&
                std::abs(sq.y - 7) >= std::abs(carrier.position.y - 7)) continue;
            if (distToEndzone(sq, side) > startDist) continue;   // nikdy dozadu
            if (state.getPlayerAtPosition(sq)) continue;
            if (countTacklezones(state, sq, side, carrier.id) > 0) continue;
            const Rest rest = restOf(sq);
            // levné vyřazení dřív než drahé výpočty: pole, které nemůže být lepší než dosud nejlepší
            const bool freeCouldWin = bestKey.first > 0 || rest < bestKey.second;
            const bool rushCouldWin = extraGfi > 0 && (bestRush.isOnPitch() ? rest < bestRushRest : Key{0, rest} < startKey);
            if (!freeCouldWin && !rushCouldWin) continue;
            // -1 = nedojde, > 0 = cesta vede přes hod (dodge nebo GFI)
            const double fail = pathFailProb(state, carrier, sq, budget + extraGfi, Position{-1, -1});
            if (fail < 0.0) continue;
            if (fail == 0.0) {
                if (!freeCouldWin) continue;
                const Key k{riskOf(sq, bestKey.first), rest};
                if (k < bestKey) { best = sq; bestKey = k; }
                continue;
            }
            if (!rushCouldWin) continue;
            // Review 08.10. (nález 5): přes hod smí nosič jen kvůli GFI — ne přes úhyb. Šance pádu
            // tedy nesmí být větší, než kolik dají samotná GFI na této cestě (1 GFI = 1/6, 2 = 11/36).
            {
                const int gfi = carrierGfiFor(state, carrier, sq);
                if (gfi < 1) continue;
                const double gfiOnly = gfi >= 2 ? 11.0 / 36.0 : 1.0 / 6.0;
                if (fail > gfiOnly + 1e-9) continue;
            }
            // P178: pád se cení celý (soupeř u míče + nezvednutí podle obratnosti + ztracený tah u
            // týmu bez časové rezervy); smí se riskovat nejvýš 5 %
            if (fail * carrierFallCost(state, carrier, sq) > 0.05) continue;
            if (anyOpponentReaches(state, side, sq)) continue;     // i tam by na něj dosáhli
            bestRush = sq;
            bestRushRest = rest;
        }
    }
    if (bestRush.isOnPitch()) {
        // je pole bez hodu už bezpečné (hrozba v pásmu 0, nebo mimo dosah)? pak GFI netřeba
        const bool freeIsSafe = forCage ? bestKey.first == 0 : !anyOpponentReaches(state, side, best);
        if (!freeIsSafe) return bestRush;
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
        // Odhad tempa klece: jen podle vzdálenosti (07.10.2026 — s přísným „bez hodu“ vyšel
        // krok kratší, klec „nestíhala“ a nosič se vypouštěl 16× místo 4× ve 40 poločasech).
        if (planner.tryAssign(state, carrier, s, {}, 0, /*diceFreeReach=*/false).feasible) { r.cageStep = s; break; }
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
    // ⭐ P187 (nález testu invariantu 08.10.2026; uživatel: „když nosič nemůže skórovat ani být
    //   v bezpečí — nesmí nastat“, „rychlejší tým by měl být pouze ve stavech — klec v pořádku —
    //   a — nosič doběhne…“). „Klec nestíhá“ vyšlo i tehdy, když rohy jen zůstaly o tah pozadu
    //   (krok klece 0) — rychlý nosič ve 3. kole pak vyběhl sám dvě pole k soupeři (hrozba 0,55).
    //   Dokud má tým časovou rezervu, nosič se nevypouští: počká / ustoupí ke spoluhráčům.
    if (r.release && cageFeatureOn(kFeatReleaseNeedsNoSlack) && teamHasTimeSlack(state, carrier)) r.release = false;
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
    if (m.type == MacroType::SCORE) return state.ball.carrierId == m.playerId && freeToAct(state.getPlayer(m.playerId));
    if (m.type == MacroType::HAND_OFF_SCORE) {
        return state.ball.carrierId == m.playerId && freeToAct(state.getPlayer(m.playerId)) &&
               freeToAct(state.getPlayer(m.targetId));
    }
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
    // ⭐⭐ P180 (uživatel 08.10.2026: „nosič doběhne, případně předá nebo hodí někomu nachystanému
    //   dát TD“; rozhodnutí téhož dne: 1) „hrozbu ztráty míče řešíme dřívějším TD vždy“,
    //   2) „pokud je míč v bezpečí a máme čas — volíme zdržovat“). Nosič sám do zóny bez hodu
    //   nedojde. Je-li míč v bezpečí (hrozba rány ≤ 0,15 — i čistá klec s ranou „dvě kostky,
    //   vybírá nosič“) a tým má časovou rezervu, TD předávkou
    //   se nehraje (ani hledáním — dřív díra ve zdržování). Jinak stejné porovnání jako u TD přes
    //   hod: šance TD předávkou teď proti šanci, že míč přežije soupeřův tah (po našem nejlepším
    //   plánu klece); je-li TD aspoň stejně pravděpodobné a lepší než TD nosičem, přikáže ho řadič.
    //   Poslední kolo zůstává hledání (může najít i přihrávku).
    auto orderViaMate = [&](const Player& c) -> bool {
        if (!cageFeatureOn(kFeatScoreViaMate) || c.teamSide != state.activeTeam || !freeToAct(c)) return false;
        if (state.getTeamState(c.teamSide).turnNumber >= 8) return false;
        double pm = 0.0;
        int rid = -1;
        Position via{-1, -1}, ez{-1, -1};
        state.forEachOnPitch(c.teamSide, [&](const Player& m) {
            if (m.id == c.id) return;
            Position v{-1, -1}, e{-1, -1};
            const double p = handOffTdChance(state, c, m, &v, &e);
            if (p > pm) { pm = p; rid = m.id; via = v; ez = e; }
        });
        // Hrozba PO našem tahu (dostavba / postup klece), ne jak desku nechal soupeř. Review 08.10.
        // (M1): zdržování i porovnání se rozhodují z téže hrozby.
        double threat = blitzThreat(state, c);
        if (threat > kSafeBlitzThreat && rid >= 0) {
            const CageAdvancePlan after = planner_.build(state, {}, /*evenInScoringRange=*/true);
            if (after.valid) threat = std::min(threat, after.blitzThreat);
        }
        // míč v bezpečí a je čas ⇒ nehraje se žádné TD přes spoluhráče — ani když příjemce pro
        // předávku není a šla by jen přihrávka (review M2)
        if (threat <= kSafeBlitzThreat && teamHasTimeSlack(state, c)) {
            mateStall_ = true;
            if (dbg) std::fprintf(stderr, "[cage ctl] míč v bezpečí (hrozba %.2f) a je čas — TD přes spoluhráče se zdržuje\n", threat);
            return false;
        }
        if (rid < 0) return false;
        double own = 0.0;
        {
            const int ezX = (c.teamSide == TeamSide::HOME) ? 25 : 0;
            const int budget = static_cast<int>(c.movementRemaining) + maxGfiSquares(c);
            for (int y = 0; y < 15; ++y) {
                const Position sq{static_cast<int8_t>(ezX), static_cast<int8_t>(y)};
                if (state.getPlayerAtPosition(sq)) continue;
                const double fail = pathFailProb(state, c, sq, budget, Position{-1, -1});
                if (fail >= 0.0) own = std::max(own, 1.0 - fail);
            }
        }
        if (own >= pm) return false;                      // nosič sám má aspoň stejnou šanci
        if (dbg) std::fprintf(stderr, "[cage ctl] TD předávkou %.2f (hráč %d) × míč přežije %.2f => %s\n", pm, rid,
                              1.0 - threat, pm >= 1.0 - threat ? "předat" : "klec");
        if (pm < 1.0 - threat) return false;
        Macro ho{MacroType::HAND_OFF_SCORE, c.id, rid, ez};
        ho.viaPos = via;
        phase_ = CagePhase::CAGE;
        queue_ = {ho};
        stage_ = Stage::DONE;
        return true;
    };
    // ⭐ P180 (6), uživatel 16.10.2026: „poslední kolo poločasu je poslední šance dát TD — vše pro dát
    //   TD — vyhledat nejbezpečnější možnost“. V posledním kole řadič přikáže cestu k TD s NEJVĚTŠÍ
    //   šancí: nosič sám (i přes GFI a úhyby), nebo předávka připravenému (příjemce i s GFI). Dosud tu
    //   rozhodovalo hledání a s nosičem daleko od zóny šla klec dál, jako by poločas pokračoval.
    //   Bez šance obojím (0) zůstává hledání — může najít přihrávku (P188).
    auto lastTurnBestTd = [&](const Player& c) -> bool {
        if (!cageFeatureOn(kFeatLastTurnBestTd) || c.teamSide != state.activeTeam || !freeToAct(c)) return false;
        if (state.getTeamState(c.teamSide).turnNumber < 8) return false;
        const int ezX = (c.teamSide == TeamSide::HOME) ? 25 : 0;
        double own = 0.0;
        Position ownSq{-1, -1};
        const int budget = static_cast<int>(c.movementRemaining) + maxGfiSquares(c);
        for (int y = 0; y < 15; ++y) {
            const Position sq{static_cast<int8_t>(ezX), static_cast<int8_t>(y)};
            if (state.getPlayerAtPosition(sq)) continue;
            const double fail = pathFailProb(state, c, sq, budget, Position{-1, -1});
            if (fail >= 0.0 && 1.0 - fail > own) { own = 1.0 - fail; ownSq = sq; }
        }
        double pm = 0.0;
        int rid = -1;
        Position via{-1, -1}, ez{-1, -1};
        state.forEachOnPitch(c.teamSide, [&](const Player& m) {
            if (m.id == c.id) return;
            Position v{-1, -1}, e{-1, -1};
            const double p = handOffTdChance(state, c, m, &v, &e);
            if (p > pm) { pm = p; rid = m.id; via = v; ez = e; }
        });
        if (dbg) std::fprintf(stderr, "[cage ctl] poslední kolo: TD nosičem %.2f, předávkou %.2f (hráč %d)\n", own, pm, rid);
        if (own <= 0.0 && pm <= 0.0) return false;
        phase_ = CagePhase::CAGE;
        if (pm > own) {
            Macro ho{MacroType::HAND_OFF_SCORE, c.id, rid, ez};
            ho.viaPos = via;
            queue_ = {ho};
        } else {
            queue_ = {Macro{MacroType::SCORE, c.id, -1, ownSq}};
        }
        stage_ = Stage::DONE;
        return true;
    };
    // ⭐ P213 (uživatel 16.10.2026: „pokud uteče — předat — pokud byl předtím v bezpečí — zdržovat“).
    //   Klec míč neochrání ani po nejlepším plánu (hrozba > 0,15) a spoluhráč po předávce s míčem
    //   doběhne tam, kam soupeř dobře nedosáhne (hrozba ≤ 0,15) ⇒ předat, je-li šance, že míč přežije
    //   (doběh nosiče × chycení × doběh příjemce × bezpečí příjemce), větší než v kleci. Nosič v bezpečí ⇒
    //   nic (zdržuje se jako dosud).
    //   17.10. (uživatel: „i útěk může obsahovat GFI“): doběh nosiče i příjemce smí mít GFI a úhyb do
    //   stupně 3+ (riskTier ≤ 1, celá cesta dohromady); jejich riziko se násobí do přežití míče.
    // P217 (3): `surviveOut` != nullptr ⇒ suchý běh — nic se nezařadí, vrátí šanci, že míč útěkem přežije
    //   (hody předávky a doběhů × nejlepší rána soupeře na příjemce).
    auto orderSafetyHandOff = [&](const Player& c, double* surviveOut = nullptr) -> bool {
        if (!cageFeatureOn(kFeatSafetyHandOff) || c.teamSide != state.activeTeam || !freeToAct(c)) return false;
        if (state.getTeamState(c.teamSide).handOffUsedThisTurn) return false;   // jedna předávka za tah
        double threat = blitzThreat(state, c);
        if (threat <= kSafeBlitzThreat) return false;
        const CageAdvancePlan after = planner_.build(state, {}, /*evenInScoringRange=*/true);
        if (after.valid) threat = std::min(threat, after.blitzThreat);
        if (threat <= kSafeBlitzThreat) return false;
        const TeamSide side = c.teamSide;
        double best = 1.0 - threat + 0.05;   // rezerva: předávka je hod, klec ne
        Macro pick;
        bool found = false;
        state.forEachOnPitch(side, [&](const Player& m) {
            if (m.id == c.id || m.state != PlayerState::STANDING || !m.canAct() || m.hasMoved || m.hasActed ||
                m.hasSkill(SkillName::NoHands) || !cageMayMove(m)) return;
            // 1) nosič na pole vedle příjemce — bez hodu
            Position via{-1, -1};
            double pvOk = 0.0;   // doběh nosiče k příjemci
            const bool rolls = extraFeatureOn(kP213EscapeRolls);
            const int carrierBudget = c.movementRemaining + (rolls ? maxGfiSquares(c) : 0);
            if (c.position.distanceTo(m.position) == 1) {
                via = c.position;
                pvOk = 1.0;
            } else {
                for (const Position& v : m.position.getAdjacent()) {
                    if (!v.isOnPitch() || state.getPlayerAtPosition(v)) continue;
                    if (v.distanceTo(c.position) > carrierBudget) continue;
                    const double pf = pathFailProb(state, c, v, carrierBudget, Position{-1, -1});
                    if (pf < 0.0 || riskTier(pf) > 1 || 1.0 - pf <= pvOk || (!rolls && pf != 0.0)) continue;
                    via = v;
                    pvOk = 1.0 - pf;
                    if (pf == 0.0) break;
                }
            }
            if (!via.isOnPitch()) return;
            // 2) chycení (přehoz Catch / týmový jako u TD předávkou)
            const int target = std::clamp(calculateCatchTarget(state, m, 1), 2, 6);
            double pc = (7 - target) / 6.0;
            if (m.hasSkill(SkillName::Catch) || state.getTeamState(side).canUseReroll()) pc = 1.0 - (1.0 - pc) * (1.0 - pc);
            if (pvOk * pc <= best) return;
            // 3) bezpečné pole příjemce — i s GFI a úhybem do 3+; napřed pole nejdál od soupeřů
            struct Sq { int key; Position sq; };
            std::vector<Sq> cands;
            const int budget = m.movementRemaining + (rolls ? maxGfiSquares(m) : 0);
            for (int x = m.position.x - budget; x <= m.position.x + budget; ++x)
                for (int y = std::max(1, m.position.y - budget); y <= std::min(13, m.position.y + budget); ++y) {
                    const Position sq{static_cast<int8_t>(x), static_cast<int8_t>(y)};
                    if (!sq.isOnPitch() || (sq != m.position && state.getPlayerAtPosition(sq))) continue;
                    if (countTacklezones(state, sq, side, m.id) > 0) continue;
                    if (distToEndzone(sq, side) < 1) continue;   // pole v zóně = TD, ne útěk (16.10. večer)
                    cands.push_back({threatsTo(state, sq, side) * 100 + distToEndzone(sq, side), sq});
                }
            std::sort(cands.begin(), cands.end(), [](const Sq& a, const Sq& b) { return a.key < b.key; });
            // napřed jen pole bez hodu; s hodem (do 3+) jen když bez hodu žádné bezpečné není
            // (uživatel 17.10.: „více riskantní pouze když není jiná možnost mín riskantní“)
            bool safeFound = false;
            for (int pass = 0; pass < (rolls ? 2 : 1) && !safeFound; ++pass) {
            int tried = 0;
            for (const Sq& q : cands) {
                const double pr = q.sq == m.position ? 0.0 : pathFailProb(state, m, q.sq, budget, Position{-1, -1});
                if (pr < 0.0 || riskTier(pr) > 1 || (pass == 0) != (pr == 0.0)) continue;
                if (++tried > 10) break;
                GameState proj = state.clone();
                proj.getPlayer(c.id).position = via;
                Player& pm = proj.getPlayer(m.id);
                pm.position = q.sq;
                proj.ball = BallState::carried(q.sq, m.id);
                const double tm = blitzThreat(proj, pm, kSafeBlitzThreat);
                if (tm > kSafeBlitzThreat) continue;
                safeFound = true;
                const double survive = pvOk * pc * (1.0 - pr) * (1.0 - tm);
                if (survive > best) {
                    best = survive;
                    pick = Macro{MacroType::HAND_OFF_SCORE, c.id, m.id, q.sq};
                    pick.viaPos = via;
                    found = true;
                }
                if (pass == 0) break;   // nejbezpečnější dosažitelné pole bez hodu
            }
            }
        });
        if (dbg) std::fprintf(stderr, "[cage ctl] P213: klec míč udrží %.2f, předávka kvůli bezpečí %s\n", 1.0 - threat,
                              found ? "ANO" : "ne");
        if (!found) return false;
        if (surviveOut) { *surviveOut = best; return true; }
        phase_ = CagePhase::CAGE;
        queue_ = {pick};
        stage_ = Stage::DONE;
        return true;
    };
    // ⭐ P217 (3) (uživatel 19.10.2026: „dokud je bezpečná klec — nic jiného — pak útěk nebo TD, když klec
    //   nedojde nebo není bezpečná“; „přednost má to, co má nejnižší pravděpodobnost neúspěchu — počítej
    //   i naše kolo — dodge a GFI — i další kolo soupeře pro blitz na nosiče“; „počítej jen aktuální kolo
    //   a pak maximální odpověď oponenta v příštím kole — víc dopředu určitě ne a složitý výpočet taky ne“).
    //   Klec není bezpečná nebo nestihne dojít ⇒ každá varianta dostane šanci na úspěch: TD teď = hody
    //   tohoto tahu; klec / útěk / výběh = hody tohoto tahu × míč přežije nejlepší ránu soupeře. Hraje se
    //   nejvyšší. Útěk i výběh jen do bezpečí (hrozba ≤ 0,15; uživatel 08.10.: „když nosič nemůže skórovat
    //   ani být v bezpečí — nesmí nastat“), u výběhu hrozba bez rohů (za vybíhajícím nedojdou). Při shodě
    //   vyhrává dřívější v pořadí klec · útěk · výběh · předávka teď · TD teď (pozdější TD nechá soupeři méně
    //   tahů). Vrací true, když řadič tah zařadil; jinak `p217Cage` / `p217Run` řekne, co dělá zbytek.
    bool p217Cage = false, p217Run = false;
    auto bestChance = [&](const Player& c, bool walksIn) -> bool {
        if (!extraFeatureOn(kP217BestChance) || c.teamSide != state.activeTeam || !freeToAct(c)) return false;
        const TeamSide side = c.teamSide;
        if (state.getTeamState(side).turnNumber >= 8) return false;   // poslední kolo: lastTurnBestTd
        double threat = blitzThreat(state, c);
        // levná kontrola napřed: bezpečný už teď a (čas, nebo zdržování) ⇒ nic dalšího se nepočítá
        if (threat <= kSafeBlitzThreat && (walksIn || teamHasTimeSlack(state, c))) return false;
        const CageAdvancePlan cage = planner_.build(state, {}, /*evenInScoringRange=*/true);
        if (cage.valid) threat = std::min(threat, cage.blitzThreat);
        const bool safe = threat <= kSafeBlitzThreat;
        if (safe && teamHasTimeSlack(state, c)) return false;  // bezpečná klec s časem — nic jiného
        if (safe && walksIn) return false;                     // zdržování (P175 / P217 (1))
        // klec dál — jen když do konce poločasu dojde (uživatel: „útěk nebo TD, když klec nedojde“)
        const bool cageArrives = decideRelease(state, c, planner_).cageMakesIt;
        double vCage = cageArrives ? 1.0 - threat : 0.0;
        if (cage.valid && cage.carrierGfi > 0) {
            for (const Macro& m : cage.macros) {
                if (m.playerId != c.id) continue;
                const double pf = pathFailProb(state, c, m.targetPos, c.movementRemaining + cage.carrierGfi, Position{-1, -1});
                if (pf >= 0.0) vCage *= 1.0 - pf;
            }
        }
        double vEsc = 0.0;
        const bool canEsc = orderSafetyHandOff(c, &vEsc);
        const Position dest = farthestSafeForward(state, c, c.movementRemaining);
        double vRun = 0.0;
        if (dest != c.position) {
            const double lone = loneCarrierThreatAt(state, c, dest);
            const double pf = pathFailProb(state, c, dest, c.movementRemaining, Position{-1, -1});
            if (lone <= kSafeBlitzThreat && pf >= 0.0) vRun = (1.0 - pf) * (1.0 - lone);
        }
        const int ezX = (side == TeamSide::HOME) ? 25 : 0;
        double vNow = 0.0;
        Position nowSq{-1, -1};
        const int budget = static_cast<int>(c.movementRemaining) + maxGfiSquares(c);
        for (int y = 0; y < 15; ++y) {
            const Position sq{static_cast<int8_t>(ezX), static_cast<int8_t>(y)};
            if (state.getPlayerAtPosition(sq)) continue;
            const double fail = pathFailProb(state, c, sq, budget, Position{-1, -1});
            if (fail >= 0.0 && 1.0 - fail > vNow) { vNow = 1.0 - fail; nowSq = sq; }
        }
        double vMate = 0.0;
        int rid = -1;
        Position via{-1, -1}, ez{-1, -1};
        state.forEachOnPitch(side, [&](const Player& m) {
            if (m.id == c.id) return;
            Position v{-1, -1}, e{-1, -1};
            const double p = handOffTdChance(state, c, m, &v, &e);
            if (p > vMate) { vMate = p; rid = m.id; via = v; ez = e; }
        });
        enum { CAGE, ESC, RUN, MATE, NOW } pick = CAGE;
        double best = vCage;   // 0 ⇒ klec mimo hru (nedojde)
        constexpr double eps = 1e-9;
        if (canEsc && vEsc > best + eps) { best = vEsc; pick = ESC; }
        if (vRun > best + eps) { best = vRun; pick = RUN; }
        if (rid >= 0 && vMate > best + eps) { best = vMate; pick = MATE; }
        if (nowSq.isOnPitch() && vNow > best + eps) { best = vNow; pick = NOW; }
        if (best <= 0.0) return false;   // nic nevychází ⇒ nerozhoduje se tu (dál jako dosud)
        if (dbg) std::fprintf(stderr, "[cage ctl] P217 šance na úspěch: klec %.2f · útěk %.2f · výběh %.2f · předávkou teď %.2f "
                              "· teď %.2f (hrozba %.2f, bezpečná %d) => %d\n", vCage, vEsc, vRun, vMate, vNow, threat, safe,
                              static_cast<int>(pick));
        switch (pick) {
            case CAGE: p217Cage = true; return false;
            case RUN: p217Run = true; return false;
            case ESC: return orderSafetyHandOff(c);
            case MATE: {
                Macro ho{MacroType::HAND_OFF_SCORE, c.id, rid, ez};
                ho.viaPos = via;
                phase_ = CagePhase::CAGE;
                queue_ = {ho};
                stage_ = Stage::DONE;
                return true;
            }
            case NOW:
                phase_ = CagePhase::CAGE;
                queue_ = {Macro{MacroType::SCORE, c.id, -1, nowSq}};
                stage_ = Stage::DONE;
                return true;
        }
        return false;
    };
    scoringRangeCage_ = false;
    if (goal == TurnGoal::SCORE_BALL && state.ball.isHeld && state.ball.carrierId > 0) {
        const Player& c = state.getPlayer(state.ball.carrierId);
        const bool lastTurn = state.getTeamState(c.teamSide).turnNumber >= 8;
        bool walksIn = false;
        Position scoreSq{-1, -1};            // pole zóny, podle kterého se rozhodlo — tam TD i půjde
        const int ezX = (c.teamSide == TeamSide::HOME) ? 25 : 0;
        for (int y = 0; y < 15 && !walksIn; ++y) {
            const Position sq{static_cast<int8_t>(ezX), static_cast<int8_t>(y)};
            if (state.getPlayerAtPosition(sq)) continue;
            walksIn = pathFailProb(state, c, sq, c.movementRemaining, Position{-1, -1}) == 0.0;
            if (walksIn) scoreSq = sq;
        }
        scoringRangeCage_ = !lastTurn && !walksIn && c.teamSide == state.activeTeam;
        // ⭐ P175, druhá polovina (uživatel 08.10.2026: „pokud hrozí blitz na nosiče a ztráta — je
        //   lepší dát TD dříve“; „toto by mohlo vrátit skavenům skórování“). Nosič do zóny dojde
        //   jen přes hod (GFI / úhyb). Dosud v takovém tahu vedla klec vždy (K3 z 06.10., psané
        //   pro trpaslíky). Teď se porovná: šance, že TD teď vyjde, proti šanci, že míč přežije
        //   soupeřův tah (1 − hrozba rány na nosiče). Je-li TD teď aspoň stejně pravděpodobné,
        //   klec nevede a skóruje se (rozhoduje hledání). Trpaslík v kleci má hrozbu kolem 2 %,
        //   takže dál čeká na jistotu; osamělý rychlý nosič s hrozbou 33–55 % běží pro TD.
        if (scoringRangeCage_ && cageFeatureOn(kFeatScoreEarly)) {
            double scoreNow = 0.0;
            const int budget = static_cast<int>(c.movementRemaining) + maxGfiSquares(c);
            for (int y = 0; y < 15; ++y) {
                const Position sq{static_cast<int8_t>(ezX), static_cast<int8_t>(y)};
                if (state.getPlayerAtPosition(sq)) continue;
                const double fail = pathFailProb(state, c, sq, budget, Position{-1, -1});
                if (fail >= 0.0 && 1.0 - fail > scoreNow) { scoreNow = 1.0 - fail; scoreSq = sq; }
            }
            double threat = blitzThreat(state, c);
            // Review 08.10.2026: hrozba na začátku tahu je hrozba na klec, jak ji nechal SOUPEŘ
            // (odtlačené rohy). Rozhoduje hrozba PO našem tahu — když klec tento tah dostavíme
            // nebo s ní postoupíme do bezpečí, míč přežije a přes hody se neběží.
            if (scoreNow > 0.0 && scoreNow >= 1.0 - threat && freeToAct(c)) {
                const CageAdvancePlan after = planner_.build(state, {}, /*evenInScoringRange=*/true);
                if (after.valid) threat = std::min(threat, after.blitzThreat);
            }
            if (scoreNow > 0.0 && scoreNow >= 1.0 - threat) scoringRangeCage_ = false;
            if (dbg) std::fprintf(stderr, "[cage ctl] TD teď %.2f × míč přežije %.2f => %s\n", scoreNow, 1.0 - threat,
                                  scoringRangeCage_ ? "klec" : "skórovat hned");
        }
        // ⭐⭐ P175 (uživatel 08.10.2026: „obecně chci, ať s TD zdržujeme za všechny, ale jen v případě,
        //   kdy máme balon bezpečně v držení a nehrozí blitz na nosiče — na druhou stranu pokud
        //   hrozí blitz na nosiče a ztráta, je lepší dát TD dříve — toto je obojí obecné
        //   pravidlo“). Nosič by do zóny došel bez hodu, není poslední kolo poločasu a soupeř na
        //   něj v příštím tahu nedosáhne, nebo jen ranou „dvě kostky, vybírá nosič“ (hrozba ≤ 0,15; do 08.10. večer 0,05) ⇒ TENTO TAH SE
        //   NESKÓRUJE: nosič stojí, kolem něj se dostaví klec a skórovací makra ani pohyb nosiče
        //   hledání nedostane. Jakmile hrozba na začátku některého dalšího tahu stoupne, nebo
        //   přijde 8. kolo, skóruje se hned (rozhoduje hledání jako dosud).
        if (cageFeatureOn(kFeatStall) && walksIn && !lastTurn && c.teamSide == state.activeTeam &&
            freeToAct(c) && blitzThreat(state, c) <= kSafeBlitzThreat) {
            stalling_ = true;
            phase_ = CagePhase::CAGE;
            if (dbg) std::fprintf(stderr, "[cage ctl] zdržování TD: nosič v bezpečí (hrozba rány %.2f), kolo %d\n",
                                  blitzThreat(state, c), state.getTeamState(c.teamSide).turnNumber);
            CageAdvancePlan fill = stallPlan(state, c);
            if (fill.valid) queue_ = std::move(fill.macros);
            stage_ = Stage::AFTER_ADVANCE;
            return;
        }
        // ⭐⭐⭐ P178 (uživatel 08.10.2026: „aby nepřicházeli o míč později — tam má být kontrola, ať
        //   raději skórují, než zůstat jako cíl pro blitz“; „když nosič nemůže skórovat ani být
        //   v bezpečí — nesmí nastat“). Dosud v tahu, kde se nezdržuje, řadič ustoupil a skórování
        //   nechal na hledání — a to někdy neskórovalo (skaven, 80 poločasů: 2 ze 17 tahů s
        //   nosičem do 9 polí od zóny). Teď TD PŘIKÁŽE ŘADIČ: když nosič do zóny dojde bez hodu
        //   (a nezdržuje se — tedy hrozí rána nebo je poslední kolo), nebo když dojde jen přes
        //   hod a TD teď je aspoň stejně pravděpodobné jako to, že míč přežije soupeřův tah.
        //   Poslední kolo s TD jen přes hod zůstává hledání (může najít lepší šanci přihrávkou).
        if (lastTurn && !walksIn && lastTurnBestTd(c)) return;
        if (bestChance(c, walksIn)) return;                       // P217 (3)
        if (p217Cage || p217Run) scoringRangeCage_ = true;        // klec / výběh: TD se tento tah nehraje
        // P213 (uživatel 16.10. večer): útěk do bezpečí má PŘEDNOST před TD předávkou — hody jsou stejné
        // (chycení, případně úhyb a GFI), pozdější TD nechá soupeři méně kol na vyrovnání; TD předávkou
        // jen tehdy, když bezpečné pole mimo dosah soupeřů není. Poslední kolo zvlášť (výše: vždy TD).
        const bool p217Decided = p217Cage || p217Run;
        if (!p217Decided && extraFeatureOn(kP213EscapeFirst) && !lastTurn && !walksIn && orderSafetyHandOff(c)) return;
        if (!p217Decided && !walksIn && orderViaMate(c)) return;
        if (!p217Decided && cageFeatureOn(kFeatForceScore) && !scoringRangeCage_ && (walksIn || !lastTurn) &&
            c.teamSide == state.activeTeam && freeToAct(c)) {
            if (dbg) std::fprintf(stderr, "[cage ctl] TD příkazem řadiče (dojde bez hodu %d, poslední kolo %d)\n", walksIn, lastTurn);
            phase_ = CagePhase::CAGE;
            queue_ = {Macro{MacroType::SCORE, c.id, -1, scoreSq}};
            stage_ = Stage::DONE;
            return;
        }
        if (dbg) std::fprintf(stderr, "[cage ctl] SCORE_BALL: dojde bez hodu %d, poslední kolo %d => %s\n",
                              walksIn, lastTurn, scoringRangeCage_ ? "klec jde dál" : "rozhoduje hledání");
    }
    if (goal == TurnGoal::ADVANCE_BALL && state.ball.isHeld && state.ball.carrierId > 0 &&
        lastTurnBestTd(state.getPlayer(state.ball.carrierId))) return;
    if (goal == TurnGoal::ADVANCE_BALL && state.ball.isHeld && state.ball.carrierId > 0 &&
        bestChance(state.getPlayer(state.ball.carrierId), false)) return;   // P217 (3)
    const bool escapeFirst = extraFeatureOn(kP213EscapeFirst) && !p217Cage && !p217Run;
    if (goal == TurnGoal::ADVANCE_BALL && state.ball.isHeld && state.ball.carrierId > 0 && escapeFirst &&
        orderSafetyHandOff(state.getPlayer(state.ball.carrierId))) return;   // P213 před TD předávkou (16.10.)
    if (goal == TurnGoal::ADVANCE_BALL && state.ball.isHeld && state.ball.carrierId > 0 && !p217Cage && !p217Run &&
        orderViaMate(state.getPlayer(state.ball.carrierId))) return;
    if (goal == TurnGoal::ADVANCE_BALL && state.ball.isHeld && state.ball.carrierId > 0 && !escapeFirst && !p217Cage && !p217Run &&
        orderSafetyHandOff(state.getPlayer(state.ball.carrierId))) return;   // staré pořadí (jen měření)
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

    // P217 (3): volba klec / výběh přebíjí rozhodnutí podle vzdálenosti (decideRelease)
    if (!released(state, carrier) && !p217Run && (p217Cage || !decideRelease(state, carrier, planner_).release)) {
        phase_ = CagePhase::CAGE;
        // ⭐ P154 (uživatel 07.10.2026: „zkus dát všechny hráče na klec a na konci kdyžtak
        //   provést i blitz — blitz je sice jeden za kolo, ale bezpečí nosiče je důležitější
        //   než pravidlo využít blitz každé kolo“). NAPŘED KLEC ZE VŠECH VOLNÝCH HRÁČŮ: vyjde-li
        //   bez hodu se čtyřmi rohy (krokem, nebo už stojí), hraje se hned a uvolňovací rány,
        //   příchody pro asistenci ani blitz řadiče se nekonají — rány a blitz zbydou hledání
        //   na konec tahu. Dosud šly rány napřed a braly hráče, ze kterých se pak klec
        //   nepostavila („v dosahu jen ti, co hráli jinde“: hlavní důvod 44 ze 106 nečistých
        //   tahů trpaslíků, 40 poločasů TV1500). Uvolňuje se jen, když klec čtyři rohy nemá.
        CageAdvancePlan pre = planner_.build(state, {}, scoringRangeCage_);
        {
            int standing = 0;
            for (int cx : {-1, 1}) for (int cy : {-1, 1}) {
                const Player* q = state.getPlayerAtPosition({static_cast<int8_t>(carrier.position.x + cx),
                                                             static_cast<int8_t>(carrier.position.y + cy)});
                if (q && q->teamSide == carrier.teamSide && q->state == PlayerState::STANDING) ++standing;
            }
            const bool cleanByPlan = pre.valid && pre.filledCorners - pre.gfiCorners >= 4;   // roh na GFI tento tah nestojí
            // P169 krok 4 (07.10.2026): stojí-li u nosiče soupeř, klec „čistá“ není — napřed rány.
            const bool carrierMarked = cageFeatureOn(kFeatMarkers) &&
                                       countTacklezones(state, carrier.position, carrier.teamSide) > 0;
            // P172: klec, která stojí, ale nepostoupí („plán není“), zkratku nebere — pokračuje
            // se ranami na uvolnění a blitzem na proboření obrany níž.
            // P229: zeď přímo před klecí rozhoduje dřív než plán „klec napřed“ — ten by pomalejší
            // klec stáhl dozadu nebo poslal bokem (s P228 jsou rohy u zdi špinavé)
            if (!carrierMarked && extraFeatureOn(kP229WallBreak) && planWallBreak(state, carrier)) {
                stage_ = Stage::AFTER_ADVANCE;
                return;
            }
            if (!carrierMarked && (cleanByPlan || (!cageFeatureOn(kFeatPhaseOneBlitz) && !pre.valid && standing >= 4))) {
                if (!cleanByPlan) { stage_ = Stage::AFTER_ADVANCE; return; }
                if (dbg) std::fprintf(stderr, "[cage ctl] klec napřed: plán se čtyřmi rohy (krok %d, rohů %d)\n",
                                      pre.step, pre.filledCorners);
                queue_ = std::move(pre.macros);
                stage_ = Stage::AFTER_ADVANCE;
                return;
            }
        }
        // Klec čtyři rohy nemá ⇒ soupeř na poli rohu se napřed shodí (uživatel 02.10.).
        // ⭐ P169 krok 4 (uživatel 07.10.2026: „nosič stojí vedle soupeře — tohle jsem na konci
        //   našeho tahu měl za opravené“). NEBYLO: rány řadiče mířily jen na soupeře na ČTYŘECH
        //   úhlopříčných polích (rozích). Soupeř, který stál před nosičem, za ním nebo vedle
        //   něj, zůstal — a nosič v zóně nikam bez úhybu nemůže. Změřeno (trpaslíci, 160
        //   poločasů): po tahu s nosičem vedle soupeře přišel míč v dalším tahu v 33 z 84
        //   případů (39 %), jinak v 9 %. Teď se shazuje soupeř na KTERÉMKOLI z osmi polí.
        std::vector<Macro> macros;
        getAvailableMacros(state, macros, config_.dauntlessInOffer);
        std::vector<int> usedAttackers;
        for (const Position& d : carrier.position.getAdjacent()) {
            if (!d.isOnPitch()) continue;
            if (!cageFeatureOn(kFeatMarkers) && (std::abs(d.x - carrier.position.x) != 1 ||
                                                 std::abs(d.y - carrier.position.y) != 1)) continue;
            const Player* opp = state.getPlayerAtPosition(d);
            if (!opp || opp->teamSide == carrier.teamSide ||
                opp->state != PlayerState::STANDING) continue;
            // P169 krok 8 (07.10.2026): KDO HO ODTLAČÍ PRYČ OD NOSIČE. Dosud vyhrál hráč s Block,
            // pak síla — bez ohledu na to, kam odtlačení povede. Soupeř se odtlačuje směrem OD
            // útočníka: stojí-li útočník „za“ ním, tlačí ho k nosiči nebo kolem něj a soupeř u
            // nosiče zůstane (31 z 36 tahů s nosičem vedle soupeře: stál u něj už na začátku,
            // ve 20 z nich řadič ránu zahrál a soupeř tam byl dál). Pořadí: (1) po odtlačení
            // už u nosiče nestojí, (2) víc kostek pro nás, (3) Block, (4) síla.
            auto clearsCarrier = [&](const Player& a) {
                const int ddx = opp->position.x - a.position.x, ddy = opp->position.y - a.position.y;
                for (int rot = -1; rot <= 1; ++rot) {
                    // směr odtlačení a jeho dva sousedé (o 45°)
                    static const int ring[8][2] = {{1,0},{1,1},{0,1},{-1,1},{-1,0},{-1,-1},{0,-1},{1,-1}};
                    int at = 0;
                    for (int i = 0; i < 8; ++i) if (ring[i][0] == ddx && ring[i][1] == ddy) at = i;
                    const int k = (at + rot + 8) % 8;
                    const Position dest{static_cast<int8_t>(opp->position.x + ring[k][0]),
                                        static_cast<int8_t>(opp->position.y + ring[k][1])};
                    if (!dest.isOnPitch() || state.getPlayerAtPosition(dest)) continue;
                    if (dest.distanceTo(carrier.position) >= 2) return true;
                }
                return false;
            };
            auto rank = [&](const Player& a) {
                return 1000 * (cageFeatureOn(kFeatMarkers) && clearsCarrier(a) ? 1 : 0) + 100 * std::clamp(blockDiceCount(state, a, *opp), -3, 3) +
                       10 * (a.hasSkill(SkillName::Block) ? 1 : 0) + a.stats.strength;
            };
            const Macro* pickBlock = nullptr;
            for (const Macro& m : macros) {
                if (m.type != MacroType::BLOCK || m.targetId != opp->id) continue;
                if (m.playerId == carrier.id) continue;   // nosič nebojuje
                if (std::find(usedAttackers.begin(), usedAttackers.end(), m.playerId) !=
                    usedAttackers.end()) continue;
                const Player& a = state.getPlayer(m.playerId);
                if (!pickBlock || rank(a) > rank(state.getPlayer(pickBlock->playerId))) pickBlock = &m;
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
            for (const Position& sq : carrier.position.getAdjacent()) { // soupeř vedle nosiče (i na rohu)
                if (!sq.isOnPitch()) continue;
                if (!cageFeatureOn(kFeatMarkers) && (sq.x == carrier.position.x || sq.y == carrier.position.y)) continue;
                const Player* o = state.getPlayerAtPosition(sq);
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
                        if (done || m.id == a.id || m.id == carrier.id || !cageMayMove(m) || used(m.id)) return;   // pomocník se přesouvá ⇒ ne Wild Animal (P211b)
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
        // ⭐⭐ P172 — PRVNÍ FÁZE CELOTAHU (uživatel 07.10.2026: „první bude klec — kde bude v obsahu
        //   i blitz pro proboření obrany nebo blocky na uvolnění klece — pak se provede celý pohyb
        //   klece s nosičem — pak ostatní věci — např. blitz, když nebyl proveden — nakonec kdyžtak
        //   faul“). BLITZ NA PROBOŘENÍ OBRANY: když rány výš blitz nespotřebovaly, zkusí se, jestli
        //   by shození některého soupeře v cestě klece dalo lepší postup (víc stojících rohů, pak
        //   delší krok) než bez něj. Jen bezpečný blitz: hráč s Block, doběh bez hodu a bez GFI,
        //   2+ kostky, které vybíráme my; roh klece ani nosič neblitzují. Postup klece se pak
        //   plánuje znovu na desce po ráně (planAdvance).
        {
            const TeamSide side = carrier.teamSide;
            const int fdx = (side == TeamSide::HOME) ? 1 : -1;
            bool blitzQueued = state.getTeamState(side).blitzUsedThisTurn;
            for (const Macro& q : queue_) blitzQueued |= (q.type == MacroType::BLITZ);
            auto planScore = [](const CageAdvancePlan& p) {
                return p.valid ? 100 * (p.filledCorners - p.gfiCorners) + p.step : -1;
            };
            if (!blitzQueued && cageFeatureOn(kFeatPhaseOneBlitz)) {
                std::vector<const Player*> inPath;
                state.forEachOnPitch(opponent(side), [&](const Player& o) {
                    if (o.state != PlayerState::STANDING) return;
                    const int ahead = (o.position.x - carrier.position.x) * fdx;
                    if (ahead < 1 || ahead > static_cast<int>(carrier.movementRemaining) + 2) return;
                    if (std::abs(o.position.y - carrier.position.y) > 3) return;
                    inPath.push_back(&o);
                });
                std::sort(inPath.begin(), inPath.end(), [&](const Player* a, const Player* b) {
                    return a->position.distanceTo(carrier.position) < b->position.distanceTo(carrier.position);
                });
                if (inPath.size() > 3) inPath.resize(3);
                const int base = planScore(pre);
                int bestScore = base, bestBlitzer = -1, bestTarget = -1;
                for (const Player* o : inPath) {
                    // Napřed BEZ plánování vybrat jednoho blitzujícího na cíl (nejvíc kostek, pak
                    // nejkratší doběh) — plán klece se pak staví jen jednou na cíl. (První verze
                    // stavěla plán pro každou dvojici hráč × cíl: sada testů 90 s → 32 min.)
                    int pickId = -1, pickDice = 0, pickSteps = 99;
                    Position pickLanding{-1, -1};
                    state.forEachOnPitch(side, [&](const Player& b) {
                        if (b.id == carrier.id || !freeToAct(b) || !b.hasSkill(SkillName::Block)) return;
                        for (const Macro& q : queue_) if (q.playerId == b.id) return;
                        if (std::abs(b.position.x - carrier.position.x) == 1 &&
                            std::abs(b.position.y - carrier.position.y) == 1) return;      // roh neblitzuje
                        if (countTacklezones(state, b.position, side, b.id) > 0) return;    // jen volný hráč
                        Position landing = b.position;
                        int steps = 0;
                        if (b.position.distanceTo(o->position) > 1) {
                            if (!blitzLandingDiceFree(state, b, o->position, landing, steps)) return;
                            if (steps > static_cast<int>(b.movementRemaining) - 1) return;
                        }
                        GameState at = state.clone();
                        at.getPlayer(b.id).position = landing;
                        const int dice = blockDiceCount(at, at.getPlayer(b.id), at.getPlayer(o->id));
                        if (dice < 2) return;
                        if (dice > pickDice || (dice == pickDice && steps < pickSteps)) {
                            pickId = b.id; pickDice = dice; pickSteps = steps; pickLanding = landing;
                        }
                    });
                    if (pickId < 0) continue;
                    GameState trial = state.clone();
                    Player& tb = trial.getPlayer(pickId);
                    tb.position = pickLanding;
                    tb.hasMoved = true;                                   // do klece už nepůjde
                    trial.getPlayer(o->id).state = PlayerState::PRONE;    // sražený soupeř zónu nemá
                    const int sc = planScore(planner_.build(trial, {}, scoringRangeCage_));
                    if (sc > bestScore) { bestScore = sc; bestBlitzer = pickId; bestTarget = o->id; }
                }
                if (bestBlitzer > 0) {
                    if (dbg) std::fprintf(stderr, "[cage ctl] blitz na proboření: hráč %d na %d, plán %d -> %d\n",
                                          bestBlitzer, bestTarget, base, bestScore);
                    queue_.push_back(Macro{MacroType::BLITZ, bestBlitzer, bestTarget, {-1, -1}});
                }
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
        run.gfiAllowance = carrierGfiFor(state, carrier, dest);
        queue_.push_back(run);
    }
    stage_ = Stage::AFTER_RUN;
}

// P154 (b): smí hledání zahrát tohle makro? Ne, když by rozebralo klec: (1) odvedlo roh klece,
// (2) pohnulo nosičem, kterého řadič v tomto tahu drží
// v kleci (postup nevyšel a čekáním o TD nepřijde) — kromě maker, která skórují nebo míč
// předávají, a kromě nosiče, který sám stojí v zóně soupeře (ústup z kontaktu má přednost:
// „blok na nosiče se nesmí stávat vůbec").
// P217 (1) (uživatel 19.10. k bezpečné kleci, ze které nosič dojde do zóny bez hodu: „pokud to jde —
// přibližujeme se k TD zóně“; k bezpečné kleci obecně: „postupujeme dopředu“ = bankování, doktrína
// 04.08./10.08. „druhé kolo bankuj a prorážej cestu kupředu“). Dosud zdržování (P175) klec jen dostavělo
// na místě a nosič stál. Teď klec postoupí stejným plánem jako v bankování (nejdelší krok bez hodu),
// pokud nosič zůstane před zónou (TD se v tomto tahu nehraje) a po kroku je dál v bezpečí. Jinak
// dostavba na místě jako dřív.
CageAdvancePlan CageController::stallPlan(const GameState& state, const Player& carrier) {
    if (extraFeatureOn(kP217StallBanks)) {
        CageAdvancePlan adv = planner_.build(state, {}, /*evenInScoringRange=*/true);
        if (adv.valid && adv.step > 0 && adv.carrierGfi == 0 &&
            adv.step < distToEndzone(carrier.position, carrier.teamSide) && adv.blitzThreat <= kSafeBlitzThreat) {
            if (std::getenv("BB_CAGE_DEBUG")) {
                std::fprintf(stderr, "[cage ctl] zdržování bankuje: klec o %d pole k zóně (hrozba %.2f)\n", adv.step,
                             adv.blitzThreat);
            }
            return adv;
        }
    }
    return planner_.buildFillOnly(state, {});
}

bool CageController::forbidsCarrierMove(const GameState& state, const Macro& m) const {
    if (!ourBall(state)) return false;
    if (state.activeTeam != team_ || state.getTeamState(team_).turnNumber != turn_ || state.half != half_) return false;
    const Player& carrier = state.getPlayer(state.ball.carrierId);

    // P175: zdržování TD — v tomto tahu nosič neskóruje, nehýbe se a míč nepouští.
    if (stalling_) {
        const bool scoring = m.type == MacroType::SCORE || m.type == MacroType::BLITZ_AND_SCORE ||
                             m.type == MacroType::HAND_OFF_SCORE || m.type == MacroType::PASS_SCORE ||
                             m.type == MacroType::CHAIN_SCORE || m.type == MacroType::PASS_ACTION;
        const bool carrierActs = m.playerId == carrier.id &&
                                 (m.type == MacroType::ADVANCE || m.type == MacroType::REPOSITION);
        if (scoring || carrierActs || m.type == MacroType::ADVANCE) return true;
    }

    // P180: míč v bezpečí a tým má čas ⇒ TD se nehraje ani přes spoluhráče
    if (mateStall_ && (m.type == MacroType::HAND_OFF_SCORE || m.type == MacroType::PASS_SCORE ||
                       m.type == MacroType::CHAIN_SCORE || m.type == MacroType::PASS_ACTION)) return true;
    // P180: hráče připraveného pro předávku hledání neodvádí
    if (m.type == MacroType::REPOSITION &&
        std::find(readyMates_.begin(), readyMates_.end(), m.playerId) != readyMates_.end() &&
        m.targetPos != state.getPlayer(m.playerId).position) return true;

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

    // ⭐ P169 (07.10.2026): ROH, KTERÝ STOJÍ, V TOMTO TAHU KLEC NEOPOUŠTÍ ANI PÁDEM. Změřeno
    //   (trpaslíci, 80 poločasů): ve 20 tazích stála na začátku klec se čtyřmi rohy, nosič
    //   se nehnul a po tahu měla rohů méně — 15× s turnoverem: rohový hráč blokoval a spadl,
    //   nebo fauloval. Roh proto smí jen BEZPEČNÝ blok (2+ kostky, které vybíráme my, a má
    //   Block; bez Frenzy — to hlídá podmínka výš) a nefauluje. Blitz rohu hlídá expandBlitz.
    if (cageFeatureOn(kFeatCornersPassive) && phase_ == CagePhase::CAGE && m.playerId > 0 &&
        m.playerId != carrier.id && (m.type == MacroType::BLOCK || m.type == MacroType::FOUL)) {
        const Player& p = state.getPlayer(m.playerId);
        const bool isCorner = p.teamSide == carrier.teamSide && p.state == PlayerState::STANDING &&
                              std::abs(p.position.x - carrier.position.x) == 1 &&
                              std::abs(p.position.y - carrier.position.y) == 1;
        if (isCorner) {
            if (m.type == MacroType::FOUL) return true;
            const bool safe = m.targetId > 0 && p.hasSkill(SkillName::Block) &&
                              blockDiceCount(state, p, state.getPlayer(m.targetId)) >= 2;
            if (!safe) return true;
        }
    }

    // P190 (rozbor skavenů 09.10.2026): nosič, který je v bezpečí, míč nepouští přihrávkou —
    // v měření přihrál z klece hráči stojícímu v zóně soupeře. (Přihrávka má vlastní sekci později.)
    if (cageFeatureOn(kFeatKeepBuiltCage) && phase_ == CagePhase::CAGE && m.type == MacroType::PASS_ACTION &&
        blitzThreat(state, carrier, kSafeBlitzThreat) <= kSafeBlitzThreat) return true;

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
    // P169 krok 3 (07.10.2026): „klec nemá ani tři rohy“ už nosiče hledání neuvolňuje — pole
    // pro nosiče bez klece vybírá řadič (planAdvance). Výjimky níž zůstávají.
    if (!why && corners < 3 && !cageFeatureOn(kFeatCarrierByCtl)) why = "klec nemá ani tři rohy";
    // P173: NOSIČ SMÍ JÍT TAM, KDE JE BEZPEČNĚJI NEŽ TADY — pro každou rasu stejně. Makro se
    // zkusí nanečisto (osm sad kostek): pád nebo ztráta míče = riziko 1, TD = 0, jinak hrozba
    // rány (carrierThreatAt) na poli, kde skončil, s rohy, které tam ještě dojdou. Tady = totéž
    // na poli, kde stojí. Rychlý tým tak uteče z dosahu (riziko 0), pomalý zůstane
    // v kleci — a nosič s jedním rohem nejde „kamkoli dopředu“, kde je to stejně zlé.
    if (!why && cageFeatureOn(kFeatCarrierByCtl)) {
        const double here = carrierThreatAt(state, carrier, carrier.position);
        double there = 0.0;
        bool endsOnEdge = false;
        const int samples = 8;
        for (int k = 0; k < samples; ++k) {
            GameState trial = state.clone();
            DiceRoller dice(static_cast<uint32_t>(7919 * (k + 1) + 31 * carrier.position.x + carrier.position.y +
                                                  101 * state.getTeamState(team_).turnNumber));
            const int scoreBefore = trial.homeTeam.score + trial.awayTeam.score;
            const MacroExpansionResult r = greedyExpandMacro(trial, m, dice);
            const bool scored = trial.homeTeam.score + trial.awayTeam.score > scoreBefore;
            const bool stillOurs = trial.ball.isHeld && trial.ball.carrierId > 0 &&
                                   trial.getPlayer(trial.ball.carrierId).teamSide == carrier.teamSide;
            if (scored) continue;
            if (r.turnover || !stillOurs) {
                // P176: pád není vždy ztráta míče — záleží, kdo je u něj (změřeno, looseBallLossRisk)
                const bool loose = !trial.ball.isHeld && trial.ball.position.isOnPitch();
                there += (loose && cageFeatureOn(kFeatFallValue))
                             ? carrierFallCost(trial, carrier, trial.ball.position) : 1.0;
                continue;
            }
            const Player& c2 = trial.getPlayer(trial.ball.carrierId);
            there += carrierThreatAt(trial, c2, c2.position);
            // krajní řádky: rohy klece tam nemají kam stát (a hrozí vyhození do davu)
            if (cageFeatureOn(kFeatKeepBuiltCage) && cageFeatureOn(kFeatSideline) &&
                (c2.position.y < 2 || c2.position.y > 12) &&
                std::abs(c2.position.y - 7) >= std::abs(carrier.position.y - 7)) endsOnEdge = true;
        }
        there /= samples;
        // ⭐ P190 (rozbor skavenů 09.10.2026): hledání odvádělo nosiče z DOSTAVĚNÉ klece pro zisk
        //   dvou setin („tady 0,05, po pohybu 0,03“) — klec měla 4 rohy, po pohybu 2. Stojí-li
        //   kolem nosiče aspoň tři rohy, musí být zisk znatelný (0,15 = rozdíl třídy rány);
        //   jinak platí dosavadní mez proti kmitání.
        const double margin = (cageFeatureOn(kFeatKeepBuiltCage) && corners >= 3) ? kSafeBlitzThreat : 0.02;
        if (!endsOnEdge && there + margin < here) {
            why = "tam je bezpečněji než tady";
            if (std::getenv("BB_CAGE_DEBUG")) {
                std::fprintf(stderr, "[cage ctl] riziko ztráty míče: tady %.2f, po pohybu %.2f\n", here, there);
            }
        }
    }
    // P169 krok 6: jen když sólo TD ještě stihne; nestihne-li ho ani tak, nosič z klece nejde.
    if (!why && waitingCostsTheTouchdown(state, carrier) &&
        (!cageFeatureOn(kFeatTimeRun) || decideRelease(state, carrier, planner_).soloMakesIt)) why = "čekáním by nestihl TD";
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
static Position laggardSquare(const GameState& state, const Player& p, Position cp, const std::vector<Position>& taken);

// P211b: Wild Animal zaostalec dojde až před koncem tahu — tedy po blitzu, pokud se hrál. Zůstal-li
// týmový blitz nevyužitý, přesun ho ohlásí (+2 k hodu, ř. 8666-8669); táž primitivní nabídka jako
// u hledání (BLITZ bez cíle, rules_engine.cpp).
static void wildAnimalLaggards(const GameState& state, std::vector<Macro>& queue) {
    const Player& carrier = state.getPlayer(state.ball.carrierId);
    std::vector<Position> taken;
    for (const Macro& m : queue) taken.push_back(m.targetPos);
    std::vector<Action> actions;
    getAvailableActions(state, actions);
    bool blitzGiven = false;   // týmový blitz je jeden: ohlásí ho jen první Wild Animal ve frontě
    state.forEachOnPitch(carrier.teamSide, [&](const Player& p) {
        if (p.id == carrier.id || !freeToAct(p) || !p.hasSkill(SkillName::WildAnimal)) return;
        const Position best = laggardSquare(state, p, carrier.position, taken);
        if (best == p.position) return;
        Macro m{MacroType::REPOSITION, p.id, -1, best};
        m.cageManaged = true;
        m.declareBlitz = !blitzGiven && std::any_of(actions.begin(), actions.end(), [&](const Action& a) {
            return a.type == ActionType::BLITZ && a.playerId == p.id && a.targetId < 0;
        });
        blitzGiven = blitzGiven || m.declareBlitz;
        queue.push_back(m);
        taken.push_back(best);
    });
}

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
    // Review 08.10.2026: ve zdržovacím tahu (P175) nosič stojí — i tady. Dosud se před koncem tahu
    // volal postup klece a nosič mohl popojít na pole s hrozbou do 0,15 (zdržuje se jen do 0,05).
    const bool stallingNow = stalling_ && team_ == state.activeTeam && turn_ == turn && half_ == state.half;
    if (freeToAct(carrier) && !released(state, carrier) && !stallingNow) {
        plan = planner_.build(state, {}, /*evenInScoringRange=*/true);
    } else if (stallingNow && freeToAct(carrier)) {
        plan = stallPlan(state, carrier);
    } else {
        plan = planner_.buildFillOnly(state, {});
    }
    if (std::getenv("BB_CAGE_DEBUG")) {
        std::fprintf(stderr, "[cage ctl] před koncem tahu: verdikt %d, platný %d, krok %d, maker %zu\n",
                     static_cast<int>(plan.verdict), plan.valid, plan.step, plan.macros.size());
    }
    std::vector<Macro> q;
    if (plan.valid) q = std::move(plan.macros);
    const bool adoptPlan = !q.empty();
    wildAnimalLaggards(state, q);
    if (q.empty()) return false;
    // next() pozná nový tah podle těchto tří polí a frontu by zahodil — nesmí záležet na tom,
    // jestli se v tomto tahu už volal
    team_ = state.activeTeam;
    turn_ = turn;
    half_ = state.half;
    queue_ = std::move(q);
    idx_ = 0;
    stage_ = Stage::DONE;
    phase_ = CagePhase::CAGE;
    if (adoptPlan) ++adopted_;
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
    // ⭐ P169 krok 6 (07.10.2026): VÝBĚH NA KONCI POLOČASU VOLÍ ŘADIČ, NE HLEDÁNÍ. Když by
    //   nosič ani po kroku klece nestihl TD („čekáním by nestihl“), hledání ho dosud smělo vést
    //   samo — kamkoli dopředu, i k soupeři a přes GFI: 14 takových tahů skončilo s 0–1 rohem a
    //   9× o míč hned přišel (trpaslíci, 80 poločasů). Teď stejný výběh jako ve fázi 3: co
    //   nejdál BEZ HODU, ne vedle soupeře, na pole, kam dosáhne nejméně soupeřů; spoluhráči pak
    //   jdou markovat ty, kdo na něj dosáhnou.
    // Jen když sólo od TOHOTO tahu TD ještě stihne — jinak výběh nic nezíská a klec míč chrání.
    if (cageFeatureOn(kFeatTimeRun) && waitingCostsTheTouchdown(state, carrier) &&
        decideRelease(state, carrier, planner_).soloMakesIt) {
        const int ma = std::max(1, static_cast<int>(carrier.stats.movement));
        const int turnsAfterThis = std::clamp(8 - state.getTeamState(carrier.teamSide).turnNumber, 0, 8);
        const int rest = distToEndzone(carrier.position, carrier.teamSide) - (plan.valid ? plan.step : 0) - (ma + 2);
        const int soloNeedAfterStep = (rest > 0 ? (rest + ma - 1) / ma : 0) + 1;
        if (soloNeedAfterStep > turnsAfterThis) {
            phase_ = CagePhase::RELEASE;
            releasedCarrier_ = carrier.id;
            releasedHalf_ = state.half;
            releasedScore_ = state.homeTeam.score + state.awayTeam.score;
            const Position dest = farthestSafeForward(state, carrier, carrier.movementRemaining);
            if (std::getenv("BB_CAGE_DEBUG")) {
                std::fprintf(stderr, "[cage ctl] výběh kvůli času: nosič (%d,%d) -> (%d,%d)\n",
                             carrier.position.x, carrier.position.y, dest.x, dest.y);
            }
            if (dest != carrier.position) {
                Macro run{MacroType::REPOSITION, carrier.id, -1, dest};
                run.cageManaged = true;
                run.gfiAllowance = carrierGfiFor(state, carrier, dest);
                queue_ = {run};
            }
            stage_ = Stage::AFTER_RUN;
            return;
        }
    }
    // ⭐ P154 (b) (uživatel 07.10.2026: „nosič dál jen s klecí“; obklíčená klec: „zůstat stát
    //   a uvolňovat ranami“; „pokud je ta situace na konci poločasu — nosič musí vyběhnout,
    //   aby stihl TD“). Postup klece nevyšel ⇒ nosič tento tah z klece nevybíhá — LEDAŽE by
    //   čekáním přišel o TD: sólo od příštího tahu potřebuje ceil((dist − (MA+2)) / MA) + 1
    //   tahů; nevejde-li se to do zbývajících, běží už teď.
    //
    // ⭐ P169 krok 3 (07.10.2026): NOSIČ SE HÝBE JEN TAM, KDE KOLEM NĚJ VZNIKNE KLEC. Když plán
    //   nevyšel nebo má méně než tři rohy, hledání dosud smělo nosičem pohnout samo („klec nemá
    //   ani tři rohy“) — a pohnulo jím kamkoli dopředu: klec pak byla čistá v 1 tahu z 21 a
    //   všech 11 ztrát míče ve vlastním tahu trpaslíků (160 poločasů) bylo právě tímhle
    //   pohybem. Teď pole vybírá řadič: bez hodu, ne vedle soupeře, ne dozadu, a to, kolem
    //   kterého se ještě v tomto tahu postaví nejvíc rohů (při shodě nejdál). Jde tam jen,
    //   když tím rohů přibude; rohy se pak dostaví jako po zvednutí míče.
    // Sjednoceno 08.10.2026 (review P181 nález 4): rozhoduje HROZBA RÁNY, ne počet rohů. Plán,
    // po kterém je nosič v bezpečí (i se dvěma rohy), platí; dřív ho „méně než tři rohy“ přebilo
    // a nosič šel sám jinam podle tabulky rohů.
    // P229: zeď přímo před klecí rozhoduje dřív než plán postupu — ten by pomalejší klec poslal
    // kolem zdi bokem, a to doktrína nechce (zeď ji dožene). Prolomení nejde ⇒ dál jako dřív.
    if (extraFeatureOn(kP229WallBreak) && planWallBreak(state, carrier)) {
        stage_ = Stage::AFTER_ADVANCE;         // nosič tento tah stojí; pak zaostalci
        return;
    }
    const bool planMoves = plan.valid && plan.step >= 1;
    const bool planSafe = planMoves && plan.blitzThreat <= kSafeBlitzThreat;
    // ⭐⭐ P190 (rozbor skavenů 09.10.2026: 61 % chybějících rohů = nosič začal tah V KONTAKTU se
    //   soupeřem, pole rohů drží soupeř nebo leží v jeho zónách, a řadič nosiči nedovolil nic —
    //   bez hodu z kontaktu odejít nejde; uživatel 08.10.: „u agilních týmů bude převažovat dodge
    //   a útěk daleko — bude u každého z týmů vyhodnoceno něco jiného jako bezpečnější“; „když
    //   nosič nemůže skórovat ani být v bezpečí — nesmí nastat“). Nosič stojí v zóně soupeře a
    //   plán klece ho neochrání ⇒ smí z kontaktu ÚHYBEM tam, kde je po tahu bezpečněji — kamkoli,
    //   i dozadu ke spoluhráčům. Cena = šance, že úhyb nevyjde × cena pádu + jinak hrozba rány
    //   na cílovém poli (s rohy, které tam dojdou). Jde jen, když je to znatelně lepší než stát.
    //   Z čísel hráče, ne z rasy: obratný s Dodge uhne skoro zadarmo, neohrabaný zůstane a bije se.
    if (cageFeatureOn(kFeatEscapeContact) && !planSafe &&
        countTacklezones(state, carrier.position, carrier.teamSide, carrier.id) > 0) {
        const TeamSide side = carrier.teamSide;
        const double here = planMoves ? plan.blitzThreat : carrierThreatAt(state, carrier, carrier.position);
        const double fallCost = carrierFallCost(state, carrier, carrier.position);
        const int budget = static_cast<int>(carrier.movementRemaining);
        struct Esc { Position sq; double fail; int back; };
        std::vector<Esc> escs;
        for (int x = carrier.position.x - budget; x <= carrier.position.x + budget; ++x) {
            for (int y = std::max(0, carrier.position.y - budget); y <= std::min(14, carrier.position.y + budget); ++y) {
                const Position sq{static_cast<int8_t>(x), static_cast<int8_t>(y)};
                if (!sq.isOnPitch() || sq == carrier.position || state.getPlayerAtPosition(sq)) continue;
                if (cageFeatureOn(kFeatSideline) && (sq.y < 2 || sq.y > 12) &&
                    std::abs(sq.y - 7) >= std::abs(carrier.position.y - 7)) continue;
                if (countTacklezones(state, sq, side, carrier.id) > 0) continue;
                const double fail = pathFailProb(state, carrier, sq, budget, Position{-1, -1});
                // nedojde / úhyb sám je dražší než stát / hod horší než 2+ (nevyjde častěji než jednou
                // ze šesti): neúspěch je turnover na ZAČÁTKU tahu, zbytek týmu by nehrál vůbec
                if (fail < 0.0 || fail * fallCost >= here || fail > 1.0 / 6.0 + 1e-9) continue;
                escs.push_back({sq, fail, distToEndzone(sq, side) - distToEndzone(carrier.position, side)});
            }
        }
        // napřed nejlevnější úhyb, pak co nejmenší ústup
        std::sort(escs.begin(), escs.end(), [](const Esc& a, const Esc& b) {
            if (std::abs(a.fail - b.fail) > 1e-9) return a.fail < b.fail;
            return a.back < b.back;
        });
        double bestCost = here;
        Position bestSq{-1, -1};
        std::tuple<int, int, int> bestTie{0, 0, 0};
        int costly = 0;                         // drahý výpočet hrozby jen pro pole v dosahu soupeře, nejvýš 30×
        for (const Esc& e : escs) {
            if (e.fail * fallCost >= bestCost) continue;
            const bool reachable = anyOpponentReaches(state, side, e.sq);
            if (reachable && ++costly > 30) continue;
            const double cost = e.fail * fallCost +
                                (reachable ? (1.0 - e.fail) * carrierThreatAt(state, carrier, e.sq) : 0.0);
            // při stejné ceně: kam dojde víc rohů, pak menší ústup, pak blíž středu
            const std::tuple<int, int, int> tie{-cornersWithinReach(state, carrier, e.sq), e.back, std::abs(e.sq.y - 7)};
            if (cost < bestCost - 1e-9 || (bestSq.isOnPitch() && std::abs(cost - bestCost) <= 1e-9 && tie < bestTie)) {
                bestCost = cost;
                bestSq = e.sq;
                bestTie = tie;
            }
        }
        if (bestSq.isOnPitch() && bestCost + 0.05 < here) {
            if (std::getenv("BB_CAGE_DEBUG")) {
                std::fprintf(stderr, "[cage ctl] nosič z kontaktu úhybem: (%d,%d) -> (%d,%d), riziko %.2f -> %.2f\n",
                             carrier.position.x, carrier.position.y, bestSq.x, bestSq.y, here, bestCost);
            }
            Macro run{MacroType::REPOSITION, carrier.id, -1, bestSq};
            run.cageManaged = true;
            queue_ = {run};
            stage_ = Stage::AFTER_PICKUP;      // pak dostavět rohy kolem nového místa, pak zaostalci
            return;
        }
    }
    if (!planSafe && cageFeatureOn(kFeatCarrierByCtl)) {
        const Position dest = farthestSafeForward(state, carrier, carrier.movementRemaining, /*forCage=*/true);
        // jde tam jen, když je tam bezpečněji (útěk z dosahu soupeře i klec se počítají stejně)
        const double here = planMoves ? plan.blitzThreat : carrierThreatAt(state, carrier, carrier.position);
        const double there = carrierThreatAt(state, carrier, dest);
        // P190: nosič, který je tam, kde stojí, v bezpečí (hrozba ≤ 0,15), sám neodbíhá — ani když
        // by o pár polí dál byl z dosahu úplně („do té doby má být v kleci“). Odběhl by spoluhráčům
        // a příští tah by klec nebylo z čeho postavit. Týž princip jako u hledání (forbidsCarrierMove).
        const bool safeHere = cageFeatureOn(kFeatKeepBuiltCage) && here <= kSafeBlitzThreat;
        if (dest != carrier.position && !safeHere && there + 0.02 < here) {
            if (std::getenv("BB_CAGE_DEBUG")) {
                std::fprintf(stderr, "[cage ctl] nosič ke kleci: (%d,%d) -> (%d,%d), riziko %.2f -> %.2f\n",
                             carrier.position.x, carrier.position.y, dest.x, dest.y, here, there);
            }
            Macro run{MacroType::REPOSITION, carrier.id, -1, dest};
            run.cageManaged = true;
            run.gfiAllowance = carrierGfiFor(state, carrier, dest);
            queue_ = {run};
            stage_ = Stage::AFTER_PICKUP;      // pak dostavět rohy kolem nového místa, pak zaostalci
            return;
        }
    }
    if (plan.valid) queue_ = std::move(plan.macros);
}

// ⭐ P229 (uživatel 26.10.2026: „když jsme silnější — dáme asistenty na 13,5 a 13,9 a přední rohy
//   block dopředu a pak blitz dopředu z 13,7 a skrz bariéru — když jsme rychlejší, ostatní kromě
//   předních rohů obíhají a pak se přesunou i bývalé přední rohy s dodge“; upřesnění: „silnější musíš
//   spočítat podle síly předních rohů a asistencí s případným guard — rychlejší spočítáš z MA — pozor
//   na průměr — počítej jen část týmu — trpaslíci proti orkům … jsou pomalejší — tak nemohou obíhat —
//   i když nespočítáš, že jsou silnější“).
//   Zeď = stojící soupeři ve sloupci dva kroky před nosičem (řádky nosiče ±2); musí stát aspoň před
//   oběma předními rohy, nebo před jedním a před nosičem.
//   RYCHLEJŠÍ = nejpomalejší z těch, kdo by obíhali (nosič a zadní rohy — přední rohy zůstávají a
//   přejdou až potom), má MA větší než nejrychlejší hráč zdi. Ne průměr týmu: u ještěrů by rychlí
//   skinkové zakryli pomalé sauruse, u zdi zase jeden pomalý zakryl rychlého.
//   SILNĚJŠÍ = každý přední roh má na soupeře před sebou aspoň 2 kostky, které volíme my — počítáno
//   s asistenty už na křídlech (asistence a Guard jako ve hře, `blockDiceCount`).
//   Prolamuje se, když jsme silnější, nebo nejsme rychlejší; jinak (rychlejší, ne silnější) rozhoduje
//   dosavadní řadič — útěk nosiče s půlkou klece (P228).
//   Pořadí jako v doktríně: (1) asistenti na křídla (vedle předních rohů, ven od nosiče) — napřed hráči
//   mimo klec, pak zadní rohy; jen bez hodu. Křídlo stojí v zóně krajního hráče zdi, takže sám
//   neasistuje, ale tím, že ho drží, ruší jeho asistenci soupeři. (2) bloky předních rohů dopředu —
//   ne tam, kde by volil obránce, a ne roh s Frenzy (P169). (3) blitz na soupeře před nosičem; blitzuje hráč, který bez hodu
//   dojde na pole před nosičem (jiné pole bez hodu jen, když tam nedojde nikdo). Nosič tento tah stojí.
//   Chybí-li přední rohy, pomalejší klec se dostaví na místě (prolomí příští tah); plánovač by ji
//   jinak s P228 poslal kolem zdi bokem nebo dozadu. Když nejde blok ani blitz, prolomení se
//   nehraje a rozhoduje dosavadní řadič.
bool CageController::planWallBreak(const GameState& state, const Player& carrier) {
    const TeamSide side = carrier.teamSide;
    const int dir = side == TeamSide::HOME ? 1 : -1;
    const Position cp = carrier.position;
    auto at = [&](int dx, int dy) { return Position{static_cast<int8_t>(cp.x + dx * dir), static_cast<int8_t>(cp.y + dy)}; };
    auto ours = [&](Position sq) -> const Player* {
        if (!sq.isOnPitch()) return nullptr;
        const Player* p = state.getPlayerAtPosition(sq);
        return p && p->teamSide == side && p->state == PlayerState::STANDING ? p : nullptr;
    };
    auto wallAt = [&](Position sq) -> const Player* {
        if (!sq.isOnPitch()) return nullptr;
        const Player* p = state.getPlayerAtPosition(sq);
        return p && p->teamSide != side && p->state == PlayerState::STANDING && !p->lostTacklezones ? p : nullptr;
    };

    // nosič vedle soupeře: napřed P190 (úhyb z kontaktu) / rány řadiče — zeď počká (review 27.10. nález 1)
    if (countTacklezones(state, cp, side, carrier.id) > 0) return false;
    // čekáním by nosič nestihl TD: rozhoduje výběh na konci poločasu, ne prolamování (review 27.10.)
    if (cageFeatureOn(kFeatTimeRun) && waitingCostsTheTouchdown(state, carrier)) return false;
    const Player* ahead[2] = {wallAt(at(2, -1)), wallAt(at(2, 1))};
    const Player* centre = wallAt(at(2, 0));
    if ((ahead[0] != nullptr) + (ahead[1] != nullptr) + (centre != nullptr) < 2) return false;

    // rychlejší: nosič + zadní rohy proti zdi (řádky nosiče ±2)
    int runnersMin = carrier.stats.movement;
    for (int dy : {-1, 1}) if (const Player* r = ours(at(-1, dy))) runnersMin = std::min<int>(runnersMin, r->stats.movement);
    int wallMax = 0;
    for (int dy = -2; dy <= 2; ++dy) if (const Player* w = wallAt(at(2, dy))) wallMax = std::max<int>(wallMax, w->stats.movement);
    const bool faster = runnersMin > wallMax;

    // přední rohy chybí: pomalejší klec zeď neobchází (plánovač by ji s P228 poslal bokem nebo
    // dozadu) — dostaví se na místě a prolamovat bude příští tah
    const Player* front[2] = {ours(at(1, -1)), ours(at(1, 1))};
    if (!front[0] || !front[1]) {
        if (faster) return false;
        CageAdvancePlan fill = planner_.buildFillOnly(state, {});
        // jen dostavba, která přední roh opravdu doplní (review 27.10. nález 9) — jinak rozhoduje plánovač
        const bool fillsFront = fill.valid && std::any_of(fill.macros.begin(), fill.macros.end(), [&](const Macro& m) {
            return m.targetPos == at(1, -1) || m.targetPos == at(1, 1);
        });
        if (!fillsFront) return false;
        if (std::getenv("BB_CAGE_DEBUG")) {
            std::fprintf(stderr, "[cage ctl] P229 zeď před klecí: běžci MA %d × zeď MA %d (ne rychlejší), přední rohy chybí ⇒ dostavba na místě (%zu)\n",
                         runnersMin, wallMax, fill.macros.size());
        }
        queue_ = std::move(fill.macros);
        return true;
    }

    // asistenti na křídla: napřed hráči mimo klec, pak zadní rohy; jen bez hodu
    std::vector<int> used{carrier.id, front[0]->id, front[1]->id};
    auto isUsed = [&](int id) { return std::find(used.begin(), used.end(), id) != used.end(); };
    auto inCage = [&](const Player& p) { return p.position.distanceTo(cp) == 1; };
    std::vector<Macro> wings;
    GameState after = state;                      // deska s asistenty na křídlech (pro kostky bloků)
    for (int dy : {-2, 2}) {
        const Position sq = at(1, dy);
        if (!sq.isOnPitch() || state.getPlayerAtPosition(sq)) continue;
        const Player* best = nullptr;
        std::tuple<int, int> bestKey{INT_MAX, INT_MAX};
        state.forEachOnPitch(side, [&](const Player& p) {
            if (!cageMayMove(p) || isUsed(p.id)) return;
            const double fail = pathFailProb(state, p, sq, static_cast<int>(p.movementRemaining), Position{-1, -1});
            if (fail < 0.0 || fail > 1e-9) return;
            const std::tuple<int, int> key{inCage(p) ? 1 : 0, p.position.distanceTo(sq)};
            if (key < bestKey) { bestKey = key; best = &p; }
        });
        if (!best) continue;
        used.push_back(best->id);
        Macro m{MacroType::REPOSITION, best->id, -1, sq};
        m.cageManaged = true;
        wings.push_back(m);
        Player& moved = after.getPlayer(best->id);
        moved.position = sq;
    }

    // silnější: oba přední rohy mají na soupeře před sebou 2+ kostky, které volíme my
    bool stronger = true;
    std::vector<Macro> blocks;
    for (int i = 0; i < 2; ++i) {
        if (!ahead[i]) { stronger = false; continue; }
        const int dice = blockDiceCount(after, after.getPlayer(front[i]->id), after.getPlayer(ahead[i]->id));
        // roh s Frenzy by musel za odsunutým do zdi a nechal díru u nosiče (P169) ⇒ neblokuje
        const bool canBlock = freeToAct(*front[i]) && !frenzyCorner(*front[i], carrier);
        if (dice < 2 || !canBlock) stronger = false;          // silnější = oba rohy opravdu bijí 2+ (nález 12)
        if (dice >= 1 && canBlock) blocks.push_back(Macro{MacroType::BLOCK, front[i]->id, ahead[i]->id, {-1, -1}});
    }
    if (faster && !stronger) return false;       // rychlejší obíhá (útěk podle P228)

    // blitz na soupeře před nosičem
    std::vector<Macro> blitz;
    if (centre && !state.getTeamState(side).blitzUsedThisTurn) {
        const Position frontSq = at(1, 0);
        const Player* best = nullptr;
        std::tuple<int, int, int> bestKey{INT_MAX, INT_MAX, INT_MAX};
        state.forEachOnPitch(side, [&](const Player& p) {
            if (!cageMayMove(p) || isUsed(p.id) || inCage(p)) return;
            Position landing{-1, -1};
            int steps = 0;
            if (!blitzLandingDiceFree(state, p, centre->position, landing, steps)) return;
            if (state.getPlayerAtPosition(landing) && landing != p.position) return;
            // kostky z pole, odkud bude blokovat — asistence soupeře se počítají kolem něj (review 27.10. nález 5)
            GameState sim = state.clone();
            sim.getPlayer(p.id).position = landing;
            const int dice = blockDiceCount(sim, sim.getPlayer(p.id), sim.getPlayer(centre->id));
            if (dice < 1) return;                                 // blitz, kde volí obránce, ne
            const std::tuple<int, int, int> key{landing == frontSq ? 0 : 1, -dice, steps};
            if (key < bestKey) { bestKey = key; best = &p; }
        });
        if (best) blitz.push_back(Macro{MacroType::BLITZ, best->id, centre->id, {-1, -1}});
    }
    if (blocks.empty() && blitz.empty()) return false;

    if (std::getenv("BB_CAGE_DEBUG")) {
        std::fprintf(stderr, "[cage ctl] P229 zeď před klecí: běžci MA %d × zeď MA %d (%s), %s ⇒ prolomení: křídla %zu, bloky %zu, blitz %zu\n",
                     runnersMin, wallMax, faster ? "rychlejší" : "ne rychlejší", stronger ? "silnější" : "ne silnější",
                     wings.size(), blocks.size(), blitz.size());
    }
    queue_ = std::move(wings);
    queue_.insert(queue_.end(), blocks.begin(), blocks.end());
    queue_.insert(queue_.end(), blitz.begin(), blitz.end());
    return true;
}

// Kam zaostalec `p` dojde dopředu bez hodu (P154 c): nejvýš 3 sloupce před nosiče, ne do kontaktu,
// co nejblíž spoluhráči. `p.position` = není kam, nebo `p` zaostalcem není.
static Position laggardSquare(const GameState& state, const Player& p, Position cp, const std::vector<Position>& taken) {
    const TeamSide side = p.teamSide;
    const int dx = (side == TeamSide::HOME) ? 1 : -1;
    if ((cp.x - p.position.x) * dx <= 1) return p.position;               // není zaostalec
    if (countTacklezones(state, p.position, side, p.id) > 0) return p.position;   // vázaný: řeší rány
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
    return best;
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
    // ⭐⭐ P180 — PŘIPRAVENÝ HRÁČ (uživatel 08.10.2026: „nosič doběhne, případně předá nebo hodí někomu
    //   nachystanému dát TD“; „pokud hráč dojde se chystat a nedojde tvořit roh — má se jít
    //   chystat“). Po tahu klece se JEDEN volný hráč (rohem není — rohy už odehrály) postaví na
    //   pole, ze kterého příští tah dojde do zóny bez hodu, kam k němu nosič příští tah dojde
    //   předat, a kde na něj soupeř nedosáhne dobrou ranou. Jen z pohybu, obratnosti a desky —
    //   žádné pravidlo podle rasy; pomalému týmu takové pole většinou nevyjde. Stojí-li už na
    //   takovém poli, zůstane. Jen když nosič sám příští tah do zóny nedojde.
    // Review 08.10. (M3, M4): chystá se nejvýš JEDEN hráč za tah (dostavba po zvednutí míče volá
    // tento krok podruhé) a nikdy ten, kdo ještě může dostavět roh — rohy mají přednost.
    std::vector<int> neededForCorners;
    for (const auto& [id, slot] : cornersComing(state, carrier, cp)) {
        (void)slot;
        neededForCorners.push_back(id);
    }
    if (readyMates_.empty() && cageFeatureOn(kFeatReadyMate) &&
        distToEndzone(cp, side) > static_cast<int>(carrier.stats.movement)) {
        // P180 (9), uživatel 16.10.2026: „preferujeme ne, ale zahrnujeme i ano“ ⇒ první klíč = potřebuje
        //   příjemce na doběh do zóny GFI (0 / 1): pole s GFI se zkusí, až když žádné bez hodu neprojde.
        // P216: první klíč = stupeň rizika nejhorší odpovědi (obstoupení); stupeň 2 (4+ a hůř) jen v záloze níže.
        const bool surround = cageFeatureOn(kFeatReadySurround);
        struct Cand { int id; Position sq; std::tuple<int, int, int, int, int, int> key; };
        std::vector<Cand> cands;
        state.forEachOnPitch(side, [&](const Player& p) {
            if (p.id == carrier.id || !cageMayMove(p) || p.hasSkill(SkillName::NoHands)) return;
            if (countTacklezones(state, p.position, side, p.id) > 0) return;
            if (std::find(neededForCorners.begin(), neededForCorners.end(), p.id) != neededForCorners.end()) return;   // roh (stojí, nebo ho dostaví)
            // P180 (7): „kdo netvoří klec a neblitzují“ — kdo už má v tahu klece jinou akci (blitz, rána, roh), se nechystá
            if (std::any_of(queue_.begin(), queue_.end(), [&](const Macro& q) { return q.playerId == p.id; })) return;
            const int target = std::clamp(calculateCatchTarget(state, p, 1), 2, 6);
            double pc = (7 - target) / 6.0;
            if (p.hasSkill(SkillName::Catch)) pc = 1.0 - (1.0 - pc) * (1.0 - pc);
            const int budget = p.movementRemaining;
            for (int x = p.position.x - budget; x <= p.position.x + budget; ++x) {
                for (int y = std::max(2, p.position.y - budget); y <= std::min(12, p.position.y + budget); ++y) {
                    const Position sq{static_cast<int8_t>(x), static_cast<int8_t>(y)};
                    if (!sq.isOnPitch()) continue;
                    const int toEz = distToEndzone(sq, side);
                    const int gfiNeed = toEz > static_cast<int>(p.stats.movement) ? 1 : 0;
                    if ((gfiNeed && !cageFeatureOn(kFeatReadyMateGfi)) ||
                        toEz > static_cast<int>(p.stats.movement) + maxGfiSquares(p) || toEz < 1) continue;
                    if (sq.distanceTo(cp) - 1 > static_cast<int>(carrier.stats.movement) || sq.distanceTo(cp) < 2) continue;
                    if (sq != p.position) {
                        if (state.getPlayerAtPosition(sq)) continue;
                        if (pathFailProb(state, p, sq, budget, Position{-1, -1}) != 0.0) continue;
                    }
                    if (countTacklezones(state, sq, side, p.id) > 0) continue;
                    int tier = 0;
                    double chance = pc;
                    if (surround) {
                        chance = readyMateChance(state, p, sq, side);
                        tier = riskTier(1.0 - chance);
                    }
                    cands.push_back({p.id, sq, {tier, gfiNeed, threatsTo(state, sq, side), -static_cast<int>(chance * 100.0),
                                                static_cast<int>(sq.distanceTo(cp)), static_cast<int>(sq.distanceTo(p.position))}});
                }
            }
        });
        std::sort(cands.begin(), cands.end(), [](const Cand& a, const Cand& b) { return a.key < b.key; });
        // P180 (7), uživatel 16.10.2026: „u elfů všichni, kdo netvoří klec a neblitzují a dojdou — postaví
        //   se o dvě od sebe, ať obránce nemůže markovat dva jedním svým“; „platí pro agilní spíše než
        //   rychlé“ ⇒ víc připravených jen AG 4+ (první může být kdokoli); mezi nimi aspoň 3 pole, aby
        //   žádné pole nesousedilo s dvěma (jinak by jeden obránce uprostřed markoval oba).
        const bool multi = cageFeatureOn(kFeatReadyMany);
        int tried = 0;
        for (const Cand& c : cands) {
            if (++tried > 24) break;                                  // zkouška hrozby je drahá
            if (std::find(readyMates_.begin(), readyMates_.end(), c.id) != readyMates_.end()) continue;
            if (std::get<0>(c.key) > 1) continue;                    // obklíčený jen v záloze níže
            if (!readyMates_.empty()) {
                if (!multi || state.getPlayer(c.id).stats.agility < 4) continue;
                bool tooClose = false;
                for (int r : readyMates_)
                    for (const Macro& q : queue_)
                        if (q.playerId == r && q.type == MacroType::REPOSITION && q.targetPos.distanceTo(c.sq) < 3) tooClose = true;
                for (int r : readyMates_)
                    if (state.getPlayer(r).position.distanceTo(c.sq) < 3 &&
                        std::none_of(queue_.begin(), queue_.end(), [&](const Macro& q) { return q.playerId == r; })) tooClose = true;
                if (tooClose || std::find(taken.begin(), taken.end(), c.sq) != taken.end()) continue;
            }
            GameState proj = state.clone();
            Player& pp = proj.getPlayer(c.id);
            pp.position = c.sq;
            if (blitzThreat(proj, pp, kSafeBlitzThreat) > kSafeBlitzThreat) continue;
            readyMates_.push_back(c.id);
            if (c.sq != state.getPlayer(c.id).position) {
                Macro m{MacroType::REPOSITION, c.id, -1, c.sq};
                m.cageManaged = true;
                queue_.push_back(m);
            }
            taken.push_back(c.sq);
            if (std::getenv("BB_CAGE_DEBUG")) {
                std::fprintf(stderr, "[cage ctl] připravený hráč %d -> (%d,%d)\n", c.id, c.sq.x, c.sq.y);
            }
            if (!multi) break;                                        // další už jen agilní (výše)
        }
        // ⭐ P216 záloha (uživatel 18.10.: „blitz je jeden a připravených může být více … když nejde nachystat
        //   tak ať jednoho blitznou a další obklíčí — tak je stejně chystej“; „když bude více hráčů volných se
        //   nachystat — tak může být“): žádné pole neprošlo ⇒ chystají se VŠICHNI volní, kdo dojdou, i přes
        //   hrozbu rány a obklíčení; mezi každými dvěma aspoň dvě pole („ať žádné dva nechytí jeden oponent —
        //   např. Rat Ogre s Prehensile Tail nebo Spawn s Tentacles“).
        if (readyMates_.empty() && extraFeatureOn(kP216ReadyAnyway)) {
            for (const Cand& c : cands) {
                if (std::find(readyMates_.begin(), readyMates_.end(), c.id) != readyMates_.end()) continue;
                if (std::any_of(taken.begin(), taken.end(), [&](const Position& t) { return t.distanceTo(c.sq) < 3; })) continue;
                readyMates_.push_back(c.id);
                if (c.sq != state.getPlayer(c.id).position) {
                    Macro m{MacroType::REPOSITION, c.id, -1, c.sq};
                    m.cageManaged = true;
                    queue_.push_back(m);
                }
                taken.push_back(c.sq);
                if (std::getenv("BB_CAGE_DEBUG"))
                    std::fprintf(stderr, "[cage ctl] připravený hráč (záloha) %d -> (%d,%d)\n", c.id, c.sq.x, c.sq.y);
            }
        }
    }
    state.forEachOnPitch(side, [&](const Player& p) {
        if (p.id == carrier.id || !cageMayMove(p) ||
            std::find(readyMates_.begin(), readyMates_.end(), p.id) != readyMates_.end()) return;
        const Position best = laggardSquare(state, p, cp, taken);
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
        if (o.position.distanceTo(cp) - 1 > blitzReachOf(o)) return;
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
        // ⭐ P179 (uživatel 08.10.2026: „vypustíme nosiče dopředu a zkusíme dát souseda všem
        //   protihráčům, co by na něj dosáhli — nejlépe ve směru k nosiči, ať zavazí co nejvíce“).
        //   Dosud se bralo pole u soupeře, kam je to nejblíž — třeba i za jeho zády, kde mu v cestě
        //   k nosiči nic nestálo. Teď napřed pole NEJBLÍŽ NOSIČI (mezi soupeřem a nosičem), při
        //   shodě to, kam je to nejblíž.
        const bool toward = cageFeatureOn(kFeatMarkerToward);
        std::pair<int, int> bestKey{INT_MAX, INT_MAX};
        state.forEachOnPitch(side, [&](const Player& m) {
            if (m.id == carrier.id || !cageMayMove(m)) return;
            if (std::find(usedMarkers.begin(), usedMarkers.end(), m.id) != usedMarkers.end()) return;
            for (const Position& sq : o->position.getAdjacent()) {
                if (!sq.isOnPitch() || state.getPlayerAtPosition(sq)) continue;
                if (std::find(usedSquares.begin(), usedSquares.end(), sq) != usedSquares.end()) continue;
                if (pathFailProb(state, m, sq, m.movementRemaining, Position{-1, -1}) != 0.0) continue;
                const int steps = pathStepsToward(state, m, sq, m.movementRemaining, Position{-1, -1});
                const std::pair<int, int> key{toward ? sq.distanceTo(cp) : 0, steps};
                if (steps >= 0 && key < bestKey) {
                    bestKey = key;
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
        ballOursAtTurnStart_ = ourBall(state);
        lateFillDone_ = false;
        stalling_ = false;
        mateStall_ = false;
        readyMates_.clear();
    }
    // ⭐ P169 krok 9 (07.10.2026): MÍČ ZVEDLO HLEDÁNÍ ⇒ ROHY SE STAVÍ HNED POTOM. Když řadič na
    //   začátku tahu zvednutí nenabídl (míč v zóně soupeře, napřed rána) a míč pak zvedlo
    //   hledání, řadič už byl „hotov“ a klec nestavěl nikdo: 14 z 25 zvednutí po 1. kole,
    //   žádný přesun na roh, nosič s 0–1 rohem (trpaslíci, 80 poločasů). Teď se po takovém
    //   zvednutí jednou za tah dostaví rohy kolem nosiče a zaostalci jdou dopředu.
    if (cageFeatureOn(kFeatLateFill) && stage_ == Stage::DONE && idx_ >= queue_.size() && !ballOursAtTurnStart_ && !lateFillDone_ &&
        ourBall(state)) {
        lateFillDone_ = true;
        phase_ = CagePhase::CAGE;
        stage_ = Stage::AFTER_PICKUP;
        if (std::getenv("BB_CAGE_DEBUG")) std::fprintf(stderr, "[cage ctl] míč zvedlo hledání: dostavba rohů\n");
    }

    for (int guard = 0; guard < 6; ++guard) {
        if (idx_ < queue_.size()) {
            const Macro& m = queue_[idx_];
            if (!stillValid(state, m)) {   // plán se rozešel s deskou ⇒ zbytek tahu MCTS
                readyMates_.clear();       // připravený se možná nepřesunul ⇒ hledání ho nedrží (review M6)
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
