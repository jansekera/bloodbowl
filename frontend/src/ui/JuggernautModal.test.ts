import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { JuggernautModal } from './JuggernautModal';

/**
 * P309: kouč volí, jestli Both Down jeho blitzujícího hráče s Juggernautem platí jako Pushed
 * (`rules_bb2016.txt` ř. 8191-8195 „may choose“). Minimální DOM mock jako DivingCatchModal.test.ts.
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

describe('JuggernautModal', () => {
    let modalEl: ReturnType<typeof mockElement>;

    beforeEach(() => {
        modalEl = mockElement();
        vi.stubGlobal('document', { createElement: () => modalEl });
    });
    afterEach(() => vi.unstubAllGlobals());

    it('names both players and offers Pushed / Both Down', () => {
        new JuggernautModal(mockElement() as unknown as HTMLElement).show('Bloodletter', 'Lineman', vi.fn());
        expect(modalEl.buttonCount()).toBe(2);
        expect(modalEl.innerHTML).toContain('Bloodletter has Juggernaut');
        expect(modalEl.innerHTML).toContain('Lineman');
    });

    it('sends use=true to treat it as Pushed', () => {
        const onChoice = vi.fn();
        new JuggernautModal(mockElement() as unknown as HTMLElement).show('Bloodletter', 'Lineman', onChoice);
        modalEl.click(0);
        expect(onChoice).toHaveBeenCalledWith({ use: true });
    });

    it('sends use=false to keep Both Down', () => {
        const onChoice = vi.fn();
        new JuggernautModal(mockElement() as unknown as HTMLElement).show('Bloodletter', 'Lineman', onChoice);
        modalEl.click(1);
        expect(onChoice).toHaveBeenCalledWith({ use: false });
    });

    it('escapes player names', () => {
        new JuggernautModal(mockElement() as unknown as HTMLElement).show('<b>x</b>', 'Lineman', vi.fn());
        expect(modalEl.innerHTML).not.toContain('<b>x</b>');
    });
});
