import type { Position } from '../api/types';

/** Base animation interface */
export interface Animation {
    /** Duration in milliseconds */
    duration: number;
    /** Called each frame with progress 0..1 */
    update(progress: number): void;
    /** Called when animation starts */
    start?(): void;
    /** Called when animation finishes */
    finish?(): void;
}

/** Easing function: ease-in-out cubic */
function easeInOutCubic(t: number): number {
    return t < 0.5 ? 4 * t * t * t : 1 - Math.pow(-2 * t + 2, 3) / 2;
}

/** Lerp between two values */
function lerp(a: number, b: number, t: number): number {
    return a + (b - a) * t;
}

/** Animated player position override (used by renderer) */
export interface AnimatedOverride {
    playerId: number;
    px: number;
    py: number;
}

/** Animated ball position override */
export interface BallOverride {
    px: number;
    py: number;
}

/** State shared between animations and renderer */
export interface AnimationState {
    playerOverrides: Map<number, { px: number; py: number }>;
    ballOverride: { px: number; py: number } | null;
    flashCell: { pos: Position; color: string } | null;
    shakePlayer: { playerId: number; offsetX: number; offsetY: number } | null;
}

export function createAnimationState(): AnimationState {
    return {
        playerOverrides: new Map(),
        ballOverride: null,
        flashCell: null,
        shakePlayer: null,
    };
}

/** Smooth player movement from one cell to another */
export function createMoveAnimation(
    state: AnimationState,
    playerId: number,
    from: Position,
    to: Position,
    gridToPixel: (pos: Position) => { px: number; py: number },
    duration = 200,
): Animation {
    const fromPx = gridToPixel(from);
    const toPx = gridToPixel(to);

    return {
        duration,
        start() {
            state.playerOverrides.set(playerId, fromPx);
        },
        update(progress: number) {
            const t = easeInOutCubic(progress);
            state.playerOverrides.set(playerId, {
                px: lerp(fromPx.px, toPx.px, t),
                py: lerp(fromPx.py, toPx.py, t),
            });
        },
        finish() {
            state.playerOverrides.delete(playerId);
        },
    };
}

/** Player shake effect (for blocks, knockdowns) */
export function createShakeAnimation(
    state: AnimationState,
    playerId: number,
    intensity = 4,
    duration = 300,
): Animation {
    return {
        duration,
        start() {
            state.shakePlayer = { playerId, offsetX: 0, offsetY: 0 };
        },
        update(progress: number) {
            const decay = 1 - progress;
            const freq = progress * 20;
            state.shakePlayer = {
                playerId,
                offsetX: Math.sin(freq) * intensity * decay,
                offsetY: Math.cos(freq * 1.3) * intensity * decay,
            };
        },
        finish() {
            state.shakePlayer = null;
        },
    };
}

/** Ball arc animation (for passes, kicks) */
export function createBallArcAnimation(
    state: AnimationState,
    from: Position,
    to: Position,
    gridToPixel: (pos: Position) => { px: number; py: number },
    arcHeight = 40,
    duration = 500,
): Animation {
    const fromPx = gridToPixel(from);
    const toPx = gridToPixel(to);

    return {
        duration,
        start() {
            state.ballOverride = fromPx;
        },
        update(progress: number) {
            const t = easeInOutCubic(progress);
            const arc = Math.sin(t * Math.PI) * arcHeight;
            state.ballOverride = {
                px: lerp(fromPx.px, toPx.px, t),
                py: lerp(fromPx.py, toPx.py, t) - arc,
            };
        },
        finish() {
            state.ballOverride = null;
        },
    };
}

/** Ball bounce animation (scatter) */
export function createBallBounceAnimation(
    state: AnimationState,
    from: Position,
    to: Position,
    gridToPixel: (pos: Position) => { px: number; py: number },
    duration = 150,
): Animation {
    const fromPx = gridToPixel(from);
    const toPx = gridToPixel(to);

    return {
        duration,
        start() {
            state.ballOverride = fromPx;
        },
        update(progress: number) {
            const t = easeInOutCubic(progress);
            const bounce = Math.abs(Math.sin(t * Math.PI)) * 10;
            state.ballOverride = {
                px: lerp(fromPx.px, toPx.px, t),
                py: lerp(fromPx.py, toPx.py, t) - bounce,
            };
        },
        finish() {
            state.ballOverride = null;
        },
    };
}

/** Flash a cell with color (for dice results, knockdowns) */
export function createFlashAnimation(
    state: AnimationState,
    pos: Position,
    color: string,
    duration = 400,
): Animation {
    return {
        duration,
        update(progress: number) {
            const alpha = Math.max(0, 1 - progress);
            state.flashCell = { pos, color: color.replace(/[\d.]+\)$/, `${alpha})`) };
        },
        finish() {
            state.flashCell = null;
        },
    };
}

/** Delay animation (pause between steps) */
export function createDelayAnimation(duration = 100): Animation {
    return {
        duration,
        update() {},
    };
}
