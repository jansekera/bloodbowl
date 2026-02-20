import { describe, it, expect, vi, beforeEach } from 'vitest';
import { AnimationQueue } from './AnimationQueue';
import type { GameEvent } from '../api/types';
import { createAnimationState, createDelayAnimation, createMoveAnimation } from './animations';

describe('AnimationQueue', () => {
    let renderCallback: ReturnType<typeof vi.fn>;
    let queue: AnimationQueue;

    beforeEach(() => {
        renderCallback = vi.fn();
        queue = new AnimationQueue(
            renderCallback,
            (pos) => ({ px: pos.x * 32 + 56, py: pos.y * 32 + 56 }),
        );
    });

    it('should start with isAnimating false', () => {
        expect(queue.isAnimating).toBe(false);
    });

    it('should have an animation state', () => {
        expect(queue.state).toBeDefined();
        expect(queue.state.playerOverrides).toBeInstanceOf(Map);
    });

    it('should resolve immediately for empty events', async () => {
        await queue.playEvents([]);
        expect(queue.isAnimating).toBe(false);
    });

    it('should parse player_move events', async () => {
        // Use a short timeout to avoid blocking test
        const events: GameEvent[] = [
            {
                type: 'player_move',
                description: 'Player moved',
                data: { playerId: 1, from: '5,7', to: '6,7' },
            },
        ];

        // Mock requestAnimationFrame to run synchronously
        let rafCallback: FrameRequestCallback | null = null;
        vi.stubGlobal('requestAnimationFrame', (cb: FrameRequestCallback) => {
            rafCallback = cb;
            return 1;
        });
        vi.stubGlobal('cancelAnimationFrame', vi.fn());

        const promise = queue.playEvents(events);
        expect(queue.isAnimating).toBe(true);

        // Simulate animation completion
        queue.skip();
        await promise;

        expect(queue.isAnimating).toBe(false);

        vi.unstubAllGlobals();
    });

    it('should skip all animations', async () => {
        const events: GameEvent[] = [
            {
                type: 'player_move',
                description: 'Player moved',
                data: { playerId: 1, from: '5,7', to: '6,7' },
            },
            {
                type: 'block',
                description: 'Block!',
                data: { targetId: 2 },
            },
        ];

        vi.stubGlobal('requestAnimationFrame', (cb: FrameRequestCallback) => 1);
        vi.stubGlobal('cancelAnimationFrame', vi.fn());

        const promise = queue.playEvents(events);
        queue.skip();
        await promise;

        expect(queue.isAnimating).toBe(false);
        expect(queue.state.playerOverrides.size).toBe(0);
        expect(queue.state.shakePlayer).toBeNull();

        vi.unstubAllGlobals();
    });

    it('should resolve for non-animated events', async () => {
        const events: GameEvent[] = [
            {
                type: 'armour_roll',
                description: 'Armor check',
                data: { playerId: 1 },
            },
        ];

        // No animations produced = resolves immediately
        await queue.playEvents(events);
        expect(queue.isAnimating).toBe(false);
    });
});

describe('animations', () => {
    it('createMoveAnimation sets and clears overrides', () => {
        const state = createAnimationState();
        const gridToPixel = (pos: { x: number; y: number }) => ({
            px: pos.x * 32 + 56,
            py: pos.y * 32 + 56,
        });

        const anim = createMoveAnimation(
            state,
            42,
            { x: 5, y: 7 },
            { x: 6, y: 7 },
            gridToPixel,
            200,
        );

        expect(anim.duration).toBe(200);

        // Start
        anim.start!();
        expect(state.playerOverrides.has(42)).toBe(true);

        // Mid-animation
        anim.update(0.5);
        const mid = state.playerOverrides.get(42)!;
        expect(mid.px).toBeGreaterThan(5 * 32 + 56);
        expect(mid.px).toBeLessThan(6 * 32 + 56);

        // End
        anim.update(1);
        anim.finish!();
        expect(state.playerOverrides.has(42)).toBe(false);
    });

    it('createDelayAnimation does nothing but wait', () => {
        const anim = createDelayAnimation(100);
        expect(anim.duration).toBe(100);
        anim.update(0.5); // should not throw
    });
});

describe('New event animations (Phase 12)', () => {
    let renderCallback: ReturnType<typeof vi.fn>;
    let queue: AnimationQueue;

    beforeEach(() => {
        renderCallback = vi.fn();
        queue = new AnimationQueue(
            renderCallback,
            (pos) => ({ px: pos.x * 32 + 56, py: pos.y * 32 + 56 }),
        );
    });

    it('should produce shake animations for wrestle events', async () => {
        const events: GameEvent[] = [
            {
                type: 'wrestle',
                description: 'Wrestle!',
                data: { attackerId: 1, defenderId: 2 },
            },
        ];

        vi.stubGlobal('requestAnimationFrame', (cb: FrameRequestCallback) => 1);
        vi.stubGlobal('cancelAnimationFrame', vi.fn());

        const promise = queue.playEvents(events);
        expect(queue.isAnimating).toBe(true);

        // Shake should be active
        queue.skip();
        await promise;

        expect(queue.isAnimating).toBe(false);
        vi.unstubAllGlobals();
    });

    it('should produce shake animations for big guy failure events', async () => {
        const events: GameEvent[] = [
            {
                type: 'bone_head',
                description: 'Bone-head!',
                data: { playerId: 1, success: false },
            },
            {
                type: 'wild_animal',
                description: 'Wild Animal!',
                data: { playerId: 2, success: false },
            },
        ];

        vi.stubGlobal('requestAnimationFrame', (cb: FrameRequestCallback) => 1);
        vi.stubGlobal('cancelAnimationFrame', vi.fn());

        const promise = queue.playEvents(events);
        expect(queue.isAnimating).toBe(true);

        queue.skip();
        await promise;

        expect(queue.isAnimating).toBe(false);
        vi.unstubAllGlobals();
    });

    it('should produce delay for dump_off and hail_mary_pass events', async () => {
        const events: GameEvent[] = [
            {
                type: 'dump_off',
                description: 'Dump-Off!',
                data: { playerId: 1 },
            },
            {
                type: 'hail_mary_pass',
                description: 'Hail Mary Pass!',
                data: { playerId: 1, from: '5,5', to: '20,10' },
            },
        ];

        vi.stubGlobal('requestAnimationFrame', (cb: FrameRequestCallback) => 1);
        vi.stubGlobal('cancelAnimationFrame', vi.fn());

        const promise = queue.playEvents(events);
        expect(queue.isAnimating).toBe(true);

        queue.skip();
        await promise;

        expect(queue.isAnimating).toBe(false);
        vi.unstubAllGlobals();
    });
});
