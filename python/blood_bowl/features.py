"""Feature extraction from game state for RL training.

Must produce identical features to PHP FeatureExtractor.
In practice, GameLogger logs pre-computed features, so this module
is mainly for verification and re-computation from raw state dicts.
"""
from __future__ import annotations

NUM_FEATURES = 70


def extract_features(state: dict, perspective: str) -> list[float]:
    """Extract ~30 features from game state dict. Must match PHP FeatureExtractor.

    Args:
        state: Game state dict (as from GameState::toArray())
        perspective: 'home' or 'away'
    """
    opp = 'away' if perspective == 'home' else 'home'

    my_team = state.get(f'{perspective}Team', {})
    opp_team = state.get(f'{opp}Team', {})

    players = state.get('players', [])

    # Categorize players
    my_standing = 0
    my_ko = 0
    my_injured = 0
    opp_standing = 0
    opp_ko = 0
    opp_injured = 0

    my_standing_on_pitch = []
    opp_standing_on_pitch = []

    for p in players:
        side = p.get('teamSide', '')
        pstate = p.get('state', '')
        pos = p.get('position')

        if side == perspective:
            if pstate == 'standing':
                my_standing += 1
                if pos is not None:
                    my_standing_on_pitch.append(p)
            elif pstate == 'ko':
                my_ko += 1
            elif pstate in ('injured', 'dead'):
                my_injured += 1
        elif side == opp:
            if pstate == 'standing':
                opp_standing += 1
                if pos is not None:
                    opp_standing_on_pitch.append(p)
            elif pstate == 'ko':
                opp_ko += 1
            elif pstate in ('injured', 'dead'):
                opp_injured += 1

    # Ball info
    ball = state.get('ball', {})
    is_held = ball.get('isHeld', False)
    carrier_id = ball.get('carrierId')
    ball_pos = ball.get('position')

    i_have_ball = 0.0
    opp_has_ball = 0.0
    ball_on_ground = 0.0
    carrier_dist_to_td = 0.5
    ball_in_my_half = 0.0

    if is_held and carrier_id is not None:
        carrier = _find_player(players, carrier_id)
        if carrier is not None:
            if carrier.get('teamSide') == perspective:
                i_have_ball = 1.0
            else:
                opp_has_ball = 1.0
            cpos = carrier.get('position')
            if cpos is not None:
                carrier_dist_to_td = _distance_to_endzone(cpos['x'], perspective) / 26.0
                ball_in_my_half = 1.0 if _is_in_my_half(cpos['x'], perspective) else 0.0
    elif ball_pos is not None:
        ball_on_ground = 1.0
        ball_in_my_half = 1.0 if _is_in_my_half(ball_pos['x'], perspective) else 0.0

    # Average positions, strength, armour, agility
    my_avg_x = 0.0
    opp_avg_x = 0.0
    my_avg_str = 0.0
    opp_avg_str = 0.0
    my_avg_armour = 0.0
    opp_avg_armour = 0.0
    my_avg_agility = 0.0
    opp_avg_agility = 0.0

    if my_standing_on_pitch:
        count = len(my_standing_on_pitch)
        sum_x = sum(_normalize_x(p['position']['x'], perspective) for p in my_standing_on_pitch)
        sum_str = sum(p.get('stats', {}).get('strength', 3) for p in my_standing_on_pitch)
        sum_armour = sum(p.get('stats', {}).get('armour', 8) for p in my_standing_on_pitch)
        sum_agility = sum(p.get('stats', {}).get('agility', 3) for p in my_standing_on_pitch)
        my_avg_x = (sum_x / count) / 26.0
        my_avg_str = (sum_str / count) / 5.0
        my_avg_armour = (sum_armour / count) / 10.0
        my_avg_agility = (sum_agility / count) / 5.0

    if opp_standing_on_pitch:
        count = len(opp_standing_on_pitch)
        sum_x = sum(_normalize_x(p['position']['x'], perspective) for p in opp_standing_on_pitch)
        sum_str = sum(p.get('stats', {}).get('strength', 3) for p in opp_standing_on_pitch)
        sum_armour = sum(p.get('stats', {}).get('armour', 8) for p in opp_standing_on_pitch)
        sum_agility = sum(p.get('stats', {}).get('agility', 3) for p in opp_standing_on_pitch)
        opp_avg_x = (sum_x / count) / 26.0
        opp_avg_str = (sum_str / count) / 5.0
        opp_avg_armour = (sum_armour / count) / 10.0
        opp_avg_agility = (sum_agility / count) / 5.0

    # Cage count
    my_cage_count = 0.0
    if i_have_ball > 0 and carrier_id is not None:
        carrier = _find_player(players, carrier_id)
        if carrier is not None:
            cpos = carrier.get('position')
            if cpos is not None:
                my_on_pitch = [p for p in players
                               if p.get('teamSide') == perspective
                               and p.get('state', '') in ('standing', 'prone', 'stunned')
                               and p.get('position') is not None]
                for p in my_on_pitch:
                    if p.get('id') == carrier_id:
                        continue
                    ppos = p['position']
                    if max(abs(ppos['x'] - cpos['x']), abs(ppos['y'] - cpos['y'])) == 1:
                        my_cage_count += 1.0

    # Receiving team
    kicking_team = state.get('kickingTeam')
    is_receiving = 1.0 if (kicking_team is not None and kicking_team != perspective) else 0.0

    # Turn progress
    turn_number = my_team.get('turnNumber', 1)
    half_num = state.get('half', 1)
    turn_progress = min(1.0, (turn_number + (half_num - 1) * 8) / 16.0)

    # Weather
    weather = state.get('weather', 'nice')

    # Active team
    active_team = state.get('activeTeam', '')

    my_score = my_team.get('score', 0)
    opp_score = opp_team.get('score', 0)

    # Sideline awareness: fraction of standing on Y=0 or Y=14
    my_sideline_count = sum(
        1 for p in my_standing_on_pitch if p['position']['y'] in (0, 14)
    )
    opp_sideline_count = sum(
        1 for p in opp_standing_on_pitch if p['position']['y'] in (0, 14)
    )
    my_sideline_frac = my_sideline_count / len(my_standing_on_pitch) if my_standing_on_pitch else 0.0
    opp_sideline_frac = opp_sideline_count / len(opp_standing_on_pitch) if opp_standing_on_pitch else 0.0

    # Turns remaining
    turns_remaining = max(0, 9 - turn_number) / 8.0

    # Score advantage with ball
    score_diff_raw = my_score - opp_score
    score_advantage_with_ball = (
        min((score_diff_raw + 1) / 4.0, 1.0)
        if score_diff_raw >= 0 and i_have_ball > 0
        else 0.0
    )

    # Carrier near endzone (within 3 squares of TD)
    carrier_near_endzone = 0.0
    if i_have_ball > 0 and carrier_id is not None:
        carrier = _find_player(players, carrier_id)
        if carrier is not None:
            cpos = carrier.get('position')
            if cpos is not None:
                dist_to_td = _distance_to_endzone(cpos['x'], perspective)
                if dist_to_td <= 3:
                    carrier_near_endzone = 1.0

    # Stall incentive (compound)
    stall_incentive = score_advantage_with_ball * turns_remaining * carrier_near_endzone

    # --- New tactical features (40-47) ---

    # Collect all on-pitch players (including prone/stunned) for new features
    my_on_pitch = []
    opp_on_pitch = []
    for p in players:
        side = p.get('teamSide', '')
        pstate = p.get('state', '')
        pos = p.get('position')
        if pos is None:
            continue
        if pstate in ('standing', 'prone', 'stunned'):
            if side == perspective:
                my_on_pitch.append(p)
            elif side == opp:
                opp_on_pitch.append(p)

    # Carrier TZ count: opp standing adjacent to my carrier
    carrier_tz_count = 0.0
    if i_have_ball > 0 and carrier_id is not None:
        carrier = _find_player(players, carrier_id)
        if carrier is not None:
            cpos = carrier.get('position')
            if cpos is not None:
                for op in opp_standing_on_pitch:
                    opos = op['position']
                    if max(abs(opos['x'] - cpos['x']), abs(opos['y'] - cpos['y'])) == 1:
                        carrier_tz_count += 1.0

    # Scoring threat: my carrier MA >= distance to endzone
    scoring_threat = 0.0
    if i_have_ball > 0 and carrier_id is not None:
        carrier = _find_player(players, carrier_id)
        if carrier is not None:
            cpos = carrier.get('position')
            if cpos is not None:
                dist_to_td = _distance_to_endzone(cpos['x'], perspective)
                ma = carrier.get('stats', {}).get('movement', 6)
                if ma >= dist_to_td:
                    scoring_threat = 1.0

    # Opp scoring threat: opp carrier MA >= distance to endzone
    opp_scoring_threat = 0.0
    if opp_has_ball > 0 and carrier_id is not None:
        carrier = _find_player(players, carrier_id)
        if carrier is not None:
            cpos = carrier.get('position')
            if cpos is not None:
                dist_to_td = _distance_to_endzone(cpos['x'], opp)
                ma = carrier.get('stats', {}).get('movement', 6)
                if ma >= dist_to_td:
                    opp_scoring_threat = 1.0

    # Engaged fractions: standing players in opponent TZ (Chebyshev dist=1)
    my_engaged = 0
    for p in my_standing_on_pitch:
        ppos = p['position']
        for op in opp_standing_on_pitch:
            opos = op['position']
            if max(abs(opos['x'] - ppos['x']), abs(opos['y'] - ppos['y'])) == 1:
                my_engaged += 1
                break

    opp_engaged = 0
    for op in opp_standing_on_pitch:
        opos = op['position']
        for p in my_standing_on_pitch:
            ppos = p['position']
            if max(abs(ppos['x'] - opos['x']), abs(ppos['y'] - opos['y'])) == 1:
                opp_engaged += 1
                break

    my_engaged_fraction = my_engaged / len(my_standing_on_pitch) if my_standing_on_pitch else 0.0
    opp_engaged_fraction = opp_engaged / len(opp_standing_on_pitch) if opp_standing_on_pitch else 0.0

    # Prone/stunned counts
    my_prone_stunned = sum(
        1 for p in my_on_pitch if p.get('state') in ('prone', 'stunned')
    )
    opp_prone_stunned = sum(
        1 for p in opp_on_pitch if p.get('state') in ('prone', 'stunned')
    )

    # Free players: my standing NOT in any opp TZ
    my_free_players = len(my_standing_on_pitch) - my_engaged

    # Skill fractions for standing players
    my_block = sum(1 for p in my_standing_on_pitch if 'Block' in p.get('skills', []))
    opp_block = sum(1 for p in opp_standing_on_pitch if 'Block' in p.get('skills', []))
    my_dodge = sum(1 for p in my_standing_on_pitch if 'Dodge' in p.get('skills', []))
    opp_dodge = sum(1 for p in opp_standing_on_pitch if 'Dodge' in p.get('skills', []))
    my_guard = sum(1 for p in my_standing_on_pitch if 'Guard' in p.get('skills', []))
    my_mighty_blow = sum(1 for p in my_standing_on_pitch if 'Mighty Blow' in p.get('skills', []))
    my_claw = sum(1 for p in my_standing_on_pitch if 'Claw' in p.get('skills', []))

    # Regen count: fraction of ALL team players with Regeneration
    all_my_players = [p for p in players if p.get('teamSide') == perspective]
    my_regen = sum(1 for p in all_my_players if 'Regeneration' in p.get('skills', []))

    my_standing_count = len(my_standing_on_pitch)
    opp_standing_count = len(opp_standing_on_pitch)
    my_total_count = len(all_my_players)

    return [
        # 0  score_diff
        _clamp((my_score - opp_score) / 6.0, -1.0, 1.0),
        # 1  my_score
        min(my_score / 4.0, 1.0),
        # 2  opp_score
        min(opp_score / 4.0, 1.0),
        # 3  turn_progress
        turn_progress,
        # 4  my_players_standing
        my_standing / 11.0,
        # 5  opp_players_standing
        opp_standing / 11.0,
        # 6  my_players_ko
        my_ko / 11.0,
        # 7  opp_players_ko
        opp_ko / 11.0,
        # 8  my_players_injured
        my_injured / 11.0,
        # 9  opp_players_injured
        opp_injured / 11.0,
        # 10 my_rerolls
        min(my_team.get('rerolls', 0) / 4.0, 1.0),
        # 11 opp_rerolls
        min(opp_team.get('rerolls', 0) / 4.0, 1.0),
        # 12 i_have_ball
        i_have_ball,
        # 13 opp_has_ball
        opp_has_ball,
        # 14 ball_on_ground
        ball_on_ground,
        # 15 carrier_dist_to_td
        carrier_dist_to_td,
        # 16 ball_in_my_half
        ball_in_my_half,
        # 17 my_avg_x
        my_avg_x,
        # 18 opp_avg_x
        opp_avg_x,
        # 19 my_avg_strength
        my_avg_str,
        # 20 opp_avg_strength
        opp_avg_str,
        # 21 my_cage_count
        min(my_cage_count / 4.0, 1.0),
        # 22 is_receiving
        is_receiving,
        # 23 is_my_turn
        1.0 if active_team == perspective else 0.0,
        # 24 weather_nice
        1.0 if weather == 'nice' else 0.0,
        # 25 weather_rain
        1.0 if weather == 'pouring_rain' else 0.0,
        # 26 weather_blizzard
        1.0 if weather == 'blizzard' else 0.0,
        # 27 my_blitz_available
        0.0 if my_team.get('blitzUsedThisTurn', False) else 1.0,
        # 28 my_pass_available
        0.0 if my_team.get('passUsedThisTurn', False) else 1.0,
        # 29 bias
        1.0,
        # 30 my_players_on_sideline
        my_sideline_frac,
        # 31 opp_players_on_sideline
        opp_sideline_frac,
        # 32 my_turns_remaining
        turns_remaining,
        # 33 score_advantage_with_ball
        score_advantage_with_ball,
        # 34 carrier_near_endzone
        carrier_near_endzone,
        # 35 stall_incentive
        stall_incentive,
        # 36 my_avg_armour
        my_avg_armour,
        # 37 opp_avg_armour
        opp_avg_armour,
        # 38 my_avg_agility
        my_avg_agility,
        # 39 opp_avg_agility
        opp_avg_agility,
        # 40 carrier_tz_count
        min(carrier_tz_count / 4.0, 1.0),
        # 41 scoring_threat
        scoring_threat,
        # 42 opp_scoring_threat
        opp_scoring_threat,
        # 43 my_engaged_fraction
        my_engaged_fraction,
        # 44 opp_engaged_fraction
        opp_engaged_fraction,
        # 45 my_prone_stunned
        my_prone_stunned / 11.0,
        # 46 opp_prone_stunned
        opp_prone_stunned / 11.0,
        # 47 my_free_players
        my_free_players / 11.0,
        # 48 my_block_skill_count
        my_block / my_standing_count if my_standing_count > 0 else 0.0,
        # 49 opp_block_skill_count
        opp_block / opp_standing_count if opp_standing_count > 0 else 0.0,
        # 50 my_dodge_skill_count
        my_dodge / my_standing_count if my_standing_count > 0 else 0.0,
        # 51 opp_dodge_skill_count
        opp_dodge / opp_standing_count if opp_standing_count > 0 else 0.0,
        # 52 my_guard_count
        my_guard / my_standing_count if my_standing_count > 0 else 0.0,
        # 53 my_mighty_blow_count
        my_mighty_blow / my_standing_count if my_standing_count > 0 else 0.0,
        # 54 my_claw_count
        my_claw / my_standing_count if my_standing_count > 0 else 0.0,
        # 55 my_regen_count
        my_regen / my_total_count if my_total_count > 0 else 0.0,
        # === NEW strategic pattern features [56-69] ===
        *_compute_strategic_features(
            players, my_standing_on_pitch, opp_standing_on_pitch,
            i_have_ball, opp_has_ball, ball_on_ground,
            carrier_id, ball, perspective, opp,
            my_standing_count, my_cage_count,
        ),
    ]


