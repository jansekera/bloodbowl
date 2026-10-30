import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { DivingCatchModal } from './DivingCatchModal';
import type { PendingDivingCatch } from '../api/types';

/**
 * 30.10.2026: kouč volí, jestli jeho hráč použije Diving Catch (`rules_bb2016.txt` ř. 8065-8071).
 * Minimální DOM mock jako StandFirmModal.test.ts.
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

const pending: PendingDivingCatch = {
    playerId: 2, landing: { x: 10, y: 6 }, action: 'pass', params: {}, dice: [2, 2, 5, 3, 7], answers: [],
};

describe('DivingCatchModal', () => {
    let modalEl: ReturnType<typeof mockElement>;

    beforeEach(() => {
        modalEl = mockElement();
        vi.stubGlobal('document', { createElement: () => modalEl });
    });
    afterEach(() => vi.unstubAllGlobals());

    it('names the player and the landing square and offers yes / no', () => {
        const modal = new DivingCatchModal(mockElement() as unknown as HTMLElement);
        modal.show(pending, 'Catcher', vi.fn());
        expect(modalEl.buttonCount()).toBe(2);
        expect(modalEl.innerHTML).toContain('Catcher');
        expect(modalEl.innerHTML).toContain('(10, 6)');
        expect(modalEl.innerHTML).toContain('Diving Catch');
    });

    it('sends use=true to dive', () => {
        const modal = new DivingCatchModal(mockElement() as unknown as HTMLElement);
        const onChoice = vi.fn();
        modal.show(pending, 'Catcher', onChoice);
        modalEl.click(0);
        expect(onChoice).toHaveBeenCalledWith({ use: true });
    });

    it('sends use=false to let the ball land', () => {
        const modal = new DivingCatchModal(mockElement() as unknown as HTMLElement);
        const onChoice = vi.fn();
        modal.show(pending, 'Catcher', onChoice);
        modalEl.click(1);
        expect(onChoice).toHaveBeenCalledWith({ use: false });
    });
});
