"""Terminal reward + discounted-return SSOT (2026-06-26, break-the-draw team).

Replaces the cost-free-draw +1/0/-1 scheme whose Nash equilibrium is "don't lose"
(-> the 0-0 collapse). Win >> draw >> loss, and scoring a TD has value EVEN IN A
LOSS (in tournaments, TDs scored count toward points; a human playing goblins/a
weak team still wants to score). This is NOT PBRS: it re-prices the terminal
outcome of the game, so it can (and is meant to) change the optimal policy.

Ordering is strict by construction:
    any win (+1.0)  >  any draw (-0.50 .. -0.05)  >  any loss (-1.00 .. -0.64)
and within draws/losses, more own TDs is strictly better. All values are in
(-1, 1) so a tanh value head can represent them.

Disconnect A note: replay_buffer.add_game previously computed its own
winner-only reward (draw == 0.0), bypassing the score-aware reward entirely.
Both the replay path and the full-log path now route through terminal_value().
"""
from __future__ import annotations
import os
from typing import List


def terminal_value(home_score: int, away_score: int, perspective: str) -> float:
    """Perspective-relative terminal value of a finished game."""
    my, opp = (home_score, away_score) if perspective == 'home' else (away_score, home_score)
    if my > opp:
        return 1.0
    if my < opp:
        # TD-in-loss has value; stays strictly below any draw (max = -1+0.36 = -0.64).
        return -1.0 + 0.12 * min(my, 3)
    # Draw: strong penalty, graded by our scoring (0-0 worst); stays below any win.
    return -0.5 + 0.15 * min(my, 3)


def episode_returns(my_scores: List[int], opp_scores: List[int],
                    term_val: float, gamma: float, C: float = 0.2) -> List[float]:
    """Discounted MC return with a local per-TD step reward folded in (Lever B).

    G_t = sum_{k>=t} gamma^(k-t) * r_k  +  gamma^(T-t) * term_val,  clamped [-1,1]
    where r_k = C*(my TD scored at k) - C*(opp TD scored at k). Gives mid-game
    states leading up to a TD a positive discounted target (the missing
    "carry -> TD" pull). Not wired into training yet — kept here as SSOT for the
    follow-up experiment after terminal_value alone is validated.
    """
    n = len(my_scores)
    if n == 0:
        return []
    G = [0.0] * n
    G[-1] = term_val
    for i in range(n - 2, -1, -1):
        sr = C * max(0, my_scores[i + 1] - my_scores[i]) \
            - C * max(0, opp_scores[i + 1] - opp_scores[i])
        G[i] = max(-1.0, min(1.0, sr + gamma * G[i + 1]))
    return G


def board_potential(features) -> float:
    """Φ(s): scoring-proximity potential, perspective-relative, from the feature
    vector (out[12]=iHaveBall, out[15]=carrierDistToTD/26). Possession × proximity
    to the attacked endzone: 0.3 for merely holding the ball, rising to 1.0 with the
    carrier at the endzone; 0.0 when we don't hold the ball.

    Used as a LEVEL shaping term added to the regression target (target += β·Φ(s)),
    NOT the PBRS difference form (γΦ(s')−Φ(s)). Root cause (2026-06-30): the flat MC
    target γ^(T−t)·terminal gives every state in a drawn game ~the same label, so the
    value head never learns that advancing the carrier is better. A LEVEL potential
    makes within-game targets rise monotonically with carrier progress, teaching that
    gradient directly. β is kept small and gated on benchmark, since a level term
    biases the value off a pure outcome predictor. The PBRS difference variant
    (mc_return_shaped + DEFAULT_SHAPING_WEIGHTS) telescopes to ~constant and already
    failed (89→80)."""
    if features[12] <= 0.5:          # out[12] = iHaveBall
        return 0.0
    proximity = 1.0 - float(features[15])   # out[15] = carrierDistToTD/26 ∈ [0,1]
    proximity = max(0.0, min(1.0, proximity))
    return 0.3 + 0.7 * proximity


def value_potential_beta() -> float:
    """β weight for board_potential() level shaping. BB_VALUE_POTENTIAL_BETA env
    (default 0.0 = off → identical to the unshaped MC return)."""
    try:
        return float(os.environ.get('BB_VALUE_POTENTIAL_BETA', '0.0'))
    except (TypeError, ValueError):
        return 0.0
