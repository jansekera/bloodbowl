#pragma once

#include "bb/enums.h"
#include <exception>
#include <vector>

namespace bb {

// P130 (12.10.2026): rozhodnutí, která v živé partii dělá KOUČ, ne engine --
// přehoz (týmový / Pro) a volitelné použití skillu (BB2016 l. 985, 1820
// "Skill use is not mandatory"; l. 987 i v tahu soupeře).
//
// Engine se uprostřed akce neumí zastavit a počkat, proto: volající předá
// frontu odpovědí; když akce narazí na otázku a fronta je prázdná, vyhodí
// CoachChoicePending. Volající vrátí stav i kostky do stavu před akcí,
// zeptá se, odpověď přidá na konec fronty a akci spustí znovu -- se stejnými
// kostkami dojde na totéž místo a pokračuje. Výsledek přehozu tak předem
// nevidí nikdo.
//
// Režim platí jen uvnitř CoachChoiceScope (jedna akce živé partie). Mimo něj
// se engine chová jako dosud ⇒ AI, měření i `tahai` beze změny.
enum class CoachQuestion : int {
    REROLL = 0,          // neúspěšný hod: nechat / tým / Pro
    PRO_GATE_REROLL = 1, // hod na Pro 1-3: nechat / tým (l. 8385-8389)
    BLOCK_REROLL = 2,    // kostky bloku, po KAŽDÉM hodu: nechat / tým / Pro
    PASS_REROLL = 3,     // nepřesná přihrávka nebo fumble: nechat / tým / Pro
    USE_DODGE = 4,       // Defender Stumbles: použít Dodge?
    // 5 neobsazeno: Block při Both Down zůstává automatický (uživatel 12.10.:
    // odmítnout ho nic nepřináší -- s Blockem spadne jen útočník).
    USE_WRESTLE = 6,     // Both Down: použít Wrestle?
    USE_STAND_FIRM = 7,  // odtlačení: zůstat stát?
    USE_SIDE_STEP = 8,   // odtlačení: vybrat pole sám?
    BLOCK_FACE = 9,      // kterou z hozených kostek vzít; odpověď = index BlockDiceFace
    // P201 (14.10.2026):
    USE_FOLLOW_UP = 10,  // po odtlačení: následovat? (l. 606-611 "coach must decide")
    USE_APOTHECARY = 11, // použít lékárníka? roll = CasualtyResult, nebo -1 u KO (l. 1200-1210)
    APOTHECARY_PICK = 12, // který výsledek platí: 0 = první (roll), 1 = druhý (target)
    // P200 (1), 14.10.2026: Kick-Off Return (ř. 8249-8256 "may use"). playerId = hráč, roll = počet
    // nabízených polí, target = dopad míče x*100+y; odpověď 0 = nepoužít, i = i-té pole
    // z kickoffReturnSquares (seřazená od nejlepšího pro AI).
    KICKOFF_RETURN = 13,
    // Uživatel 14.10.: víc oprávněných nositelů KOR ⇒ napřed KDO. playerId = -1, roll = počet,
    // target = dopad x*100+y; odpověď 0 = nikdo, i = i-tý z kickoffReturnCandidates.
    KICKOFF_RETURN_PLAYER = 14,
    // P222 (20.10.2026): útočník s Grabem při odtlačení (ř. 8149 "may choose"); playerId =
    // útočník, otherId = odtlačený. Ptá se jen, když Grab platit může (Block Action / blok B&C,
    // volné pole vedle soupeře, bez Side Step obránce). Pole pak volí příkaz `push`.
    USE_GRAB = 15,
    // Uživatel 30.10.2026: hráč s Diving Catch, když míč z přihrávky, výkopu nebo vhazování (ne odraz)
    // dopadá na prázdné pole v jeho tackle zóně (ř. 8065-8071 "can attempt"). playerId = hráč, target =
    // pole dopadu x*100+y, roll = počet DALŠÍCH kandidátů, otherId = hráč, který už se pokouší (-1 = nikdo).
    // Ptá se kouče hráče, i v tahu soupeře; víc kandidátů = otázka na každého (varianta B, divingCatchAttempt).
    USE_DIVING_CATCH = 16
};

namespace CoachAnswer {
constexpr int NONE = 0;   // přehoz: nechat · skill: nepoužít
constexpr int TEAM = 1;   // přehoz: týmový reroll · skill: použít
constexpr int PRO = 2;    // přehoz: Pro
}

struct CoachChoicePending : std::exception {
    CoachQuestion question;
    int playerId;
    int otherId;
    int roll;      // co padlo (u bloku kostky po 3 bitech jako v události BLOCK)
    int target;    // potřeba (u bloku počet kostek)
    int options;   // bitová maska povolených odpovědí: 1 << CoachAnswer
    CoachChoicePending(CoachQuestion q, int p, int o, int r, int t, int opts)
        : question(q), playerId(p), otherId(o), roll(r), target(t), options(opts) {}
    const char* what() const noexcept override { return "coach choice pending"; }
};

// Rozhoduje za tuto stranu kouč?
bool coachDecides(TeamSide side);
// Běží akce živé partie (kterákoli strana)? Pak se přehozy zapisují do událostí.
bool coachModeOn();
// Vrátí další odpověď z fronty; je-li prázdná, vyhodí CoachChoicePending.
// Odpověď mimo `options` = std::invalid_argument.
int askCoach(CoachQuestion q, int playerId, int otherId, int roll, int target, int options);

class CoachChoiceScope {
public:
    // P201 (6), 14.10.2026: `bothSides` = kouč rozhoduje za OBĚ strany (živá partie, kde druhou
    // stranu hraje ručně Claude); `side` pak určuje jen výchozí stranu pro výpis.
    CoachChoiceScope(TeamSide side, std::vector<int> answers, bool bothSides = false);
    ~CoachChoiceScope();
    CoachChoiceScope(const CoachChoiceScope&) = delete;
    CoachChoiceScope& operator=(const CoachChoiceScope&) = delete;
};

} // namespace bb
