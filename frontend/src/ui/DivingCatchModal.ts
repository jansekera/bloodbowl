import type { PendingDivingCatch } from '../api/types';

/**
 * 30.10.2026 (uživatel): kouč-člověk volí, jestli jeho hráč použije Diving Catch (`rules_bb2016.txt`
 * ř. 8065-8071: „can attempt to catch any pass, kick off or crowd throw-in … that would land in an
 * empty square in one of his tackle zones as if it had landed in his own square“). Ptá se i v tahu
 * soupeře (tah AI na odpověď čeká). Ne ⇒ míč dopadne normálně a odrazí se.
 * Varianta B (30.10.2026): víc kandidátů ⇒ otázka na každého; dialog řekne, kolik dalších hráčů dopad
 * stihne a kdo už skáče — dva a víc pokusů se ruší (ř. 8069-8071 „neither can use it“).
 */
export class DivingCatchModal {
    private container: HTMLElement;

    constructor(parent: HTMLElement) {
        this.container = document.createElement('div');
        this.container.className = 'reroll-modal';
        this.container.style.display = 'none';
        parent.appendChild(this.container);
    }

    /**
     * onChoice: { use: true } = skočí a chytá ze svého pole, { use: false } = míč dopadne normálně.
     * attemptingName: hráč, který už skáče (null = nikdo).
     */
    show(
        pending: PendingDivingCatch,
        playerName: string,
        attemptingName: string | null,
        onChoice: (params: Record<string, unknown>) => void,
    ): void {
        const at = `(${pending.landing.x}, ${pending.landing.y})`;
        const others = pending.otherCandidates > 0
            ? `<div class="reroll-modal__text">${pending.otherCandidates} other player${pending.otherCandidates > 1 ? 's' : ''} with Diving Catch can reach ${at} too. If two or more try, they get in each other's way and nobody catches.</div>`
            : '';
        const already = attemptingName !== null
            ? `<div class="reroll-modal__text">${this.escape(attemptingName)} is already diving for it — if you dive too, nobody catches.</div>`
            : '';
        this.container.innerHTML = `
            <div class="reroll-modal__backdrop"></div>
            <div class="reroll-modal__content">
                <div class="reroll-modal__title">${this.escape(playerName)} has Diving Catch — the ball is landing on ${at}, next to him. Use Diving Catch?</div>
                <div class="reroll-modal__text">Yes: he tries to catch it from his own square (a failed catch bounces from there). No: the ball lands on ${at} and bounces.</div>
                ${others}${already}
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
