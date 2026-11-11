import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { MovePlanBar } from './MovePlanBar';

/** P296: lišta trasy nabízí Go / Step / Cancel a hlásí volbu. Minimální DOM mock jako DivingCatchModal.test.ts. */
type Handler = () => void;

function mockElement() {
    let html = '';
    const handlers: Handler[] = [];
    const el = {
        className: '',
        style: { display: '' },
        get innerHTML() { return html; },
        set innerHTML(v: string) { html = v; handlers.length = 0; },
        appendChild: vi.fn(),
        querySelectorAll(selector: string) {
            if (selector !== '.move-plan__btn') return [];
            return [...html.matchAll(/<button class="move-plan__btn[^"]*"([^>]*)>/g)].map(m => {
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

describe('MovePlanBar', () => {
    let barEl: ReturnType<typeof mockElement>;
    beforeEach(() => {
        barEl = mockElement();
        vi.stubGlobal('document', { createElement: () => barEl });
    });
    afterEach(() => vi.unstubAllGlobals());

    it('shows the route and offers go / step / cancel', () => {
        const bar = new MovePlanBar(mockElement() as unknown as HTMLElement);
        bar.show('Catcher', '3 squares · 1 dodge', vi.fn());
        expect(barEl.buttonCount()).toBe(3);
        expect(barEl.innerHTML).toContain('Catcher: 3 squares · 1 dodge');
        expect(barEl.style.display).toBe('');
    });

    it('reports each choice', () => {
        const bar = new MovePlanBar(mockElement() as unknown as HTMLElement);
        const onChoice = vi.fn();
        bar.show('Catcher', '1 square', onChoice);
        barEl.click(0);
        barEl.click(1);
        barEl.click(2);
        expect(onChoice.mock.calls).toEqual([['go'], ['step'], ['cancel']]);
    });

    it('escapes the player name and hides', () => {
        const bar = new MovePlanBar(mockElement() as unknown as HTMLElement);
        bar.show('<b>x</b>', '1 square', vi.fn());
        expect(barEl.innerHTML).toContain('&lt;b&gt;x&lt;/b&gt;');
        bar.hide();
        expect(barEl.style.display).toBe('none');
    });
});
