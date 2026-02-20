import type { GameState, MatchPlayer, Position } from '../api/types';

export type SelectionMode = 'none' | 'move' | 'block' | 'blitz' | 'pass' | 'handoff' | 'foul' | 'setup' | 'multiple_block';

export interface SelectionState {
    selectedPlayerId: number | null;
    mode: SelectionMode;
    validTargets: Position[];
    hoveredCell: Position | null;
    firstTargetId: number | null;
}

/**
 * Manages the client-side game state and UI selection state.
 * Pure data management - no rendering or DOM interaction.
 */
export class GameStateManager {
    private gameState: GameState | null = null;
    private selection: SelectionState = {
        selectedPlayerId: null,
        mode: 'none',
        validTargets: [],
        hoveredCell: null,
        firstTargetId: null,
    };

    private listeners: Array<() => void> = [];

    getGameState(): GameState | null {
        return this.gameState;
    }

    getSelection(): SelectionState {
        return this.selection;
    }

    /** Update the full game state from the server */
    setGameState(state: GameState): void {
        this.gameState = state;
        this.clearSelection();
        this.notify();
    }

    /** Select a player on the pitch */
    selectPlayer(playerId: number): void {
        if (!this.gameState) return;

        const player = this.getPlayerById(playerId);
        if (!player) return;

        this.selection = {
            selectedPlayerId: playerId,
            mode: 'none',
            validTargets: [],
            hoveredCell: null,
            firstTargetId: null,
        };

        this.notify();
    }

    /** Set valid target cells (e.g., move destinations) */
    setValidTargets(mode: SelectionMode, targets: Position[]): void {
        this.selection = {
            ...this.selection,
            mode,
            validTargets: targets,
        };
        this.notify();
    }

    /** Update hover position */
    setHoveredCell(pos: Position | null): void {
        this.selection = { ...this.selection, hoveredCell: pos };
        this.notify();
    }

    /** Clear all selection state */
    clearSelection(): void {
        this.selection = {
            selectedPlayerId: null,
            mode: 'none',
            validTargets: [],
            hoveredCell: null,
            firstTargetId: null,
        };
    }

    /** Set the first target for multiple block mode */
    setFirstTarget(targetId: number, remainingTargets: Position[]): void {
        this.selection = {
            ...this.selection,
            firstTargetId: targetId,
            validTargets: remainingTargets,
        };
        this.notify();
    }

    /** Get a player by their match player ID */
    getPlayerById(id: number): MatchPlayer | null {
        return this.gameState?.players.find(p => p.id === id) ?? null;
    }

    /** Get the player at a specific grid position */
    getPlayerAtPosition(pos: Position): MatchPlayer | null {
        return this.gameState?.players.find(
            p => p.position && p.position.x === pos.x && p.position.y === pos.y
        ) ?? null;
    }

    /** Get all players for a specific team side */
    getTeamPlayers(side: 'home' | 'away'): MatchPlayer[] {
        return this.gameState?.players.filter(p => p.teamSide === side) ?? [];
    }

    /** Check if it's the player's turn to act (home team is the human player) */
    isPlayerTurn(): boolean {
        return this.gameState?.activeTeam === 'home';
    }

    /** Subscribe to state changes */
    onChange(listener: () => void): () => void {
        this.listeners.push(listener);
        return () => {
            this.listeners = this.listeners.filter(l => l !== listener);
        };
    }

    private notify(): void {
        for (const listener of this.listeners) {
            listener();
        }
    }
}
