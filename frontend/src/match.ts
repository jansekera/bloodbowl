import { ApiClient } from './api/client';
import type { ActionResult, BlockTarget, FoulTarget, HandOffTarget, MoveTarget, PassTarget, Position } from './api/types';
import { PitchRenderer } from './canvas/PitchRenderer';
import { GameStateManager } from './state/GameStateManager';
import { AnimationQueue } from './animation/AnimationQueue';
import { Tooltip } from './ui/Tooltip';
import { ScoreBoard } from './ui/ScoreBoard';
import { ActionPanel } from './ui/ActionPanel';
import { GameLog } from './ui/GameLog';
import { ReservesPanel } from './ui/ReservesPanel';
import { ReplayControls } from './ui/ReplayControls';
import { MatchSummary, buildSummaryData } from './ui/MatchSummary';
import { PhaseBanner } from './ui/PhaseBanner';
import { Toast } from './ui/Toast';
import { LevelUpModal } from './ui/LevelUpModal';

const api = new ApiClient();

// Get matchId from the DOM
const container = document.querySelector('.match-container') as HTMLElement;
const matchId = parseInt(container?.dataset.matchId ?? '0', 10);

if (!matchId) {
    throw new Error('No match ID found');
}

// Initialize components
const canvas = document.getElementById('pitch-canvas') as HTMLCanvasElement;
const renderer = new PitchRenderer(canvas, 32);
const stateManager = new GameStateManager();

const scoreboardEl = document.getElementById('scoreboard')!;
const scoreboard = new ScoreBoard(scoreboardEl);

const logEl = document.getElementById('game-log')!;
const gameLog = new GameLog(logEl);

const actionPanelEl = document.getElementById('action-panel')!;
const actionPanel = new ActionPanel(actionPanelEl, handlePanelAction);

const reservesPanelEl = document.getElementById('reserves-panel')!;
const reservesPanel = new ReservesPanel(reservesPanelEl, handleReservesSelect);

const replayControlsEl = document.getElementById('replay-controls')!;
const replayControls = new ReplayControls(replayControlsEl);

const tooltipEl = document.getElementById('tooltip')!;
const tooltip = new Tooltip(tooltipEl.parentElement!);

const matchSummary = new MatchSummary(container);
const phaseBanner = new PhaseBanner(container);
const toast = new Toast(container);

const levelUpEl = document.createElement('div');
container.appendChild(levelUpEl);
const levelUpModal = new LevelUpModal(levelUpEl);

// Animation queue
const animationQueue = new AnimationQueue(
    () => renderFrame(),
    (pos: Position) => renderer.geometry.gridToPixel(pos),
);
renderer.setAnimationState(animationQueue.state);

function renderFrame(): void {
    const state = stateManager.getGameState();
    if (!state) return;
    const sel = stateManager.getSelection();

    // Use risk highlights for move mode, regular highlights for others
    const useRisk = sel.mode === 'move' && moveTargets.length > 0;
    renderer.render(
        state.players,
        state.ball,
        useRisk ? [] : sel.validTargets,
        sel.selectedPlayerId,
        sel.hoveredCell,
    );
    if (useRisk) {
        renderer.drawRiskHighlights(moveTargets);
    }
    if (state.phase === 'setup') {
        renderer.drawSetupZone(state.activeTeam);
    }
    if (state.ball.isHeld && state.ball.carrierId) {
        const carrier = state.players.find(p => p.id === state.ball.carrierId);
        if (carrier?.position) {
            renderer.drawBallOnPlayer(carrier.position);
        }
    }
    // Draw selected-target indicator for multiple block first target
    if (sel.firstTargetId !== null) {
        const firstTarget = state.players.find(p => p.id === sel.firstTargetId);
        if (firstTarget?.position) {
            renderer.drawSelectedTarget(firstTarget.position);
        }
    }
}

let lastPhase = '';

/** Process action result: play animations then update state */
async function handleActionResult(result: ActionResult): Promise<void> {
    await animationQueue.playEvents(result.events);
    gameLog.addEvents(result.events);

    // Phase banners
    const newPhase = result.state.phase;
    if (newPhase !== lastPhase) {
        if (newPhase === 'half_time') {
            await phaseBanner.show('HALF TIME', 1500);
        } else if (newPhase === 'touchdown') {
            const tdEvent = result.events.find(e => e.type === 'touchdown');
            const scorer = tdEvent?.data?.['playerName'] ?? '';
            await phaseBanner.show(scorer ? `TOUCHDOWN! ${scorer}` : 'TOUCHDOWN!', 1500);
        } else if (newPhase === 'game_over') {
            matchSummary.show(buildSummaryData(result.state));
        }
        lastPhase = newPhase;
    }

    stateManager.setGameState(result.state);
}

