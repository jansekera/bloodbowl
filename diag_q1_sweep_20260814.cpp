// diag_q1_sweep_20260814.cpp
//
// Q1 SWEEP: tytéž tři otázky přes MNOHO GEOMETRIÍ (14.08.2026).
//
// `diag_q1_decisions_20260814.cpp` dal tři jednoznačné odpovědi, ale na JEDNÉ
// postavené pozici na scénář. Vlastní výhrada u toho zápisu zněla: „průkazné
// pro ni, ne pro všechny pozice — než se z toho udělá doktrína, obměnit
// geometrii." Tohle je ta obměna.
//
// Obměňuje se:
//   - vzdálenost nosiče od místa akce (blízko / daleko)
//   - počet našich asistencí u cíle (0 / 1 / 2)
//   - kolo (3 = klid, 7 = tlak na konec půle)
//   - y-souřadnice pásma (střed / u lajny)
// Každá kombinace × nSeeds opakování.
//
// ⚠️ Pořád je to Q1: měří VOLBU, ne hodnotu. Neříká, že bít Black Orka je
// lepší — říká jen, jestli si ho policy vybere, když má.
//
// Argv: [repoRoot=.] [nSeeds=12]
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "bb/game_state.h"
#include "bb/macro_mcts.h"
#include "bb/value_function.h"
#include "bb/policy_network.h"

using namespace bb;

namespace {

Player& place(GameState& gs, int id, TeamSide side, int x, int y,
              int ma, int st, int ag, int av) {
    Player& p = gs.getPlayer(id);
    p.id = id; p.teamSide = side; p.state = PlayerState::STANDING;
    p.position = {static_cast<int8_t>(x), static_cast<int8_t>(y)};
    p.stats = {static_cast<int8_t>(ma), static_cast<int8_t>(st),
               static_cast<int8_t>(ag), static_cast<int8_t>(av)};
    p.movementRemaining = static_cast<int8_t>(ma);
    p.hasMoved = false; p.hasActed = false;
    return p;
}

struct Geom {
    int ballDx;    // jak daleko je nosič od místa akce
    int assists;   // 0/1/2 naše těla u cíle
    int turn;      // 3 nebo 7
    int y;         // 7 = střed, 3 = blíž lajně
};

GameState baseState(const Geom& g) {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    gs.half = 1;
    gs.homeTeam.turnNumber = g.turn;
    gs.awayTeam.turnNumber = g.turn;
    gs.homeTeam.rerolls = 3; gs.awayTeam.rerolls = 3;
    gs.weather = Weather::NICE;
    return gs;
}

// scénář 1: Slayer mezi ST4 a ST3
GameState posDauntless(const Geom& g) {
    GameState gs = baseState(g);
    int bx = 13 - g.ballDx;
    Player& r = place(gs, 1, TeamSide::HOME, bx, g.y, 6, 3, 3, 8);
    r.skills.add(SkillName::SureHands); r.skills.add(SkillName::Block);
    gs.ball = BallState::carried({static_cast<int8_t>(bx), static_cast<int8_t>(g.y)}, 1);
    Player& s = place(gs, 2, TeamSide::HOME, 13, g.y, 5, 3, 2, 8);
    s.skills.add(SkillName::Block); s.skills.add(SkillName::Dauntless);
    Player& bo = place(gs, 12, TeamSide::AWAY, 14, g.y - 1, 4, 4, 2, 9);
    bo.skills.add(SkillName::Block); bo.skills.add(SkillName::Guard);
    place(gs, 13, TeamSide::AWAY, 14, g.y + 1, 5, 3, 3, 9).skills.add(SkillName::Block);
    place(gs, 14, TeamSide::AWAY, 16, g.y, 5, 3, 3, 9);
    // asistence: naše těla sousedící s OBĚMA cíli, ať nezvýhodní jeden
    if (g.assists >= 1) place(gs, 3, TeamSide::HOME, 13, g.y - 1, 4, 3, 2, 9).skills.add(SkillName::Block);
    if (g.assists >= 2) place(gs, 4, TeamSide::HOME, 13, g.y + 1, 4, 3, 2, 9).skills.add(SkillName::Block);
    return gs;
}

// scénář 2: Longbeard mezi jejich NOSIČEM a stejným linemanem
GameState posCarrier(const Geom& g) {
    GameState gs = baseState(g);
    place(gs, 1, TeamSide::HOME, 13 - g.ballDx - 2, g.y, 6, 3, 3, 8)
        .skills.add(SkillName::Block);
    Player& lb = place(gs, 2, TeamSide::HOME, 13, g.y, 4, 3, 2, 9);
    lb.skills.add(SkillName::Block); lb.skills.add(SkillName::Tackle);
    Player& car = place(gs, 12, TeamSide::AWAY, 14, g.y - 1, 6, 3, 3, 8);
    car.skills.add(SkillName::Block);
    gs.ball = BallState::carried({14, static_cast<int8_t>(g.y - 1)}, 12);
    place(gs, 13, TeamSide::AWAY, 14, g.y + 1, 6, 3, 3, 8).skills.add(SkillName::Block);
    place(gs, 14, TeamSide::AWAY, 16, g.y, 5, 3, 3, 9);
    if (g.assists >= 1) place(gs, 3, TeamSide::HOME, 13, g.y - 1, 4, 3, 2, 9).skills.add(SkillName::Block);
    if (g.assists >= 2) place(gs, 4, TeamSide::HOME, 13, g.y + 1, 4, 3, 2, 9).skills.add(SkillName::Block);
    return gs;
}

// scénář 3: náš LONGBEARD drží míč, vedle volný Runner
GameState posHandoff(const Geom& g) {
    GameState gs = baseState(g);
    Player& lb = place(gs, 1, TeamSide::HOME, 10, g.y, 4, 3, 2, 9);
    lb.skills.add(SkillName::Block); lb.skills.add(SkillName::Tackle);
    gs.ball = BallState::carried({10, static_cast<int8_t>(g.y)}, 1);
    // Runner o `ballDx` polí dopředu, ať se mění „je vepředu" i vzdálenost
    Player& run = place(gs, 2, TeamSide::HOME, 11, g.y, 6, 3, 3, 8);
    run.skills.add(SkillName::SureHands); run.skills.add(SkillName::Block);
    if (g.assists >= 1) place(gs, 3, TeamSide::HOME, 9, g.y - 1, 4, 3, 2, 9).skills.add(SkillName::Block);
    if (g.assists >= 2) place(gs, 4, TeamSide::HOME, 9, g.y + 1, 4, 3, 2, 9).skills.add(SkillName::Block);
    place(gs, 12, TeamSide::AWAY, 13 + g.ballDx, g.y - 1, 6, 3, 3, 8);
    place(gs, 13, TeamSide::AWAY, 13 + g.ballDx, g.y + 1, 6, 3, 3, 8);
    return gs;
}

struct Res { int a = 0, b = 0, none = 0, n = 0; };

Res sweep(GameState (*build)(const Geom&), const ValueFunction* vf,
          const MCTSConfig& cfg, const std::vector<Geom>& geoms, int nSeeds,
          bool handoffMode) {
    Res r;
    for (const Geom& g : geoms) {
        for (int i = 0; i < nSeeds; ++i) {
            GameState gs = build(g);
            MacroMCTSPolicy policy(vf, cfg, 1000u + static_cast<uint32_t>(i));
            Action act = policy(gs);
            r.n++;
            if (handoffMode) {
                if (act.type == ActionType::HAND_OFF || act.type == ActionType::PASS) r.a++;
                else r.none++;
            } else if (act.type == ActionType::BLOCK || act.type == ActionType::BLITZ) {
                if (act.targetId == 12) r.a++; else if (act.targetId == 13) r.b++;
            } else r.none++;
        }
    }
    return r;
}

void line(const char* label, const Res& r, const char* na, const char* nb) {
    int acts = r.a + r.b;
    printf("  %-22s pozic %4d | akcí %4d | %s %4d (%5.1f %%) | %s %4d (%5.1f %%)\n",
           label, r.n, acts, na, r.a, acts ? 100.0 * r.a / acts : 0.0,
           nb, r.b, acts ? 100.0 * r.b / acts : 0.0);
}

}  // namespace

