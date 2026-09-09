#pragma once

#include "bb/game_state.h"
#include "bb/position.h"

namespace bb {

// (struct MoveTarget odstranen 02.09. spolu s `getValidMoveTargets` -- byl jeho jediny uzivatel)

// Can the player reach any square adjacent to target?
// If yes, returns true and sets outAdjacent to the best adjacent square.
// reserveMove: movement points to hold back from the budget (a blitz must
// keep 1 for the block itself -- CRP: the block costs 1 MP/GFI).
// Prvni krok po NEJKRATSI ceste na pole sousedici s `target`, s rezervou
// jednoho pole na blok. Tacklezony rozhoduji mezi stejne dlouhymi cestami.
// false = nikam nevede (pak se blitz nema o co pokouset).
long takeBlitzPathPicksInSearch();
int optimalPathStepsToAdjacent(const GameState& state, const Player& player,
                               Position target);
bool nextStepTowardAdjacent(const GameState& state, const Player& player,
                            Position target, Position& outStep);

// Zobecneni M14b pro OBECNY pohyb (09.09.2026): prvni krok po nejlevnejsi
// (riziko-vazene) ceste co NEJBLIZ `target` -- presne na nej, pokud je to
// v rozpoctu dosazitelne a volne, jinak na nejblizsi dosazitelne pole
// (stejny duch jako stara hladova chuze "priblizit se, i kdyz presny cil
// neni k mani", ale bez jejiho bloudeni -- BFS vybere jednu globalne
// nejlepsi bunku najednou, misto aby ji hladovy vyber hadal krok po kroku
// a osciloval). `budget` je EXPLICITNI, ne odvozeny z hrace -- volajici
// (napr. W-GFI rameno) muze chtit mensi rozpocet, nez je hracovo absolutni
// `maxGfiSquares`. `blockedSquare` se nikdy neprochazi, ani jako mezikrok
// (typicky volny mic -- `avoid` u `movePlayerToward`); {-1,-1} = zadne
// omezeni. False = zadne dosazitelne pole nezlepsi vzdalenost k cili (uz
// na miste, nebo skutecne zaseknuto).
bool nextStepToward(const GameState& state, const Player& player,
                    Position target, int budget, Position blockedSquare,
                    Position& outStep);

// 09.09.2026 (W-GFI gap oprava): delka NEJLEVNEJSI (riziko-vazene, stejny
// tiebreak jako `nextStepToward`) cesty PRESNE na `target` v ramci
// `budget` kroku. -1 = nedosazitelne. MERITKO (napr. "kolik GFI poli
// skutecne chybi"), ne krok k chuzi -- na to je `nextStepToward`.
int pathStepsToward(const GameState& state, const Player& player,
                    Position target, int budget, Position blockedSquare);

bool canReachAdjacentTo(const GameState& state, const Player& player,
                        Position target, Position& outAdjacent,
                        int reserveMove = 0);

} // namespace bb
