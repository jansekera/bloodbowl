#include "bb/kickoff_handler.h"
#include "bb/ball_handler.h"
#include "bb/helpers.h"
#include "bb/injury.h"
#include "bb/turn_handler.h"
#include <algorithm>
#include <cmath>

// SJEDNOCENO 02.10.2026 (F13): do té doby existovaly dvě výkopové cesty — `simpleKickoff`
// (game_simulator.cpp, bez tabulky a bez Kick-Off Return, na ní běžely VŠECHNY hry) a tahle
// (`useFullKickoff`, volaly ji jen testy). Rozdíly a co se s nimi stalo:
// evidence/kickoff_sjednoceni_20261002.md. FAME, roztleskávačky a asistenty engine nemodeluje ⇒ 0.

namespace bb {

namespace {

bool inHalfOf(TeamSide side, Position p) {
    return side == TeamSide::HOME ? p.x <= 12 : p.x >= 13;
}

int losXOf(TeamSide side) { return side == TeamSide::HOME ? 12 : 13; }

// D3 = D6 rozpůlená nahoru (1-2 → 1, 3-4 → 2, 5-6 → 3).
int rollD3(DiceRollerBase& dice) { return (dice.rollD6() + 1) / 2; }

// Rovnoměrný náhodný index 0..n-1 (ř. 1250-1257 „select a random player“ — žetony z hrnku).
// Dva D6 = 36 stejně pravděpodobných hodnot, přebytek nad násobkem n se hází znovu.
// OPRAVENO 02.10. — dřív `rollD6() % n`, což pro n = 11 bralo jen prvních šest hráčů.
int randomIndex(DiceRollerBase& dice, int n) {
    if (n <= 1) return 0;
    const int limit = 36 - 36 % n;
    for (;;) {
        const int v = (dice.rollD6() - 1) * 6 + (dice.rollD6() - 1);
        if (v < limit) return v % n;
    }
}

// Touchback, ř. 281-283: „the receiving coach is awarded a 'touchback' and must give the ball to
// ANY PLAYER IN HIS TEAM“. Kterému, to je volba trenéra: dáváme ho nejhlubšímu stojícímu hráči
// (přednostně se Sure Hands), tedy tomu, kdo je nejdál od LoS a nejméně ohrožený. No Hands míč
// nést nesmí (ř. 8318-8320). Převzato ze `simpleKickoff` — plná cesta dřív dávala míč nejbližšímu
// k místu dopadu, i hráči bez rukou.
void awardTouchback(GameState& state, TeamSide receiving, DiceRollerBase& dice,
                    std::vector<GameEvent>* events) {
    const int kickLos = losXOf(opponent(receiving));
    Player* pick = nullptr;
    int bestScore = -1;
    state.forEachOnPitch(receiving, [&](const Player& p) {
        if (p.state != PlayerState::STANDING || p.hasSkill(SkillName::NoHands)) return;
        const int score = std::abs(p.position.x - kickLos) + (p.hasSkill(SkillName::SureHands) ? 100 : 0);
        if (score > bestScore) { bestScore = score; pick = &state.getPlayer(p.id); }
    });
    if (pick) {
        state.ball = BallState::carried(pick->position, pick->id);
    } else {
        // Nikdo stojící s rukama: míč na zem doprostřed přijímající poloviny (krajní případ). Leží-li
        // tam hráč, míč se odrazí (ř. 857-858). OPRAVENO 02.10. (review #9) — zůstal ležet pod ním.
        const Position mid{static_cast<int8_t>(receiving == TeamSide::HOME ? 6 : 19), 7};
        state.ball = BallState::onGround(mid);
        if (state.getPlayerAtPosition(mid)) resolveBounce(state, mid, dice, 0, events);
    }
}

// Dopad, ř. 274-283: dopad mimo hřiště nebo do kopající poloviny = touchback; na prázdné pole ⇒
// odraz o jedno pole; na hráče ⇒ ten musí chytat (kterýkoli tým; ležící chytat nesmí, ř. 857-858
// ⇒ odraz); i odraz ven nebo do kopající poloviny je touchback (táž věta „scatters or bounces“).
// OPRAVENO 02.10. — plná cesta neměla odraz na prázdné pole a výkop mimo hřiště ořízla na okraj
// (P123); zjednodušená po odrazu ven nejdřív vhazovala z davu. Míč, který dopadl na kopajícího
// nebo ležícího, zůstal v obou cestách ležet pod ním.
void landBall(GameState& state, TeamSide receiving, Position at, DiceRollerBase& dice,
              std::vector<GameEvent>* events) {
    bool bounced = false;
    for (int guard = 0; guard < 200; ++guard) {   // jen pojistka proti nekonečnému řetězu
        if (!at.isOnPitch() || !inHalfOf(receiving, at)) {
            awardTouchback(state, receiving, dice, events);
            return;
        }
        Player* p = state.getPlayerAtPosition(at);
        // Míč na zem DŘÍV, než ho kdo chytá — resolveCatch při neúspěchu míčem nehýbe (11.08.).
        state.ball = BallState::onGround(at);
        if (!p && bounced) return;                  // po odrazu na prázdné pole zůstane ležet
        if (p && p->state == PlayerState::STANDING && resolveCatch(state, p->id, dice, 0, events)) return;
        const int d8 = dice.rollD8();
        const Position off = scatterDirection(d8);
        const Position next{static_cast<int8_t>(at.x + off.x), static_cast<int8_t>(at.y + off.y)};
        emitEvent(events, {GameEvent::Type::BALL_BOUNCE, -1, -1, at, next, d8, true});
        at = next;
        bounced = true;
    }
}

// Kick-Off Return, BB2016 ř. 8249-8256. OPRAVENO 01.10.2026 (P122): pohyb byl až PO výkopové tabulce
// a bez kontroly lajny a tacklezón. Pravidlo: hráč přijímajícího týmu, který NENÍ na lajně (LoS) ani
// v soupeřově tacklezóně, se po rozptylu míče a PŘED hodem na tabulku posune až o 3 pole; jen jeden
// hráč na výkop; ne při touchbacku; nesmí přejít do soupeřovy poloviny. AI: použije ho hráč nejblíž míči.
void resolveKickOffReturn(GameState& state, TeamSide receiving, Position ballPos,
                          std::vector<GameEvent>* events) {
    const int losX = losXOf(receiving);
    int bestId = -1;
    int bestDist = 999;
    state.forEachOnPitch(receiving, [&](const Player& p) {
        if (p.state != PlayerState::STANDING || !p.hasSkill(SkillName::KickOffReturn)) return;
        if (p.position.x == losX) return;                                   // na lajně ne
        if (countTacklezones(state, p.position, receiving) > 0) return;     // v soupeřově zóně ne
        const int d = p.position.distanceTo(ballPos);
        if (d < bestDist) { bestDist = d; bestId = p.id; }
    });
    if (bestId < 0) return;

    // Pohyb během výkopu bez uhýbání (uživatel 02.10., review #7): z pole v soupeřově tacklezóně se
    // NEODCHÁZÍ (vstoupit smí, pak stojí) a ze stejně dobrých cílů má přednost pole mimo tacklezóny.
    // OPRAVENO 02.10. — dřív šel nenasytně k míči přes jakákoli pole, i z tacklezóny do tacklezóny.
    // Prohledání do šířky (nejvýš 3 kroky): cíl = nejblíž míči, pak mimo TZ, pak méně kroků.
    Player& p = state.getPlayer(bestId);
    const Position from = p.position;
    auto inTz = [&](Position q) { return countTacklezones(state, q, receiving) > 0; };
    bool seen[Position::PITCH_WIDTH][Position::PITCH_HEIGHT] = {};
    seen[from.x][from.y] = true;
    std::vector<Position> frontier{from};
    Position best = from;
    auto key = [&](Position q) { return q.distanceTo(ballPos) * 2 + (inTz(q) ? 1 : 0); };
    int bestKey = key(from);
    for (int step = 0; step < 3; ++step) {
        std::vector<Position> next;
        for (const Position& at : frontier) {
            if (at != from && inTz(at)) continue;                 // z tacklezóny dál nejde
            for (const Position& q : at.getAdjacent()) {
                if (!q.isOnPitch() || !inHalfOf(receiving, q) || seen[q.x][q.y] ||
                    state.getPlayerAtPosition(q) != nullptr) continue;
                seen[q.x][q.y] = true;
                next.push_back(q);
                if (const int k = key(q); k < bestKey) { bestKey = k; best = q; }
            }
        }
        frontier = std::move(next);
    }
    p.position = best;
    if (p.position != from) {
        emitEvent(events, {GameEvent::Type::SKILL_USED, bestId, -1, from, p.position,
                           static_cast<int>(SkillName::KickOffReturn), true});
    }
}

// Riot, ř. 1284-1296. OPRAVENO 02.10. (Ž3) — dřív se hýbala jen značka přijímajícího, bez D6.
// `turnNumber` přijímajícího je po výkopu číslo kola, které právě začíná ⇒ jeho značka (odehraná
// kola) je o jedna menší. Kopající má v `turnNumber` odehraná kola.
void resolveRiot(GameState& state, TeamSide receiving, DiceRollerBase& dice) {
    TeamState& recv = state.getTeamState(receiving);
    TeamState& kick = state.getTeamState(opponent(receiving));
    const int recvMarker = recv.turnNumber - 1;
    int shift;
    if (recvMarker == 7) shift = -1;            // „on turn 7 … both teams move their turn marker back“
    else if (recvMarker == 0) shift = +1;       // „not yet taken a turn … moved forward one space“
    else shift = (dice.rollD6() <= 3) ? +1 : -1; // „On a 1-3 … forward … On a 4-6 … back“
    recv.turnNumber += shift;
    kick.turnNumber = std::max(0, kick.turnNumber + shift);
}

// Cheering Fans (ř. 1309-1314) a Brilliant Coaching (ř. 1321-1326): D3 + FAME + roztleskávačky /
// asistenti (vše 0), vyšší dostane přehoz, při remíze OBA. OPRAVENO 02.10. (Ž4, P90) — dřív D6 a
// při remíze nikdo.
void resolveRerollContest(GameState& state, DiceRollerBase& dice) {
    const int home = rollD3(dice);
    const int away = rollD3(dice);
    if (home >= away) state.homeTeam.rerolls++;
    if (away >= home) state.awayTeam.rerolls++;
}

// High Kick, ř. 1302-1308: „Any one player on the receiving team who is not in an opposing player's
// tackle zone may be moved into the square where the ball will land … as long as the square is
// unoccupied.“ OPRAVENO 02.10. (Ž5) — dřív nejbližší stojící bez ohledu na tacklezóny.
// Volba AI: nejlepší chytač (Catch, pak AG), pak nejbližší; No Hands ne.
void resolveHighKick(GameState& state, TeamSide receiving, Position landing) {
    if (!landing.isOnPitch() || !inHalfOf(receiving, landing) || state.getPlayerAtPosition(landing)) return;
    int bestId = -1;
    int bestScore = -1;
    state.forEachOnPitch(receiving, [&](const Player& p) {
        if (p.state != PlayerState::STANDING || p.hasSkill(SkillName::NoHands)) return;
        if (countTacklezones(state, p.position, receiving) > 0) return;
        const int score = (p.hasSkill(SkillName::Catch) ? 1000 : 0) + p.stats.agility * 100 +
                          (99 - p.position.distanceTo(landing));
        if (score > bestScore) { bestScore = score; bestId = p.id; }
    });
    if (bestId >= 0) state.getPlayer(bestId).position = landing;
}

// Quick Snap!, ř. 1327-1333: „All of the players on the receiving team are allowed to move one
// square. This is a free move and may be made into any adjacent empty square, ignoring tackle zones.
// It may be used to enter the opposing half of the pitch.“ OPRAVENO 02.10. (Ž6) — dřív se všichni
// šoupli ke středu VLASTNÍ lajny. Volba AI: hráči mimo LoS udělají krok k místu dopadu (nejbližší
// napřed, ať si nepřekážejí); lajna drží.
void resolveQuickSnap(GameState& state, TeamSide receiving, Position landing) {
    const int losX = losXOf(receiving);
    std::vector<int> ids;
    state.forEachOnPitch(receiving, [&](const Player& p) {
        if (p.state == PlayerState::STANDING && p.position.x != losX) ids.push_back(p.id);
    });
    std::stable_sort(ids.begin(), ids.end(), [&](int a, int b) {
        return state.getPlayer(a).position.distanceTo(landing) < state.getPlayer(b).position.distanceTo(landing);
    });
    for (int id : ids) {
        Player& p = state.getPlayer(id);
        Position best = p.position;
        int bestD = p.position.distanceTo(landing);
        for (const Position& q : p.position.getAdjacent()) {
            if (!q.isOnPitch() || state.getPlayerAtPosition(q)) continue;
            const int d = q.distanceTo(landing);
            if (d < bestD) { bestD = d; best = q; }
        }
        p.position = best;
    }
}

// Blitz!, ř. 1334-1341: „The kicking team receives a free 'bonus' turn: however, players that are in
// an enemy tackle zone at the beginning of this free turn may not perform an Action. The kicking team
// may use team re-rolls during a Blitz. If any player suffers a turnover then the bonus turn ends.“
// OPRAVENO 02.10. (Ž1) — dřív se kopající jen šoupli o pole k LoS. Kolo hraje politika jako každé
// jiné; tady se jen otevře. Konec kola: `resolveEndTurn` → `executeAction` → `resolveKickoffLanding`.
void openBlitzTurn(GameState& state, TeamSide kicking, Position landing) {
    state.kickoffBallInAir = true;
    state.kickoffLanding = landing;
    state.ball = BallState::offPitch();
    state.activeTeam = kicking;
    state.getTeamState(kicking).resetForNewTurn();
    state.resetPlayersForNewTurn(kicking);
    state.forEachOnPitch(kicking, [&](const Player& p) {
        if (countTacklezones(state, p.position, kicking) > 0) state.getPlayer(p.id).hasActed = true;
    });
}

// Throw a Rock, ř. 1342-1350: D6 + FAME každý; fanoušci vyššího hodí na SOUPEŘE, remíza ⇒ na oba;
// náhodný hráč na hřišti; hod na zranění bez brnění. OPRAVENO 02.10. (Ž2) — dřív stun náhodného
// stojícího v KAŽDÉM týmu, bez souboje a bez hodu na zranění.
void resolveThrowARock(GameState& state, DiceRollerBase& dice, std::vector<GameEvent>* events) {
    const int home = dice.rollD6();
    const int away = dice.rollD6();
    std::vector<TeamSide> hit;
    if (home >= away) hit.push_back(TeamSide::AWAY);   // domácí fanoušci házeli na hosty
    if (away >= home) hit.push_back(TeamSide::HOME);
    for (TeamSide side : hit) {
        std::vector<int> onPitch;
        state.forEachOnPitch(side, [&](const Player& p) { onPitch.push_back(p.id); });
        if (onPitch.empty()) continue;
        const int victim = onPitch[randomIndex(dice, static_cast<int>(onPitch.size()))];
        resolveInjuryRoll(state, victim, dice, InjuryContext{}, events);
    }
}

// Pitch Invasion, ř. 1351-1356: D6 + FAME za každého soupeřova hráče NA HŘIŠTI; 6+ ⇒ Stunned,
// Ball & Chain ⇒ KO; 1 nikdy. OPRAVENO 02.10. (Ž7) — dřív jen za stojící a Ball & Chain jako ostatní.
void resolvePitchInvasion(GameState& state, DiceRollerBase& dice, std::vector<GameEvent>* events) {
    for (auto& p : state.players) {
        if (!p.isOnPitch()) continue;
        const int roll = dice.rollD6();
        if (roll < 6) continue;
        emitEvent(events, {GameEvent::Type::KNOCKED_DOWN, p.id, -1, p.position, {}, roll, false});
        if (p.hasSkill(SkillName::BallAndChain)) {
            p.setState(PlayerState::KO);
            p.position = {-1, -1};
        } else {
            p.setState(PlayerState::STUNNED);
            p.stunnedThisTurn = true;   // jako injury.cpp (ř. 707); review #8 — dřív chyběl
        }
    }
}

// Výkopová tabulka, ř. 1265-1356. `landing` je místo, kam míč dopadne (Changing Weather ho může
// posunout). Vrací true, když padl Blitz! (pak dopad čeká na konec bonusového kola).
bool resolveKickoffEvent(GameState& state, KickoffEvent event, TeamSide receiving, Position& landing,
                         DiceRollerBase& dice, std::vector<GameEvent>* events) {
    switch (event) {
        case KickoffEvent::GET_THE_REF:
            // ř. 1270-1282: úplatek každému týmu. Úplatky engine nemá ⇒ zatím bez účinku.
            return false;
        case KickoffEvent::RIOT:
            resolveRiot(state, receiving, dice);
            return false;
        case KickoffEvent::PERFECT_DEFENCE:
            // ř. 1297-1301: kopající „may reorganize“ — volba; AI nepřestavuje (legální).
            return false;
        case KickoffEvent::HIGH_KICK:
            resolveHighKick(state, receiving, landing);
            return false;
        case KickoffEvent::CHEERING:
        case KickoffEvent::BRILLIANT_COACHING:
            resolveRerollContest(state, dice);
            return false;
        case KickoffEvent::CHANGING_WEATHER: {
            const int weatherRoll = dice.roll2D6();
            state.weather = weatherFromRoll(weatherRoll);
            emitEvent(events, {GameEvent::Type::WEATHER_CHANGE, -1, -1, {}, {}, weatherRoll, true});
            // ř. 1318-1320: „Nice“ ⇒ poryv rozptýlí míč před dopadem o jedno pole. OPRAVENO 02.10. (P91).
            // Jen míč, který ještě míří na hřiště do přijímající poloviny: FAQ ř. 9315-9317 „any event
            // that causes the ball to go out of bounds or over the line of scrimmage during a kick-off
            // results in a touchback“. OPRAVENO 02.10. (review #2) — poryv vracel míč zpoza autu/LoS.
            if (state.weather == Weather::NICE && landing.isOnPitch() && inHalfOf(receiving, landing)) {
                const Position off = scatterDirection(dice.rollD8());
                landing = {static_cast<int8_t>(landing.x + off.x), static_cast<int8_t>(landing.y + off.y)};
            }
            return false;
        }
        case KickoffEvent::QUICK_SNAP:
            resolveQuickSnap(state, receiving, landing);
            return false;
        case KickoffEvent::BLITZ:
            openBlitzTurn(state, opponent(receiving), landing);
            return true;
        case KickoffEvent::THROW_A_ROCK:
            resolveThrowARock(state, dice, events);
            return false;
        case KickoffEvent::PITCH_INVASION:
            resolvePitchInvasion(state, dice, events);
            return false;
    }
    return false;
}

// Kick, ř. 8205-8213: hráč nastavený mimo lajnu a křídla smí rozptyl výkopu půlit (dolů).
bool hasKickPlayer(const GameState& state, TeamSide kickingTeam) {
    bool found = false;
    state.forEachOnPitch(kickingTeam, [&](const Player& p) {
        if (p.state == PlayerState::STANDING && p.hasSkill(SkillName::Kick))
            found = true;
    });
    return found;
}

// Dopad + začátek kola přijímajících. Reset kola přijímajících až TEĎ, ne před tabulkou: hráč
// omráčený při výkopu (Pitch Invasion, Throw a Rock, Blitz!) je omráčen mimo jejich kolo, takže
// lícem nahoru se otočí na konci jejich PRVNÍHO kola (ř. 703-708). OPRAVENO 02.10. — dřív mu
// `stunnedThisTurn` zůstal a ležel o kolo déle.
void finishKickoff(GameState& state, Position landing, DiceRollerBase& dice,
                   std::vector<GameEvent>* events) {
    const TeamSide receiving = opponent(state.kickingTeam);
    state.activeTeam = receiving;
    landBall(state, receiving, landing, dice, events);
    // Po Blitz! může míč chytit kopající hráč v koncové zóně přijímajících ⇒ TD kopajících mimo
    // jejich kolo. Přijímající své kolo nezačal (resolveKickoff mu ho už připsal) ⇒ vrátit; značku
    // skórujících posune `executeAction` (ř. 997-1004, „Scoring in the opponent's turn“).
    // OPRAVENO 02.10. (review #4) — dřív kolo propadlo přijímajícímu.
    if (checkTouchdown(state) && state.getPlayer(state.ball.carrierId).teamSide == state.kickingTeam)
        state.getTeamState(receiving).turnNumber--;
    state.getTeamState(receiving).resetForNewTurn();
    state.resetPlayersForNewTurn(receiving);
    state.phase = GamePhase::PLAY;
}

} // anonymous namespace

bool receivingTeamHasATurnLeft(const GameState& state) {
    return state.getTeamState(opponent(state.kickingTeam)).turnNumber < 8;
}

void resolveKickoff(GameState& state, DiceRollerBase& dice, std::vector<GameEvent>* events) {
    KickoffScope kickoffScope(state);
    const TeamSide receiving = opponent(state.kickingTeam);
    state.activeTeam = receiving;
    state.kickoffBallInAir = false;

    // Advance to the receiving team's NEXT turn (2026-07-10 fix: do not
    // reset turnNumber here -- at a true half boundary setupHalf() already
    // zeroed both teams' turnNumber before doKickoff() runs, so ++ still
    // yields 1; after a post-TD kickoff mid-half, setupDrive() deliberately
    // PRESERVES turnNumber (676bb50), and this function used to stomp that
    // right back to 0/1, silently reviving the "every TD grants a fresh
    // 8-turn clock" bug the 676bb50 fix was meant to close. The kicking
    // team's own turnNumber is left untouched -- it's advanced by the
    // normal turn-end flow, not by kickoff.
    const bool hasATurnLeft = receivingTeamHasATurnLeft(state);
    TeamState& recvTeam = state.getTeamState(receiving);
    recvTeam.turnNumber++;

    // ř. 1033-1035: „Play stops when both coaches have had eight turns each.“ Přijímajícímu nezbylo
    // kolo ⇒ výkop se nekoná, poločas končí (smyčka hry to pozná z checkHalfOver). OPRAVENO 02.10. —
    // dřív se kopalo i házelo na tabulku (Blitz!, kámen, invaze) a teprve pak poločas skončil.
    if (!hasATurnLeft) {
        state.phase = GamePhase::PLAY;
        return;
    }

    // Kick target: short vs fast, deep vs slow/mixed
    const int kickX = (state.receiverSpeed == RosterSpeed::FAST)
                          ? ((state.kickingTeam == TeamSide::HOME) ? 18 : 7)
                          : ((state.kickingTeam == TeamSide::HOME) ? 22 : 3);
    const int kickY = 7;

    // Scatter: D6 for distance, D8 for direction
    int dist = dice.rollD6();
    // Kick skill, BB2016 l. 8211-8213: "you may choose to halve the number of
    // squares that the ball scatters on kick-off, ROUNDING ANY FRACTIONS DOWN
    // (i.e., 1 = 0, 2-3 = 1, 4-5 = 2, 6 = 3)". Do 24.08.2026 se zaokrouhlovalo
    // NAHORU, takze u tri hodu ze sesti (1, 3, 5) mic uletel o pole dal, nez ma.
    if (hasKickPlayer(state, state.kickingTeam)) dist = dist / 2;
    const Position scatter = scatterDirection(dice.rollD8());
    // P123 (OPRAVENO 02.10.): žádný `clamp` — míč mimo hřiště je touchback (landBall), ne okraj.
    Position landing{static_cast<int8_t>(kickX + scatter.x * dist), static_cast<int8_t>(kickY + scatter.y * dist)};
    // Míč letí: na zem přijde až při dopadu (ř. 1242-1248).
    state.ball = BallState::offPitch();

    emitEvent(events, {GameEvent::Type::KICKOFF, -1, -1, {}, landing, 0, true});

    // Kick-Off Return PŘED výkopovou tabulkou (P122); „may not be used for a touchback kick-off“.
    if (landing.isOnPitch() && inHalfOf(receiving, landing))
        resolveKickOffReturn(state, receiving, landing, events);

    const int kickoffRoll = dice.roll2D6();
    const KickoffEvent koEvent = kickoffEventFromRoll(std::clamp(kickoffRoll, 2, 12));
    if (resolveKickoffEvent(state, koEvent, receiving, landing, dice, events)) {
        state.phase = GamePhase::PLAY;       // Blitz!: na tahu kopající, míč ve vzduchu
        return;
    }

    // Počasí se po výkopu NEHÁZÍ (P66, 29.09.2026): platí počasí zápasu
    // (rollMatchWeather, l. 2571-2573) a mění ho jen CHANGING_WEATHER výš.
    finishKickoff(state, landing, dice, events);
}

void resolveKickoffLanding(GameState& state, DiceRollerBase& dice, std::vector<GameEvent>* events) {
    KickoffScope kickoffScope(state);      // chytání při dopadu bez týmového přehozu (ř. 1261-1263)
    state.kickoffBallInAir = false;
    finishKickoff(state, state.kickoffLanding, dice, events);
}

} // namespace bb