int main(int argc, char** argv) {
    std::string root = (argc > 1) ? argv[1] : ".";
    int nSeeds = (argc > 2) ? atoi(argv[2]) : 12;

    auto vf = loadValueFunction(root + "/weights_best.json");
    auto pol = loadPolicyNetworkFromFile(root + "/weights_policy.json");
    if (!vf) { fprintf(stderr, "chybí weights_best.json\n"); return 1; }

    std::vector<Geom> geoms;
    for (int dx : {2, 5, 9})
        for (int as : {0, 1, 2})
            for (int t : {3, 7})
                for (int y : {7, 4})
                    geoms.push_back({dx, as, t, y});

    auto mk = [&](bool d) {
        MCTSConfig c;
        c.maxIterations = 100; c.timeBudgetMs = 0; c.explorationC = 1.0;
        c.dirichletAlpha = 0.0f; c.vfBlend = 0.15f; c.nRollouts = 1;
        c.policy = pol.get(); c.policyBlend = 0.0f; c.dauntlessInOffer = d;
        return c;
    };
    MCTSConfig off = mk(false), on = mk(true);
    printf("geometrií: %zu × %d seedů = %zu pozic na rameno\n\n",
           geoms.size(), nSeeds, geoms.size() * nSeeds);

    printf("=== 1. DAUNTLESS: Slayer mezi Black Orkem ST4 a linemanem ST3 ===\n");
    line("offer OFF", sweep(posDauntless, vf.get(), off, geoms, nSeeds, false),
         "BlackOrc", "Lineman ");
    line("offer ON", sweep(posDauntless, vf.get(), on, geoms, nSeeds, false),
         "BlackOrc", "Lineman ");

    printf("\n=== 2. NOSIČ: Longbeard mezi jejich NOSIČEM a linemanem ===\n");
    line("dnešní stav", sweep(posCarrier, vf.get(), off, geoms, nSeeds, false),
         "NOSIČ   ", "Lineman ");

    printf("\n=== 3. HAND-OFF: náš LONGBEARD drží míč, vedle volný Runner ===\n");
    Res h = sweep(posHandoff, vf.get(), off, geoms, nSeeds, true);
    printf("  %-22s pozic %4d | předání %4d (%5.1f %%) | jiná akce %4d\n",
           "dnešní stav", h.n, h.a, 100.0 * h.a / h.n, h.none);

    printf("\n⚠️ Pořád Q1: měří VOLBU, ne hodnotu.\n");
    return 0;
}
