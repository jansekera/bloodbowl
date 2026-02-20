import { describe, it, expect, vi } from 'vitest';
import { GameStateManager } from './GameStateManager';
import type { GameState, MatchPlayer } from '../api/types';

function createMinimalGameState(): GameState {
    return {
        matchId: 1,
        half: 1,
        activeTeam: 'home',
        phase: 'play',
        homeTeam: {
            teamId: 1, name: 'Home', raceName: 'Human', side: 'home',
            score: 0, rerolls: 3, rerollUsedThisTurn: false, turnNumber: 1,
            blitzUsedThisTurn: false, passUsedThisTurn: false, foulUsedThisTurn: false,
        },
        awayTeam: {
            teamId: 2, name: 'Away', raceName: 'Orc', side: 'away',
            score: 0, rerolls: 2, rerollUsedThisTurn: false, turnNumber: 1,
            blitzUsedThisTurn: false, passUsedThisTurn: false, foulUsedThisTurn: false,
        },
        players: [
            createPlayer(1, 'home', { x: 5, y: 5 }),
            createPlayer(2, 'home', { x: 6, y: 5 }),
            createPlayer(3, 'away', { x: 13, y: 7 }),
        ],
        ball: { position: { x: 5, y: 5 }, isHeld: true, carrierId: 1 },
        turnoverPending: false,
        kickingTeam: null,
        aiTeam: null,
        weather: 'nice',
    };
}

function createPlayer(id: number, side: 'home' | 'away', pos: { x: number; y: number }): MatchPlayer {
    return {
        id,
        playerId: id,
        name: `Player ${id}`,
        number: id,
        positionalName: 'Lineman',
        stats: { movement: 6, strength: 3, agility: 3, armour: 8 },
        skills: [],
        state: 'standing',
        position: pos,
        hasMoved: false,
        hasActed: false,
        movementRemaining: 6,
        teamSide: side,
    };
}

describe('GameStateManager', () => {
    it('starts with null game state', () => {
        const mgr = new GameStateManager();
        expect(mgr.getGameState()).toBeNull();
    });

    it('stores game state', () => {
        const mgr = new GameStateManager();
        const state = createMinimalGameState();
        mgr.setGameState(state);
        expect(mgr.getGameState()).toBe(state);
    });

    it('notifies listeners on state change', () => {
        const mgr = new GameStateManager();
        const listener = vi.fn();
        mgr.onChange(listener);

        mgr.setGameState(createMinimalGameState());
        expect(listener).toHaveBeenCalledTimes(1);
    });

    it('can unsubscribe listeners', () => {
        const mgr = new GameStateManager();
        const listener = vi.fn();
        const unsub = mgr.onChange(listener);

        unsub();
        mgr.setGameState(createMinimalGameState());
        expect(listener).not.toHaveBeenCalled();
    });

    it('selects a player', () => {
        const mgr = new GameStateManager();
        mgr.setGameState(createMinimalGameState());

        mgr.selectPlayer(1);
        expect(mgr.getSelection().selectedPlayerId).toBe(1);
    });

    it('clears selection', () => {
        const mgr = new GameStateManager();
        mgr.setGameState(createMinimalGameState());

        mgr.selectPlayer(1);
        mgr.clearSelection();
        expect(mgr.getSelection().selectedPlayerId).toBeNull();
        expect(mgr.getSelection().mode).toBe('none');
    });

    it('sets valid targets', () => {
        const mgr = new GameStateManager();
        mgr.setGameState(createMinimalGameState());
        mgr.selectPlayer(1);

        const targets = [{ x: 4, y: 5 }, { x: 5, y: 4 }];
        mgr.setValidTargets('move', targets);

        expect(mgr.getSelection().mode).toBe('move');
        expect(mgr.getSelection().validTargets).toEqual(targets);
    });

    it('finds player at position', () => {
        const mgr = new GameStateManager();
        mgr.setGameState(createMinimalGameState());

        const player = mgr.getPlayerAtPosition({ x: 5, y: 5 });
        expect(player).not.toBeNull();
        expect(player!.id).toBe(1);
    });

    it('returns null for empty position', () => {
        const mgr = new GameStateManager();
        mgr.setGameState(createMinimalGameState());

        expect(mgr.getPlayerAtPosition({ x: 0, y: 0 })).toBeNull();
    });

    it('gets team players', () => {
        const mgr = new GameStateManager();
        mgr.setGameState(createMinimalGameState());

        expect(mgr.getTeamPlayers('home')).toHaveLength(2);
        expect(mgr.getTeamPlayers('away')).toHaveLength(1);
    });

    it('detects player turn', () => {
        const mgr = new GameStateManager();
        const state = createMinimalGameState();
        state.activeTeam = 'home';
        mgr.setGameState(state);
        expect(mgr.isPlayerTurn()).toBe(true);

        state.activeTeam = 'away';
        mgr.setGameState(state);
        expect(mgr.isPlayerTurn()).toBe(false);
    });

    it('updates hover cell', () => {
        const mgr = new GameStateManager();
        mgr.setGameState(createMinimalGameState());

        mgr.setHoveredCell({ x: 10, y: 7 });
        expect(mgr.getSelection().hoveredCell).toEqual({ x: 10, y: 7 });

        mgr.setHoveredCell(null);
        expect(mgr.getSelection().hoveredCell).toBeNull();
    });

    it('clears selection when new game state is set', () => {
        const mgr = new GameStateManager();
        mgr.setGameState(createMinimalGameState());
        mgr.selectPlayer(1);
        mgr.setValidTargets('move', [{ x: 4, y: 5 }]);

        // Setting new state clears selection
        mgr.setGameState(createMinimalGameState());
        expect(mgr.getSelection().selectedPlayerId).toBeNull();
        expect(mgr.getSelection().validTargets).toEqual([]);
    });
});
