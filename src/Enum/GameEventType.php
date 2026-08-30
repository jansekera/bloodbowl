<?php
declare(strict_types=1);

namespace App\Enum;

/**
 * Typ události zápasu -- hodnota sloupce `match_events.event_type`.
 *
 * ⛔ PROČ ENUM A NE VOLNÝ VARCHAR: sloupec neměl ani `CHECK`, ani protějšek
 *    v PHP, na rozdíl od sousedního `team_side`. Překlep v typu se tedy
 *    nikde neprojevil -- řádek se uložil a nikdo se to nedozvěděl.
 *
 * ⛔ PROČ `string` A NE `int`: hodnoty jsou v databázi čitelné, a hlavně
 *    NENÍ CO PŘEČÍSLOVAT. U intu se mapování stává daty: vloží se hodnota
 *    doprostřed výčtu a uložené řádky tiše znamenají něco jiného.
 *
 * ⛔ SEZNAM SE JEN DOPLŇUJE, NIKDY NEPŘEJMENOVÁVÁ ani nemaže -- stejné
 *    pravidlo jako u `GameEvent::Type` v C++ enginu, kde přejmenování
 *    přeznačí každou událost v už sebraných korpusech.
 *
 * Zdroj pravdy je tenhle enum; `CHECK` v migraci a tovární metody
 * `GameEvent` z něj musí vycházet -- hlídá to `GameEventTypeTest`.
 */
enum GameEventType: string
{
    case ALWAYS_HUNGRY          = 'always_hungry';
    case ANIMOSITY              = 'animosity';
    case APOTHECARY             = 'apothecary';
    case ARMOUR_ROLL            = 'armour_roll';
    case BALL_AND_CHAIN_BLOCK   = 'ball_and_chain_block';
    case BALL_AND_CHAIN_MOVE    = 'ball_and_chain_move';
    case BALL_BOUNCE            = 'ball_bounce';
    case BLOCK                  = 'block';
    case BLOODLUST_BITE         = 'bloodlust_bite';
    case BLOODLUST_FAIL         = 'bloodlust_fail';
    case BOMB_EXPLOSION         = 'bomb_explosion';
    case BOMB_LANDING           = 'bomb_landing';
    case BOMB_THROW             = 'bomb_throw';
    case BONE_HEAD              = 'bone_head';
    case CATCH                  = 'catch';
    case CHAIN_PUSH             = 'chain_push';
    case CHAINSAW               = 'chainsaw';
    case CHAINSAW_KICKBACK      = 'chainsaw_kickback';
    case CROWD_SURF             = 'crowd_surf';
    case DIVING_CATCH           = 'diving_catch';
    case DIVING_TACKLE          = 'diving_tackle';
    case DODGE                  = 'dodge';
    case DUMP_OFF               = 'dump_off';
    case EJECTION               = 'ejection';
    case END_TURN               = 'end_turn';
    case FEND                   = 'fend';
    case FOLLOW_UP              = 'follow_up';
    case FOUL                   = 'foul';
    case FOUL_APPEARANCE        = 'foul_appearance';
    case FRENZY                 = 'frenzy';
    case GAME_OVER              = 'game_over';
    case GFI                    = 'gfi';
    case HAIL_MARY_PASS         = 'hail_mary_pass';
    case HALF_TIME              = 'half_time';
    case HAND_OFF               = 'hand_off';
    case HYPNOTIC_GAZE          = 'hypnotic_gaze';
    case INJURY_ROLL            = 'injury_roll';
    case INTERCEPTION           = 'interception';
    case JUGGERNAUT             = 'juggernaut';
    case KICK_OFF_RETURN        = 'kick_off_return';
    case KICK_SKILL             = 'kick_skill';
    case KICKOFF                = 'kickoff';
    case KICKOFF_TABLE          = 'kickoff_table';
    case KO_RECOVERY            = 'ko_recovery';
    case LEADER                 = 'leader';
    case LEAP                   = 'leap';
    case LONER                  = 'loner';
    case MULTIPLE_BLOCK         = 'multiple_block';
    case NO_HANDS               = 'no_hands';
    case NURGLES_ROT            = 'nurgles_rot';
    case PASS                   = 'pass';
    case PASS_BLOCK             = 'pass_block';
    case PICKUP                 = 'pickup';
    case PILING_ON              = 'piling_on';
    case PLAYER_FELL            = 'player_fell';
    case PLAYER_MOVE            = 'player_move';
    case PRO                    = 'pro';
    case PUSH                   = 'push';
    case REALLY_STUPID          = 'really_stupid';
    case REGENERATION           = 'regeneration';
    case REROLL                 = 'reroll';
    case SAFE_THROW             = 'safe_throw';
    case SECRET_WEAPON          = 'secret_weapon';
    case SHADOWING              = 'shadowing';
    case SNEAKY_GIT             = 'sneaky_git';
    case STAB                   = 'stab';
    case STAKES_BLOCK_REGEN     = 'stakes_block_regen';
    case STAND_UP               = 'stand_up';
    case STRIP_BALL             = 'strip_ball';
    case SWELTERING_HEAT        = 'sweltering_heat';
    case TAKE_ROOT              = 'take_root';
    case TENTACLES              = 'tentacles';
    case THROW_IN               = 'throw_in';
    case THROW_TEAM_MATE        = 'throw_team_mate';
    case TOUCHBACK              = 'touchback';
    case TOUCHDOWN              = 'touchdown';
    case TTM_LANDING            = 'ttm_landing';
    case TURNOVER               = 'turnover';
    case WEATHER_CHANGE         = 'weather_change';
    case WILD_ANIMAL            = 'wild_animal';
    case WRESTLE                = 'wrestle';
}
