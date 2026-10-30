import type { PendingDivingCatch } from '../api/types';

/**
 * 30.10.2026 (uživatel): kouč-člověk volí, jestli jeho hráč použije Diving Catch (`rules_bb2016.txt`
 * ř. 8065-8071: „can attempt to catch any pass, kick off or crowd throw-in … that would land in an
 * empty square in one of his tackle zones as if it had landed in his own square“). Ptá se i v tahu
 * soupeře (tah AI na odpověď čeká). Ne ⇒ míč dopadne normálně a odrazí se.
 */
export class DivingCatchModal {
    private container: HTMLElement;

    constructor(parent: HTMLElement) {
        this.container = document.createElement('div');
        this.container.className = 'reroll-modal';
        this.container.style.display = 'none';
        parent.appendChild(this.container);
    }

    /** onChoice: { use: true } = skočí a chytá ze svého pole, { use: false } = míč dopadne normálně. */
    show(pending: PendingDivingCatch, playerName: string, onChoice: (params: Record<string, unknown>) => void): void {
        const at = `(${pending.landing.x}, ${pending.landing.y})`;
        this.container.innerHTML = `
            <div class="reroll-modal__backdrop"></div>
            <div class="reroll-modal__content">
                <div class="reroll-modal__title">${this.escape(playerName)} has Diving Catch — the ball is landing on ${at}, next to him. Use Diving Catch?</div>
                <div class="reroll-modal__text">Yes: he tries to catch it from his own square (a failed catch bounces from there). No: the ball lands on ${at} and bounces.</div>
                <div class="reroll-modal__buttons">
                    <button class="reroll-modal__btn reroll-modal__btn--team" data-use="1">Diving Catch</button>
                    <button class="reroll-modal__btn reroll-modal__btn--decline" data-use="0">Let it land</button>
                </div>
            </div>
        `;
        this.container.style.display = '';
        this.container.querySelectorAll('.reroll-modal__btn').forEach(btn => {
            btn.addEventListener('click', () => {
                const use = (btn as HTMLElement).dataset.use === '1';
                this.hide();
                onChoice({ use });
            });
        });
    }

    hide(): void {
        this.container.style.display = 'none';
        this.container.innerHTML = '';
    }

    private escape(text: string): string {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }
}
