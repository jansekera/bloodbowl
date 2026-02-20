import { describe, it, expect } from 'vitest';
import { buildSummaryData, type MatchSummaryData } from './MatchSummary';
import type { GameState } from '../api/types';

function makeGameState(homeScore: number, awayScore: number): GameState {
    return {
        matchId: 1,
        half: 2,
        activeTeam: 'home',
        phase: 'game_over',
        homeTeam: {
            teamId: 1,
            name: 'Home Humans',
            raceName: 'Human',
            side: 'home',
            score: homeScore,
            rerolls: 3,
            rerollUsedThisTurn: false,
            turnNumber: 8,
        },
        awayTeam: {
            teamId: 2,
            name: 'Away Orcs',
            raceName: 'Orc',
            side: 'away',
            score: awayScore,
            rerolls: 2,
            rerollUsedThisTurn: false,
            turnNumber: 8,
        },
        players: [],
        ball: { x: 0, y: 0, isHeld: false, carrierId: null },
        turnoverPending: false,
        kickingTeam: 'away',
        weather: 'nice',
    };
}

describe('buildSummaryData', () => {
    it('extracts correct scores from game state', () => {
        const state = makeGameState(2, 1);
        const data = buildSummaryData(state);

        expect(data.homeTeamName).toBe('Home Humans');
        expect(data.awayTeamName).toBe('Away Orcs');
        expect(data.homeScore).toBe(2);
        expect(data.awayScore).toBe(1);
    });

    it('handles draw correctly', () => {
        const state = makeGameState(1, 1);
        const data = buildSummaryData(state);

        expect(data.homeScore).toBe(1);
        expect(data.awayScore).toBe(1);
    });

    it('returns empty touchdown lists and null mvp by default', () => {
        const state = makeGameState(3, 0);
        const data = buildSummaryData(state);

        expect(data.homeTouchdowns).toEqual([]);
        expect(data.awayTouchdowns).toEqual([]);
        expect(data.mvp).toBeNull();
    });
});
