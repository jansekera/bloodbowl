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
    const followUp = { checked: true };
    const handlers: { dataset: Record<string, string>; click: Handler }[] = [];
    const el = {
        className: '',
        style: { display: '' },
        textContent: '',
        get innerHTML() { return html || el.textContent; },
        set innerHTML(v: string) {
            html = v;
            followUp.checked = /followup-input"[^>]*checked/.test(v);
            handlers.length = 0;
        },
        appendChild: vi.fn(),
        followUp,
        querySelector(selector: string) {
            return selector === '.grab-modal__followup-input' && html.includes('grab-modal__followup-input')
                ? followUp : null;
        },
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
    faceIndex: 0, face: null,
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
        modal.show(pending, 'A', 'B', 'pushed', vi.fn());
        expect(modalEl.buttonCount()).toBe(3);
        expect(modalEl.innerHTML).toContain('(5, 6)');
        expect(modalEl.innerHTML).toContain('(6, 8)');
    });

    it('sends the chosen square with use=true', () => {
        const modal = new GrabModal(mockElement() as unknown as HTMLElement);
        const onChoice = vi.fn();
        modal.show(pending, 'A', 'B', 'pushed', onChoice);
        modalEl.click(1);
        expect(onChoice).toHaveBeenCalledWith({ use: true, x: 6, y: 8, followUp: true });
    });

    it('sends use=false for the standard push', () => {
        const modal = new GrabModal(mockElement() as unknown as HTMLElement);
        const onChoice = vi.fn();
        modal.show(pending, 'A', 'B', 'pushed', onChoice);
        modalEl.click(2);
        expect(onChoice).toHaveBeenCalledWith({ use: false, followUp: true });
    });

    it('shows the chosen die', () => {
        const modal = new GrabModal(mockElement() as unknown as HTMLElement);
        modal.show(pending, 'A', 'B', 'defender_down', vi.fn());
        expect(modalEl.innerHTML).toContain('defender_down');
    });

    // rules_bb2016.txt ř. 608-611: follow-up až po odtlačení => volí se v tomto dialogu
    it('sends followUp=false when the coach unticks it', () => {
        const modal = new GrabModal(mockElement() as unknown as HTMLElement);
        const onChoice = vi.fn();
        modal.show(pending, 'A', 'B', 'pushed', onChoice);
        modalEl.followUp.checked = false;
        modalEl.click(0);
        expect(onChoice).toHaveBeenCalledWith({ use: true, x: 5, y: 6, followUp: false });
    });

    it('has no follow-up choice for Ball & Chain (follow-up is mandatory)', () => {
        const modal = new GrabModal(mockElement() as unknown as HTMLElement);
        const onChoice = vi.fn();
        modal.show({ ...pending, kind: 'bnc', face: 'pushed' }, 'A', 'B', 'pushed', onChoice);
        expect(modalEl.innerHTML).not.toContain('grab-modal__followup-input');
        modalEl.click(2);
        expect(onChoice).toHaveBeenCalledWith({ use: false });
    });
});
