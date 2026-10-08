import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { BlockDiceModal } from './BlockDiceModal';
import type { PendingBlock } from '../api/types';

/**
 * P81 krok 3 (30.09.2026): follow-up je volba utocnika (BB2016 r. 608-611)
 * a posila se spolu s volbou kostky. Minimalni DOM mock, stejny styl jako
 * LevelUpModal.test.ts (projekt nema jsdom).
 */
type Handler = () => void;

function mockElement() {
    let html = '';
    const dieHandlers = new Map<string, Handler>();
    let acceptHandler: Handler | null = null;
    const followUp = { checked: true };
    const el = {
        className: '',
        style: { display: '' },
        textContent: '',
        get innerHTML() { return html || el.textContent; },
        set innerHTML(v: string) {
            html = v;
            dieHandlers.clear();
            acceptHandler = null;
            followUp.checked = /followup-input"[^>]*checked/.test(v);
        },
        appendChild: vi.fn(),
        remove: vi.fn(),
        querySelectorAll(selector: string) {
            if (selector !== '.block-dice-modal__die') return [];
            // kostky bez data-index (vybira souper) maji dataset prazdny
            return [...html.matchAll(/class="block-dice-modal__die [^"]*"( data-index="(\d+)")?/g)].map(m => ({
                dataset: m[2] === undefined ? {} : { index: m[2] },
                addEventListener: (_e: string, h: Handler) => dieHandlers.set(m[2] ?? 'none', h),
            }));
        },
        querySelector(selector: string) {
            if (selector === '.block-dice-modal__accept') {
                return html.includes('block-dice-modal__accept')
                    ? { addEventListener: (_e: string, h: Handler) => { acceptHandler = h; } }
                    : null;
            }
            return selector === '.block-dice-modal__followup-input' && html.includes('followup-input')
                ? followUp : null;
        },
        clickDie(index: number) { dieHandlers.get(String(index))?.(); },
        clickAccept() { acceptHandler?.(); },
        dieHandlerCount() { return dieHandlers.size; },
        followUp,
    };
    return el;
}

const pending = {
    attackerId: 1, defenderId: 2, faces: ['pushed', 'defender_down'],
    attackerChooses: true, isBlitz: true, isFrenzy: false,
    proAvailable: false, teamRerollAvailable: false,
} as unknown as PendingBlock;

describe('BlockDiceModal follow-up', () => {
    let modalEl: ReturnType<typeof mockElement>;

    beforeEach(() => {
        modalEl = mockElement();
        vi.stubGlobal('document', { createElement: () => modalEl });
    });
    afterEach(() => vi.unstubAllGlobals());

    it('offers the follow-up checkbox, ticked by default', () => {
        const modal = new BlockDiceModal(mockElement() as unknown as HTMLElement);
        modal.show(pending, 'A', 'B', vi.fn(), vi.fn());
        expect(modalEl.innerHTML).toContain('block-dice-modal__followup-input');
        expect(modalEl.followUp.checked).toBe(true);
    });

    it('sends followUp=true with the die by default', () => {
        const modal = new BlockDiceModal(mockElement() as unknown as HTMLElement);
        const onChoose = vi.fn();
        modal.show(pending, 'A', 'B', onChoose, vi.fn());
        modalEl.clickDie(1);
        expect(onChoose).toHaveBeenCalledWith(1, true);
    });

    it('sends followUp=false when the coach unticks it', () => {
        const modal = new BlockDiceModal(mockElement() as unknown as HTMLElement);
        const onChoose = vi.fn();
        modal.show(pending, 'A', 'B', onChoose, vi.fn());
        modalEl.followUp.checked = false;
        modalEl.clickDie(0);
        expect(onChoose).toHaveBeenCalledWith(0, false);
    });

    // BB2016 r. 633-634: "The coach of the stronger player picks which block dice is used."
    it('does not let the attacker pick a die when the opponent picks', () => {
        const modal = new BlockDiceModal(mockElement() as unknown as HTMLElement);
        const onChoose = vi.fn();
        modal.show({ ...pending, attackerChooses: false }, 'A', 'B', onChoose, vi.fn(), true);
        expect(modalEl.dieHandlerCount()).toBe(0);
        modalEl.clickDie(1);
        expect(onChoose).not.toHaveBeenCalled();
    });

    it('confirms the roll without a face index when the opponent picks', () => {
        const modal = new BlockDiceModal(mockElement() as unknown as HTMLElement);
        const onChoose = vi.fn();
        modal.show({ ...pending, attackerChooses: false }, 'A', 'B', onChoose, vi.fn(), true);
        modalEl.clickAccept();
        expect(onChoose).toHaveBeenCalledWith(null, true);
    });
});
