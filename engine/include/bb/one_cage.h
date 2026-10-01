#pragma once

#include "bb/cage_advance.h"
#include "bb/game_state.h"
#include "bb/macro_actions.h"
#include "bb/mcts.h"
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
Position farthestSafeForward(const GameState& state, const Player& carrier, int budget);

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

    // Makro z plánu se na skutečné desce neprovedlo ⇒ zbytek tahu patří search().
    void abandonTurn() { queue_.clear(); idx_ = 0; stage_ = Stage::DONE; }

    CagePhase phase() const { return phase_; }
    int plansAdopted() const { return adopted_; }

private:
    enum class Stage { START, AFTER_PICKUP, AFTER_BLOCKS, AFTER_RUN, DONE };

    CageAdvancePlanner planner_;
    MCTSConfig config_;

    // stav jednoho tahu
    TeamSide team_ = TeamSide::HOME;
    int turn_ = -1;
    int half_ = -1;
    Stage stage_ = Stage::DONE;
    CagePhase phase_ = CagePhase::NONE;
    std::vector<Macro> queue_;
    size_t idx_ = 0;

    // fáze 3 platí do konce držení: jednou vypuštěný nosič se do klece nevrací
    int releasedCarrier_ = -1;
    int releasedHalf_ = -1;
    int releasedScore_ = -1;

    int adopted_ = 0;

    void planStart(const GameState& state);
    void planAfterPickup(const GameState& state);
    void planAdvance(const GameState& state);
    void planMarkers(const GameState& state);
    bool released(const GameState& state, const Player& carrier) const;
    bool stillValid(const GameState& state, const Macro& m) const;
};

} // namespace bb
