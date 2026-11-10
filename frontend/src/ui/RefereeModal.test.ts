import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { RefereeModal } from './RefereeModal';
import type { PendingReferee } from '../api/types';

/**
 * P291 (10.11.2026): rozhodčí vylučuje hráče kouče — úplatek (`rules_bb2016.txt` ř. 2871-2879) / protest (ř. 1091-1102).
 * Minimální DOM mock jako DivingCatchModal.test.ts.
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

const foul: PendingReferee = {
    playerId: 1, foul: true, canBribe: true, canArgue: true, action: 'foul', params: {}, dice: [3, 3], answers: [], queue: [],
};

describe('RefereeModal', () => {
    let modalEl: ReturnType<typeof mockElement>;

    beforeEach(() => {
        modalEl = mockElement();
        vi.stubGlobal('document', { createElement: () => modalEl });
    });
    afterEach(() => vi.unstubAllGlobals());

    it('offers all four combinations when both are possible, bribe first after a foul', () => {
        const modal = new RefereeModal(mockElement() as unknown as HTMLElement);
        const onChoice = vi.fn();
        modal.show(foul, 'Fouler', onChoice);
        expect(modalEl.buttonCount()).toBe(4);
        expect(modalEl.innerHTML).toContain('Fouler');
        expect(modalEl.innerHTML).toContain('Bribe, then argue');
        modalEl.click(0);
        expect(onChoice).toHaveBeenCalledWith({ bribe: true, argue: true });
    });

    it('argues first for a secret weapon at the end of the drive', () => {
        const modal = new RefereeModal(mockElement() as unknown as HTMLElement);
        modal.show({ ...foul, foul: false, action: null }, 'Looney', vi.fn());
        expect(modalEl.innerHTML).toContain('Argue, then bribe');
        expect(modalEl.innerHTML).toContain('Secret Weapon');
    });

    it('without a bribe offers only argue or accept', () => {
        const modal = new RefereeModal(mockElement() as unknown as HTMLElement);
        const onChoice = vi.fn();
        modal.show({ ...foul, canBribe: false }, 'Fouler', onChoice);
        expect(modalEl.buttonCount()).toBe(2);
        modalEl.click(1);
        expect(onChoice).toHaveBeenCalledWith({ bribe: false, argue: false });
    });

    it('with a sent-off coach offers only bribe or accept', () => {
        const modal = new RefereeModal(mockElement() as unknown as HTMLElement);
        const onChoice = vi.fn();
        modal.show({ ...foul, canArgue: false }, 'Fouler', onChoice);
        expect(modalEl.buttonCount()).toBe(2);
        modalEl.click(0);
        expect(onChoice).toHaveBeenCalledWith({ bribe: true, argue: false });
    });
});