let moveTargets: MoveTarget[] = [];
let blockTargets: BlockTarget[] = [];
let passTargets: PassTarget[] = [];
let handoffTargets: HandOffTarget[] = [];
let foulTargets: FoulTarget[] = [];
let setupSelectedPlayerId: number | null = null;
let isProcessing = false;

// Skip animations on click during animation
canvas.addEventListener('dblclick', () => {
    if (animationQueue.isAnimating) {
        animationQueue.skip();
    }
});

// State change handler
stateManager.onChange(() => {
    const state = stateManager.getGameState();
    if (!state) return;

    const sel = stateManager.getSelection();

    // Use risk highlights for move mode
    const useRisk = sel.mode === 'move' && moveTargets.length > 0;
    renderer.render(
        state.players,
        state.ball,
        useRisk ? [] : sel.validTargets,
        sel.selectedPlayerId,
        sel.hoveredCell,
    );
    if (useRisk) {
        renderer.drawRiskHighlights(moveTargets);
    }

    // Draw setup zone overlay
    if (state.phase === 'setup') {
        renderer.drawSetupZone(state.activeTeam);
    }

    // Draw ball on carrier
    if (state.ball.isHeld && state.ball.carrierId) {
        const carrier = state.players.find(p => p.id === state.ball.carrierId);
        if (carrier?.position) {
            renderer.drawBallOnPlayer(carrier.position);
        }
    }

    // Draw selected-target indicator for multiple block first target
    if (sel.firstTargetId !== null) {
        const firstTarget = state.players.find(p => p.id === sel.firstTargetId);
        if (firstTarget?.position) {
            renderer.drawSelectedTarget(firstTarget.position);
        }
    }

    scoreboard.update(state);
    actionPanel.update(state, stateManager.isPlayerTurn());
    reservesPanel.update(state, stateManager.isPlayerTurn());
});

