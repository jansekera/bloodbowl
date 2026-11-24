import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { PilingOnModal } from './PilingOnModal';

/**
 * P310: kouč volí, jestli jeho hráč použije Piling On (`rules_bb2016.txt` ř. 8361-8371: za týmový přehoz přehodit
 * brnění NEBO zranění, pak si lehne). Minimální DOM mock jako JuggernautModal.test.ts.
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

describe('PilingOnModal', () => {
    let modalEl: ReturnType<typeof mockElement>;

    beforeEach(() => {
        modalEl = mockElement();
        vi.stubGlobal('document', { createElement: () => modalEl });
    });
    afterEach(() => vi.unstubAllGlobals());

    it('says which roll is re-rolled and what it costs', () => {
        new PilingOnModal(mockElement() as unknown as HTMLElement).show('Troll', 'Lineman', true, vi.fn());
        expect(modalEl.buttonCount()).toBe(2);
        expect(modalEl.innerHTML).toContain('Troll has Piling On');
        expect(modalEl.innerHTML).toContain('Armour roll');
        expect(modalEl.innerHTML).toContain('team re-roll');
        expect(modalEl.innerHTML).toContain('placed prone');
        new PilingOnModal(mockElement() as unknown as HTMLElement).show('Troll', 'Lineman', false, vi.fn());
        expect(modalEl.innerHTML).toContain('Injury roll');
    });

    it('sends use=true to pile on', () => {
        const onChoice = vi.fn();
        new PilingOnModal(mockElement() as unknown as HTMLElement).show('Troll', 'Lineman', true, onChoice);
        modalEl.click(0);
        expect(onChoice).toHaveBeenCalledWith({ use: true });
    });

    it('sends use=false to decline', () => {
        const onChoice = vi.fn();
        new PilingOnModal(mockElement() as unknown as HTMLElement).show('Troll', 'Lineman', true, onChoice);
        modalEl.click(1);
        expect(onChoice).toHaveBeenCalledWith({ use: false });
    });

    it('escapes player names', () => {
        new PilingOnModal(mockElement() as unknown as HTMLElement).show('<b>x</b>', '<i>y</i>', true, vi.fn());
        expect(modalEl.innerHTML).not.toContain('<b>x</b>');
        expect(modalEl.innerHTML).not.toContain('<i>y</i>');
    });
});
