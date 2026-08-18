// diag_q1_carrier_block_20260818.cpp
//
// Q1 TEST PRO P10a: VEZME SI SEARCH SOUPEŘOVA NOSIČE, KDYŽ MU HO NABÍDNEME?
//
// ⚑ PROČ TENHLE TEST PŘED NOCÍ (pravidlo z 14.08., potvrzené 18.08.)
//   Noční A/B měří VÝSLEDEK ZÁPASU tam, kde je otázka o JEDNOM ROZHODNUTÍ.
//   14.08. se tak málem propálilo 14 hodin na Dauntless: nabídka stoupla
//   84 → 112 bloků, ale Black Orka si search NEVYBRAL ANI JEDNOU. Noc by byla
//   změřila vedlejší efekt přidané volby, ne bití Black Orků.
//   ⇒ Napřed pojistka mechanismu (minuty), teprve pak měření (14 h).
//
// ⚠️ NENÍ to Q2. Neříká, že je ten cíl LEPŠÍ. Říká jen, jestli si ho search
//   vezme, když ho dostane do nabídky s vyšším priorem.
//
// ⭐ TESTUJE SE I PODMÍNKA, NE JEN PREFERENCE. P10a nezní „preferuj nosiče",
//   ale „udeř na nosiče, když u jeho pole máme aspoň tolik těl co soupeř" —
//   sražení nosiče totiž míč UVOLNÍ a soupeř má v dosahu víc těl v 54,1 %
//   případů (evidence/carrier_block_reach_20260818.md). Proto dvě varianty:
//     A) SPORNÉ  — u nosiče máme těl víc  ⇒ rameno MÁ zabrat
//     B) NESPORNÉ— soupeř má těl víc      ⇒ rameno NESMÍ zabrat
//   Kdyby se hnula i varianta B, není to podmínka, jen preference.
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

// Náš Blocker (id 2) stojí mezi soupeřovým NOSIČEM (id 12) a obyčejným
// linemanem (id 13) — obojí na jednu kostku, aby volbu nerozhodovala síla.
// `contestable` řídí, kdo má u pole nosiče víc těl.
GameState makePosition(bool contestable) {
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

    // ⚑ PŘEPLNĚNÁ DESKA ZÁMĚRNĚ (18.08.). První verze měla dva cíle a search
    //   si nosiče bral ve 100 % i bez ramene -- nebylo co zvedat a test
    //   neřekl nic. Prior floor je ale mechanismus HLADOVĚNÍ: BLOCK dává
    //   ~2,17 kandidáta na uzel a všichni mají týž floor 0,12, takže se blok
    //   na nosiče projeví teprve tam, kde o prior masu soutěží s dalšími.
    //   Proto pět našich těl, každé s vlastním soupeřem vedle sebe.

    // soupeřův nosič -- cíl, o který jde
    Player& carrier = place(12, TeamSide::AWAY, 14, 6, 6, 3, 3, 8);
    carrier.skills.add(SkillName::Block);
    gs.ball = BallState::carried({14, 6}, 12);

    // naše tělo vedle nosiče
    place(2, TeamSide::HOME, 13, 6, 5, 3, 2, 9).skills.add(SkillName::Block);

    // čtyři další dvojice náš/soupeř jinde na desce -- konkurence o prior
    const int px[4] = {8, 8, 20, 20};
    const int py[4] = {4, 11, 4, 11};
    for (int k = 0; k < 4; ++k) {
        place(3 + k, TeamSide::HOME, px[k], py[k], 5, 3, 2, 9)
            .skills.add(SkillName::Block);
        place(13 + k, TeamSide::AWAY, px[k] + 1, py[k], 6, 3, 3, 8)
            .skills.add(SkillName::Block);
    }

    if (contestable) {
        // naše těla v dosahu pole nosiče
        place(7, TeamSide::HOME, 12, 5, 6, 3, 2, 9).skills.add(SkillName::Block);
        place(8, TeamSide::HOME, 12, 7, 6, 3, 2, 9).skills.add(SkillName::Block);
    } else {
        // u pole nosiče stojí naopak soupeř
        place(7, TeamSide::HOME, 2, 13, 4, 3, 2, 9).skills.add(SkillName::Block);
        place(17, TeamSide::AWAY, 15, 5, 6, 3, 3, 8);
        place(18, TeamSide::AWAY, 16, 6, 6, 3, 3, 8);
        place(19, TeamSide::AWAY, 15, 7, 6, 3, 3, 8);
    }
    return gs;
}

