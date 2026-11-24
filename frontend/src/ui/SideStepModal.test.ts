import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { SideStepModal } from './SideStepModal';

/** P312: kouč volí pole Side Step (`rules_bb2016.txt` ř. 8474-8478 „any adjacent square“), nebo běžné odtlačení. */
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

const pending = { playerId: 2, squares: [{ x: 5, y: 4 }, { x: 7, y: 5 }], suggested: { x: 7, y: 5 } };

describe('SideStepModal', () => {
    let modalEl: ReturnType<typeof mockElement>;

    beforeEach(() => {
        modalEl = mockElement();
        vi.stubGlobal('document', { createElement: () => modalEl });
    });
    afterEach(() => vi.unstubAllGlobals());

    it('offers every square plus the standard push and marks the suggestion', () => {
        new SideStepModal(mockElement() as unknown as HTMLElement).show(pending, 'Gutter Runner', vi.fn());
        expect(modalEl.buttonCount()).toBe(3);
        expect(modalEl.innerHTML).toContain('(5, 4)');
        expect(modalEl.innerHTML).toContain('(7, 5) (suggested)');
        expect(modalEl.innerHTML).toContain('Standard push');
    });

    it('sends the chosen square', () => {
        const onChoice = vi.fn();
        new SideStepModal(mockElement() as unknown as HTMLElement).show(pending, 'Gutter Runner', onChoice);
        modalEl.click(0);
        expect(onChoice).toHaveBeenCalledWith({ use: true, x: 5, y: 4 });
    });

    it('sends use=false for the standard push', () => {
        const onChoice = vi.fn();
        new SideStepModal(mockElement() as unknown as HTMLElement).show(pending, 'Gutter Runner', onChoice);
        modalEl.click(2);
        expect(onChoice).toHaveBeenCalledWith({ use: false });
    });

    it('escapes the player name', () => {
        new SideStepModal(mockElement() as unknown as HTMLElement).show(pending, '<b>x</b>', vi.fn());
        expect(modalEl.innerHTML).not.toContain('<b>x</b>');
    });
});
