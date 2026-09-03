#include "bb/macro_actions.h"
#include "bb/move_handler.h"   // Q3: rozpad turnoveru uvnitr uteku (03.09.)
#include "bb/action_resolver.h"
#include "bb/helpers.h"
#include <algorithm>
#include <cmath>

namespace bb {

// --- Helper functions ---

static int endzoneX(TeamSide side) {
    return (side == TeamSide::HOME) ? 25 : 0;
}

static int distToEndzone(Position pos, TeamSide side) {
    return std::abs(pos.x - endzoneX(side));
}

// Direction toward endzone: +1 or -1 in X
static int forwardDx(TeamSide side) {
    return (side == TeamSide::HOME) ? 1 : -1;
}

// Find the ball carrier for the active team, or nullptr
static const Player* findCarrier(const GameState& state) {
    if (!state.ball.isHeld || state.ball.carrierId <= 0) return nullptr;
    const Player& p = state.getPlayer(state.ball.carrierId);
    if (p.teamSide != state.activeTeam) return nullptr;
    if (!p.isOnPitch()) return nullptr;
    return &p;
}

// Score a MOVE action: lower is better.
// Prefers: close to target, no enemy TZ, no GFI.
// LEAP do makrove chuze (rodina M), 26.08.2026. Default OFF -- pri OFF je
// admise bajtove identicka s dneskem, takze nulovy test je cisty.
// ⛔ Deklarace MUSI byt nad findMoveToward, ktery citac inkrementuje;
// ostatni ramena jsou deklarovana az u svych setteru nize a to tu neslo.
thread_local bool g_leapWalk[2] = {false, false};
thread_local long g_leapWalkPicks = 0;

static int scoreMoveAction(const GameState& state, const Action& a,
                           Position target, int playerId) {
    const Player& p = state.getPlayer(playerId);
    int dist = a.target.distanceTo(target);

    // Enemy tackle zones at destination
    int destTZ = countTacklezones(state, a.target, p.teamSide);

    // Currently in TZ? (leaving requires dodge regardless of destination)
    bool currentlyInTZ = countTacklezones(state, p.position, p.teamSide) > 0;

    // GFI penalty: movementRemaining <= 0 means GFI roll needed
    bool needsGfi = (p.movementRemaining <= 0);

    // Score: distance * 10 + TZ penalty + GFI penalty
    // Distance is primary (each square = 10 points)
    // TZ penalty must exceed distance savings (10 per square) to prefer going around
    int score = dist * 10;

    // LEAP (26.08.2026) -- mluvi TOUTEZ menou jako MOVE, jinak se uvnitr
    // jednoho vyberu porovnava hruska s jablkem.
    //   6 * (leapTarget - 1) == 36 * p_fail: AG4 wardancer (cil 3+) = +12,
    //   tedy tak drahy jako jeden hlidany krok; AG3 (cil 4+) = +18.
    // ZADNY odchozi dodge se neuctuje (l. 8277-8278): tahle funkce dodge
    // neoceňuje ani u MOVE (plati ho resolver), takze uspora se projevi sama.
    // Cil hodu je BEZ modifikatoru za TZ cile (l. 8276-8277) -- destTZ se
    // proto pricita jen k POZICNI cene mezicile, ne k obtiznosti hodu.
    if (a.type == ActionType::LEAP) {
        score += 6 * (calculateLeapTarget(p) - 1);
        // Skok stoji 2 pole MA; kolik z nich je nad ramec => GFI hody.
        int gfiRolls = std::max(0, 2 - std::max(0, static_cast<int>(p.movementRemaining)));
        score += 8 * gfiRolls;
        if (a.target != target) {
            if (destTZ > 0) score += (currentlyInTZ ? 12 : 20) * destTZ;
            if (a.target.y <= 1 || a.target.y >= 13) score += 6;
        }
        return score;
    }
    // Stepping ONTO the walk's target square is never penalized for tackle
    // zones or sidelines (2026-08-04): the macro that chose the target owns
    // that risk decision (cage corners deliberately stand next to
    // defenders), and the pre-execution probes price it. Penalizing the
    // final square made walkers stop one short of any marked slot forever.
    if (a.target == target) {
        if (needsGfi) score += 8;
        return score;
    }
    // Straight-route tiebreak (2026-08-04, sibling of the cage corner-pick
    // Manhattan tiebreak): at equal Chebyshev distance the adjacency order
    // used to pick a DIAGONAL drift first, wasting a square of walk budget --
    // an exact-reach walk (dist == MA+GFI, e.g. the cage carrier's tempo-
    // emergency leg) then stops one square off target every time. The
    // surplus is capped under one distance point so it can never override
    // a genuinely shorter or safer square.
    int manhattan = std::abs(a.target.x - target.x) + std::abs(a.target.y - target.y);
    score += std::min(manhattan - dist, 9);
    if (destTZ > 0 && !currentlyInTZ) {
        score += 20 * destTZ;  // Entering enemy TZ from safe = very bad
    } else if (destTZ > 0) {
        // Already dodging: prefer TZ-free dest even if 1 square farther
        // 12 > 10 means going around (1 sq farther, 0 TZ) beats through (1 sq closer, 1 TZ)
        score += 12 * destTZ;
    }
    if (needsGfi) {
        score += 8;  // GFI is risky
    }
    // Sideline penalty: avoid Y=0 and Y=14 (Frenzy crowd-surf risk)
    if (a.target.y <= 1 || a.target.y >= 13) {
        score += 6;  // mild sideline avoidance
    }
    return score;
}

// Find available MOVE action toward a target position.
// Prefers safe routes (avoids enemy tackle zones and GFI).
static bool findMoveToward(const std::vector<Action>& actions, int playerId,
                           Position target, Action& bestMove,
                           const GameState* state = nullptr,
                           Position avoid = {-1, -1}) {
    int bestScore = 9999;
    bool found = false;

    // --- LEAP admise (26.08.2026, rameno setLeapWalkArm, default OFF) ------
    // Bez predikatu by greedy krokove skore srovnavalo akci o DVOU polich
    // s akci o jednom a na volnem hristi by "vyhodne" skakalo tam, kam se
    // dojde zadarmo dvema kroky. Skok ma cenu jen tam, kde neco NAHRAZUJE:
    //   (a) hrac stoji v nepratelske TZ  -> skok nahrazuje dodge (l. 8277-8278)
    //   (b) zadny MOVE nesnizuje vzdalenost -> je obezdeny a skok je jedina cesta
    bool allowLeap = false;
    if (state) {
        const Player& mv = state->getPlayer(playerId);
        if (leapWalkArm(mv.teamSide)) {
            if (countTacklezones(*state, mv.position, mv.teamSide) > 0) {
                allowLeap = true;                     // (a)
            } else {
                int here = mv.position.distanceTo(target);
                bool anyProgress = false;
                for (auto& a : actions) {
                    if (a.type != ActionType::MOVE || a.playerId != playerId) continue;
                    if (avoid.x >= 0 && a.target == avoid) continue;
                    if (a.target.distanceTo(target) < here) { anyProgress = true; break; }
                }
                allowLeap = !anyProgress;             // (b)
            }
        }
    }

    for (auto& a : actions) {
        bool isLeap = (a.type == ActionType::LEAP);
        if (isLeap && !allowLeap) continue;
        if (!isLeap && a.type != ActionType::MOVE) continue;
        if (a.playerId != playerId) continue;
        // Hard exclusion (item 11): a square the caller must never enter,
        // even as a mere waypoint -- landing on a loose ball's square
        // auto-triggers a real pickup roll regardless of intent.
        if (avoid.x >= 0 && a.target == avoid) continue;

        int score;
        if (state) {
            score = scoreMoveAction(*state, a, target, playerId);
        } else {
            // Fallback: pure distance (no safety check)
            score = a.target.distanceTo(target) * 10;
        }

        if (score < bestScore) {
            bestScore = score;
            bestMove = a;
            found = true;
        }
    }
    // Tika az kdyz skok VYHRAL vyber -- pouhe pripusteni mezi kandidaty by
    // tikalo i tam, kde stejne vyhral MOVE, a citac by prestal znamenat
    // "zmenena volba".
    if (found && bestMove.type == ActionType::LEAP) ++g_leapWalkPicks;
    return found;
}

// Check if a player is standing, on pitch, free to act
static bool isFreeToAct(const Player& p) {
    return p.canAct() && !p.hasMoved;
}

// See takeDauntlessOfferEvalsInSearch(): incremented only on the candidate arm, since
// the branch that touches it is gated on dauntlessInOffer.
thread_local long g_dauntlessOffers = 0;

// --- P35 arm: price the blitz block from the square the blitzer LANDS on ---
//
// getBlockDiceCount counts DEFENDER assists around the attacker's square. For a
// plain BLOCK that square is where the block is thrown from, so it is right. For
// a BLITZ it is not: action_resolver.cpp:86-118 walks the blitzer adjacent to
// the target FIRST and block_handler.cpp:491 counts the assists there. A blitzer
// standing in the open has zero defender assists at home and can pick up several
// on arrival -- so the ranking that chooses WHICH blitzer to send prices a block
// that is not the one thrown.
//
// The executor already knows this: the comment at action_resolver.cpp:89-91 says
// "fewer enemies next to the blitzer = fewer defender assists on the block, see
// getBlockDiceCount", which is why pickApproachStep is TZ-aware. The route
// respected the dependency; the candidate ranking did not. Corpus 2026-08-19,
// 27 928 reconstructed blitzes: the dice bracket changes in 16.2 % of them and
// flips from "we choose" to "the opponent chooses" in 9.7 % -- most often
// +1 -> -2, about 0.9 blitzes per game thrown with the sign reversed.
//
// Per side, default off, so an A/B pairs cleanly. The counter ticks only when
// the arm actually changes which blitzer is sent, i.e. it is a null-arm test in
// the sense of 2026-08-17: zero means both arms ran the same code.
// --- P38 arm: the carrier's destination square is derived from the cage it
// would produce, instead of the cage being fitted around wherever the carrier
// happened to stop (user, 2026-08-19: "podle toho, kde bude stat nosic v nasem
// kole, prece dopocitame vse vcetne toho, aby byly rohy ciste").
//
// expandAdvance picks a y-offset by counting tackle zones along the route and
// nothing else: the four squares that will BE the cage never enter the choice.
// Corpus 2026-08-19, 19 964 turns: a reachable square from which a full clean
// cage can be built -- four corners, all clean, no other neighbour of the
// carrier, and four free bodies that can actually reach those corners -- exists
// in 95.6 % of turns, and in 25.7 % of those the carrier is already standing on
// one. We satisfy the rule in 2.7 %. Body budget blocks it in 3.7 %, the
// opponent in 0.7 %; the rest is the choice of square.
//
// ⚠️ Tempo is not for sale here. K9a (schedule floor) is the strongest
// predictor we have (20.7 sigma), so the arm only ever ranks squares that give
// up AT MOST ONE square of forward progress against the best available -- it
// changes WHICH square the carrier ends on, not how far it goes.
// ⭐ M12, KROK 1 (30.08.2026): MĚŘIDLO, KTERÉ NEMĚNÍ CHOVÁNÍ.
// Otázka zní: kolikrát `ADVANCE` rezignuje (záložní smyčka stáhne `steps` na 0)
// a KOLIKRÁT Z TOHO existovalo volné pole bez tacklezóny s postupem >= 1
// uvnitř rozpočtu, jen MIMO PŘÍMKU? To rozhodne, jestli je (A/C) VADA, nebo
// taktická volba -- a rozhodne to měřením, ne úvahou.
// ⛔ Schválně mimo rameno: měřidlo nesmí viset na tom, jestli se rozhoduje
//    (T5.34). Tenhle čítač tiká v PRODUKCI, i když jsou všechna ramena vypnutá.
// ⭐ Q3, KROK 1 (30.08.2026): MĚŘIDLO KE VSTÁVÁNÍ, které NEMĚNÍ CHOVÁNÍ.
// Q3 je zodpovězená doktrinálně (21.08., tři větve a cena na hráči), ale
// plánovač neexistuje -- `P45` vstávání nabízí BEZPODMÍNEČNĚ a jeho vlastní
// komentář to přiznává. Než se začne oceňovat, je potřeba vědět, jak často
// se vstává v té DRAHÉ situaci: vedle stojícího soupeře, kde se hráč převede
// z „stojím ho blitz" na „dávám mu blok zadarmo".
// ⛔ Měří se NABÍDKA i PROVEDENÍ zvlášť, a rozdíl mezi nimi je ta odpověď:
//   nabídka je bezpodmínečná, takže když provedení tu drahou situaci samo
//   obchází, hledání ji už umí ocenit a plánovač je méně naléhavý.
thread_local long g_standOffered = 0;
thread_local long g_standOfferedNextToEnemy = 0;
// ⭐ 30.08. (uživatel): „vstát vedle někoho s MB je ještě horší než jen vstát
//   vedle někoho." Jeden koš na všechny sousedy je TÁŽ chyba průměrování, jakou
//   zakazuje pravidlo `OCEŇOVÁNÍ`: soused s Mighty Blow / Claw / Piling On
//   nemá stejnou cenu jako obyčejný -- zvedá hod na zbroj i na zranění, takže
//   „dostanu ránu zdarma" se u něj mění na „dostanu DRAHOU ránu zdarma".
thread_local long g_standOfferedNextToHitter = 0;
// Q3 krok A (31.08.2026): druha moznost -- VSTAT A ODEJIT. Meri se ZVLAST,
// kolikrat se nabidla a kolikrat NESLA (zadne pole bez tacklezony v dosahu),
// protoze "moznost neexistuje" a "moznost se nevybrala" jsou RUZNE nalezy.
// ⭐⭐ Q3 (03.09.2026): CO STOJI VSTAVANI. Noc 02.->03.09. skoncila -0,0933
//   (-10,86 sigma). Pri prehodnoceni se ukazaly TRI vady, a zadnou z nich
//   ta noc nemerila:
//   (1) `worstReplyCost` ocenuje jen CILOVE pole -- dodge ani GFI ne, pritom
//       odchod z kontaktu vzdy vyvola dodge (move_handler.cpp:104) a rameno
//       planuje i s GFI => az tri hody, kazdy neuspech TURNOVER CELEMU TYMU.
//   (2) `stayEarnsItsKeep` zna jen dva pripady, oba vazane na mic -- ZED v nem
//       neni (uzivatel 03.09.: „vstat a nechat se prastit je dulezite zachovat
//       pro trpaslika ve stene").
//   (3) rameno dela DVE VECI NARAZ: prida moznost utect A odebere „zustat".
//       ⇒ noc nemuze rict, ktera pulka skodila. Uzivatel 03.09.: „to je proti
//         zasade jedna zmena a kontrola."
//   ⭐ Meri se OBE VETVE, jinak je cislo neinterpretovatelne: „utek ma X %
//     turnoveru" nic nerika bez „zustat ma Y %".
thread_local long g_q3EscTried = 0, g_q3EscTurnover = 0;
// ⭐ Rozpad turnoveru UVNITR uteku. Celkovy rozpad (POHYB/TURNOVER-PRICINA)
//   je pres cely beh vcetne bezne chuze, takze se utekum PRIPSAT NESMI.
//   Tady se citace ctou tesne pred chuzi a tesne po ni, takze rozdil patri
//   vyhradne tomuhle jednomu makru.
thread_local long g_q3EscDodge = 0, g_q3EscGfi = 0;
thread_local long g_q3StayTried = 0, g_q3StayTurnover = 0;

thread_local long g_standEscapeOffered = 0;
thread_local long g_standEscapeImpossible = 0;

// ============================================================================
// Q3 KROK B (31.08.2026): OCENENI TRI VETVI VSTAVANI -- rameno, default OFF.
//
// ⭐ DOKTRINA (uzivatel, 20.08. a znovu 30.08.): vetev se oceni NEJSILNEJSI
//   odpovedi, kterou souper OPRAVDU MA, omezenou vzacnosti zdroje:
//     blitz 1/kolo · faul 1/kolo (zvlastni pridel) · BLOK NEOMEZENE
//   ⇒ poradi nejhorsi odpovedi na tri vetve vstavani:
//     vstat a zustat vedle -> BLOK ZDARMA (neomezeny)      nejhorsi
//     vstat a odejit       -> BLITZ (1/kolo, jen na dosah) stredni
//     zustat lezet         -> FAUL (1/kolo, zvlast)        nejlevnejsi
//
// ⭐ ZMERENO 31.08. (evidence/q3_standup_response_20260831.txt), takze to
//   NENI uvaha: na vstale hrace dopada odveta z 78,2 % jako BLOK a jen
//   z 21,8 % jako blitz, a 54,7 % ran konci srazenim. Kdyby to bylo
//   obracene, tohle rameno by nemelo duvod existovat.
//
// ⛔ RAMENO NEDELA "vzdy utec". Bere jen vetev, kterou doktrina oznacuje za
//   PRISNE NEJHORSI a u ktere zaroven existuje nahrada: vstat a zustat vedle
//   soupere, ktery ma MightyBlow / Claw / PilingOn (uzivatel 30.08.: "vstat
//   vedle nekoho s MB je jeste horsi nez jen vstat vedle nekoho"), kdyz je
//   kam utect. Bez uteku se nabidka NEBERE -- alternativa by byla zustat
//   lezet a to je absence nabidky, ne jina nabidka.
// ⚠️ "Zustat lezet" ma vlastni cenu (chybejici telo v kleci), kterou tahle
//   meridla NEZMERI. Proto se rameno drzi jen tam, kde je nahrada.
// ============================================================================
// ⛔ RAMENO PRO BLITZOVOU CHUZI (M14b, 01.09.2026), default OFF, mode 15.
//   ⚠️ VRACENO 02.09.: pri odebirani ramene M13 jsem mazal blok mezi dvema
//     kotvami a tohle rameno lezelo UVNITR -- linker to chytil hned
//     ("undefined reference to blitzPathArm"). Mazat podle kotev je levne,
//     ale musi se overit, CO mezi nimi je.
//   Stav: ZAMITNUTO dvema nezavislymi merenimi (parove A/B -0,1667 +- 0,1076;
//   a hladova chuze je z 97,6 % uz optimalni, takze nema co opravovat).
//   Zustava za vypinacem, noc si nezaslouzi.
thread_local bool g_blitzPath[2] = {false, false};
void setBlitzPathArm(TeamSide side, bool on) {
    g_blitzPath[side == TeamSide::HOME ? 0 : 1] = on;
}
bool blitzPathArm(TeamSide side) {
    return g_blitzPath[side == TeamSide::HOME ? 0 : 1];
}

// ⭐ M13 NASAZENO 02.09.2026 — rameno `setProneActionArm` odebrano po noci
// 01.->02.09. (2 400 paru): +0,0048 +- 0,0084, tedy neodlisitelne od nuly.
// Citac picku ZUSTAVA jako DIAGNOSTIKA (tyz princip jako cageSnapshot,
// T5.34: meridlo nesmi viset na tom, jestli se rozhoduje) -- rika, jak casto
// lezici hraci vubec jednaji, a to je udaj o desce, ne o rameni.
thread_local long g_proneActionPicks = 0;
void noteProneActionTaken() { ++g_proneActionPicks; }
long takeProneActionPicksInSearch() {
    long v = g_proneActionPicks; g_proneActionPicks = 0; return v;
}

thread_local bool g_standPricing[2] = {false, false};
thread_local long g_standPricingRepicks = 0;

void setStandUpPricingArm(TeamSide side, bool on) {
    g_standPricing[side == TeamSide::HOME ? 0 : 1] = on;
}

bool standUpPricingArm(TeamSide side) {
    return g_standPricing[side == TeamSide::HOME ? 0 : 1];
}

// Tika jen kdyz rameno OPRAVDU zmenilo nabidku (vzalo vetev "vstat a zustat").
// Nula pres matchup = obe ramena nabidla totez => nulovy test.
long takeStandUpPricingRepicksInSearch() {
    long v = g_standPricingRepicks; g_standPricingRepicks = 0; return v;
}

long takeStandOfferedInSearch() {
    long v = g_standOffered; g_standOffered = 0; return v;
}
long takeStandOfferedNextToEnemyInSearch() {
    long v = g_standOfferedNextToEnemy; g_standOfferedNextToEnemy = 0; return v;
}
long takeStandOfferedNextToHitterInSearch() {
    long v = g_standOfferedNextToHitter; g_standOfferedNextToHitter = 0; return v;
}

// ⭐ Q3: [0] utek zkusen [1] z toho TURNOVER [2] „vstat a zustat" zkuseno [3] z toho TURNOVER
void takeQ3StandUpCost(long* out6) {
    out6[0]=g_q3EscTried;  out6[1]=g_q3EscTurnover;
    out6[2]=g_q3StayTried; out6[3]=g_q3StayTurnover;
    out6[4]=g_q3EscDodge;  out6[5]=g_q3EscGfi;
    g_q3EscTried=g_q3EscTurnover=g_q3StayTried=g_q3StayTurnover=0;
    g_q3EscDodge=g_q3EscGfi=0;
}

long takeStandEscapeOfferedInSearch() {
    long v = g_standEscapeOffered; g_standEscapeOffered = 0; return v;
}

long takeStandEscapeImpossibleInSearch() {
    long v = g_standEscapeImpossible; g_standEscapeImpossible = 0; return v;
}

thread_local long g_advanceResigned = 0;        // smyčka stáhla steps na 0
thread_local long g_advanceResignedButSideFree = 0;  // ...a přitom bylo volno vedle

long takeAdvanceResignedInSearch() {
    long v = g_advanceResigned; g_advanceResigned = 0; return v;
}
long takeAdvanceResignedButSideFreeInSearch() {
    long v = g_advanceResignedButSideFree; g_advanceResignedButSideFree = 0; return v;
}

thread_local bool g_cageAwareAdvance[2] = {false, false};
thread_local long g_cageAwareAdvancePicks = 0;
// ⭐ REPICKY KLECOVÉHO KRITÉRIA (30.08.2026) -- vlastní čítač pro (B).
// ⛔ PROČ NESTAČÍ `g_cageAwareAdvancePicks`: tiká i PLACEBU, protože oba
//   projdou týž blok. V běhu „rameno proti placebu" (mode 12) by tedy obě
//   strany hlásily „rameno jednalo" a leak test i jmenovatel by ztratily
//   smysl. Tenhle čítač tiká, JEN kdyz kritérium ZMĚNILO VOLBU oproti tomu,
//   co by se vybralo bez něj -- táž oprava, jakou dostalo P35 (25.08.)
//   a B2 (27.08.) z téhož důvodu.
thread_local long g_cageCritRepicks = 0;

thread_local bool g_placeboAdvance[2] = {false, false};

void setCageAwareAdvanceArm(TeamSide side, bool on) {
    const int i = side == TeamSide::HOME ? 0 : 1;
    g_cageAwareAdvance[i] = on;
    // P40: the two arms differ by ONE predicate, so having both on would
    // measure their sum and call it either name. Turning one on clears the
    // other by construction rather than by remembering to.
    if (on) g_placeboAdvance[i] = false;
}

// P40 placebo: same search, no cage criterion. Shares the pick counter with
// P38 on purpose -- the counter answers "did the arm move the target square",
// which is the same question for both, and a matchup with zero picks is a null
// arm either way.
void setPlaceboAdvanceArm(TeamSide side, bool on) {
    const int i = side == TeamSide::HOME ? 0 : 1;
    g_placeboAdvance[i] = on;
    if (on) g_cageAwareAdvance[i] = false;
}

bool placeboAdvanceArm(TeamSide side) {
    return g_placeboAdvance[side == TeamSide::HOME ? 0 : 1];
}

bool cageAwareAdvanceArm(TeamSide side) {
    return g_cageAwareAdvance[side == TeamSide::HOME ? 0 : 1];
}

long takeCageAwareAdvancePicksInSearch() {
    long v = g_cageAwareAdvancePicks;
    g_cageAwareAdvancePicks = 0;
    return v;
}

long takeCageCritRepicksInSearch() {
    long v = g_cageCritRepicks; g_cageCritRepicks = 0; return v;
}



void setLeapWalkArm(TeamSide side, bool on) {
    g_leapWalk[side == TeamSide::HOME ? 0 : 1] = on;
}

bool leapWalkArm(TeamSide side) {
    return g_leapWalk[side == TeamSide::HOME ? 0 : 1];
}

long takeLeapWalkPicksInSearch() {
    long v = g_leapWalkPicks;
    g_leapWalkPicks = 0;
    return v;
}


// ⛔ 30.08.: čítače ramene B2 odstraněny spolu s ramenem. 01.09. totéž pro
// P35 (`g_blitzLandingRepicks`) -- rameno nasazeno, čítač odešel s ním.
// Poučení z obou ale PLATÍ a drží se u `g_proneActionPicks`
// a `g_standPricingRepicks`: čítač musí tikat, jen když se ZMĚNILA VOLBA,
// ne když se jen lišila cena -- jinak noc hlásí „rameno jednalo" o rameni,
// které se jen dívalo.


// ⭐ DIAGNOSTIKA, NE RAMENO (30.08.2026). Vypínač `setWrestlePricingArm` padl
// spolu s nasazením ceny -- čítač zůstal. Je to týž princip jako u `T5.34`
// (`cageSnapshot`): MĚŘIDLO NESMÍ VISET NA TOM, JESTLI SE ROZHODUJE. Bez něj
// by na otázku „jak často vůbec oceňujeme blok proti obránci, který Wrestle
// POUŽIJE" neuměl odpovědět nikdo -- a nula z chybějícího měřidla se od nuly
// z chybějícího jevu nepozná.
thread_local long g_wrestleDefenderPriced = 0;

long takeWrestleDefenderPricedInSearch() {
    long v = g_wrestleDefenderPriced;
    g_wrestleDefenderPriced = 0;
    return v;
}

thread_local bool g_blitzContinuation[2] = {false, false};
thread_local long g_blitzContinuationEvents = 0;

void setBlitzContinuationArm(TeamSide side, bool on) {
    g_blitzContinuation[side == TeamSide::HOME ? 0 : 1] = on;
}

bool blitzContinuationArm(TeamSide side) {
    return g_blitzContinuation[side == TeamSide::HOME ? 0 : 1];
}

void noteBlitzContinuationEvent() { ++g_blitzContinuationEvents; }

long takeBlitzContinuationEventsInSearch() {
    long v = g_blitzContinuationEvents;
    g_blitzContinuationEvents = 0;
    return v;
}

long takeDauntlessOfferEvalsInSearch() {
    long v = g_dauntlessOffers;
    g_dauntlessOffers = 0;
    return v;
}

// 2026-08-17 (P21): the same instrument for the hand-off swap. The corpus logs
// ZERO hand-offs in 3000 games while the situation the swap was written for
// occurs in at least 329 of our turns and the policy takes it in 18.3 % of
// planted positions. "Never offered" and "offered and never chosen" are
// different defects with different fixes -- the first is the gate, the second
// is the leaf evaluation -- and nothing could tell them apart.
thread_local long g_handOffOffers = 0;

long takeHandOffOfferEvalsInSearch() {
    long v = g_handOffOffers;
    g_handOffOffers = 0;
    return v;
}

// Count block dice for attacker vs defender
// attPos: the square the attacker throws the block FROM. Defaults to where he
// stands, which is correct for a BLOCK; a BLITZ must pass the landing square,
// because that is where block_handler will count the defender's assists (P35).
static int getBlockDiceCount(const GameState& state, const Player& att, const Player& def,
                             bool isBlitz, bool dauntlessInOffer,
                             Position attPos = Position{-1, -1}) {
    if (attPos.x < 0) attPos = att.position;
    int attST = att.stats.strength;
    int defST = def.stats.strength;
    if (isBlitz && att.hasSkill(SkillName::Horns)) attST += 1;
    // Dauntless, the same way block_handler resolves it: before assists, and
    // equalising onto the opponent's pre-assist strength. Leaving it out here
    // priced a Slayer beside a Black Orc as ST3 against ST4 -- uphill, negative
    // dice, offer discarded -- for a block that resolves at equal strength 83%
    // of the time (d6 + 3 > 4 is a 2+). The Slayer was never offered a block he
    // would mostly have won the strength contest for.
    //
    // The gate takes the equalised value rather than a probability-weighted one
    // because failing the roll is not a turnover, only a worse block, and the
    // search rolls the real Dauntless die when it expands the macro. The job
    // here is to stop excluding the option, not to price it exactly.
    //
    // It matters most exactly where it is most likely: the roll is a 2+ against
    // ST4, a 3+ against ST5 and a 4+ against a ST6 Treeman -- and orcs are the
    // only side we face fielding four ST4 Black Orcs.
    if (dauntlessInOffer && att.hasSkill(SkillName::Dauntless) && defST > attST) {
        attST = defST;
        // Mechanism counter, diagnostics only. A measurement has to be able to
        // show that the thing it measures actually happened -- the hand-off run
        // this morning reported "0 hand-offs" across 3000 games because the
        // event did not exist, and read as "no effect" rather than "no change".
        // The branch is only reachable with the flag on, i.e. in the candidate
        // arm, so the count needs no per-policy plumbing.
        ++g_dauntlessOffers;
    }
    int attAssists = countAssists(state, def.position, att.teamSide,
                                  att.id, def.id, def.id);
    int defAssists = countAssists(state, attPos, def.teamSide,
                                  def.id, att.id, att.id);
    BlockDiceInfo info = getBlockDiceInfo(attST + attAssists, defST + defAssists);
    return info.attackerChooses ? info.count : -info.count;
}

// Fraction of a single block die's 6 faces that are bad for the attacker
// (ATTACKER_DOWN always; BOTH_DOWN too unless the attacker has Block --
// mirrors block_handler.cpp's shouldRerollBlock). A simplified proxy for
// the chooser -- doesn't model defender Block/Dodge/Tackle nuance the way
// autoChooseBlockDie's scoreFace does, deliberately: this runs on every
// macro expansion during MCTS (thousands of times per search), so it needs
// to stay cheap, not skill-exact.
// B2 (25.08.2026): `defenderCanWrestle` je druhá polovina téhle otázky a do
// 25.08. tu nebyla. BB2016 ř. 8670-8676: Wrestle položí OBA hráče "even if one
// or both have the Block skill" -- útočníkův Block ho tedy před BOTH_DOWN
// NEZACHRÁNÍ a špatné jsou obě tváře kostky, ne jedna.
// ⚠️ Wrestle je VOLBA obránce, ne mechanika. Předpokládá se, že ji použije,
// když se mu vyplatí -- což u nositele BEZ Blocku platí vždy: padl by tak jako
// tak, a Placed Prone navíc nemá hod na zbroj. Korpus to potvrzuje: 4 923
// ze 4 923 Both Down proti `Lineman +Wrestle` položilo OBĚ těla.
// ⛔ U obránce, který má Block I Wrestle, je to skutečná volba a 2/6 je pak
// PŘÍSNÝ odhad; takového nositele ale žádná korpusová sestava nemá.
static double blockDieBadFraction(bool attackerHasBlock, bool defenderCanWrestle) {
    if (defenderCanWrestle) return 2.0 / 6.0;
    return attackerHasBlock ? (1.0 / 6.0) : (2.0 / 6.0);
}

// Estimated probability that the block itself goes badly for the attacker,
// from a diceCount as returned by getBlockDiceCount (positive = attacker
// chooses N dice, negative = defender chooses N dice).
static double estimateBlockFailChance(int diceCount, bool attackerHasBlock,
                                      bool defenderCanWrestle = false) {
    int n = std::abs(diceCount);
    if (n == 0) return 1.0;
    double bad = blockDieBadFraction(attackerHasBlock, defenderCanWrestle);
    if (diceCount > 0) {
        // Attacker picks the best of n dice -> fails only if ALL n are bad.
        return std::pow(bad, n);
    }
    // Defender picks the worst-for-attacker of n dice -> fails if ANY is bad.
    return 1.0 - std::pow(1.0 - bad, n);
}

// Approximate fail-probability for the APPROACH move to a square adjacent
// to `target` -- a mover with no Dodge skill crossing a crowded midfield
// can easily be riskier than the block itself, but expandBlitz's scoring
// historically had zero visibility into this (item 14). Walks the SAME
// route the BLITZ executor will actually take (pickApproachStep, shared --
// unified per the item7 post-implementation review so the estimate can't
// drift from execution again), accumulating dodge fail chance (via
// calculateDodgeTarget) for each square left while standing in an enemy
// tackle zone, plus GFI fail chance (1/6, natural 1) for each square
// beyond movementRemaining.
// landingOut (optional): the square the mover ends up on, i.e. the square the
// blitz block is actually thrown from. Same walk as the executor -- handing it
// back here is what keeps the P35 dice estimate from drifting away from the
// route again.
// ============================================================================
// Q3 KROK B (31.08.2026): NEJHORSI SOUPEROVA ODPOVED -- POCITANA, NE MERENA.
//
// ⭐ UZIVATEL 31.08.: "souperova nejhorsi odpoved se neda overit, dokud ji
//   soupere nenaucime -- ale co je spatne na tom mit to spocitane matematikou?"
//   Nic. Naopak: MERENI odpovida na otazku o SOUPEROVI, ne o vetvi.
//   Nas souper je nase vlastni AI, takze zmerene "co udelal" je DOLNI ODHAD
//   toho, co UDELAT SLO -- a ten se zlepsi pokazde, kdyz zesili soupere, cimz
//   by se ocenení pod nohama hybalo. Pocitany worst case je na sile soupere
//   NEZAVISLY a je to konzervativni mez, presne jak doktrina zada.
//   ⇒ Meridla z q3_standup_response_20260831 tuhle funkci NEOVERUJI a nemohou;
//     ukazuji jen, ze uz i dnesni slaby souper tu vetev trestá.
//
// Vzacnost zdroju (uzivatel 30.08.): blok NEOMEZENE · blitz 1/kolo ·
// faul 1/kolo (zvlastni pridel, takze neni tak vzacny jako blitz).
// Nasobky nize proto NEJSOU ladici parametry -- rikaji, kolikrat tutez ranu
// souper OPRAVDU muze zopakovat.
// ============================================================================
namespace {

// P(obrance jde k zemi) podle poctu blokovych kostek. Zaporne = vybira souper.
double knockdownChanceFromDice(int dice) {
    switch (dice) {
        case -3: return 0.09;
        case -2: return 0.14;
        case -1: return 0.28;   // jedna kostka, vybira nas souper
        case  1: return 0.44;   // jedna kostka
        case  2: return 0.69;
        default: return dice >= 3 ? 0.83 : 0.44;
    }
}

// Nasobek za to, ze rana od TOHOHLE hrace boli vic (uzivatel 30.08.:
// "vstat vedle nekoho s MB je jeste horsi nez jen vstat vedle nekoho").
double hurtAmplifier(const Player& e) {
    double amp = 1.0;
    if (e.hasSkill(SkillName::MightyBlow)) amp += 0.35;
    if (e.hasSkill(SkillName::Claw))       amp += 0.35;
    if (e.hasSkill(SkillName::PilingOn))   amp += 0.25;
    return amp;
}

// Vzacnost: blok muze souper hodit KAZDYM sousedem a nic ho to nestoji;
// blitz jen JEDNOU za kolo, takze tuz ranu "zaplati" a nemuze ji dat jinam.
constexpr double kBlockWeight = 1.00;
constexpr double kBlitzWeight = 0.45;
constexpr double kFoulWeight  = 0.30;

} // namespace

// Kolik nas stoji, kdyz nas hrac `p` stoji (nebo lezi) na poli `q`.
// Vraci NEJSILNEJSI odpoved, kterou souper opravdu ma -- ne prumer, ne to,
// co by nas soucasny souper skutecne zahral.
static double worstReplyCost(const GameState& state, const Player& p,
                             Position q, bool standing) {
    const TeamSide mySide = p.teamSide;
    double worst = 0.0;

    state.forEachOnPitch(opponent(mySide), [&](const Player& e) {
        if (e.state != PlayerState::STANDING || e.lostTacklezones) return;
        const int d = e.position.distanceTo(q);

        if (!standing) {
            // Lezici se NEDA blokovat ani blitzovat -- da se jen FAULOVAT,
            // a to jednou za kolo. r. 674-676 + katalog akci.
            if (d <= 1) {
                worst = std::max(worst, kFoulWeight * hurtAmplifier(e));
            }
            return;
        }

        const int dice = getBlockDiceCount(state, e, p, false, false);
        const double dmg = knockdownChanceFromDice(dice) * hurtAmplifier(e);

        if (d <= 1) {
            // BLOK ZDARMA: neomezeny zdroj, souper za nej neplati nicim.
            worst = std::max(worst, kBlockWeight * dmg);
        } else if (d <= static_cast<int>(e.stats.movement) + maxGfiSquares(e)) {
            // BLITZ: dosah vcetne GFI (P37), ale jen JEDEN za kolo.
            worst = std::max(worst, kBlitzWeight * dmg);
        }
    });
    return worst;
}

static double estimateApproachFailChance(const GameState& state, const Player& mover,
                                          Position target, Position* landingOut = nullptr) {
    if (landingOut) *landingOut = mover.position;
    if (mover.position.distanceTo(target) <= 1) return 0.0;

    Position cur = mover.position;
    // The blitz block itself costs 1 MP (CRP) -- reserve it so a
    // budget-tight approach correctly prices the trailing GFI risk.
    // ⛔⛔ M13b (01.09.2026): LEZICIMU SE MUSI ODECIST VSTANI.
    //   Do dneska tu stalo `mover.movementRemaining - 1`, takze lezici MA6 se
    //   ocenil, jako by mel po rezerve na blok PET poli -- ma DVE (r. 690-695,
    //   vstani stoji tri). GFI riziko proto zacalo pricitat o TRI pole pozdeji
    //   a lezici blitzer se jevil BEZPECNEJSI, NEZ JE.
    //   Do 31.08. to byla spici vada (lezici blitzovat nesmel); M13 ji
    //   probudila -- tataz trida jako Bone Head hozeny za kazde pole, kterou
    //   probudilo P45. Bez teto opravy by noc 01.09. merila M13 PRIZNIVEJI,
    //   nez zaslouzi, protoze planovac lezici blitzery podcenuje v riziku.
    int moveLeft = movementAfterStandUp(mover) - 1;
    double failChance = 0.0;

    for (int guard = 0; guard < 20 && cur.distanceTo(target) > 1; ++guard) {
        Position next = pickApproachStep(state, mover, cur, target);
        if (next.x < 0) return 1.0;  // executor would fail the blitz outright

        if (countTacklezones(state, cur, mover.teamSide) > 0) {
            int dodgeTarget = calculateDodgeTarget(state, mover, next, cur);
            double dodgeFail = std::clamp((dodgeTarget - 1) / 6.0, 0.0, 5.0 / 6.0);
            failChance += dodgeFail * (1.0 - failChance);
        }
        if (moveLeft <= 0) {
            failChance += (1.0 / 6.0) * (1.0 - failChance);
        }
        moveLeft -= 1;
        cur = next;
        if (landingOut) *landingOut = cur;
    }
    return failChance;
}

// Combined estimate used to rank blitzer candidates for a fixed target:
// block-dice risk and approach risk are treated as independent enough for
// a cheap combination. Lower is better (0 = certain success).
// Testovaci pristup k oceneni DOBEHU. Vystaveno schvalne: bez nej by se
// M13b (vstani se musi odecist) dalo hlidat jen neprimo pres volbu blitzujiciho,
// a takovy test by prosel i pri spatne cene, kdyz by volba nahodou vysla stejne.
double blitzApproachRiskForTest(const GameState& state, const Player& mover,
                                const Player& target) {
    return estimateApproachFailChance(state, mover, target.position);
}

static double estimateBlitzFailChance(const GameState& state, const Player& blitzer,
                                       const Player& target) {
    // Risk estimate and feature extraction keep the raw strengths: they describe
    // the block as thrown, not whether to offer it.
    Position landing{-1, -1};
    double approachFail = estimateApproachFailChance(state, blitzer, target.position,
                                                     &landing);
    int diceCount = getBlockDiceCount(state, blitzer, target, true, false,
                                      landing);   // P35: VZDY z ciloveho pole
    // B2: obráncův Wrestle se do ceny promítne jen POD RAMENEM, aby šel rozdíl
    // změřit párovým A/B. Čítač tiká jen tam, kde se cena OPRAVDU liší -- tedy
    // když útočník Block má; bez Blocku vychází 2/6 v obou případech.
    //
    // ⛔ 27.08.: PODMÍNKA MUSÍ BÝT TÁŽ JAKO V RESOLVERU. Do dneška tu stálo
    //   pouhé `target.hasSkill(Wrestle)` -- tedy „Wrestle MÁ", ne „Wrestle by
    //   POUŽIL". Dokud Block+Wrestle neměl nikdo, byl v tom rozdíl nulový;
    //   27.08. dostaly všechny týmy dva linemany s Wrestle a u trpaslíka jsou
    //   to Longbeardi, kteří Block MAJÍ, takže ta větev ožila.
    //   Resolver (`block_handler.cpp`, case BOTH_DOWN) rozhoduje takto:
    //     defWantsWrestle = Wrestle && (!Block || attHasBall)
    //   -- obránce s Blockem by Both Down přestál VESTOJE a útočníka složil,
    //   což je lepší než jít k zemi s ním; sáhne po Wrestle jen tehdy, když
    //   útočník drží míč (jeho položení je turnover).
    //   ⇒ Kdyby vybírač počítal 2/6 i tam, kde by obránce Wrestle NEPOUŽIL,
    //     oceňuje JINOU VĚC, NEŽ SE PAK STANE -- táž vada, před kterou varuje
    //     `autoChooseBlockDie` o dvě stě řádků výš.
    const bool attHasBall = state.ball.isHeld &&
                            state.ball.carrierId == blitzer.id;
    const bool defWouldWrestle = target.hasSkill(SkillName::Wrestle) &&
                                 (!target.hasSkill(SkillName::Block) || attHasBall);
    // ⭐ NASAZENO 30.08.2026 (B2). Do téhle změny cena visela na rameni
    //   `setWrestlePricingArm`, které se v produkci nikdy nezapínalo -- engine
    //   tedy hrál dál s cenou, o které je DOLOŽENO, že je špatná: r. 8670-8676
    //   položí při Both Down OBA hráče, i když jeden nebo oba mají Block,
    //   takže špatné jsou DVĚ šestiny, ne jedna (2 kostky: 11,1 %, ne 2,8 %).
    //   Noc 29.->30.08. změřila, že oprava nic nezhorší: delta +0,0010 ± 0,0034,
    //   celé 95% CI [-0,0057; +0,0078] uvnitř prahu ±0,015 => EKVIVALENCE.
    // ⛔ Rameno TÍM padá. „Neškodí" není důvod nechat v kódu vypínač, který
    //   nikdo nikdy nepřepne (uživatel 30.08.); a nechat místo toho v enginu
    //   prokazatelně špatnou cenu by bylo implementovat VÝSLEDEK místo
    //   PRAVIDLA -- vada, kterou projekt vede pod
    //   `feedback_implement_the_rule_not_the_outcome`.
    const bool defWrestle = defWouldWrestle;
    if (defWrestle && blitzer.hasSkill(SkillName::Block)) ++g_wrestleDefenderPriced;
    double blockFail = estimateBlockFailChance(diceCount, blitzer.hasSkill(SkillName::Block),
                                               defWrestle);
    return 1.0 - (1.0 - blockFail) * (1.0 - approachFail);
}

// Find nearest free standing teammate to a position (excluding specific player)
static const Player* findNearestFreePlayer(const GameState& state, Position target,
                                            int excludeId = -1) {
    const Player* best = nullptr;
    int bestDist = 999;
    state.forEachOnPitch(state.activeTeam, [&](const Player& p) {
        if (p.id == excludeId) return;
        if (!isFreeToAct(p)) return;
        if (p.hasSkill(SkillName::BallAndChain)) return;
        int d = p.position.distanceTo(target);
        if (d < bestDist) {
            bestDist = d;
            best = &p;
        }
    });
    return best;
}

// --- Macro Generation ---

void getAvailableMacros(const GameState& state, std::vector<Macro>& out,
                        bool dauntlessInOffer) {
    out.clear();

    if (state.phase != GamePhase::PLAY) return;

    TeamSide mySide = state.activeTeam;
    const TeamState& myTeam = state.getTeamState(mySide);

    // Always: END_TURN
    out.push_back({MacroType::END_TURN, -1, -1, {-1, -1}});

    // STAND UP (2026-08-21). Prone players were invisible to this whole layer:
    // isFreeToAct() requires STANDING, so no macro ever touched them and
    // nobody ever got up -- 1 067 of 280 719 prone player-turns on the
    // 3 000-game corpus (0.4 %), at EVERY MA, not just the Treeman's 2. Each
    // knockdown therefore removed a body for the rest of the drive, for both
    // teams. Emitted as a REPOSITION onto the player's own square; the walk in
    // movePlayerToward now treats "prone and already there" as unfinished.
    // Deliberately NO new MacroType: MACRO_COUNT is a positional feature-vector
    // width, and this fix must not move it.
    // ⚠️ UNCONDITIONAL ON PURPOSE, AND THAT IS NOT THE FINAL SHAPE. Standing up
    // next to an enemy converts the player from "costs them a BLITZ" into
    // "gives them a free BLOCK" (user doctrine 2026-08-20), so it is strictly
    // more expensive than staying down; sometimes the point is to stand and
    // dodge AWAY (a dwarf beside a Mighty Blow orc) -- and only if that body is
    // needed elsewhere. See Q3 in evidence/task_queue.md; this is the version
    // that makes the rule reachable, not the version that prices it.
    state.forEachOnPitch(mySide, [&](const Player& p) {
        if (p.state != PlayerState::PRONE) return;
        if (p.hasActed || p.hasMoved || p.lostTacklezones) return;
        if (p.hasSkill(SkillName::BallAndChain)) return;
        // Q3 měřidlo: nabídnuto -- a je vedle něj STOJÍCÍ soupeř?
        ++g_standOffered;
        bool nextToEnemy = false, nextToHitter = false;
        for (const Position& adj : p.position.getAdjacent()) {
            if (!adj.isOnPitch()) continue;
            const Player* e = state.getPlayerAtPosition(adj);
            if (!e || e->teamSide == mySide || e->state != PlayerState::STANDING) continue;
            nextToEnemy = true;
            if (e->hasSkill(SkillName::MightyBlow) || e->hasSkill(SkillName::Claw) ||
                e->hasSkill(SkillName::PilingOn)) {
                nextToHitter = true; break;   // horší případ, dál hledat netřeba
            }
        }
        if (nextToEnemy) ++g_standOfferedNextToEnemy;
        if (nextToHitter) ++g_standOfferedNextToHitter;

        // Q3 krok B: nabidka "vstat a ZUSTAT" se vypousti VZDY, krome jedineho
        // pripadu pod ramenem -- viz komentar u `setStandUpPricingArm`.
        // Rozhodnuti se musi udelat AZ POTOM, co je znamo, jestli utek vubec
        // existuje, jinak by rameno bralo nabidku i tam, kde nahrada neni.
        bool offerStay = true;
        // Tika JEDNOU za hrace, i kdyz rameno zmenilo obe veci naraz --
        // jinak by se jeden zasah pocital dvakrat a jmenovatel by lhal.
        bool armChangedOffer = false;

        // ⭐ Q3 KROK A (31.08.2026): DRUHA MOZNOST -- VSTAT A ODEJIT.
        // Uzivatelova doktrina (20.08.): vstat vedle soupere meni hrace
        // z "stoji je BLITZ" na "dava jim BLOK ZDARMA", takze je to prisne
        // drazsi nez zustat lezet; nekdy je pointa vstat a ODSKOCIT
        // (trpaslik vedle orka s Mighty Blow).
        // ⛔ Ta moznost do dneska v MAKRO vrstve NEEXISTOVALA: obecna
        //   REPOSITION smycka zada isFreeToAct() (tedy STOJICIHO) A zadneho
        //   souseda, takze lezici u soupere dostal jedinou nabidku -- vstat
        //   a zustat. Rozhodovalo se tedy mezi DVEMA moznostmi ze TRI.
        //   Tohle je verze, ktera moznost ZPRISTUPNUJE; ocenit ji je krok B.
        // ⚠️ Nabizi se JEN vedle soupere: bez kontaktu neni pred cim utikat
        //   a obecna smycka si stojiciho hrace prevezme sama v pristim kole.
        // Q3: cela vetev "vstat a odejit" (krok A) i jeji ocenení (krok B) sedi
        // pod JEDNIM vypinacem. Nabidnout utek bez ocenení je pulka zmeny --
        // planovac by dostal moznost navic, ale duvod, proc si ji vybrat, ne.
        if (nextToEnemy && standUpPricingArm(mySide)) {
            const int budget = movementAfterStandUp(p) + maxGfiSquares(p);
            const bool priced = true;
            Position best{-1, -1};
            int bestSteps = 99;
            double bestCost = 1e9;
            for (int dx = -budget; dx <= budget; ++dx) {
                for (int dy = -budget; dy <= budget; ++dy) {
                    if (dx == 0 && dy == 0) continue;
                    Position cand{static_cast<int8_t>(p.position.x + dx),
                                  static_cast<int8_t>(p.position.y + dy)};
                    if (!cand.isOnPitch()) continue;
                    if (state.getPlayerAtPosition(cand) != nullptr) continue;
                    const int steps = p.position.distanceTo(cand);
                    if (steps > budget) continue;
                    // Cilem je VEN Z KONTAKTU, ne "o kus dal": pole s cizi
                    // tacklezonou by hrace nechalo v teze situaci.
                    if (countTacklezones(state, cand, mySide) > 0) continue;
                    if (priced) {
                        // ⭐ Pod ramenem se pole vybira podle NEJHORSI ODPOVEDI,
                        //   ne podle vzdalenosti. Pole bez tacklezony jeste
                        //   nemusi byt bezpecne -- souper na nej muze dosahnout
                        //   BLITZEM, a to zadna vzdalenost nerekne.
                        const double c = worstReplyCost(state, p, cand, true);
                        if (c < bestCost || (c == bestCost && steps < bestSteps)) {
                            bestCost = c; bestSteps = steps; best = cand;
                        }
                    } else if (steps < bestSteps) {
                        bestSteps = steps; best = cand;
                    }
                }
            }
            if (best.x >= 0) {
                ++g_standEscapeOffered;
                // ⛔⛔ AUDIT 01.09.: SIGNAL BYL NEUPLNY. Citac tikal jen kdyz
                //   rameno VZALO nabidku "zustat" -- jenze rameno dela DVE
                //   veci a druha je, ze nabidku "odejit" vubec PRIDA (cely
                //   tenhle blok je pod vypinacem). Par, kde rameno jen pridalo
                //   utek a planovac si ho vybral, by hlasil "arm acted 0" nad
                //   POHNUTOU hrou => LEAK TEST BY KRICEL NA VLASTNI RAMENO
                //   a noc by se nesmela precist.
                //   ⇒ Signal je "rameno ZMENILO NABIDKU", at uz jakkoli.
                //   Tataz vada jako u M13 (31.08.) -- druhe rameno za dva dny.
                armChangedOffer = true;
                out.push_back({MacroType::REPOSITION, p.id, -1, best});
                // ⭐ Rameno bere nabidku "vstat a zustat" jen tehdy, kdyz je
                //   POCITANE nejhorsi odpovedi PRISNE DRAZ nez utek. Zadna
                //   heuristika typu "ma MB" -- MightyBlow uz sedi uvnitr
                //   `hurtAmplifier`, a stejne tak sila, pocet asistenci
                //   a dosah blitzu, ktere by se z priznaku vycist nedaly.
                // ⚠️ Prah 1e-9 je jen ochrana proti rovnosti v plovouci
                //   carce, ne ladici parametr: pri SHODNE cene se nabidka
                //   NEBERE, protoze "zustat" ma vlastni hodnotu (telo drzi
                //   pole v kleci), kterou tenhle vypocet nezna.
                // ⛔⛔ CENA JE RELATIVNI, NE ABSOLUTNI (uzivatel 31.08.:
                //   "blok zdarma, kdyz zavazim postupu ballcarriera, je
                //   relativni cena"). `worstReplyCost` umi jen RIZIKO; neumi,
                //   CO TO TELO NA TOM POLI DELA. Kdyz stanim znacim souperova
                //   nosice nebo drzim roh vlastni klece, je blok zdarma cena,
                //   kterou PLATIM ZA NECO -- a vetev se brat nesmi.
                //   ⇒ Bez teto podminky by rameno rozpustilo prave ty pozice,
                //     kvuli kterym se telo zvedá.
                bool stayEarnsItsKeep = false;
                if (state.ball.isHeld && state.ball.carrierId > 0) {
                    const Player& bc = state.getPlayer(state.ball.carrierId);
                    if (bc.isOnPitch()) {
                        const int dToCarrier = p.position.distanceTo(bc.position);
                        if (bc.teamSide != mySide && dToCarrier <= 1) {
                            stayEarnsItsKeep = true;   // znacim souperova nosice
                        } else if (bc.teamSide == mySide && dToCarrier <= 2) {
                            stayEarnsItsKeep = true;   // drzim roh vlastni klece
                        }
                    }
                }
                if (priced && !stayEarnsItsKeep &&
                    worstReplyCost(state, p, p.position, true) > bestCost + 1e-9) {
                    offerStay = false;
                    armChangedOffer = true;
                }
            } else {
                ++g_standEscapeImpossible;
            }
        }

        if (offerStay) out.push_back({MacroType::REPOSITION, p.id, -1, p.position});
        if (armChangedOffer) ++g_standPricingRepicks;
    });

    const Player* carrier = findCarrier(state);
    bool iHaveBall = (carrier != nullptr);
    bool ballOnGround = !state.ball.isHeld && state.ball.isOnPitch();

    // M1/N10 second half (25.08.2026): the blitzer whose activation is STILL
    // OPEN. resolveBlock now leaves hasActed clear for a blitzer who is still
    // on his feet (l. 347-350, "the block may be made at any point during the
    // move"), but the general REPOSITION loop below rejects him TWICE:
    // isFreeToAct() demands !hasMoved, and the loop returns early for anyone
    // adjacent to a standing enemy -- which is exactly what a blitzer is once
    // he has thrown his block. Permission without an offer is the P45 shape
    // again (a finished resolver nobody can reach), so the retreat is emitted
    // HERE, separately, leaving the general gates untouched.
    //
    // M9 measured the size on 18 000 games (24.08.): 4.09 blitzes a game end
    // with the blitzer stuck in contact although he has MA left AND a free
    // square to go to, and it happens to AV7 pieces (Wardancer, Catcher,
    // Gutter Runner) 1.5x more often than to AV9.
    //
    // `usedBlitz` is the marker rather than hasMoved: it means "declared a
    // blitz", so this cannot hand a second move to everyone who merely walked.
    //
    // ⚠️ SCOPE -- this is the RETREAT only. The user's other case, "the carrier
    // opens his own lane with a blitz and runs through it", needs SCORE/ADVANCE
    // to accept a mid-activation player and is a bigger change; the carrier is
    // skipped here and keeps his own macros.
    // ⚠️ No GFI: a retreat bought with a Go For It is a gamble, not hygiene,
    // and M9's ceiling counted only squares reachable on real movement.
    state.forEachOnPitch(mySide, [&](const Player& p) {
        if (!blitzContinuationArm(mySide)) return;
        if (!p.canAct() || !p.usedBlitz) return;
        if (p.hasSkill(SkillName::BallAndChain)) return;
        if (iHaveBall && p.id == carrier->id) return;
        if (p.movementRemaining <= 0) return;
        if (countTacklezones(state, p.position, mySide) == 0) return;  // not exposed

        // Nearest free square that is outside EVERY enemy tackle zone. The
        // target is geometric and the executor walks it, exactly like every
        // other REPOSITION target here -- if the walk stalls, the blitzer ends
        // up no worse than he already is.
        int reach = p.movementRemaining;
        Position best{-1, -1};
        int bestDist = 99;
        for (int dy = -reach; dy <= reach; ++dy) {
            for (int dx = -reach; dx <= reach; ++dx) {
                if (dx == 0 && dy == 0) continue;
                Position cand{static_cast<int8_t>(p.position.x + dx),
                              static_cast<int8_t>(p.position.y + dy)};
                if (!cand.isOnPitch()) continue;
                if (state.getPlayerAtPosition(cand) != nullptr) continue;
                if (countTacklezones(state, cand, mySide) != 0) continue;
                int d = p.position.distanceTo(cand);
                if (d < bestDist) { bestDist = d; best = cand; }
            }
        }
        if (best.x >= 0) {
            noteBlitzContinuationEvent();
            out.push_back({MacroType::REPOSITION, p.id, -1, best});
        }
    });

    // SCORE: carrier can reach endzone with MA + 2 GFI
    if (iHaveBall && carrier->canAct()) {
        int dist = distToEndzone(carrier->position, mySide);
        int maxReach = carrier->movementRemaining + maxGfiSquares(*carrier); // +2 GFI
        if (dist <= maxReach && dist > 0) {
            out.push_back({MacroType::SCORE, carrier->id, -1, {-1, -1}});
        }
    }

    // HAND_OFF_SCORE: carrier stuck/in heavy TZ, nearby teammate can score
    // 2026-08-17 (P4/P26): hand-off má vlastní limit na kolo, ne sdílený s pass.
    if (iHaveBall && carrier->canAct() && !myTeam.handOffUsedThisTurn) {
        int carrierDist = distToEndzone(carrier->position, mySide);
        int carrierMaxReach = carrier->movementRemaining + maxGfiSquares(*carrier);
        int carrierTZ = countTacklezones(state, carrier->position, carrier->teamSide);
        bool carrierStuck = (carrierDist > carrierMaxReach) || (carrierTZ >= 2 && carrierDist > 0);

        if (carrierStuck) {
            state.forEachOnPitch(mySide, [&](const Player& teammate) {
                if (teammate.id == carrier->id) return;
                if (teammate.state != PlayerState::STANDING) return;
                if (teammate.hasActed) return;
                if (teammate.hasSkill(SkillName::NoHands)) return;

                int adjDist = carrier->position.distanceTo(teammate.position);
                if (adjDist > 2) return; // carrier must reach adjacency within 1 move

                int receiverDist = distToEndzone(teammate.position, mySide);
                int receiverMaxReach = teammate.movementRemaining + maxGfiSquares(teammate);
                if (receiverDist > 0 && receiverDist <= receiverMaxReach) {
                    out.push_back({MacroType::HAND_OFF_SCORE, carrier->id, teammate.id, {-1, -1}});
                }
            });
        }
    }

    // PASS_SCORE: carrier stuck, pass (longer range) to teammate who can score
    if (iHaveBall && carrier->canAct() && !myTeam.passUsedThisTurn) {
        int carrierDist = distToEndzone(carrier->position, mySide);
        int carrierMaxReach = carrier->movementRemaining + maxGfiSquares(*carrier);
        int carrierTZ = countTacklezones(state, carrier->position, carrier->teamSide);
        bool carrierStuck = (carrierDist > carrierMaxReach) || (carrierTZ >= 2 && carrierDist > 0);

        if (carrierStuck) {
            int bestScore = -999;
            int bestTargetId = -1;
            state.forEachOnPitch(mySide, [&](const Player& teammate) {
                if (teammate.id == carrier->id) return;
                if (teammate.state != PlayerState::STANDING) return;
                if (teammate.hasActed) return;
                if (teammate.hasSkill(SkillName::NoHands)) return;

                int passDist = carrier->position.distanceTo(teammate.position);
                if (passDist < 2 || passDist > 10) return; // hand-off is separate; max pass ~10

                int receiverDist = distToEndzone(teammate.position, mySide);
                int receiverMaxReach = teammate.movementRemaining + maxGfiSquares(teammate);
                if (receiverDist <= 0 || receiverDist > receiverMaxReach) return;

                int score = teammate.stats.agility * 5 - passDist;
                if (teammate.hasSkill(SkillName::Catch)) score += 5;
                if (score > bestScore) {
                    bestScore = score;
                    bestTargetId = teammate.id;
                }
            });
            if (bestTargetId > 0) {
                out.push_back({MacroType::PASS_SCORE, carrier->id, bestTargetId, {-1, -1}});
            }
        }
    }

    // CHAIN_SCORE: carrier passes to relay, relay hand-offs to scorer near endzone
    // 2026-08-17 (P4/P26): potřebuje OBĚ akce volné -- přihrávku i hand-off.
    // Dokud sdílely jeden příznak, byl tenhle řetěz nesplnitelný z definice:
    // krok 1 spálil to, co potřeboval krok 2. Nabídnut 270× za 3 000 her,
    // dokončen ani jednou.
    if (iHaveBall && carrier->canAct() && !myTeam.passUsedThisTurn
        && !myTeam.handOffUsedThisTurn) {
        int carrierDist = distToEndzone(carrier->position, mySide);
        int carrierMaxReach = carrier->movementRemaining + maxGfiSquares(*carrier);
        bool carrierStuck = (carrierDist > carrierMaxReach);

        if (carrierStuck) {
            int bestChainScore = -999;
            int bestRelayId = -1, bestScorerId = -1;

            state.forEachOnPitch(mySide, [&](const Player& relay) {
                if (relay.id == carrier->id) return;
                if (relay.state != PlayerState::STANDING || relay.hasActed) return;
                if (relay.hasSkill(SkillName::NoHands)) return;

                int passDist = carrier->position.distanceTo(relay.position);
                if (passDist < 1 || passDist > 10) return;

                state.forEachOnPitch(mySide, [&](const Player& scorer) {
                    if (scorer.id == carrier->id || scorer.id == relay.id) return;
                    if (scorer.state != PlayerState::STANDING || scorer.hasActed) return;
                    if (scorer.hasSkill(SkillName::NoHands)) return;

                    int adjDist = relay.position.distanceTo(scorer.position);
                    if (adjDist > 2) return; // relay must reach adjacency for hand-off

                    int scorerDist = distToEndzone(scorer.position, mySide);
                    int scorerMaxReach = scorer.movementRemaining + maxGfiSquares(scorer);
                    if (scorerDist <= 0 || scorerDist > scorerMaxReach) return;

                    int score = relay.stats.agility * 3 + scorer.stats.agility * 5
                              + scorer.stats.movement - passDist * 2;
                    if (relay.hasSkill(SkillName::Catch)) score += 5;
                    if (scorer.hasSkill(SkillName::Catch)) score += 3;
                    if (score > bestChainScore) {
                        bestChainScore = score;
                        bestRelayId = relay.id;
                        bestScorerId = scorer.id;
                    }
                });
            });

            if (bestRelayId > 0 && bestScorerId > 0) {
                Macro m{MacroType::CHAIN_SCORE, carrier->id, bestRelayId, {-1, -1}};
                m.thirdId = bestScorerId;
                out.push_back(m);
            }
        }
    }

    // ADVANCE: carrier can move forward but can't score
    if (iHaveBall && carrier->canAct() && carrier->movementRemaining > 0) {
        int dist = distToEndzone(carrier->position, mySide);
        int maxReach = carrier->movementRemaining + maxGfiSquares(*carrier);
        if (dist > maxReach) {
            out.push_back({MacroType::ADVANCE, carrier->id, -1, {-1, -1}});
        }
    }

    // CAGE: have ball and at least one free teammate
    if (iHaveBall) {
        bool hasFreePlayer = false;
        state.forEachOnPitch(mySide, [&](const Player& p) {
            if (p.id != carrier->id && isFreeToAct(p) && !p.hasSkill(SkillName::BallAndChain)) {
                hasFreePlayer = true;
            }
        });
        if (hasFreePlayer) {
            out.push_back({MacroType::CAGE, carrier->id, -1, {-1, -1}});
        }
    }

    // BLITZ: not used this turn, at least one standing enemy
    // Defense-aware: prioritizes ball carrier and scoring threats
    if (!myTeam.blitzUsedThisTurn) {
        bool onDef = !iHaveBall && !ballOnGround; // opponent has ball
        int oppCarrierId = (state.ball.isHeld && state.ball.carrierId > 0)
                            ? state.ball.carrierId : -1;

        // Score each target (best blitzer for each)
        struct BlitzCandidate {
            int targetId;
            int bestScore;
        };
        std::vector<BlitzCandidate> candidates;

        state.forEachOnPitch(opponent(mySide), [&](const Player& def) {
            if (def.state != PlayerState::STANDING) return;

            int targetBestScore = -999;

            state.forEachOnPitch(mySide, [&](const Player& blitzer) {
                // M13 krok C (31.08.2026): BLITZ smi i LEZICI (r. 676 --
                // blitz neni Block Action). `isFreeToAct` zada STANDING, takze
                // makro vrstva by lezici blitzery vynechala, zatimco surova
                // vrstva (rules_engine, krok B) uz je nabizi. Ten nesoulad je
                // horsi nez obe krajnosti: rollouty by ocenovaly tahy, ktere
                // planovac nikdy nemuze vybrat.
                // ⚠️ !hasMoved zustava: kdo se uz hnul, blitz deklarovat nemuze.
                if (!blitzer.canDeclareAction() || blitzer.hasMoved) return;
                if (blitzer.hasSkill(SkillName::BallAndChain)) return;
                // Dosah se zkrati o vstani; pod 3 MA je vstani hod na 4+ a
                // pohyb pak nula, takze blok plati GFI (r. 690-695).
                if (blitzer.state == PlayerState::PRONE) {
                    const int gfiB = blitzer.rooted ? 0 : maxGfiSquares(blitzer);
                    if (movementAfterStandUp(blitzer) + gfiB
                        < blitzer.position.distanceTo(def.position)) return;
                }

                int dice = getBlockDiceCount(state, blitzer, def, true, dauntlessInOffer);
                int score = dice * 2;

                // Sideline trap: target near sideline = fewer escape routes
                if (def.position.y <= 2 || def.position.y >= Position::PITCH_HEIGHT - 3) {
                    score += 3;
                } else if (def.position.y <= 4 || def.position.y >= Position::PITCH_HEIGHT - 5) {
                    score += 1;
                }

                if (onDef) {
                    // DEFENSE: ball carrier is top priority
                    if (def.id == oppCarrierId) {
                        score += 10;
                    }
                    // Opponent scoring threat (can score this turn)
                    int oppEzX = (def.teamSide == TeamSide::HOME) ? 25 : 0;
                    int distEz = std::abs(def.position.x - oppEzX);
                    if (def.stats.movement + 2 >= distEz) {
                        score += 4;
                    }
                    // Free opponent (no friendly TZ on them) — more dangerous
                    if (countTacklezones(state, def.position, def.teamSide) == 0) {
                        score += 2;
                    }
                } else {
                    // OFFENSE: near carrier bonus + ball carrier bonus
                    if (iHaveBall && def.position.distanceTo(carrier->position) <= 2) {
                        score += 2;
                    }
                    if (state.ball.isHeld && state.ball.carrierId == def.id) {
                        score += 5;
                    }
                }

                if (score > targetBestScore) {
                    targetBestScore = score;
                }
            });

            if (targetBestScore > -999) {
                candidates.push_back({def.id, targetBestScore});
            }
        });

        // Sort by score descending
        std::sort(candidates.begin(), candidates.end(),
                  [](const BlitzCandidate& a, const BlitzCandidate& b) {
                      return a.bestScore > b.bestScore;
                  });

        // On defense: top 2 targets for MCTS choice; on offense: top 1
        int maxBlitz = (onDef && candidates.size() > 1) ? 2 : 1;
        maxBlitz = std::min(maxBlitz, static_cast<int>(candidates.size()));
        for (int i = 0; i < maxBlitz; ++i) {
            out.push_back({MacroType::BLITZ, -1, candidates[i].targetId, {-1, -1}});
        }
    }

    // BLITZ_AND_SCORE: carrier can almost reach endzone, but opponent blocks path
    // Blitz the blocker out of the way, then move carrier to score
    //
    // ⛔ T5.35a (27.08.2026): DOSAH SE UŽ NEPŘIPOČÍTÁVÁ „S REZERVOU".
    //   Podmínka zněla `dist <= maxReach + 3`. To `+3` je fudge ke slovu
    //   „almost" v komentáři výše -- jenže krok 2 tohohle makra nosiče
    //   DOVEDE DO ENDZÓNY, a „skoro" TD nedává. Změřeno na korpusu 25.08.
    //   (3 000 her, M10): z 1 281 nabídek jich 944 padlo do kol, kde je
    //   BLITZ_AND_SCORE jediná skórující cesta -- a ve VŠECH 944 byl nosič
    //   dál než `MA + 2 GFI`, průměrně 10,0 pole. TD tam padlo 0,6 % a bylo
    //   JEDNO, co nosič udělal (blitz 0/21, blok bez kroku 0/376, jen krok
    //   4/328, nic 2/219; turnover shodně 32-38 %).
    //   ⇒ Nabízel se tah, který v tom kole nelze dokončit -- a P27 na tom
    //     vzorku deset dní měřilo „vadu ve volbě". Vada je v ADMISI.
    //   ⇒ Šlo tedy jen o to, přestat měřit vlastní šum: to, co zbude, je
    //     teprve vzorek, kde jde něco rozeznat.
    // ⚠️ GFI se bere STEJNĚ jako v rules_engine.cpp:36 a pathfinder.cpp:39
    //   (Sprint dává tři), aby zúžení neodmítlo nabídku, kterou hráč
    //   SKUTEČNĚ dokáže doběhnout. Blok při blitzu stojí pole pohybu, ale
    //   platí ho zpravidla SPOLUHRÁČ: výběr blitzujícího se od nosiče
    //   schválně odklání (expandBlitzAndScore, tie-break `isCarrier`).
    if (iHaveBall && carrier->canAct() && !myTeam.blitzUsedThisTurn) {
        int dist = distToEndzone(carrier->position, mySide);
        int gfi = carrier->rooted ? 0
                : maxGfiSquares(*carrier);
        int maxReach = carrier->movementRemaining + gfi;
        int ezX = endzoneX(mySide);
        int dx = forwardDx(mySide);

        // Carrier can't directly score (SCORE not available) or would need to go through enemies
        if (dist > 0 && dist <= maxReach) {
            // Find opponent on the path between carrier and endzone
            int bestBlocker = -1;
            int bestBlockerDist = 999;
            state.forEachOnPitch(opponent(mySide), [&](const Player& def) {
                if (def.state != PlayerState::STANDING) return;
                // Is the defender roughly between carrier and endzone?
                int defDist = distToEndzone(def.position, mySide);
                int carrierDistEz = distToEndzone(carrier->position, mySide);
                if (defDist >= carrierDistEz) return; // defender is behind carrier
                // Is defender close to carrier's path (within 2 Y)?
                int yDiff = std::abs(def.position.y - carrier->position.y);
                if (yDiff > 2) return;
                // Is defender in carrier's TZ or blocking the direct path?
                int xDist = std::abs(def.position.x - carrier->position.x);
                if (xDist <= 2 && xDist + yDiff <= 3) {
                    int totalDist = xDist + yDiff;
                    if (totalDist < bestBlockerDist) {
                        bestBlockerDist = totalDist;
                        bestBlocker = def.id;
                    }
                }
            });

            if (bestBlocker > 0) {
                out.push_back({MacroType::BLITZ_AND_SCORE, carrier->id, bestBlocker, {-1, -1}});
            }
        }
    }

    // BLOCK: favorable block (2+ dice, attacker chooses)
    state.forEachOnPitch(mySide, [&](const Player& att) {
        if (!att.canAct() || att.hasActed) return;
        if (att.hasSkill(SkillName::BallAndChain)) return;

        auto adj = att.position.getAdjacent();
        for (auto& pos : adj) {
            if (!pos.isOnPitch()) continue;
            const Player* def = state.getPlayerAtPosition(pos);
            if (!def || def->teamSide == mySide) continue;
            if (def->state != PlayerState::STANDING) continue;

            // One-die blocks were not merely rejected, they were never
            // generated -- the search could not weigh one even when it was
            // the right play. For a player with Block that is wrong: Both
            // Down costs him nothing, so the only turnover face left is
            // Attacker Down at 1/6, against a 4.6% casualty chance per
            // knockdown on AV8. The dwarf guide puts it plainly -- Block and
            // Tackle "make even 1D blocks with confidence" -- and a team of
            // AV9 Blockers throwing 1.26 blocks a turn is the reason our
            // attrition funnel starts too narrow to produce casualties
            // (2026-08-11: 0.87 casualties and 0.14 deaths a game, with the
            // casualty table itself verified exact against CRP).
            //
            // Without Block a one-die block is a 1/3 turnover and stays out,
            // and so do uphill blocks where the defender picks the die
            // (negative count). The ball carrier is excluded outright: his
            // turnovers are the ones that end drives.
            int dice = getBlockDiceCount(state, att, *def, false, dauntlessInOffer);
            bool oneDieWorthOffering =
                dice == 1 && att.hasSkill(SkillName::Block) &&
                !(state.ball.isHeld && state.ball.carrierId == att.id);
            if (dice >= 2 || oneDieWorthOffering) {
                out.push_back({MacroType::BLOCK, att.id, def->id, {-1, -1}});
            }
        }
    });

    // PICKUP: ball on ground, top-2 pickers by AG/distance/skills.
    // A single bestPicker candidate left the search with no alternative
    // recoverer at all: per-turn ground recovery sat at ~28% and the
    // near-ball pickup miss at 82% across two independent minings
    // (evidence/fable_replay_mining_findings.md 07-02, _20260714 07-14;
    // master-list item 7). The macro list is emitted BEST-FIRST -- the
    // prior-floor split in macro_mcts.cpp (expand(), case PICKUP) relies
    // on this ordering contract.
    if (ballOnGround) {
        const Player* bestPicker = nullptr;
        int bestPickerScore = -999;
        const Player* secondPicker = nullptr;
        int secondPickerScore = -999;
        // Gate: a second picker more than 25 points behind (~1.5 pips of
        // pickup chance, ~5 squares of extra distance) is categorically
        // worse -- emitting it would only donate floored prior mass.
        constexpr int kSecondPickerMaxGap = 25;

        state.forEachOnPitch(mySide, [&](const Player& p) {
            // M5/A7 (29.08.2026): lezici hrac smi vstat a UTRATIT ZBYTEK
            // pohybu -- r. 668-671, "at a cost of THREE SQUARES of his
            // movement ... The player may take ANY Action other than a Block
            // Action". P45 udelal vstavani dosazitelnym, ale jen NA MISTE
            // (REPOSITION na vlastni pole), takze lezici hrac u volneho mice
            // se k nemu ten tah nikdy nedostal. Executor to umi uz ted:
            // `movePlayerToward` bere "prone a uz na cili" jako nedokoncene,
            // postavi ho a pokracuje. Chybela jen NABIDKA -- tataz trida jako
            // F12 Leap, P45 vstavani a M4 Sprint.
            const bool prone = (p.state == PlayerState::PRONE);
            if (prone) {
                if (p.hasActed || p.hasMoved || p.lostTacklezones) return;
            } else if (!isFreeToAct(p)) {
                return;
            }
            if (p.hasSkill(SkillName::BallAndChain)) return;
            if (p.hasSkill(SkillName::NoHands)) return;

            // Rozpocet po vstani. Jump Up (r. 8196-8198) vstava zdarma;
            // pod 3 MA je vstani na 4+ a `resolveStandUp` pak nuluje pohyb
            // (move_handler.cpp:390, "any further step must be a GFI"), takze
            // zbytek je v obou tech pripadech presne to, co zbyde.
            const int afterStand = movementAfterStandUp(p);

            int dist = p.position.distanceTo(state.ball.position);
            int maxReach = afterStand + maxGfiSquares(p);
            if (dist > maxReach) return;

            // Rank by the COMPUTED chance of coming up with the ball, not by
            // a linear AG/skill proxy (2026-08-07, user doctrine "prefer the
            // reliable handler"). The proxy was blind to what makes a pickup
            // hard in practice: tackle zones ON THE BALL and weather. Those
            // hurt a low-AG body disproportionately (a marked ball costs an
            // AG2 dwarf a third of his chance but an AG4 runner far less),
            // so on exactly the situations the staged planner kept failing
            // (mined g0003) the proxy still nominated the nearest dwarf.
            // Priced by the engine's own target formula evaluated at the
            // ball square, plus Sure Hands' personal reroll; the team reroll
            // is deliberately NOT assumed (shared scarce resource). Distance
            // stays secondary at ~1 square = 5 chance points, keeping the
            // old trade-off (1 AG pip ~ 3.3 squares).
            int target = calculatePickupTargetAt(state, p, state.ball.position);
            double chance = (7.0 - target) / 6.0;
            if (p.hasSkill(SkillName::SureHands)) {
                chance += (1.0 - chance) * chance;  // personal reroll
            }
            int score = static_cast<int>(std::lround(chance * 100.0)) - dist * 5;

            if (score > bestPickerScore) {
                secondPickerScore = bestPickerScore;
                secondPicker = bestPicker;
                bestPickerScore = score;
                bestPicker = &p;
            } else if (score > secondPickerScore) {
                secondPickerScore = score;
                secondPicker = &p;
            }
        });

        if (!bestPicker) {
            // findNearestFreePlayer() is a generic helper (also used for CAGE
            // movement, where "can't reach this turn" is fine) -- it has no
            // reach or NoHands check. For PICKUP specifically, an unreachable
            // or NoHands fallback pick would emit a macro that's a guaranteed
            // wasted action (2026-07-02, project_bloodbowl_roadmap_20260702
            // Tier 1 item 3, Opus 4.8 finding). Apply the same checks the main
            // loop above already applies before accepting the fallback.
            const Player* fallback = findNearestFreePlayer(state, state.ball.position);
            if (fallback && !fallback->hasSkill(SkillName::NoHands) &&
                fallback->position.distanceTo(state.ball.position) <= fallback->movementRemaining + maxGfiSquares(*fallback)) {
                bestPicker = fallback;
            }
        }
        if (bestPicker) {
            out.push_back({MacroType::PICKUP, bestPicker->id, -1, state.ball.position});
        }
        // Secondary picker: only from the main loop (never from the
        // findNearestFreePlayer fallback -- secondPicker stays null there).
        if (secondPicker &&
            bestPickerScore - secondPickerScore <= kSecondPickerMaxGap) {
            out.push_back({MacroType::PICKUP, secondPicker->id, -1, state.ball.position});
        }
    }

    // PASS: have ball, the relevant action not used, teammate in range
    // 2026-08-17 (P4/P26): tohle makro se při expanzi provede jako HAND_OFF,
    // když je cíl soused, a jako hod jinak -- takže se hlídá ten limit, který
    // se opravdu spotřebuje. Vnější podmínka pustí dál, dokud je volný aspoň
    // jeden z nich; rozhodne se uvnitř, až je známá vzdálenost.
    if (iHaveBall && carrier->canAct()
        && !(myTeam.passUsedThisTurn && myTeam.handOffUsedThisTurn)) {
        state.forEachOnPitch(mySide, [&](const Player& target) {
            if (target.id == carrier->id) return;
            if (target.state != PlayerState::STANDING) return;
            int dist = carrier->position.distanceTo(target.position);
            int targetDist = distToEndzone(target.position, mySide);
            int carrierDist = distToEndzone(carrier->position, mySide);
            if (dist > 10 || dist < 1) return;

            // "Ahead of the carrier" is the right gate for a THROW, whose point
            // is ground. It is the wrong gate for a hand-off, whose point is
            // getting the ball off a man who should not be holding it -- and
            // pricing hand-offs correctly (38dcad6) without fixing the gate
            // made things worse, not better: measured over 3000 games, the
            // Longbeard share of carrying turns went from 1-4% up to 6-10%,
            // because any Longbeard standing one square further forward was a
            // legal target for a Runner. The offer answered "is he ahead" when
            // the question is "is he a better pair of hands".
            //
            // So the swap has its own gate, as the fix queue specified: the
            // criterion is "the carrier is wrong", not "the receiver is nicer".
            // Wrong means AG2 without Sure Hands -- our Longbeards, Blockers and
            // Slayers, who fumble a dodge half the time and have nothing to
            // re-roll it with. Better means a genuinely safer pair of hands,
            // not one point of anything. And the ball may not go backwards to
            // get there.
            const bool handOff = dist == 1;
            if (handOff ? myTeam.handOffUsedThisTurn : myTeam.passUsedThisTurn) return;
            auto poorHands = [](const Player& p) {
                return p.stats.agility <= 2 && !p.hasSkill(SkillName::SureHands);
            };
            // For a hand-off the swap gate REPLACES the ground gate rather than
            // joining it. Gating on "ahead" alone is what let a Runner give the
            // ball to a Longbeard standing one square further up, and forward is
            // the direction the damage actually ran in.
            const bool swap = poorHands(*carrier) && !poorHands(target) &&
                              targetDist <= carrierDist;
            if (!handOff && targetDist >= carrierDist) return;   // throws need ground

            // A pass was offered to anyone with the ball, at any agility,
            // toward any team-mate ahead of him. For this side that is a
            // standing invitation to lose the drive: measured on the 08-11
            // corpus, 21 turnovers came from our own AG2 throws and catches.
            // Two rolls at AG2 complete about a quarter of the time
            // (quick pass 50% x catch 50%); even a Runner to a Runner is 44%.
            //
            // So it is offered only when it is likely to work, OR when the
            // half ends without it anyway -- the user's rule is that dwarves
            // pass "only in an emergency". Emergency is not a mood: it is the
            // last two turns, the carrier out of reach of the endzone, and a
            // team-mate who is not.
            PassRange range;
            if (!passRangeFromOffset(target.position.x - carrier->position.x,
                                     target.position.y - carrier->position.y,
                                     range)) {
                return;
            }
            if (state.weather == Weather::BLIZZARD &&
                (range == PassRange::LONG_PASS || range == PassRange::LONG_BOMB)) {
                return;
            }
            int throwTarget = 7 - carrier->stats.agility - passModifier(range)
                            + countTacklezones(state, carrier->position, mySide);
            int catchTarget = 7 - target.stats.agility - 1   // accurate pass
                            + countTacklezones(state, target.position, mySide);
            auto chance = [](int t) {
                return (7.0 - static_cast<double>(std::clamp(t, 2, 6))) / 6.0;
            };
            // expandPass() performs a HAND_OFF whenever the target is adjacent
            // and only falls back to a throw beyond that -- but this filter
            // priced every offer as a throw, so it was vetoing an action nobody
            // was ever going to take. resolveHandOff (pass_handler.cpp) has no
            // throw roll at all and catches at the same +1 the accurate-pass
            // branch here already assumes, so at distance 1 the completion
            // chance simply IS the catch chance.
            //
            // Longbeard AG2 -> Blitzer AG3 in the clear was priced
            // 0.50 x 0.67 = 33% and thrown away; the hand-off it would have
            // performed is 67%. The comment above concedes the same thing
            // about "even a Runner to a Runner is 44%" -- that pair is 67% too.
            // Under the old price no hand-off between any two dwarves we field
            // cleared 0.5, so the macro was dead in every turn of every game,
            // and with it the only way to get the ball off a carrier who should
            // not be holding it (measured 2026-08-13).
            const double complete = handOff
                                  ? chance(catchTarget)
                                  : chance(throwTarget) * chance(catchTarget);

            const int turnsLeft = 9 - myTeam.turnNumber;
            const bool emergency =
                turnsLeft <= 2 &&
                carrierDist > static_cast<int>(carrier->movementRemaining) + 2 &&
                targetDist <= static_cast<int>(target.stats.movement) + 2;

            // A hand-off must clear BOTH gates: it has to be a swap worth making
            // and likely to arrive. The emergency clause stays common to both --
            // the half ends without it either way.
            const bool worthIt = handOff ? (swap && complete >= 0.5)
                                         : (complete >= 0.5);
            if (worthIt || emergency) {
                if (handOff) ++g_handOffOffers;   // P21, see takeHandOffOfferEvalsInSearch()
                out.push_back({MacroType::PASS_ACTION, carrier->id, target.id, {-1, -1}});
            }
        });
    }

    // FOUL: foul not used, prone/stunned enemy adjacent
    if (!myTeam.foulUsedThisTurn) {
        state.forEachOnPitch(mySide, [&](const Player& fouler) {
            if (!fouler.canAct() || fouler.hasActed) return;
            if (fouler.hasSkill(SkillName::BallAndChain)) return;

            auto adj = fouler.position.getAdjacent();
            for (auto& pos : adj) {
                if (!pos.isOnPitch()) continue;
                const Player* target = state.getPlayerAtPosition(pos);
                if (!target || target->teamSide == mySide) continue;
                if (target->state != PlayerState::PRONE &&
                    target->state != PlayerState::STUNNED) continue;
                out.push_back({MacroType::FOUL, fouler.id, target->id, {-1, -1}});
                return; // one foul macro per fouler is enough
            }
        });
    }

    // REPOSITION: free (no adjacent enemies) standing player
    // Smart targeting: carrier protection, safety player, defensive screen
    int myEndzone = endzoneX(opponent(mySide));  // our own endzone to defend
    bool onDefense = !iHaveBall && !ballOnGround;
    bool receiverPlaced = false;
    bool hunterPlaced = false;
    bool cageTagPlaced = false;
    bool interceptPlaced = false;
    bool safetyPlaced = false;
    bool markerPlaced = false;
    int turnsLeft = std::max(0, 9 - myTeam.turnNumber);
    int endzoneGuardCount = 0;
    int screenSlot = 0;

    // Pre-compute defensive info
    const Player* oppCarrierPtr = nullptr;
    int oppScoringThreatCount = 0;
    if (onDefense) {
        if (state.ball.isHeld && state.ball.carrierId > 0) {
            oppCarrierPtr = &state.getPlayer(state.ball.carrierId);
            if (!oppCarrierPtr->isOnPitch()) oppCarrierPtr = nullptr;
        }
        state.forEachOnPitch(opponent(mySide), [&](const Player& op) {
            if (op.state != PlayerState::STANDING) return;
            int oppEzX = (op.teamSide == TeamSide::HOME) ? 25 : 0;
            int dist = std::abs(op.position.x - oppEzX);
            if (op.stats.movement + 2 >= dist &&
                countTacklezones(state, op.position, op.teamSide) == 0) {
                oppScoringThreatCount++;
            }
        });
    }

    state.forEachOnPitch(mySide, [&](const Player& p) {
        if (!isFreeToAct(p)) return;
        if (p.hasSkill(SkillName::BallAndChain)) return;
        if (iHaveBall && p.id == carrier->id) return; // carrier has SCORE/ADVANCE

        // Check if player is free (no adjacent enemies)
        bool hasAdjacentEnemy = false;
        auto adj = p.position.getAdjacent();
        for (auto& pos : adj) {
            if (!pos.isOnPitch()) continue;
            const Player* other = state.getPlayerAtPosition(pos);
            if (other && other->teamSide != mySide && other->state == PlayerState::STANDING) {
                hasAdjacentEnemy = true;
                break;
            }
        }
        if (hasAdjacentEnemy) return;

        Position target;

        if (ballOnGround) {
            // Loose ball: surround it -- stand ADJACENT, never on the ball's
            // own square. Any move landing there auto-triggers a real pickup
            // roll (move_handler.cpp), which would make this the one
            // REPOSITION that secretly gambles; an actual pickup attempt is
            // the PICKUP macro's job (item 11). Already adjacent = already
            // denying, stay put.
            if (p.position.distanceTo(state.ball.position) == 1) {
                target = p.position;
            } else {
                Position bestAdj{-1, -1};
                int bestDist = 999;
                for (auto& apos : state.ball.position.getAdjacent()) {
                    if (!apos.isOnPitch()) continue;
                    if (state.getPlayerAtPosition(apos) != nullptr) continue;
                    int d = p.position.distanceTo(apos);
                    if (d < bestDist) {
                        bestDist = d;
                        bestAdj = apos;
                    }
                }
                if (bestAdj.x < 0) return;  // ball fully surrounded
                target = bestAdj;
            }
        } else if (iHaveBall) {
            // Offense: support carrier (cage corners, screen ahead, receiver setup)
            int dx = forwardDx(mySide);
            int carrierDist = p.position.distanceTo(carrier->position);
            int ezX = endzoneX(mySide);

            // Hunter/shield split: fast players (MA≥7) pressure opponent scoring threats
            // while slow players stay as shields near carrier
            if (!hunterPlaced && p.stats.movement >= 7 && carrierDist > 4) {
                Position huntTarget = carrier->position;
                int bestThreat = -999;
                state.forEachOnPitch(opponent(mySide), [&](const Player& opp) {
                    if (opp.state != PlayerState::STANDING) return;
                    int threat = opp.stats.movement * 2 + opp.stats.agility;
                    if (countTacklezones(state, opp.position, opp.teamSide) == 0) threat += 5;
                    if (threat > bestThreat) {
                        bestThreat = threat;
                        huntTarget = opp.position;
                    }
                });
                target = huntTarget;
                hunterPlaced = true;
            }
            // Receiver setup: when ≤2 turns left, send fast player near endzone
            // as a pass/hand-off target for next turn's scoring chain
            else if (!receiverPlaced && turnsLeft <= 2 && p.stats.movement >= 6 &&
                carrierDist > 3) {
                int recvY = carrier->position.y + ((p.position.y > carrier->position.y) ? 2 : -2);
                recvY = std::clamp(recvY, 2, 12);
                int recvX = ezX - dx * 3; // 3 squares from endzone (reachable next turn)
                recvX = std::clamp(recvX, 1, 24);
                target = {static_cast<int8_t>(recvX), static_cast<int8_t>(recvY)};
                receiverPlaced = true;
            } else if (carrierDist <= 3) {
                // Already near carrier — move to cage/screen position ahead of carrier
                target = {static_cast<int8_t>(carrier->position.x + dx * 2),
                          static_cast<int8_t>(carrier->position.y)};
            } else {
                // Far from carrier — move toward carrier
                target = carrier->position;
            }
        } else if (onDefense) {
            // Defense: safety + marker on carrier + endzone guard + screen
            Position oppBallPos = state.ball.isOnPitch() ? state.ball.position
                : Position{static_cast<int8_t>(endzoneX(mySide)), 7};

            // Strategy 0: Cage corner tag — break opponent cage (one player at a time)
            bool usedCageTag = false;
            if (!cageTagPlaced && oppCarrierPtr != nullptr) {
                int cageCount = 0;
                state.forEachOnPitch(opponent(mySide), [&](const Player& opp) {
                    if (opp.id == oppCarrierPtr->id) return;
                    if (opp.state != PlayerState::STANDING) return;
                    if (oppCarrierPtr->position.distanceTo(opp.position) == 1) cageCount++;
                });
                if (cageCount >= 2) {
                    Position bestCorner = oppCarrierPtr->position;
                    int minFriendlyTZ = 999;
                    auto adj = oppCarrierPtr->position.getAdjacent();
                    for (auto& apos : adj) {
                        if (!apos.isOnPitch()) continue;
                        if (state.getPlayerAtPosition(apos)) continue;
                        int friendlyTZ = countTacklezones(state, apos, opponent(mySide));
                        if (friendlyTZ < minFriendlyTZ) {
                            minFriendlyTZ = friendlyTZ;
                            bestCorner = apos;
                        }
                    }
                    if (bestCorner.x != oppCarrierPtr->position.x ||
                        bestCorner.y != oppCarrierPtr->position.y) {
                        target = bestCorner;
                        cageTagPlaced = true;
                        usedCageTag = true;
                    }
                }
            }
            // Strategies 0.5-4: only when cage tag not used this iteration
            if (!usedCageTag) {
            // Strategy 0.5: Intercept lane -- put a defender between the
            // carrier and the defended endzone in the carrier's ACTUAL Y
            // lane. The fixed safety spot (y=7) and the fixed screen Ys
            // {3,5,7,9,11} never cover a carrier sprinting the flank
            // (y=1-2 / 12-13), so nothing ever actually stood in his lane
            // (research_fable_20260709 section 3b, "screen=0" hole). One
            // interceptor per generation pass; cheap point geometry only,
            // no pathfinding -- MCTS arbitrates whether the macro is used.
            bool usedIntercept = false;
            if (!interceptPlaced && oppCarrierPtr != nullptr) {
                int dxOpp = forwardDx(opponent(mySide)); // carrier's attack direction
                // Intercept point: halfway between the carrier and the
                // defended endzone (same X idiom as Strategy 4's screenX),
                // in the carrier's own lane, clamped off the sidelines.
                int laneX = (oppCarrierPtr->position.x + myEndzone) / 2;
                int laneY = std::clamp(static_cast<int>(oppCarrierPtr->position.y), 1, 13);
                Position lane{static_cast<int8_t>(std::clamp(laneX, 1, 24)),
                              static_cast<int8_t>(laneY)};
                // Gate: only a defender who is goal-side of the carrier
                // (2-square slack to cut back in) and can reach the lane
                // within two activations commits to it; everyone else
                // falls through to Strategies 1-4 unchanged.
                bool goalSide =
                    (p.position.x - oppCarrierPtr->position.x) * dxOpp >= -2;
                if (goalSide && p.position.distanceTo(lane) <= p.stats.movement * 2) {
                    target = lane;
                    interceptPlaced = true;
                    usedIntercept = true;
                }
            }
            if (!usedIntercept) {
            // Strategy 1: Safety player (fast, near our endzone)
            if (!safetyPlaced && p.stats.movement >= 6) {
                target = {static_cast<int8_t>(myEndzone),
                          static_cast<int8_t>(7)};
                safetyPlaced = true;
            }
            // Strategy 2: Pressure marker — move toward opponent carrier
            else if (!markerPlaced && oppCarrierPtr != nullptr) {
                target = oppCarrierPtr->position;
                markerPlaced = true;
            }
            // Strategy 3: Endzone guard — prevent one-turn TD
            else if (oppScoringThreatCount > 0 && endzoneGuardCount < 2) {
                int guardX = myEndzone + forwardDx(mySide) * 4;
                int guardY = (endzoneGuardCount == 0) ? 5 : 9;
                target = {static_cast<int8_t>(std::clamp(guardX, 1, 24)),
                          static_cast<int8_t>(guardY)};
                endzoneGuardCount++;
            }
            // Strategy 4: Defensive screen — evenly spread between ball and endzone
            else {
                int screenX = (oppBallPos.x + myEndzone) / 2;
                static const int screenYs[] = {3, 5, 7, 9, 11};
                int screenY = screenYs[screenSlot % 5];
                screenSlot++;
                target = {static_cast<int8_t>(std::clamp(screenX, 1, 24)),
                          static_cast<int8_t>(screenY)};
            }
            } // end !usedIntercept
            } // end !usedCageTag
        } else {
            // Move forward toward center
            int dx = forwardDx(mySide);
            target = {static_cast<int8_t>(p.position.x + dx * 3),
                      static_cast<int8_t>(7)}; // center Y
        }

        out.push_back({MacroType::REPOSITION, p.id, -1, target});
    });
}

// --- Macro Expansion ---

// Execute a single action, add to result, return true if turnover
static bool executeAndRecord(GameState& state, const Action& action,
                             DiceRollerBase& dice, MacroExpansionResult& result) {
    result.actions.push_back(action);
    ActionResult ar = executeAction(state, action, dice, nullptr);
    if (ar.turnover) {
        result.turnover = true;
        return true;
    }
    return false;
}

// Find and execute MOVE actions for playerId toward target, up to maxSteps.
// Uses state-aware scoring to avoid enemy tackle zones (prefers safe routes).
// ⭐ MERIDLO MAKROVE CHUZE (01.09.2026, uzivatel: "zkontroluj poradne vsechno
//   okolo pohybu"). `movePlayerToward` ma ctyri mista, kde se VZDA, a kazde
//   znamena neco jineho:
//     NENASEL  findMoveToward nevratil krok (obsazeno / mimo rozpocet)
//     OBCHAZKA cesta by se vzdalila o vic nez JEDNO pole => makro SELZE CELE.
//              ⛔ Podezrele: screen nebo klec vynuti dvoupolovou obchazku
//                 a hrac se nehne vubec. Merime nez opravujeme.
//     SMYCKA   krok zpet na predchozi pole bez priblizeni
//     STOJI    akce vratila ok(), ale hrac se nepohnul (Tentacles)
//     LIMIT    dosel maxSteps
thread_local long g_mwNoStep = 0, g_mwDetour = 0, g_mwLoop = 0,
                  g_mwStuck = 0, g_mwLimit = 0;
// ⛔ 02.09.: `smycka 158 170` NEMA JMENOVATEL. Pocet vzdani sam o sobe nerika
//   nic, dokud nevim, kolik chuzi naopak DOJDE -- tataz vada jako u turnoveru
//   z lehu, kde chybela reference ze stoje.
//   K tomu PROFIL situace, kdy se smycka spusti: kolik kroku uz hrac udelal
//   (0 = nerozesel se vubec, 3+ = opravdu oscilace po pokroku) a jak daleko
//   od cile stal. ⭐ ZADNA HYPOTEZA PREDEM -- 01.09. jich pet selhalo.
thread_local long g_mwArrived = 0;
thread_local long g_mwLoopSteps = 0, g_mwLoopStep0 = 0, g_mwLoopDist = 0;

// ⭐ ZACHYCENI SITUACE, NE JEN POCTU (02.09.2026, uzivatel: „mas k te smycce
//   i priklad ze hry?"). Citac rekne KOLIK, ne CO -- a nad cislem se neda
//   diskutovat. Dumpuje se prvnich N pripadu: pozice vsech hracu, cil chuze,
//   kolik kroku uz hrac udelal. Vic nez N se zahazuje, aby z toho nebyly
//   gigabajty (v rolloutech se chuze vola miliony krat).
// ⚠️ Zapina se promennou prostredi BB_WALKLOOP_DUMP=<soubor>, takze v beznem
//   behu i v NOCI je to UPLNE VYPNUTE a nic to nestoji.
thread_local int  g_wlDumpLeft = -1;      // -1 = jeste nezjisteno
thread_local FILE* g_wlDumpFile = nullptr;

static void dumpWalkLoop(const GameState& st, const Player& p,
                         Position target, int step, int dist) {
    if (g_wlDumpLeft < 0) {
        const char* path = getenv("BB_WALKLOOP_DUMP");
        if (!path) { g_wlDumpLeft = 0; return; }
        g_wlDumpFile = fopen(path, "a");
        g_wlDumpLeft = g_wlDumpFile ? 300 : 0;
    }
    if (g_wlDumpLeft <= 0 || !g_wlDumpFile) return;
    --g_wlDumpLeft;
    fprintf(g_wlDumpFile, "{\"mover\":%d,\"side\":%d,\"from\":[%d,%d],"
            "\"target\":[%d,%d],\"step\":%d,\"dist\":%d,\"mv\":%d,"
            "\"ball\":[%d,%d],\"held\":%s,\"players\":[",
            p.id, p.teamSide == TeamSide::HOME ? 0 : 1,
            p.position.x, p.position.y, target.x, target.y, step, dist,
            static_cast<int>(p.movementRemaining),
            st.ball.position.x, st.ball.position.y,
            st.ball.isHeld ? "true" : "false");
    bool first = true;
    for (const Player& q : st.players) {
        if (!q.isOnPitch()) continue;
        fprintf(g_wlDumpFile, "%s{\"id\":%d,\"s\":%d,\"x\":%d,\"y\":%d,\"st\":%d}",
                first ? "" : ",", q.id, q.teamSide == TeamSide::HOME ? 0 : 1,
                q.position.x, q.position.y, static_cast<int>(q.state));
        first = false;
    }
    fprintf(g_wlDumpFile, "]}\n");
    fflush(g_wlDumpFile);
}
void takeMoveWalkBailout(long* out5) {
    out5[0]=g_mwNoStep; out5[1]=g_mwDetour; out5[2]=g_mwLoop;
    out5[3]=g_mwStuck;  out5[4]=g_mwLimit;
    g_mwNoStep=g_mwDetour=g_mwLoop=g_mwStuck=g_mwLimit=0;
}
void takeMoveWalkProfile(long* out4) {
    out4[0]=g_mwArrived; out4[1]=g_mwLoopSteps;
    out4[2]=g_mwLoopStep0; out4[3]=g_mwLoopDist;
    g_mwArrived=g_mwLoopSteps=g_mwLoopStep0=g_mwLoopDist=0;
}

static bool movePlayerToward(GameState& state, int playerId, Position target,
                              DiceRollerBase& dice, MacroExpansionResult& result,
                              int maxSteps = 12, Position avoid = {-1, -1}) {
    Position lastPos{-1, -1};  // Detect loops
    for (int step = 0; step < maxSteps; ++step) {
        const Player& p = state.getPlayer(playerId);
        if (!p.isOnPitch() || p.lostTacklezones) return false;
        // A PRONE player standing on the target square has NOT arrived -- he
        // still has to get up. Before 2026-08-21 this returned "arrived", so a
        // REPOSITION onto one's own square was a silent no-op and the macro
        // layer had no way to stand anybody up at all.
        if (p.position == target && p.state != PlayerState::PRONE) { ++g_mwArrived; return true; }

        // Get available actions
        std::vector<Action> actions;
        getAvailableActions(state, actions);

        // Find best move toward target (with TZ avoidance)
        Action bestMove;
        if (!findMoveToward(actions, playerId, target, bestMove, &state, avoid)) { ++g_mwNoStep; return false; }

        // Allow sideways moves to dodge around opponents, but don't go too far
        int currentDist = p.position.distanceTo(target);
        int moveDist = bestMove.target.distanceTo(target);
        if (moveDist > currentDist + 1) { ++g_mwDetour; return false; } // max 1 square detour
        if (moveDist >= currentDist && bestMove.target == lastPos) {
            ++g_mwLoop;
            g_mwLoopSteps += step;          // kolik kroku uz hrac udelal
            if (step == 0) ++g_mwLoopStep0; // vubec se nerozesel
            g_mwLoopDist += currentDist;    // jak daleko od cile stal
            dumpWalkLoop(state, p, target, step, currentDist);
            return false;
        } // loop

        lastPos = p.position;
        Position before = p.position;
        // ⚠️ `p` je REFERENCE -- po akci uz ukazuje na NOVY stav. Stav pred
        //   akci se proto musi zkopirovat, jinak je porovnani nize no-op.
        const PlayerState beforeState = p.state;
        if (executeAndRecord(state, bestMove, dice, result)) return false;

        // ⛔ GUARD "USPECH BEZ POHYBU" (26.08.2026, treti misto Leapu).
        // resolveLeap pri chyceni Tentacles vraci ok() a hrac se NEPOHNE
        // (move_handler.cpp; l. 8586-8587 jmenuje leap vyslovne). Blitzova
        // smycka tenhle guard ma uz od doby, kdy ji Tentacles potkaly;
        // makrova chuze ho nemela, protoze do ni zadna "ok-ale-stojim" akce
        // dosud nevedla. Pripustenim LEAPu se ta cesta OTEVIRA -- bez guardu
        // by smycka tocila naprazdno az do maxSteps (leapUsedThisTurn sice
        // brani druhemu skoku, ale mrtve iterace zustavaji).
        // ⛔⛔ VSTANI NENI "USPECH BEZ POHYBU" (01.09.2026).
        //   Vstavaci makro je REPOSITION na VLASTNI pole: hrac vstane a pozice
        //   se nezmeni, takze tenhle guard to vyhodnotil jako "nepohnul se"
        //   a vratil SELHANI -- prestoze vstani probehlo. Dalsi iterace by
        //   pritom spravne vratila uspech (`position == target && !PRONE`).
        //   ⇒ Makro vrstva hlasila KAZDE VSTANI jako neuspech.
        //   Vada vznikla 26.08., kdy se guard pridal kvuli Leapu/Tentacles,
        //   a rozbila vstavani, ktere pribylo 21.08. (P45). Po M13 (31.08.)
        //   je vstavani jeste castejsi.
        //   ⚠️ Guard sam je spravny a zustava -- jen se stav PRONE -> STANDING
        //     pocita jako POKROK, protoze jim je.
        if (state.getPlayer(playerId).position == before &&
            state.getPlayer(playerId).state == beforeState) { ++g_mwStuck; return false; }
    }
    ++g_mwLimit;
    return false;
}

static MacroExpansionResult expandScore(GameState& state, const Macro& macro,
                                         DiceRollerBase& dice) {
    MacroExpansionResult result;
    const Player& carrier = state.getPlayer(macro.playerId);

    // Guard against a stale SCORE macro replayed against a carrier who is no
    // longer on pitch. replayToNode replays a macro path open-loop (fresh
    // dice each MCTS iteration), so the player cached in macro.playerId can
    // have been KO'd/crowd-surfed off pitch by an earlier macro *in this
    // same replay* even though the macro itself was only ever generated for
    // an on-pitch carrier. Without this check carrier.position is the
    // {-1,-1} off-pitch sentinel, and the TZ-probe walk below assumes
    // position.x is between the carrier and their own endzone -- for an
    // AWAY carrier (dx=-1, targetX=0) starting at x=-1, cx walks away from
    // 0 and only terminates after a signed-integer-overflow wraparound
    // (confirmed via gdb on a live hang: cx observed at 224583956 mid-spin,
    // i.e. ~4 billion iterations in, ~100-200s of wall time and technically
    // undefined behavior). Mirrors the same guard expandBlitzAndScore
    // already has before its own movePlayerToward call (:934).
    if (!carrier.isOnPitch()) return result;

    int targetX = endzoneX(carrier.teamSide);
    int dx = forwardDx(carrier.teamSide);

    // Evaluate TZ exposure for different Y-target routes, pick safest
    int bestY = carrier.position.y;
    int bestTZ = 999;

    for (int yOff = -2; yOff <= 2; ++yOff) {
        int testY = carrier.position.y + yOff;
        if (testY < 1 || testY > 13) continue; // avoid sidelines

        // Count enemy TZ along approximate path
        int tzSum = 0;
        int cx = carrier.position.x;
        int cy = carrier.position.y;
        // Hard iteration cap as defense-in-depth: cx/cy each converge
        // monotonically toward (targetX, testY) by exactly one square per
        // step, so a legitimate on-pitch carrier never needs more than
        // PITCH_WIDTH + PITCH_HEIGHT steps. Bail rather than spin if that
        // invariant is ever violated again (e.g. by a future bug reintroducing
        // an off-pitch/invalid carrier here).
        constexpr int MAX_TZ_PROBE_STEPS = Position::PITCH_WIDTH + Position::PITCH_HEIGHT;
        int guard = 0;
        while ((cx != targetX || cy != testY) && guard++ < MAX_TZ_PROBE_STEPS) {
            if (cy < testY) cy++;
            else if (cy > testY) cy--;
            if (cx != targetX) cx += dx;
            Position p{static_cast<int8_t>(cx), static_cast<int8_t>(cy)};
            if (p.isOnPitch()) {
                tzSum += countTacklezones(state, p, carrier.teamSide);
            }
        }
        if (tzSum < bestTZ) {
            bestTZ = tzSum;
            bestY = testY;
        }
    }

    Position target{static_cast<int8_t>(targetX), static_cast<int8_t>(bestY)};

    movePlayerToward(state, macro.playerId, target, dice, result, 14);
    return result;
}

// Can any standing opponent reach the carrier with a blitz on their next
// activation?
//
// P37 (31.08.2026) -- BB2016 l. 347-350: "Blitz: The player may move a number
// of squares equal to their MA. He may make one block during the move. The
// block ... 'costs' one square of movement." So reaching a carrier at
// chebyshev distance d costs d-1 squares to close plus 1 for the block = d.
// The test was `d <= MA`, which is right for the movement but FORGETS GOING
// FOR IT: l. 8487-8490 grants 2 more squares (3 with Sprint), and l. 8577-8578
// takes them away from a rooted player. A blitz on GFI is a normal blitz.
// ⇒ the reach is `d <= MA + maxGfiSquares(opp)`.
//
// Why it matters where it is read: this decides whether the carrier keeps
// movement in reserve (carrierStallAwareSteps). Understating the opponent's
// reach makes the carrier hold movement back in exactly the turns where the
// blitz is already coming -- the reserve buys nothing and the exposure is paid.
//
// ⚠️ Feature f63 (carrier_blitzable, feature_extractor.cpp:356-362) uses the
// OLD approximation and is deliberately left alone: it is a network input, so
// changing it shifts the input distribution under an already-trained policy.
// The two are no longer the same test -- see ledger row P37.
//
// ⚠️ Prone opponents are still skipped. Per l. 674-676 a prone player MAY
// declare a Blitz Action (standing up costs 3 of his MA), so this understates
// reach for them too -- but our engine cannot play that yet, so counting it
// here would predict a blitz the opponent is unable to make. Belongs to M13.
static bool carrierIsBlitzable(const GameState& state, const Player& carrier) {
    bool blitzable = false;
    state.forEachOnPitch(opponent(carrier.teamSide), [&](const Player& opp) {
        if (blitzable) return;
        if (opp.state != PlayerState::STANDING) return;
        const int reach = static_cast<int>(opp.stats.movement) + maxGfiSquares(opp);
        if (opp.position.distanceTo(carrier.position) <= reach) {
            blitzable = true;
        }
    });
    return blitzable;
}

// Stall-aware step budget for a ball carrier's own movement: advance just
// enough to reach the endzone by the last turn of the half, keeping the rest
// of the carrier's movement in reserve so teammates still have a decision
// window to form a cage around them (unless the half is about to end, in
// which case a full sprint is worth the exposure).
int carrierStallAwareSteps(const GameState& state, const Player& carrier,
                           const TeamState& myTeam) {
    int dist = distToEndzone(carrier.position, carrier.teamSide);
    int turnsRemaining = std::max(1, 9 - myTeam.turnNumber); // turns left including this one

    // Target: advance dist/turnsRemaining per turn (arrive on last turn)
    // Add small buffer for GFI (2 per turn)
    int idealStepsThisTurn = std::max(1, (dist + turnsRemaining - 1) / turnsRemaining);

    // Don't exceed remaining movement, don't use more than half MA (save for cage/dodge)
    int mvRemaining = static_cast<int>(carrier.movementRemaining);
    int maxSafe = std::max(1, mvRemaining / 2);
    int steps = std::min(idealStepsThisTurn, maxSafe);
    // Holding movement back only buys a cage if the carrier is still standing
    // there next turn. Once an opponent is already in blitz range that reserve
    // buys nothing, so spend it: same full-sprint branch as the last 2 turns.
    if (turnsRemaining <= 2 || carrierIsBlitzable(state, carrier)) {
        steps = std::min(idealStepsThisTurn, mvRemaining);
    }
    return steps;
}

// --- P38 helpers ---------------------------------------------------------
//
// Score the cage a candidate carrier square would produce. The three clauses
// of the rule (K29**, spec 15.0b) are scored as a CONJUNCTION with the corner
// count leading, because three corners out of four are not 75 % of a cage --
// they are an open cage. Deliberately cheap: this runs inside MCTS expansion.
static int cageScoreForSquare(const GameState& state, const Player& carrier,
                              Position cand) {
    static const int DX[4] = {-1, -1, 1, 1};
    static const int DY[4] = {-1, 1, -1, 1};
    static const int OX[4] = {-1, 1, 0, 0};
    static const int OY[4] = {0, 0, -1, 1};
    const TeamSide theirs = opponent(carrier.teamSide);

    // (3) no other neighbour of the carrier: the four orthogonal squares empty,
    // and no opponent anywhere in the eight.
    for (int i = 0; i < 4; ++i) {
        Position o{static_cast<int8_t>(cand.x + OX[i]),
                   static_cast<int8_t>(cand.y + OY[i])};
        if (!o.isOnPitch()) continue;
        const Player* p = state.getPlayerAtPosition(o);
        if (p && p->id != carrier.id) return -1;
    }

    Position corners[4];
    int nCorners = 0;
    for (int i = 0; i < 4; ++i) {
        Position c{static_cast<int8_t>(cand.x + DX[i]),
                   static_cast<int8_t>(cand.y + DY[i])};
        if (!c.isOnPitch()) continue;          // sideline: the corner cannot exist
        const Player* p = state.getPlayerAtPosition(c);
        if (p && p->teamSide == theirs) return -1;   // corner held by the opponent
        // (2) clean: no standing opponent beside the corner square
        bool dirty = false;
        state.forEachOnPitch(theirs, [&](const Player& e) {
            if (e.state == PlayerState::STANDING && e.position.distanceTo(c) <= 1) dirty = true;
        });
        if (dirty) return -1;
        corners[nCorners++] = c;
    }
    if (nCorners < 4) return -1;               // fewer than four corners exist at all

    // (1) four bodies that can actually reach those four corners. Greedy is
    // enough for 4x N and stays cheap; a body already standing on a corner
    // counts as its own filler.
    bool used[32] = {false};
    int filled = 0;
    for (int c = 0; c < 4; ++c) {
        int bestId = -1, bestDist = 999;
        state.forEachOnPitch(carrier.teamSide, [&](const Player& b) {
            if (b.id == carrier.id) return;
            if (b.state != PlayerState::STANDING) return;
            if (b.id >= 0 && b.id < 32 && used[b.id]) return;
            int d = b.position.distanceTo(corners[c]);
            if (d > b.movementRemaining) return;
            if (d < bestDist) { bestDist = d; bestId = b.id; }
        });
        if (bestId < 0) break;
        used[bestId] = true;
        ++filled;
    }
    if (filled < 4) return -1;
    return 1;   // all three clauses hold
}

static MacroExpansionResult expandAdvance(GameState& state, const Macro& macro,
                                           DiceRollerBase& dice) {
    MacroExpansionResult result;
    const Player& carrier = state.getPlayer(macro.playerId);
    int dx = forwardDx(carrier.teamSide);
    const auto& myTeam = state.getTeamState(carrier.teamSide);

    int steps = carrierStallAwareSteps(state, carrier, myTeam);

    int targetX = carrier.position.x + dx * steps;
    targetX = std::clamp(targetX, 1, 24); // stay on pitch
    // Bias Y toward center (7)
    int targetY = carrier.position.y;
    if (targetY < 5) targetY++;
    else if (targetY > 9) targetY--;

    Position target{static_cast<int8_t>(targetX), static_cast<int8_t>(targetY)};
    // P38: the square is chosen by the cage it produces, not by arithmetic on x
    // plus a one-square nudge toward the centre. Candidates are limited to the
    // SAME stall-aware step budget, and to within one square of the best forward
    // progress available inside it -- tempo (K9a, 20.7 sigma) is not for sale;
    // what changes is which square, not how far.
    bool armChoseSquare = false;
    const bool placebo = placeboAdvanceArm(state.activeTeam);
    if (cageAwareAdvanceArm(state.activeTeam) || placebo) {
        const int budget = steps;
        int maxProgress = 0;
        for (int ox = -budget; ox <= budget; ++ox) {
            for (int oy = -budget; oy <= budget; ++oy) {
                Position cand{static_cast<int8_t>(carrier.position.x + ox),
                              static_cast<int8_t>(carrier.position.y + oy)};
                if (!cand.isOnPitch()) continue;
                if (state.getPlayerAtPosition(cand)) continue;
                int prog = dx * (cand.x - carrier.position.x);
                if (prog > maxProgress) maxProgress = prog;
            }
        }
        // Výběr jako funkce kritéria, aby šel spočítat KONTRAFAKTUÁL:
        // „co bych vybral, kdyby se klec neposuzovala".
        auto pickSquare = [&](bool useCrit) {
            Position b{-1, -1};
            int bp = 0;
            for (int ox = -budget; ox <= budget; ++ox) {
                for (int oy = -budget; oy <= budget; ++oy) {
                    Position cand{static_cast<int8_t>(carrier.position.x + ox),
                                  static_cast<int8_t>(carrier.position.y + oy)};
                    if (!cand.isOnPitch()) continue;
                    if (state.getPlayerAtPosition(cand)) continue;
                    int prog = dx * (cand.x - carrier.position.x);
                    if (prog < 1 || prog < maxProgress - 1) continue;
                    // A carrier parked in a tackle zone hands over a free block
                    // on the ball -- the same guard the fallback below applies.
                    if (countTacklezones(state, cand, carrier.teamSide) > 0) continue;
                    // ⭐ P40: THE one functional difference between the two arms.
                    if (useCrit && cageScoreForSquare(state, carrier, cand) < 0) continue;
                    if (prog > bp) { bp = prog; b = cand; }
                }
            }
            return b;
        };
        const Position best = pickSquare(/*useCrit=*/!placebo);
        // Repick: kritérium tiká, jen kdyz volbu ZMĚNILO. Počítá se jen tam,
        // kde kritérium skutečně běží (tedy ne u placeba).
        if (!placebo && best != pickSquare(/*useCrit=*/false)) ++g_cageCritRepicks;
        if (best.x >= 0) {
            armChoseSquare = true;
            if (best != target) {
                ++g_cageAwareAdvancePicks;
                target = best;
            }
        }
    }

    // The walk's final square is exempt from TZ scoring on the premise that
    // the macro which chose it owns the risk (cage corners deliberately stand
    // next to defenders, probes price it). ADVANCE has no such pricing: the
    // target is pure arithmetic, and a carrier parked in a tackle zone hands
    // the opponent a free block on the ball. Pull the target back to the
    // nearest unoccupied TZ-free square; if none exists ahead, don't advance
    // at all and let the search's other macros handle the turn.
    const int origSteps = steps;   // M12 krok 1: rozpočet PŘED snižováním
    bool sideFree = false;
    while (!armChoseSquare && steps > 0 &&
           (state.getPlayerAtPosition(target) != nullptr ||
            countTacklezones(state, target, carrier.teamSide) > 0)) {
        --steps;
        targetX = std::clamp(carrier.position.x + dx * steps, 1, 24);
        target.x = static_cast<int8_t>(targetX);
    }

    // ⭐ M12/A+C (30.08.2026): KDYŽ PŘÍMKA NEVEDE, HLEDEJ VEDLE.
    //   Smyčka výš mění jen `x` a `y` nechává, takže zavřená přímka pro ni
    //   znamená „nikam nelze" -- a `ADVANCE` se vzdá, i když je volno o pole
    //   stranou. Změřeno v produkci (krok 1, 6 párů dw-dw): z 9202 rezignací
    //   jich 8294 = 90,1 % mělo volné pole bez TZ mimo přímku. Vzdát postup,
    //   když je kam jít, se jako záměr obhájit nedá => je to VADA.
    //
    // ⛔ ROZSAH JE PŘESNĚ TA VADA, NIC VÍC. Z ramene P38 se sem NEBERE ani
    //   klecové kritérium `cageScoreForSquare` (to je okruh KLEC), ani okno
    //   „do jednoho pole od nejlepšího postupu". Tohle je jen „nerezignuj,
    //   když existuje volné pole vpřed" -- podmínky zůstávají tytéž, co má
    //   smyčka: volné pole, bez soupeřovy tacklezóny, postup >= 1.
    if (!armChoseSquare && steps <= 0) {
        Position best{-1, -1};
        int bestProg = 0, bestDrift = 99;
        for (int ox = -origSteps; ox <= origSteps; ++ox) {
            for (int oy = -origSteps; oy <= origSteps; ++oy) {
                Position cand{static_cast<int8_t>(carrier.position.x + ox),
                              static_cast<int8_t>(carrier.position.y + oy)};
                if (!cand.isOnPitch()) continue;
                if (state.getPlayerAtPosition(cand)) continue;
                const int prog = dx * (cand.x - carrier.position.x);
                if (prog < 1) continue;
                if (countTacklezones(state, cand, carrier.teamSide) > 0) continue;
                // Nejdál vpřed; při shodě to, co se nejmíň uhne od přímky --
                // aby se z opravy nestalo „chodit do stran", když jde rovně.
                const int drift = std::abs(oy);
                if (prog > bestProg || (prog == bestProg && drift < bestDrift)) {
                    bestProg = prog; bestDrift = drift; best = cand;
                }
            }
        }
        if (best.x >= 0) {
            target = best;
            steps = std::max(bestProg, std::abs(best.y - carrier.position.y));
        }
    }
    if (steps <= 0) {
        // ⭐ M12 krok 1: TADY `ADVANCE` rezignuje. Než se vrátíme, zeptáme se,
        //   jestli to bylo nutné -- prohledáme TÝŽ rozpočet ve ČTVERCI a
        //   hledáme volné pole bez TZ, které vede vpřed. Je to jen čtení
        //   stavu, nic se nemění.
        ++g_advanceResigned;
        const int budget0 = origSteps;
        for (int ox = -budget0; ox <= budget0 && !sideFree; ++ox) {
            for (int oy = -budget0; oy <= budget0; ++oy) {
                Position cand{static_cast<int8_t>(carrier.position.x + ox),
                              static_cast<int8_t>(carrier.position.y + oy)};
                if (!cand.isOnPitch()) continue;
                if (state.getPlayerAtPosition(cand)) continue;
                if (dx * (cand.x - carrier.position.x) < 1) continue;
                if (countTacklezones(state, cand, carrier.teamSide) > 0) continue;
                sideFree = true; break;
            }
        }
        if (sideFree) ++g_advanceResignedButSideFree;
        return result;
    }

    movePlayerToward(state, macro.playerId, target, dice, result, steps + 2);
    return result;
}

static MacroExpansionResult expandCage(GameState& state, const Macro& macro,
                                        DiceRollerBase& dice) {
    MacroExpansionResult result;
    const Player& carrier = state.getPlayer(macro.playerId);
    Position cp = carrier.position;

    // 4 diagonal cage positions
    Position cagePositions[4] = {
        {static_cast<int8_t>(cp.x + 1), static_cast<int8_t>(cp.y + 1)},
        {static_cast<int8_t>(cp.x + 1), static_cast<int8_t>(cp.y - 1)},
        {static_cast<int8_t>(cp.x - 1), static_cast<int8_t>(cp.y + 1)},
        {static_cast<int8_t>(cp.x - 1), static_cast<int8_t>(cp.y - 1)},
    };

    for (auto& cagePos : cagePositions) {
        if (!cagePos.isOnPitch()) continue;

        // Already occupied?
        const Player* occupant = state.getPlayerAtPosition(cagePos);
        if (occupant) {
            // If it's our standing player, that's fine
            if (occupant->teamSide == state.activeTeam &&
                occupant->state == PlayerState::STANDING) continue;
            // Otherwise skip this position
            continue;
        }

        // Find nearest free player (not carrier)
        const Player* mover = findNearestFreePlayer(state, cagePos, carrier.id);
        if (!mover) continue;

        // Move them there (max 4 steps)
        movePlayerToward(state, mover->id, cagePos, dice, result, 4);
        if (result.turnover) return result;
    }
    return result;
}

static MacroExpansionResult expandBlitz(GameState& state, const Macro& macro,
                                         DiceRollerBase& dice) {
    MacroExpansionResult result;

    const Player& target = state.getPlayer(macro.targetId);

    // Find best BLITZ action for this target (prefer more dice, closer blitzer)
    std::vector<Action> actions;
    getAvailableActions(state, actions);

    Action bestBlitzAction{};
    bool found = false;
    double bestFail = 2.0; // worse than any real fail chance (max 1.0)
    // P35: with the arm on, the same ranking is also run the old way, purely so
    // the counter can say whether the arm changed anything. Zero repicks over a
    // matchup means the two arms executed the same decision -- the null-arm test
    // of 2026-08-17, which is what makes a paired delta readable at all.
    // ⭐ P35 NASAZENO 01.09.2026 (noc 31.08.: jednostranne +0,0073 +- 0,0060,
    //   skoda na urovni prahu vyloucena). Rameno odebrano -- default-OFF
    //   rameno, ktere neskodi, je jen dalsi hotova vec, o ktere nikdo nevi
    //   (B2, 30.08., tyz duvod).
    for (auto& a : actions) {
        if (a.type != ActionType::BLITZ || a.targetId != macro.targetId) continue;
        const Player& blitzer = state.getPlayer(a.playerId);
        // Prefer the candidate least likely to fail (block dice + approach
        // path combined), not just the most dice + shortest raw distance --
        // item 14: raw dice/distance alone can pick a low-agility, no-Dodge
        // blitzer through a crowded midfield over a safer alternative.
        double fail = estimateBlitzFailChance(state, blitzer, target);
        if (fail < bestFail) {
            bestFail = fail;
            bestBlitzAction = a;
            found = true;
        }
    }

    if (!found) return result;

    executeAndRecord(state, bestBlitzAction, dice, result);
    return result;
}

static MacroExpansionResult expandBlitzAndScore(GameState& state, const Macro& macro,
                                                  DiceRollerBase& dice) {
    MacroExpansionResult result;

    // Step 1: Find a blitzer and blitz the blocker out of the way
    const Player& blocker = state.getPlayer(macro.targetId);
    int carrierId = macro.playerId;

    std::vector<Action> actions;
    getAvailableActions(state, actions);

    // Find the best blitzer for this target: least likely to fail (block
    // dice + approach path, item 14), tie-broken toward non-carrier so the
    // ball carrier isn't risked on the blitz when an equally-safe teammate
    // is available.
    Action bestBlitz{};
    bool foundBlitz = false;
    double bestScore = -2.0; // worse than any real -fail (min -1.0)
    // P35 applies here too: BLITZ_AND_SCORE picks a blitzer the same way, so
    // leaving this call site alone would price the same block two ways
    // depending on which macro asked -- exactly the split the arm exists to close.

    for (auto& a : actions) {
        if (a.type != ActionType::BLITZ) continue;
        if (a.targetId != macro.targetId) continue;
        const Player& blitzer = state.getPlayer(a.playerId);
        double fail = estimateBlitzFailChance(state, blitzer, blocker);
        bool isCarrier = (a.playerId == carrierId);
        double score = -fail + (isCarrier ? 0.0 : 0.001);
        if (score > bestScore) {
            bestScore = score;
            bestBlitz = a;
            foundBlitz = true;
        }
    }

    if (!foundBlitz) return result; // can't blitz, abort

    // Execute the blitz
    if (executeAndRecord(state, bestBlitz, dice, result)) return result;

    // After blitz, continue with any follow-up moves/blocks from the blitz action
    // The blitz action may generate further MOVE/BLOCK actions
    for (int step = 0; step < 12; ++step) {
        // The blitz may have surfed/KO'd the blocker off the pitch, leaving
        // its position at the {-1,-1} sentinel. findMoveToward would then
        // walk the blitzer toward that square. Today the chase is unreachable
        // -- resolveBlock leaves the blitzer with hasActed=true on every path,
        // so no MOVE is generated for it after a completed block -- but that
        // is a non-local invariant of block_handler's bookkeeping, not of this
        // loop. Bail locally rather than depend on it (same defense-in-depth
        // idiom as MAX_TZ_PROBE_STEPS above). break, not return: Step 2
        // (carrier scores) must still run.
        if (!blocker.isOnPitch()) break;
        actions.clear();
        getAvailableActions(state, actions);

        // If we can block the target, do it
        bool blocked = false;
        for (auto& a : actions) {
            if (a.type == ActionType::BLOCK && a.playerId == bestBlitz.playerId &&
                a.targetId == macro.targetId) {
                if (executeAndRecord(state, a, dice, result)) return result;
                blocked = true;
                break;
            }
        }
        if (blocked) break;

        // Move blitzer toward target
        Action moveAction;
        if (!findMoveToward(actions, bestBlitz.playerId, blocker.position, moveAction, &state))
            break;
        if (executeAndRecord(state, moveAction, dice, result)) return result;
    }

    // Step 2: Now move the carrier to score
    const Player& carrier = state.getPlayer(carrierId);
    if (!carrier.isOnPitch() || carrier.lostTacklezones) return result;
    if (!carrier.canAct()) return result;

    int targetX = endzoneX(carrier.teamSide);
    Position target{static_cast<int8_t>(targetX), carrier.position.y};
    movePlayerToward(state, carrierId, target, dice, result, 14);
    return result;
}

static MacroExpansionResult expandBlock(GameState& state, const Macro& macro,
                                         DiceRollerBase& dice) {
    MacroExpansionResult result;

    std::vector<Action> actions;
    getAvailableActions(state, actions);

    for (auto& a : actions) {
        if (a.type == ActionType::BLOCK && a.playerId == macro.playerId &&
            a.targetId == macro.targetId) {
            executeAndRecord(state, a, dice, result);
            return result;
        }
    }
    return result;
}

static MacroExpansionResult expandPickup(GameState& state, const Macro& macro,
                                          DiceRollerBase& dice) {
    MacroExpansionResult result;
    // Candidate generation (getAvailableMacros) admits pickers up to
    // movementRemaining+2 squares away (2 GFI) -- the move-to-ball step must
    // allow the same reach, or a picker legitimately selected at distance
    // 9-11 (e.g. MA9 Skaven Gutter Runners) walks the previously-hardcoded
    // 8 steps, stops short, and wastes the whole activation with the ball
    // still loose (project_bloodbowl_audit_findings_20260703 finding 6).
    const Player& picker = state.getPlayer(macro.playerId);
    int maxSteps = picker.movementRemaining + maxGfiSquares(picker);
    movePlayerToward(state, macro.playerId, macro.targetPos, dice, result, maxSteps);
    if (result.turnover) return result;

    // After pickup: if we now have the ball, advance toward endzone.
    // Same stall-aware throttle as expandAdvance — a fresh pickup must not
    // burn the carrier's entire remaining movement on a naked dash, or the
    // team never gets a decision window to form a cage around them.
    const Player& p = state.getPlayer(macro.playerId);
    if (state.ball.isHeld && state.ball.carrierId == macro.playerId &&
        p.isOnPitch() && p.movementRemaining > 0 && !p.lostTacklezones) {
        const auto& myTeam = state.getTeamState(p.teamSide);
        int steps = carrierStallAwareSteps(state, p, myTeam);
        int targetX = endzoneX(p.teamSide);
        int targetY = p.position.y;
        if (targetY < 5) targetY++;
        else if (targetY > 9) targetY--;
        Position target{static_cast<int8_t>(targetX), static_cast<int8_t>(targetY)};
        movePlayerToward(state, macro.playerId, target, dice, result, steps);
    }
    return result;
}

static MacroExpansionResult expandPass(GameState& state, const Macro& macro,
                                        DiceRollerBase& dice) {
    MacroExpansionResult result;

    std::vector<Action> actions;
    getAvailableActions(state, actions);

    // Try HAND_OFF first (safer), then PASS
    for (ActionType passType : {ActionType::HAND_OFF, ActionType::PASS}) {
        for (auto& a : actions) {
            if (a.type == passType && a.playerId == macro.playerId &&
                a.targetId == macro.targetId) {
                executeAndRecord(state, a, dice, result);
                return result;
            }
        }
    }
    return result;
}

static MacroExpansionResult expandFoul(GameState& state, const Macro& macro,
                                        DiceRollerBase& dice) {
    MacroExpansionResult result;

    std::vector<Action> actions;
    getAvailableActions(state, actions);

    for (auto& a : actions) {
        if (a.type == ActionType::FOUL && a.playerId == macro.playerId &&
            a.targetId == macro.targetId) {
            executeAndRecord(state, a, dice, result);
            return result;
        }
    }
    return result;
}

static MacroExpansionResult expandReposition(GameState& state, const Macro& macro,
                                              DiceRollerBase& dice) {
    MacroExpansionResult result;
    // Candidate generation (getAvailableMacros) hands out REPOSITION targets
    // with no reach check at all (safety/screen spots are routinely 5-10+
    // squares away), but the walk here was hardcoded to maxSteps=4 -- any
    // player repositioning farther than 4 squares stopped short every single
    // turn, so defensive screens/safeties never actually formed
    // (research_fable_20260709 section 3b; same bug class as the PICKUP step
    // cap fix, 2899cd5). Cap at the player's real movement budget instead.
    // Deliberately NO +2 GFI headroom (unlike expandPickup): REPOSITION is
    // the only dice-free macro, and a failed GFI on a free repositioning
    // player is pure downside with no ball at stake. Exception (2026-08-04):
    // macro.gfiAllowance (0-2) opts a SPECIFIC walk into real GFI rolls --
    // set only by the cage-advance planner for the ball carrier in a tempo
    // emergency, where not arriving loses the drive anyway.
    int maxSteps = state.getPlayer(macro.playerId).movementRemaining
                   + std::clamp(macro.gfiAllowance, 0, 2);
    // Loose ball: never step onto its square, not even as a waypoint --
    // the auto-pickup in move_handler.cpp would turn this dice-free macro
    // into a real gamble (item 11).
    Position avoid = state.ball.isHeld ? Position{-1, -1} : state.ball.position;
    // Pojistka (21.08.): vstávací makro má cíl == vlastní pole hráče. Kdyby na
    // něm ležel volný míč, `avoid` by tu JEDINOU akci vetoval, expanze by
    // vrátila prázdno a MacroMCTSPolicy z toho udělá END_TURN -- tedy zahodí
    // zbytek kola CELÉHO týmu. Vstáváním se na pole nevstupuje, jen se na něm
    // hráč zvedá, takže tu žádný pickup hrozit nemůže.
    if (macro.targetPos == state.getPlayer(macro.playerId).position) {
        avoid = Position{-1, -1};
    }
    // ⭐⭐ Q3 MERIDLO (03.09.): rozliseni se dela PRED chuzi -- potom uz hrac
    //   lezici neni. „Vstat a zustat" ma cil == vlastni pole (viz vyse),
    //   „vstat a odejit" cokoli jineho.
    const Player& q3p = state.getPlayer(macro.playerId);
    const bool q3WasProne = (q3p.state == PlayerState::PRONE);
    const bool q3Leaving  = (macro.targetPos != q3p.position);
    if (q3WasProne) { if (q3Leaving) ++g_q3EscTried; else ++g_q3StayTried; }

    // ⛔⛔ 03.09.: PRVNI VERZE BYLA SPATNE. `takeMoveTurnoverCause` vraci
    //   NAAKUMULOVANY soucet od posledniho cteni, ne prispevek tohohle makra.
    //   Cetl jsem ho jen PO chuzi a cely soucet pripsal uteku => sectly se
    //   turnovery z celeho behu, opakovane (DODGE 2 617 490 z 3 001 uteku).
    //   Prozradil to sloupec „jine", ktery vysel ZAPORNY.
    //   ⇒ Musi se cist PRED i PO a brat ROZDIL. Cteni hodnoty vraci zpatky,
    //     aby souhrnny radek zustal netknuty.
    long q3Before[3] = {0,0,0};
    if (q3WasProne && q3Leaving) {
        takeMoveTurnoverCause(q3Before);
        addBackMoveTurnoverCause(q3Before);
    }

    movePlayerToward(state, macro.playerId, macro.targetPos, dice, result,
                     maxSteps, avoid);

    if (q3WasProne && result.turnover) {
        if (q3Leaving) {
            ++g_q3EscTurnover;
            long q3After[3];
            takeMoveTurnoverCause(q3After);
            addBackMoveTurnoverCause(q3After);   // souhrnny radek zustava netknuty
            g_q3EscDodge += (q3After[0] - q3Before[0]);
            g_q3EscGfi   += (q3After[1] - q3Before[1]);
        } else ++g_q3StayTurnover;
    }
    return result;
}

static MacroExpansionResult expandEndTurn(GameState& state, const Macro& /*macro*/,
                                           DiceRollerBase& dice) {
    MacroExpansionResult result;
    Action endTurn{ActionType::END_TURN, -1, -1, {-1, -1}};
    executeAndRecord(state, endTurn, dice, result);
    return result;
}

static MacroExpansionResult expandHandOffScore(GameState& state, const Macro& macro,
                                                DiceRollerBase& dice) {
    MacroExpansionResult result;
    int carrierId = macro.playerId;
    int receiverId = macro.targetId;
    if (carrierId <= 0 || receiverId <= 0) return result;

    const Player& receiver = state.getPlayer(receiverId);
    if (!receiver.isOnPitch()) return result;

    // Step 1: Move carrier adjacent to receiver (if not already adjacent)
    {
        const Player& carrier = state.getPlayer(carrierId);
        if (!carrier.isOnPitch() || !carrier.canAct()) return result;
        int dist = carrier.position.distanceTo(receiver.position);
        if (dist > 1) {
            movePlayerToward(state, carrierId, receiver.position, dice, result,
                             carrier.movementRemaining);
            if (result.turnover) return result;
        }
    }

    // Step 2: Execute HAND_OFF
    {
        std::vector<Action> actions;
        getAvailableActions(state, actions);
        bool executed = false;
        for (auto& a : actions) {
            if (a.type == ActionType::HAND_OFF && a.playerId == carrierId &&
                a.targetId == receiverId) {
                if (executeAndRecord(state, a, dice, result)) return result;
                executed = true;
                break;
            }
        }
        if (!executed) return result;
    }

    // Step 3: Move receiver to score
    if (!state.ball.isHeld || state.ball.carrierId != receiverId) return result;
    const Player& newCarrier = state.getPlayer(receiverId);
    if (!newCarrier.isOnPitch() || !newCarrier.canAct()) return result;

    int targetX = endzoneX(newCarrier.teamSide);
    Position target{static_cast<int8_t>(targetX), newCarrier.position.y};
    movePlayerToward(state, receiverId, target, dice, result, 14);
    return result;
}

static MacroExpansionResult expandPassScore(GameState& state, const Macro& macro,
                                             DiceRollerBase& dice) {
    MacroExpansionResult result;
    int carrierId = macro.playerId;
    int receiverId = macro.targetId;
    if (carrierId <= 0 || receiverId <= 0) return result;

    // Step 1: Pass to receiver
    std::vector<Action> actions;
    getAvailableActions(state, actions);

    bool passed = false;
    for (auto& a : actions) {
        if (a.type == ActionType::PASS && a.playerId == carrierId &&
            a.targetId == receiverId) {
            if (executeAndRecord(state, a, dice, result)) return result;
            passed = true;
            break;
        }
    }
    if (!passed) return result;

    // Step 2: Move receiver to endzone (if catch succeeded)
    if (!state.ball.isHeld || state.ball.carrierId != receiverId) return result;
    const Player& rcv = state.getPlayer(receiverId);
    if (!rcv.isOnPitch() || rcv.lostTacklezones) return result;

    int targetX = endzoneX(rcv.teamSide);
    Position target{static_cast<int8_t>(targetX), rcv.position.y};
    movePlayerToward(state, receiverId, target, dice, result, 14);
    return result;
}

static MacroExpansionResult expandChainScore(GameState& state, const Macro& macro,
                                              DiceRollerBase& dice) {
    MacroExpansionResult result;
    int carrierId = macro.playerId;
    int relayId   = macro.targetId;
    int scorerId  = macro.thirdId;
    if (carrierId <= 0 || relayId <= 0 || scorerId <= 0) return result;

    // Step 1: Pass to relay
    std::vector<Action> actions;
    getAvailableActions(state, actions);
    bool passed = false;
    for (auto& a : actions) {
        if (a.type == ActionType::PASS && a.playerId == carrierId &&
            a.targetId == relayId) {
            if (executeAndRecord(state, a, dice, result)) return result;
            passed = true;
            break;
        }
    }
    if (!passed) return result;
    if (!state.ball.isHeld || state.ball.carrierId != relayId) return result;

    // Step 2: Relay moves adjacent to scorer and hand-offs
    const Player& relay = state.getPlayer(relayId);
    if (!relay.isOnPitch() || !relay.canAct()) return result;

    const Player& scorer = state.getPlayer(scorerId);
    if (!scorer.isOnPitch()) return result;

    // Move relay adjacent to scorer
    if (relay.position.distanceTo(scorer.position) > 1) {
        movePlayerToward(state, relayId, scorer.position, dice, result, relay.movementRemaining);
        if (result.turnover) return result;
    }

    // Hand-off to scorer
    actions.clear();
    getAvailableActions(state, actions);
    for (auto& a : actions) {
        if (a.type == ActionType::HAND_OFF && a.playerId == relayId &&
            a.targetId == scorerId) {
            if (executeAndRecord(state, a, dice, result)) return result;
            break;
        }
    }
    if (!state.ball.isHeld || state.ball.carrierId != scorerId) return result;

    // Step 3: Scorer moves to endzone
    const Player& sc = state.getPlayer(scorerId);
    if (!sc.isOnPitch() || !sc.canAct()) return result;
    int targetX = endzoneX(sc.teamSide);
    Position target{static_cast<int8_t>(targetX), sc.position.y};
    movePlayerToward(state, scorerId, target, dice, result, 14);
    return result;
}

MacroExpansionResult greedyExpandMacro(GameState& state, const Macro& macro,
                                       DiceRollerBase& dice) {
    switch (macro.type) {
        case MacroType::SCORE:       return expandScore(state, macro, dice);
        case MacroType::ADVANCE:     return expandAdvance(state, macro, dice);
        case MacroType::CAGE:        return expandCage(state, macro, dice);
        case MacroType::BLITZ:       return expandBlitz(state, macro, dice);
        case MacroType::BLOCK:       return expandBlock(state, macro, dice);
        case MacroType::PICKUP:      return expandPickup(state, macro, dice);
        case MacroType::PASS_ACTION: return expandPass(state, macro, dice);
        case MacroType::FOUL:        return expandFoul(state, macro, dice);
        case MacroType::REPOSITION:  return expandReposition(state, macro, dice);
        case MacroType::END_TURN:    return expandEndTurn(state, macro, dice);
        case MacroType::BLITZ_AND_SCORE: return expandBlitzAndScore(state, macro, dice);
        case MacroType::HAND_OFF_SCORE:  return expandHandOffScore(state, macro, dice);
        case MacroType::PASS_SCORE:      return expandPassScore(state, macro, dice);
        case MacroType::CHAIN_SCORE:     return expandChainScore(state, macro, dice);
        default:                     return {};
    }
}

// --- Macro Feature Extraction ---

void extractMacroFeatures(const GameState& state, const Macro& macro, float* out) {
    for (int i = 0; i < NUM_ACTION_FEATURES; ++i) out[i] = 0.0f;

    int typeIdx = static_cast<int>(macro.type);

    // [0-9] one-hot macro type (BLITZ_AND_SCORE shares BLITZ slot, HAND_OFF_SCORE shares SCORE slot)
    if (macro.type == MacroType::BLITZ_AND_SCORE) {
        out[static_cast<int>(MacroType::BLITZ)] = 1.0f;
    } else if (macro.type == MacroType::HAND_OFF_SCORE ||
               macro.type == MacroType::PASS_SCORE ||
               macro.type == MacroType::CHAIN_SCORE) {
        out[static_cast<int>(MacroType::SCORE)] = 1.0f;
    } else if (typeIdx >= 0 && typeIdx < 10) {
        out[typeIdx] = 1.0f;
    }

    TeamSide mySide = state.activeTeam;

    // [10] scoring_potential
    if (macro.type == MacroType::SCORE || macro.type == MacroType::BLITZ_AND_SCORE ||
        macro.type == MacroType::HAND_OFF_SCORE ||
        macro.type == MacroType::PASS_SCORE ||
        macro.type == MacroType::CHAIN_SCORE) {
        out[10] = 1.0f;
    } else if (macro.type == MacroType::ADVANCE && macro.playerId > 0) {
        const Player& p = state.getPlayer(macro.playerId);
        if (p.isOnPitch()) {
            int dist = distToEndzone(p.position, mySide);
            int ma = p.movementRemaining + maxGfiSquares(p);
            out[10] = std::min(1.0f, static_cast<float>(ma) / std::max(dist, 1));
        }
    }

    // [11] block_dice_quality
    if ((macro.type == MacroType::BLOCK || macro.type == MacroType::BLITZ ||
         macro.type == MacroType::BLITZ_AND_SCORE) &&
        macro.targetId > 0 && macro.playerId > 0) {
        const Player& att = state.getPlayer(macro.playerId);
        const Player& def = state.getPlayer(macro.targetId);
        if (att.isOnPitch() && def.isOnPitch()) {
            bool isBlitz = (macro.type == MacroType::BLITZ);
            int dice = getBlockDiceCount(state, att, def, isBlitz, false);
            out[11] = dice / 3.0f;
        }
    } else if (macro.type == MacroType::BLITZ && macro.targetId > 0 && macro.playerId <= 0) {
        // Blitz with unspecified blitzer — estimate based on target
        out[11] = 0.3f; // default moderate quality
    }

    // [12] player_strength / 7
    if (macro.playerId > 0) {
        const Player& p = state.getPlayer(macro.playerId);
        out[12] = p.stats.strength / 7.0f;
    }

    // [13] risk_level (probability of failure estimate)
    switch (macro.type) {
        case MacroType::END_TURN:
            out[13] = 0.0f; // no risk
            break;
        case MacroType::BLOCK:
            out[13] = 0.15f; // low risk for favorable block
            break;
        case MacroType::BLITZ:
            out[13] = 0.25f; // moderate (movement + block)
            break;
        case MacroType::BLITZ_AND_SCORE:
            out[13] = 0.35f; // higher risk (blitz + movement to score)
            break;
        case MacroType::SCORE:
            // Risk depends on distance (GFIs)
            if (macro.playerId > 0) {
                const Player& p = state.getPlayer(macro.playerId);
                if (p.isOnPitch()) {
                    int dist = distToEndzone(p.position, mySide);
                    int gfis = std::max(0, dist - p.movementRemaining);
                    out[13] = gfis * 0.17f; // ~1/6 per GFI
                }
            }
            break;
        case MacroType::PICKUP:
            out[13] = 0.33f; // pickup roll
            break;
        case MacroType::PASS_ACTION:
            out[13] = 0.4f; // catch + interception risk
            break;
        case MacroType::PASS_SCORE:
            out[13] = 0.40f; // pass accuracy + catch + interception risk
            break;
        case MacroType::CHAIN_SCORE:
            out[13] = 0.55f; // pass + catch + hand-off + catch — highest risk
            break;
        case MacroType::FOUL:
            out[13] = 0.08f; // ejection risk
            break;
        default:
            out[13] = 0.1f;
            break;
    }

    // [14] positional_gain
    if (macro.type == MacroType::SCORE || macro.type == MacroType::BLITZ_AND_SCORE ||
        macro.type == MacroType::HAND_OFF_SCORE ||
        macro.type == MacroType::PASS_SCORE ||
        macro.type == MacroType::CHAIN_SCORE) {
        out[14] = 1.0f;
    } else if (macro.type == MacroType::ADVANCE && macro.playerId > 0) {
        const Player& p = state.getPlayer(macro.playerId);
        if (p.isOnPitch()) {
            int steps = std::max(1, p.movementRemaining / 2);
            out[14] = std::min(1.0f, steps / 6.0f);
        }
    } else if (macro.type == MacroType::CAGE) {
        out[14] = 0.5f; // good positional improvement
    } else if (macro.type == MacroType::REPOSITION) {
        out[14] = 0.3f;
    }
}

} // namespace bb
