#pragma once

#include "bb/cage_advance.h"
#include "bb/game_state.h"
#include "bb/macro_actions.h"
#include "bb/mcts.h"
#include <utility>
#include <vector>

namespace bb {

// ⭐ JEDNA KLEC (P126, uživatel 01.–02.10.2026). Nahrazuje dvě věci, které se
// obě jmenovaly klec: makro CAGE (jen postavilo rohy kolem nosiče, nikdy s nimi
// nepohnulo) a plánovač F1 (posouval klec, ale byl vypnutý). Postup podle
// uživatele, ve třech fázích jednoho držení míče:
//
//  1. ZVEDNUTÍ — nosič zvedne míč a jde co nejdál dopředu (přednost má on),
//     ostatní se postaví kolem něj do rohů. Že ostatní nestojí za ním, zajišťuje
//     už rozestavení při příjmu výkopu (nejhlubší pole = nejlepší držitel míče).
//  2. KLEC — v dalších kolech s klecí co nejdál dopředu (CageAdvancePlanner:
//     napřed rohy, nosič poslední, tempo podle nejpomalejšího rohu). Soupeře,
//     který stojí na poli rohu, napřed shodit blokem.
//  3. VÝBĚH — v kole PŘED tím, než by se ukázalo, že klec nedoběhne: nosič
//     vyrazí sám co nejdál a ostatní MARKUJÍ soupeře, kteří by k němu doběhli
//     (jen markovat, „na víc nejsou zdroje"). Pak už sám až do TD zóny.
//
// Zbytek týmu (kdo není roh ani marker) řídí MCTS, stejně jako každý tah, kde
// plán nic nenavrhne nebo se rozbije (hod, obrat, jiné rozestavení).

enum class CagePhase : uint8_t { NONE = 0, PICKUP = 1, CAGE = 2, RELEASE = 3 };

// Pole nejblíž soupeřově TD zóně, kam nosič dojde BEZ HODU (žádný dodge ani
// GFI v rozpočtu `budget`) a kde neskončí vedle stojícího soupeře („blok na
// nosiče se nesmí stávat vůbec", uživatel 20.08.). Mezi stejně vzdálenými
// vybírá to, na které dosáhne méně soupeřů, pak blíž středu hřiště. Když nic
// není lepší než stát, vrací nosičovo pole.
//
// `forCage` (P154, 07.10.2026 — po zvednutí míče): napřed pole, kolem kterého se ještě v tomto
// tahu postaví nejvíc rohů (volní spoluhráči, kteří ještě nehráli a na pole rohu dosáhnou),
// teprve mezi nimi to nejdál. Dosud nosič po zvednutí odběhl co nejdál a rohy za ním nedošly
// („nikdo v dosahu“: 14 chybějících rohů ve 30 tazích se zvednutím, trpaslíci TV1500).
Position farthestSafeForward(const GameState& state, const Player& carrier, int budget,
                             bool forCage = false);

// Kolik ze čtyř polí rohů kolem `sq` ještě v tomto tahu obsadí různí spoluhráči: stojí tam,
// nebo jsou volní (nehráli, nestojí v zóně soupeře) a na pole dosáhnou pohybem bez GFI.
int cornersWithinReach(const GameState& state, const Player& carrier, Position sq);
// Totéž jako seznam (hráč, pole rohu).
std::vector<std::pair<int, Position>> cornersComing(const GameState& state, const Player& carrier, Position sq);

// ⭐ P173 → sjednoceno 08.10.2026 na blitzThreat (review P181 nález 4; uživatel: „pokud má klec dva
//   nebo tři rohy tak, ať soupeř nedosáhne na nosiče — tak je to také validní … tři rohy s volným
//   tím, odkud přijde blitz, jsou víc chyba než správně postavené dva“). Hrozba nejlepší rány na
//   nosiče, KDYBY skončil na `sq` a kolem něj se postavili ti spoluhráči, kteří tam v tomto tahu
//   ještě bez hodu dojdou (cornersComing). Nikdo ze soupeřů nedosáhne → 0. Jedno měřítko pro
//   výběr pole nosiče, pro „smí hledání pohnout nosičem“ i pro plánovač klece.
//   Dřív tu byla tabulka podle POČTU rohů (ballLossRisk) s jinou definicí dosahu.
double carrierThreatAt(const GameState& state, const Player& carrier, Position sq, double stopAbove = 2.0);

// ⭐ P176 — HODNOTA RIZIKA (uživatel 08.10.2026: „když skaven upadne na GFI daleko ode všech
//   soupeřů a nezraní se — je to relativně bezpečnější“). Šance, že míč ležící na `ball` vezme
//   v příštím tahu soupeř, podle vzdálenosti nejbližšího stojícího soupeře. ZMĚŘENO (skaven
//   TV1500, 160 útočných poločasů, 61 případů míče na zemi po vlastním tahu): do 2 polí 23 ze 41
//   (56 %), 3–5 polí 5 ze 13 (38 %), 6 a víc 1 ze 7 (14 %).
double looseBallLossRisk(const GameState& state, TeamSide side, Position ball);

// ⭐ P176 / P178 — CO STOJÍ PÁD NOSIČE (uživatel 08.10.2026: „trpaslíci jsou pomalí a špatně zvedají
//   míč — u nich je cena za pád při GFI vysoká … pomalým týmům a týmům s malou agilitou zvedni cenu
//   GFI, ať to nedělají“). Tři složky, všechny z čísel na hřišti, ne z rasy:
//     · míč vezme soupeř (podle vzdálenosti, looseBallLossRisk),
//     · míč znovu nezvedneme (hod na zvednutí podle obratnosti nosiče, Sure Hands = přehoz) — půl váhy,
//     · ztracený tah: tým bez časové rezervy (teamHasTimeSlack) ho nemá ⇒ +0,5.
//   Trpasličí Runner daleko od soupeře, bez rezervy: 0,14 + 0,05 + 0,5 = 0,69; Gutter Runner
//   s rezervou: 0,14 + 0,07 = 0,21.
double carrierFallCost(const GameState& state, const Player& carrier, Position ball);

// Kdy pustit nosiče samotného (fáze 3). Spočítá, jestli by klec doběhla:
// tento tah ujde `cageStep` (největší krok, pro který jdou obsadit rohy), další
// tahy `futurePace` (tentýž krok minus přirážka za soupeře v koridoru), a
// poslední tah nosič doběhne sám, až je do MA + 2. Když by to po tomto tahu už
// nevyšlo, je TEĎ to kolo před tím, než to zjistíme ⇒ pustit. Jen když nosič
// sám doběhnout stihne; jinak klec drží míč dál.
struct ReleaseDecision {
    bool release = false;
    bool cageMakesIt = false;
    bool soloMakesIt = false;
    int dist = 0;          // pole do soupeřovy TD zóny
    int turnsLeft = 0;     // včetně tohoto tahu
    int cageStep = 0;
    int futurePace = 0;
};
ReleaseDecision decideRelease(const GameState& state, const Player& carrier,
                              const CageAdvancePlanner& planner);

class CageController {
public:
    CageController(const ValueFunction* vf, MCTSConfig config, uint32_t seed = 0);