// Canvas mouse events
canvas.addEventListener('click', async (e: MouseEvent) => {
    if (isProcessing) return;

    const state = stateManager.getGameState();
    if (!state) return;

    const rect = canvas.getBoundingClientRect();
    const x = e.clientX - rect.left;
    const y = e.clientY - rect.top;
    const gridPos = renderer.geometry.pixelToGrid(x, y);
    if (!gridPos) return;

    const sel = stateManager.getSelection();
    const mode = actionPanel.getSelectedMode();

    // Setup mode: click pitch to place selected player
    if (state.phase === 'setup' && setupSelectedPlayerId !== null && stateManager.isPlayerTurn()) {
        await submitSetupPlayer(setupSelectedPlayerId, gridPos.x, gridPos.y);
        return;
    }

    // Multiple Block mode: two-target selection
    if (mode === 'multiple_block') {
        if (sel.selectedPlayerId) {
            const target = blockTargets.find(t => t.x === gridPos.x && t.y === gridPos.y);
            if (target) {
                if (sel.firstTargetId === null) {
                    // First target selected — store it and show remaining targets
                    const remaining = blockTargets.filter(t => t.playerId !== target.playerId);
                    stateManager.setFirstTarget(
                        target.playerId,
                        remaining.map(t => ({ x: t.x, y: t.y })),
                    );
                    actionPanel.setFirstTargetSelected(true);
                    actionPanel.update(state, stateManager.isPlayerTurn());
                    return;
                } else {
                    // Second target selected — submit
                    await submitMultipleBlock(sel.selectedPlayerId, sel.firstTargetId, target.playerId);
                    return;
                }
            }
        }

        // Select a player for multiple block
        const player = stateManager.getPlayerAtPosition(gridPos);
        if (player && player.teamSide === state.activeTeam && stateManager.isPlayerTurn()) {
            if (player.state === 'standing' && !player.hasActed && player.skills.includes('Multiple Block')) {
                stateManager.selectPlayer(player.id);
                try {
                    blockTargets = await api.getBlockTargets(matchId, player.id);
                    if (blockTargets.length >= 2) {
                        stateManager.setValidTargets(
                            'multiple_block',
                            blockTargets.map(t => ({ x: t.x, y: t.y })),
                        );
                    } else {
                        blockTargets = [];
                        toast.error('Need 2+ adjacent enemies for Multiple Block');
                    }
                } catch {
                    blockTargets = [];
                }
            }
        }
        return;
    }

    // Block/Blitz mode: clicking an enemy player submits block/blitz
    if (mode === 'block' || mode === 'blitz') {
        if (sel.selectedPlayerId) {
            const target = blockTargets.find(t => t.x === gridPos.x && t.y === gridPos.y);
            if (target) {
                await submitBlock(sel.selectedPlayerId, target.playerId, mode);
                return;
            }
        }

        // Select a player for block/blitz
        const player = stateManager.getPlayerAtPosition(gridPos);
        if (player && player.teamSide === state.activeTeam && stateManager.isPlayerTurn()) {
            if (player.state === 'standing' && !player.hasActed) {
                stateManager.selectPlayer(player.id);
                // Fetch block targets
                try {
                    blockTargets = await api.getBlockTargets(matchId, player.id);
                    stateManager.setValidTargets(
                        mode === 'block' ? 'block' : 'blitz',
                        blockTargets.map(t => ({ x: t.x, y: t.y })),
                    );
                } catch {
                    blockTargets = [];
                }
            }
        }
        return;
    }

    // Pass mode: select ball carrier then click a target square
    if (mode === 'pass') {
        if (sel.selectedPlayerId) {
            const target = passTargets.find(t => t.x === gridPos.x && t.y === gridPos.y);
            if (target) {
                await submitPass(sel.selectedPlayerId, gridPos.x, gridPos.y);
                return;
            }
        }

        // Select ball carrier for pass
        const player = stateManager.getPlayerAtPosition(gridPos);
        if (player && player.teamSide === state.activeTeam && stateManager.isPlayerTurn()) {
            if (player.state === 'standing' && !player.hasActed && state.ball.carrierId === player.id) {
                stateManager.selectPlayer(player.id);
                try {
                    passTargets = await api.getPassTargets(matchId, player.id);
                    stateManager.setValidTargets(
                        'pass',
                        passTargets.map(t => ({ x: t.x, y: t.y })),
                    );
                } catch {
                    passTargets = [];
                }
            }
        }
        return;
    }

    // Hand-off mode: select ball carrier then click adjacent teammate
    if (mode === 'handoff') {
        if (sel.selectedPlayerId) {
            const target = handoffTargets.find(t => t.x === gridPos.x && t.y === gridPos.y);
            if (target) {
                await submitHandOff(sel.selectedPlayerId, target.playerId);
                return;
            }
        }

        // Select ball carrier for hand-off
        const player = stateManager.getPlayerAtPosition(gridPos);
        if (player && player.teamSide === state.activeTeam && stateManager.isPlayerTurn()) {
            if (player.state === 'standing' && !player.hasActed && state.ball.carrierId === player.id) {
                stateManager.selectPlayer(player.id);
                try {
                    handoffTargets = await api.getHandOffTargets(matchId, player.id);
                    stateManager.setValidTargets(
                        'handoff',
                        handoffTargets.map(t => ({ x: t.x, y: t.y })),
                    );
                } catch {
                    handoffTargets = [];
                }
            }
        }
        return;
    }

    // Foul mode: select a standing player then click adjacent prone/stunned enemy
    if (mode === 'foul') {
        if (sel.selectedPlayerId) {
            const target = foulTargets.find(t => t.x === gridPos.x && t.y === gridPos.y);
            if (target) {
                await submitFoul(sel.selectedPlayerId, target.playerId);
                return;
            }
        }

        // Select a player for foul
        const player = stateManager.getPlayerAtPosition(gridPos);
        if (player && player.teamSide === state.activeTeam && stateManager.isPlayerTurn()) {
            if (player.state === 'standing' && !player.hasActed) {
                stateManager.selectPlayer(player.id);
                try {
                    foulTargets = await api.getFoulTargets(matchId, player.id);
                    stateManager.setValidTargets(
                        'foul',
                        foulTargets.map(t => ({ x: t.x, y: t.y })),
                    );
                } catch {
                    foulTargets = [];
                }
            }
        }
        return;
    }

    // Move mode: if we have a selected player and valid targets, try to move
    if (sel.selectedPlayerId && sel.mode === 'move') {
        const isTarget = moveTargets.some(t => t.x === gridPos.x && t.y === gridPos.y);
        if (isTarget) {
            await submitMove(sel.selectedPlayerId, gridPos.x, gridPos.y);
            return;
        }
    }

    // Default: try to select a player for movement
    const player = stateManager.getPlayerAtPosition(gridPos);

    if (player && player.teamSide === state.activeTeam && stateManager.isPlayerTurn()) {
        stateManager.selectPlayer(player.id);

        if (!player.hasMoved && (player.state === 'standing' || player.state === 'prone')) {
            try {
                moveTargets = await api.getValidMoves(matchId, player.id);
                stateManager.setValidTargets(
                    'move',
                    moveTargets.map(t => ({ x: t.x, y: t.y })),
                );
            } catch {
                moveTargets = [];
            }
        }
    } else {
        stateManager.clearSelection();
        clearAllTargets();
        stateManager.setGameState(state);
    }
});

