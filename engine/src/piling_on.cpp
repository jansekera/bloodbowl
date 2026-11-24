#include "bb/piling_on.h"

namespace bb {

namespace {
// počet kombinací 2k6 se součtem s (2..12) z 36
int ways2d6(int s) { return (s < 2 || s > 12) ? 0 : 6 - (s > 7 ? s - 7 : 7 - s); }

bool breaks(int roll, int av, bool claw) { return roll > av || (claw && roll >= 8); }
}

double injuryOutChance(const Player& victim, int mod) {
    const bool stunty = victim.hasSkill(SkillName::Stunty);
    const bool thickSkull = victim.hasSkill(SkillName::ThickSkull);
    int out = 0;
    for (int s = 2; s <= 12; ++s) {
        const int m = s + mod;
        const bool stunned = m <= (stunty ? 6 : 7) || (thickSkull && m == 8);
        if (!stunned) out += ways2d6(s);
    }
    return out / 36.0;
}

double armourThenOutChance(const Player& victim, int armourMod, bool mightyBlow, bool claw) {
    const int av = victim.stats.armour;
    double p = 0.0;
    for (int s = 2; s <= 12; ++s) {
        const int base = s + armourMod;
        // Mighty Blow jako `resolveArmourAndInjury`: na brnění jen, když ho potřebuje, jinak na zranění
        if (breaks(base, av, claw)) {
            p += ways2d6(s) / 36.0 * injuryOutChance(victim, mightyBlow ? 1 : 0);
        } else if (mightyBlow && breaks(base + 1, av, claw)) {
            p += ways2d6(s) / 36.0 * injuryOutChance(victim, 0);
        }
    }
    return p;
}

bool pilingOnAllowed(const GameState& state, const Player& att, const Player& def) {
    return att.hasSkill(SkillName::PilingOn) && att.state == PlayerState::STANDING &&
           att.position.distanceTo(def.position) == 1 && state.getTeamState(att.teamSide).canUseReroll();
}

double foulOutChance(const GameState& state, const Player& p, int excludeId) {
    const TeamSide foe = opponent(p.teamSide);
    int assists = 0;
    bool dirty = false;
    state.forEachPlayer(foe, [&](const Player& e) {
        if (e.state != PlayerState::STANDING || e.id == excludeId) return;
        if (e.hasSkill(SkillName::DirtyPlayer)) dirty = true;
        if (e.position.distanceTo(p.position) == 1) ++assists;
    });
    return armourThenOutChance(p, assists + (dirty ? 1 : 0), false, false);
}

bool aiPilingOn(const GameState& state, const Player& att, const Player& def, bool armourStage,
                int armourMod, bool mightyBlow, bool claw, int injuryMod, bool multipleBlock) {
    if (multipleBlock) return false;
    const BallState& ball = state.ball;
    if (ball.isHeld && ball.carrierId == att.id) return false;   // turnover
    // šetřit přehoz, dokud tým hraje s míčem (otázka 2: „ano“)
    bool ballDone = false;
    if (ball.isHeld) {
        const Player& carrier = state.getPlayer(ball.carrierId);
        ballDone = carrier.teamSide != att.teamSide || carrier.hasActed;
    }
    if (!ballDone) return false;
    const double gain = armourStage ? armourThenOutChance(def, armourMod, mightyBlow, claw)
                                    : injuryOutChance(def, injuryMod);
    return gain > foulOutChance(state, att, def.id);
}

} // namespace bb