    // Další makro klece pro tento stav, nebo false = rozhoduje MCTS.
    bool next(const GameState& state, Macro& out);

    // P154 (b): řadič v tomto tahu drží nosiče v kleci a makro `m` by ho z ní samotného odvedlo.
    bool forbidsCarrierMove(const GameState& state, const Macro& m) const;

    // P154: hledání chce ukončit tah a míč držíme ⇒ napřed dotáhnout klec (postup, nebo aspoň
    // rohy kolem nosiče). True = `out` je první makro; další vydá next(). Jednou za tah.
    bool beforeEndTurn(const GameState& state, Macro& out);

    // Makro z plánu se na skutečné desce neprovedlo ⇒ zbytek tahu patří search().
    void abandonTurn() { queue_.clear(); idx_ = 0; stage_ = Stage::DONE; }

    CagePhase phase() const { return phase_; }
    int plansAdopted() const { return adopted_; }

private:
    enum class Stage { START, AFTER_PICKUP, AFTER_BLOCKS, AFTER_ADVANCE, AFTER_RUN, DONE };

    CageAdvancePlanner planner_;
    MCTSConfig config_;

    // stav jednoho tahu
    TeamSide team_ = TeamSide::HOME;
    int turn_ = -1;
    int half_ = -1;
    Stage stage_ = Stage::DONE;
    // P169 krok 9: míč na začátku tahu náš nebyl a zvedlo ho až hledání ⇒ rohy se dostaví hned potom
    bool ballOursAtTurnStart_ = false;
    bool lateFillDone_ = false;
    CagePhase phase_ = CagePhase::NONE;
    std::vector<Macro> queue_;
    size_t idx_ = 0;

    // fáze 3 platí do konce držení: jednou vypuštěný nosič se do klece nevrací
    int releasedCarrier_ = -1;
    int releasedHalf_ = -1;
    int releasedScore_ = -1;

    int adopted_ = 0;

    // planStart: tah je SCORE_BALL, ale nosič do zóny bez hodu nedojde ⇒ klec postupuje dál
    bool scoringRangeCage_ = false;
    bool stalling_ = false;      // P175: v tomto tahu se TD zdržuje (míč v bezpečí, není poslední kolo)

    // beforeEndTurn: ve kterém tahu už se zkoušelo
    TeamSide endTurnTeam_ = TeamSide::HOME;
    int endTurnTurn_ = -1;
    int endTurnHalf_ = -1;

    void planStart(const GameState& state);
    void planAfterPickup(const GameState& state);
    void planAdvance(const GameState& state);
    void planLaggards(const GameState& state);
    void planMarkers(const GameState& state);
    bool released(const GameState& state, const Player& carrier) const;
    bool stillValid(const GameState& state, const Macro& m) const;
};

} // namespace bb
