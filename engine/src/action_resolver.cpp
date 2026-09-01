#include "bb/action_resolver.h"
#include "bb/macro_actions.h"
#include "bb/move_handler.h"
#include "bb/block_handler.h"
#include "bb/foul_handler.h"
#include "bb/pass_handler.h"
#include "bb/big_guy_handler.h"
#include "bb/turn_handler.h"
#include "bb/pathfinder.h"
#include "bb/helpers.h"
#include "bb/ttm_handler.h"
#include "bb/bomb_handler.h"
#include "bb/gaze_handler.h"
#include "bb/ball_and_chain_handler.h"

namespace bb {

namespace {

// M2/N13 = P55 (29.08.2026): deklarovana akce, ktera propadne big-guy
// kontrolou, MUSI tymu odecist jeho limit -- Bone-head to rika doslova
// ("the team cannot declare another Blitz Action that turn", r. 7981-7983).
// Dosud se `blitzUsedThisTurn` nastavovalo az uvnitr `case BLITZ`, kam se
// pri zablokovane akci nikdy nedoslo, takze tym dostal DRUHY blitz.
// Plati na kazdou akci s tymovym limitem, ne jen na blitz -- pravidlo mluvi
// o "the declared Action", ne o blitzu.
void consumeDeclaredTeamAction(GameState& state, TeamSide side, ActionType type) {
    TeamState& team = state.getTeamState(side);
    switch (type) {
        case ActionType::BLITZ:           team.blitzUsedThisTurn = true;   break;
        case ActionType::PASS:
        case ActionType::THROW_TEAM_MATE: team.passUsedThisTurn = true;    break;
        case ActionType::HAND_OFF:        team.handOffUsedThisTurn = true; break;
        case ActionType::FOUL:            team.foulUsedThisTurn = true;    break;
        default: break;   // MOVE a BLOCK zadny tymovy limit nemaji
    }
}

}  // namespace

// ============================================================================
// M13 DIAGNOSTIKA (01.09.2026) — uzivatel: "dopln raději vice logu".
//
// ⭐ K CEMU: M13 je OPRAVA PRAVIDLA a nasadi se, i kdyby merena delta vysla
//   ZAPORNE -- nehraje se podle toho, co nam vyhovuje. Mereni proto
//   nerozhoduje "nasadit ano/ne"; kupuje dve JINE veci:
//     (1) ZNAT CENU -- lezici hrac, ktery vstane a jde, MUSI hazet na dodge
//         z tacklezony, a neuspesny dodge je TURNOVER. To je legitimni cena
//         pravidla, ne vada.
//     (2) CHYTIT CHYBU -- kdyby byl efekt velky a zaporny, neznamena to
//         "pravidla nam skodi", ale "neco jsme naprogramovali spatne".
//   ⛔ Bez techle citacu se ty dve veci od sebe NEROZEZNAJI, a to je presne
//     ten pripad, kdy vic diagnostiky poradi lip nez vetsi vzorek.
//
// Meri se na JEDNOM MISTE (obalka kolem resolveAction), ne rozsypane po
// resolverech -- dve kopie teze ucetni logiky znamenaji, ze jedna zestarne.
// ============================================================================
namespace {
thread_local long g_proneActs      = 0;   // akci zacatych vleze (mimo vstani na miste)
thread_local long g_proneTurnovers = 0;   // z toho skoncilo TURNOVEREM
thread_local long g_proneStandFail = 0;   // z toho selhalo uz vstani (neni turnover)
thread_local long g_proneNoBlock   = 0;   // blitz z lehu, ktery ranu nakonec nehodil
thread_local long g_proneBlitzes   = 0;   // z akci z lehu: kolik jich bylo BLITZ
// ⭐ REFERENCE (uzivateluv tvar "merit to, co zmena dela"): podil turnoveru
//   z akci z lehu sam o sobe NIC NERIKA. Bez cisla za STOJICI hrace se necda
//   poznat, jestli je vstavani drazsi nez bezna akce, nebo jestli ma engine
//   vysokou turnoverovost vsude. Merit zmenu proti spatne referenci znamena,
//   ze vysledek nemuze nic rozhodnout -- tataz vada jako Leap proti dodge.
thread_local long g_standActs      = 0;
thread_local long g_standTurnovers = 0;
// ⭐ Tataz reference i pro VYHOZENY BLITZ (uzivatel 01.09.: "a mame u std
//   blitz ze stoje taky?"). Stojici blitzer taky nemusi dobehnout -- spadne
//   na dodge, neuhodi GFI, nebo mu cestu zavrou tela. Bez tohohle cisla je
//   "blitz z lehu bez rany 1,7 %" zase udaj bez meritka.
thread_local long g_standBlitzes   = 0;
thread_local long g_standNoBlock   = 0;
// ⛔⛔ PAST SE JMENOVATELEM (nalez 01.09., pri cteni prvni reference).
//   "TURNOVER z lehu 20,9 % PROTI ze stoje 2,0 %" vypadalo jako dolozena cena
//   pravidla. NENI to srovnani stejneho se stejnym: akce z lehu je JEDNA akce
//   obsahujici cely blitz (vstat + dojit + prastit), kdezto stojici hrac, ktery
//   jde sest poli, je SEST samostatnych akci MOVE, vetsinou zadarmo. Jmenovatel
//   ze stoje byl proto nafouknuty trivialnimi kroky (1 390 173 proti 5 981)
//   a podil 10,7x meril z velke casti HRUBOST JEDNOTKY, ne cenu vstavani.
//   ⇒ Poctive srovnani je BLITZ PROTI BLITZU: tataz akce, tataz vnitrni
//     struktura, lisi se jen tim, jestli hrac na zacatku lezel.
thread_local long g_proneBlitzTO   = 0;
thread_local long g_standBlitzTO   = 0;

// ⭐⭐ ROZPAD VYHOZENEHO BLITZU PODLE PRICINY (01.09.2026).
//   Zmereno: 9,3 % blitzu ZE STOJE nikdy nehodi ranu -- skoro kazdy jedenacty,
//   a blitz je jedina akce s tvrdym limitem 1/kolo. Nez se to zacne opravovat,
//   musi se vedet PROC. Zkusil jsem to uhodnout (hladova chuze osciluje) a
//   bylo to spatne DVAKRAT: teorie padla na Cebysevove metrice a oprava
//   nedodala (9,3 % -> 9,6 %). ⇒ Dalsi krok neni dalsi domnenka, je to rozpad.
//   NEDOSAH  = pathfinder rekl "nikam", nebo hladovy vyber nenasel krok
//   POHYB    = krok selhal (dosel pohyb vcetne GFI)
//   TURNOVER = pri dojiti padl dodge/GFI => ztrata kola
//   SRAZEN   = po kroku uz nestal (Diving Tackle a spol.)
//   STOJI    = krok "uspel", ale hrac se nehnul (Tentacles)
//   DALEKO   = dosel, ale neskoncil sousedici
thread_local long g_bwNoReach = 0, g_bwMove = 0, g_bwTurnover = 0,
                  g_bwDown = 0, g_bwStuck = 0, g_bwFar = 0;
thread_local long g_blitzDeclAdj = 0, g_blitzDeclFar = 0;
// ⭐ P37b DIAGNOSTIKA (01.09.2026, pripravena za behu noci, NESPUSTENA).
//   Nabidka rezervuje pole na ranu (`canReachAdjacentTo(..., 1)`), chuze ne
//   (reserve=0). Pripad "dosel, ale rana neni z ceho" proto muze nastat JEN
//   tehdy, kdyz hladova chuze jde DELSI cestou nez BFS optimum -- jinak by
//   rezerva zbyla. Merime tedy presne to: kroky navic proti optimu.
//   ⛔ Rano jsem P37b "opravil" v NABIDCE a nezabralo to (142 -> 140).
//     Teprve tenhle rozdil rekne, jestli je vada v chuzi, nebo jinde.
thread_local long g_blitzSteps = 0, g_blitzOptimal = 0, g_blitzExtra = 0;
}

void noteBlitzPathLength(int steps, int optimal) {
    ++g_blitzSteps;
    if (steps <= optimal) ++g_blitzOptimal;
    else g_blitzExtra += (steps - optimal);
}
void takeBlitzPathStats(long* out3) {
    out3[0]=g_blitzSteps; out3[1]=g_blitzOptimal; out3[2]=g_blitzExtra;
    g_blitzSteps=g_blitzOptimal=g_blitzExtra=0;
}

void noteBlitzDeclaredAdjacent(bool adjacent) {
    if (adjacent) ++g_blitzDeclAdj; else ++g_blitzDeclFar;
}
void takeBlitzDeclSplit(long* out2) {
    out2[0] = g_blitzDeclAdj; out2[1] = g_blitzDeclFar;
    g_blitzDeclAdj = g_blitzDeclFar = 0;
}

void noteBlitzWasted(int reason) {
    switch (reason) {
        case 0: ++g_bwNoReach;  break;
        case 1: ++g_bwMove;     break;
        case 2: ++g_bwTurnover; break;
        case 3: ++g_bwDown;     break;
        case 4: ++g_bwStuck;    break;
        default: ++g_bwFar;     break;
    }
}

void takeBlitzWastedBreakdown(long* out6) {
    out6[0]=g_bwNoReach; out6[1]=g_bwMove; out6[2]=g_bwTurnover;
    out6[3]=g_bwDown;    out6[4]=g_bwStuck; out6[5]=g_bwFar;
    g_bwNoReach=g_bwMove=g_bwTurnover=g_bwDown=g_bwStuck=g_bwFar=0;
}

long takeProneActsInSearch()      { long v=g_proneActs;      g_proneActs=0;      return v; }
long takeProneTurnoversInSearch() { long v=g_proneTurnovers; g_proneTurnovers=0; return v; }
long takeProneStandFailInSearch() { long v=g_proneStandFail; g_proneStandFail=0; return v; }
long takeProneNoBlockInSearch()   { long v=g_proneNoBlock;   g_proneNoBlock=0;   return v; }
long takeProneBlitzesInSearch()   { long v=g_proneBlitzes;   g_proneBlitzes=0;   return v; }
long takeStandActsInSearch()      { long v=g_standActs;      g_standActs=0;      return v; }
long takeStandTurnoversInSearch() { long v=g_standTurnovers; g_standTurnovers=0; return v; }
long takeStandBlitzesInSearch()   { long v=g_standBlitzes;   g_standBlitzes=0;   return v; }
long takeStandNoBlockInSearch()   { long v=g_standNoBlock;   g_standNoBlock=0;   return v; }
long takeProneBlitzTOInSearch()   { long v=g_proneBlitzTO;   g_proneBlitzTO=0;   return v; }
long takeStandBlitzTOInSearch()   { long v=g_standBlitzTO;   g_standBlitzTO=0;   return v; }

static ActionResult resolveActionInner(GameState& state, const Action& action,
                                       DiceRollerBase& dice,
                                       std::vector<GameEvent>* events);

ActionResult resolveAction(GameState& state, const Action& action,
                           DiceRollerBase& dice, std::vector<GameEvent>* events) {
    // Stav PRED akci -- potom uz hrac stoji a nepoznalo by se to.
    const bool actorProne =
        requiresPlayer(action.type) && action.playerId > 0 &&
        state.getPlayer(action.playerId).state == PlayerState::PRONE &&
        !(action.type == ActionType::MOVE &&
          action.target == state.getPlayer(action.playerId).position);
    const bool wasBlitz = actorProne && action.type == ActionType::BLITZ;
    const long blocksBefore = blockThrowSeq();

    ActionResult r = resolveActionInner(state, action, dice, events);

    // Reference: tataz akce, ale od hrace, ktery uz STAL.
    if (!actorProne && requiresPlayer(action.type) && action.playerId > 0) {
        ++g_standActs;
        if (r.turnover) ++g_standTurnovers;
        if (action.type == ActionType::BLITZ) {
            ++g_standBlitzes;
            if (r.turnover) ++g_standBlitzTO;
            if (blockThrowSeq() == blocksBefore) ++g_standNoBlock;
        }
    }

    if (actorProne) {
        ++g_proneActs;
        if (action.type == ActionType::BLITZ) {
            ++g_proneBlitzes;
            if (r.turnover) ++g_proneBlitzTO;
        }
        if (r.turnover) ++g_proneTurnovers;
        // Vstani selhalo: hrac po akci LEZI DAL a akce je spotrebovana.
        // r. 693-694 -- neni to turnover, a proto se to pocita zvlast.
        const Player& p = state.getPlayer(action.playerId);
        if (!r.success && !r.turnover && p.state == PlayerState::PRONE) {
            ++g_proneStandFail;
        }
        if (wasBlitz && p.state != PlayerState::PRONE) {
            // Vstal, ale ranu nehodil => TYMOVY BLITZ PADL NA DOJITI.
            // ⭐ VYZNAM JE JEDNOZNACNY (uzivatel 01.09.): blitz je JEDNA akce
            //   uvnitr jedne aktivace. Souper behem ni nehraje, nas pohyb
            //   nikoho nepoklada, a dovednosti, ktere se pri nem spousti,
            //   miri na POHYBUJICIHO SE hrace (Diving Tackle srazi toho, kdo
            //   dodgeuje; Tentacles ho drzi; Shadowing ho nasleduje) -- CIL
            //   nepolozi ani neodsunou. ⇒ "rana nepadla" NEMUZE znamenat
            //   "cil uz lezel"; znamena vzdy "blitzujici se k nemu nedostal".
            //   Duvody jsou v resolveru vyjmenovatelne: nenasel krok, neuspel
            //   v pohybu, spadl pri dodge/GFI, nebo skoncil nesousedici.
            //   ⛔ Nepridavat sem znovu vyhradu "mohl ho slozit nekdo jiny" --
            //     byla nespravna a merilo by se pak mene, nez to umi.
            // ⛔ Cte se POCITADLO ran, ne `events`: behem hledani jsou udalosti
            //   vypnute (nullptr) a puvodni verze proto hlasila "bez rany"
            //   u KAZDEHO blitzu z lehu. Meridlo nesmi viset na logovani.
            if (blockThrowSeq() == blocksBefore) ++g_proneNoBlock;
        }
    }
    return r;
}

static ActionResult resolveActionInner(GameState& state, const Action& action,
                           DiceRollerBase& dice, std::vector<GameEvent>* events) {
    // BigGuy pre-action checks for player actions
    if (requiresPlayer(action.type) && action.playerId > 0) {
        Player& p = state.getPlayer(action.playerId);
        bool hasBigGuySkill = p.hasSkill(SkillName::BoneHead) ||
                              p.hasSkill(SkillName::ReallyStupid) ||
                              p.hasSkill(SkillName::WildAnimal) ||
                              p.hasSkill(SkillName::TakeRoot) ||
                              p.hasSkill(SkillName::Bloodlust);
        // ⭐ VSTÁVÁNÍ SE NEBLOKUJE TAKE ROOTEM (oprava 21.08.). BB2016
        // l. 8583-8584 doslova: "...he may not block that turn (HE CAN STILL
        // ROLL TO STAND UP IF HE IS PRONE)." Bone Head ("can't do anything
        // for the turn") a Really Stupid blokují správně -- výjimku má JEN
        // Take Root. Bez toho vstane Treeman s p = 5/6 x 1/2 = 41,7 % místo
        // 50 %, a je to jediné tělo pod 3 MA v pěti TV1200 sestavách.
        const bool standUpAttempt =
            (action.type == ActionType::MOVE &&
             p.state == PlayerState::PRONE &&
             action.target == p.position);
        // ⭐ A JEN JEDNOU ZA AKTIVACI (oprava 21.08.). BB2016 l. 8573:
        // "Immediately after declaring an ACTION". Vícepolový pohyb je u nás
        // N akcí MOVE, takže se házelo N-krát -- Ogre a Treeman rolovali
        // Bone Head / Take Root za každé pole. Spící vada, kterou probudilo
        // P45 (dokud se ležící nezvedal, nikam nešel).
        // ⛔ N14 (01.09.2026): vyjimka `!(standUpAttempt && onlyTakeRoot)`
        //   ZRUSENA. Prehazovala cely hod, takze zakorenení pri vstavani
        //   NIKDY nevzniklo. Misto toho se hod hazi a `standUpInPlace` rekne
        //   handleru, ze vstani se blokovat nesmi (r. 8583-8584).
        if (hasBigGuySkill && !p.bigGuyCheckedThisTurn) {
            p.bigGuyCheckedThisTurn = true;
            BigGuyResult bgResult = resolveBigGuyCheck(state, action.playerId,
                                                        action.type, dice, events,
                                                        standUpAttempt);
            if (bgResult.turnover) {
                // TA10: Blood Lust -- upir nemel koho kousnout (l. 7942-7943),
                // nebo kousnuty Thrall drzel mic (l. 7941-7942).
                return ActionResult::turnovr();
            }
            if (bgResult.actionBlocked && !bgResult.proceed) {
                // M2: akce propadla -- pokud ji pravidlo bere i TYMU, odecist
                // ji tady, protoze do switche (kde se limit nastavuje) se uz
                // nedostaneme. `wastesTeamAction` nese to rozliseni.
                if (bgResult.wastesTeamAction) {
                    consumeDeclaredTeamAction(state, p.teamSide, action.type);
                }
                return ActionResult::ok();  // Action wasted, not turnover
            }
        }
    }

    switch (action.type) {
        case ActionType::MOVE: {
            Player& player = state.getPlayer(action.playerId);

            // If prone, stand up first
            if (player.state == PlayerState::PRONE) {
                // ⛔⛔ M13 SIGNAL, DRUHA POLOVINA (audit 01.09.2026).
                //   Citac puvodne tikal JEN na blitz z lehu -- jenze rameno
                //   pousti lezice i k POHYBU (vstat a jit jinam, ne jen vstat
                //   na miste). Par, kde rameno zmenilo POUZE pohyb, by tedy
                //   hlasil "arm acted 0" a zaroven pohnutou hrou => LEAK TEST
                //   BY KRICEL NA VLASTNIM RAMENI a noc by se necetla.
                //   ⇒ Vstani NA MISTE se nepocita: to slo i pred M13
                //     (samostatna smycka v rules_engine).
                if (action.target != player.position) noteProneActionTaken();
                ActionResult standResult = resolveStandUp(state, action.playerId, dice, events);
                if (!standResult.success) return standResult;

                // If target is player's own position, this was just a stand-up
                if (action.target == player.position) {
                    return ActionResult::ok();
                }
            }

            return resolveMoveStep(state, action.playerId, action.target, dice, events);
        }

        case ActionType::BLOCK: {
            BlockParams params;
            params.attackerId = action.playerId;
            params.targetId = action.targetId;
            params.isBlitz = false;
            params.hornsBonus = false;
            return resolveBlock(state, params, dice, events);
        }

        case ActionType::BLITZ: {
            Player& player = state.getPlayer(action.playerId);
            Player& target = state.getPlayer(action.targetId);
            // P37b diagnostika: byl blitzujici pri DEKLARACI uz soused?
            // Rozliseni rozhodne, kde vada je: u souseda = vada NABIDKY,
            // u vzdaleneho = CHUZE spotrebovala rezervu, kterou reachability
            // slibila (a to je vada pohybu, ne nabidky).
            noteBlitzDeclaredAdjacent(player.position.distanceTo(target.position) == 1);

            // Mark blitz used
            state.getTeamState(player.teamSide).blitzUsedThisTurn = true;
            player.usedBlitz = true;

            // If prone, stand up first
            if (player.state == PlayerState::PRONE) {
                // M13 signal: blitz z lehu do 31.08. NESEL vubec, takze kazdy
                // vyskyt je zmenena hra, ne jen "rameno bezelo".
                noteProneActionTaken();
                ActionResult standResult = resolveStandUp(state, action.playerId, dice, events);
                if (!standResult.success) return standResult;
            }

            // Move toward target if not adjacent. Distance stays the primary
            // criterion (progress toward the target is guaranteed, same as the
            // old raw-distance picker), but enemy tackle zones now break ties
            // between equally-close squares — this loop was the one movement
            // path in the engine with zero TZ awareness (item 7), unlike every
            // macro routed through scoreMoveAction. Weights mirror
            // scoreMoveAction's 20/12 split; both are < 100 so a TZ-laden
            // square is still taken when it's the only one making progress
            // (a blitz through an unavoidable TZ wall must not fail outright).
            while (player.position.distanceTo(target.position) > 1) {
                // Reachability gate only: canReachAdjacentTo's adjPos (BFS by
                // pure movement cost, TZ-blind) is deliberately ignored — the
                // TZ-scored picker below owns both the route and the final
                // adjacent square (fewer enemies next to the blitzer = fewer
                // defender assists on the block, see getBlockDiceCount).
                // ⭐⭐ M14b (01.09.2026): CHUZE UHYBA TACKLEZONAM, i za cenu
                //   delsi cesty. ZMERENO: 2 953 z 3 420 vyhozenych blitzu
                //   (86 %) je TURNOVER PRI DOBEHU -- blitzujici vlezl do
                //   tacklezony, hodil dodge a slozil se. Puvodni hladovy
                //   `pickApproachStep` ma `vzdalenost*100 + TZ*12`, takze
                //   za JEDNO pole priblizeni bere tretinovou sanci na ztratu
                //   CELEHO KOLA.
                //   ⛔ Prvni pokus (M14, lexikograficky) NEDODAL a byl vracen:
                //     minimalizoval TZ jen mezi stejne dlouhymi cestami.
                //     Tohle je druhy pokus a lisi se prave tim, ze delsi
                //     bezpecnou cestu vzit UMI.
                Position bestNext;
                if (blitzPathArm(player.teamSide)) {
                    if (!nextStepTowardAdjacent(state, player, target.position, bestNext)) {
                        noteBlitzWasted(0);
                        return ActionResult::fail();
                    }
                } else {
                    Position adjPos;
                    if (!canReachAdjacentTo(state, player, target.position, adjPos)) {
                        noteBlitzWasted(0);
                        return ActionResult::fail();
                    }
                    bestNext = pickApproachStep(state, player, player.position,
                                                target.position);
                    if (bestNext.x < 0) { noteBlitzWasted(0); return ActionResult::fail(); }
                }
                

                Position beforeStep = player.position;
                ActionResult moveResult = resolveMoveStep(state, action.playerId,
                                                           bestNext, dice, events);
                if (moveResult.turnover) { noteBlitzWasted(2); return moveResult; }
                if (!moveResult.success) { noteBlitzWasted(1); return moveResult; }

                // Check if player is still standing (might have been knocked down)
                if (player.state != PlayerState::STANDING) { noteBlitzWasted(3); return ActionResult::turnovr(); }

                // A step that reports success without actually moving the player
                // (e.g. caught by Tentacles: resolveMoveStep returns ok() but the
                // player stays at `from`, see move_handler.cpp's checkTentacles)
                // would otherwise retry the identical step forever — this loop has
                // no other progress guard. Treat no-progress as "can't reach",
                // consistent with the other bail-out paths above.
                if (player.position == beforeStep) { noteBlitzWasted(4); return ActionResult::fail(); }
            }

            // Now adjacent — perform block
            if (player.position.distanceTo(target.position) != 1) {
                noteBlitzWasted(5);
                return ActionResult::fail();
            }

            BlockParams params;
            params.attackerId = action.playerId;
            params.targetId = action.targetId;
            params.isBlitz = true;
            params.hornsBonus = true; // Horns applies on blitz
            return resolveBlock(state, params, dice, events);
        }

        case ActionType::PASS: {
            return resolvePass(state, action.playerId, action.target, dice, events);
        }

        case ActionType::HAND_OFF: {
            return resolveHandOff(state, action.playerId, action.targetId, dice, events);
        }

        case ActionType::FOUL: {
            return resolveFoul(state, action.playerId, action.targetId, dice, events);
        }

        case ActionType::THROW_TEAM_MATE: {
            return resolveThrowTeamMate(state, action.playerId, action.targetId,
                                        action.target, dice, events);
        }

        case ActionType::BOMB_THROW: {
            return resolveBombThrow(state, action.playerId, action.target, dice, events);
        }

        case ActionType::HYPNOTIC_GAZE: {
            return resolveHypnoticGaze(state, action.playerId, action.targetId, dice, events);
        }

        case ActionType::BALL_AND_CHAIN: {
            return resolveBallAndChain(state, action.playerId, dice, events);
        }

        case ActionType::LEAP: {
            // F12: dosud sem nevedla zadna cesta. l. 8283 -- jednou za kolo.
            Player& p = state.getPlayer(action.playerId);
            if (p.leapUsedThisTurn) return ActionResult::fail();
            // T5.32 (26.08.): priznak se UZ NENASTAVUJE tady. Z validace
            // v resolveLeap vedou tri cesty k fail() (vzdalenost, obsazene
            // pole, strop GFI) a kazda z nich brala hracovi skok na cele
            // kolo, ac se zadny skok nekonal. Propaluje se az v resolveLeap,
            // tesne pred agility hodem -- tedy ve chvili, kdy se skok
            // SKUTECNE pokousi.
            return resolveLeap(state, action.playerId, action.target, dice, events);
        }

        case ActionType::MULTIPLE_BLOCK: {
            // targetId encodes first target, target.x/y encodes second target ID
            // We use targetId for first target and target position's x as second target ID
            return resolveMultipleBlock(state, action.playerId, action.targetId,
                                        action.target.x, dice, events);
        }

        case ActionType::END_TURN: {
            resolveEndTurn(state, events);
            return ActionResult::ok();
        }

        default:
            return ActionResult::fail();
    }
}

ActionResult executeAction(GameState& state, const Action& action,
                           DiceRollerBase& dice, std::vector<GameEvent>* events) {
    // Activation close-out at the actor-switch boundary: a successful MOVE never
    // sets hasActed (only failure paths do), so without this a player who moved
    // could be independently reactivated later in the same team-turn (free
    // blitz / second action bug, see evidence/fable_hasacted_bug_20260715.md).
    // When a DIFFERENT player starts acting, the previous player's activation is
    // over: if they had moved, mark them as having acted. Continuous multi-step
    // moves and move->pass/foul/score sequences by the SAME player are untouched.
    if (requiresPlayer(action.type) && action.playerId > 0) {
        if (state.currentActivationId > 0 &&
            state.currentActivationId != action.playerId) {
            Player& prev = state.getPlayer(state.currentActivationId);
            if (prev.hasMoved) prev.hasActed = true;
        }
        state.currentActivationId = action.playerId;
    }

    ActionResult result = resolveAction(state, action, dice, events);

    // Auto end turn on turnover
    if (result.turnover) {
        state.turnoverPending = true;
        resolveEndTurn(state, events, /*wasTurnover=*/true);
    }

    // Check touchdown
    if (checkTouchdown(state)) {
        TeamSide scoringSide = state.getPlayer(state.ball.carrierId).teamSide;
        state.getTeamState(scoringSide).score++;
        state.phase = GamePhase::TOUCHDOWN;
        emitEvent(events, {GameEvent::Type::TOUCHDOWN, state.ball.carrierId, -1,
                          state.ball.position, {}, 0, true});
    }

    // Check half over
    if (checkHalfOver(state)) {
        if (state.half >= 2) {
            state.phase = GamePhase::GAME_OVER;
        } else {
            state.phase = GamePhase::HALF_TIME;
        }
    }

    return result;
}

} // namespace bb
