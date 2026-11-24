import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { WrestleModal } from './WrestleModal';

/** P312: kouč volí Wrestle (`rules_bb2016.txt` ř. 8670-8676 „may use“). Minimální DOM mock jako PilingOnModal.test.ts. */
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

describe('WrestleModal', () => {
    let modalEl: ReturnType<typeof mockElement>;

    beforeEach(() => {
        modalEl = mockElement();
        vi.stubGlobal('document', { createElement: () => modalEl });
    });
    afterEach(() => vi.unstubAllGlobals());

    it('names both players and offers Wrestle / Both Down', () => {
        new WrestleModal(mockElement() as unknown as HTMLElement).show('Longbeard', 'Blitzer', vi.fn());
        expect(modalEl.buttonCount()).toBe(2);
        expect(modalEl.innerHTML).toContain('Longbeard has Wrestle');
        expect(modalEl.innerHTML).toContain('Blitzer');
    });

    it('sends use=true / use=false', () => {
        const onChoice = vi.fn();
        new WrestleModal(mockElement() as unknown as HTMLElement).show('Longbeard', 'Blitzer', onChoice);
        modalEl.click(0);
        expect(onChoice).toHaveBeenCalledWith({ use: true });
        new WrestleModal(mockElement() as unknown as HTMLElement).show('Longbeard', 'Blitzer', onChoice);
        modalEl.click(1);
        expect(onChoice).toHaveBeenLastCalledWith({ use: false });
    });

    it('escapes player names', () => {
        new WrestleModal(mockElement() as unknown as HTMLElement).show('<b>x</b>', '<i>y</i>', vi.fn());
        expect(modalEl.innerHTML).not.toContain('<b>x</b>');
        expect(modalEl.innerHTML).not.toContain('<i>y</i>');
    });
});
