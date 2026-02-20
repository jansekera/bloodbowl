import { describe, it, expect, vi, beforeEach } from 'vitest';
import { ReplayControls } from './ReplayControls';
import type { GameEvent } from '../api/types';

function createEvent(type: string, description: string): GameEvent {
    return { type, description, data: {} };
}

/** Minimal HTMLElement mock for ReplayControls (no jsdom needed) */
function createMockContainer(): HTMLElement {
    let html = '';
    return {
        classList: { add: vi.fn() },
        get innerHTML() { return html; },
        set innerHTML(v: string) { html = v; },
        querySelectorAll: () => [],
    } as unknown as HTMLElement;
}

describe('ReplayControls', () => {
    let container: HTMLElement;
    let controls: ReplayControls;

    beforeEach(() => {
        container = createMockContainer();
        controls = new ReplayControls(container);
    });

    it('initializes with no events', () => {
        expect(controls.getEventCount()).toBe(0);
        expect(controls.getCurrentIndex()).toBe(-1);
    });

    it('sets events and resets state', () => {
        const events = [
            createEvent('move', 'Player moved'),
            createEvent('block', 'Player blocked'),
        ];
        controls.setEvents(events);

        expect(controls.getEventCount()).toBe(2);
        expect(controls.getCurrentIndex()).toBe(-1);
    });

    it('steps forward through events', () => {
        const events = [
            createEvent('move', 'Move 1'),
            createEvent('block', 'Block 1'),
            createEvent('pass', 'Pass 1'),
        ];
        controls.setEvents(events);

        const e1 = controls.next();
        expect(e1?.type).toBe('move');
        expect(controls.getCurrentIndex()).toBe(0);

        const e2 = controls.next();
        expect(e2?.type).toBe('block');
        expect(controls.getCurrentIndex()).toBe(1);

        const e3 = controls.next();
        expect(e3?.type).toBe('pass');
        expect(controls.getCurrentIndex()).toBe(2);

        // At end: returns null
        const e4 = controls.next();
        expect(e4).toBeNull();
        expect(controls.getCurrentIndex()).toBe(2);
    });

    it('steps backward through events', () => {
        const events = [
            createEvent('move', 'Move 1'),
            createEvent('block', 'Block 1'),
        ];
        controls.setEvents(events);
        controls.next(); // index 0
        controls.next(); // index 1

        const e = controls.prev();
        expect(e?.type).toBe('move');
        expect(controls.getCurrentIndex()).toBe(0);

        // At start: returns null
        const eNull = controls.prev();
        expect(eNull).toBeNull();
    });

    it('calls onStep callback when stepping', () => {
        const callback = vi.fn();
        controls.setOnStep(callback);

        controls.setEvents([createEvent('move', 'Move 1')]);
        controls.next();

        expect(callback).toHaveBeenCalledOnce();
        expect(callback).toHaveBeenCalledWith(expect.objectContaining({ type: 'move' }));
    });

    it('cycles speed 1 -> 2 -> 4 -> 1', () => {
        controls.setEvents([createEvent('move', 'M')]);
        controls.render();

        // Initial speed is 1x, shown in render
        expect(container.innerHTML).toContain('1×');

        controls.cycleSpeed();
        expect(container.innerHTML).toContain('2×');

        controls.cycleSpeed();
        expect(container.innerHTML).toContain('4×');

        controls.cycleSpeed();
        expect(container.innerHTML).toContain('1×');
    });
});
