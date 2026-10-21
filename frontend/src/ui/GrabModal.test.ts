import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { GrabModal } from './GrabModal';
import type { PendingGrab } from '../api/types';

/**
 * P224 (21.10.2026): kouč volí pole odtlačení s Grabem, nebo běžné odtlačení
 * (`rules_bb2016.txt` ř. 8147-8154). Minimální DOM mock jako BlockDiceModal.test.ts.
 */
type Handler = () => void;

function mockElement() {
    let html = '';
    const handlers: { dataset: Record<string, string>; click: Handler }[] = [];
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
                return {
                    dataset,
                    addEventListener: (_e: string, h: Handler) => handlers.push({ dataset, click: h }),
                };
            });
        },
        click(index: number) { handlers[index]?.click(); },
        buttonCount() { return handlers.length; },
    };
    return el;
}

const pending: PendingGrab = {
    kind: 'block', attackerId: 1, defenderId: 2,
    squares: [{ x: 5, y: 6 }, { x: 6, y: 8 }],
};

describe('GrabModal', () => {
    let modalEl: ReturnType<typeof mockElement>;

    beforeEach(() => {
        modalEl = mockElement();
        vi.stubGlobal('document', { createElement: () => modalEl });
    });
    afterEach(() => vi.unstubAllGlobals());

    it('offers every free square plus the standard push', () => {
        const modal = new GrabModal(mockElement() as unknown as HTMLElement);
        modal.show(pending, 'A', 'B', vi.fn());
        expect(modalEl.buttonCount()).toBe(3);
        expect(modalEl.innerHTML).toContain('(5, 6)');
        expect(modalEl.innerHTML).toContain('(6, 8)');
    });

    it('sends the chosen square with use=true', () => {
        const modal = new GrabModal(mockElement() as unknown as HTMLElement);
        const onChoice = vi.fn();
        modal.show(pending, 'A', 'B', onChoice);
        modalEl.click(1);
        expect(onChoice).toHaveBeenCalledWith({ use: true, x: 6, y: 8 });
    });

    it('sends use=false for the standard push', () => {
        const modal = new GrabModal(mockElement() as unknown as HTMLElement);
        const onChoice = vi.fn();
        modal.show(pending, 'A', 'B', onChoice);
        modalEl.click(2);
        expect(onChoice).toHaveBeenCalledWith({ use: false });
    });
});
