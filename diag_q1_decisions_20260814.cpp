// diag_q1_decisions_20260814.cpp
//
// Q1 TESTY ROZHODNUTÍ (uživatel 14.08.2026)
//
// Neměří výsledek zápasu, měří ROZHODNUTÍ: postaví pozici, kde volba existuje,
// a spočítá, co si policy vybere. Nulová hypotéza je, že si toho cíle nevšímá
// — pak je poměr voleb v obou ramenech stejný.
//
// ⚠️ NENÍ to Q2. Neříká, jestli je ta volba LEPŠÍ, jen jestli ji policy udělá.
// Vynutit volbu a pak měřit, že se stala, by bylo měření dodržování.
//
// Proč to existuje: 14.08. by noční A/B na Dauntless spálilo 14 h na měření
// vedlejšího efektu — Q1 test to ukázal za 5 minut (0 ze 112 voleb Black Orka).
//
// Scénáře:
//   1 DAUNTLESS  Slayer mezi Black Orkem ST4 a linemanem ST3.
//                Ramena: dauntlessInOffer OFF / ON.
//   2 CARRIER    Longbeard mezi soupeřovým NOSIČEM a stejným linemanem.
//                Jedno rameno — ptáme se, jestli si vůbec všímá, kdo drží míč.
//                (P15/P10a: prior je pro všechny bloky plochý = 15.)
//   3 HANDOFF    Náš LONGBEARD drží míč, vedle stojí volný Runner.
//                Ptáme se, jestli policy předání vůbec zvolí, když situace
//                nastane — v 3000 hrách nenastala ani jednou.
//
// Argv: [repoRoot=.] [nPositions=120] [seed0=1]
#include <cstdio>
#include <cstdlib>
#include <string>
#include "bb/game_state.h"
#include "bb/macro_mcts.h"
#include "bb/value_function.h"
#include "bb/policy_network.h"

using namespace bb;

namespace {

Player& place(GameState& gs, int id, TeamSide side, int x, int y,
              int ma, int st, int ag, int av) {
    Player& p = gs.getPlayer(id);
    p.id = id;
    p.teamSide = side;
    p.state = PlayerState::STANDING;
    p.position = {static_cast<int8_t>(x), static_cast<int8_t>(y)};
    p.stats = {static_cast<int8_t>(ma), static_cast<int8_t>(st),
               static_cast<int8_t>(ag), static_cast<int8_t>(av)};
    p.movementRemaining = static_cast<int8_t>(ma);
    p.hasMoved = false;
    p.hasActed = false;
    return p;
}

GameState baseState() {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    gs.half = 1;
    gs.homeTeam.turnNumber = 3;
    gs.awayTeam.turnNumber = 3;
    gs.homeTeam.rerolls = 3;
    gs.awayTeam.rerolls = 3;
    gs.weather = Weather::NICE;
    return gs;
}

// --- 1: Slayer si vybírá mezi ST4 a ST3 -------------------------------------
GameState posDauntless() {
    GameState gs = baseState();
    Player& r = place(gs, 1, TeamSide::HOME, 10, 7, 6, 3, 3, 8);
    r.skills.add(SkillName::SureHands); r.skills.add(SkillName::Block);
    gs.ball = BallState::carried({10, 7}, 1);
    Player& s = place(gs, 2, TeamSide::HOME, 13, 7, 5, 3, 2, 8);
    s.skills.add(SkillName::Block); s.skills.add(SkillName::Dauntless);
    place(gs, 3, TeamSide::HOME, 9, 6, 4, 3, 2, 9).skills.add(SkillName::Block);
    place(gs, 4, TeamSide::HOME, 9, 8, 4, 3, 2, 9).skills.add(SkillName::Block);
    Player& bo = place(gs, 12, TeamSide::AWAY, 14, 6, 4, 4, 2, 9);
    bo.skills.add(SkillName::Block); bo.skills.add(SkillName::Guard);
    place(gs, 13, TeamSide::AWAY, 14, 8, 5, 3, 3, 9).skills.add(SkillName::Block);
    place(gs, 14, TeamSide::AWAY, 16, 7, 5, 3, 3, 9);
    return gs;
}

// --- 2: Longbeard si vybírá mezi jejich NOSIČEM a linemanem -----------------
GameState posCarrier() {
    GameState gs = baseState();
    place(gs, 1, TeamSide::HOME, 8, 7, 6, 3, 3, 8).skills.add(SkillName::Block);
    Player& lb = place(gs, 2, TeamSide::HOME, 13, 7, 4, 3, 2, 9);
    lb.skills.add(SkillName::Block); lb.skills.add(SkillName::Tackle);
    place(gs, 3, TeamSide::HOME, 12, 5, 4, 3, 2, 9).skills.add(SkillName::Block);
    // id 12 = jejich NOSIČ, id 13 = obyčejný lineman, jinak totožní
    Player& car = place(gs, 12, TeamSide::AWAY, 14, 6, 6, 3, 3, 8);
    car.skills.add(SkillName::Block);
    gs.ball = BallState::carried({14, 6}, 12);
    place(gs, 13, TeamSide::AWAY, 14, 8, 6, 3, 3, 8).skills.add(SkillName::Block);
    place(gs, 14, TeamSide::AWAY, 16, 7, 5, 3, 3, 9);
    return gs;
}

// --- 3: náš LONGBEARD drží míč, vedle volný Runner ---------------------------
GameState posHandoff() {
    GameState gs = baseState();
    Player& lb = place(gs, 1, TeamSide::HOME, 10, 7, 4, 3, 2, 9);   // špatný nosič
    lb.skills.add(SkillName::Block); lb.skills.add(SkillName::Tackle);
    gs.ball = BallState::carried({10, 7}, 1);
    Player& run = place(gs, 2, TeamSide::HOME, 11, 7, 6, 3, 3, 8);  // lepší ruce
    run.skills.add(SkillName::SureHands); run.skills.add(SkillName::Block);
    place(gs, 3, TeamSide::HOME, 9, 6, 4, 3, 2, 9).skills.add(SkillName::Block);
    place(gs, 4, TeamSide::HOME, 9, 8, 4, 3, 2, 9).skills.add(SkillName::Block);
    place(gs, 12, TeamSide::AWAY, 15, 6, 6, 3, 3, 8);
    place(gs, 13, TeamSide::AWAY, 15, 8, 6, 3, 3, 8);
    return gs;
}

struct Tally { int a = 0, b = 0, other = 0, none = 0; };

Tally run(GameState (*build)(), const ValueFunction* vf, const MCTSConfig& cfg,
          int n, uint32_t seed0, bool handoffMode) {
    Tally t;
    for (int i = 0; i < n; ++i) {
        GameState gs = build();
        MacroMCTSPolicy policy(vf, cfg, seed0 + static_cast<uint32_t>(i));
        Action act = policy(gs);
        if (handoffMode) {
            if (act.type == ActionType::HAND_OFF || act.type == ActionType::PASS) t.a++;
            else t.none++;
        } else {
            const bool hit = (act.type == ActionType::BLOCK || act.type == ActionType::BLITZ);
            if (!hit) t.none++;
            else if (act.targetId == 12) t.a++;
            else if (act.targetId == 13) t.b++;
            else t.other++;
        }
    }
    return t;
}

void report(const char* label, const Tally& t, int n,
            const char* nameA, const char* nameB) {
    int acts = t.a + t.b + t.other;
    printf("  %-26s akcí %3d/%d", label, acts, n);
    if (acts) {
        printf("   %s %3d (%.0f %%)   %s %3d (%.0f %%)",
               nameA, t.a, 100.0 * t.a / acts, nameB, t.b, 100.0 * t.b / acts);
    }
    printf("\n");
}

}  // namespace

