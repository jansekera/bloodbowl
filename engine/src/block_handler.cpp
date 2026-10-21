#include "bb/coach_choice.h"
#include "bb/block_handler.h"
#include <cmath>
#include "bb/helpers.h"
#include "bb/injury.h"
#include "bb/ball_handler.h"
#include "bb/macro_actions.h"   // M1/N10: noteBlitzContinuationEvent()
#include <algorithm>

namespace bb {

// Score a block die face from the attacker's perspective (higher = better for attacker)
//
// ⛔ 24.08.2026: tabulka neznala WRESTLE, a tim si sama zavirala dvere, ktere
// jsme prave otevreli v resolveru. Utocnik s Block+Wrestle proti obranci
// s Blockem: BOTH_DOWN se ocenoval 4 ("nestane se nic"), PUSHED 5 -- takze
// vybirac sahl po odsunu a Wrestle se NIKDY nespustil. Presne ta situace,
// kvuli ktere se o Wrestle u trpaslika uvazuje: PROLOMENI ZDI, kde odsun
// nestaci a je potreba obrance SLOZIT. Tohle je zas jednou "filtr ocenuje
// jinou akci, nez jakou resolver provede".
static int scoreFace(BlockDiceFace face, bool attHasBlock, bool defHasBlock,
                     bool defHasDodge, bool attHasTackle,
                     bool attUsesWrestle, bool defUsesWrestle) {
    switch (face) {
        case BlockDiceFace::DEFENDER_DOWN:
            return 10;
        case BlockDiceFace::DEFENDER_STUMBLES:
            return (defHasDodge && !attHasTackle) ? 5 : 9;
        case BlockDiceFace::PUSHED:
            return 5;
        case BlockDiceFace::BOTH_DOWN:
            // Wrestle prepisuje vsechny ctyri radky pod sebou: oba jdou na zem
            // bez hodu na zbroj, bez ohledu na Block (l. 8674-8676).
            if (attUsesWrestle) {
                // Obrance SLOZEN. Neni to zadarmo -- utocnik lezi taky, takze
                // to je min nez DEFENDER_DOWN a min nez cisty odsun s hodem na
                // zbroj proti nekomu, kdo by stejne spadl. Vic nez PUSHED (5)
                // to je jen tam, kde odsun problem neresi: proti Blocku.
                return defHasBlock ? 7 : 6;
            }
            if (defUsesWrestle) return 1;               // obrance nas polozi taky
            if (attHasBlock && !defHasBlock) return 8;  // only def falls
            if (attHasBlock && defHasBlock) return 4;   // nothing happens
            if (!attHasBlock && defHasBlock) return 1;  // only att falls
            return 3;  // both fall
        case BlockDiceFace::ATTACKER_DOWN:
            return 0;
    }
    return 0;
}

thread_local long g_dauntlessRolls = 0;

// P9/P9c -- 08.09.2026 NASAZENO DO PRODUKCE, nepodmineny (uzivatel: "nastav
// AI ať odstrkává tak, ať nesousedí s nosičem ani s klecí -- víc netřeba").
// Statisticka delta na dw-we vysla jako ekvivalence (+0,0017 +- 0,0060), ale
// nasazuje se jako doložená oprava vadné volby (1,28 horší volby na zápas),
// ne jako prokázaný zisk chess -- stejný důvod jako u B2/Wrestle pricing.
// Rozbor konkrétní situace se skutečnou klecí (diagonální tělo u nosiče)
// potvrdil, že napsané pravidlo (roh -> nosič -> rovně dozadu) odpovídá
// tomu, co by hráč sám zvolil. Přepínač zůstal jako diagnostika (viz níže),
// ale sám mechanismus je teď bezpodmínečný.
thread_local long g_pushGeometryPicks = 0;
thread_local long g_pushGeometryPicksDodge = 0;   // redirected AND pushed target has Dodge
thread_local long g_pushGeometryPicksFastMA = 0;  // redirected AND pushed target MA >= 6 (first-pass "fast mover" cutoff, not a tuned constant)
thread_local long g_standFirmDeclinedForFrenzy = 0;   // P225
long takeStandFirmDeclinedForFrenzy() {
    long v = g_standFirmDeclinedForFrenzy; g_standFirmDeclinedForFrenzy = 0; return v;
}


long takeDauntlessRollEvalsInSearch() {
    long v = g_dauntlessRolls;
    g_dauntlessRolls = 0;
    return v;
}

long takePushGeometryEvalsInSearch() {
    long v = g_pushGeometryPicks;
    g_pushGeometryPicks = 0;
    return v;
}

long takePushGeometryDodgePicksInSearch() {
    long v = g_pushGeometryPicksDodge;
    g_pushGeometryPicksDodge = 0;
    return v;
}
long takePushGeometryFastMAPicksInSearch() {
    long v = g_pushGeometryPicksFastMA;
    g_pushGeometryPicksFastMA = 0;
    return v;
}

BlockDiceFace autoChooseBlockDie(const BlockDiceFace* faces, int count,
                                 bool attackerChooses,
                                 const Player& att, const Player& def) {
    bool attBlock = att.hasSkill(SkillName::Block);
    bool defBlock = def.hasSkill(SkillName::Block);
    bool defDodge = def.hasSkill(SkillName::Dodge);
    bool attTackle = att.hasSkill(SkillName::Tackle);
    // Musi to byt TAZ podminka jako v resolveru (case BOTH_DOWN), jinak vybirac
    // ocenuje jinou vec, nez se pak stane. Mic sem nedosahne, takze se pocita
    // konzervativne: bez mice, tj. jak to vypada ve zdi.
    bool attWrestle = att.hasSkill(SkillName::Wrestle) &&
                      (!attBlock || defBlock);
    bool defWrestle = def.hasSkill(SkillName::Wrestle) && (!defBlock || attBlock);   // táž podmínka jako v resolveru (16.10.)

    int bestIdx = 0;
    int bestScore = scoreFace(faces[0], attBlock, defBlock, defDodge, attTackle,
                              attWrestle, defWrestle);

    for (int i = 1; i < count; i++) {
        int s = scoreFace(faces[i], attBlock, defBlock, defDodge, attTackle,
                          attWrestle, defWrestle);
        if (attackerChooses) {
            if (s > bestScore) { bestScore = s; bestIdx = i; }
        } else {
            if (s < bestScore) { bestScore = s; bestIdx = i; }
        }
    }
    return faces[bestIdx];
}

static bool shouldRerollBlock(BlockDiceFace face, const Player& att) {
    if (face == BlockDiceFace::ATTACKER_DOWN) return true;
    if (face == BlockDiceFace::BOTH_DOWN && !att.hasSkill(SkillName::Block)) return true;
    return false;
}

// The square a pushed player leaves the pitch through. resolvePushback reports
// a surf as pushDest = {-1,-1}, which classifyExit cannot read, so the throw-in
// template needs this reconstructed exit to know which edge it is centred on.
// OPRAVENO 09.10.2026 (nález 3b) — dřív vždy pole „rovně za ním“. Od chvíle, kdy
// se do davu tlačí i hráč, který má mimo hřiště jen NĚKTERÉ ze tří polí odtlačení
// (ř. 650-651), může pole rovně za ním ležet na hřišti (odtlačení podél lajny) a
// šablona vhazování by mířila od špatné hrany. Bere se tedy první ze tří polí
// odtlačení, které je mimo hřiště (rovně, pak obě šikmá).
static Position pushOffPitchExit(Position pusher, Position pushed) {
    const int dx = (pushed.x > pusher.x) - (pushed.x < pusher.x);
    const int dy = (pushed.y > pusher.y) - (pushed.y < pusher.y);
    // rovně; pak dvě pole o 45° vedle: u šikmého směru (dx,0) a (0,dy), u přímého ±1 napříč
    const int dirs[3][2] = {{dx, dy},
                            {dx != 0 ? dx : -1, dx != 0 && dy != 0 ? 0 : (dy != 0 ? dy : -1)},
                            {dx != 0 && dy != 0 ? 0 : (dx != 0 ? dx : 1), dy != 0 ? dy : 1}};
    for (const auto& d : dirs) {
        const Position p{static_cast<int8_t>(pushed.x + d[0]), static_cast<int8_t>(pushed.y + d[1])};
        if (!p.isOnPitch()) return p;
    }
    return {static_cast<int8_t>(pushed.x + dx), static_cast<int8_t>(pushed.y + dy)};
}

static int distanceToEdge(Position p) {
    return std::min({(int)p.x, Position::PITCH_WIDTH - 1 - p.x,
                     (int)p.y, Position::PITCH_HEIGHT - 1 - p.y});
}

// The squares a pushed player may be moved into.
//   normal — the three squares directly away from the pusher (push-back diagram)
//   wide   — any adjacent square.  Side Step: "the coach may choose to move the
//            player to any adjacent square, not just the three squares shown on
//            the Push Back diagram".  Grab: "he may choose any empty square
//            adjacent to his opponent".
// Both skills lapse when nothing adjacent is open ("may not use this skill if
// there are no open squares on the pitch adjacent to this player" / "Grab will
// not work if there are no empty adjacent squares"), so a wide set that turns
// up empty-handed falls back to the standard three and the push chains as usual.
// `wide` se pak vrací jako false: skill neplatí ani pro výběr mezi třemi poli.
static int pushCandidates(const GameState& state, Position pusherPos,
                          Position pushedPos, bool& wide, Position out[8]) {
    if (wide) {
        int n = 0;
        for (Position p : pushedPos.getAdjacent()) {
            if (p.isOnPitch() && !state.getPlayerAtPosition(p)) out[n++] = p;
        }
        if (n > 0) return n;
        wide = false;
    }
    return getPushbackSquares(pusherPos, pushedPos, out);
}

// CRP: "The player must be pushed back into an empty square if possible."  That
// holds no matter who is doing the choosing, so empty candidates always win and
// the skill heuristics only rank within them.  Only when every candidate is
// occupied does the push go into an occupied square and chain.
// A push may not hand the man being pushed the touchdown he is carrying the
// ball toward.  CRP scores a carrier who ends up standing in his opponent's end
// zone even during our own turn ("Scoring in the opponent's turn": "a player
// holding the ball could be pushed into the End Zone by a block"), so this is a
// gift we are free to decline by choosing another square.  It went unnoticed
// because choosePushSquare picks on geometry alone -- straight back first -- and
// straight back is sometimes the end zone: measured over 3000 games, eight
// opposing carriers were walked in that way (g0289, pusher on (23,8), carrier on
// (24,7), straight back (25,6)).  Nothing here is a trade-off; the other two
// squares are strictly better, so the only case left alone is the one where
// every square scores and there is no choice to decline.
static bool pushWouldScore(const GameState& state, const Player& pushed,
                           TeamSide blockingSide, Position dest) {
    if (pushed.teamSide == blockingSide) return false;   // our own man scoring is a gift
    if (!state.ball.isHeld || state.ball.carrierId != pushed.id) return false;
    // HOME scores on x = PITCH_WIDTH-1, AWAY on x = 0 (turn_handler.cpp).
    return dest.isInEndZone(pushed.teamSide == TeamSide::AWAY);
}

// P9/P9c (2026-08-18): score a push DESTINATION instead of taking the first
// square geometry offers. The ordering is the user's, stated 2026-08-14:
//   "priorita u špinavého rohu je odklidit protihráče PRYČ od rohu -- ne jej
//    nechat u rohu a posunout blíž k balonu"
// so it is a sequence, not a trade-off:
//   (1) he stops being adjacent to a corner of OUR cage -- that was the point
//   (2) he does not end up closer to OUR ball carrier  -- REACH0, -16.7 sigma
//   (3) straight back                                   -- today's behaviour
// (3) stays as the tiebreak, so with no carrier on the pitch (i.e. on defence)
// this scores exactly like the old code and the arm is a no-op by construction.
static int pushDestScore(const GameState& state, TeamSide blockingSide,
                         Position dest, int straightBackScore) {
    const Player* carrier = nullptr;
    if (state.ball.isHeld && state.ball.carrierId > 0) {
        const Player& c = state.getPlayer(state.ball.carrierId);
        if (c.teamSide == blockingSide && c.isOnPitch()) carrier = &c;
    }
    if (!carrier) return straightBackScore;

    auto cheb = [](Position a, Position b) {
        return std::max(std::abs(a.x - b.x), std::abs(a.y - b.y));
    };

    // 08.09.2026 (uživatel): PRIORITA OTOČENA. "Pro nás je horší, když soupeř
    // sousedí s nosičem -- proto nejdřív odsunout od nosiče, pak až je
    // problém, že sousedí s rohem klece." Sousedství s nosičem je přímé
    // ohrožení míče; špinění rohu je až druhotné, řeší se teprve když
    // vzdálenost od nosiče už nerozhoduje (viz strop níž).

    // (1) distance from our carrier, capped -- past 4 squares he is out of
    // reach anyway and further shoving buys nothing worth outranking (2).
    int dist = std::min(cheb(dest, carrier->position), 4);

    // (2) does he still dirty a STANDING corner of our cage? -- tiebreak,
    // used only once (1) stops discriminating (dist tied, typically capped).
    bool dirties = false;
    for (int dx = -1; dx <= 1 && !dirties; dx += 2) {
        for (int dy = -1; dy <= 1 && !dirties; dy += 2) {
            Position corner{static_cast<int8_t>(carrier->position.x + dx),
                            static_cast<int8_t>(carrier->position.y + dy)};
            if (!corner.isOnPitch()) continue;
            const Player* occ = state.getPlayerAtPosition(corner);
            if (!occ || occ->teamSide != blockingSide ||
                occ->state != PlayerState::STANDING) continue;
            if (cheb(dest, corner) <= 1) dirties = true;
        }
    }

    return 10000 * dist + (dirties ? 0 : 100) + straightBackScore;
}

// Ruční volba kouče (živá partie s člověkem, 02.10.2026). Pravidla dávají
// výběr pole odtlačení i follow-up útočícímu kouči (l. 608-611); AI je volí
// pravidly níž, člověk je chce volit sám. Jednorázové: platí pro nejbližší
// odtlačení / follow-up a pak se smažou. Volba, kterou pravidla nedovolí (pole
// není mezi kandidáty, nebo je obsazené, zatímco jiné je volné; povinný
// follow-up u Frenzy), se ignoruje a rozhodne pravidlo.
namespace {
thread_local Position g_manualPush{-1, -1};
thread_local int g_manualFollowUp = -1;   // -1 = nezvoleno, 0 = ne, 1 = ano
thread_local Position g_manualBlitzSquare{-1, -1};
// 02.10.2026 živá partie: volby kouče — kterou z hozených kostek vzít (jen když vybírá útočník,
// spotřebuje se na jednom bloku) a zda útočník NEPOUŽIJE Tackle (ř. 8569-8571: „…and uses the Tackle skill“).
thread_local int g_manualFace = -1;
thread_local bool g_manualNoTackle = false;
thread_local Position g_manualSideStep{-1, -1};   // P130: pole, kam Side Step uhne
}  // namespace

void setManualPushChoice(Position p) { g_manualPush = p; }
void setManualFollowUp(bool follow) { g_manualFollowUp = follow ? 1 : 0; }
void setManualBlitzSquare(Position p) { g_manualBlitzSquare = p; }
void setManualBlockFace(int face) { g_manualFace = face; }
void setManualNoTackle(bool noTackle) { g_manualNoTackle = noTackle; }
void setManualSideStepSquare(Position p) { g_manualSideStep = p; }
Position takeManualBlitzSquare() {
    const Position p = g_manualBlitzSquare;
    g_manualBlitzSquare = {-1, -1};
    return p;
}
ManualBlockChoices saveManualBlockChoices() {
    return {g_manualPush, g_manualFollowUp, g_manualBlitzSquare, g_manualFace, g_manualNoTackle, g_manualSideStep};
}
void restoreManualBlockChoices(const ManualBlockChoices& c) {
    g_manualPush = c.push;
    g_manualFollowUp = c.followUp;
    g_manualBlitzSquare = c.blitzSquare;
    g_manualFace = c.face;
    g_manualNoTackle = c.noTackle;
    g_manualSideStep = c.sideStep;
}
void clearManualBlockChoices() {
    restoreManualBlockChoices(ManualBlockChoices{});   // výchozí = „nezvoleno“, jen ve struct
}

static int choosePushSquare(const GameState& state, const Position* cand, int count,
                            Position pusherPos, bool defenderChooses, bool towardEdge,
                            const Player& pushed, TeamSide blockingSide) {
    bool anyEmpty = false;
    for (int i = 0; i < count; i++) {
        if (!state.getPlayerAtPosition(cand[i])) { anyEmpty = true; break; }
    }

    // Side Step gives the choice to the pushed player's own coach, who would
    // take the touchdown, so the refusal only applies where the choice is ours.
    bool refuseScoring = false;
    if (!defenderChooses) {
        for (int i = 0; i < count; i++) {
            if (anyEmpty && state.getPlayerAtPosition(cand[i])) continue;
            if (!pushWouldScore(state, pushed, blockingSide, cand[i])) {
                refuseScoring = true;   // a square that does not score exists
                break;
            }
        }
    }

    // P130: pole pro Side Step má vlastní volbu (`sidestep`), ne `push` -- jinak by
    // se pole zvolené útočníkem použilo jako volba obránce a naopak.
    if (defenderChooses && coachDecides(pushed.teamSide) && g_manualSideStep.isOnPitch()) {
        const Position want = g_manualSideStep;
        g_manualSideStep = {-1, -1};
        for (int i = 0; i < count; i++) {
            if (cand[i] == want && !(anyEmpty && state.getPlayerAtPosition(cand[i]))) return i;
        }
    }
    if (!defenderChooses && g_manualPush.isOnPitch()) {
        const Position want = g_manualPush;
        g_manualPush = {-1, -1};
        for (int i = 0; i < count; i++) {
            if (cand[i] == want && !(anyEmpty && state.getPlayerAtPosition(cand[i]))) return i;
        }
    }

    int best = -1, bestScore = 0;
    for (int i = 0; i < count; i++) {
        if (anyEmpty && state.getPlayerAtPosition(cand[i])) continue;
        if (refuseScoring && pushWouldScore(state, pushed, blockingSide, cand[i])) continue;
        int score;
        if (defenderChooses)   score = cand[i].distanceTo(pusherPos);   // Side Step: get clear
        else if (towardEdge)   score = 100 - distanceToEdge(cand[i]);   // Grab: toward the crowd
        else                   score = pushDestScore(state, blockingSide, cand[i], count - i);  // P9/P9c
        if (best < 0 || score > bestScore) { best = i; bestScore = score; }
    }
    if (best < 0) return 0;

    // Diagnostic only, ticks on every redirect now that the rule is
    // unconditional -- no longer an A/B signal (see modeHasArmSignal-style
    // note for M1/N10). Says how often the rule actually changes the pick
    // vs. agreeing with straight-back, and against whom.
    if (!defenderChooses && !towardEdge) {
        int plain = -1, plainScore = 0;
        for (int i = 0; i < count; i++) {
            if (anyEmpty && state.getPlayerAtPosition(cand[i])) continue;
            if (refuseScoring && pushWouldScore(state, pushed, blockingSide, cand[i])) continue;
            int sc = count - i;
            if (plain < 0 || sc > plainScore) { plain = i; plainScore = sc; }
        }
        if (plain >= 0 && plain != best) {
            ++g_pushGeometryPicks;
            if (pushed.hasSkill(SkillName::Dodge)) ++g_pushGeometryPicksDodge;
            if (pushed.stats.movement >= 6) ++g_pushGeometryPicksFastMA;
        }
    }
    return best;
}

// Push one player a square away from `pusherPos`, chaining into whoever is in
// the way.  CRP: "If all such squares are occupied by other players, then the
// player is pushed into an occupied square, and the player that originally
// occupied the square is pushed back in turn.  This secondary push back is
// treated exactly like a normal push back as if the second player had been
// blocked by the first" — hence the recursion rather than a single hop.
// Returns true when the pushed player left the pitch.
//
// resolveSurfHere is false for the player who was actually blocked: resolveBlock
// runs his crowd surf itself, because it has to read his last square and then
// run the attacker's follow-up.  Further down the chain nobody else will, so
// those surfs are resolved here.
// Stand Firm reaches down the chain too: "If a player is pushed back into a
// player using Stand Firm then neither player moves." It is optional ("may
// choose to not be pushed back"), so it is used when it helps its owner --
// a bystander on the blocking team wants the free square a chain hands him,
// an opponent wants to jam the push.
static bool holdsGround(const Player& p, TeamSide blockingSide) {
    // Take Root (l. 8578-8579) drzi pole bezpodminecne -- i vlastnimu tymu:
    // zakorenený nesmi byt odsunut "for any reason", takze na rozdil od Stand
    // Firm to neni volba jeho trenéra a nezalezi na tom, kdo blokuje.
    if (p.rooted) return true;
    // Stand Firm jen STOJICI: "Only Extraordinary skills work when a player is
    // Prone or Stunned" (r. 1824-1825). P68 (30.09.2026), vzor PHP 3cec72e2.
    if (!(p.state == PlayerState::STANDING
          && p.hasSkill(SkillName::StandFirm) && p.teamSide != blockingSide)) return false;
    // P130: v živé partii volí kouč ("may choose to not be pushed back",
    // l. 8511) -- nechat se odtlačit může chtít kvůli pasti na Frenzy nebo
    // řetězovému odtlačení (uživatel 12.10.).
    if (coachDecides(p.teamSide))
        return askCoach(CoachQuestion::USE_STAND_FIRM, p.id, -1, 0, 0, 3) == 1;
    // P225 (past na Frenzy) tu záměrně NENÍ: sem chodí jen hráč v ŘETĚZU odtlačení
    // a pushAwayFrom (B&C, odtlačení mimo blok). Druhý blok Frenzy míří na přímo
    // blokovaného obránce (ř. 8139-8141 "against the same opponent"), ne na hráče v řetězu,
    // a pushAwayFrom druhý blok Frenzy nemá -- past tu nevzniká, AI drží pole jako dosud.
    // Přímo blokovaného obránce rozhoduje resolvePushback.
    return true;
}

// Side Step jen STOJÍCÍ. OPRAVENO 09.10.2026 (audit parity 08.10., nález 17) — v řetězu
// a v pushAwayFrom se četlo holé `hasSkill(SideStep)`, takže si pole vybíral i ležící
// nebo omráčený hráč. Pravidla ř. 1824-1825: „Only Extraordinary skills work when a
// player is Prone or Stunned.“ (Side Step je Agility.) Týž princip jako P68 u Stand Firm
// v holdsGround. Jedno místo pro všechna tři čtení.
static bool canSideStep(const Player& p) {
    return p.state == PlayerState::STANDING && p.hasSkill(SkillName::SideStep);
}

// Je vedle pole volné pole na hřišti? Bez něj Side Step ani Grab neplatí (ř. 8477-8478,
// 8153-8154) -- táž podmínka jako `wide` v pushCandidates.
static bool hasOpenAdjacent(const GameState& state, Position pos) {
    for (Position p : pos.getAdjacent())
        if (p.isOnPitch() && !state.getPlayerAtPosition(p)) return true;
    return false;
}

// P130: Side Step je volba ("his coach MAY choose", l. 8471-8473). V živé partii
// se kouč odtlačeného ptá; pole pak určí příkazem `sidestep` (jinak co nejdál od útočníka).
// OPRAVENO 20.10.2026 (P222, review M6) -- kouče se ptalo i tehdy, když vedle hráče nebylo
// volné pole a skill použít nešel (ř. 8477-8478 „may not use this skill if there are no open
// squares on the pitch adjacent to this player“). Teď se ptá, jen když platit může.
static bool usesSideStep(const GameState& state, const Player& p) {
    if (!canSideStep(p) || !hasOpenAdjacent(state, p.position)) return false;
    if (coachDecides(p.teamSide))
        return askCoach(CoachQuestion::USE_SIDE_STEP, p.id, -1, 0, 0, 3) == 1;
    return true;
}

static bool pushOne(GameState& state, Position pusherPos, Player& pushed,
                    bool sideStep, bool grab, bool resolveSurfHere,
                    TeamSide blockingSide, DiceRollerBase& dice, Position& dest,
                    std::vector<GameEvent>* events, int depth) {
    Position cand[8];
    bool wide = sideStep || grab;
    int count = pushCandidates(state, pusherPos, pushed.position, wide, cand);
    // OPRAVENO 20.10.2026 (review P192, nález 5) -- bez volného sousedního pole se
    // kandidáti vrátili na tři pole diagramu, ale výběr mezi nimi (komu se řetězí) dál
    // řídil skill: u Side Step obránce, u Grabu „k lajně“. Pravidla ř. 8153-8154 („Grab
    // will not work if there are no empty adjacent squares“) a ř. 8477-8478 (Side Step
    // „may not use this skill if there are no open squares on the pitch adjacent“) ⇒
    // platí běžná pravidla včetně toho, kdo volí (ř. 637-638: kouč útočníka).
    if (!wide) { sideStep = false; grab = false; }

    bool anyEmpty = false;
    for (int i = 0; i < count; i++) {
        if (!state.getPlayerAtPosition(cand[i])) { anyEmpty = true; break; }
    }
    Position three[3];
    const bool squareOffPitch = getPushbackSquares(pusherPos, pushed.position, three) < 3;

    // OPRAVENO 09.10.2026 (audit parity 08.10., nález 3b) — tady stálo `if (count == 0)`:
    // do davu šel hráč, jen když byla mimo hřiště VŠECHNA tři pole odtlačení; u lajny
    // s obsazeným polem se řetězilo. Pravidla ř. 639-651: „must be pushed back into an
    // empty square if possible“ (ř. 639) · řetěz jen „if ALL such squares are occupied
    // by other players“ (ř. 641-644) · „Players must be pushed off the pitch if there are
    // no eligible empty squares on the pitch“ (ř. 650-651). Pořadí: volné pole na hřišti >
    // dav (některé ze tří polí je mimo hřiště a žádné volné) > řetěz. Vzor PHP bdad4afc.
    if (!anyEmpty && squareOffPitch) {
        Position last = pushed.position;
        emitEvent(events, {GameEvent::Type::PUSH, pushed.id, -1, last, {-1, -1}, 0, true});
        dest = {-1, -1};
        if (resolveSurfHere) {
            if (state.ball.isHeld && state.ball.carrierId == pushed.id) {
                state.ball = BallState::onGround(last);
                resolveThrowIn(state, last, pushOffPitchExit(pusherPos, last), dice, events);
            } else {
                handleBallOnPlayerDown(state, pushed.id, dice, events);
            }
            pushed.position = {-1, -1};
            resolveCrowdSurf(state, pushed.id, last, dice, events);
        }
        return true;
    }

    const int pick = choosePushSquare(state, cand, count, pusherPos,
                                      sideStep && !grab, grab && !sideStep,
                                      pushed, blockingSide);
    dest = cand[pick];

    // Chain: every candidate is occupied, so the square has to be vacated first.
    // The coach picks the direction, so the chosen body is tried first and then
    // any other that will not dig in.  The end-zone refusal holds for those
    // alternatives too -- dodging a Stand Firm jam is not worth conceding a
    // touchdown.
    //
    // OPRAVENO 09.10.2026 (review P186, M3; vzor PHP d3f2c3ad) — zaseknutí se hlídalo
    // jen o JEDEN stupeň: když sám hráč v cestě pole nedržel, ale neměl se kam hnout
    // (za ním jen Stand Firm / zakořenění), zůstal stát a odtlačený byl přesto posunut
    // NA JEHO POLE — dva hráči na jednom poli a útočník následoval. Pravidla ř. 8514-8516:
    // „If a player is pushed back into a player with using Stand Firm then neither player
    // moves“ a ř. 644-646: řetězové odtlačení „is treated exactly like a normal push back“.
    // Teď se odtlačený pohne jen na pole, které se řetězem OPRAVDU uvolnilo; neuvolní-li
    // se žádné, nehýbe se nikdo. Neúspěšný pokus stav nemění (vrací se dřív, než kýmkoli
    // pohne), takže další pole lze zkusit bez vracení.
    if (!anyEmpty) {
        bool vacated = false;
        for (int k = 0; k < count && !vacated && depth < GameState::PLAYERS_TOTAL; k++) {
            const int i = (k == 0) ? pick : (k <= pick ? k - 1 : k);   // zvolené pole první
            Player* occupant = state.getPlayerAtPosition(cand[i]);
            if (!occupant || holdsGround(*occupant, blockingSide)) continue;
            if (i != pick && pushWouldScore(state, pushed, blockingSide, cand[i])) continue;
            // "The coach of the moving team decides all push back directions for
            // secondary push backs unless the pushed player has a skill that
            // overrides this" — so Side Step carries down the chain, Grab does not
            // (it only ever applies to the player its owner blocked).
            Position chainDest;
            pushOne(state, pushed.position, *occupant,
                    usesSideStep(state, *occupant), false,
                    /*resolveSurfHere=*/true, blockingSide, dice, chainDest, events,
                    depth + 1);
            if (occupant->position != cand[i]) {
                dest = cand[i];
                vacated = true;
            }
        }
        if (!vacated) {
            dest = pushed.position;   // "neither player moves"
            return false;
        }
    }

    emitEvent(events, {GameEvent::Type::PUSH, pushed.id, -1,
                       pushed.position, dest, 0, true});
    pushed.position = dest;
    if (state.ball.isHeld && state.ball.carrierId == pushed.id) {
        state.ball.position = dest;
    } else if (!state.ball.isHeld && state.ball.position == dest) {
        // Pushed onto a loose ball -- it scatters, no catch attempt/turnover
        resolveBounce(state, dest, dice, 0, events);
    }
    return false;
}

// Kdo vybírá pole prvního odtlačení: Side Step obránce × Grab útočníka. Jedno místo pro blok
// (resolvePushback) i blok Ball & Chain (pushAwayFrom). `grabMayApply` = Grab smí působit
// (Block Action; u B&C „as if a Block Action was being used“, ř. 7829-7831), v blitzu ne.
static void pushSkills(const GameState& state, const Player& attacker, const Player& defender,
                       bool grabMayApply, bool& sideStep, bool& grab) {
    sideStep = canSideStep(defender);
    // "Grab only works on a Block Action" and "Grab and Side Step will cancel
    // each other out and the standard pushback rules apply".
    // OPRAVENO 20.10.2026 (review P192, nález 3) -- zrušení se počítalo z `grab`, které je
    // v blitzu vypnuté, takže v blitzu si obránce se Side Step pole vybíral sám. Pravidla
    // ř. 8151-8153: „When making a Block OR BLITZ Action, Grab and Side Step will cancel
    // each other out“ -- Grab v blitzu sám nepůsobí (ř. 8148), Side Step ale ruší.
    const bool hasGrab = attacker.hasSkill(SkillName::Grab);
    grab = hasGrab && grabMayApply;
    if (sideStep && hasGrab) { sideStep = false; grab = false; }
    if (sideStep) sideStep = usesSideStep(state, defender);   // P130: volba kouče
    // OPRAVENO 20.10.2026 (review P192, nález 4) -- Grab se použil vždy, a protože umí jen
    // prázdná pole na hřišti, útočník s Grabem nikdy nevytlačil do davu. Grab je volba
    // (ř. 8149 „he MAY choose“; uživatel 10.10.: „může grab nevyužít pro vysurfování“):
    // vede-li běžné odtlačení do davu (žádné ze tří polí volné, některé mimo hřiště,
    // ř. 650-651 -- táž podmínka jako v pushOne), útočník ho nepoužije.
    // P222 (2): v živé partii volí kouč útočníka (uživatel 20.10.: „grab ano ptát se — pro
    // možnost vytlačení“); ptá se jen, když Grab platit může (ř. 8153-8154: volné pole vedle).
    if (grab && !hasOpenAdjacent(state, defender.position)) grab = false;
    if (grab && coachDecides(attacker.teamSide)) {
        grab = askCoach(CoachQuestion::USE_GRAB, attacker.id, defender.id, 0, 0, 3) == 1;
    } else if (grab) {
        Position three[3];
        const int onPitch = getPushbackSquares(attacker.position, defender.position, three);
        bool anyEmpty = false;
        for (int i = 0; i < onPitch; i++) {
            if (!state.getPlayerAtPosition(three[i])) anyEmpty = true;
        }
        if (onPitch < 3 && !anyEmpty) grab = false;
    }
}

// Smí blitzující zaplatit další blok (pole pohybu, případně GFI)? ř. 8143-8145: Frenzy
// v blitzu "must pay a square of Movement and must make the second block unless he has no
// further normal movement and cannot go for it again". Jedno místo pro druhý blok Frenzy
// i pro P225 (past na Frenzy počítá jen s blokem, který se opravdu hodí).
static bool blitzBlockPayable(const Player& att) {
    // Take Root, l. 8577-8578: zakorenený nesmi GFI.
    const int gfiFloor = att.rooted ? 0 : (att.hasSkill(SkillName::Sprint) ? -3 : -2);
    return att.movementRemaining - 1 >= gfiFloor;
}

// Kostky bloku ze strengthů PO Horns a Dauntless a PŘED asistencemi (ř. 8023-8033: Dauntless
// "before any defensive or offensive assists are added") + asistence z polí, kde hráči právě
// stojí. Jedno místo pro hod bloku (resolveBlock) i pro P225 (frenzyTrapReady).
static BlockDiceInfo blockDiceFor(const GameState& state, const Player& att, const Player& def,
                                  int attST, int defST) {
    const int a = countAssists(state, def.position, att.teamSide, att.id, def.id, def.id);
    const int d = countAssists(state, att.position, def.teamSide, att.id, def.id, att.id);
    return getBlockDiceInfo(attST + a, defST + d);
}

// P225: podepsaný počet kostek (+ volí útočník, − obránce), jako getBlockDiceCount.
// Horns na druhý blok neplatí (secondParams.hornsBonus = false).
static int signedDice(const GameState& state, const Player& att, const Player& def, int attST) {
    const BlockDiceInfo info = blockDiceFor(state, att, def, attST, def.stats.strength);
    return info.attackerChooses ? info.count : -info.count;
}

// P225 (uživatel 21.10.2026): nachystaná past na Frenzy. Stand Firm je volba (ř. 8510-8516
// "may choose to not be pushed back"; ř. 985 a 1820 "Skill use is not mandatory"). Frenzy po
// Pushed / Defender Stumbles musí následovat a hodit druhý blok, dokud oba stojí a sousedí
// (ř. 8134-8141). Se Stand Firm hází druhý blok ze STEJNÉHO pole, bez něj z NOVÉHO po
// odtlačení a follow-upu. Past = druhý blok z nového pole je pro útočníka horší (méně kostek
// pro něj), odtlačení nevede do davu a obránce nenese míč. Jen tato akce, žádný výhled dál.
// Kam bude obránce odtlačen, rozhoduje útočník -- proto se odtlačení přehraje na kopii stavu
// TÝMIŽ funkcemi jako skutečné (pushSkills + pushOne: výběr pole, Side Step, Grab, řetěz, dav),
// s neutrálními kostkami (hází se jen v řetězu u cizích hráčů, na pole obránce to nemá vliv).
// Volá se jen mimo živou partii: tam pole volí člověk až potom, AI ho předem nezná.
// Útočník, který NESMÍ následovat (Fend obránce ř. 8113-8118 bez Juggernautu v blitzu; Take Root
// ř. 8581-8582 „without following-up“), zůstane stát a odtlačený obránce s ním přestane sousedit
// ⇒ druhý blok Frenzy odpadne (ř. 8139-8141 „both still standing and adjacent“). Se Stand Firm by
// přišel. Uživatel 21.10. („1 ano“): i tehdy AI Stand Firm nepoužije (za týchž podmínek (b), (c)).
// Zůstane-li obránce po odtlačení vedle (Side Step, řetěz), druhý blok přijde a platí srovnání (a).
static bool frenzyTrapReady(const GameState& state, const Player& att, const Player& def,
                            bool isBlitz, bool followUpAllowed) {
    if (state.ball.isHeld && state.ball.carrierId == def.id) return false;      // (c)
    struct NeutralDice : DiceRollerBase {
        int rollD6() override { return 4; }
        int rollD8() override { return 1; }
    } neutral;
    // Přehrání nesmí sníst ruční volbu kouče ani tiknout diagnostické čítače geometrie
    // odtlačení -- strážce je vrátí na každé cestě ven.
    struct RestoreOnExit {
        ManualBlockChoices manual = saveManualBlockChoices();
        long picks = g_pushGeometryPicks, dodge = g_pushGeometryPicksDodge,
             fast = g_pushGeometryPicksFastMA;
        ~RestoreOnExit() {
            restoreManualBlockChoices(manual);
            g_pushGeometryPicks = picks; g_pushGeometryPicksDodge = dodge; g_pushGeometryPicksFastMA = fast;
        }
    } restore;
    GameState sim = state;
    Player& sa = sim.getPlayer(att.id);
    Player& sd = sim.getPlayer(def.id);
    bool sideStep, grab;
    pushSkills(sim, sa, sd, /*grabMayApply=*/!isBlitz, sideStep, grab);
    Position dest;
    const bool surf = pushOne(sim, sa.position, sd, sideStep, grab, /*resolveSurfHere=*/false,
                              sa.teamSide, neutral, dest, nullptr, 0);
    // (b) explicitně: do davu ⇒ Stand Firm. Kryla by to i kontrola polohy níž (obránce mimo
    // hřiště nesousedí), ale podmínka zadání má stát na vlastním řádku.
    if (surf) return false;
    if (sd.position == def.position) return false;   // řetěz se zasekl: nikdo se nehne, tentýž blok
    if (followUpAllowed) sa.position = def.position; // povinný follow-up Frenzy (ř. 8138)
    // Bez Stand Firm druhý blok nebude (bez follow-upu obránce odletěl), se Stand Firm by přišel
    // ⇒ Stand Firm nepoužít. S follow-upem sem nedojde: útočník stojí na uvolněném poli vedle.
    if (!canAct(sd.state) || sa.position.distanceTo(sd.position) != 1) return true;
    // (a) Dauntless (ř. 8023-8033) zvedá ST útočníka na ST obránce PŘED asistencemi, a kostky
    // jsou poměrové -- jedno srovnání bez něj by lhalo (ST3 vs ST6: +asistence obránce mění
    // počet kostek jinak než u ST6 vs ST6). Hod Dauntless ještě nepadl ⇒ obě větve: past jen,
    // když je nové pole horší aspoň v jedné a v žádné lepší.
    const int baseST = att.stats.strength;
    const bool dauntless = att.hasSkill(SkillName::Dauntless) && def.stats.strength > baseST;
    bool worse = false;
    for (const int st : {baseST, dauntless ? def.stats.strength : baseST}) {
        const int fresh = signedDice(sim, sa, sd, st), same = signedDice(state, att, def, st);
        if (fresh > same) return false;
        worse = worse || fresh < same;
    }
    return worse;
}

// Resolve pushback: returns true if defender was pushed off pitch (crowd surf)
// `frenzyTrapPossible` (P225): se Stand Firm by po tomto odtlačení přišel druhý blok Frenzy
// (obránce zůstane stát, v blitzu je čím zaplatit); `followUpAllowed`: útočník smí následovat.
// Obojí skládá resolveBlock, který to ví.
static bool resolvePushback(GameState& state, Player& attacker, Player& defender,
                            bool isBlitz, bool frenzyTrapPossible, bool followUpAllowed,
                            DiceRollerBase& dice,
                            Position& pushDest, std::vector<GameEvent>* events) {
    // Take Root, l. 8578-8579: zakorenený hráč "may not Go For It, BE PUSHED
    // BACK FOR ANY REASON, or use any skill that would allow him to move out of
    // his current square". Zadny Juggernaut override -- pravidlo vyjimku nema.
    if (defender.rooted) {
        pushDest = defender.position;
        return false; // no push
    }

    // StandFirm check (Juggernaut ignores on blitz)
    if (defender.hasSkill(SkillName::StandFirm)) {
        bool juggernautOverride = isBlitz && attacker.hasSkill(SkillName::Juggernaut);
        // P130: v živé partii volí kouč obránce, jestli Stand Firm použije.
        bool declined = !juggernautOverride && coachDecides(defender.teamSide) &&
            askCoach(CoachQuestion::USE_STAND_FIRM, defender.id, attacker.id, 0, 0, 3) == 0;
        // P225: AI obránce nepoužije Stand Firm, když na něj útočník s Frenzy nachystal past,
        // nebo když bez Stand Firm druhý blok Frenzy odpadne, protože útočník nesmí následovat
        // (frenzyTrapReady). Rozhoduje se TADY, ne v holdsGround: přímo blokovaného obránce řeší
        // jen tahle větev; holdsGround slouží řetězu a pushAwayFrom, kde past nevzniká (viz tam).
        if (!juggernautOverride && !declined && !coachModeOn() && frenzyTrapPossible &&
            extraFeatureOn(kP225FrenzyTrapStandFirm) &&
            frenzyTrapReady(state, attacker, defender, isBlitz, followUpAllowed)) {
            declined = true;
            ++g_standFirmDeclinedForFrenzy;
        }
        if (!juggernautOverride && !declined) {
            // P201 (4): v živé partii vlastní řádek (dřív bez důvodu, proč odtlačen nebyl)
            if (coachModeOn()) emitEvent(events, {GameEvent::Type::SKILL_USED, defender.id, attacker.id, {}, {},
                                                  static_cast<int>(SkillName::StandFirm), true});
            pushDest = defender.position;
            return false; // no push
        }
    }

    bool sideStep, grab;
    pushSkills(state, attacker, defender, /*grabMayApply=*/!isBlitz, sideStep, grab);
    return pushOne(state, attacker.position, defender, sideStep, grab,
                   /*resolveSurfHere=*/false, attacker.teamSide, dice, pushDest,
                   events, 0);
}

bool pushAwayFrom(GameState& state, Player& pusher, Player& pushed, bool isBlock,
                  DiceRollerBase& dice, std::vector<GameEvent>* events) {
    if (holdsGround(pushed, pusher.teamSide)) return false;
    // OPRAVENO 20.10.2026 (P222) -- tady stálo `grab=false` napevno, takže B&C s Grabem tlačil
    // jen běžně. Pravidla ř. 7829-7831: „A Ball & Chain player may use the Grab skill (as if a
    // Block Action was being used) with his blocks“ ⇒ u BLOKU tatáž volba jako v Block Action
    // (i zrušení se Side Step); odtlačení ležícího blok není (ř. 7843-7845), tam Grab neplatí.
    bool sideStep = false, grab = false;
    if (isBlock) pushSkills(state, pusher, pushed, /*grabMayApply=*/true, sideStep, grab);
    else sideStep = usesSideStep(state, pushed);
    Position dest;
    const bool surf = pushOne(state, pusher.position, pushed, sideStep, grab,
                              /*resolveSurfHere=*/false, pusher.teamSide, dice, dest,
                              events, 0);
    if (!surf) return false;
    // Tataz vetev jako crowd surf v resolveBlock: nosic -> vhazovani od
    // posledniho pole, jinak mic z jeho pole.
    const Position lastPos = pushed.position;
    if (state.ball.isHeld && state.ball.carrierId == pushed.id) {
        state.ball = BallState::onGround(lastPos);
        resolveThrowIn(state, lastPos, pushOffPitchExit(pusher.position, lastPos), dice, events);
    } else {
        handleBallOnPlayerDown(state, pushed.id, dice, events);
    }
    pushed.position = {-1, -1};
    resolveCrowdSurf(state, pushed.id, lastPos, dice, events);
    return true;
}

// M1/N10: jediná definice toho, kdy blok aktivaci ZAVÍRÁ. Volá se ze VŠECH
// míst, kde blok normálně končí -- 25.08. jsem nejdřív opravil jen to poslední
// a větev s pushnutím (ř. ~875) se vrací dřív, takže test padal a vypadalo to,
// jako by rameno nefungovalo. Dvě kopie téhož pravidla = jedna z nich zestárne.
//
// ⭐ M1/N10 NASAZENO 07.09.2026 -- rameno odebrano, otevrena aktivace je
// PRODUKCE. Noc 27.->28.08. dala +0,0177 +- 0,0069 (>2,5 sigma, 6/6
// predpovedi). Duvod odebrani je tyz jako u P35 (01.09.) a M13 (02.09.):
// pravidlova oprava, o ktere je dolozeno, ze POMAHA, nema co delat za
// default-OFF vypinacem -- to je jen dalsi hotova vec, o ktere nikdo nevi.
static void endBlockActivation(Player& att, const BlockParams& params) {
    const bool staysOpen = params.isBlitz && canAct(att.state);
    if (staysOpen) noteBlitzContinuationEvent();
    att.hasActed = !staysOpen;
}

// M1c/T5.29 (25.08.2026): FOLLOW-UP JE VOLBA, NE DŮSLEDEK.
// BB2016 l. 608-611: "A player who has made a block IS ALLOWED to make a
// special follow up move... The player's COACH MUST DECIDE whether to follow
// up." Do 25.08. se následovalo vždy: `noFollowUp` defaultoval na false a žádné
// volací místo ho nepředalo.
//
// ⭐ Tvar řešení je převzatý z Wrestle (24.08., d0093607): rozhodovací pravidlo
// v resolveru, sepsané i s důvody. Plánovač by to unesl líp, ale tady je volba
// BINÁRNÍ a LOKÁLNÍ -- follow-up je zdarma, ignoruje tackle zóny a týká se
// jediného pole -- takže nepotřebuje vlastní vrstvu.
//
// ⭐ Default je NÁSLEDOVAT, a to schválně: follow-up je pohyb ZDARMA, který
// nemusí dodgovat ("This move is free, and the player can ignore enemy tackle
// zones"). Je to tedy obvykle výhoda, ne past -- odmítá se jen tam, kde má
// blitzující ještě otevřenou aktivaci a uvolněné pole by ho dalo do VÍC
// soupeřových zón než to, kde stojí. To je přesně hit-and-run (M1/N10).
static bool wantsFollowUp(const GameState& state, const Player& att, const Player& def,
                          const BlockParams& params, Position vacated) {
    // Povinné případy -- pravidla volbu neposkytují:
    //   Frenzy l. 8138: "must always follow up if they can"
    //   Ball & Chain l. 7825: "must follow up if they push back another player"
    if (att.hasSkill(SkillName::Frenzy) ||
        att.hasSkill(SkillName::BallAndChain)) {
        return true;
    }
    if (g_manualFollowUp >= 0) {   // kouč rozhodl sám (živá partie)
        const bool f = g_manualFollowUp == 1;
        g_manualFollowUp = -1;
        return f;
    }
    // P201 (12), 14.10.2026: v živé partii se kouč útočníka ptá AŽ TEĎ -- po kostkách, po
    // přehozu i po odtlačení (l. 606-611 "The player's COACH MUST DECIDE"). Dřív šlo jen
    // předem příkazem `follow`, a po přehozu kostek bloku už volbu nešlo změnit.
    if (coachDecides(att.teamSide)) {
        return askCoach(CoachQuestion::USE_FOLLOW_UP, att.id, def.id, 0, 0,
                        (1 << CoachAnswer::NONE) | (1 << CoachAnswer::TEAM)) == CoachAnswer::TEAM;
    }
    // Mimo blitz je blok celá aktivace: není co si šetřit, pole zdarma se bere.
    // ⭐ P154 (07.10.2026): ROH KLECE PO RÁNĚ NENÁSLEDUJE. Dosud běžný blok následoval VŽDY
    //   (řádek níž), takže roh, který zahrál blok, z rohu odešel — klec měla hned po tahu
    //   řadiče 3,15 rohu a na konci tahu 2,17; 14× roh „zmizel“ právě blokem (40 poločasů).
    //   Roh = stojím na úhlopříčce vedle vlastního stojícího nosiče; následování by mě z ní
    //   odvedlo (uvolněné pole není úhlopříčka nosiče). Frenzy následovat musí (výš).
    if (state.ball.isHeld && state.ball.carrierId > 0 && state.ball.carrierId != att.id) {
        const Player& carrier = state.getPlayer(state.ball.carrierId);
        auto diagonal = [&](Position sq) {
            return std::abs(sq.x - carrier.position.x) == 1 && std::abs(sq.y - carrier.position.y) == 1;
        };
        if (carrier.teamSide == att.teamSide && carrier.state == PlayerState::STANDING &&
            diagonal(att.position) && !diagonal(vacated)) {
            return false;
        }
    }
    if (!params.isBlitz) return true;
    // Blitzující, který už nemá čím pokračovat, taky nemá co získat.
    if (!canAct(att.state) || att.movementRemaining <= 0) return true;

    const int tzStay = countTacklezones(state, att.position, att.teamSide);
    const int tzGo   = countTacklezones(state, vacated, att.teamSide);
    if (tzGo > tzStay) {
        noteBlitzContinuationEvent();
        return false;
    }
    return true;
}

// Follow-up útočníka na pole uvolněné obráncem (l. 606-611). Volá se HNED po
// odtlačení, před jakýmkoli dalším hodem (l. 609-610) -- míč, který se pak odrazí
// na jeho nové pole, tak najde stojícího hráče a ten ho chytá (resolveBounce).
// 2026-08-28: FOLLOW_UP se emituje I KDYŽ se nenásleduje -- `success` nese volbu,
// ne úspěch hodu; bez toho v logu chybí jmenovatel (viz game_event.h).
static void followUp(GameState& state, Player& att, const Player& def, const BlockParams& params,
                     Position defOldPos, DiceRollerBase& dice, std::vector<GameEvent>* events) {
    const Position attOldPos = att.position;
    const bool wants = wantsFollowUp(state, att, def, params, defOldPos);
    emitEvent(events, {GameEvent::Type::FOLLOW_UP, att.id, def.id, attOldPos, defOldPos, 0, wants});
    if (!wants) return;
    att.position = defOldPos;
    if (state.ball.isHeld && state.ball.carrierId == att.id) {
        state.ball.position = att.position;
    } else if (!state.ball.isHeld && state.ball.position == att.position) {
        // Míč na uvolněném poli už leží: odrazil se tam během odtlačení (odtlačený
        // vstoupil na pole s míčem, l. 605-607), tedy hodem, který podle l. 609-610
        // přišel až PO rozhodnutí o follow-upu -- útočník tam už stál a míč CHYTÁ.
        // OPRAVENO 13.10.2026 (P194/6, uživatel: „ano chytá ... při follow se hýbe
        // voluntarily“) -- tady bylo zvednutí (jiný cíl hodu) a neúspěch = turnover.
        if (!resolveCatch(state, att.id, dice, 0, events)) {
            resolveBounce(state, att.position, dice, 0, events);
        }
    }
}

// ============================================================================
// Q3 krok B (31.08.2026): MERIDLO SOUPEROVY ODPOVEDI NA VSTANI
//
// Uzivatelova doktrina (20.08.): kdo vstane vedle soupere, meni se z "stoji je
// BLITZ" na "da se srazit BLOKEM ZDARMA" -- a blok je NEOMEZENY zdroj, kdezto
// blitz je jeden za kolo. Doktrina tedy tvrdi neco o TOM, CO SOUPER UDELA,
// a to se z nabidky ani z provedeni vstani precist NEDA.
//
// ⛔ Rozliseni blok vs blitz je cely smysl mericich: kdyby se ukazalo, ze na
//   vstale hrace se plytva BLITZEM, doktrina by neplatila -- souper by za to
//   platil svym vzacnym zdrojem misto toho, aby bral neco zadarmo.
// ============================================================================
// ⛔ 01.09.: POCITADLO HOZENYCH RAN, NEZAVISLE NA UDALOSTECH.
// M13 diagnostika nejdriv hledala blok v `events` -- jenze behem hledani je
// `events == nullptr`, takze hlasila "blitz bez rany" u KAZDEHO blitzu z lehu
// (3 909 z 5 981 na sonde). Tataz past jako u `cageSnapshot`, T5.34:
// MERIDLO NESMI VISET NA TOM, JESTLI SE ZROVNA LOGUJE.
thread_local long g_blocksThrown = 0;
long blockThrowSeq() { return g_blocksThrown; }

// ⭐ P37b (01.09.2026): BLITZ DEKLAROVAN, ALE RANA JE NEZAPLATITELNA.
//   Blok pri blitzu stoji jedno pole (r. 549-550). Nabidka to u STOJICICH
//   hracu nekontroluje -- lezici ji od 31.08. maji, stojici ne. Hrac tedy
//   muze blitz deklarovat, tym prijde o svuj JEDINY blitz na kolo, a rana
//   se nehodi, protoze neni z ceho.
//   ⛔ MERIME NEZ OPRAVUJEME: vim, ze ta cesta v kodu existuje, ale ne,
//     jestli nastava. Rozpad `BLITZ/PROC` ji NEVIDI -- hrac u cile stoji,
//     chuzi neprojde, a odmitnuti rany zadny citac netiklo.
thread_local long g_blitzBlockUnpayable = 0;
long takeBlitzBlockUnpayableInSearch() {
    long v = g_blitzBlockUnpayable; g_blitzBlockUnpayable = 0; return v;
}

thread_local long g_hitOnStoodUp = 0;
thread_local long g_hitOnStoodUpByBlitz = 0;
thread_local long g_knockedOnStoodUp = 0;

long takeHitOnStoodUpInSearch() { long v = g_hitOnStoodUp; g_hitOnStoodUp = 0; return v; }
long takeHitOnStoodUpByBlitzInSearch() { long v = g_hitOnStoodUpByBlitz; g_hitOnStoodUpByBlitz = 0; return v; }
long takeKnockedOnStoodUpInSearch() { long v = g_knockedOnStoodUp; g_knockedOnStoodUp = 0; return v; }

// Tri emitovaci mista srazeni obrance (r. ~547, ~794, ~968) volaji TOHLE, aby
// pravidlo nebylo opsane trikrat -- dve kopie tehoz pravidla znamenaji, ze
// jedna z nich zestarne (poucení z `endBlockActivation`, 25.08.).
static void noteKnockdownOnStoodUp(const Player& def) {
    if (def.stoodUpNextToEnemy) ++g_knockedOnStoodUp;
}

ActionResult resolveBlock(GameState& state, const BlockParams& params,
                          DiceRollerBase& dice, std::vector<GameEvent>* events,
                          bool frenzySecondBlock, bool noFollowUp,
                          bool frenzyDisabled) {
    Player& att = state.getPlayer(params.attackerId);
    Player& def = state.getPlayer(params.targetId);
    Position attOldPos = att.position;
    Position defOldPos = def.position;

    // Q3: zaucotvat POKUS jeste nez se cokoli hodi. Frenzy druhy blok se
    // nepocita znovu -- je to tataz rana, ne dalsi rozhodnuti soupere.
    if (def.stoodUpNextToEnemy && !frenzySecondBlock) {
        ++g_hitOnStoodUp;
        if (params.isBlitz) ++g_hitOnStoodUpByBlitz;
    }

    // Take Root, l. 8580-8582: "The player may block adjacent players WITHOUT
    // FOLLOWING-UP as part of a Block Action." Zakorenený se tedy nikam nehne
    // -- follow-up by ho vytahl z pole, ktere opustit nesmi (l. 8578-8579).
    if (att.rooted) noFollowUp = true;

    // Ř. 347-350: v blitzu jen jeden blok (Frenzy druhý blok je táž rána). Viz Player::blitzBlockThrown.
    if (params.isBlitz) att.blitzBlockThrown = true;

    // CRP: a block thrown as part of a Blitz costs 1 point of movement.
    // Out of normal movement the blitzer may GFI into the block (2+, 3+
    // in a blizzard, counts against the GFI limit); with no GFI left the
    // block cannot be thrown at all. This is what denies a Frenzy second
    // block after "GFI to arrive + GFI for the first block".
    if (params.isBlitz) {
        if (!blitzBlockPayable(att)) {
            // No movement and no GFI left: no block is thrown.
            ++g_blitzBlockUnpayable;   // P37b
            att.hasActed = true;
            return ActionResult::ok();
        }
        att.movementRemaining--;
        if (att.movementRemaining < 0) {
            int gfiTarget = (state.weather == Weather::BLIZZARD) ? 3 : 2;
            bool gfiOk = attemptRollLogged(state, att.id, dice, gfiTarget, SkillName::SureFeet, false, true,
                                           events, GameEvent::Type::GFI, att.position, att.position);
            if (!gfiOk) {
                // Falls before the block is thrown
                att.setState(PlayerState::PRONE);
                att.hasActed = true;
                InjuryContext ctx;
                resolveArmourAndInjury(state, att.id, dice, ctx, events);
                handleBallOnPlayerDown(state, att.id, dice, events);
                return ActionResult::turnovr();
            }
        }
    }

    // 1. FoulAppearance check
    if (def.hasSkill(SkillName::FoulAppearance)) {
        int faRoll = dice.rollD6();
        if (faRoll == 1) {
            // Ř. 8131-8133: „too revolted to make the block and IT is wasted“ — propadá BLOK, ne akce
            // (srov. „the Action is wasted“ u Wild Animal ř. 8669). V blitzu tedy smí dál běžet
            // (ř. 548-553), blok blitzu je spotřebován (blitzBlockThrown). Dřív `hasActed = true`
            // vždy (15.10.2026). Propadlý blok pole STOJÍ (ř. 550, strhává se výš) — uživatel 15.10.:
            // „nikde nevidím důvod nespotřebovat bod pohybu na deklarovaný block“.
            endBlockActivation(att, params);
            emitEvent(events, {GameEvent::Type::SKILL_USED, def.id, att.id, {}, {},
                              static_cast<int>(SkillName::FoulAppearance), true});
            return ActionResult::fail(); // block wasted, not turnover
        }
    }

    // 2. Chainsaw
    if (att.hasSkill(SkillName::Chainsaw)) {
        int chainsawRoll = dice.rollD6();
        ++g_blocksThrown;
        emitEvent(events, {GameEvent::Type::BLOCK, att.id, def.id, att.position,
                          def.position, chainsawRoll, chainsawRoll >= 2});

        // TA7 (24.08.2026) -- BB2016 l. 8001-8006: "Make an Armour roll for the
        // player hit by the chainsaw, ADDING 3 TO THE SCORE. If the roll beats
        // the victim's Armour value then the victim is Knocked Down and injured.
        // If the roll FAILS to beat the victim's Armour value then the attack
        // HAS NO EFFECT."
        // Dve vady: (1) +3 nikde -- pila mela probojnost obycejneho bloku;
        // (2) kickback srazil nositele a vratil turnover BEZ OHLEDU na to, jestli
        // zbroj vubec prorazil, a test `ChainsawKickback` to certifikoval
        // (kostky {1,3,3} => 6 <= AV8 "neproraženo", a presto PRONE + turnover).
        InjuryContext ctx;
        ctx.chainsaw = true;   // +3 (P75: pres priznak, at se nesecte s nositelem)

        if (chainsawRoll == 1) {
            // Kickback: obeti je NOSITEL
            bool broken = resolveArmourAndInjury(state, att.id, dice, ctx, events);
            att.hasActed = true;
            if (!broken) return ActionResult::ok();   // "no effect"
            emitEvent(events, {GameEvent::Type::KNOCKED_DOWN, att.id, -1,
                              att.position, {}, 0, false});
            handleBallOnPlayerDown(state, att.id, dice, events);
            // Sražený hráč aktivního tymu = turnover (katalog l. 368-369).
            return ActionResult::turnovr();
        } else {
            bool broken = resolveArmourAndInjury(state, def.id, dice, ctx, events);
            att.hasActed = true;
            if (broken) {
                emitEvent(events, {GameEvent::Type::KNOCKED_DOWN, def.id, -1,
                                  def.position, {}, 0, false});
                noteKnockdownOnStoodUp(def);   // Q3
                handleBallOnPlayerDown(state, def.id, dice, events);
            }
            return ActionResult::ok();
        }
    }

    // 3. Stab
    if (att.hasSkill(SkillName::Stab)) {
        emitEvent(events, {GameEvent::Type::BLOCK, att.id, def.id, att.position,
                          def.position, 0, true});
        InjuryContext ctx;
        ctx.unmodifiedArmour = true;   // Stab: "unmodified Armour roll" (P75: bez +3 za pilu)
        if (att.hasSkill(SkillName::Stakes)) ctx.hasStakes = true;
        // OPRAVENO 16.10.2026 (P199) — tady se míč pouštěl vždy, i když bodnutí zbroj neprorazilo.
        // Pravidla ř. 8496-8498: „less than or equal to the victim's Armour value then the attack has
        // no effect“ ⇒ obránce stojí a míč drží. Probodnutý: KO / zranění odrazí removeFromPitch,
        // omráčený se odrazí tady — obojí až po hodu na zranění (ř. 678-681).
        const bool wounded = resolveArmourAndInjury(state, def.id, dice, ctx, events);
        if (wounded) handleBallOnPlayerDown(state, def.id, dice, events);
        att.hasActed = true;
        return ActionResult::ok(); // Stab never causes turnover
    }

    // 4. Normal block — calculate strengths
    int attST = att.stats.strength;
    int defST = def.stats.strength;

    if (params.hornsBonus && att.hasSkill(SkillName::Horns)) {
        attST += 1;
    }

    // Dauntless, before assists are counted. CRP: "The skill only works when the
    // player attempts to block an opponent who is stronger than himself ... If
    // the total is greater, then the player with the Dauntless skill counts as
    // having a Strength equal to his opponent's when he makes the block. The
    // strength of both players is calculated before any defensive or offensive
    // assists are added but after all other modifiers."
    //
    // It used to be tested after assists, against effDefST > effAttST, which
    // fired it whenever the opponent merely out-assisted us -- and then it could
    // not fail, since d6 + ST > ST always holds at equal strength. It also
    // equalised onto the opponent's assisted total, handing our player their
    // assists instead of his own. Both are gone: the comparison, the roll and
    // the equalisation now all happen on the modified pre-assist strengths, and
    // assists are added on top afterwards.
    // Dauntless, BB2016 l. 8023-8033 (doslova shodne s CRP): "The skill only
    // works when the player attempts to block an opponent who is STRONGER than
    // himself. ... rolls a D6 and adds it to his strength. If the total is EQUAL
    // TO OR LOWER than the opponent's Strength, the player must block using his
    // normal Strength. If the total is GREATER, then the player counts as having
    // a Strength EQUAL TO HIS OPPONENT'S ... calculated BEFORE any defensive or
    // offensive assists are added but AFTER all other modifiers."
    if (att.hasSkill(SkillName::Dauntless) && defST > attST) {
        // ⛔ Tady stálo, že tohle počítá, „kolikrát se OPRAVDU hodil", a že
        // rozdíl proti nabídce odpovídá na „vzal si to search?". Obojí je
        // nepravda: sem se chodí i z každé simulace MCTS. 349/zápas proti
        // 1,88 odehraným (P25, 17.08.) -- 186×. Odehrané se čtou z logu:
        // SKILL_USED s roll == SkillName::Dauntless.
        ++g_dauntlessRolls;
        int dauntlessRoll = dice.rollD6();
        bool psyched = dauntlessRoll + attST > defST;
        if (psyched) {
            attST = defST;
        }
        emitEvent(events, {GameEvent::Type::SKILL_USED, att.id, -1, {}, {},
                          static_cast<int>(SkillName::Dauntless), psyched});
    }

    // Roll block dice
    BlockDiceInfo diceInfo = blockDiceFor(state, att, def, attST, defST);

    BlockDiceFace faces[3];
    for (int i = 0; i < diceInfo.count; i++) {
        faces[i] = dice.rollBlockDie();
    }

    // Choose best die
    BlockDiceFace chosen = autoChooseBlockDie(faces, diceInfo.count,
                                               diceInfo.attackerChooses, att, def);

    // Juggernaut: BD → PUSHED on blitz
    if (chosen == BlockDiceFace::BOTH_DOWN && params.isBlitz &&
        att.hasSkill(SkillName::Juggernaut)) {
        chosen = BlockDiceFace::PUSHED;
    }

    // P130: v živé partii se kouč útočníka ptá PO KAŽDÉM hodu kostek, ještě
    // před výběrem kostky a před push/follow -- co je u bloku "neúspěch", ví
    // jen on (uživatel 10.10.: chce třeba jiný výsledek kvůli vytlačení do davu).
    // Přehoz hází VŠECHNY kostky znovu (l. 920).
    const bool coachAttacker = coachDecides(att.teamSide);
    auto packFaces = [&]() {
        int packed = 0;
        for (int i = 0; i < diceInfo.count; i++) packed |= (static_cast<int>(faces[i]) + 1) << (3 * i);
        return packed;
    };
    if (coachAttacker) {
        const int before = packFaces();
        bool viaPro = false;
        if (coachAskReroll(state, att, dice, CoachQuestion::BLOCK_REROLL, def.id, before,
                           diceInfo.count, /*teamAllowed=*/true, events, viaPro)) {
            for (int i = 0; i < diceInfo.count; i++) faces[i] = dice.rollBlockDie();
            emitEvent(events, {GameEvent::Type::REROLL, att.id, def.id, {}, {}, 0,
                              true, before, packFaces()});
        }
        chosen = autoChooseBlockDie(faces, diceInfo.count, diceInfo.attackerChooses, att, def);
        // Výběr kostky: kouč vidí hozené kostky a vybere (jen když vybírá útočník
        // a je z čeho). Starší příkaz `kostka` (g_manualFace) má přednost.
        bool differ = false;
        for (int i = 1; i < diceInfo.count; i++) differ = differ || faces[i] != faces[0];
        if (diceInfo.attackerChooses && differ && g_manualFace < 0) {
            int opts = 0;
            for (int i = 0; i < diceInfo.count; i++) opts |= 1 << static_cast<int>(faces[i]);
            chosen = static_cast<BlockDiceFace>(
                askCoach(CoachQuestion::BLOCK_FACE, att.id, def.id, packFaces(), diceInfo.count, opts));
        }
        if (chosen == BlockDiceFace::BOTH_DOWN && params.isBlitz &&
            att.hasSkill(SkillName::Juggernaut)) {
            chosen = BlockDiceFace::PUSHED;
        }
    }

    // Pro/team reroll on bad result
    if (!coachAttacker && shouldRerollBlock(chosen, att)) {
        bool rerolled = false;

        // Pro reroll
        // ⛔ OPRAVA 21.09.2026: `rules_bb2016.txt` r. 8385-8387 -- "On a roll of
        //   1, 2 or 3 the original result stands and **may not be re-rolled with
        //   a skill or team re-roll**; however you can re-roll the Pro roll with
        //   a Team re-roll."
        //   Do ted po neuspesnem hodu Pro propadl kod nize do tymoveho prehozu
        //   a prehodil KOSTKY BLOKU -- tedy presne to, co pravidla zakazuji.
        //   PHP vrstva to ma spravne (`ProCheck`), odtud ten rozdil.
        bool proAttempted = false;
        if (!rerolled && att.hasSkill(SkillName::Pro) && !att.proUsedThisTurn) {
            att.proUsedThisTurn = true;
            proAttempted = true;
            int proRoll = dice.rollD6();

            // Tymovy prehoz smi prehodit jen HOD PRO, ne kostky.
            if (proRoll < 4) {
                TeamState& team = state.getTeamState(att.teamSide);
                if (team.canUseReroll()) {
                    spendTeamRerollByEngine(team, att.id, events);

                    bool canReroll = true;
                    if (att.hasSkill(SkillName::Loner)) {
                        int lonerRoll = dice.rollD6();
                        if (lonerRoll < 4) canReroll = false;
                    }
                    if (canReroll) proRoll = dice.rollD6();
                }
            }

            if (proRoll >= 4) {
                for (int i = 0; i < diceInfo.count; i++) faces[i] = dice.rollBlockDie();
                chosen = autoChooseBlockDie(faces, diceInfo.count,
                                             diceInfo.attackerChooses, att, def);
                if (chosen == BlockDiceFace::BOTH_DOWN && params.isBlitz &&
                    att.hasSkill(SkillName::Juggernaut)) {
                    chosen = BlockDiceFace::PUSHED;
                }
                rerolled = true;
            }
        }

        // Team reroll -- jen kdyz se Pro vubec nezkouselo (r. 8386: po neuspesnem
        //   Pro uz kostky prehodit nelze ani tymovym prehozem)
        if (!rerolled && !proAttempted && shouldRerollBlock(chosen, att)) {
            TeamState& team = state.getTeamState(att.teamSide);
            if (team.canUseReroll()) {
                spendTeamRerollByEngine(team, att.id, events);

                bool canReroll = true;
                if (att.hasSkill(SkillName::Loner)) {
                    int lonerRoll = dice.rollD6();
                    if (lonerRoll < 4) canReroll = false;
                }

                if (canReroll) {
                    for (int i = 0; i < diceInfo.count; i++) faces[i] = dice.rollBlockDie();
                    chosen = autoChooseBlockDie(faces, diceInfo.count,
                                                 diceInfo.attackerChooses, att, def);
                    if (chosen == BlockDiceFace::BOTH_DOWN && params.isBlitz &&
                        att.hasSkill(SkillName::Juggernaut)) {
                        chosen = BlockDiceFace::PUSHED;
                    }
                }
            }
        }
    }

    // P130 (uživatel 12.10.2026: "ať je to podle pravidel"): když je obránce silnější,
    // kostku vybírá JEHO kouč (l. 566-570) -- v živé partii se ho engine zeptá.
    if (!diceInfo.attackerChooses && coachDecides(def.teamSide)) {
        bool differ = false;
        int opts = 0;
        for (int i = 0; i < diceInfo.count; i++) {
            differ = differ || faces[i] != faces[0];
            opts |= 1 << static_cast<int>(faces[i]);
        }
        if (differ) {
            chosen = static_cast<BlockDiceFace>(
                askCoach(CoachQuestion::BLOCK_FACE, def.id, att.id, packFaces(), diceInfo.count, opts));
        }
    }

    if (g_manualFace >= 0 && diceInfo.attackerChooses) {   // kouč vybral kostku (jen z hozených)
        for (int i = 0; i < diceInfo.count; i++)
            if (static_cast<int>(faces[i]) == g_manualFace) { chosen = faces[i]; break; }
        g_manualFace = -1;
    }
    ++g_blocksThrown;
    // 02.10.2026 (uživatel: „zkus do logu doplnit všechny kostky hodu"): die1 nese VŠECHNY hozené
    // kostky (po případném přehozu), zakódované po 3 bitech jako (face+1); die2 = jejich počet.
    int packedFaces = 0;
    for (int i = 0; i < diceInfo.count; i++) packedFaces |= (static_cast<int>(faces[i]) + 1) << (3 * i);
    emitEvent(events, {GameEvent::Type::BLOCK, att.id, def.id, att.position,
                      def.position, static_cast<int>(chosen), true, packedFaces, diceInfo.count});

    // 5. Apply block result
    bool defPushed = false;
    bool defKnockedDown = false;
    bool attKnockedDown = false;
    bool turnover = false;
    Position pushDest = def.position;

    switch (chosen) {
        case BlockDiceFace::ATTACKER_DOWN: {
            att.setState(PlayerState::PRONE);
            attKnockedDown = true;
            turnover = true;
            emitEvent(events, {GameEvent::Type::KNOCKED_DOWN, att.id, -1,
                              att.position, {}, 0, false});
            break;
        }

        case BlockDiceFace::BOTH_DOWN: {
            // Check Wrestle. Juggernaut cancels the DEFENDER's Wrestle on a
            // Blitz (rules parity, 2026-08-10) -- CRP Juggernaut: "the
            // opposing player may not use his Fend, Stand Firm or Wrestle
            // skills against the Juggernaut player's blocks." Stand Firm was
            // already handled (resolvePushback), Fend and Wrestle were not.
            // ⛔ VOLBA, NE MECHANIKA (24.08.2026). BB2016 l. 8672-8676: "This
            // player MAY USE Wrestle ... Both players are Placed Prone in their
            // respective squares EVEN IF ONE OR BOTH HAVE THE BLOCK SKILL."
            // Block tedy Wrestle NEPREBIJI -- rozhoduje trenér. Do 24.08. byla
            // volba zadratovana, a to v OPACNYCH smerech: utocnik s Blockem
            // Wrestle nepouzil NIKDY (`&& !att.hasSkill(Block)`), obrance
            // naopak VZDY. Kombinace Block+Wrestle na jednom hráči tim byla
            // v utoku mrtva -- prave ta, kvuli ktere se o Wrestle u trpaslika
            // uvazuje (Longbeard).
            const bool attHasBall = state.ball.isHeld && state.ball.carrierId == att.id;
            const bool defHasBall = state.ball.isHeld && state.ball.carrierId == def.id;

            // Utocnik: bez Blocku ho Both Down slozi tak jako tak, takze Wrestle
            // je lepsi vzdy (slozi i souperě a nikdo nehazi na zbroj). S Blockem
            // by zustal stat, takze Wrestle stoji za to jen kdyz je co ziskat:
            // souper ma Block (jinak by se NESTALO NIC), nebo drzi mic.
            const bool attWantsWrestle =
                att.hasSkill(SkillName::Wrestle) &&
                (!att.hasSkill(SkillName::Block) ||
                 (!attHasBall && (def.hasSkill(SkillName::Block) || defHasBall)));

            // Obrance (ROZHODL UZIVATEL 16.10.2026, audit parity 08.10. bod (a)): „pokud ma obrance
            // wrestle a utocnik block — tak obrance pouzije“; „pokud nosic block nema a obrance ma —
            // wrestle nevoli“. Utocnik s Blockem by Both Down ustal, takze Wrestle je jediny zpusob,
            // jak ho polozit. Bez Blocku utocnik pada sam a obrance s Blockem stoji -- Wrestle by ho
            // jen polozil taky. Obrance bez Blocku pada tak jako tak => Wrestle (bez hodu na zbroj).
            // Drive: s Blockem jen proti nosici (i bez Blocku nosice) -- a ne proti utocniku s Blockem.
            const bool defWantsWrestle =
                def.hasSkill(SkillName::Wrestle) &&
                (!def.hasSkill(SkillName::Block) || att.hasSkill(SkillName::Block));

            // Juggernaut na blitzu rusi Wrestle OBRANCE (rules parity 10.08.):
            // "the opposing player may not use his Fend, Stand Firm or Wrestle
            // skills against the Juggernaut player's blocks."
            const bool defWrestleAllowed = def.hasSkill(SkillName::Wrestle) &&
                              !(params.isBlitz && att.hasSkill(SkillName::Juggernaut));
            bool defWrestle = defWantsWrestle && defWrestleAllowed;
            // P130: Wrestle obránce je v živé partii volba jeho kouče.
            if (defWrestleAllowed && coachDecides(def.teamSide)) {
                defWrestle = askCoach(CoachQuestion::USE_WRESTLE, def.id, att.id, 0, 0, 3) == 1;
            }
            bool attWrestle = attWantsWrestle;
            // P201 (5), 14.10.2026: i Wrestle útočníka je v živé partii volba jeho kouče
            // (ř. 8672-8676 "MAY USE"); dřív rozhodl engine. Ptá se jen, když Wrestle
            // obránce už oba nepoložil.
            if (!defWrestle && att.hasSkill(SkillName::Wrestle) && coachDecides(att.teamSide)) {
                attWrestle = askCoach(CoachQuestion::USE_WRESTLE, att.id, def.id, 0, 0, 3) == 1;
            }

            if (defWrestle || attWrestle) {
                // F11, 24.08.2026: Wrestle NENI bezpodminecne bez turnoveru.
                // BB2016 l. 8677-8678: "Use of this skill does not cause
                // a turnover UNLESS THE ACTIVE PLAYER WAS HOLDING THE BALL",
                // a katalog turnoveru l. 368-372 to rika stejne: "being Placed
                // Prone is not a turnover unless it is a player from the active
                // team holding the ball ... e.g. skills like Diving Tackle,
                // Piling On and Wrestle count as being Placed Prone".
                // Cte se PRED padem, protoze handleBallOnPlayerDown mic upusti.
                const bool activeHeldBall =
                    state.ball.isHeld && state.ball.carrierId == att.id;

                // Wrestle: both prone, no armor
                att.setState(PlayerState::PRONE);
                def.setState(PlayerState::PRONE);
                emitEvent(events, {GameEvent::Type::SKILL_USED,
                                  defWrestle ? def.id : att.id, -1, {}, {},
                                  static_cast<int>(SkillName::Wrestle), true});
                // Handle ball on either player going down
                handleBallOnPlayerDown(state, att.id, dice, events);
                handleBallOnPlayerDown(state, def.id, dice, events);
                att.hasActed = true;
                return activeHeldBall ? ActionResult::turnovr() : ActionResult::ok();
            }

            // No Wrestle — check Block
            bool attFalls = !att.hasSkill(SkillName::Block);
            bool defFalls = !def.hasSkill(SkillName::Block);

            if (attFalls) {
                att.setState(PlayerState::PRONE);
                attKnockedDown = true;
                turnover = true;
                emitEvent(events, {GameEvent::Type::KNOCKED_DOWN, att.id, -1,
                                  att.position, {}, 0, false});
            }
            // ⛔⛔ N8 (24.08.2026): tady stalo `defPushed = true;` s komentarem
            // "defender gets pushed then knocked down" -- to je ale chovani
            // DEFENDER DOWN, ne BOTH DOWN.
            // BB2016 l. 514-519: "BOTH DOWN: **Both players are Knocked Down**,
            // unless one or both of the players involved has the Block skill."
            // Zadny odsun, zadny follow-up. Srovnej l. 530-533 (Defender Down):
            // "The defending player is **pushed back and then Knocked Down in
            // the square they are moved to**. The attacking player may follow up."
            // ⇒ Kazdy Both Down proti obranci bez Blocku posouval DVA hráče
            // o pole, kam pravidla nedovoluji -- a `scoreFace` tuhle tvar ceni
            // 8/10 pro utocnika s Blockem, takze se VOLI casto. Deformovalo to
            // klece, screeny a vsechna dosavadni pozicni mereni.
            if (defFalls) {
                def.setState(PlayerState::PRONE);
                emitEvent(events, {GameEvent::Type::KNOCKED_DOWN, def.id, -1,
                                  def.position, {}, 0, false});
                noteKnockdownOnStoodUp(def);   // Q3
                InjuryContext defCtx;
                if (att.hasSkill(SkillName::MightyBlow)) defCtx.mightyBlow = true;
                if (att.hasSkill(SkillName::Claw)) defCtx.hasClaw = true;
                resolveArmourAndInjury(state, def.id, dice, defCtx, events);
                handleBallOnPlayerDown(state, def.id, dice, events);
            }
            if (attKnockedDown) {
                InjuryContext attCtx;
                attCtx.chainsaw = def.hasSkill(SkillName::Chainsaw);   // P75: srazil se o nositele pily
                resolveArmourAndInjury(state, att.id, dice, attCtx, events);
                handleBallOnPlayerDown(state, att.id, dice, events);
            }
            // 15.10.2026: dřív `att.hasActed = true` vždy — útočník s Blockem, který Both Down
            // ustál, v blitzu nesměl dál běžet (ř. 347-350). Zavírá jediné místo (endBlockActivation):
            // stojí-li a je to blitz, aktivace zůstává otevřená; ležícímu se zavře.
            endBlockActivation(att, params);
            return turnover ? ActionResult::turnovr() : ActionResult::ok();
        }

        case BlockDiceFace::PUSHED: {
            defPushed = true;
            break;
        }

        case BlockDiceFace::DEFENDER_STUMBLES: {
            // Dodge saves: DS → PUSHED (unless Tackle)
            // Tackle, l. 8569-8571: "...nor may they use their Dodge skill if
            // the player throws a block at them and uses the Tackle skill."
            // P130: Dodge je volba obránce (l. 985, 1820) -- bez něj je odtlačen
            // A PAK sražen (l. 524-527), takže Frenzy nedostane druhý blok.
            bool useDodge = def.hasSkill(SkillName::Dodge) &&
                            !(att.hasSkill(SkillName::Tackle) && !g_manualNoTackle);
            if (useDodge && coachDecides(def.teamSide)) {
                useDodge = askCoach(CoachQuestion::USE_DODGE, def.id, att.id, 0, 0, 3) == 1;
            }
            if (useDodge) {
                defPushed = true; // just a push
            } else {
                defPushed = true;
                defKnockedDown = true;
            }
            break;
        }

        case BlockDiceFace::DEFENDER_DOWN: {
            defPushed = true;
            defKnockedDown = true;
            break;
        }
    }

    // Handle attacker knocked down (from AD)
    if (attKnockedDown && chosen == BlockDiceFace::ATTACKER_DOWN) {
        InjuryContext attCtx;
        attCtx.chainsaw = def.hasSkill(SkillName::Chainsaw);   // P75: srazil se o nositele pily
        resolveArmourAndInjury(state, att.id, dice, attCtx, events);
        handleBallOnPlayerDown(state, att.id, dice, events);
        att.hasActed = true;
        return ActionResult::turnovr();
    }

    // Fend zakazuje follow-up i sraženému a i do davu (ř. 8115); Juggernaut ho v blitzu ruší.
    const bool fendPrevents = def.hasSkill(SkillName::Fend) &&
                              !(params.isBlitz && att.hasSkill(SkillName::Juggernaut));
    // Pushback resolution
    if (defPushed) {
        // P225: past na Frenzy jen tam, kde by se druhý blok se Stand Firm opravdu hodil:
        // obránce stojí, není to druhý blok ani Multiple Block a v blitzu ho útočník zaplatí.
        const bool frenzyTrapPossible = !frenzySecondBlock && !frenzyDisabled && !defKnockedDown &&
            att.hasSkill(SkillName::Frenzy) && (!params.isBlitz || blitzBlockPayable(att));
        bool crowdSurf = resolvePushback(state, att, def, params.isBlitz, frenzyTrapPossible,
                                          !noFollowUp && !fendPrevents, dice, pushDest, events);

        if (crowdSurf) {
            // Crowd surf. A surfed BALL CARRIER does not just drop the ball
            // at the touchline (rules parity, 2026-08-10). CRP: "If the
            // player who is holding the ball is pushed out of bounds, then
            // he is beaten up by the fans, who are more than happy to throw
            // the ball back into play! The Throw-in template is centred on
            // the last square the player was in before he was pushed off
            // the pitch." We used to call handleBallOnPlayerDown, leaving
            // the ball a single bounce from the edge -- so a surf won us the
            // removal AND the ball, and was badly overpaid.
            Position lastPos = def.position;
            // OPRAVENO 13.10.2026 (P194/6) -- follow-up se tu řešil AŽ PO vhazování
            // a hodu na zranění v davu. Pravidla ř. 609-610: "The player's coach
            // must decide whether to follow up BEFORE ANY OTHER DICE ROLLS are made."
            // OPRAVENO 14.10.2026 (review) -- obránce je v davu UŽ PŘED follow-upem: dřív
            // stál při vhazování na hřišti (mohl chytit vlastní vhazovaný míč) a směr
            // vhazování se počítal z útočníkova NOVÉHO pole (tj. z pole obránce ⇒ (0,0)).
            // Fend zakazuje follow-up i tady (ř. 8115), Juggernaut na blitzu ho ruší.
            const Position exitAt = pushOffPitchExit(att.position, lastPos);
            const bool defHadBall = state.ball.isHeld && state.ball.carrierId == def.id;
            if (!defHadBall) handleBallOnPlayerDown(state, def.id, dice, events);
            def.position = {-1, -1};
            if (!noFollowUp && !fendPrevents) followUp(state, att, def, params, defOldPos, dice, events);
            if (defHadBall) {
                state.ball = BallState::onGround(lastPos);
                resolveThrowIn(state, lastPos, exitAt, dice, events);
            }
            resolveCrowdSurf(state, def.id, lastPos, dice, events);

            // StripBall: ball drops at crowd edge (already handled by crowd surf)

            endBlockActivation(att, params);

            // Handle BD attacker knockdown
            if (attKnockedDown) {
                InjuryContext attCtx;
                attCtx.chainsaw = def.hasSkill(SkillName::Chainsaw);   // P75: srazil se o nositele pily
                resolveArmourAndInjury(state, att.id, dice, attCtx, events);
                handleBallOnPlayerDown(state, att.id, dice, events);
            }
            return turnover ? ActionResult::turnovr() : ActionResult::ok();
        }

        // Follow-up: attacker moves to old defender position.
        // Fend forbids the follow-up EVEN IF the Fend player goes down
        // (rules parity, 2026-08-10). CRP Fend: "Opposing players may not
        // follow-up blocks made against this player even if the Fend player
        // is Knocked Down." Our `!defKnockedDown` guard switched Fend off in
        // exactly the results where a follow-up is usually at stake, leaving
        // it working only on a clean Pushed. Juggernaut cancels it on a
        // Blitz: "the opposing player may not use his Fend, Stand Firm or
        // Wrestle skills against the Juggernaut player's blocks."
        // (fendPrevents výš, před odtlačením -- čte ho i P225.)
        // A follow-up needs a square that was actually vacated. Stand Firm (and
        // a chain that jams against one) leaves the defender where he stood, and
        // we used to walk the attacker onto him -- two players on one square.
        bool defVacated = def.position != defOldPos;
        if (!noFollowUp && !fendPrevents && defVacated) {
            followUp(state, att, def, params, defOldPos, dice, events);
        }

        // OPRAVENO 13.10.2026 (P194/6) -- vyražení míče Strip Ballem stálo PŘED
        // follow-upem, takže se míč odrážel dřív, než kouč rozhodl (ř. 609-610).
        // Strip Ball, l. 8517-8521: "When a player with this skill blocks an
        // opponent with the ball, applying a 'Pushed' or 'Defender Stumbles'
        // result will cause the opposing player to DROP THE BALL IN THE SQUARE
        // THAT THEY ARE PUSHED TO, EVEN IF the opposing player is NOT KNOCKED
        // DOWN." Sražený nosič mic pusti i bez Strip Ballu, takze zdejsi vetev
        // resi prave to "i kdyz nespadl". Sure Hands ho rusi (l. 8546-8548).
        // Mic padá z def.position, coz uz JE cilove pole odsunu (odsun probehl
        // vys v resolvePushback).
        if (!defKnockedDown && state.ball.isHeld && state.ball.carrierId == def.id &&
            att.hasSkill(SkillName::StripBall)) {
            if (def.hasSkill(SkillName::SureHands)) {
                // Sure Hands negates Strip Ball (BB2016/LRB6) — ball stays with carrier.
                emitEvent(events, {GameEvent::Type::SKILL_USED, def.id, att.id,
                                  def.position, {},
                                  static_cast<int>(SkillName::SureHands), true});
            } else {
                state.ball = BallState::onGround(def.position);
                resolveBounce(state, def.position, dice, 0, events);
            }
        }

        // Knockdown
        if (defKnockedDown) {
            def.setState(PlayerState::PRONE);
            emitEvent(events, {GameEvent::Type::KNOCKED_DOWN, def.id, -1,
                              def.position, {}, 0, false});
            noteKnockdownOnStoodUp(def);   // Q3

            InjuryContext defCtx;
            // One flag, not two modifiers: resolveArmourAndInjury decides which
            // roll it lands on, because CRP spends Mighty Blow on one of them.
            if (att.hasSkill(SkillName::MightyBlow)) defCtx.mightyBlow = true;
            if (att.hasSkill(SkillName::Claw)) defCtx.hasClaw = true;
            if (att.hasSkill(SkillName::Stakes)) defCtx.hasStakes = true;

            resolveArmourAndInjury(state, def.id, dice, defCtx, events);
            handleBallOnPlayerDown(state, def.id, dice, events);
        }
    }

    // Handle BD attacker knockdown (if both down and attacker falls)
    if (attKnockedDown && chosen == BlockDiceFace::BOTH_DOWN) {
        InjuryContext attCtx;
        attCtx.chainsaw = def.hasSkill(SkillName::Chainsaw);   // P75: srazil se o nositele pily
        resolveArmourAndInjury(state, att.id, dice, attCtx, events);
        handleBallOnPlayerDown(state, att.id, dice, events);
    }

    // M1/N10 (25.08.2026): BB2016 l. 347-350 -- "He may make one block during
    // the move. The block may be made AT ANY POINT during the move." A Block
    // Action IS the activation and ends here; a Blitz is a move with a block
    // inside it, so it stays open while the blitzer is still on his feet.
    // Every other `hasActed = true` above is a path where the attacker went
    // down or the action was wasted (Foul Appearance, chainsaw kickback, stab,
    // failed GFI) -- those close the activation in a Blitz too, which is why
    // this is the only site that changes.
    // ⚠️ The offer side is a SEPARATE half: macro_actions.cpp:584 does not emit
    // REPOSITION for a player adjacent to a standing opponent, i.e. exactly the
    // player who just blocked. Reopening the activation without that gives him
    // permission to move and nowhere to go.
    endBlockActivation(att, params);

    // Frenzy: if both standing and adjacent after block, mandatory 2nd block.
    // ⛔ JEN PO PUSHED / DEFENDER STUMBLES (oprava 21.08.). BB2016 l. 8139-8141:
    // "If a 'Pushed' or 'Defender Stumbles' result WAS CHOSEN, the player must
    // immediately throw a second block." Dosud se druhý blok házel po každém
    // výsledku, kde oba zůstali stát a sousedili -- tedy i po BOTH_DOWN, kdy
    // oba mají Block a nikdo nepadne. To je blok navíc zadarmo, a v TV1200 to
    // má jen trpaslík (2 Troll Slayeři), takže to hrálo PRO nás.
    const bool frenzyTrigger = (chosen == BlockDiceFace::PUSHED ||
                                chosen == BlockDiceFace::DEFENDER_STUMBLES);
    // L6 (29.08.): uvnitr Multiple Blocku se Frenzy nezapina vubec --
    // r. 8304-8305 "both skills cannot be used together".
    if (!frenzySecondBlock && !frenzyDisabled && frenzyTrigger &&
        att.hasSkill(SkillName::Frenzy) &&
        canAct(att.state) && canAct(def.state) &&
        att.position.distanceTo(def.position) == 1) {
        // CRP: during a Blitz the second block costs movement too — with
        // no movement and no GFI left it is not thrown. Gated here (not
        // only via the charge in the recursive call) so the first block's
        // outcome is preserved unchanged when the second is impossible.
        if (params.isBlitz && !blitzBlockPayable(att)) {
            return turnover ? ActionResult::turnovr() : ActionResult::ok();
        }
        // Mandatory second block
        BlockParams secondParams = params;
        secondParams.hornsBonus = false; // no Horns on 2nd block
        att.hasActed = false; // allow second block
        ActionResult secondResult = resolveBlock(state, secondParams, dice, events, true);
        return secondResult;
    }

    return turnover ? ActionResult::turnovr() : ActionResult::ok();
}

ActionResult resolveMultipleBlock(GameState& state, int attackerId,
                                  int target1Id, int target2Id,
                                  DiceRollerBase& dice, std::vector<GameEvent>* events) {
    Player& att = state.getPlayer(attackerId);
    // P75: pila "cannot be used with ... Multiple Block" (r. 8014-8015).
    if (att.hasSkill(SkillName::Chainsaw)) return ActionResult::fail();

    // Resolve first block (defender gets +2 ST, no follow-up)
    {
        Player& def1 = state.getPlayer(target1Id);

        // FoulAppearance check
        if (def1.hasSkill(SkillName::FoulAppearance)) {
            int faRoll = dice.rollD6();
            if (faRoll == 1) {
                // Skip this block (action wasted on this target)
                emitEvent(events, {GameEvent::Type::SKILL_USED, def1.id, att.id, {}, {},
                                  static_cast<int>(SkillName::FoulAppearance), true});
                goto second_block;
            }
        }

        {
            // Temporarily add +2 ST to defender
            int8_t origST = def1.stats.strength;
            def1.stats.strength += 2;

            BlockParams params;
            params.attackerId = attackerId;
            params.targetId = target1Id;
            params.isBlitz = false;
            params.hornsBonus = false;
            ActionResult result = resolveBlock(state, params, dice, events, false, true,
                                              /*frenzyDisabled=*/true);

            def1.stats.strength = origST;

            // If attacker went down, turnover — skip 2nd block
            if (result.turnover || att.state != PlayerState::STANDING) {
                return ActionResult::turnovr();
            }
        }
    }

second_block:
    // Reset hasActed so 2nd block can proceed
    att.hasActed = false;

    // Resolve second block (same rules)
    {
        Player& def2 = state.getPlayer(target2Id);

        // FoulAppearance check
        if (def2.hasSkill(SkillName::FoulAppearance)) {
            int faRoll = dice.rollD6();
            if (faRoll == 1) {
                att.hasActed = true;
                emitEvent(events, {GameEvent::Type::SKILL_USED, def2.id, att.id, {}, {},
                                  static_cast<int>(SkillName::FoulAppearance), true});
                return ActionResult::ok();
            }
        }

        // Temporarily add +2 ST to defender
        int8_t origST = def2.stats.strength;
        def2.stats.strength += 2;

        BlockParams params;
        params.attackerId = attackerId;
        params.targetId = target2Id;
        params.isBlitz = false;
        params.hornsBonus = false;
        ActionResult result = resolveBlock(state, params, dice, events, false, true,
                                              /*frenzyDisabled=*/true);

        def2.stats.strength = origST;

        return result;
    }
}

} // namespace bb