canvas.addEventListener('mousemove', (e: MouseEvent) => {
    const rect = canvas.getBoundingClientRect();
    const x = e.clientX - rect.left;
    const y = e.clientY - rect.top;
    const gridPos = renderer.geometry.pixelToGrid(x, y);

    stateManager.setHoveredCell(gridPos);

    if (gridPos) {
        const player = stateManager.getPlayerAtPosition(gridPos);
        if (player) {
            tooltip.show(player, e.clientX, e.clientY);
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

function clearAllTargets(): void {
    moveTargets = [];
    blockTargets = [];
    passTargets = [];
    handoffTargets = [];
    foulTargets = [];
}

async function submitMove(playerId: number, x: number, y: number): Promise<void> {
    isProcessing = true;
    try {
        const result = await api.submitAction(matchId, 'move', { playerId, x, y });
        await handleActionResult(result);
        clearAllTargets();
    } catch (err) {
        toast.error('Move failed');
    } finally {
        isProcessing = false;
    }
}

async function submitBlock(playerId: number, targetId: number, action: string): Promise<void> {
    isProcessing = true;
    try {
        const result = await api.submitAction(matchId, action, { playerId, targetId });
        await handleActionResult(result);
        clearAllTargets();
        actionPanel.setSelectedMode(null);
    } catch (err) {
        toast.error('Block failed');
    } finally {
        isProcessing = false;
    }
}

async function submitMultipleBlock(playerId: number, targetId: number, targetId2: number): Promise<void> {
    isProcessing = true;
    try {
        const result = await api.submitAction(matchId, 'multiple_block', { playerId, targetId, targetId2 });
        await handleActionResult(result);
        clearAllTargets();
        actionPanel.setSelectedMode(null);
    } catch (err) {
        toast.error('Multiple Block failed');
    } finally {
        isProcessing = false;
    }
}

async function submitPass(playerId: number, targetX: number, targetY: number): Promise<void> {
    isProcessing = true;
    try {
        const result = await api.submitAction(matchId, 'pass', { playerId, targetX, targetY });
        await handleActionResult(result);
        clearAllTargets();
        actionPanel.setSelectedMode(null);
    } catch (err) {
        toast.error('Pass failed');
    } finally {
        isProcessing = false;
    }
}

async function submitHandOff(playerId: number, targetId: number): Promise<void> {
    isProcessing = true;
    try {
        const result = await api.submitAction(matchId, 'hand_off', { playerId, targetId });
        await handleActionResult(result);
        clearAllTargets();
        actionPanel.setSelectedMode(null);
    } catch (err) {
        toast.error('Hand-off failed');
    } finally {
        isProcessing = false;
    }
}

async function submitFoul(playerId: number, targetId: number): Promise<void> {
    isProcessing = true;
    try {
        const result = await api.submitAction(matchId, 'foul', { playerId, targetId });
        await handleActionResult(result);
        clearAllTargets();
        actionPanel.setSelectedMode(null);
    } catch (err) {
        toast.error('Foul failed');
    } finally {
        isProcessing = false;
    }
}

function handleReservesSelect(playerId: number): void {
    setupSelectedPlayerId = playerId;
    reservesPanel.setSelectedPlayer(playerId);
}

async function submitSetupPlayer(playerId: number, x: number, y: number): Promise<void> {
    isProcessing = true;
    try {
        const result = await api.submitAction(matchId, 'setup_player', { playerId, x, y });
        gameLog.addEvents(result.events);
        stateManager.setGameState(result.state);
        setupSelectedPlayerId = null;
        reservesPanel.setSelectedPlayer(null);
    } catch (err) {
        toast.error('Setup failed');
    } finally {
        isProcessing = false;
    }
}


async function handlePanelAction(action: string): Promise<void> {
    // Toggle mode buttons
    const toggleModes: Record<string, string> = {
        toggle_block: 'block',
        toggle_multiple_block: 'multiple_block',
        toggle_blitz: 'blitz',
        toggle_pass: 'pass',
        toggle_handoff: 'handoff',
        toggle_foul: 'foul',
    };

    if (action in toggleModes) {
        const targetMode = toggleModes[action];
        const current = actionPanel.getSelectedMode();
        actionPanel.setSelectedMode(current === targetMode ? null : targetMode);
        stateManager.clearSelection();
        clearAllTargets();
        const state = stateManager.getGameState();
        if (state) stateManager.setGameState(state); // re-render
        return;
    }

    if (isProcessing) return;
    isProcessing = true;
    try {
        const result = await api.submitAction(matchId, action);
        await handleActionResult(result);
        clearAllTargets();
        actionPanel.setSelectedMode(null);
    } catch (err) {
        toast.error('Action failed');
    } finally {
        isProcessing = false;
    }
}

// Keyboard shortcuts
document.addEventListener('keydown', (e: KeyboardEvent) => {
    const state = stateManager.getGameState();
    if (!state) return;
    if (isProcessing || animationQueue.isAnimating) return;
    if (!stateManager.isPlayerTurn()) return;

    const key = e.key.toLowerCase();

    // End Turn: Space or Enter (only during PLAY phase)
    if ((key === ' ' || key === 'enter') && state.phase === 'play') {
        e.preventDefault();
        handlePanelAction('end_turn');
        return;
    }

    // Cancel selection: Escape
    if (key === 'escape') {
        e.preventDefault();
        stateManager.clearSelection();
        clearAllTargets();
        actionPanel.setSelectedMode(null);
        if (state) stateManager.setGameState(state);
        return;
    }

    // Mode toggles (only during PLAY phase)
    if (state.phase !== 'play') return;

    const modeKeys: Record<string, string> = {
        b: 'block',
        m: 'multiple_block',
        z: 'blitz',
        p: 'pass',
        h: 'handoff',
        f: 'foul',
    };

    if (key in modeKeys) {
        e.preventDefault();
        const targetMode = modeKeys[key];
        const current = actionPanel.getSelectedMode();
        actionPanel.setSelectedMode(current === targetMode ? null : targetMode);
        stateManager.clearSelection();
        clearAllTargets();
        if (state) stateManager.setGameState(state);
    }
});

// Replay: load all events for finished games
async function initReplay(): Promise<void> {
    try {
        const events = await api.getAllEvents(matchId);
        if (events.length === 0) return;

        replayControlsEl.style.display = '';
        replayControls.setEvents(events);
        replayControls.setOnStep((event) => {
            gameLog.addEvents([event]);
        });
    } catch (err) {
        toast.error('Failed to load replay');
    }
}

/** Check for level-ups on game over and show modal for each eligible player */
async function checkLevelUps(): Promise<void> {
    const state = stateManager.getGameState();
    if (!state || state.phase !== 'game_over') return;

    // Collect unique player IDs from both teams
    const playerIds = state.players
        .filter(p => p.playerId > 0)
        .map(p => p.playerId);

    const uniqueIds = [...new Set(playerIds)];

    for (const playerId of uniqueIds) {
        try {
            const skills = await api.getAvailableSkills(playerId);
            if (!skills.can_advance) continue;
            if (skills.normal.length === 0 && skills.double.length === 0) continue;

            const player = state.players.find(p => p.playerId === playerId);
            const playerName = player?.name ?? `Player #${playerId}`;

            await new Promise<void>((resolve) => {
                levelUpModal.show(
                    playerName,
                    skills,
                    async (skillId) => {
                        try {
                            await api.advancePlayer(playerId, skillId);
                            toast.success(`${playerName} learned a new skill!`);
                        } catch {
                            toast.error('Failed to advance player');
                        }
                        resolve();
                    },
                    () => resolve(),
                );
            });
        } catch {
            // Player not eligible, skip silently
        }
    }
}

// Load initial state
async function init(): Promise<void> {
    try {
        const state = await api.getMatchState(matchId);
        stateManager.setGameState(state);

        // Initialize replay controls for finished games
        if (state.phase === 'game_over') {
            initReplay();
            checkLevelUps();
        }
    } catch (err) {
        toast.error('Failed to load match state');
    }
}

init();
