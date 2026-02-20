import { PitchRenderer } from './canvas/PitchRenderer';
import { PitchGeometry } from './canvas/PitchGeometry';
import { GameStateManager } from './state/GameStateManager';
import { Tooltip } from './ui/Tooltip';
import { ScoreBoard } from './ui/ScoreBoard';
import { createDemoGameState } from './canvas/DemoState';

/**
 * Main entry point for the Blood Bowl match canvas.
 * Initializes the renderer, state manager, and event handlers.
 */
function init(): void {
    const canvas = document.getElementById('pitch-canvas') as HTMLCanvasElement | null;
    const container = document.getElementById('match-container');

    if (!canvas || !container) {
        console.error('Missing #pitch-canvas or #match-container');
        return;
    }

    // Initialize components
    const cellSize = 32;
    const renderer = new PitchRenderer(canvas, cellSize);
    const stateManager = new GameStateManager();
    const tooltip = new Tooltip(container);
    const scoreBoard = new ScoreBoard(container);

    // Render loop: re-render when state changes
    stateManager.onChange(() => {
        const gameState = stateManager.getGameState();
        if (!gameState) return;

        const selection = stateManager.getSelection();

        renderer.render(
            gameState.players,
            gameState.ball,
            selection.validTargets,
            selection.selectedPlayerId,
            selection.hoveredCell,
        );

        // Draw ball on carrier
        if (gameState.ball?.isHeld && gameState.ball.carrierId) {
            const carrier = stateManager.getPlayerById(gameState.ball.carrierId);
            if (carrier?.position) {
                renderer.drawBallOnPlayer(carrier.position);
            }
        }

        scoreBoard.update(gameState);
    });

    // Mouse move: hover tooltip + cell highlight
    canvas.addEventListener('mousemove', (e) => {
        const rect = canvas.getBoundingClientRect();
        const scaleX = canvas.width / rect.width;
        const scaleY = canvas.height / rect.height;
        const px = (e.clientX - rect.left) * scaleX;
        const py = (e.clientY - rect.top) * scaleY;

        const gridPos = renderer.geometry.pixelToGrid(px, py);
        stateManager.setHoveredCell(gridPos);

        if (gridPos) {
            const player = stateManager.getPlayerAtPosition(gridPos);
            if (player) {
                tooltip.show(player, e.clientX - container.getBoundingClientRect().left, e.clientY - container.getBoundingClientRect().top);
            } else {
                tooltip.hide();
            }
        } else {
            tooltip.hide();
        }
    });

    canvas.addEventListener('mouseleave', () => {
        tooltip.hide();
        stateManager.setHoveredCell(null);
    });

    // Click: select/deselect player
    canvas.addEventListener('click', (e) => {
        const rect = canvas.getBoundingClientRect();
        const scaleX = canvas.width / rect.width;
        const scaleY = canvas.height / rect.height;
        const px = (e.clientX - rect.left) * scaleX;
        const py = (e.clientY - rect.top) * scaleY;

        const gridPos = renderer.geometry.pixelToGrid(px, py);
        if (!gridPos) return;

        const selection = stateManager.getSelection();
        const player = stateManager.getPlayerAtPosition(gridPos);

        if (player && player.teamSide === 'home') {
            // Select home player
            if (selection.selectedPlayerId === player.id) {
                stateManager.clearSelection();
                stateManager.setHoveredCell(null);
            } else {
                stateManager.selectPlayer(player.id);

                // Demo: highlight reachable positions
                if (player.state === 'standing' && !player.hasMoved) {
                    const reachable = getReachablePositions(
                        player.position!,
                        player.stats.movement,
                        renderer.geometry,
                        stateManager.getGameState()!.players,
                    );
                    stateManager.setValidTargets('move', reachable);
                }
            }
        } else if (selection.selectedPlayerId !== null) {
            // Clicked on empty cell or enemy - clear selection
            stateManager.clearSelection();
            stateManager.setHoveredCell(gridPos);
        }
    });

    // Load demo state
    const demoState = createDemoGameState();
    stateManager.setGameState(demoState);
}

/**
 * Simple BFS to find reachable positions (demo, no dodge/tackle zones yet).
 * This will be replaced by the server-side pathfinder in Phase 4.
 */
function getReachablePositions(
    start: { x: number; y: number },
    movement: number,
    geometry: PitchGeometry,
    players: { position: { x: number; y: number } | null }[],
): { x: number; y: number }[] {
    const occupied = new Set<string>();
    for (const p of players) {
        if (p.position) {
            occupied.add(`${p.position.x},${p.position.y}`);
        }
    }

    const visited = new Set<string>();
    const result: { x: number; y: number }[] = [];
    const queue: Array<{ x: number; y: number; dist: number }> = [{ ...start, dist: 0 }];
    visited.add(`${start.x},${start.y}`);

    while (queue.length > 0) {
        const current = queue.shift()!;

        if (current.dist > 0 && !occupied.has(`${current.x},${current.y}`)) {
            result.push({ x: current.x, y: current.y });
        }

        if (current.dist >= movement) continue;

        for (const adj of geometry.getAdjacentPositions(current)) {
            const key = `${adj.x},${adj.y}`;
            if (!visited.has(key) && !occupied.has(key)) {
                visited.add(key);
                queue.push({ ...adj, dist: current.dist + 1 });
            }
        }
    }

    return result;
}

// Initialize when DOM is ready
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
} else {
    init();
}
