import type { PendingStandFirm } from '../api/types';

/**
 * P226 (21.10.2026): kouč-člověk volí, jestli jeho hráč použije Stand Firm (`rules_bb2016.txt`
 * ř. 8510-8516: „may choose to not be pushed back as the result of a block“). Ptá se i v tahu
 * soupeře (tah AI na odpověď čeká) a i u hráče v řetězovém odtlačení.
 */
export class StandFirmModal {
    private container: HTMLElement;

    constructor(parent: HTMLElement) {
        this.container = document.createElement('div');
        this.container.className = 'reroll-modal';
        this.container.style.display = 'none';
        parent.appendChild(this.container);
    }

    /** onChoice: { use: true } = zůstane stát, { use: false } = nechá se odtlačit. */
    show(
        pending: PendingStandFirm,
        playerName: string,
        attackerName: string,
        onChoice: (params: Record<string, unknown>) => void,
    ): void {
        const chain = pending.playerId !== pending.defenderId ? ' (chain push)' : '';
        this.container.innerHTML = `
            <div class="reroll-modal__backdrop"></div>
            <div class="reroll-modal__content">
                <div class="reroll-modal__title">${this.escape(playerName)} has Stand Firm — ${this.escape(attackerName)}'s block pushes him back${chain}. Use Stand Firm?</div>
                <div class="reroll-modal__buttons">
                    <button class="reroll-modal__btn reroll-modal__btn--team" data-use="1">Stand Firm (stay)</button>
                    <button class="reroll-modal__btn reroll-modal__btn--decline" data-use="0">Be pushed back</button>
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
