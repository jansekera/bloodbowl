import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { StandFirmModal } from './StandFirmModal';
import type { PendingStandFirm } from '../api/types';

/**
 * P226 (21.10.2026): kouč volí, jestli jeho hráč použije Stand Firm (`rules_bb2016.txt` ř. 8510-8516).
 * Minimální DOM mock jako GrabModal.test.ts.
 */
type Handler = () => void;

function mockElement() {
    let html = '';
    const handlers: Handler[] = [];
    const el = {
        className: '',
        style: { display: '' },
        textContent: '',
        get innerHTML() { return html || el.textContent; },
        set innerHTML(v: string) {
            html = v;
            handlers.length = 0;
        },
        appendChild: vi.fn(),
        querySelectorAll(selector: string) {
            if (selector !== '.reroll-modal__btn') return [];
            return [...html.matchAll(/<button class="reroll-modal__btn[^"]*"([^>]*)>/g)].map(m => {
                const dataset: Record<string, string> = {};
                for (const a of m[1].matchAll(/data-(\w+)="([^"]*)"/g)) dataset[a[1]] = a[2];
                return { dataset, addEventListener: (_e: string, h: Handler) => handlers.push(h) };
            });
        },
        click(index: number) { handlers[index]?.(); },
        buttonCount() { return handlers.length; },
    };
    return el;
}

const pending: PendingStandFirm = {
    playerId: 2, attackerId: 1, defenderId: 2, kind: 'block', face: 'pushed', faceIndex: 0, pushTo: { x: 7, y: 8 },
};
const at = { x: 6, y: 7 };

describe('StandFirmModal', () => {
    let modalEl: ReturnType<typeof mockElement>;

    beforeEach(() => {
        modalEl = mockElement();
        vi.stubGlobal('document', { createElement: () => modalEl });
    });
    afterEach(() => vi.unstubAllGlobals());

    it('offers to stand firm or be pushed', () => {
        const modal = new StandFirmModal(mockElement() as unknown as HTMLElement);
        modal.show(pending, 'B', 'A', at, vi.fn());
        expect(modalEl.buttonCount()).toBe(2);
        expect(modalEl.innerHTML).toContain('Stand Firm');
    });

    it('sends use=true for Stand Firm', () => {
        const modal = new StandFirmModal(mockElement() as unknown as HTMLElement);
        const onChoice = vi.fn();
        modal.show(pending, 'B', 'A', at, onChoice);
        modalEl.click(0);
        expect(onChoice).toHaveBeenCalledWith({ use: true });
    });

    it('sends use=false to be pushed back', () => {
        const modal = new StandFirmModal(mockElement() as unknown as HTMLElement);
        const onChoice = vi.fn();
        modal.show(pending, 'B', 'A', at, onChoice);
        modalEl.click(1);
        expect(onChoice).toHaveBeenCalledWith({ use: false });
    });

    it('marks a chain push (asked player is not the blocked defender)', () => {
        const modal = new StandFirmModal(mockElement() as unknown as HTMLElement);
        modal.show({ ...pending, playerId: 5 }, 'C', 'A', at, vi.fn());
        expect(modalEl.innerHTML).toContain('chain push');
    });

    it('does not mark the blocked defender as a chain push', () => {
        const modal = new StandFirmModal(mockElement() as unknown as HTMLElement);
        modal.show(pending, 'B', 'A', at, vi.fn());
        expect(modalEl.innerHTML).not.toContain('chain push');
    });

    // review P226: Pushed vs Knock-down (ř. 8511-8513) a pole, kam by šel
    it('shows the block die and where he would be pushed', () => {
        const modal = new StandFirmModal(mockElement() as unknown as HTMLElement);
        modal.show({ ...pending, face: 'defender_down' }, 'B', 'A', at, vi.fn());
        expect(modalEl.innerHTML).toContain('defender_down');
        expect(modalEl.innerHTML).toContain('(7, 8)');
    });

    it('says crowd when the push goes off the pitch', () => {
        const modal = new StandFirmModal(mockElement() as unknown as HTMLElement);
        modal.show({ ...pending, pushTo: null }, 'B', 'A', at, vi.fn());
        expect(modalEl.innerHTML).toContain('crowd');
    });

    it('says the push is blocked when he would not move', () => {
        const modal = new StandFirmModal(mockElement() as unknown as HTMLElement);
        modal.show({ ...pending, pushTo: { x: 6, y: 7 } }, 'B', 'A', at, vi.fn());
        expect(modalEl.innerHTML).toContain('blocked');
    });
});