struct Tally { int carrier = 0, other = 0, none = 0; long armEvals = 0; };

Tally run(const ValueFunction* vf, const PolicyNetwork* pol,
          bool arm, bool contestable, int nPos, uint32_t seed0) {
    MCTSConfig cfg;
    cfg.maxIterations = 100;
    cfg.timeBudgetMs = 0;
    cfg.explorationC = 1.0;
    cfg.dirichletAlpha = 0.0f;
    cfg.vfBlend = 0.15f;
    cfg.nRollouts = 1;
    cfg.policy = pol;
    cfg.policyBlend = 0.0f;
    cfg.carrierBlockPrior = arm;

    Tally t;
    for (int i = 0; i < nPos; ++i) {
        GameState gs = makePosition(contestable);
        MacroMCTSPolicy policy(vf, cfg, seed0 + static_cast<uint32_t>(i));
        Action a = policy(gs);
        const bool isHit = (a.type == ActionType::BLOCK || a.type == ActionType::BLITZ);
        if (!isHit)                  t.none++;
        else if (a.targetId == 12)   t.carrier++;
        else                         t.other++;
        // ⭐ „Zabralo rameno vůbec?" -- táž otázka jako per-pair leak test
        //   v noci. Nula tady znamená, že se test díval na dvě identická
        //   ramena a jakýkoli rozdíl by byl šum.
        t.armEvals += takeCarrierBlockPriorEvalsInSearch();
    }
    return t;
}

void report(const char* label, const Tally& off, const Tally& on, int nPos) {
    auto share = [](const Tally& t) {
        int hits = t.carrier + t.other;
        return hits ? 100.0 * t.carrier / hits : 0.0;
    };
    printf("--- %s ---\n", label);
    printf("   %-6s úderů %3d/%3d   na NOSIČE %3d  (%.1f %% z úderů)\n",
           "OFF", off.carrier + off.other, nPos, off.carrier, share(off));
    printf("   %-6s úderů %3d/%3d   na NOSIČE %3d  (%.1f %% z úderů)\n",
           "ON ", on.carrier + on.other, nPos, on.carrier, share(on));
    printf("   rozdíl ON−OFF: %+.1f pp\n", share(on) - share(off));
    printf("   rameno zabralo (evaluací v searchi): OFF %ld   ON %ld%s\n\n",
           off.armEvals, on.armEvals,
           on.armEvals == 0 ? "   ⛔ RAMENO SE NESPUSTILO -- rozdíl je šum" : "");
}

}  // namespace

int main(int argc, char** argv) {
    std::string root = (argc > 1) ? argv[1] : ".";
    int nPos = (argc > 2) ? atoi(argv[2]) : 200;
    uint32_t seed0 = (argc > 3) ? static_cast<uint32_t>(atoi(argv[3])) : 1u;

    auto vf = loadValueFunction(root + "/weights_best.json");
    auto pol = loadPolicyNetworkFromFile(root + "/weights_policy.json");
    if (!vf) { fprintf(stderr, "chybí weights_best.json v %s\n", root.c_str()); return 1; }

    printf("Q1 / P10a — vezme si search soupeřova nosiče?   pozic na rameno: %d\n\n", nPos);

    Tally aOff = run(vf.get(), pol.get(), false, true,  nPos, seed0);
    Tally aOn  = run(vf.get(), pol.get(), true,  true,  nPos, seed0);
    report("A) SPORNÉ pole — u nosiče máme těl VÍC (rameno MÁ zabrat)", aOff, aOn, nPos);

    Tally bOff = run(vf.get(), pol.get(), false, false, nPos, seed0);
    Tally bOn  = run(vf.get(), pol.get(), true,  false, nPos, seed0);
    report("B) NESPORNÉ pole — soupeř má těl víc (rameno NESMÍ zabrat)", bOff, bOn, nPos);

    printf("Čtení: A se musí hnout, B ne. Kdyby se hnulo obojí, není to podmínka,\n"
           "jen preference — a P10a je psané jako podmínka. Kdyby se nehnulo nic,\n"
           "nemá smysl pouštět noc: search si ten cíl nevezme, ať mu ho nabídneš jakkoli.\n");
    return 0;
}
