import type { GameState } from '../api/types';

/**
 * Creates a demo game state for testing the renderer before the game engine is built.
 */
export function createDemoGameState(): GameState {
    return {
        matchId: 0,
        half: 1,
        activeTeam: 'home',
        phase: 'play',
        homeTeam: {
            teamId: 1,
            name: 'Reikland Reavers',
            raceName: 'Human',
            side: 'home',
            score: 1,
            rerolls: 3,
            rerollUsedThisTurn: false,
            turnNumber: 3,
            blitzUsedThisTurn: false,
            passUsedThisTurn: false,
            foulUsedThisTurn: false,
        },
        awayTeam: {
            teamId: 2,
            name: 'Gouged Eye',
            raceName: 'Orc',
            side: 'away',
            score: 0,
            rerolls: 2,
            rerollUsedThisTurn: false,
            turnNumber: 3,
            blitzUsedThisTurn: false,
            passUsedThisTurn: false,
            foulUsedThisTurn: false,
        },
        players: [
            // Home team (Human) - blue
            { id: 1, playerId: 1, name: 'Karl', number: 1, positionalName: 'Lineman', stats: { movement: 6, strength: 3, agility: 3, armour: 8 }, skills: [], state: 'standing', position: { x: 12, y: 5 }, hasMoved: false, hasActed: false, movementRemaining: 6, teamSide: 'home' },
            { id: 2, playerId: 2, name: 'Franz', number: 2, positionalName: 'Lineman', stats: { movement: 6, strength: 3, agility: 3, armour: 8 }, skills: [], state: 'standing', position: { x: 12, y: 7 }, hasMoved: false, hasActed: false, movementRemaining: 6, teamSide: 'home' },
            { id: 3, playerId: 3, name: 'Heinrich', number: 3, positionalName: 'Lineman', stats: { movement: 6, strength: 3, agility: 3, armour: 8 }, skills: [], state: 'standing', position: { x: 12, y: 9 }, hasMoved: false, hasActed: false, movementRemaining: 6, teamSide: 'home' },
            { id: 4, playerId: 4, name: 'Griff', number: 7, positionalName: 'Blitzer', stats: { movement: 7, strength: 3, agility: 3, armour: 8 }, skills: ['Block'], state: 'standing', position: { x: 10, y: 4 }, hasMoved: true, hasActed: false, movementRemaining: 0, teamSide: 'home' },
            { id: 5, playerId: 5, name: 'Wolfgang', number: 8, positionalName: 'Blitzer', stats: { movement: 7, strength: 3, agility: 3, armour: 8 }, skills: ['Block'], state: 'standing', position: { x: 10, y: 10 }, hasMoved: false, hasActed: false, movementRemaining: 7, teamSide: 'home' },
            { id: 6, playerId: 6, name: 'Hans', number: 9, positionalName: 'Catcher', stats: { movement: 8, strength: 2, agility: 3, armour: 7 }, skills: ['Dodge', 'Catch'], state: 'standing', position: { x: 8, y: 2 }, hasMoved: false, hasActed: false, movementRemaining: 8, teamSide: 'home' },
            { id: 7, playerId: 7, name: 'Otto', number: 10, positionalName: 'Thrower', stats: { movement: 6, strength: 3, agility: 3, armour: 8 }, skills: ['Sure Hands'], state: 'standing', position: { x: 7, y: 7 }, hasMoved: false, hasActed: false, movementRemaining: 6, teamSide: 'home' },
            { id: 8, playerId: 8, name: 'Fritz', number: 4, positionalName: 'Lineman', stats: { movement: 6, strength: 3, agility: 3, armour: 8 }, skills: [], state: 'prone', position: { x: 11, y: 6 }, hasMoved: false, hasActed: false, movementRemaining: 0, teamSide: 'home' },

            // Away team (Orc) - red
            { id: 9, playerId: 9, name: 'Grishnak', number: 1, positionalName: 'Lineman', stats: { movement: 5, strength: 3, agility: 3, armour: 9 }, skills: [], state: 'standing', position: { x: 13, y: 5 }, hasMoved: false, hasActed: false, movementRemaining: 5, teamSide: 'away' },
            { id: 10, playerId: 10, name: 'Ugluk', number: 2, positionalName: 'Lineman', stats: { movement: 5, strength: 3, agility: 3, armour: 9 }, skills: [], state: 'standing', position: { x: 13, y: 7 }, hasMoved: false, hasActed: false, movementRemaining: 5, teamSide: 'away' },
            { id: 11, playerId: 11, name: 'Bolg', number: 3, positionalName: 'Lineman', stats: { movement: 5, strength: 3, agility: 3, armour: 9 }, skills: [], state: 'standing', position: { x: 13, y: 9 }, hasMoved: false, hasActed: false, movementRemaining: 5, teamSide: 'away' },
            { id: 12, playerId: 12, name: 'Varag', number: 5, positionalName: 'Blitzer', stats: { movement: 6, strength: 3, agility: 3, armour: 9 }, skills: ['Block'], state: 'standing', position: { x: 15, y: 6 }, hasMoved: false, hasActed: false, movementRemaining: 6, teamSide: 'away' },
            { id: 13, playerId: 13, name: 'Azog', number: 6, positionalName: 'Blitzer', stats: { movement: 6, strength: 3, agility: 3, armour: 9 }, skills: ['Block'], state: 'standing', position: { x: 15, y: 8 }, hasMoved: false, hasActed: false, movementRemaining: 6, teamSide: 'away' },
            { id: 14, playerId: 14, name: 'Muzgash', number: 7, positionalName: 'Black Orc', stats: { movement: 4, strength: 4, agility: 2, armour: 9 }, skills: [], state: 'standing', position: { x: 14, y: 4 }, hasMoved: false, hasActed: false, movementRemaining: 4, teamSide: 'away' },
            { id: 15, playerId: 15, name: 'Gorgut', number: 8, positionalName: 'Black Orc', stats: { movement: 4, strength: 4, agility: 2, armour: 9 }, skills: [], state: 'standing', position: { x: 14, y: 10 }, hasMoved: false, hasActed: false, movementRemaining: 4, teamSide: 'away' },
            { id: 16, playerId: 16, name: 'Snaglak', number: 4, positionalName: 'Lineman', stats: { movement: 5, strength: 3, agility: 3, armour: 9 }, skills: [], state: 'stunned', position: { x: 13, y: 11 }, hasMoved: false, hasActed: false, movementRemaining: 0, teamSide: 'away' },
        ],
        ball: {
            position: { x: 7, y: 7 },
            isHeld: true,
            carrierId: 7,
        },
        turnoverPending: false,
        kickingTeam: null,
        aiTeam: null,
        weather: 'nice',
    };
}