def _chebyshev(p1: dict, p2: dict) -> int:
    return max(abs(p1['x'] - p2['x']), abs(p1['y'] - p2['y']))


def _compute_strategic_features(
    players, my_standing, opp_standing,
    i_have_ball, opp_has_ball, ball_on_ground,
    carrier_id, ball, perspective, opp,
    my_standing_count, my_cage_count,
) -> list[float]:
    """Compute features [56-69]: strategic patterns."""

    carrier = _find_player(players, carrier_id) if carrier_id else None
    carrier_pos = carrier.get('position') if carrier else None

    opp_carrier_pos = None
    if opp_has_ball > 0 and carrier:
        opp_carrier_pos = carrier_pos

    my_carrier_pos = None
    if i_have_ball > 0 and carrier:
        my_carrier_pos = carrier_pos

    # [56] cage_diagonal_quality
    cage_diagonal = 0
    if my_carrier_pos:
        cx, cy = my_carrier_pos['x'], my_carrier_pos['y']
        diags = [(cx-1,cy-1),(cx+1,cy-1),(cx-1,cy+1),(cx+1,cy+1)]
        for dx, dy in diags:
            if dx < 0 or dx > 25 or dy < 0 or dy > 14:
                continue
            for p in my_standing:
                if p['position']['x'] == dx and p['position']['y'] == dy:
                    cage_diagonal += 1
                    break

    # [57] cage_overload_risk
    cage_overload = max(0.0, min(1.0, (my_cage_count - 4) / 4.0)) if i_have_ball > 0 else 0.0

    # [58] opp_cage_diagonal_quality
    opp_cage_diagonal = 0
    if opp_carrier_pos:
        cx, cy = opp_carrier_pos['x'], opp_carrier_pos['y']
        diags = [(cx-1,cy-1),(cx+1,cy-1),(cx-1,cy+1),(cx+1,cy+1)]
        for dx, dy in diags:
            if dx < 0 or dx > 25 or dy < 0 or dy > 14:
                continue
            for p in opp_standing:
                if p['position']['x'] == dx and p['position']['y'] == dy:
                    opp_cage_diagonal += 1
                    break

    # [59] carrier_can_score (MA+2GFI >= dist)
    carrier_can_score = 0.0
    if my_carrier_pos and carrier:
        dist = _distance_to_endzone(my_carrier_pos['x'], perspective)
        ma = carrier.get('stats', {}).get('movement', 6)
        if ma + 2 >= dist:
            carrier_can_score = 1.0

    # [60] pass_scoring_threat
    pass_threats = 0
    if my_carrier_pos and carrier:
        for p in my_standing:
            if p.get('id') == carrier_id:
                continue
            ppos = p['position']
            if _chebyshev(ppos, my_carrier_pos) <= 10:
                dist_td = _distance_to_endzone(ppos['x'], perspective)
                ma = p.get('stats', {}).get('movement', 6)
                if ma + 2 >= dist_td:
                    pass_threats += 1

    # [61] frenzy_trap_risk
    frenzy_traps = 0
    frenzy_count = 0
    for p in my_standing:
        if 'Frenzy' not in p.get('skills', []):
            continue
        frenzy_count += 1
        adj_opp = sum(
            1 for op in opp_standing
            if _chebyshev(p['position'], op['position']) == 1
        )
        if adj_opp >= 2:
            frenzy_traps += 1

    # [62] screen_between_ball
    screen_count = 0
    if opp_carrier_pos:
        ball_x = opp_carrier_pos['x']
        for p in my_standing:
            px = p['position']['x']
            if perspective == 'home':
                if px < ball_x:
                    screen_count += 1
            else:
                if px > ball_x:
                    screen_count += 1

    # [63] carrier_blitzable
    carrier_blitzable = 0.0
    if my_carrier_pos:
        for op in opp_standing:
            dist = _chebyshev(op['position'], my_carrier_pos)
            ma = op.get('stats', {}).get('movement', 6)
            if dist <= ma:
                carrier_blitzable = 1.0
                break

    # [64] surfable_opponents
    surfable = 0
    for op in opp_standing:
        oy = op['position']['y']
        if oy != 0 and oy != 14:
            continue
        for p in my_standing:
            dist = _chebyshev(p['position'], op['position'])
            ma = p.get('stats', {}).get('movement', 6)
            if dist <= ma:
                surfable += 1
                break

    # [65] favorable_blocks (simplified: attST > defST based on assists)
    favorable = 0
    for p in my_standing:
        ppos = p['position']
        p_st = p.get('stats', {}).get('strength', 3)
        for op in opp_standing:
            opos = op['position']
            if _chebyshev(ppos, opos) != 1:
                continue
            op_st = op.get('stats', {}).get('strength', 3)
            # Count assists (simplified)
            my_assists = 0
            for helper in my_standing:
                if helper.get('id') == p.get('id'):
                    continue
                if _chebyshev(helper['position'], opos) == 1:
                    # Check not in opp TZ (excl defender)
                    in_tz = any(
                        _chebyshev(helper['position'], chk['position']) == 1
                        for chk in opp_standing if chk.get('id') != op.get('id')
                        and 'Guard' not in helper.get('skills', [])
                    )
                    if not in_tz:
                        my_assists += 1
            opp_assists = 0
            for helper in opp_standing:
                if helper.get('id') == op.get('id'):
                    continue
                if _chebyshev(helper['position'], ppos) == 1:
                    in_tz = any(
                        _chebyshev(helper['position'], chk['position']) == 1
                        for chk in my_standing if chk.get('id') != p.get('id')
                        and 'Guard' not in helper.get('skills', [])
                    )
                    if not in_tz:
                        opp_assists += 1
            if p_st + my_assists > op_st + opp_assists:
                favorable += 1
                break
            break

    # [66] one_turn_td_vulnerability
    one_turn_vuln = 0.0
    for op in opp_standing:
        dist = _distance_to_endzone(op['position']['x'], opp)
        ma = op.get('stats', {}).get('movement', 6)
        if ma + 2 >= dist:
            in_tz = any(
                _chebyshev(p['position'], op['position']) == 1
                for p in my_standing
            )
            if not in_tz:
                one_turn_vuln = 1.0
                break

    # [67] loose_ball_proximity
    loose_prox = 0.5
    ball_pos = ball.get('position')
    if ball_on_ground > 0 and ball_pos:
        my_closest = min(
            (_chebyshev(p['position'], ball_pos) for p in my_standing), default=99
        )
        opp_closest = min(
            (_chebyshev(p['position'], ball_pos) for p in opp_standing), default=99
        )
        loose_prox = _clamp((opp_closest - my_closest + 5) / 10.0, 0.0, 1.0)

    # [68] deep_safety_count
    deep_safeties = 0
    if opp_standing:
        min_opp_dist = 99
        for op in opp_standing:
            d = op['position']['x'] if perspective == 'home' else (25 - op['position']['x'])
            if d < min_opp_dist:
                min_opp_dist = d
        for p in my_standing:
            d = p['position']['x'] if perspective == 'home' else (25 - p['position']['x'])
            if d < min_opp_dist:
                deep_safeties += 1

    # [69] isolation_count
    isolated = 0
    for p in my_standing:
        has_nearby = any(
            _chebyshev(p['position'], other['position']) <= 3
            for other in my_standing if other.get('id') != p.get('id')
        )
        if not has_nearby:
            isolated += 1

    return [
        cage_diagonal / 4.0,                                             # 56
        cage_overload,                                                   # 57
        opp_cage_diagonal / 4.0,                                         # 58
        carrier_can_score,                                               # 59
        min(pass_threats / 3.0, 1.0),                                    # 60
        frenzy_traps / frenzy_count if frenzy_count > 0 else 0.0,        # 61
        min(screen_count / 5.0, 1.0) if opp_has_ball > 0 else 0.0,      # 62
        carrier_blitzable if i_have_ball > 0 else 0.0,                   # 63
        min(surfable / 3.0, 1.0),                                        # 64
        min(favorable / my_standing_count, 1.0) if my_standing_count > 0 else 0.0,  # 65
        one_turn_vuln,                                                   # 66
        loose_prox,                                                      # 67
        min(deep_safeties / 3.0, 1.0),                                   # 68
        isolated / my_standing_count if my_standing_count > 0 else 0.0,  # 69
    ]


def _find_player(players: list[dict], player_id: int) -> dict | None:
    for p in players:
        if p.get('id') == player_id:
            return p
    return None


def _normalize_x(x: int, perspective: str) -> float:
    if perspective == 'away':
        return float(25 - x)
    return float(x)


def _distance_to_endzone(x: int, perspective: str) -> int:
    if perspective == 'home':
        return 25 - x
    return x


def _is_in_my_half(x: int, perspective: str) -> bool:
    if perspective == 'home':
        return x <= 12
    return x >= 13


def _clamp(value: float, min_val: float, max_val: float) -> float:
    return max(min_val, min(max_val, value))
