import { describe, it, expect, vi } from 'vitest';
import { ActionPanel } from './ActionPanel';
import type { GameState, MatchPlayer } from '../api/types';

/**
 * P272 (10.11.2026): hod bombou za kouče-člověka. Bomba nesdílí týmovou akci Pass (`rules_bb2016.txt` ř. 7950-7951),
 * tlačítko je tedy jen u týmu s bombardierem a nevypíná ho použitá přihrávka.
 */
function mockContainer() {
    return {
        innerHTML: '',
        classList: { add: vi.fn() },
        querySelectorAll: () => [],
    };
}

function player(id: number, teamSide: 'home' | 'away', skills: string[]): MatchPlayer {
    return {
        id, playerId: id, name: `P${id}`, number: id, positionalName: 'x',
        stats: { movement: 6, strength: 3, agility: 3, armour: 8 } as MatchPlayer['stats'],
        skills, state: 'standing', position: { x: 5, y: 5 }, hasMoved: false, hasActed: false,
        movementRemaining: 6, teamSide,
    };
}

function state(players: MatchPlayer[], passUsed = false): GameState {
    const team = (side: 'home' | 'away') => ({
        teamId: 1, name: side, raceName: 'Goblin', side, score: 0, rerolls: 2, turnNumber: 1,
        blitzUsedThisTurn: false, passUsedThisTurn: passUsed, foulUsedThisTurn: false,
    });
    return {
        matchId: 1, half: 1, activeTeam: 'home', phase: 'play',
        homeTeam: team('home'), awayTeam: team('away'), players,
        ball: { position: null, isHeld: false, carrierId: null },
        turnoverPending: false, kickingTeam: null, aiTeam: 'away', weather: 'nice',
    } as unknown as GameState;
}

describe('ActionPanel bomb button', () => {
    it('shows Bomb only when the active team has a Bombardier', () => {
        const c = mockContainer();
        const panel = new ActionPanel(c as unknown as HTMLElement, vi.fn());
        panel.update(state([player(1, 'home', ['Bombardier'])]), true);
        expect(c.innerHTML).toContain('data-action="toggle_bomb"');

        panel.update(state([player(1, 'home', []), player(2, 'away', ['Bombardier'])]), true);
        expect(c.innerHTML).not.toContain('toggle_bomb');
    });

    it('a used Pass action does not disable the bomb', () => {
        const c = mockContainer();
        const panel = new ActionPanel(c as unknown as HTMLElement, vi.fn());
        panel.update(state([player(1, 'home', ['Bombardier'])], true), true);
        expect(c.innerHTML).toMatch(/data-action="toggle_bomb">/);
    });
});
