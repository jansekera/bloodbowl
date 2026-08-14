// diag_q1_target_choice_20260814.cpp
//
// Q1 TEST: VYBERE SI POLICY TEN CÍL SÁM? (uživatel 14.08.2026)
//
// Neměří výsledek zápasu, měří ROZHODNUTÍ. Postaví pozici, kde volba existuje,
// a spočítá, co si policy vybere — proti náhodě mezi nabídnutými možnostmi jako
// nulové hypotéze.
//
//   „Pokud to nevynutíš podstrčením situace — kdy má na výběr — a porovnáváš
//    náhodu vs vybrání."
//
// ⚠️ NENÍ to Q2. Neříká, jestli je ten cíl LEPŠÍ. Říká jen, jestli si ho policy
// vezme, když jí ho nabídneme. Vynutit volbu a pak měřit, že se stala, by bylo
// měření dodržování — přesně ta chyba, kterou udělala brána klece.
//
// POZICE: náš Troll Slayer (ST3, Dauntless+Block) stojí mezi dvěma soupeři:
//   - Black Orc  ST4  — Dauntless ho srovná (2+, 83 %), bez Dauntless je to do kopce
//   - Lineman    ST3  — rovnocenný, jedna kostka, dostupný vždycky
// Kdyby si policy Black Orka nevšímala, vybere linemana i se zapnutým Dauntless.
//
// Argv: [repoRoot=.] [nPositions=200] [seed0=1]
#include <cstdio>
#include <cstdlib>
#include <string>
#include "bb/game_state.h"
#include "bb/roster.h"
#include "bb/macro_mcts.h"
#include "bb/value_function.h"
#include "bb/policy_network.h"

using namespace bb;

namespace {

// Postaví desku: náš nosič vzadu, Slayer uprostřed, vedle něj Black Orc
// a Lineman, plus pár těl na obou stranách, ať pozice není degenerovaná.
GameState makePosition() {
    GameState gs;
    gs.phase = GamePhase::PLAY;
    gs.activeTeam = TeamSide::HOME;
    gs.half = 1;
    gs.homeTeam.turnNumber = 3;
    gs.awayTeam.turnNumber = 3;
    gs.homeTeam.rerolls = 3;
    gs.awayTeam.rerolls = 3;
    gs.weather = Weather::NICE;

    auto place = [&](int id, TeamSide side, int x, int y,
                     int ma, int st, int ag, int av) -> Player& {
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
    };

    // --- naše strana ---
    Player& runner = place(1, TeamSide::HOME, 10, 7, 6, 3, 3, 8);  // nosič
    runner.skills.add(SkillName::SureHands);
    runner.skills.add(SkillName::Block);
    gs.ball = BallState::carried({10, 7}, 1);

    Player& slayer = place(2, TeamSide::HOME, 13, 7, 5, 3, 2, 8);  // ten, kdo volí
    slayer.skills.add(SkillName::Block);
    slayer.skills.add(SkillName::Dauntless);

    // dvě těla, ať má nosič doprovod a pozice nevypadá jako sólo výběh
    place(3, TeamSide::HOME, 9, 6, 4, 3, 2, 9).skills.add(SkillName::Block);
    place(4, TeamSide::HOME, 9, 8, 4, 3, 2, 9).skills.add(SkillName::Block);

    // --- soupeř: dva cíle vedle Slayera, jinak stejní ---
    Player& bo = place(12, TeamSide::AWAY, 14, 6, 4, 4, 2, 9);     // Black Orc ST4
    bo.skills.add(SkillName::Block);
    bo.skills.add(SkillName::Guard);

    Player& lin = place(13, TeamSide::AWAY, 14, 8, 5, 3, 3, 9);    // Lineman ST3
    lin.skills.add(SkillName::Block);

    place(14, TeamSide::AWAY, 16, 7, 5, 3, 3, 9);                  // vzdálené tělo
    return gs;
}

const char* targetName(int id) {
    switch (id) {
        case 12: return "BlackOrc ST4";
        case 13: return "Lineman  ST3";
        case 14: return "vzdálený";
        default: return "jiný";
    }
}

}  // namespace

int main(int argc, char** argv) {
    std::string root = (argc > 1) ? argv[1] : ".";
    int nPos = (argc > 2) ? atoi(argv[2]) : 200;
    uint32_t seed0 = (argc > 3) ? static_cast<uint32_t>(atoi(argv[3])) : 1u;

    auto vf = loadValueFunction(root + "/weights_best.json");
    auto pol = loadPolicyNetworkFromFile(root + "/weights_policy.json");
    if (!vf) { fprintf(stderr, "chybí weights_best.json v %s\n", root.c_str()); return 1; }

    for (int arm = 0; arm < 2; ++arm) {
        const bool dauntless = (arm == 1);
        MCTSConfig cfg;
        cfg.maxIterations = 100;
        cfg.timeBudgetMs = 0;
        cfg.explorationC = 1.0;
        cfg.dirichletAlpha = 0.0f;
        cfg.vfBlend = 0.15f;
        cfg.nRollouts = 1;
        cfg.policy = pol.get();
        cfg.policyBlend = 0.0f;
        cfg.dauntlessInOffer = dauntless;

        int hitBO = 0, hitLin = 0, hitOther = 0, noBlock = 0;
        for (int i = 0; i < nPos; ++i) {
            GameState gs = makePosition();
            MacroMCTSPolicy policy(vf.get(), cfg, seed0 + static_cast<uint32_t>(i));
            Action a = policy(gs);
            const bool isHit = (a.type == ActionType::BLOCK || a.type == ActionType::BLITZ);
            if (!isHit)              noBlock++;
            else if (a.targetId == 12) hitBO++;
            else if (a.targetId == 13) hitLin++;
            else                       hitOther++;
        }
        int blocks = hitBO + hitLin + hitOther;
        printf("=== dauntlessInOffer = %s ===  pozic: %d\n",
               dauntless ? "ON " : "OFF", nPos);
        printf("   blok/blitz zvolen        %4d  (%.1f %%)\n",
               blocks, 100.0 * blocks / nPos);
        printf("   ├─ %-16s      %4d  (%.1f %% z bloků)\n",
               targetName(12), hitBO, blocks ? 100.0 * hitBO / blocks : 0.0);
        printf("   ├─ %-16s      %4d  (%.1f %% z bloků)\n",
               targetName(13), hitLin, blocks ? 100.0 * hitLin / blocks : 0.0);
        printf("   └─ jiný cíl               %4d\n", hitOther);
        printf("   jiná akce než úder        %4d\n\n", noBlock);
    }
    printf("Nulová hypotéza: kdyby si policy cíle nevšímala, poměr BlackOrc:Lineman\n"
           "je stejný v obou ramenech. Rozdíl mezi ON a OFF je celá odpověď na Q1.\n");
    return 0;
}
