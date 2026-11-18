import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { RerollModal } from './RerollModal';
import type { PendingReroll } from '../api/types';

/**
 * P294 krok 5b (uživatel 17.11.2026: „musíš se ptát na použití rerolu při např. neúspěchu dodge — tak je dobře
 * označit správně situaci, která chce reroll“): dialog přehozu pohybu ukáže odkud kam a co je v sázce.
 * Minimální DOM mock jako DivingCatchModal.test.ts.
 */
type Handler = () => void;

function mockElement() {
    let html = '';
    const handlers: Handler[] = [];
    const el = {
        className: '',
        style: { display: '' },
        get innerHTML() { return html; },
        set innerHTML(v: string) { html = v; if (v === '') return; handlers.length = 0; },
        appendChild: vi.fn(),
        querySelectorAll(selector: string) {
            if (selector !== '.reroll-modal__btn') return [];
            return [...html.matchAll(/<button class="reroll-modal__btn[^"]*" data-choice="([^"]*)"/g)].map(m => ({
                dataset: { choice: m[1] },
                addEventListener: (_e: string, h: Handler) => handlers.push(h),
            }));
        },
        click(index: number) { handlers[index]?.(); },
    };
    return el;
}

const dodge: PendingReroll = {
    rollType: 'dodge', playerId: 1, target: 4, roll: 2, proAvailable: false, teamRerollAvailable: true,
    targetX: 6, targetY: 6,
    // P306 A7: popisek, kategorii a následek posílá server (`App\Enum\RollType`)
    label: 'Dodge', category: 'move', consequence: 'Accept Failure = the player falls (armour roll) and the turn ends.',
};

describe('RerollModal', () => {
    let modalEl: ReturnType<typeof mockElement>;

    beforeEach(() => {
        modalEl = mockElement();
        vi.stubGlobal('document', { createElement: () => modalEl });
    });
    afterEach(() => vi.unstubAllGlobals());

    it('dodge: shows from, to and that declining means a fall and the end of the turn', () => {
        new RerollModal({ appendChild: vi.fn() } as unknown as HTMLElement).show(dodge, 'Lineman', { x: 5, y: 6 }, vi.fn());
        expect(modalEl.innerHTML).toContain('from (5, 6) to (6, 6)');
        expect(modalEl.innerHTML).toContain('rolled <strong>2</strong> (needed 4+)');
        expect(modalEl.innerHTML).toContain('falls');
    });

    it('pickup: shows the ball square and that declining ends the turn; catch: only a bounce (P294 krok 5d)', () => {
        new RerollModal({ appendChild: vi.fn() } as unknown as HTMLElement).show({ ...dodge, rollType: 'pickup', label: 'Pickup', category: 'ball', consequence: 'Accept Failure = the ball bounces and the turn ends.' }, 'Gutter', { x: 6, y: 6 }, vi.fn());
        expect(modalEl.innerHTML).toContain('Pickup Failed!');
        expect(modalEl.innerHTML).toContain('at (6, 6)');
        expect(modalEl.innerHTML).toContain('the ball bounces and the turn ends');
        new RerollModal({ appendChild: vi.fn() } as unknown as HTMLElement).show({ ...dodge, rollType: 'catch', label: 'Catch', category: 'ball', consequence: 'Accept Failure = the ball bounces.' }, 'Catcher', { x: 6, y: 6 }, vi.fn());
        expect(modalEl.innerHTML).toContain('Catch Failed!');
        expect(modalEl.innerHTML).toContain('Accept Failure = the ball bounces.');
        expect(modalEl.innerHTML).not.toContain('turn ends');
    });

    it('big guy check: declining loses the declared action (P294 krok 5e)', () => {
        new RerollModal({ appendChild: vi.fn() } as unknown as HTMLElement).show({ ...dodge, rollType: 'bone_head', label: 'Bone Head', category: 'big_guy', consequence: 'Accept Failure = the player loses the declared action and his tackle zones.' }, 'Ogre', { x: 6, y: 6 }, vi.fn());
        expect(modalEl.innerHTML).toContain('Bone Head Failed!');
        expect(modalEl.innerHTML).toContain('loses the declared action');
        expect(modalEl.innerHTML).not.toContain('from (');
    });

    it('GFI on the spot (blitz block): only the square, still the fall warning', () => {
        new RerollModal({ appendChild: vi.fn() } as unknown as HTMLElement).show({ ...dodge, rollType: 'gfi', label: 'Going For It' }, 'Blitzer', { x: 6, y: 6 }, vi.fn());
        expect(modalEl.innerHTML).toContain('at (6, 6)');
        expect(modalEl.innerHTML).not.toContain('from (');
        expect(modalEl.innerHTML).toContain('falls');
    });

    it('pass: no move squares and no fall warning', () => {
        new RerollModal({ appendChild: vi.fn() } as unknown as HTMLElement).show({ ...dodge, rollType: 'pass', label: 'Pass', category: 'throw', consequence: null }, 'Thrower', { x: 5, y: 6 }, vi.fn());
        expect(modalEl.innerHTML).not.toContain('from (');
        expect(modalEl.innerHTML).not.toContain('falls');
    });

    it('a click on a button reports the choice (bug since 9.3.2026: hide() cleared the callback before it was called)', () => {
        const onChoice = vi.fn();
        new RerollModal({ appendChild: vi.fn() } as unknown as HTMLElement).show(dodge, 'Lineman', { x: 5, y: 6 }, onChoice);
        modalEl.click(0);   // Team Reroll (bez Pro je první)
        expect(onChoice).toHaveBeenCalledWith('team_reroll');
    });
});