int main(int argc, char** argv) {
    std::string root = (argc > 1) ? argv[1] : ".";
    int n = (argc > 2) ? atoi(argv[2]) : 120;
    uint32_t seed0 = (argc > 3) ? static_cast<uint32_t>(atoi(argv[3])) : 1u;

    auto vf = loadValueFunction(root + "/weights_best.json");
    auto pol = loadPolicyNetworkFromFile(root + "/weights_policy.json");
    if (!vf) { fprintf(stderr, "chybí weights_best.json v %s\n", root.c_str()); return 1; }

    auto mk = [&](bool daunt) {
        MCTSConfig c;
        c.maxIterations = 100; c.timeBudgetMs = 0; c.explorationC = 1.0;
        c.dirichletAlpha = 0.0f; c.vfBlend = 0.15f; c.nRollouts = 1;
        c.policy = pol.get(); c.policyBlend = 0.0f; c.dauntlessInOffer = daunt;
        return c;
    };
    MCTSConfig off = mk(false), on = mk(true);
    // vf se musí předat policy, jinak se hodnotí naslepo
    off.policy = pol.get(); on.policy = pol.get();

    printf("=== 1. DAUNTLESS: Slayer mezi Black Orkem ST4 a linemanem ST3 ===\n");
    printf("    (P13 — mění nabídka volbu cíle?)\n");
    report("offer OFF", run(posDauntless, vf.get(), off, n, seed0, false), n, "BlackOrc", "Lineman");
    report("offer ON ", run(posDauntless, vf.get(), on,  n, seed0, false), n, "BlackOrc", "Lineman");

    printf("\n=== 2. NOSIČ: Longbeard mezi jejich NOSIČEM a stejným linemanem ===\n");
    printf("    (P15/P10a — všímá si vůbec, kdo drží míč? prior BLOCK je plochý 15)\n");
    report("dnešní stav", run(posCarrier, vf.get(), off, n, seed0, false), n, "NOSIČ   ", "Lineman");

    printf("\n=== 3. HAND-OFF: náš LONGBEARD drží míč, vedle volný Runner ===\n");
    printf("    (P5 — zvolí předání, když situace NASTANE? v 3000 hrách nenastala)\n");
    Tally h = run(posHandoff, vf.get(), off, n, seed0, true);
    printf("  %-26s předání %3d/%d (%.0f %%)   jiná akce %3d\n",
           "dnešní stav", h.a, n, 100.0 * h.a / n, h.none);

    printf("\nNulová hypotéza: policy si cíle nevšímá ⇒ poměr voleb se mezi rameny\n"
           "nezmění. U scénářů 2 a 3 je referencí to, co by dal náhodný výběr\n"
           "mezi nabídnutými možnostmi (dva rovnocenné cíle ⇒ 50:50).\n");
    return 0;
}
